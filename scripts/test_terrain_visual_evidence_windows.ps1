Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$fixtureRoot = Join-Path $repoRoot (".terrain-visual-validator-test-" + [Guid]::NewGuid().ToString("N"))
[System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
Add-Type -AssemblyName System.Drawing

function New-TestTerrainImage {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][bool]$Flat,
        [Parameter(Mandatory = $true)][bool]$Rendered,
        [bool]$Gray = $false
    )

    $bitmap = [System.Drawing.Bitmap]::new(640, 360)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        for ($y = 0; $y -lt $bitmap.Height; ++$y) {
            for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                if ($Flat) {
                    $red = 80
                    $green = 80
                    $blue = 80
                }
                else {
                    $wave = [int](18.0 * [Math]::Sin($x * 0.07) + 14.0 * [Math]::Cos($y * 0.11))
                    if ($Gray) {
                        $red = 80 + $wave
                        $green = 80 + $wave
                        $blue = 80 + $wave
                    }
                    else {
                        $red = 48 + $wave + $(if ($Rendered) { 18 } else { 0 })
                        $green = 72 + $wave + $(if ($Rendered) { 8 } else { 0 })
                        $blue = 42 + $wave + $(if ($Rendered) { -4 } else { 0 })
                    }
                }
                $bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(
                    [Math]::Max(0, [Math]::Min(255, $red)),
                    [Math]::Max(0, [Math]::Min(255, $green)),
                    [Math]::Max(0, [Math]::Min(255, $blue))))
            }
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Invoke-Validator {
    param([Parameter(Mandatory = $true)][string]$InputDirectory)
    & (Join-Path $PSScriptRoot "check_terrain_visual_evidence_windows.ps1") `
        -InputDirectory $InputDirectory | Out-Null
}

try {
    New-TestTerrainImage -Path (Join-Path $fixtureRoot "terrain-same-camera-solid.png") -Flat $false -Rendered $false
    New-TestTerrainImage -Path (Join-Path $fixtureRoot "terrain-same-camera-material-preview.png") -Flat $false -Rendered $false
    New-TestTerrainImage -Path (Join-Path $fixtureRoot "terrain-same-camera-rendered.png") -Flat $false -Rendered $true
    Invoke-Validator -InputDirectory $fixtureRoot

    foreach ($mode in @("solid", "material-preview", "rendered")) {
        New-TestTerrainImage `
            -Path (Join-Path $fixtureRoot ("terrain-corner-{0}.png" -f $mode)) `
            -Flat $false `
            -Rendered ($mode -eq "rendered")
    }
    & (Join-Path $PSScriptRoot "check_terrain_corner_visual_evidence_windows.ps1") `
        -InputDirectory $fixtureRoot | Out-Null

    foreach ($mode in @("solid", "material-preview", "rendered")) {
        New-TestTerrainImage `
            -Path (Join-Path $fixtureRoot ("terrain-close-{0}.png" -f $mode)) `
            -Flat $false `
            -Rendered ($mode -eq "rendered")
    }
    & (Join-Path $PSScriptRoot "check_terrain_close_visual_evidence_windows.ps1") `
        -InputDirectory $fixtureRoot | Out-Null

    New-TestTerrainImage -Path (Join-Path $fixtureRoot "terrain-same-camera-solid.png") -Flat $true -Rendered $false
    New-TestTerrainImage -Path (Join-Path $fixtureRoot "terrain-same-camera-material-preview.png") -Flat $true -Rendered $false
    New-TestTerrainImage -Path (Join-Path $fixtureRoot "terrain-same-camera-rendered.png") -Flat $true -Rendered $true
    $rejectedFlatEvidence = $false
    $flatEvidenceMessage = ""
    try {
        Invoke-Validator -InputDirectory $fixtureRoot
    }
    catch {
        $rejectedFlatEvidence = $true
        $flatEvidenceMessage = $_.Exception.Message
    }
    if (-not $rejectedFlatEvidence -or $flatEvidenceMessage -notmatch "too flat") {
        throw "The validator did not reject flat terrain evidence for the expected reason."
    }

    foreach ($mode in @("solid", "material-preview", "rendered")) {
        New-TestTerrainImage `
            -Path (Join-Path $fixtureRoot ("terrain-same-camera-{0}.png" -f $mode)) `
            -Flat $false `
            -Rendered ($mode -eq "rendered") `
            -Gray $true
    }
    $rejectedGrayEvidence = $false
    $grayEvidenceMessage = ""
    try {
        Invoke-Validator -InputDirectory $fixtureRoot
    }
    catch {
        $rejectedGrayEvidence = $true
        $grayEvidenceMessage = $_.Exception.Message
    }
    if (-not $rejectedGrayEvidence -or $grayEvidenceMessage -notmatch "material identity") {
        throw "The validator did not reject low-chroma terrain evidence for the expected reason."
    }

    Write-Output "Terrain visual evidence validator tests passed."
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
