param(
    [string]$ExecutablePath = "out\HenkaSandbox3D\HenkaSandbox3D.exe",
    [string]$OutputDirectory = "build\compass-visual-evidence"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Get-Location).Path
$executable = (Resolve-Path -LiteralPath (Join-Path $repoRoot $ExecutablePath)).Path
$outputRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
. (Join-Path $repoRoot "scripts\henka_script_common.ps1")
. (Join-Path $repoRoot "scripts\henka_ui_automation_helpers.ps1")

if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "The packaged Compass executable was not found: $executable"
}

Add-Type -AssemblyName System.Drawing

if (-not ("HenkaCompassVisualNative" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class HenkaCompassVisualNative
{
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool BringWindowToTop(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll")]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int maxLength);

    public static IntPtr FindProcessWindow(uint processId, string title)
    {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hWnd, IntPtr lParam) {
            uint owner;
            GetWindowThreadProcessId(hWnd, out owner);
            if (owner == processId)
            {
                StringBuilder text = new StringBuilder(256);
                GetWindowText(hWnd, text, text.Capacity);
                if (text.ToString().Contains(title))
                {
                    result = hWnd;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

}
"@
}

[System.IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$stdoutPath = Join-Path $outputRoot "stdout.log"
$stderrPath = Join-Path $outputRoot "stderr.log"
$eventPath = Join-Path $outputRoot "automation-input.events"
$process = $null
$capturedProcess = $null
$handle = [System.IntPtr]::Zero
$previousOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
$previousFile = $env:HENKA_AUTOMATION_INPUT_FILE
$encoding = New-Object System.Text.UTF8Encoding($false)

Remove-Item -LiteralPath @($stdoutPath, $stderrPath, $eventPath) -Force -ErrorAction SilentlyContinue
New-Item -ItemType File -Path $eventPath -Force | Out-Null

function Send-CompassEvent {
    param([Parameter(Mandatory = $true)][string]$Line)

    $bytes = $script:encoding.GetBytes($Line + [Environment]::NewLine)
    $stream = [System.IO.File]::Open(
        $eventPath,
        [System.IO.FileMode]::OpenOrCreate,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::ReadWrite)
    try {
        $stream.Seek(0, [System.IO.SeekOrigin]::End) | Out-Null
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush($true)
    }
    finally {
        $stream.Dispose()
    }
    Start-Sleep -Milliseconds 160
}

function Send-CompassKey {
    param([Parameter(Mandatory = $true)][string]$KeyName)
    Send-CompassEvent -Line ("key {0} down" -f $KeyName)
    Send-CompassEvent -Line ("key {0} up" -f $KeyName)
}

function Send-CompassMove {
    param([double]$X, [double]$Y)
    Send-CompassEvent -Line ("move {0} {1}" -f $X, $Y)
}

function Send-CompassClick {
    param([double]$X, [double]$Y)
    Send-CompassMove -X $X -Y $Y
    Send-CompassEvent -Line ("button left down {0} {1}" -f $X, $Y)
    Send-CompassEvent -Line ("button left up {0} {1}" -f $X, $Y)
}

function Capture-Compass {
    param([Parameter(Mandatory = $true)][string]$Name)
    $path = Join-Path $outputRoot ($Name + ".png")
    $rect = [HenkaCompassVisualNative+RECT]::new()
    if (-not [HenkaCompassVisualNative]::GetWindowRect($handle, [ref]$rect) -or
        $rect.Right -le $rect.Left -or $rect.Bottom -le $rect.Top) {
        throw "The Compass target window rectangle was invalid."
    }
    $windowWidth = [int]$rect.Right - [int]$rect.Left
    $windowHeight = [int]$rect.Bottom - [int]$rect.Top
    $bitmap = New-Object System.Drawing.Bitmap($windowWidth, $windowHeight)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen(
            $rect.Left,
            $rect.Top,
            0,
            0,
            $bitmap.Size,
            [System.Drawing.CopyPixelOperation]::SourceCopy)
        $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
    Write-Output ("[capture] {0}" -f $path)
}

function Get-CompassViewport {
    $match = Get-LastLogRegexMatch `
        -Path $stdoutPath `
        -Pattern 'Sandbox viewport: origin (?<x>[0-9]+),(?<y>[0-9]+) size (?<width>[0-9]+)x(?<height>[0-9]+)\.'
    if ($null -eq $match) {
        throw "The packaged runtime did not report Compass viewport geometry."
    }
    return $match
}

try {
    $env:HENKA_AUTOMATION_INPUT_OWNED = "1"
    $env:HENKA_AUTOMATION_INPUT_FILE = $eventPath
    $capturedProcess = Start-HenkaCapturedProcess `
        -FilePath $executable `
        -WorkingDirectory (Split-Path -Parent $executable) `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath `
        -CreateNoWindow
    $process = $capturedProcess.Process

    for ($attempt = 0; $attempt -lt 100 -and $handle -eq [System.IntPtr]::Zero; ++$attempt) {
        Start-Sleep -Milliseconds 200
        $process.Refresh()
        $handle = [HenkaCompassVisualNative]::FindProcessWindow([uint32]$process.Id, "Henka Engine Sandbox 3D")
    }
    if ($handle -eq [System.IntPtr]::Zero) {
        throw "The packaged Compass window did not become available."
    }
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -TimeoutMilliseconds 30000)) {
        throw "The packaged runtime did not report UI readiness."
    }

    Set-HenkaAutomationForeground -Handle $handle
    Send-CompassKey -KeyName "F5"
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox layout: Focus Viewport" -TimeoutMilliseconds 5000)) {
        throw "The packaged runtime did not enter Focus Viewport layout."
    }
    Start-Sleep -Milliseconds 300

    $viewport = Get-CompassViewport
    $viewportX = [double]$viewport.Groups["x"].Value
    $viewportY = [double]$viewport.Groups["y"].Value
    $viewportWidth = [double]$viewport.Groups["width"].Value
    $viewportHeight = [double]$viewport.Groups["height"].Value
    $compassRadius = 64.0
    $compassCenterX = $viewportX + $viewportWidth - 12.0 - $compassRadius
    $compassCenterY = $viewportY + 12.0 + $compassRadius
    $infoCenterX = $compassCenterX
    $infoCenterY = $compassCenterY + $compassRadius + 6.0 + 14.5

    Set-HenkaAutomationForeground -Handle $handle
    Capture-Compass -Name "A-normal-perspective"

    Send-CompassMove -X ($compassCenterX + 36.0) -Y $compassCenterY
    Capture-Compass -Name "D-hover-ring"

    Send-CompassMove -X $compassCenterX -Y $compassCenterY
    Send-CompassEvent -Line ("button left down {0} {1}" -f $compassCenterX, $compassCenterY)
    Send-CompassMove -X ($compassCenterX + 48.0) -Y ($compassCenterY + 28.0)
    Send-CompassMove -X ($compassCenterX + 78.0) -Y ($compassCenterY + 46.0)
    Send-CompassEvent -Line ("button left up {0} {1}" -f ($compassCenterX + 78.0), ($compassCenterY + 46.0))
    Capture-Compass -Name "B-rotated-three-quarter"

    Send-CompassClick -X $infoCenterX -Y $infoCenterY
    Capture-Compass -Name "F-info-position"

    Send-CompassClick -X ($compassCenterX + 54.0) -Y $infoCenterY
    Capture-Compass -Name "C-orthographic"

    Send-CompassMove -X ($viewportX + $viewportWidth * 0.48) -Y ($viewportY + $viewportHeight * 0.52)
    Start-Sleep -Milliseconds 200
    Capture-Compass -Name "E-viewport-hover"

    Copy-Item -LiteralPath $stdoutPath -Destination (Join-Path $outputRoot "stdout.final.log") -Force
    Copy-Item -LiteralPath $stderrPath -Destination (Join-Path $outputRoot "stderr.final.log") -Force -ErrorAction SilentlyContinue
    Write-Output ("[pass] Compass packaged visual evidence captured in {0}" -f $outputRoot)
}
finally {
    if ($null -eq $previousOwned) {
        Remove-Item Env:HENKA_AUTOMATION_INPUT_OWNED -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_INPUT_OWNED = $previousOwned
    }
    if ($null -eq $previousFile) {
        Remove-Item Env:HENKA_AUTOMATION_INPUT_FILE -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_INPUT_FILE = $previousFile
    }
    if ($null -ne $capturedProcess) {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
    }
}
