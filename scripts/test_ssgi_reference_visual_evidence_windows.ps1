Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$fixtureRoot = Join-Path $repoRoot (".ssgi-visual-validator-test-" + [Guid]::NewGuid().ToString("N"))
[System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
Add-Type -AssemblyName System.Drawing

try {
    $bitmap = [System.Drawing.Bitmap]::new(640, 360)
    try {
        for ($y = 0; $y -lt $bitmap.Height; ++$y) {
            for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                $wave = [int](22.0 * [Math]::Sin($x * 0.035) + 14.0 * [Math]::Cos($y * 0.045))
                $red = [Math]::Max(0, [Math]::Min(255, 64 + $wave + [int]($x / 18)))
                $green = [Math]::Max(0, [Math]::Min(255, 76 + $wave + [int]($y / 24)))
                $blue = [Math]::Max(0, [Math]::Min(255, 58 + $wave))
                $bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($red, $green, $blue))
            }
        }
        $bitmap.Save((Join-Path $fixtureRoot "ssgi-reference-close-rendered.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $bitmap.Dispose() }

    $metadata = "CAPTURE_READY_SSGI_REFERENCE mode=rendered view=close reference_layout=close_grid reference_texture_edge=32 reference_exposure_stops=0.0000 reference_ssgi_active=1 viewport=0,0,640,360 aspect=1.777778 camera_position=0.0000,0.0000,5.0000 yaw=-1.570796 pitch=0.000000 roll=0.000000 fov=1.047198 reference_bounds=0.0000,1.9000,-3.5000,2.8500,1.8500,1.2500 reference_midpoint=320.00,180.00 reference_count=9 settled_frames=3 draw_expected=1"
    @(
        "Source: henka_sandbox3d.exe",
        "Evidence profile: SSGI_REFERENCE",
        "SSGI reference: deterministic rendered capture proving the bounded screen-space indirect-diffuse path is active; same nine-subject camera and view=close",
        $metadata
    ) | Set-Content -LiteralPath (Join-Path $fixtureRoot "INDEX.txt")
    "Realism reference capture: debug grid hidden." | Set-Content -LiteralPath (Join-Path $fixtureRoot "ssgi_reference.stdout.txt")
    "" | Set-Content -LiteralPath (Join-Path $fixtureRoot "ssgi_reference.stderr.txt")

    & (Join-Path $PSScriptRoot "check_ssgi_reference_visual_evidence_windows.ps1") -InputDirectory $fixtureRoot | Out-Null

    (Get-Content -LiteralPath (Join-Path $fixtureRoot "INDEX.txt") -Raw) -replace "reference_ssgi_active=1", "reference_ssgi_active=0" |
        Set-Content -LiteralPath (Join-Path $fixtureRoot "INDEX.txt")
    $rejected = $false
    $message = ""
    try {
        & (Join-Path $PSScriptRoot "check_ssgi_reference_visual_evidence_windows.ps1") -InputDirectory $fixtureRoot | Out-Null
    }
    catch {
        $rejected = $true
        $message = $_.Exception.Message
    }
    if (-not $rejected -or $message -notmatch "exactly one rendered readiness record") {
        throw "The SSGI reference validator did not reject an inactive readiness record."
    }

    $bitmap = [System.Drawing.Bitmap]::new(640, 360)
    try {
        for ($y = 0; $y -lt $bitmap.Height; ++$y) {
            for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                $wave = [int](8.0 * [Math]::Sin($x * 0.035) + 6.0 * [Math]::Cos($y * 0.045))
                $value = [Math]::Max(0, [Math]::Min(255, 245 + $wave))
                $bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($value, $value, $value))
            }
        }
        $bitmap.Save((Join-Path $fixtureRoot "ssgi-reference-close-rendered.png"), [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally { $bitmap.Dispose() }
    (Get-Content -LiteralPath (Join-Path $fixtureRoot "INDEX.txt") -Raw) -replace "reference_ssgi_active=0", "reference_ssgi_active=1" |
        Set-Content -LiteralPath (Join-Path $fixtureRoot "INDEX.txt")
    $rejected = $false
    $message = ""
    try {
        & (Join-Path $PSScriptRoot "check_ssgi_reference_visual_evidence_windows.ps1") -InputDirectory $fixtureRoot | Out-Null
    }
    catch {
        $rejected = $true
        $message = $_.Exception.Message
    }
    if (-not $rejected -or $message -notmatch "excessively clipped or over-bright") {
        throw "The SSGI reference validator did not reject an excessively bright image."
    }
    Write-Output "SSGI reference visual evidence validator tests passed."
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
