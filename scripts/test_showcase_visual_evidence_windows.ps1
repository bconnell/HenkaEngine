Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$fixtureRoot = Join-Path $repoRoot (".showcase-visual-validator-test-" + [Guid]::NewGuid().ToString("N"))
[System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
Add-Type -AssemblyName System.Drawing

function New-TestShowcaseImage {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][bool]$Flat,
        [int]$Offset = 0
    )

    $bitmap = [System.Drawing.Bitmap]::new(640, 360)
    try {
        for ($y = 0; $y -lt $bitmap.Height; ++$y) {
            for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                if ($Flat) {
                    $red = 42
                    $green = 42
                    $blue = 42
                }
                else {
                    $wave = [int](34.0 * [Math]::Sin(($x + $Offset) * 0.045) + 22.0 * [Math]::Cos(($y + $Offset) * 0.065))
                    $red = 60 + $wave + [int]($Offset * 3)
                    $green = 78 + $wave
                    $blue = 52 + $wave
                }
                $bitmap.SetPixel(
                    $x,
                    $y,
                    [System.Drawing.Color]::FromArgb(
                        [Math]::Max(0, [Math]::Min(255, $red)),
                        [Math]::Max(0, [Math]::Min(255, $green)),
                        [Math]::Max(0, [Math]::Min(255, $blue))))
            }
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

try {
    foreach ($view in @("front", "three-quarter", "profile", "wide")) {
        New-TestShowcaseImage -Path (Join-Path $fixtureRoot ("giraffe-{0}-rendered.png" -f $view)) -Flat $false -Offset ($view.Length * 4)
    }
    foreach ($view in @("front", "three-quarter", "profile")) {
        New-TestShowcaseImage -Path (Join-Path $fixtureRoot ("rocket-{0}-rendered.png" -f $view)) -Flat $false -Offset ($view.Length * 6)
    }
    New-TestShowcaseImage -Path (Join-Path $fixtureRoot "giraffe-front-material-preview.png") -Flat $false -Offset 2
    New-TestShowcaseImage -Path (Join-Path $fixtureRoot "startup-showcase.png") -Flat $false -Offset 8
    New-TestShowcaseImage -Path (Join-Path $fixtureRoot "same-camera-solid.png") -Flat $false -Offset 10
    New-TestShowcaseImage -Path (Join-Path $fixtureRoot "same-camera-material-preview.png") -Flat $false -Offset 12
    New-TestShowcaseImage -Path (Join-Path $fixtureRoot "same-camera-rendered.png") -Flat $false -Offset 14
    $metadata = @(
        "solid metadata: CAPTURE_READY mode=solid viewport=0,0,640,360 aspect=1.777778 camera_position=0.0000,0.0000,5.0000 yaw=-1.570796 pitch=0.000000 roll=0.000000 fov=1.047198 giraffe_bounds=-2.0,2.0,0.0,1.0,2.0,1.0 rocket_bounds=2.0,2.0,0.0,1.0,2.0,1.0 combined_bounds=0.0,2.0,0.0,3.0,2.0,1.0 giraffe_screen=50.00,30.00,250.00,330.00 rocket_screen=390.00,30.00,590.00,330.00 combined_midpoint=320.00,180.00 giraffe_parts=13 rocket_parts=13 giraffe_sss_regions=5 giraffe_normal_texture_regions=5 giraffe_normal_texture_loaded=5 giraffe_normal_texture_fallbacks=0 giraffe_thickness_texture_regions=5 giraffe_thickness_texture_loaded=5 giraffe_thickness_texture_fallbacks=0 settled_frames=3 giraffe_provenance=GENERATED_TEST_FIXTURE rocket_provenance=GENERATED_TEST_FIXTURE preset_applied=0 capture_subject=pair draw_expected=1",
        "material metadata: CAPTURE_READY mode=material_preview viewport=0,0,640,360 aspect=1.777778 camera_position=0.0000,0.0000,5.0000 yaw=-1.570796 pitch=0.000000 roll=0.000000 fov=1.047198 giraffe_bounds=-2.0,2.0,0.0,1.0,2.0,1.0 rocket_bounds=2.0,2.0,0.0,1.0,2.0,1.0 combined_bounds=0.0,2.0,0.0,3.0,2.0,1.0 giraffe_screen=50.00,30.00,250.00,330.00 rocket_screen=390.00,30.00,590.00,330.00 combined_midpoint=320.00,180.00 giraffe_parts=13 rocket_parts=13 giraffe_sss_regions=5 giraffe_normal_texture_regions=5 giraffe_normal_texture_loaded=5 giraffe_normal_texture_fallbacks=0 giraffe_thickness_texture_regions=5 giraffe_thickness_texture_loaded=5 giraffe_thickness_texture_fallbacks=0 settled_frames=3 giraffe_provenance=GENERATED_TEST_FIXTURE rocket_provenance=GENERATED_TEST_FIXTURE preset_applied=0 capture_subject=pair draw_expected=1",
        "rendered metadata: CAPTURE_READY mode=rendered viewport=0,0,640,360 aspect=1.777778 camera_position=0.0000,0.0000,5.0000 yaw=-1.570796 pitch=0.000000 roll=0.000000 fov=1.047198 giraffe_bounds=-2.0,2.0,0.0,1.0,2.0,1.0 rocket_bounds=2.0,2.0,0.0,1.0,2.0,1.0 combined_bounds=0.0,2.0,0.0,3.0,2.0,1.0 giraffe_screen=50.00,30.00,250.00,330.00 rocket_screen=390.00,30.00,590.00,330.00 combined_midpoint=320.00,180.00 giraffe_parts=13 rocket_parts=13 giraffe_sss_regions=5 giraffe_normal_texture_regions=5 giraffe_normal_texture_loaded=5 giraffe_normal_texture_fallbacks=5 settled_frames=3 giraffe_provenance=GENERATED_TEST_FIXTURE rocket_provenance=GENERATED_TEST_FIXTURE preset_applied=0 capture_subject=pair draw_expected=1"
    )
    $metadata = $metadata | ForEach-Object {
        $_ -replace 'giraffe_normal_texture_fallbacks=5 settled_frames=3 giraffe_provenance=', 'giraffe_normal_texture_fallbacks=0 giraffe_thickness_texture_regions=5 giraffe_thickness_texture_loaded=5 giraffe_thickness_texture_fallbacks=0 settled_frames=3 giraffe_provenance='
    }
    @(
        "Source: henka_sandbox3d.exe",
        "Evidence profile: FULL_SHOWCASE",
        "pair-solid: same-camera-solid.png",
        "pair-material-preview: same-camera-material-preview.png",
        "pair-rendered: same-camera-rendered.png",
        "giraffe-front-rendered: giraffe-front-rendered.png",
        "giraffe-three-quarter-rendered: giraffe-three-quarter-rendered.png",
        "giraffe-profile-rendered: giraffe-profile-rendered.png",
        "giraffe-wide-rendered: giraffe-wide-rendered.png",
        "giraffe-front-material-preview: giraffe-front-material-preview.png",
        "rocket-front-rendered: rocket-front-rendered.png",
        "rocket-three-quarter-rendered: rocket-three-quarter-rendered.png",
        "rocket-profile-rendered: rocket-profile-rendered.png",
        "startup: startup-showcase.png"
    ) + $metadata | Set-Content -LiteralPath (Join-Path $fixtureRoot "INDEX.txt")

    & (Join-Path $PSScriptRoot "check_showcase_visual_evidence_windows.ps1") -InputDirectory $fixtureRoot | Out-Null

    New-TestShowcaseImage -Path (Join-Path $fixtureRoot "giraffe-profile-rendered.png") -Flat $true
    $rejectedFlat = $false
    $flatMessage = ""
    try {
        & (Join-Path $PSScriptRoot "check_showcase_visual_evidence_windows.ps1") -InputDirectory $fixtureRoot | Out-Null
    }
    catch {
        $rejectedFlat = $true
        $flatMessage = $_.Exception.Message
    }
    if (-not $rejectedFlat -or $flatMessage -notmatch "profile") {
        throw "The showcase validator did not reject flat profile evidence with a useful view diagnostic."
    }

    Write-Output "Showcase visual evidence validator tests passed."
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
