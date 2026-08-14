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
    New-TestShowcaseImage -Path (Join-Path $fixtureRoot "giraffe-front-material-preview.png") -Flat $false -Offset 2
    New-TestShowcaseImage -Path (Join-Path $fixtureRoot "startup-showcase.png") -Flat $false -Offset 8
    @(
        "Source: henka_sandbox3d.exe",
        "giraffe-front-rendered: giraffe-front-rendered.png",
        "giraffe-three-quarter-rendered: giraffe-three-quarter-rendered.png",
        "giraffe-profile-rendered: giraffe-profile-rendered.png",
        "giraffe-wide-rendered: giraffe-wide-rendered.png",
        "giraffe-front-material-preview: giraffe-front-material-preview.png",
        "startup: startup-showcase.png"
    ) | Set-Content -LiteralPath (Join-Path $fixtureRoot "INDEX.txt")

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
