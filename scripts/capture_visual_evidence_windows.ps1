param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$OutputDirectory = "",

    [string]$ExecutablePath = "",

    [switch]$IncludeStartupShowcase,

    [switch]$IncludeTerrain
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$executable = if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
    Join-Path $repoRoot ("build\examples\sandbox3d\{0}\henka_sandbox3d.exe" -f $Configuration)
}
else {
    [System.IO.Path]::GetFullPath($ExecutablePath)
}
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
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int command);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr insertAfter, int x, int y, int width, int height, uint flags);
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
    public static void ActivateSandboxWindow(IntPtr handle) {
        IntPtr topmost = new IntPtr(-1);
        IntPtr notTopmost = new IntPtr(-2);
        const uint SWP_NOSIZE = 0x0001;
        const uint SWP_NOMOVE = 0x0002;
        const uint SWP_SHOWWINDOW = 0x0040;
        ShowWindow(handle, 9);
        SetWindowPos(handle, topmost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetWindowPos(handle, notTopmost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        BringWindowToTop(handle);
        SetForegroundWindow(handle);
    }
}
"@

[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
Add-Type -AssemblyName System.Drawing
$modes = @(
    @{ Label = "solid"; Arguments = @("--capture-mode", "solid"); File = "same-camera-solid.png" },
    @{ Label = "material_preview"; Arguments = @("--capture-mode", "material_preview"); File = "same-camera-material-preview.png" },
    @{ Label = "rendered"; Arguments = @("--capture-mode", "rendered"); File = "same-camera-rendered.png" }
)
if ($IncludeStartupShowcase) {
    $modes = @(
        @{ Label = "startup"; Arguments = @(); File = "startup-showcase.png" }
    ) + $modes
}
$records = New-Object System.Collections.Generic.List[string]
$records.Add("Same-camera viewport evidence")
$records.Add("Source: $executable")
$records.Add("Camera policy: capture-mode runs use the same deterministic two-model showcase camera and never save capture-mode settings")
$records.Add("Modes: Solid, Material Preview, Rendered")
$records.Add("Startup evidence: optional ordinary startup camera with the default showcase models")
$records.Add("Terrain evidence: deterministic wide and four-region-corner cameras")
$records.Add("Capture: application window bounds copied from the desktop into repo-local generated output")

foreach ($mode in $modes) {
    $process = Start-HenkaProcess `
        -FilePath $executable `
        -Arguments $mode.Arguments `
        -WorkingDirectory (Split-Path -Parent $executable)
    $handle = [IntPtr]::Zero
    try {
        for ($attempt = 0; $attempt -lt 80 -and $handle -eq [IntPtr]::Zero; ++$attempt) {
            Start-Sleep -Milliseconds 250
            $process.Refresh()
            $handle = [HenkaVisualCaptureNativeMethods]::FindSandboxWindow([uint32]$process.Id)
        }
        if ($handle -eq [IntPtr]::Zero -or -not [HenkaVisualCaptureNativeMethods]::IsWindow($handle)) {
            throw "Sandbox window did not become available for $($mode.Label)."
        }
        [HenkaVisualCaptureNativeMethods]::ActivateSandboxWindow($handle)
        Start-Sleep -Milliseconds 1500
        $rect = New-Object HenkaVisualCaptureNativeMethods+RECT
        $dwmResult = [HenkaVisualCaptureNativeMethods]::DwmGetWindowAttribute(
            $handle,
            9,
            [ref]$rect,
            [System.Runtime.InteropServices.Marshal]::SizeOf($rect))
        if ($dwmResult -ne 0) {
            throw "Window bounds could not be read for $($mode.Label): $dwmResult"
        }
        $width = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top
        if ($width -le 0 -or $height -le 0) {
            throw "Window bounds were invalid for $($mode.Label)."
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
            $records.Add("$($mode.Label): $($mode.File) SHA-256=$hash bounds=$width`x$height")
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

if ($IncludeTerrain) {
    $terrainModes = @(
        @{ Label = "terrain_solid"; Arguments = @("--capture-terrain-mode", "solid"); File = "terrain-same-camera-solid.png" },
        @{ Label = "terrain_material_preview"; Arguments = @("--capture-terrain-mode", "material_preview"); File = "terrain-same-camera-material-preview.png" },
        @{ Label = "terrain_rendered"; Arguments = @("--capture-terrain-mode", "rendered"); File = "terrain-same-camera-rendered.png" },
        @{ Label = "terrain_corner_solid"; Arguments = @("--capture-terrain-view", "corner", "solid"); File = "terrain-corner-solid.png" },
        @{ Label = "terrain_corner_material_preview"; Arguments = @("--capture-terrain-view", "corner", "material_preview"); File = "terrain-corner-material-preview.png" },
        @{ Label = "terrain_corner_rendered"; Arguments = @("--capture-terrain-view", "corner", "rendered"); File = "terrain-corner-rendered.png" }
    )
    foreach ($terrainMode in $terrainModes) {
        $process = Start-HenkaProcess `
            -FilePath $executable `
            -Arguments $terrainMode.Arguments `
            -WorkingDirectory (Split-Path -Parent $executable)
        $handle = [IntPtr]::Zero
        try {
            for ($attempt = 0; $attempt -lt 80 -and $handle -eq [IntPtr]::Zero; ++$attempt) {
                Start-Sleep -Milliseconds 250
                $process.Refresh()
                $handle = [HenkaVisualCaptureNativeMethods]::FindSandboxWindow([uint32]$process.Id)
            }
            if ($handle -eq [IntPtr]::Zero -or -not [HenkaVisualCaptureNativeMethods]::IsWindow($handle)) {
                throw "Sandbox window did not become available for $($terrainMode.Label)."
            }
            [HenkaVisualCaptureNativeMethods]::ActivateSandboxWindow($handle)
            Start-Sleep -Milliseconds 1500
            $rect = New-Object HenkaVisualCaptureNativeMethods+RECT
            $dwmResult = [HenkaVisualCaptureNativeMethods]::DwmGetWindowAttribute(
                $handle,
                9,
                [ref]$rect,
                [System.Runtime.InteropServices.Marshal]::SizeOf($rect))
            if ($dwmResult -ne 0) {
                throw "Window bounds could not be read for $($terrainMode.Label): $dwmResult"
            }
            $width = $rect.Right - $rect.Left
            $height = $rect.Bottom - $rect.Top
            if ($width -le 0 -or $height -le 0) {
                throw "Window bounds were invalid for $($terrainMode.Label)."
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
                $path = Join-Path $OutputDirectory $terrainMode.File
                $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
                $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
                $records.Add("$($terrainMode.Label): $($terrainMode.File) SHA-256=$hash bounds=$width`x$height")
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
}

if ($IncludeTerrain) {
    & (Join-Path $PSScriptRoot "check_terrain_visual_evidence_windows.ps1") `
        -InputDirectory $OutputDirectory
    & (Join-Path $PSScriptRoot "check_terrain_corner_visual_evidence_windows.ps1") `
        -InputDirectory $OutputDirectory
}

$records | Set-Content -LiteralPath (Join-Path $OutputDirectory "INDEX.txt")
Write-Host "[pass] Same-camera Solid, Material Preview, and Rendered evidence captured in $OutputDirectory"
