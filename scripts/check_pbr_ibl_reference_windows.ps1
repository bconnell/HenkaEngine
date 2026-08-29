param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "PBR IBL reference directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory "INDEX.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    throw "PBR IBL reference evidence is missing its capture index."
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
if ($indexText -notmatch "(?m)^Evidence profile: PBR_IBL_REFERENCE\s*$") {
    throw "PBR IBL reference evidence does not declare its dedicated profile."
}

$metadataPattern = "(?m)^.*CAPTURE_READY_IBL_REFERENCE mode=rendered view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) reference_texture_edge=(?<texture_edge>\d+) .*ibl_reference=1 ibl_direct_lighting=0 ibl_roughness_ladder=1 ibl_roughness_samples=9 ibl_irradiance_resolution=32 ibl_prefilter_resolution=256 ibl_prefilter_levels=7 ibl_brdf_resolution=128 .*viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>\d+) .*reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) draw_expected=1"
$metadata = @([regex]::Matches($indexText, $metadataPattern))
if ($metadata.Count -ne 1) {
    throw "PBR IBL reference evidence must contain exactly one rendered readiness record."
}
$entry = $metadata[0]
$expectedView = $entry.Groups["view"].Value
$expectedLayout = if ($expectedView -eq "close") { "close_grid" } else { "wide_row" }
if ($entry.Groups["layout"].Value -ne $expectedLayout -or
    [int]$entry.Groups["texture_edge"].Value -lt 32 -or
    [int]$entry.Groups["count"].Value -ne 9 -or
    [int]$entry.Groups["settled"].Value -lt 3) {
    throw "PBR IBL reference metadata is incomplete or uses the wrong deterministic layout."
}

$renderedPath = Join-Path $InputDirectory "ibl-reference-$expectedView-rendered.png"
if (-not (Test-Path -LiteralPath $renderedPath -PathType Leaf)) {
    throw "PBR IBL rendered evidence is missing: $renderedPath"
}

Add-Type -AssemblyName System.Drawing

function Get-ImageStatistics {
    param(
        [Parameter(Mandatory = $true)][string]$Path
    )

    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        [double]$sum = 0.0
        [double]$sumSquares = 0.0
        [int]$clipped = 0
        [int]$count = 0
        $step = [Math]::Max(1, [int][Math]::Floor($bitmap.Width / 160.0))
        for ($y = 0; $y -lt $bitmap.Height; $y += $step) {
            for ($x = 0; $x -lt $bitmap.Width; $x += $step) {
                $pixel = $bitmap.GetPixel($x, $y)
                [double]$luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                $sum += $luma
                $sumSquares += $luma * $luma
                if ([Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B)) -ge 254) {
                    ++$clipped
                }
                ++$count
            }
        }
        [double]$mean = $sum / $count
        return [pscustomobject]@{
            Width = $bitmap.Width
            Height = $bitmap.Height
            Mean = $mean
            StandardDeviation = [Math]::Sqrt([Math]::Max(0.0, ($sumSquares / $count) - ($mean * $mean)))
            ClippedFraction = $clipped / [double]$count
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-SubjectLuma {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][double]$CenterX,
        [Parameter(Mandatory = $true)][double]$CenterY
    )

    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        $centerX = [int][Math]::Round($bitmap.Width * $CenterX)
        $centerY = [int][Math]::Round($bitmap.Height * $CenterY)
        $radius = [Math]::Max(12, [int][Math]::Floor([Math]::Min($bitmap.Width, $bitmap.Height) * 0.035))
        [double]$sum = 0.0
        [int]$count = 0
        $minimumX = [Math]::Max(0, $centerX - $radius)
        $maximumX = [Math]::Min($bitmap.Width, $centerX + $radius)
        $minimumY = [Math]::Max(0, $centerY - $radius)
        $maximumY = [Math]::Min($bitmap.Height, $centerY + $radius)
        for ($y = $minimumY; $y -lt $maximumY; ++$y) {
            for ($x = $minimumX; $x -lt $maximumX; ++$x) {
                $pixel = $bitmap.GetPixel($x, $y)
                $sum += 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                ++$count
            }
        }
        return $sum / $count
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-PixelLuma {
    param(
        [Parameter(Mandatory = $true)][System.Drawing.Bitmap]$Bitmap,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y
    )

    $pixelX = [Math]::Max(0, [Math]::Min($Bitmap.Width - 1, [int][Math]::Round($X)))
    $pixelY = [Math]::Max(0, [Math]::Min($Bitmap.Height - 1, [int][Math]::Round($Y)))
    $pixel = $Bitmap.GetPixel($pixelX, $pixelY)
    return 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
}

$statistics = Get-ImageStatistics -Path $renderedPath
if ($statistics.Width -lt 160 -or $statistics.Height -lt 120 -or
    $statistics.StandardDeviation -lt 12.0 -or
    $statistics.ClippedFraction -gt 0.08 -or
    $statistics.Mean -lt 8.0 -or $statistics.Mean -gt 238.0) {
    throw "PBR IBL Rendered evidence is empty, too flat, or excessively clipped (mean=$([Math]::Round($statistics.Mean, 2)), standard-deviation=$([Math]::Round($statistics.StandardDeviation, 2)), clipped=$([Math]::Round($statistics.ClippedFraction, 4)))."
}

$centers = if ($expectedView -eq "close") {
    @(
        @(0.319, 0.235), @(0.506, 0.235), @(0.694, 0.235),
        @(0.319, 0.500), @(0.506, 0.500), @(0.694, 0.500),
        @(0.319, 0.765), @(0.506, 0.765), @(0.694, 0.765)
    )
}
else {
    @(
        @(0.125, 0.520), @(0.234, 0.520), @(0.344, 0.520),
        @(0.453, 0.520), @(0.500, 0.520), @(0.562, 0.520),
        @(0.672, 0.520), @(0.781, 0.520), @(0.891, 0.520)
    )
}
$lumas = @()
foreach ($center in $centers) {
    $lumas += Get-SubjectLuma -Path $renderedPath -CenterX $center[0] -CenterY $center[1]
}
$unreadableSubjects = @($lumas | Where-Object { $_ -lt 8.0 }).Count
if ($unreadableSubjects -gt 0) {
    throw "PBR IBL reference contains unreadable evaluated subjects (unreadable=$unreadableSubjects, luma=$([string]::Join(',', ($lumas | ForEach-Object { [Math]::Round($_, 1) }))))."
}
$ladderRange = ($lumas | Measure-Object -Maximum).Maximum - ($lumas | Measure-Object -Minimum).Minimum
$resolvedSteps = 0
for ($i = 1; $i -lt $lumas.Count; ++$i) {
    if ([Math]::Abs($lumas[$i] - $lumas[$i - 1]) -ge 1.5) {
        ++$resolvedSteps
    }
}
$bitmap = [System.Drawing.Bitmap]::new($renderedPath)
try {
    $lowerBlemishes = 0
    $concentratedHighlights = 0
    $sphereRadius = if ($expectedView -eq "close") {
        [Math]::Floor([Math]::Min($bitmap.Width, $bitmap.Height) * 0.125)
    }
    else {
        [Math]::Floor([Math]::Min($bitmap.Width, $bitmap.Height) * 0.055)
    }
    foreach ($center in $centers) {
        $centerX = $bitmap.Width * $center[0]
        $centerY = $bitmap.Height * $center[1]
        $bottomCenter = Get-PixelLuma -Bitmap $bitmap -X $centerX -Y ($centerY + $sphereRadius * 0.55)
        $bottomShoulders = (
            (Get-PixelLuma -Bitmap $bitmap -X ($centerX - $sphereRadius * 0.32) -Y ($centerY + $sphereRadius * 0.42)) +
            (Get-PixelLuma -Bitmap $bitmap -X ($centerX + $sphereRadius * 0.32) -Y ($centerY + $sphereRadius * 0.42))
        ) / 2.0
        if ($bottomCenter / [Math]::Max($bottomShoulders, 1.0) -lt 0.82) {
            ++$lowerBlemishes
        }

        $topCenter = Get-PixelLuma -Bitmap $bitmap -X $centerX -Y ($centerY - $sphereRadius * 0.22)
        $topShoulders = (
            (Get-PixelLuma -Bitmap $bitmap -X ($centerX - $sphereRadius * 0.28) -Y ($centerY - $sphereRadius * 0.12)) +
            (Get-PixelLuma -Bitmap $bitmap -X ($centerX + $sphereRadius * 0.28) -Y ($centerY - $sphereRadius * 0.12))
        ) / 2.0
        if ($topCenter / [Math]::Max($topShoulders, 1.0) -gt 1.18) {
            ++$concentratedHighlights
        }
    }
}
finally {
    $bitmap.Dispose()
}
if ($lowerBlemishes -gt 0 -or $concentratedHighlights -gt 0) {
    throw "PBR IBL reference contains localized sphere defects (lower-blemishes=$lowerBlemishes, concentrated-highlights=$concentratedHighlights)."
}
if ($ladderRange -lt 8.0 -or $resolvedSteps -lt 7) {
    throw "PBR IBL roughness ladder was not visibly resolved across the full range (range=$([Math]::Round($ladderRange, 2)), adjacent-steps=$resolvedSteps, luma=$([string]::Join(',', ($lumas | ForEach-Object { [Math]::Round($_, 1) }))))."
}

Write-Output "PBR IBL reference validation: passed (rendered-mean=$([Math]::Round($statistics.Mean, 2)), rendered-sd=$([Math]::Round($statistics.StandardDeviation, 2)), clipped-fraction=$([Math]::Round($statistics.ClippedFraction, 4)), visible-subjects=$($lumas.Count - $unreadableSubjects)/$($lumas.Count), roughness-ladder-range=$([Math]::Round($ladderRange, 2)), adjacent-steps=$resolvedSteps, lower-blemishes=$lowerBlemishes, concentrated-highlights=$concentratedHighlights)."
