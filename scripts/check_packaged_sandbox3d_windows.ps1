param(
    [switch]$NonInteractive,

    [switch]$ContractOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($ContractOnly -and -not $NonInteractive) {
    throw "ContractOnly requires NonInteractive."
}

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

if (-not ("HenkaUiAutomationNative" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class HenkaUiAutomationNative
{
    public const uint WM_MOUSEMOVE = 0x0200;
    public const uint WM_RBUTTONDOWN = 0x0204;
    public const uint WM_RBUTTONUP = 0x0205;
    public const uint MK_RBUTTON = 0x0002;
    public const int SW_RESTORE = 9;

    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool BringWindowToTop(IntPtr hWnd);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool PostMessage(
        IntPtr hWnd,
        uint msg,
        UIntPtr wParam,
        IntPtr lParam);
}
"@
}

function Set-HenkaAutomationForeground {
    param([Parameter(Mandatory = $true)][System.IntPtr]$Handle)

    if ($Handle -eq [System.IntPtr]::Zero) {
        throw "The Henka automation target handle is invalid."
    }

    if ([HenkaUiAutomationNative]::GetForegroundWindow() -eq $Handle) {
        return
    }

    [HenkaUiAutomationNative]::ShowWindowAsync(
        $Handle,
        [HenkaUiAutomationNative]::SW_RESTORE) | Out-Null
    [HenkaUiAutomationNative]::BringWindowToTop($Handle) | Out-Null

    $deadline = (Get-Date).AddSeconds(3)

    do {
        [HenkaUiAutomationNative]::SetForegroundWindow($Handle) | Out-Null

        if ([HenkaUiAutomationNative]::GetForegroundWindow() -eq $Handle) {
            Start-Sleep -Milliseconds 150
            return
        }

        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)

    throw (
        "The packaged UI harness could not acquire the Henka window as " +
        "foreground within three seconds. This is an automation-environment " +
        "failure; no engine UI assertion was made.")
}

function New-HenkaMouseLParam {
    param(
        [Parameter(Mandatory = $true)][int]$X,
        [Parameter(Mandatory = $true)][int]$Y
    )

    if ($X -lt 0 -or $Y -lt 0 -or $X -gt 65535 -or $Y -gt 65535) {
        throw "The Henka client mouse coordinate is outside the Win32 message range."
    }

    $packed = (($Y -band 0xFFFF) -shl 16) -bor ($X -band 0xFFFF)
    return [System.IntPtr][int]$packed
}

function Get-PackageInfoValue {
    param(
        [string]$Path,
        [string]$Name
    )

    $match = Select-String -LiteralPath $Path -Pattern ("^" + [Regex]::Escape($Name) + ":\s*(.+)$") | Select-Object -First 1
    if ($null -eq $match) {
        throw "Package field was not found: $Name"
    }
    return $match.Matches[0].Groups[1].Value.Trim()
}
function Write-Step {
    param([string]$Message)
    Write-Output "[check] $Message"
}

function Assert-PathExists {
    param(
        [string]$Path,
        [string]$Description
    )

    if (-not (Test-Path $Path)) {
        throw "$Description was not found: $Path"
    }

    Write-Output "[pass] $Description"
}

function Assert-FileContains {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Description
    )

    if (-not (Test-Path $Path)) {
        throw "$Description could not be checked because the log file was not created: $Path"
    }

    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
        throw "$Description was not found in $Path"
    }

    Write-Output "[pass] $Description"
}

function Try-AssertFileContains {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Description
    )

    if (-not (Test-Path $Path)) {
        Write-Output "[warn] $Description could not be checked because the log file was not created: $Path"
        return $false
    }

    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
        Write-Output "[warn] $Description was not found in $Path"
        return $false
    }

    Write-Output "[pass] $Description"
    return $true
}

function Try-AssertPathExists {
    param(
        [string]$Path,
        [string]$Description
    )

    if (-not (Test-Path $Path)) {
        Write-Output "[warn] $Description was not found: $Path"
        return $false
    }

    Write-Output "[pass] $Description"
    return $true
}

function Wait-FileContains {
    param(
        [string]$Path,
        [string]$Pattern,
        [int]$TimeoutMilliseconds = 5000
    )

    $deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
            return $true
        }

        Start-Sleep -Milliseconds 150
    }

    return $false
}

function Get-WindowRect {
    param([System.IntPtr]$Handle)

    $rect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetWindowRect($Handle, [ref]$rect)) {
        throw "The packaged sandbox window bounds could not be read."
    }

    return $rect
}

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Content
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        [System.IO.Directory]::CreateDirectory($parent) | Out-Null
    }

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

function Save-WindowScreenshot {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Description
    )

    Set-HenkaAutomationForeground -Handle $Handle
    $rect = Get-WindowRect -Handle $Handle
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0) {
        throw "$Description window bounds are invalid for screenshot capture."
    }

    $bitmap = New-Object System.Drawing.Bitmap -ArgumentList $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $size = New-Object System.Drawing.Size -ArgumentList $width, $height
        $graphics.CopyFromScreen(
            $rect.Left,
            $rect.Top,
            0,
            0,
            $size)
        $bitmap.Save(
            $Path,
            [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }

    Assert-PathExists -Path $Path -Description $Description
}

function Click-WindowPoint {
    param(
        [System.IntPtr]$Handle,
        [int]$OffsetX,
        [int]$OffsetY
    )

    $rect = Get-WindowRect -Handle $Handle
    $x = $rect.Left + $OffsetX
    $y = $rect.Top + $OffsetY

    Set-HenkaAutomationForeground -Handle $Handle
    [NativeMethods]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 100
    [NativeMethods]::mouse_event([NativeMethods]::MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [System.UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [NativeMethods]::mouse_event([NativeMethods]::MOUSEEVENTF_LEFTUP, 0, 0, 0, [System.UIntPtr]::Zero)
    Start-Sleep -Milliseconds 250
}

function Get-LastLogRegexMatch {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }

    $deadline = (Get-Date).AddSeconds(5)
    $lastReadError = $null
    do {
        $stream = $null
        $reader = $null
        try {
            $shareMode = [System.IO.FileShare](
                [int][System.IO.FileShare]::ReadWrite -bor
                [int][System.IO.FileShare]::Delete)
            $stream = [System.IO.FileStream]::new(
                $Path,
                [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read,
                $shareMode)
            $reader = [System.IO.StreamReader]::new(
                $stream,
                [System.Text.Encoding]::UTF8,
                $true,
                4096,
                $false)
            $text = $reader.ReadToEnd()
            $reader.Dispose()
            $reader = $null
            $stream = $null
            $lastReadError = $null

            $matches = [Regex]::Matches(
                $text,
                $Pattern,
                [System.Text.RegularExpressions.RegexOptions]::Multiline)
            if ($matches.Count -gt 0) {
                return $matches[$matches.Count - 1]
            }
        }
        catch [System.IO.IOException] {
            $lastReadError = $_.Exception
        }
        catch [System.UnauthorizedAccessException] {
            $lastReadError = $_.Exception
        }
        finally {
            if ($null -ne $reader) {
                $reader.Dispose()
            }
            elseif ($null -ne $stream) {
                $stream.Dispose()
            }
        }

        if ((Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 100
        }
    } while ((Get-Date) -lt $deadline)

    if ($null -ne $lastReadError) {
        throw (
            "The live packaged-sandbox log could not be read " +
            "with shared read access within five seconds: " +
            $lastReadError.Message)
    }

    return $null
}
function Assert-FramebufferRect {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y,
        [Parameter(Mandatory = $true)][double]$Width,
        [Parameter(Mandatory = $true)][double]$Height
    )

    foreach ($value in @($X, $Y, $Width, $Height)) {
        if ([double]::IsNaN($value) -or
            [double]::IsInfinity($value)) {
            throw "$Name reported a non-finite framebuffer rectangle."
        }
    }

    if ($FramebufferWidth -le 0 -or $FramebufferHeight -le 0) {
        throw "Framebuffer dimensions must be positive for geometry validation."
    }
    if ($Width -le 0.0 -or $Height -le 0.0) {
        throw "$Name reported a non-positive framebuffer rectangle."
    }
    if ($X -lt 0.0 -or
        $Y -lt 0.0 -or
        $X + $Width -gt [double]$FramebufferWidth -or
        $Y + $Height -gt [double]$FramebufferHeight) {
        throw (
            "$Name is outside the reported framebuffer: " +
            "rect=($X,$Y,$Width,$Height), " +
            "framebuffer=$($FramebufferWidth)x$($FramebufferHeight).")
    }

    Write-Output "[pass] $Name is inside the reported framebuffer"
}

function Click-FramebufferPoint {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$FramebufferX,
        [Parameter(Mandatory = $true)][double]$FramebufferY
    )

    if ($FramebufferWidth -le 0 -or $FramebufferHeight -le 0) {
        throw "Framebuffer dimensions must be positive for UI automation."
    }

    $clientRect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetClientRect(
            $Handle,
            [ref]$clientRect)) {
        throw "The packaged sandbox client bounds could not be read."
    }

    $clientWidth = $clientRect.Right - $clientRect.Left
    $clientHeight = $clientRect.Bottom - $clientRect.Top
    if ($clientWidth -le 0 -or $clientHeight -le 0) {
        throw "The packaged sandbox client bounds are invalid."
    }

    $point = New-Object NativeMethods+POINT
    $point.X = [int][Math]::Round(
        $FramebufferX *
        [double]$clientWidth /
        [double]$FramebufferWidth)
    $point.Y = [int][Math]::Round(
        $FramebufferY *
        [double]$clientHeight /
        [double]$FramebufferHeight)

    if (-not [NativeMethods]::ClientToScreen(
            $Handle,
            [ref]$point)) {
        throw "The packaged sandbox client point could not be converted to screen coordinates."
    }

    Set-HenkaAutomationForeground -Handle $Handle
    [NativeMethods]::SetCursorPos(
        $point.X,
        $point.Y) | Out-Null
    Start-Sleep -Milliseconds 100
    [NativeMethods]::mouse_event(
        [NativeMethods]::MOUSEEVENTF_LEFTDOWN,
        0,
        0,
        0,
        [System.UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [NativeMethods]::mouse_event(
        [NativeMethods]::MOUSEEVENTF_LEFTUP,
        0,
        0,
        0,
        [System.UIntPtr]::Zero)
    Start-Sleep -Milliseconds 250
}
function Click-FramebufferPointRight {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$FramebufferX,
        [Parameter(Mandatory = $true)][double]$FramebufferY
    )

    $clientRect = New-Object NativeMethods+RECT

    if (-not [NativeMethods]::GetClientRect($Handle, [ref]$clientRect)) {
        throw "The packaged sandbox client bounds could not be read for right click."
    }

    $clientWidth = $clientRect.Right - $clientRect.Left
    $clientHeight = $clientRect.Bottom - $clientRect.Top

    if ($FramebufferWidth -le 0 -or
        $FramebufferHeight -le 0 -or
        $clientWidth -le 0 -or
        $clientHeight -le 0) {
        throw "Invalid framebuffer or client dimensions for right click."
    }

    $point = New-Object NativeMethods+POINT
    $point.X = [int][Math]::Round(
        $FramebufferX *
        [double]$clientWidth /
        [double]$FramebufferWidth)
    $point.Y = [int][Math]::Round(
        $FramebufferY *
        [double]$clientHeight /
        [double]$FramebufferHeight)

    if ($point.X -lt 0 -or
        $point.Y -lt 0 -or
        $point.X -ge $clientWidth -or
        $point.Y -ge $clientHeight) {
        throw "The packaged sandbox right-click coordinate is outside the client area."
    }

    if (-not [NativeMethods]::ClientToScreen($Handle, [ref]$point)) {
        throw "The packaged sandbox right-click point could not be converted to screen coordinates."
    }

    Set-HenkaAutomationForeground -Handle $Handle

    if (-not [NativeMethods]::SetCursorPos($point.X, $point.Y)) {
        throw "The packaged sandbox cursor could not be positioned for right click."
    }

    Start-Sleep -Milliseconds 125

    if ([HenkaUiAutomationNative]::GetForegroundWindow() -ne $Handle) {
        throw (
            "Henka lost foreground ownership after positioning the context-menu " +
            "cursor and before the right-click input was sent.")
    }

    [NativeMethods]::mouse_event(
        0x0008,
        0,
        0,
        0,
        [System.UIntPtr]::Zero)
    Start-Sleep -Milliseconds 90
    [NativeMethods]::mouse_event(
        0x0010,
        0,
        0,
        0,
        [System.UIntPtr]::Zero)
    Start-Sleep -Milliseconds 350
}

function Save-FramebufferRegionScreenshot {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y,
        [Parameter(Mandatory = $true)][double]$Width,
        [Parameter(Mandatory = $true)][double]$Height,
        [Parameter(Mandatory = $true)][string]$Path
    )
    Set-HenkaAutomationForeground -Handle $Handle
    $clientRect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetClientRect($Handle, [ref]$clientRect)) {
        throw "The packaged sandbox client bounds could not be read for scene capture."
    }
    $clientWidth = $clientRect.Right - $clientRect.Left
    $clientHeight = $clientRect.Bottom - $clientRect.Top
    $origin = New-Object NativeMethods+POINT
    $origin.X = 0
    $origin.Y = 0
    if (-not [NativeMethods]::ClientToScreen($Handle, [ref]$origin)) {
        throw "The packaged sandbox client origin could not be converted to screen coordinates."
    }
    $screenX = $origin.X + [int][Math]::Round($X * $clientWidth / $FramebufferWidth)
    $screenY = $origin.Y + [int][Math]::Round($Y * $clientHeight / $FramebufferHeight)
    $screenWidth = [Math]::Max(1, [int][Math]::Round($Width * $clientWidth / $FramebufferWidth))
    $screenHeight = [Math]::Max(1, [int][Math]::Round($Height * $clientHeight / $FramebufferHeight))
    $bitmap = New-Object System.Drawing.Bitmap -ArgumentList $screenWidth, $screenHeight
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($screenX,$screenY,0,0,
            (New-Object System.Drawing.Size -ArgumentList $screenWidth,$screenHeight))
        $bitmap.Save($Path,[System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
    Assert-PathExists -Path $Path -Description "Static scene stability frame"
}

function Assert-SceneFramesStable {
    param([string]$First,[string]$Second)
    $a = New-Object System.Drawing.Bitmap -ArgumentList $First
    $b = New-Object System.Drawing.Bitmap -ArgumentList $Second
    try {
        if ($a.Width -ne $b.Width -or $a.Height -ne $b.Height) {
            throw "Static scene captures have different dimensions."
        }
        [long]$difference = 0
        [long]$samples = 0
        [long]$changed = 0
        for ($y = 0; $y -lt $a.Height; $y += 4) {
            for ($x = 0; $x -lt $a.Width; $x += 4) {
                $ca = $a.GetPixel($x,$y)
                $cb = $b.GetPixel($x,$y)
                $delta = [Math]::Abs([int]$ca.R-[int]$cb.R) +
                    [Math]::Abs([int]$ca.G-[int]$cb.G) +
                    [Math]::Abs([int]$ca.B-[int]$cb.B)
                $difference += $delta
                $samples++
                if ($delta -gt 9) { $changed++ }
            }
        }
        $mean = if ($samples -gt 0) { [double]$difference / (3.0 * $samples) } else { 999.0 }
        $ratio = if ($samples -gt 0) { [double]$changed / $samples } else { 1.0 }
        if ($mean -gt 0.85 -or $ratio -gt 0.02) {
            throw ("Stationary rendered viewport is not stable: mean channel delta={0:N3}, changed sample ratio={1:P2}." -f $mean,$ratio)
        }
        Write-Output ("[pass] Stationary rendered viewport is stable: mean channel delta={0:N3}, changed sample ratio={1:P2}" -f $mean,$ratio)
    }
    finally {
        $a.Dispose()
        $b.Dispose()
    }
}

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$gitCommand = Get-HenkaGitPath
$packageRoot = Join-Path $repoRoot "out\HenkaSandbox3D"
$packagedExe = Join-Path $packageRoot "HenkaSandbox3D.exe"
$assetsDir = Join-Path $packageRoot "assets"
$showcaseModelsDir = Join-Path $assetsDir "models"
$helpPath = Join-Path $packageRoot "docs\help\sandbox3d.md"
$readmePath = Join-Path $packageRoot "README.txt"
$packageInfoPath = Join-Path $packageRoot "PACKAGE_INFO.txt"
$settingsPath = Join-Path $packageRoot "user\sandbox3d.settings"
$logDir = Join-Path $repoRoot "build\test_tmp"
$stdoutPath = Join-Path $logDir "check_packaged_sandbox3d_stdout.log"
$stderrPath = Join-Path $logDir "check_packaged_sandbox3d_stderr.log"
$startupScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_startup.png"
$qaScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_controls_qa.png"
$nativeScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_native_panel.png"
$nativeAuthoringScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_native_authoring.png"
$contextMenuScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_context_menu.png"
$stabilityFirstPath = Join-Path $logDir "check_packaged_sandbox3d_stability_a.png"
$stabilitySecondPath = Join-Path $logDir "check_packaged_sandbox3d_stability_b.png"
$persistenceStdoutPath = Join-Path $logDir "check_packaged_sandbox3d_persistence_stdout.log"
$persistenceStderrPath = Join-Path $logDir "check_packaged_sandbox3d_persistence_stderr.log"

if (-not $NonInteractive) {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class NativeMethods {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT {
        public int X;
        public int Y;
    }

    public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    public const uint MOUSEEVENTF_LEFTUP = 0x0004;

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll")]
    public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll")]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int maxLength);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);

    public static IntPtr FindProcessWindow(uint processId, string title) {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hWnd, IntPtr lParam) {
            uint owner;
            GetWindowThreadProcessId(hWnd, out owner);
            if (owner == processId) {
                StringBuilder text = new StringBuilder(256);
                GetWindowText(hWnd, text, text.Capacity);
                if (text.ToString().Contains(title)) {
                    result = hWnd;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
'@
    $shell = New-Object -ComObject WScript.Shell
}

Write-Step "Checking packaged sandbox contents"
Assert-PathExists -Path $packageRoot -Description "Packaged sandbox folder"
Assert-PathExists -Path $packagedExe -Description "Packaged sandbox executable"
Assert-PathExists -Path $assetsDir -Description "Packaged assets folder"
Assert-PathExists -Path (Join-Path $assetsDir "branding\henka_engine_emblem.png") -Description "Packaged Henka emblem branding"
Assert-PathExists -Path (Join-Path $assetsDir "branding\henka_engine_lockup.png") -Description "Packaged Henka lockup branding"
foreach ($showcaseFile in @(
    "cheeky_giraffe.gltf",
    "cheeky_giraffe.bin",
    "giraffe_detail_normal.png",
    "giraffe_metallic_roughness.png",
    "original_realistic_rocket.gltf",
    "original_realistic_rocket.bin",
    "rocket_detail_normal.png",
    "rocket_metallic_roughness.png"
)) {
    Assert-PathExists -Path (Join-Path $showcaseModelsDir $showcaseFile) -Description "Packaged showcase asset $showcaseFile"
}
Assert-PathExists -Path (Join-Path $assetsDir "textures\residency\residency_64.png") -Description "Packaged residency stress fixtures"
Assert-PathExists -Path $helpPath -Description "Packaged offline help"
Assert-PathExists -Path $readmePath -Description "Packaged run guide"
Assert-PathExists -Path $packageInfoPath -Description "Packaged build marker"
$packageSchema = Get-PackageInfoValue -Path $packageInfoPath -Name "Package schema"
$sourceCommit = Get-PackageInfoValue -Path $packageInfoPath -Name "Source commit"
$sourceState = Get-PackageInfoValue -Path $packageInfoPath -Name "Source state"
$buildConfiguration = Get-PackageInfoValue -Path $packageInfoPath -Name "Build configuration"
$buildRef = Get-PackageInfoValue -Path $packageInfoPath -Name "Build ref"
$detachedHead = Get-PackageInfoValue -Path $packageInfoPath -Name "Detached HEAD"
$sourceHash = Get-PackageInfoValue -Path $packageInfoPath -Name "Source executable SHA-256"
$packagedHash = Get-PackageInfoValue -Path $packageInfoPath -Name "Packaged executable SHA-256"
$actualPackagedHash = (Get-FileHash -LiteralPath $packagedExe -Algorithm SHA256).Hash.ToLowerInvariant()
$currentCommitLines = @(& $gitCommand -C $repoRoot rev-parse HEAD 2>$null)
if ($LASTEXITCODE -ne 0 -or $currentCommitLines.Count -ne 1) { throw "Current commit could not be read for package verification." }
$currentCommit = ([string]$currentCommitLines[0]).Trim()
$currentStatusLines = @(& $gitCommand -C $repoRoot status --porcelain=v1 --untracked-files=all 2>$null)
if ($LASTEXITCODE -ne 0) { throw "Current source state could not be read for package verification." }
$currentSourceState = if ($currentStatusLines.Count -eq 0) { "clean" } else { "working-tree" }
if ($packageSchema -ne "3") { throw "Packaged build marker has an unsupported schema." }
if ($sourceCommit -ne $currentCommit) { throw "Packaged source commit does not match current HEAD." }
if ($sourceState -ne $currentSourceState) { throw "Packaged source state does not match the current working tree." }
if ($buildConfiguration -ne "Debug" -and $buildConfiguration -ne "Release") { throw "Packaged build configuration is invalid." }
if ([string]::IsNullOrWhiteSpace($buildRef)) { throw "Packaged build ref is missing." }
if ($detachedHead -ne "True" -and $detachedHead -ne "False") { throw "Packaged detached-HEAD value is invalid." }
if ($sourceHash -ne $packagedHash -or $packagedHash -ne $actualPackagedHash) { throw "Packaged executable provenance hash verification failed." }
Write-Output "[pass] Package provenance schema"
Write-Output "[pass] Package source commit"
Write-Output "[pass] Package build configuration"
Write-Output "[pass] Packaged executable SHA-256"
Assert-FileContains -Path $readmePath -Pattern "Use the in-window utilities" -Description "Packaged utility guidance"
Assert-FileContains -Path $readmePath -Pattern "panels open automatically" -Description "Packaged automatic panel guidance"
Assert-FileContains -Path $readmePath -Pattern "no selected scene object" -Description "Packaged startup no-selection guidance"
Assert-FileContains -Path $readmePath -Pattern "Physics QA explains Static, Dynamic, and Kinematic" -Description "Packaged physics body-type guidance"
Assert-FileContains -Path $readmePath -Pattern "Editable selected scene objects show a viewport transform highlight" -Description "Packaged editable selection highlight guidance"
Assert-FileContains -Path $readmePath -Pattern "Locked objects remain selectable for inspection without a transform highlight or gizmo" -Description "Packaged locked selection guidance"
Assert-FileContains -Path $readmePath -Pattern "Ground starts locked and requires an explicit Unlock Transform action before it can move" -Description "Packaged ground lock guidance"
Assert-FileContains -Path $readmePath -Pattern "Clearing selection also clears active transform-session ownership" -Description "Packaged transform-session ownership guidance"
Assert-FileContains -Path $readmePath -Pattern "release away from the outlines to open a separate native tool window" -Description "Packaged workspace guidance"
Assert-FileContains -Path $readmePath -Pattern "Open Native Panel Test from the Controls QA page to exercise a separate OS-level validation window" -Description "Packaged native test panel guidance"
Assert-FileContains -Path $readmePath -Pattern "If saved live workspace geometry is incompatible, Henka restores current safe defaults and rewrites them after a clean shutdown" -Description "Packaged workspace recovery guidance"
Assert-FileContains -Path $readmePath -Pattern "Close a detached tool window to return its panel to the last valid dock" -Description "Packaged workspace limitation guidance"
Assert-FileContains -Path $readmePath -Pattern "Use M or G, R, and S for action-based transforms" -Description "Packaged transform hotkey guidance"
Assert-FileContains -Path $readmePath -Pattern "status area" -Description "Packaged status guidance"
Assert-FileContains -Path $helpPath -Pattern "Utility panel" -Description "Packaged utility help"
Assert-FileContains -Path $helpPath -Pattern "Perspective 3D, Side 2.5D, Top-down 2.5D, and Isometric 2.5D" -Description "Packaged camera preset help"
Assert-FileContains -Path $helpPath -Pattern "Showcase Giraffe" -Description "Packaged showcase help"

if ($NonInteractive) {
    if ($ContractOnly) {
        Write-Step "Completing hosted package contract validation"
        Write-Output "[pass] Packaged sandbox contract validation completed."
        return
    }

    Write-Step "Running deterministic packaged startup smoke test"
    $smoke = Invoke-HenkaNativeCapture `
        -FilePath $packagedExe `
        -Arguments @("--smoke-test") `
        -WorkingDirectory $packageRoot `
        -Label "Run packaged sandbox smoke test"

    if ($smoke.Stdout -notmatch "Henka Engine Sandbox 3D") {
        throw "The packaged smoke test did not print the startup heading."
    }
    if ($smoke.Stdout -notmatch "Runtime mode: Packaged") {
        throw "The packaged smoke test did not report Packaged mode."
    }
    if ($smoke.Stdout -notmatch "Showcase assets: Cheeky Giraffe \(5 parts\), Original Realistic Rocket \(4 parts\)") {
        throw "The packaged smoke test did not load both showcase glTF scenes."
    }
    $terrainPassMatch = [regex]::Match($smoke.Stdout, "Terrain Rendered pass diagnostics: mask=0x([0-9a-fA-F]+) required=0x([0-9a-fA-F]+)")
    if (-not $terrainPassMatch.Success -or
        (([Convert]::ToUInt32($terrainPassMatch.Groups[1].Value, 16) -band
          [Convert]::ToUInt32($terrainPassMatch.Groups[2].Value, 16)) -ne
         [Convert]::ToUInt32($terrainPassMatch.Groups[2].Value, 16))) {
        throw "The packaged smoke test did not prove the required Terrain Rendered pass participation mask."
    }
    if ($smoke.Stdout -notmatch "Sandbox smoke test completed\.") {
        throw "The packaged smoke test did not reach its deterministic exit."
    }
    if ($smoke.Stderr -notmatch "leaving engine run loop") {
        throw "The packaged smoke test did not leave the engine run loop cleanly."
    }

    Write-Output "[pass] Deterministic packaged startup smoke test completed."

    Write-Step "Running bounded packaged Terrain stream stress"
    $terrainStreamStress = Invoke-HenkaNativeCapture `
        -FilePath $packagedExe `
        -Arguments @("--terrain-stream-stress") `
        -WorkingDirectory $packageRoot `
        -Label "Run packaged Terrain stream stress"

    if ($terrainStreamStress.Stdout -notmatch "Terrain stream stress: seeded=2x2 camera-window=\(0,0\)\+1 crossed=\(2,0\)->\(2,2\)->\(0,0\)" -or
        $terrainStreamStress.Stdout -notmatch "failed=0" -or
        $terrainStreamStress.Stdout -notmatch "render-return=valid" -or
        $terrainStreamStress.Stdout -notmatch "diagonal-render-return=valid" -or
        $terrainStreamStress.Stdout -notmatch "collision-overlap-return=valid" -or
        $terrainStreamStress.Stdout -notmatch "diagonal-collision-return=valid" -or
        $terrainStreamStress.Stdout -notmatch "Sandbox smoke test completed\.") {
        throw "The packaged Terrain stream stress did not prove the bounded camera crossing contract."
    }
    Write-Output "[pass] Bounded packaged Terrain stream stress completed."
    return
}

New-Item -ItemType Directory -Path $logDir -Force | Out-Null
Remove-Item `
    -LiteralPath @(
        $stdoutPath,
        $stderrPath,
        $startupScreenshotPath,
        $qaScreenshotPath,
        $nativeScreenshotPath,
        $nativeAuthoringScreenshotPath,
        $persistenceStdoutPath,
        $persistenceStderrPath) `
    -ErrorAction SilentlyContinue

$capturedProcess = $null
$process = $null
$mainWindowHandle = [System.IntPtr]::Zero
$uiAutomationVerified = $false
try {
    Write-Step "Launching the packaged sandbox"
    $capturedProcess = Start-HenkaCapturedProcess `
        -FilePath $packagedExe `
        -WorkingDirectory $packageRoot `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath
    $process = $capturedProcess.Process

    for ($index = 0; $index -lt 80 -and $mainWindowHandle -eq [System.IntPtr]::Zero; $index++) {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
        $mainWindowHandle = [NativeMethods]::FindProcessWindow([uint32]$process.Id, "Henka Engine Sandbox 3D")
    }

    if ($mainWindowHandle -eq [System.IntPtr]::Zero) {
        throw "The packaged sandbox window did not become available."
    }

    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Henka Engine Sandbox 3D" -TimeoutMilliseconds 15000)) {
        throw "Startup help heading was not found in the packaged sandbox output within the bounded 15-second startup window."
    }
    Assert-FileContains -Path $stdoutPath -Pattern "Henka Engine Sandbox 3D" -Description "Startup help heading"
    Assert-FileContains -Path $stdoutPath -Pattern "F4               Show or hide the sandbox panels" -Description "F4 help text"
    Assert-FileContains -Path $stdoutPath -Pattern "F5               Cycle View, Inspect, and Full Tools layouts" -Description "F5 help text"
    Assert-FileContains -Path $stdoutPath -Pattern "Runtime mode: Packaged" -Description "Packaged runtime mode output"
    Assert-FileContains -Path $stdoutPath -Pattern "Startup selection: None" -Description "Packaged startup no-selection output"
    Assert-FileContains -Path $stdoutPath -Pattern "Startup UI:" -Description "Startup UI cue"
    Assert-FileContains -Path $stdoutPath -Pattern "Controls and Physics QA are discoverable immediately" -Description "Startup auto panel cue"
    Assert-FileContains -Path $stdoutPath -Pattern "use the in-window .*utilities" -Description "Startup utility cue"
    Assert-FileContains -Path $stdoutPath -Pattern "recent actions and warnings appear" -Description "Startup status cue"

    Write-Step "Checking packaged UI open and close"
    Set-HenkaAutomationForeground -Handle $mainWindowHandle
    $null = $shell.AppActivate($process.Id)
    Start-Sleep -Milliseconds 600
    if (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -TimeoutMilliseconds 1500) {
        Assert-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -Description "Startup UI readiness output"
        Assert-FileContains -Path $stdoutPath -Pattern "View mode|Inspect mode|Full Tools mode" -Description "Layout mode output"
        Try-AssertFileContains -Path $stdoutPath -Pattern "Sandbox viewport:" -Description "Viewport output"
        $uiAutomationVerified = $true
    }
    else {
        [System.Windows.Forms.SendKeys]::SendWait('{F4}')
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox panel: shown" -TimeoutMilliseconds 2000)) {
            Set-HenkaAutomationForeground -Handle $mainWindowHandle
            Start-Sleep -Milliseconds 250
            [System.Windows.Forms.SendKeys]::SendWait('{F4}')
        }
        if (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox panel: shown" -TimeoutMilliseconds 4000) {
            Assert-FileContains -Path $stdoutPath -Pattern "Sandbox panel: shown" -Description "Panel open output"
            Assert-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -Description "UI readiness output after F4"
            Assert-FileContains -Path $stdoutPath -Pattern "View mode|Inspect mode|Full Tools mode" -Description "Layout mode output after F4"
            Try-AssertFileContains -Path $stdoutPath -Pattern "Sandbox viewport:" -Description "Viewport output after F4"
            $uiAutomationVerified = $true
        }
    }

    if ($uiAutomationVerified) {
        Write-Step "Capturing packaged startup workspace visual proof"
        Set-HenkaAutomationForeground -Handle $mainWindowHandle
        Start-Sleep -Milliseconds 350
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $startupScreenshotPath `
            -Description "Packaged startup workspace screenshot"

        Write-Step "Checking packaged UI click controls"

        foreach ($requiredPattern in @(
            "Sandbox UI ready:",
            "Sandbox viewport:",
            "Workspace UI geometry:",
            "Workspace header chrome:",
            "Grid control:",
            "Controls QA tab:",
            "Viewport shading controls:")) {
            if (-not (Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern $requiredPattern `
                    -TimeoutMilliseconds 4000)) {
                throw "Required packaged UI automation geometry was not reported: $requiredPattern"
            }
        }

        $framebufferMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Sandbox UI ready:.*framebuffer ([0-9]+)x([0-9]+)'
        $viewportMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Sandbox viewport: origin ([0-9]+),([0-9]+) size ([0-9]+)x([0-9]+)\.'
        $workspaceGeometryMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Workspace UI geometry: left=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+) right=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+) controls=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+) utility=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+) scene_objects=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+) details=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+)\.'
        $gridMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Grid control: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)'
        $qaTabMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Controls QA tab: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)'
        $shadingMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Viewport shading controls: x=([-0-9.]+) y=([-0-9.]+) button=([-0-9.]+) gap=([-0-9.]+)'

        if ($null -eq $framebufferMatch -or
            $null -eq $viewportMatch -or
            $null -eq $workspaceGeometryMatch -or
            $null -eq $gridMatch -or
            $null -eq $qaTabMatch -or
            $null -eq $shadingMatch) {
            throw "Packaged UI automation geometry could not be parsed."
        }

        $framebufferWidth =
            [int]$framebufferMatch.Groups[1].Value
        $framebufferHeight =
            [int]$framebufferMatch.Groups[2].Value

        $viewportX = [double]$viewportMatch.Groups[1].Value
        $viewportY = [double]$viewportMatch.Groups[2].Value
        $viewportWidth = [double]$viewportMatch.Groups[3].Value
        $viewportHeight = [double]$viewportMatch.Groups[4].Value

        $leftDockX =
            [double]$workspaceGeometryMatch.Groups[1].Value
        $leftDockY =
            [double]$workspaceGeometryMatch.Groups[2].Value
        $leftDockWidth =
            [double]$workspaceGeometryMatch.Groups[3].Value
        $leftDockHeight =
            [double]$workspaceGeometryMatch.Groups[4].Value
        $rightDockX =
            [double]$workspaceGeometryMatch.Groups[5].Value
        $rightDockY =
            [double]$workspaceGeometryMatch.Groups[6].Value
        $rightDockWidth =
            [double]$workspaceGeometryMatch.Groups[7].Value
        $rightDockHeight =
            [double]$workspaceGeometryMatch.Groups[8].Value
        $controlsX =
            [double]$workspaceGeometryMatch.Groups[9].Value
        $controlsY =
            [double]$workspaceGeometryMatch.Groups[10].Value
        $controlsWidth =
            [double]$workspaceGeometryMatch.Groups[11].Value
        $controlsHeight =
            [double]$workspaceGeometryMatch.Groups[12].Value
        $utilityX =
            [double]$workspaceGeometryMatch.Groups[13].Value
        $utilityY =
            [double]$workspaceGeometryMatch.Groups[14].Value
        $utilityWidth =
            [double]$workspaceGeometryMatch.Groups[15].Value
        $utilityHeight =
            [double]$workspaceGeometryMatch.Groups[16].Value
        $sceneObjectsX =
            [double]$workspaceGeometryMatch.Groups[17].Value
        $sceneObjectsY =
            [double]$workspaceGeometryMatch.Groups[18].Value
        $sceneObjectsWidth =
            [double]$workspaceGeometryMatch.Groups[19].Value
        $sceneObjectsHeight =
            [double]$workspaceGeometryMatch.Groups[20].Value
        $detailsX =
            [double]$workspaceGeometryMatch.Groups[21].Value
        $detailsY =
            [double]$workspaceGeometryMatch.Groups[22].Value
        $detailsWidth =
            [double]$workspaceGeometryMatch.Groups[23].Value
        $detailsHeight =
            [double]$workspaceGeometryMatch.Groups[24].Value

        $gridX =
            [double]$gridMatch.Groups[1].Value
        $gridY =
            [double]$gridMatch.Groups[2].Value
        $gridWidth =
            [double]$gridMatch.Groups[3].Value
        $gridHeight =
            [double]$gridMatch.Groups[4].Value

        $qaTabX =
            [double]$qaTabMatch.Groups[1].Value
        $qaTabY =
            [double]$qaTabMatch.Groups[2].Value
        $qaTabWidth =
            [double]$qaTabMatch.Groups[3].Value
        $qaTabHeight =
            [double]$qaTabMatch.Groups[4].Value

        $shadingX =
            [double]$shadingMatch.Groups[1].Value
        $shadingY =
            [double]$shadingMatch.Groups[2].Value
        $shadingButtonWidth =
            [double]$shadingMatch.Groups[3].Value
        $shadingGap =
            [double]$shadingMatch.Groups[4].Value
        $shadingGroupWidth =
            $shadingButtonWidth * 4.0 +
            $shadingGap * 3.0

        Assert-FramebufferRect `
            -Name "Left dock" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $leftDockX `
            -Y $leftDockY `
            -Width $leftDockWidth `
            -Height $leftDockHeight
        Assert-FramebufferRect `
            -Name "Right dock" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $rightDockX `
            -Y $rightDockY `
            -Width $rightDockWidth `
            -Height $rightDockHeight

        $panelRects = @(
            [pscustomobject]@{
                Name = "Controls panel"
                X = $controlsX
                Y = $controlsY
                Width = $controlsWidth
                Height = $controlsHeight
            },
            [pscustomobject]@{
                Name = "Utility panel"
                X = $utilityX
                Y = $utilityY
                Width = $utilityWidth
                Height = $utilityHeight
            },
            [pscustomobject]@{
                Name = "Scene Objects panel"
                X = $sceneObjectsX
                Y = $sceneObjectsY
                Width = $sceneObjectsWidth
                Height = $sceneObjectsHeight
            },
            [pscustomobject]@{
                Name = "Object Details panel"
                X = $detailsX
                Y = $detailsY
                Width = $detailsWidth
                Height = $detailsHeight
            }
        )
        $visiblePanelRects = @(
            $panelRects |
                Where-Object {
                    $_.Width -gt 0.0 -and $_.Height -gt 0.0
                }
        )
        if ($visiblePanelRects.Count -lt 2) {
            throw "Too few active workspace panel rectangles were reported."
        }
        foreach ($panelRect in $visiblePanelRects) {
            Assert-FramebufferRect `
                -Name $panelRect.Name `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $panelRect.X `
                -Y $panelRect.Y `
                -Width $panelRect.Width `
                -Height $panelRect.Height
        }

        function Assert-DockCoverage {
            param(
                [Parameter(Mandatory = $true)][string]$Name,
                [Parameter(Mandatory = $true)][double]$X,
                [Parameter(Mandatory = $true)][double]$Y,
                [Parameter(Mandatory = $true)][double]$Width,
                [Parameter(Mandatory = $true)][double]$Height,
                [Parameter(Mandatory = $true)][object[]]$Rectangles
            )

            $geometryTolerance = 1.1
            $gapTolerance = 6.1
            $matching = @(
                $Rectangles |
                    Where-Object {
                        [Math]::Abs($_.X - $X) -le $geometryTolerance -and
                        [Math]::Abs($_.Width - $Width) -le $geometryTolerance -and
                        $_.Y + $_.Height -gt $Y -and
                        $_.Y -lt $Y + $Height
                    } |
                    Sort-Object Y
            )
            if ($matching.Count -eq 0) {
                throw "$Name has no active section content."
            }
            if ([Math]::Abs($matching[0].Y - $Y) -gt $geometryTolerance) {
                throw "$Name has unused space above its first active section."
            }

            $coveredBottom = $Y
            foreach ($rect in $matching) {
                if ($rect.Y - $coveredBottom -gt $gapTolerance) {
                    throw "$Name contains an unexpected empty vertical region."
                }
                $rectBottom = $rect.Y + $rect.Height
                if ($rectBottom -gt $coveredBottom) {
                    $coveredBottom = $rectBottom
                }
            }
            if ([Math]::Abs(($Y + $Height) - $coveredBottom) -gt $geometryTolerance) {
                throw "$Name has unused space below its last active section."
            }
            Write-Output "[pass] $Name active sections cover the dock without empty voids"
        }

        Assert-DockCoverage `
            -Name "Left dock" `
            -X $leftDockX `
            -Y $leftDockY `
            -Width $leftDockWidth `
            -Height $leftDockHeight `
            -Rectangles $visiblePanelRects
        Assert-DockCoverage `
            -Name "Right dock" `
            -X $rightDockX `
            -Y $rightDockY `
            -Width $rightDockWidth `
            -Height $rightDockHeight `
            -Rectangles $visiblePanelRects
        Write-Output "[pass] Inactive merged tabs may report zero content rectangles safely"

        Write-Step "Checking imported showcase native authoring bridge"
        Set-HenkaAutomationForeground -Handle $mainWindowHandle
        [System.Windows.Forms.SendKeys]::SendWait('{F5}')
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox layout: Inspect" -TimeoutMilliseconds 4000)) {
            throw "F5 did not enter the Inspect layout required for native authoring validation."
        }
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring row:" -TimeoutMilliseconds 4000)) {
            throw "The Inspect layout did not expose a showcase primitive authoring row."
        }
        $nativeRowMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring row: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)\.'
        if ($null -eq $nativeRowMatch) {
            throw "The native authoring row geometry could not be parsed."
        }
        $nativeRowX = [double]$nativeRowMatch.Groups[2].Value
        $nativeRowY = [double]$nativeRowMatch.Groups[3].Value
        $nativeRowWidth = [double]$nativeRowMatch.Groups[4].Value
        $nativeRowHeight = [double]$nativeRowMatch.Groups[5].Value
        Assert-FramebufferRect `
            -Name "Native authoring showcase row" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeRowX `
            -Y $nativeRowY `
            -Width $nativeRowWidth `
            -Height $nativeRowHeight
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $nativeAuthoringScreenshotPath `
            -Description "Packaged native authoring pre-selection screenshot"
        $nativeSelectionObserved = $false
        for ($attempt = 0; $attempt -lt 3 -and -not $nativeSelectionObserved; ++$attempt) {
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeRowX + $nativeRowWidth * 0.5) `
                -FramebufferY ($nativeRowY + $nativeRowHeight * 0.5)
                $nativeSelectionObserved = Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern "Native authoring row clicked:" `
                    -TimeoutMilliseconds 2000
        }
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $nativeAuthoringScreenshotPath `
            -Description "Packaged native authoring post-selection screenshot"
        if (-not $nativeSelectionObserved) {
            $inspectViewportMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Sandbox viewport: origin ([0-9]+),([0-9]+) size ([0-9]+)x([0-9]+)\.'
            if ($null -ne $inspectViewportMatch) {
                $inspectViewportX = [double]$inspectViewportMatch.Groups[1].Value
                $inspectViewportY = [double]$inspectViewportMatch.Groups[2].Value
                $inspectViewportWidth = [double]$inspectViewportMatch.Groups[3].Value
                $inspectViewportHeight = [double]$inspectViewportMatch.Groups[4].Value
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($inspectViewportX + $inspectViewportWidth * 0.30) `
                    -FramebufferY ($inspectViewportY + $inspectViewportHeight * 0.45)
                $nativeSelectionObserved = Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern "Native authoring row clicked:" `
                    -TimeoutMilliseconds 3000
            }
        }
        if (-not $nativeSelectionObserved) {
            throw "Selecting the showcase row did not expose Object Details > Authoring > Make Editable."
        }
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring Make Editable control:" -TimeoutMilliseconds 3000)) {
            throw "The selected showcase authoring controls did not become visible in the prioritized Authoring group."
        }
        $nativeMakeEditableMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring Make Editable control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=180.0 height=24.0\.'
        if ($null -eq $nativeMakeEditableMatch) {
            throw "The Make Editable control geometry could not be parsed."
        }
        $nativeMakeEditableX = [double]$nativeMakeEditableMatch.Groups[2].Value
        $nativeMakeEditableY = [double]$nativeMakeEditableMatch.Groups[3].Value
        Assert-FramebufferRect `
            -Name "Native authoring Make Editable control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMakeEditableX `
            -Y $nativeMakeEditableY `
            -Width 180.0 `
            -Height 24.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMakeEditableX + 90.0) `
            -FramebufferY ($nativeMakeEditableY + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring dogfood: Make Editable converted" -TimeoutMilliseconds 5000)) {
            throw "Make Editable did not create the user-owned native authoring source."
        }
        Write-Output "[pass] Imported showcase primitive entered the user-facing native authoring workflow"
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring material control:" -TimeoutMilliseconds 3000)) {
            throw "The converted showcase did not expose the native material ownership control."
        }
        $nativeMaterialOwnershipMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring material control: name=(.+) own_x=([-0-9.]+) own_y=([-0-9.]+) width=100.0 height=24.0 owned=0\.'
        if ($null -eq $nativeMaterialOwnershipMatch) {
            throw "The native material ownership control geometry could not be parsed."
        }
        $nativeMaterialOwnershipX = [double]$nativeMaterialOwnershipMatch.Groups[2].Value
        $nativeMaterialOwnershipY = [double]$nativeMaterialOwnershipMatch.Groups[3].Value
        Assert-FramebufferRect `
            -Name "Native authoring material ownership control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMaterialOwnershipX `
            -Y $nativeMaterialOwnershipY `
            -Width 100.0 `
            -Height 24.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMaterialOwnershipX + 50.0) `
            -FramebufferY ($nativeMaterialOwnershipY + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring material: editable runtime definition adopted" -TimeoutMilliseconds 5000)) {
            throw "The showcase material was not promoted to a manager-owned editable definition."
        }
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring material controls:" -TimeoutMilliseconds 3000)) {
            throw "The native material editor controls did not become visible after ownership promotion."
        }
        $nativeMaterialControlsMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring material controls: name=(.+) tint_x=([-0-9.]+) texture_x=([-0-9.]+) y=([-0-9.]+) width=96.0 height=24.0\.'
        if ($null -eq $nativeMaterialControlsMatch) {
            throw "The native material editor control geometry could not be parsed."
        }
        $nativeMaterialTintX = [double]$nativeMaterialControlsMatch.Groups[2].Value
        $nativeMaterialTextureX = [double]$nativeMaterialControlsMatch.Groups[3].Value
        $nativeMaterialY = [double]$nativeMaterialControlsMatch.Groups[4].Value
        Assert-FramebufferRect `
            -Name "Native authoring material tint control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMaterialTintX `
            -Y $nativeMaterialY `
            -Width 96.0 `
            -Height 24.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMaterialTintX + 48.0) `
            -FramebufferY ($nativeMaterialY + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring material edited" -TimeoutMilliseconds 5000)) {
            throw "The user-facing native material parameter edit did not complete."
        }
        Assert-FramebufferRect `
            -Name "Native authoring texture control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMaterialTextureX `
            -Y $nativeMaterialY `
            -Width 96.0 `
            -Height 24.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMaterialTextureX + 48.0) `
            -FramebufferY ($nativeMaterialY + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring texture edited" -TimeoutMilliseconds 5000)) {
            throw "The user-facing native texture assignment did not complete."
        }
        Write-Output "[pass] User-facing native material and texture edits completed"
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring move control:" -TimeoutMilliseconds 3000)) {
            throw "The converted showcase did not expose a bounded component-edit control."
        }
        $nativeMoveMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring move control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
        if ($null -eq $nativeMoveMatch) {
            throw "The native authoring component-edit control geometry could not be parsed."
        }
        $nativeMoveX = [double]$nativeMoveMatch.Groups[2].Value
        $nativeMoveY = [double]$nativeMoveMatch.Groups[3].Value
        Assert-FramebufferRect `
            -Name "Native authoring component-edit control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMoveX `
            -Y $nativeMoveY `
            -Width 88.0 `
            -Height 24.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMoveX + 44.0) `
            -FramebufferY ($nativeMoveY + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring dogfood: component move edited" -TimeoutMilliseconds 5000)) {
            throw "The user-facing component edit did not update the native authoring source."
        }
        Write-Output "[pass] User-facing component edit changed the native showcase source"
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring project controls:" -TimeoutMilliseconds 3000)) {
            throw "The converted showcase did not expose bounded project save/reload controls."
        }
        $nativeProjectMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring project controls: name=(.+) save_x=([-0-9.]+) save_y=([-0-9.]+) reload_x=([-0-9.]+) reload_y=([-0-9.]+) width=140.0 height=24.0\.'
        if ($null -eq $nativeProjectMatch) {
            throw "The native authoring project control geometry could not be parsed."
        }
        $nativeSaveX = [double]$nativeProjectMatch.Groups[2].Value
        $nativeSaveY = [double]$nativeProjectMatch.Groups[3].Value
        $nativeReloadX = [double]$nativeProjectMatch.Groups[4].Value
        $nativeReloadY = [double]$nativeProjectMatch.Groups[5].Value
        Assert-FramebufferRect `
            -Name "Native authoring Save Project control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeSaveX `
            -Y $nativeSaveY `
            -Width 140.0 `
            -Height 24.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeSaveX + 70.0) `
            -FramebufferY ($nativeSaveY + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring dogfood: project saved" -TimeoutMilliseconds 5000)) {
            throw "The user-facing native authoring project save did not complete."
        }
        Write-Output "[pass] User-facing native authoring project save completed"
        Assert-FramebufferRect `
            -Name "Native authoring Reload Project control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeReloadX `
            -Y $nativeReloadY `
            -Width 140.0 `
            -Height 24.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeReloadX + 70.0) `
            -FramebufferY ($nativeReloadY + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring dogfood: project reloaded" -TimeoutMilliseconds 5000)) {
            throw "The user-facing native authoring project reload did not complete transactionally."
        }
        Write-Output "[pass] User-facing native authoring project reload completed transactionally"
        Start-Sleep -Milliseconds 350
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $nativeAuthoringScreenshotPath `
            -Description "Packaged native authoring screenshot"

        Write-Step "Checking section-header context menu"
        $contextMenuPattern = "Workspace context menu: section=Controls horizontal=available vertical=available"

        Click-FramebufferPointRight `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($controlsX + 18.0) `
            -FramebufferY ($controlsY + 13.0)

        $contextMenuObserved = Wait-FileContains `
            -Path $stdoutPath `
            -Pattern $contextMenuPattern `
            -TimeoutMilliseconds 4000

        if (-not $contextMenuObserved) {
            Write-Output "[retry] Controls context menu was not observed after the first verified right click; retrying once."

            Set-HenkaAutomationForeground -Handle $mainWindowHandle
            Start-Sleep -Milliseconds 250

            Click-FramebufferPointRight `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($controlsX + 18.0) `
                -FramebufferY ($controlsY + 13.0)

            $contextMenuObserved = Wait-FileContains `
                -Path $stdoutPath `
                -Pattern $contextMenuPattern `
                -TimeoutMilliseconds 4000
        }

        if (-not $contextMenuObserved) {
            throw (
                "Right-clicking the Controls header did not open the horizontal/vertical " +
                "section context menu after two verified real-input attempts.")
        }

        Write-Output "[pass] Controls section-header context menu opened from verified real mouse input"

        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $contextMenuScreenshotPath `
            -Description "Workspace context-menu screenshot"

        [System.Windows.Forms.SendKeys]::SendWait('{ESC}')
        Start-Sleep -Milliseconds 350
        Write-Step "Checking stationary rendered viewport stability"
        Start-Sleep -Milliseconds 1800
        $stableX = $viewportX + 8.0
        $stableY = $viewportY + 38.0
        $stableWidth = [Math]::Max(1.0, $viewportWidth - 16.0)
        $stableHeight = [Math]::Max(1.0, $viewportHeight - 82.0)
        Save-FramebufferRegionScreenshot `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $stableX -Y $stableY -Width $stableWidth -Height $stableHeight `
            -Path $stabilityFirstPath
        Start-Sleep -Milliseconds 700
        Save-FramebufferRegionScreenshot `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $stableX -Y $stableY -Width $stableWidth -Height $stableHeight `
            -Path $stabilitySecondPath
        Assert-SceneFramesStable -First $stabilityFirstPath -Second $stabilitySecondPath

        $headerChromeMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Workspace header chrome: controls=([a-z]+):([0-9]+) utility=([a-z]+):([0-9]+)\.'
        if ($null -eq $headerChromeMatch) {
            throw "Workspace header chrome state could not be parsed."
        }
        $controlsChrome = $headerChromeMatch.Groups[1].Value
        $controlsTabCount = [int]$headerChromeMatch.Groups[2].Value
        $utilityChrome = $headerChromeMatch.Groups[3].Value
        $utilityTabCount = [int]$headerChromeMatch.Groups[4].Value
        $expectedControlsChrome =
            if ($controlsTabCount -gt 1) { "tabs" } else { "compact" }
        $expectedUtilityChrome =
            if ($utilityTabCount -gt 1) { "tabs" } else { "compact" }
        if ($controlsChrome -ne $expectedControlsChrome -or
            $utilityChrome -ne $expectedUtilityChrome) {
            throw (
                "Workspace header chrome does not match the live topology " +
                "tab counts.")
        }
        Write-Output (
            "[pass] Workspace headers match live topology: " +
            "Controls=$controlsChrome/$controlsTabCount, " +
            "Utility=$utilityChrome/$utilityTabCount")

        Assert-FramebufferRect `
            -Name "Grid control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $gridX `
            -Y $gridY `
            -Width $gridWidth `
            -Height $gridHeight
        Assert-FramebufferRect `
            -Name "Controls QA tab" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $qaTabX `
            -Y $qaTabY `
            -Width $qaTabWidth `
            -Height $qaTabHeight
        Assert-FramebufferRect `
            -Name "Viewport shading controls" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $shadingX `
            -Y $shadingY `
            -Width $shadingGroupWidth `
            -Height 22.0

        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($gridX + $gridWidth * 0.5) `
            -FramebufferY ($gridY + $gridHeight * 0.5)
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($gridX + $gridWidth * 0.5) `
            -FramebufferY ($gridY + $gridHeight * 0.5)

        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($qaTabX + $qaTabWidth * 0.5) `
            -FramebufferY ($qaTabY + $qaTabHeight * 0.5)

        Write-Step "Capturing Controls QA page visual proof"
        Set-HenkaAutomationForeground -Handle $mainWindowHandle
        Start-Sleep -Milliseconds 350
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $qaScreenshotPath `
            -Description "Packaged Controls QA screenshot"

        if (-not (Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native Panel Test control:" `
                -TimeoutMilliseconds 4000)) {
            throw (
                "The Controls QA page did not report the Native Panel Test " +
                "control after the QA tab was activated.")
        }

        $nativeMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native Panel Test control: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)'
        if ($null -eq $nativeMatch) {
            throw "The Native Panel Test control geometry could not be parsed."
        }

        $nativeX =
            [double]$nativeMatch.Groups[1].Value
        $nativeY =
            [double]$nativeMatch.Groups[2].Value
        $nativeWidth =
            [double]$nativeMatch.Groups[3].Value
        $nativeHeight =
            [double]$nativeMatch.Groups[4].Value

        Assert-FramebufferRect `
            -Name "Native Panel Test control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeX `
            -Y $nativeY `
            -Width $nativeWidth `
            -Height $nativeHeight
        $nativeOpened = $false
        for ($attempt = 0;
             $attempt -lt 3 -and -not $nativeOpened;
             ++$attempt) {
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeX + $nativeWidth * 0.5) `
                -FramebufferY ($nativeY + $nativeHeight * 0.5)
            $nativeOpened = Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native Panel Test: opened" `
                -TimeoutMilliseconds 2000
        }
        if (-not $nativeOpened) {
            throw "The Native Panel Test control did not open the secondary native window after three framebuffer-aware attempts."
        }

        Assert-FileContains `
            -Path $stdoutPath `
            -Pattern "Native Panel Test: opened" `
            -Description "Native test panel open output"
        $nativeWindowHandle =
            [NativeMethods]::FindProcessWindow(
                [uint32]$process.Id,
                "Henka Native Panel Test")
        if ($nativeWindowHandle -eq [System.IntPtr]::Zero) {
            throw "The Native Panel Test window was not visible as a separate OS-level window."
        }

        Write-Output "[pass] Native test panel visible as a separate OS-level window"
        Write-Step "Capturing native panel visual proof"
        Set-HenkaAutomationForeground -Handle $nativeWindowHandle
        Start-Sleep -Milliseconds 350
        Save-WindowScreenshot `
            -Handle $nativeWindowHandle `
            -Path $nativeScreenshotPath `
            -Description "Packaged native panel screenshot"
        Set-HenkaAutomationForeground -Handle $mainWindowHandle
        Start-Sleep -Milliseconds 250

        [NativeMethods]::PostMessage(
            $nativeWindowHandle,
            0x0010,
            [System.IntPtr]::Zero,
            [System.IntPtr]::Zero) | Out-Null
        Start-Sleep -Milliseconds 500

        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeX + $nativeWidth * 0.5) `
            -FramebufferY ($nativeY + $nativeHeight * 0.5)
        Start-Sleep -Milliseconds 500

        if ((Select-String `
                -LiteralPath $stdoutPath `
                -Pattern "Native Panel Test: opened").Count -lt 2) {
            throw "The Native Panel Test window did not reopen after being closed."
        }
        Write-Output "[pass] Native test panel closes and reopens without closing the main sandbox"

        $shadingX =
            [double]$shadingMatch.Groups[1].Value
        $shadingY =
            [double]$shadingMatch.Groups[2].Value
        $shadingButtonWidth =
            [double]$shadingMatch.Groups[3].Value
        $shadingGap =
            [double]$shadingMatch.Groups[4].Value
        $shadingNames = @(
            "Wireframe",
            "Solid",
            "Material Preview",
            "Rendered")

        for ($modeIndex = 0;
             $modeIndex -lt $shadingNames.Count;
             ++$modeIndex) {
            $modeCenterX =
                $shadingX +
                ($shadingButtonWidth + $shadingGap) *
                [double]$modeIndex +
                $shadingButtonWidth * 0.5
            $modeCenterY =
                $shadingY + 11.0

            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX $modeCenterX `
                -FramebufferY $modeCenterY

            $expectedModePattern =
                "Viewport shading: " +
                [Regex]::Escape($shadingNames[$modeIndex]) +
                "\."
            if (-not (Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern $expectedModePattern `
                    -TimeoutMilliseconds 2000)) {
                throw "Viewport shading mode could not be confirmed: $($shadingNames[$modeIndex])"
            }
        }

        Click-WindowPoint `
            -Handle $mainWindowHandle `
            -OffsetX 230 `
            -OffsetY 60
        Click-WindowPoint `
            -Handle $mainWindowHandle `
            -OffsetX 100 `
            -OffsetY 610

        $uiClickChecks = @(
            @{
                Pattern = "Debug grid: hidden"
                Description = "UI debug grid click output"
            },
            @{
                Pattern = "Debug grid: shown"
                Description = "UI debug grid restore output"
            },
            @{
                Pattern = "Viewport shading: Wireframe\."
                Description = "UI Wireframe mode output"
            },
            @{
                Pattern = "Viewport shading: Solid\."
                Description = "UI Solid mode output"
            },
            @{
                Pattern = "Viewport shading: Material Preview\."
                Description = "UI Material Preview mode output"
            },
            @{
                Pattern = "Viewport shading: Rendered\."
                Description = "UI Rendered mode output"
            },
            @{
                Pattern = "Sandbox settings saved\."
                Description = "UI save settings output"
            }
        )

        $uiClickFailures = 0
        foreach ($check in $uiClickChecks) {
            if (-not (Try-AssertFileContains `
                    -Path $stdoutPath `
                    -Pattern $check.Pattern `
                    -Description $check.Description)) {
                $uiClickFailures++
            }
        }

        if ($uiClickFailures -gt 0) {
            Write-Output "[warn] Some packaged UI click checks could not be confirmed automatically. Manual packaged UI QA is still needed."
        }
        if (-not (Select-String -LiteralPath $stdoutPath -Pattern "Sandbox panel: shown" -Quiet)) {
            Set-HenkaAutomationForeground -Handle $mainWindowHandle
            Start-Sleep -Milliseconds 400
            [System.Windows.Forms.SendKeys]::SendWait('{F4}')
            if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox panel: hidden" -TimeoutMilliseconds 2000)) {
                Set-HenkaAutomationForeground -Handle $mainWindowHandle
                Start-Sleep -Milliseconds 250
                [System.Windows.Forms.SendKeys]::SendWait('{F4}')
            }
            if (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox panel: hidden" -TimeoutMilliseconds 4000) {
                Assert-FileContains -Path $stdoutPath -Pattern "Sandbox panel: hidden" -Description "Panel close output"
            }
            else {
                Write-Output "[warn] Automated F4 panel close could not be confirmed. Manual packaged UI QA is still needed."
            }
        }
    }
    else {
        Write-Output "[warn] Automated F4 panel open could not be confirmed. Manual packaged UI QA is still needed."
    }

    Write-Step "Checking clean close-window shutdown"
    [NativeMethods]::PostMessage($mainWindowHandle, 0x0010, [System.IntPtr]::Zero, [System.IntPtr]::Zero) | Out-Null
    if (-not $process.WaitForExit(10000)) {
        throw "The packaged sandbox did not exit within the expected time."
    }

    if (Wait-FileContains -Path $stderrPath -Pattern "leaving engine run loop" -TimeoutMilliseconds 3000) {
        Assert-FileContains -Path $stderrPath -Pattern "leaving engine run loop" -Description "Run loop shutdown log"
    }
    else {
        Write-Output "[warn] Clean close-window shutdown log output could not be confirmed automatically. Manual packaged shutdown QA is still useful."
    }
    if (-not (Try-AssertPathExists -Path $settingsPath -Description "Packaged settings file")) {
        Write-Output "[warn] Automated packaged close did not leave behind a settings file in this run. Manual packaged persistence QA is still needed."
    }

    Write-Step "Checking persisted live workspace settings recovery"
    $persistenceSmoke = Invoke-HenkaNativeCapture `
        -FilePath $packagedExe `
        -Arguments @("--smoke-test") `
        -WorkingDirectory $packageRoot `
        -Label "Run post-interactive packaged persistence smoke test"

    Write-Utf8NoBom `
        -Path $persistenceStdoutPath `
        -Content $persistenceSmoke.Stdout
    Write-Utf8NoBom `
        -Path $persistenceStderrPath `
        -Content $persistenceSmoke.Stderr

    if ($persistenceSmoke.Stdout -notmatch "Sandbox smoke test completed\.") {
        throw "The post-interactive packaged persistence smoke test did not complete."
    }
    if ($persistenceSmoke.Stderr -match
        "Unsafe or incompatible (workspace panel|workspace topology|live workspace) settings") {
        throw (
            "Unsafe or incompatible live workspace settings were reported " +
            "again after a clean interactive shutdown.")
    }

    Assert-PathExists `
        -Path $startupScreenshotPath `
        -Description "Packaged startup workspace visual proof"
    Assert-PathExists `
        -Path $qaScreenshotPath `
        -Description "Packaged Controls QA visual proof"
    Assert-PathExists `
        -Path $nativeScreenshotPath `
        -Description "Packaged native panel visual proof"
    Assert-PathExists `
        -Path $nativeAuthoringScreenshotPath `
        -Description "Packaged native authoring visual proof"
    Write-Output "[pass] Live workspace settings recovery persisted across relaunch"
    Write-Output "[pass] Packaged sandbox checks completed."
}
finally {
    if ($null -ne $capturedProcess) {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
    }
    elseif ($process -ne $null) {
        $process.Dispose()
    }
}
