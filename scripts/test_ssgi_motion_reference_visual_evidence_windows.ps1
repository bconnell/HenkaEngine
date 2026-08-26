Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$fixtureRoot = Join-Path $repoRoot (".ssgi-motion-visual-validator-test-" + [Guid]::NewGuid().ToString("N"))
[System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
Add-Type -AssemblyName System.Drawing

try {
    foreach ($phase in @("before", "after")) {
        $bitmap = [System.Drawing.Bitmap]::new(640, 360)
        try {
            for ($y = 0; $y -lt $bitmap.Height; ++$y) {
                for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                    $phaseOffset = if ($phase -eq "after") { 4 } else { 0 }
                    $wave = [int](18.0 * [Math]::Sin($x * 0.035) + 12.0 * [Math]::Cos($y * 0.045))
                    $red = [Math]::Max(0, [Math]::Min(255, 64 + $phaseOffset + $wave + [int]($x / 18)))
                    $green = [Math]::Max(0, [Math]::Min(255, 76 + $phaseOffset + $wave + [int]($y / 24)))
                    $blue = [Math]::Max(0, [Math]::Min(255, 58 + $phaseOffset + $wave))
                    $bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($red, $green, $blue))
                }
            }
            $bitmap.Save((Join-Path $fixtureRoot ("ssgi-motion-reference-close-$phase.png")), [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally { $bitmap.Dispose() }
    }

    $before = "CAPTURE_READY_SSGI_MOTION_REFERENCE phase=before mode=rendered view=close reference_layout=close_grid reference_texture_edge=32 reference_exposure_stops=0.0000 reference_ssgi_active=1 viewport=0,0,640,360 aspect=1.777778 camera_position=0.0000,0.0000,5.0000 yaw=-1.570796 pitch=0.000000 roll=0.000000 fov=1.047198 reference_bounds=0.0000,1.9000,-3.5000,2.8500,1.8500,1.2500 reference_midpoint=320.00,180.00 reference_count=9 settled_frames=3 draw_expected=1"
    $after = "CAPTURE_READY_SSGI_MOTION_REFERENCE phase=after mode=rendered view=close reference_layout=close_grid reference_texture_edge=32 reference_exposure_stops=0.0000 reference_ssgi_active=1 viewport=0,0,640,360 aspect=1.777778 camera_position=0.3500,0.0000,4.8000 yaw=-1.570796 pitch=0.000000 roll=0.000000 fov=1.047198 reference_bounds=0.0000,1.9000,-3.5000,2.8500,1.8500,1.2500 reference_midpoint=320.00,180.00 reference_count=9 settled_frames=3 draw_expected=1"
    $before = $before -replace 'reference_ssgi_active=1 ', 'reference_ssgi_active=1 reference_probe_diffuse_active=1 reference_probe_prefilter_active=1 reference_probe_blend_active=1 reference_probe_enabled_count=2 reference_probe_captured_count=2 reference_probe_capture_generation=1 reference_probe_capture_failures=0 '
    $after = $after -replace 'reference_ssgi_active=1 ', 'reference_ssgi_active=1 reference_probe_diffuse_active=1 reference_probe_prefilter_active=1 reference_probe_blend_active=1 reference_probe_enabled_count=2 reference_probe_captured_count=2 reference_probe_capture_generation=1 reference_probe_capture_failures=0 '
    @(
        "Source: henka_sandbox3d.exe",
        "Evidence profile: SSGI_MOTION_REFERENCE",
        "SSGI motion reference: deterministic before/after rendered captures from one process",
        $before,
        $after
    ) | Set-Content -LiteralPath (Join-Path $fixtureRoot "INDEX.txt")
    "Realism reference capture: debug grid hidden." | Set-Content -LiteralPath (Join-Path $fixtureRoot "ssgi_motion_reference.stdout.txt")

    & (Join-Path $PSScriptRoot "check_ssgi_motion_reference_visual_evidence_windows.ps1") -InputDirectory $fixtureRoot | Out-Null

    (Get-Content -LiteralPath (Join-Path $fixtureRoot "INDEX.txt") -Raw) -replace "camera_position=0.3500,0.0000,4.8000", "camera_position=0.0000,0.0000,5.0000" |
        Set-Content -LiteralPath (Join-Path $fixtureRoot "INDEX.txt")
    $rejected = $false
    $message = ""
    try {
        & (Join-Path $PSScriptRoot "check_ssgi_motion_reference_visual_evidence_windows.ps1") -InputDirectory $fixtureRoot | Out-Null
    }
    catch {
        $rejected = $true
        $message = $_.Exception.Message
    }
    if (-not $rejected -or $message -notmatch "camera translation") {
        throw "The SSGI motion reference validator did not reject a missing camera translation."
    }
    Write-Output "SSGI motion reference visual evidence validator tests passed."
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
