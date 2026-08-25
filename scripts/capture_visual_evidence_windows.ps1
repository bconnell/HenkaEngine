param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$OutputDirectory = "",

    [string]$ExecutablePath = "",

    [ValidateSet("FULL_SHOWCASE", "GIRAFFE_INSPECTION", "GEOMETRY_SOLID", "REALISM_REFERENCE")]
    [string]$EvidenceProfile = "FULL_SHOWCASE",

    [ValidateSet("wide", "close")]
    [string]$ReferenceView = "wide",

    [switch]$IncludeStartupShowcase,

    [switch]$IncludeGiraffeInspection,

    [switch]$IncludeTerrain,

    # Default capture is background-safe and must not take the user's
    # foreground window.  Use this only for a test of Windows foreground
    # integration itself.
    [switch]$AllowForegroundIntegration
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
$captureRuntimeDirectory = Join-Path $repoRoot ("build\test_tmp\visual-evidence-runtime-" + [Guid]::NewGuid().ToString("N"))
$captureExecutable = Join-Path $captureRuntimeDirectory "henka_sandbox3d.exe"
$captureAssets = Join-Path (Split-Path -Parent $executable) "assets"
if (-not (Test-Path -LiteralPath $captureAssets -PathType Container)) {
    throw "Sandbox runtime assets were not found beside the executable: $captureAssets"
}
New-Item -ItemType Directory -Path $captureRuntimeDirectory -Force | Out-Null
Copy-Item -LiteralPath $executable -Destination $captureExecutable -Force
Copy-Item -LiteralPath $captureAssets -Destination (Join-Path $captureRuntimeDirectory "assets") -Recurse -Force
if ($EvidenceProfile -eq "FULL_SHOWCASE" -and -not $IncludeStartupShowcase) {
    throw "FULL_SHOWCASE evidence requires -IncludeStartupShowcase. Use GIRAFFE_INSPECTION for inspection-only captures."
}
if ($EvidenceProfile -eq "GEOMETRY_SOLID" -and -not $IncludeStartupShowcase) {
    throw "GEOMETRY_SOLID evidence requires -IncludeStartupShowcase so ordinary startup readiness is verified."
}
if ($EvidenceProfile -eq "GEOMETRY_SOLID") {
    $IncludeGiraffeInspection = $true
}
if ($EvidenceProfile -eq "REALISM_REFERENCE" -and
    ($IncludeStartupShowcase -or $IncludeGiraffeInspection -or $IncludeTerrain)) {
    throw "REALISM_REFERENCE evidence is a dedicated reference-scene capture and cannot include showcase or terrain extras."
}

$script:allowForegroundIntegration = $AllowForegroundIntegration.IsPresent
if (-not $script:allowForegroundIntegration) {
    Write-Output "[safe] Visual evidence capture will not acquire foreground focus."
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
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int command);
    [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr hWnd, int command);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr hWnd, bool altTab);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr insertAfter, int x, int y, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint flags);
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
        ShowWindowAsync(handle, 9);
        SetWindowPos(handle, topmost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetWindowPos(handle, notTopmost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        BringWindowToTop(handle);
        SwitchToThisWindow(handle, true);
        SetForegroundWindow(handle);
    }
}
"@

function Wait-HenkaCaptureReady {
    param(
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$Label
    )

    for ($attempt = 0; $attempt -lt 200; ++$attempt) {
        if (Test-Path -LiteralPath $StdoutPath -PathType Leaf) {
            $line = Select-String -LiteralPath $StdoutPath -Pattern '^CAPTURE_READY(_REFERENCE)? ' |
                Select-Object -Last 1
            if ($null -ne $line) {
                return $line.Line
            }
        }
        if ($Process.HasExited) {
            throw "Sandbox exited before bounded capture readiness for $Label."
        }
        Start-Sleep -Milliseconds 100
    }
    throw "Sandbox did not report bounded capture readiness for $Label within 20 seconds."
}

function Assert-HenkaSandboxCaptureReady {
    param(
        [Parameter(Mandatory = $true)][IntPtr]$Handle,
        [Parameter(Mandatory = $true)][string]$Label
    )

    # The capture process owns this window. Restore its visibility without
    # assigning foreground focus so PrintWindow can render it in background-
    # safe mode, even when the process was created with a hidden/minimized
    # initial presentation state.
    for ($attempt = 0; $attempt -lt 20; ++$attempt) {
        [HenkaVisualCaptureNativeMethods]::ShowWindow($Handle, 9)
        if ([HenkaVisualCaptureNativeMethods]::IsWindowVisible($Handle) -and
            -not [HenkaVisualCaptureNativeMethods]::IsIconic($Handle)) {
            break
        }
        Start-Sleep -Milliseconds 25
    }

    if (-not [HenkaVisualCaptureNativeMethods]::IsWindowVisible($Handle) -or
        [HenkaVisualCaptureNativeMethods]::IsIconic($Handle)) {
        throw (
            "Background-safe visual capture is unavailable for ${Label}: " +
            "the Sandbox window is hidden or minimized. No focus change was attempted.")
    }

    if (-not $script:allowForegroundIntegration) {
        return
    }

    for ($attempt = 0; $attempt -lt 20; ++$attempt) {
        [HenkaVisualCaptureNativeMethods]::ActivateSandboxWindow($Handle)
        if ([HenkaVisualCaptureNativeMethods]::GetForegroundWindow() -eq $Handle) {
            return
        }
        Start-Sleep -Milliseconds 50
    }
    throw "Sandbox window did not become the foreground capture owner for $Label."
}

function Capture-HenkaWindowBitmap {
    param(
        [Parameter(Mandatory = $true)][IntPtr]$Handle,
        [Parameter(Mandatory = $true)][HenkaVisualCaptureNativeMethods+RECT]$Rect,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $width = $Rect.Right - $Rect.Left
    $height = $Rect.Bottom - $Rect.Top
    if ($width -le 0 -or $height -le 0) {
        throw "Window bounds were invalid for $Label."
    }

    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        if ($script:allowForegroundIntegration) {
            $graphics.CopyFromScreen($Rect.Left, $Rect.Top, 0, 0, $bitmap.Size)
        }
        else {
            $deviceContext = $graphics.GetHdc()
            try {
                if (-not [HenkaVisualCaptureNativeMethods]::PrintWindow(
                        $Handle,
                        $deviceContext,
                        2)) {
                    throw (
                        "Background-safe visual capture is unavailable for ${Label}: " +
                        "the window did not render through PrintWindow. " +
                        "No desktop or foreground capture was attempted.")
                }
            }
            finally {
                $graphics.ReleaseHdc($deviceContext) | Out-Null
            }
        }
    }
    catch {
        $bitmap.Dispose()
        throw
    }
    finally {
        $graphics.Dispose()
    }
    return $bitmap
}

function Assert-HenkaCaptureMetadata {
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Label,
        [ValidateSet("pair", "giraffe", "rocket")]
        [string]$ExpectedSubject = "pair"
    )

    $pattern = '^\s*CAPTURE_READY mode=(?<mode>[a-z_]+) viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>\d+) aspect=(?<aspect>[-0-9.]+) camera_position=(?<px>[-0-9.]+),(?<py>[-0-9.]+),(?<pz>[-0-9.]+) yaw=(?<yaw>[-0-9.]+) pitch=(?<pitch>[-0-9.]+) roll=(?<roll>[-0-9.]+) fov=(?<fov>[-0-9.]+) .* giraffe_screen=(?<gminx>[-0-9.]+),(?<gminy>[-0-9.]+),(?<gmaxx>[-0-9.]+),(?<gmaxy>[-0-9.]+) rocket_screen=(?<rminx>[-0-9.]+),(?<rminy>[-0-9.]+),(?<rmaxx>[-0-9.]+),(?<rmaxy>[-0-9.]+) combined_midpoint=(?<mx>[-0-9.]+),(?<my>[-0-9.]+) giraffe_parts=(?<gp>\d+) rocket_parts=(?<rp>\d+) giraffe_sss_regions=(?<sss>\d+) giraffe_normal_texture_regions=(?<normal>\d+) giraffe_normal_texture_loaded=(?<loaded>\d+) giraffe_normal_texture_fallbacks=(?<fallback>\d+) giraffe_thickness_texture_regions=(?<thickness>\d+) giraffe_thickness_texture_loaded=(?<thicknessLoaded>\d+) giraffe_thickness_texture_fallbacks=(?<thicknessFallback>\d+) settled_frames=(?<sf>\d+) giraffe_provenance=(?<giraffeProvenance>[A-Z_]+) rocket_provenance=(?<rocketProvenance>[A-Z_]+) preset_applied=(?<presetApplied>[01]) capture_subject=(?<captureSubject>pair|giraffe|rocket) draw_expected=1\s*$'
    $match = [regex]::Match($Line, $pattern)
    if (-not $match.Success) {
        throw "Capture readiness metadata was malformed for $Label."
    }
    if ($match.Groups["captureSubject"].Value -ne $ExpectedSubject) {
        throw "Capture readiness metadata targeted $($match.Groups['captureSubject'].Value) but $Label requires $ExpectedSubject."
    }
    if ([int]$match.Groups["sss"].Value -lt 1 -or
        [int]$match.Groups["gp"].Value -lt 13 -or
        [int]$match.Groups["rp"].Value -lt 13 -or
        [int]$match.Groups["normal"].Value -ne [int]$match.Groups["sss"].Value -or
        [int]$match.Groups["loaded"].Value -ne [int]$match.Groups["normal"].Value -or
        [int]$match.Groups["fallback"].Value -ne 0 -or
        [int]$match.Groups["thickness"].Value -ne [int]$match.Groups["sss"].Value -or
        [int]$match.Groups["thicknessLoaded"].Value -ne [int]$match.Groups["thickness"].Value -or
        [int]$match.Groups["thicknessFallback"].Value -ne 0) {
        throw "Capture readiness metadata did not prove the showcase material dependencies for $Label."
    }
    if ($match.Groups["giraffeProvenance"].Value -notin @("GENERATED_TEST_FIXTURE", "IMPORT_COMPATIBILITY_ASSET", "HENKA_NATIVE_GENERATED_FIXTURE", "HENKA_NATIVE_EDITED_FIXTURE", "HENKA_NATIVE_AUTHORED") -or
        $match.Groups["rocketProvenance"].Value -notin @("GENERATED_TEST_FIXTURE", "IMPORT_COMPATIBILITY_ASSET", "HENKA_NATIVE_GENERATED_FIXTURE", "HENKA_NATIVE_EDITED_FIXTURE", "HENKA_NATIVE_AUTHORED") -or
        $match.Groups["presetApplied"].Value -ne "0") {
        throw "Capture readiness metadata reported an unknown or preset-determined geometry provenance for $Label."
    }

    $width = [int]$match.Groups["vw"].Value
    $height = [int]$match.Groups["vh"].Value
    $pitch = [double]::Parse($match.Groups["pitch"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $roll = [double]::Parse($match.Groups["roll"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $midpointX = [double]::Parse($match.Groups["mx"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $midpointY = [double]::Parse($match.Groups["my"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $marginX = $width * 0.04
    $marginY = $height * 0.04
    $rectangles = if ($ExpectedSubject -eq "pair") {
        @(
            [pscustomobject]@{ Name = "giraffe"; MinX = "gminx"; MinY = "gminy"; MaxX = "gmaxx"; MaxY = "gmaxy" },
            [pscustomobject]@{ Name = "rocket"; MinX = "rminx"; MinY = "rminy"; MaxX = "rmaxx"; MaxY = "rmaxy" }
        )
    }
    elseif ($ExpectedSubject -eq "rocket") {
        @([pscustomobject]@{ Name = "rocket"; MinX = "rminx"; MinY = "rminy"; MaxX = "rmaxx"; MaxY = "rmaxy" })
    }
    else {
        @([pscustomobject]@{ Name = "giraffe"; MinX = "gminx"; MinY = "gminy"; MaxX = "gmaxx"; MaxY = "gmaxy" })
    }

    if ([Math]::Abs($pitch) -gt 0.001 -or [Math]::Abs($roll) -gt 0.001) {
        throw "Capture camera is not level for $Label (pitch=$pitch roll=$roll)."
    }
    if ($ExpectedSubject -eq "pair" -and
        ([Math]::Abs($midpointX - ($width / 2.0)) -gt ($width * 0.02) -or
         [Math]::Abs($midpointY - ($height / 2.0)) -gt ($height * 0.02))) {
        throw "Showcase midpoint is not centered for $Label (midpoint=$midpointX,$midpointY viewport=$($width)x$($height))."
    }
    if ($ExpectedSubject -ne "pair") {
        $subjectMinimumX = if ($ExpectedSubject -eq "rocket") {
            [double]::Parse($match.Groups["rminx"].Value, [Globalization.CultureInfo]::InvariantCulture)
        }
        else {
            [double]::Parse($match.Groups["gminx"].Value, [Globalization.CultureInfo]::InvariantCulture)
        }
        $subjectMinimumY = if ($ExpectedSubject -eq "rocket") {
            [double]::Parse($match.Groups["rminy"].Value, [Globalization.CultureInfo]::InvariantCulture)
        }
        else {
            [double]::Parse($match.Groups["gminy"].Value, [Globalization.CultureInfo]::InvariantCulture)
        }
        $subjectMaximumX = if ($ExpectedSubject -eq "rocket") {
            [double]::Parse($match.Groups["rmaxx"].Value, [Globalization.CultureInfo]::InvariantCulture)
        }
        else {
            [double]::Parse($match.Groups["gmaxx"].Value, [Globalization.CultureInfo]::InvariantCulture)
        }
        $subjectMaximumY = if ($ExpectedSubject -eq "rocket") {
            [double]::Parse($match.Groups["rmaxy"].Value, [Globalization.CultureInfo]::InvariantCulture)
        }
        else {
            [double]::Parse($match.Groups["gmaxy"].Value, [Globalization.CultureInfo]::InvariantCulture)
        }
        $subjectMidpointX = ($subjectMinimumX + $subjectMaximumX) / 2.0
        $subjectMidpointY = ($subjectMinimumY + $subjectMaximumY) / 2.0
        if ([Math]::Abs($subjectMidpointX - ($width / 2.0)) -gt ($width * 0.02) -or
            [Math]::Abs($subjectMidpointY - ($height / 2.0)) -gt ($height * 0.02)) {
            throw "$ExpectedSubject subject is not centered for $Label (midpoint=$subjectMidpointX,$subjectMidpointY viewport=$($width)x$($height))."
        }
    }
    foreach ($rectangle in $rectangles) {
        $name = $rectangle.Name
        $minX = [double]::Parse($match.Groups[$rectangle.MinX].Value, [Globalization.CultureInfo]::InvariantCulture)
        $minY = [double]::Parse($match.Groups[$rectangle.MinY].Value, [Globalization.CultureInfo]::InvariantCulture)
        $maxX = [double]::Parse($match.Groups[$rectangle.MaxX].Value, [Globalization.CultureInfo]::InvariantCulture)
        $maxY = [double]::Parse($match.Groups[$rectangle.MaxY].Value, [Globalization.CultureInfo]::InvariantCulture)
        if ($minX -lt $marginX -or $minY -lt $marginY -or
            $maxX -gt ($width - $marginX) -or $maxY -gt ($height - $marginY) -or
            $maxX -le $minX -or $maxY -le $minY) {
            throw "$name projected bounds are outside the bounded capture frame for $Label."
        }
    }
    if ([int]$match.Groups["gp"].Value -lt 1 -or
        [int]$match.Groups["rp"].Value -lt 1 -or
        [int]$match.Groups["sf"].Value -lt 3) {
        throw "Capture readiness did not prove the subject meshes and settled frames for $Label."
    }

    return [pscustomobject]@{
        Canonical = ($Line -replace 'mode=[^ ]+', 'mode=shared')
        Mode = $match.Groups["mode"].Value
    }
}

function Assert-HenkaReferenceCaptureMetadata {
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][string]$ExpectedView
    )

    $pattern = '^\s*CAPTURE_READY_REFERENCE mode=(?<mode>[a-z_]+) view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>\d+) aspect=(?<aspect>[-0-9.]+) camera_position=(?<px>[-0-9.]+),(?<py>[-0-9.]+),(?<pz>[-0-9.]+) yaw=(?<yaw>[-0-9.]+) pitch=(?<pitch>[-0-9.]+) roll=(?<roll>[-0-9.]+) fov=(?<fov>[-0-9.]+) reference_bounds=(?<cx>[-0-9.]+),(?<cy>[-0-9.]+),(?<cz>[-0-9.]+),(?<ex>[-0-9.]+),(?<ey>[-0-9.]+),(?<ez>[-0-9.]+) reference_midpoint=(?<mx>[-0-9.]+),(?<my>[-0-9.]+) reference_count=(?<count>\d+) settled_frames=(?<sf>\d+) draw_expected=1\s*$'
    $match = [regex]::Match($Line, $pattern)
    if (-not $match.Success) {
        throw "Reference capture readiness metadata was malformed for $Label."
    }
    $expectedLayout = if ($ExpectedView -eq "close") { "close_grid" } else { "wide_row" }
    if ($match.Groups["view"].Value -ne $ExpectedView -or
        $match.Groups["layout"].Value -ne $expectedLayout -or
        [int]$match.Groups["count"].Value -ne 9 -or
        [int]$match.Groups["sf"].Value -lt 3) {
        throw "Reference capture readiness metadata is incomplete for $Label."
    }
    return [pscustomobject]@{
        Canonical = ($Line -replace 'mode=[^ ]+', 'mode=shared')
        Mode = $match.Groups["mode"].Value
        View = $match.Groups["view"].Value
        Layout = $match.Groups["layout"].Value
    }
}

[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
Add-Type -AssemblyName System.Drawing
$modes = @(
    @{ Label = "solid"; Arguments = @("--capture-mode", "solid"); File = "same-camera-solid.png" },
    @{ Label = "material_preview"; Arguments = @("--capture-mode", "material_preview"); File = "same-camera-material-preview.png" },
    @{ Label = "rendered"; Arguments = @("--capture-mode", "rendered"); File = "same-camera-rendered.png" }
)
if ($EvidenceProfile -eq "REALISM_REFERENCE") {
    $modes = @(
        @{ Label = "realism_reference_solid"; Arguments = @("--capture-realism-reference", $ReferenceView, "solid"); File = "realism-reference-$ReferenceView-solid.png" },
        @{ Label = "realism_reference_material_preview"; Arguments = @("--capture-realism-reference", $ReferenceView, "material_preview"); File = "realism-reference-$ReferenceView-material-preview.png" },
        @{ Label = "realism_reference_rendered"; Arguments = @("--capture-realism-reference", $ReferenceView, "rendered"); File = "realism-reference-$ReferenceView-rendered.png" }
    )
}
if ($EvidenceProfile -eq "GEOMETRY_SOLID") {
    $modes = @(
        @{ Label = "solid"; Arguments = @("--capture-mode", "solid"); File = "same-camera-solid.png" }
    )
}
if ($IncludeStartupShowcase) {
    $modes = @(
            @{ Label = "startup"; Arguments = @("--capture-startup"); File = "startup-showcase.png" }
    ) + $modes
}
$records = New-Object System.Collections.Generic.List[string]
$records.Add("Same-camera viewport evidence")
$records.Add("Source: $executable")
$records.Add("Isolated runtime: $captureExecutable")
$records.Add("Evidence profile: $EvidenceProfile")
$records.Add("Camera policy: capture-mode runs use the same deterministic two-model showcase camera and never save capture-mode settings")
$records.Add("Modes: Solid, Material Preview, Rendered")
$records.Add("Startup evidence: application-provided readiness for the ordinary startup scene")
$records.Add("Geometry authority: GEOMETRY_SOLID uses neutral Solid captures only; generated fixtures and asset-specific presets are not native-authored proof")
$records.Add("Giraffe inspection: optional close front, three-quarter, profile, and wide Rendered views plus front Material Preview")
$records.Add("Terrain evidence: deterministic wide, close-material, and four-region-corner cameras")
$records.Add("Realism reference: nine deterministic PBR material subjects; view=$ReferenceView")
$capturePolicy = if ($script:allowForegroundIntegration) {
        "foreground desktop capture was explicitly enabled"
    }
    else {
        "background-safe PrintWindow capture; no foreground or desktop ownership"
    }
$records.Add("Capture policy: $capturePolicy")
$captureMetadata = New-Object System.Collections.Generic.List[object]

foreach ($mode in $modes) {
    $capturedProcess = $null
    $stdoutPath = Join-Path $OutputDirectory "$($mode.Label).stdout.txt"
    $stderrPath = Join-Path $OutputDirectory "$($mode.Label).stderr.txt"
    $capturedProcess = Start-HenkaCapturedProcess -FilePath $captureExecutable -Arguments $mode.Arguments -WorkingDirectory $captureRuntimeDirectory -StdoutPath $stdoutPath -StderrPath $stderrPath
    $process = $capturedProcess.Process
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
        Assert-HenkaSandboxCaptureReady -Handle $handle -Label $mode.Label
        $metadataLine = Wait-HenkaCaptureReady -StdoutPath $stdoutPath -Process $process -Label $mode.Label
        if ($EvidenceProfile -eq "REALISM_REFERENCE") {
            [void]$captureMetadata.Add(
                (Assert-HenkaReferenceCaptureMetadata -Line $metadataLine -Label $mode.Label -ExpectedView $ReferenceView))
        }
        else {
            [void]$captureMetadata.Add(
                (Assert-HenkaCaptureMetadata -Line $metadataLine -Label $mode.Label))
        }
        Start-Sleep -Milliseconds 150
        Assert-HenkaSandboxCaptureReady -Handle $handle -Label $mode.Label
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
        $bitmap = Capture-HenkaWindowBitmap `
            -Handle $handle `
            -Rect $rect `
            -Label $mode.Label
        try {
            $path = Join-Path $OutputDirectory $mode.File
            $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
            $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
            $records.Add("$($mode.Label): $($mode.File) SHA-256=$hash bounds=$($width)x$($height)")
            $records.Add("$($mode.Label) metadata: $metadataLine")
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
            if (-not $capturedProcess.WaitForExit(10000)) {
                Stop-HenkaProcessTree -ProcessId $process.Id
            }
        }
        if ($null -ne $capturedProcess) {
            Close-HenkaCapturedProcess $capturedProcess
        }
    }
}

if ($captureMetadata.Count -gt 1) {
    $canonical = $captureMetadata[0].Canonical
    foreach ($metadata in $captureMetadata) {
        if ($metadata.Canonical -ne $canonical) {
            throw "Same-camera capture metadata diverged between shading modes."
        }
    }
    $records.Add("Composition metadata: identical across Solid, Material Preview, and Rendered")
}

if ($IncludeGiraffeInspection) {
    $inspectionModes = if ($EvidenceProfile -eq "GEOMETRY_SOLID") {
        @(
            @{ Label = "giraffe_front_solid"; Arguments = @("--capture-showcase-view", "front", "solid"); File = "giraffe-front-solid.png" },
            @{ Label = "giraffe_three_quarter_solid"; Arguments = @("--capture-showcase-view", "three-quarter", "solid"); File = "giraffe-three-quarter-solid.png" },
            @{ Label = "giraffe_profile_solid"; Arguments = @("--capture-showcase-view", "profile", "solid"); File = "giraffe-profile-solid.png" },
            @{ Label = "rocket_front_solid"; Arguments = @("--capture-rocket-view", "front", "solid"); File = "rocket-front-solid.png" },
            @{ Label = "rocket_three_quarter_solid"; Arguments = @("--capture-rocket-view", "three-quarter", "solid"); File = "rocket-three-quarter-solid.png" },
            @{ Label = "rocket_profile_solid"; Arguments = @("--capture-rocket-view", "profile", "solid"); File = "rocket-profile-solid.png" }
        )
    }
    else {
        @(
        @{ Label = "giraffe_front_rendered"; Arguments = @("--capture-showcase-view", "front", "rendered"); File = "giraffe-front-rendered.png" },
        @{ Label = "giraffe_three_quarter_rendered"; Arguments = @("--capture-showcase-view", "three-quarter", "rendered"); File = "giraffe-three-quarter-rendered.png" },
        @{ Label = "giraffe_profile_rendered"; Arguments = @("--capture-showcase-view", "profile", "rendered"); File = "giraffe-profile-rendered.png" },
        @{ Label = "giraffe_wide_rendered"; Arguments = @("--capture-showcase-view", "wide", "rendered"); File = "giraffe-wide-rendered.png" },
        @{ Label = "giraffe_front_material_preview"; Arguments = @("--capture-showcase-view", "front", "material_preview"); File = "giraffe-front-material-preview.png" },
        @{ Label = "rocket_front_rendered"; Arguments = @("--capture-rocket-view", "front", "rendered"); File = "rocket-front-rendered.png" },
        @{ Label = "rocket_three_quarter_rendered"; Arguments = @("--capture-rocket-view", "three-quarter", "rendered"); File = "rocket-three-quarter-rendered.png" },
        @{ Label = "rocket_profile_rendered"; Arguments = @("--capture-rocket-view", "profile", "rendered"); File = "rocket-profile-rendered.png" }
        )
    }
    foreach ($inspectionMode in $inspectionModes) {
        $capturedProcess = $null
        $stdoutPath = Join-Path $OutputDirectory "$($inspectionMode.Label).stdout.txt"
        $stderrPath = Join-Path $OutputDirectory "$($inspectionMode.Label).stderr.txt"
        $capturedProcess = Start-HenkaCapturedProcess -FilePath $captureExecutable -Arguments $inspectionMode.Arguments -WorkingDirectory $captureRuntimeDirectory -StdoutPath $stdoutPath -StderrPath $stderrPath
        $process = $capturedProcess.Process
        $handle = [IntPtr]::Zero
        try {
            for ($attempt = 0; $attempt -lt 80 -and $handle -eq [IntPtr]::Zero; ++$attempt) {
                Start-Sleep -Milliseconds 250
                $process.Refresh()
                $handle = [HenkaVisualCaptureNativeMethods]::FindSandboxWindow([uint32]$process.Id)
            }
            if ($handle -eq [IntPtr]::Zero -or -not [HenkaVisualCaptureNativeMethods]::IsWindow($handle)) {
                throw "Sandbox window did not become available for $($inspectionMode.Label)."
            }
            Assert-HenkaSandboxCaptureReady -Handle $handle -Label $inspectionMode.Label
            $metadataLine = Wait-HenkaCaptureReady -StdoutPath $stdoutPath -Process $process -Label $inspectionMode.Label
            $inspectionSubject = if ($inspectionMode.Label -match "_wide_") {
                "pair"
            }
            elseif ($inspectionMode.Label -like "rocket_*") {
                "rocket"
            }
            else {
                "giraffe"
            }
            Assert-HenkaCaptureMetadata -Line $metadataLine -Label $inspectionMode.Label -ExpectedSubject $inspectionSubject | Out-Null
            Start-Sleep -Milliseconds 150
            Assert-HenkaSandboxCaptureReady -Handle $handle -Label $inspectionMode.Label
            $rect = New-Object HenkaVisualCaptureNativeMethods+RECT
            $dwmResult = [HenkaVisualCaptureNativeMethods]::DwmGetWindowAttribute(
                $handle,
                9,
                [ref]$rect,
                [System.Runtime.InteropServices.Marshal]::SizeOf($rect))
            if ($dwmResult -ne 0) {
                throw "Window bounds could not be read for $($inspectionMode.Label): $dwmResult"
            }
            $width = $rect.Right - $rect.Left
            $height = $rect.Bottom - $rect.Top
            if ($width -le 0 -or $height -le 0) {
                throw "Window bounds were invalid for $($inspectionMode.Label)."
            }
            $bitmap = Capture-HenkaWindowBitmap `
                -Handle $handle `
                -Rect $rect `
                -Label $inspectionMode.Label
            try {
                $path = Join-Path $OutputDirectory $inspectionMode.File
                $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
                $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
                $records.Add("$($inspectionMode.Label): $($inspectionMode.File) SHA-256=$hash bounds=$($width)x$($height)")
                $records.Add("$($inspectionMode.Label) metadata: $metadataLine")
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
                if (-not $capturedProcess.WaitForExit(10000)) {
                    Stop-HenkaProcessTree -ProcessId $process.Id
                }
            }
            if ($null -ne $capturedProcess) {
                Close-HenkaCapturedProcess $capturedProcess
            }
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
        @{ Label = "terrain_corner_rendered"; Arguments = @("--capture-terrain-view", "corner", "rendered"); File = "terrain-corner-rendered.png" },
        @{ Label = "terrain_close_solid"; Arguments = @("--capture-terrain-view", "close", "solid"); File = "terrain-close-solid.png" },
        @{ Label = "terrain_close_material_preview"; Arguments = @("--capture-terrain-view", "close", "material_preview"); File = "terrain-close-material-preview.png" },
        @{ Label = "terrain_close_rendered"; Arguments = @("--capture-terrain-view", "close", "rendered"); File = "terrain-close-rendered.png" }
    )
    foreach ($terrainMode in $terrainModes) {
        $process = Start-HenkaProcess `
            -FilePath $captureExecutable `
            -Arguments $terrainMode.Arguments `
            -WorkingDirectory $captureRuntimeDirectory
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
            Assert-HenkaSandboxCaptureReady -Handle $handle -Label $terrainMode.Label
            Start-Sleep -Milliseconds 1500
            Assert-HenkaSandboxCaptureReady -Handle $handle -Label $terrainMode.Label
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
            $bitmap = Capture-HenkaWindowBitmap `
                -Handle $handle `
                -Rect $rect `
                -Label $terrainMode.Label
            try {
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

if ($EvidenceProfile -eq "REALISM_REFERENCE") {
    $records | Set-Content -LiteralPath (Join-Path $OutputDirectory "INDEX.txt")
    & (Join-Path $PSScriptRoot "check_realism_reference_visual_evidence_windows.ps1") `
        -InputDirectory $OutputDirectory
}

if ($IncludeTerrain) {
    & (Join-Path $PSScriptRoot "check_terrain_visual_evidence_windows.ps1") `
        -InputDirectory $OutputDirectory
    & (Join-Path $PSScriptRoot "check_terrain_corner_visual_evidence_windows.ps1") `
        -InputDirectory $OutputDirectory
    & (Join-Path $PSScriptRoot "check_terrain_close_visual_evidence_windows.ps1") `
        -InputDirectory $OutputDirectory
}

$records | Set-Content -LiteralPath (Join-Path $OutputDirectory "INDEX.txt")
Write-Host "[pass] Same-camera Solid, Material Preview, and Rendered evidence captured in $OutputDirectory"
