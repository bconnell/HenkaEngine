Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$fixtureRoot = Join-Path $repoRoot (".subsurface-visual-validator-test-" + [Guid]::NewGuid().ToString("N"))
$shader = Get-Content (Join-Path $repoRoot 'assets/shaders/basic_lit.frag') -Raw
if ($shader -notmatch 'subsurfaceDirectProfile' -or
    $shader -notmatch 'mix\(0\.02, 0\.08, curvatureValue\)') {
    throw "Subsurface direct profile is missing its bounded grazing-edge energy limit."
}
[System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
Add-Type -AssemblyName System.Drawing

function New-TestSubsurfaceImage {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][int]$Variant
    )
    $bitmap = [System.Drawing.Bitmap]::new(640, 360)
    try {
        for ($y = 0; $y -lt $bitmap.Height; ++$y) {
            for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                $wave = [int](18.0 * [Math]::Sin($x * 0.035) + 12.0 * [Math]::Cos($y * 0.045))
                $red = 56 + $wave + ($Variant * 12)
                $green = 72 + $wave - ($Variant * 5)
                $blue = 58 + $wave
                if ([Math]::Abs($x - 320) -lt 38 -and [Math]::Abs($y - 180) -lt 38) {
                    $red += $Variant * 26
                    $green += $Variant * 12
                    $blue += $Variant * 8
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
    finally { $bitmap.Dispose() }
}

try {
    New-TestSubsurfaceImage (Join-Path $fixtureRoot "sss-reference-close-opaque.png") 0
    New-TestSubsurfaceImage (Join-Path $fixtureRoot "sss-reference-close-thin.png") 1
    New-TestSubsurfaceImage (Join-Path $fixtureRoot "sss-reference-close-thick.png") 2
    $metadataPrefix = "mode=rendered view=close reference_layout=close_grid reference_texture_edge=32 reference_exposure_stops=0.0000 reference_sss_variant="
    $metadataSuffix = " viewport=0,0,640,360 aspect=1.777778 camera_position=0.0000,0.0000,5.0000 yaw=-1.570796 pitch=0.000000 roll=0.000000 fov=1.047198 reference_bounds=0.0000,1.9000,-3.5000,2.8500,1.8500,1.2500 reference_midpoint=320.00,180.00 reference_count=9 settled_frames=3 draw_expected=1"
    @(
        "Source: henka_sandbox3d.exe",
        "Evidence profile: SUBSURFACE_REFERENCE",
        "sss_reference_opaque: sss-reference-close-opaque.png",
        "sss_reference_thin: sss-reference-close-thin.png",
        "sss_reference_thick: sss-reference-close-thick.png",
        "sss opaque stdout: Realism reference capture: debug grid hidden.",
        "sss thin stdout: Realism reference capture: debug grid hidden.",
        "sss thick stdout: Realism reference capture: debug grid hidden.",
        ("CAPTURE_READY_SSS_REFERENCE " + $metadataPrefix + "opaque" + $metadataSuffix),
        ("CAPTURE_READY_SSS_REFERENCE " + $metadataPrefix + "thin" + $metadataSuffix),
        ("CAPTURE_READY_SSS_REFERENCE " + $metadataPrefix + "thick" + $metadataSuffix)
    ) | Set-Content -LiteralPath (Join-Path $fixtureRoot "INDEX.txt")
    "Realism reference capture: debug grid hidden." | Set-Content -LiteralPath (Join-Path $fixtureRoot "sss_reference_opaque.stdout.txt")
    "Realism reference capture: debug grid hidden." | Set-Content -LiteralPath (Join-Path $fixtureRoot "sss_reference_thin.stdout.txt")
    "Realism reference capture: debug grid hidden." | Set-Content -LiteralPath (Join-Path $fixtureRoot "sss_reference_thick.stdout.txt")
    "" | Set-Content -LiteralPath (Join-Path $fixtureRoot "sss_reference_opaque.stderr.txt")
    "" | Set-Content -LiteralPath (Join-Path $fixtureRoot "sss_reference_thin.stderr.txt")
    "" | Set-Content -LiteralPath (Join-Path $fixtureRoot "sss_reference_thick.stderr.txt")

    & (Join-Path $PSScriptRoot "check_subsurface_reference_visual_evidence_windows.ps1") -InputDirectory $fixtureRoot | Out-Null

    Copy-Item -LiteralPath (Join-Path $fixtureRoot "sss-reference-close-opaque.png") -Destination (Join-Path $fixtureRoot "sss-reference-close-thin.png") -Force
    Copy-Item -LiteralPath (Join-Path $fixtureRoot "sss-reference-close-opaque.png") -Destination (Join-Path $fixtureRoot "sss-reference-close-thick.png") -Force
    $rejected = $false
    $message = ""
    try {
        & (Join-Path $PSScriptRoot "check_subsurface_reference_visual_evidence_windows.ps1") -InputDirectory $fixtureRoot | Out-Null
    }
    catch {
        $rejected = $true
        $message = $_.Exception.Message
    }
    if (-not $rejected -or $message -notmatch "measurable bounded response") {
        throw "The subsurface reference validator did not reject identical material variants with a useful diagnostic."
    }
    Write-Output "Subsurface reference visual evidence validator tests passed."
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
