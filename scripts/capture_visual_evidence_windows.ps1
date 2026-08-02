param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$OutputDirectory = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$executable = Join-Path $repoRoot ("build\examples\sandbox3d\{0}\henka_sandbox3d.exe" -f $Configuration)
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot "build\visual_evidence"
}
else {
    $OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
}

if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Sandbox executable was not found: $executable"
}

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class HenkaVisualCaptureNativeMethods
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    public delegate bool EnumWindowsProc(IntPtr handle, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr handle, out uint processId);
    [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr handle, System.Text.StringBuilder text, int capacity);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr handle, int attribute, out RECT rect, int size);
    public static IntPtr FindSandboxWindow(uint processId) {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr handle, IntPtr parameter) {
            uint owner;
            GetWindowThreadProcessId(handle, out owner);
            if (owner == processId) {
                System.Text.StringBuilder text = new System.Text.StringBuilder(256);
                GetWindowText(handle, text, text.Capacity);
                if (text.ToString().Contains("Henka Engine Sandbox 3D")) { result = handle; return false; }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
"@

[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
Add-Type -AssemblyName System.Drawing
$modes = @(
    @{ Argument = "solid"; File = "same-camera-solid.png" },
    @{ Argument = "material_preview"; File = "same-camera-material-preview.png" },
    @{ Argument = "rendered"; File = "same-camera-rendered.png" }
)
$records = New-Object System.Collections.Generic.List[string]
$records.Add("Same-camera viewport evidence")
$records.Add("Source: $executable")
$records.Add("Camera policy: each run loads the same persisted camera and never saves capture-mode settings")
$records.Add("Modes: Solid, Material Preview, Rendered")
$records.Add("Capture: application window bounds copied from the desktop into repo-local generated output")

foreach ($mode in $modes) {
    $process = Start-HenkaProcess `
        -FilePath $executable `
        -Arguments @("--capture-mode", $mode.Argument) `
        -WorkingDirectory (Split-Path -Parent $executable)
    $handle = [IntPtr]::Zero
    try {
        for ($attempt = 0; $attempt -lt 80 -and $handle -eq [IntPtr]::Zero; ++$attempt) {
            Start-Sleep -Milliseconds 250
            $process.Refresh()
            $handle = [HenkaVisualCaptureNativeMethods]::FindSandboxWindow([uint32]$process.Id)
        }
        if ($handle -eq [IntPtr]::Zero -or -not [HenkaVisualCaptureNativeMethods]::IsWindow($handle)) {
            throw "Sandbox window did not become available for $($mode.Argument)."
        }
        [HenkaVisualCaptureNativeMethods]::SetForegroundWindow($handle) | Out-Null
        Start-Sleep -Milliseconds 1500
        $rect = New-Object HenkaVisualCaptureNativeMethods+RECT
        $dwmResult = [HenkaVisualCaptureNativeMethods]::DwmGetWindowAttribute(
            $handle,
            9,
            [ref]$rect,
            [System.Runtime.InteropServices.Marshal]::SizeOf($rect))
        if ($dwmResult -ne 0) {
            throw "Window bounds could not be read for $($mode.Argument): $dwmResult"
        }
        $width = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top
        if ($width -le 0 -or $height -le 0) {
            throw "Window bounds were invalid for $($mode.Argument)."
        }
        $bitmap = New-Object System.Drawing.Bitmap($width, $height)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
            }
            finally {
                $graphics.Dispose()
            }
            $path = Join-Path $OutputDirectory $mode.File
            $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
            $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
            $records.Add("$($mode.Argument): $($mode.File) SHA-256=$hash bounds=$width`x$height")
        }
        finally {
            $bitmap.Dispose()
        }
    }
    finally {
        if ($null -ne $process -and -not $process.HasExited) {
            if ($handle -ne [IntPtr]::Zero) {
                [HenkaVisualCaptureNativeMethods]::PostMessage($handle, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
            }
            if (-not $process.WaitForExit(10000)) {
                Stop-HenkaProcessTree -ProcessId $process.Id
            }
        }
        if ($null -ne $process) {
            $process.Dispose()
        }
    }
}

$records | Set-Content -LiteralPath (Join-Path $OutputDirectory "INDEX.txt")
Write-Host "[pass] Same-camera Solid, Material Preview, and Rendered evidence captured in $OutputDirectory"
