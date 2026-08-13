param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "Terrain corner visual evidence directory was not found: $InputDirectory"
}

Add-Type -AssemblyName System.Drawing

$modeFiles = [ordered]@{
    Solid = "terrain-corner-solid.png"
    "Material Preview" = "terrain-corner-material-preview.png"
    Rendered = "terrain-corner-rendered.png"
}
$measurements = [ordered]@{}
$referenceWidth = 0
$referenceHeight = 0

foreach ($mode in $modeFiles.Keys) {
    $path = Join-Path $InputDirectory $modeFiles[$mode]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "$mode Terrain corner evidence is missing: $path"
    }

    $bitmap = [System.Drawing.Bitmap]::new($path)
    try {
        if ($bitmap.Width -le 0 -or $bitmap.Height -le 0) {
            throw "$mode Terrain corner evidence has invalid dimensions."
        }
        if ($referenceWidth -eq 0) {
            $referenceWidth = $bitmap.Width
            $referenceHeight = $bitmap.Height
        }
        elseif ($bitmap.Width -ne $referenceWidth -or $bitmap.Height -ne $referenceHeight) {
            throw "$mode Terrain corner evidence dimensions do not match the other modes."
        }

        $left = [int]($bitmap.Width * 0.28)
        $right = [int]($bitmap.Width * 0.70)
        $top = [int]($bitmap.Height * 0.12)
        $bottom = [int]($bitmap.Height * 0.88)
        $step = [Math]::Max(1, [int][Math]::Floor($bitmap.Width / 160.0))
        [double]$sum = 0.0
        [double]$sumSquares = 0.0
        [double]$saturationSum = 0.0
        [double]$minimum = 255.0
        [double]$maximum = 0.0
        [double[]]$quadrantSums = @(0.0, 0.0, 0.0, 0.0)
        [int[]]$quadrantCounts = @(0, 0, 0, 0)
        [int]$count = 0

        for ($y = $top; $y -lt $bottom; $y += $step) {
            for ($x = $left; $x -lt $right; $x += $step) {
                $pixel = $bitmap.GetPixel($x, $y)
                [double]$luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                [double]$high = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B))
                [double]$low = [Math]::Min($pixel.R, [Math]::Min($pixel.G, $pixel.B))
                $sum += $luma
                $sumSquares += $luma * $luma
                $saturationSum += $high - $low
                $minimum = [Math]::Min($minimum, $luma)
                $maximum = [Math]::Max($maximum, $luma)
                $quadrantX = if ($x -lt (($left + $right) / 2)) { 0 } else { 1 }
                $quadrantY = if ($y -lt (($top + $bottom) / 2)) { 0 } else { 1 }
                $quadrantIndex = $quadrantX + 2 * $quadrantY
                $quadrantSums[$quadrantIndex] += $luma
                $quadrantCounts[$quadrantIndex] += 1
                ++$count
            }
        }

        if ($count -le 0) {
            throw "$mode Terrain corner evidence contained no sampled Scene View pixels."
        }
        [double]$mean = $sum / $count
        [double]$variance = [Math]::Max(0.0, ($sumSquares / $count) - ($mean * $mean))
        [double]$quadrantMean0 = if ($quadrantCounts[0] -gt 0) { $quadrantSums[0] / $quadrantCounts[0] } else { 0.0 }
        [double]$quadrantMean1 = if ($quadrantCounts[1] -gt 0) { $quadrantSums[1] / $quadrantCounts[1] } else { 0.0 }
        [double]$quadrantMean2 = if ($quadrantCounts[2] -gt 0) { $quadrantSums[2] / $quadrantCounts[2] } else { 0.0 }
        [double]$quadrantMean3 = if ($quadrantCounts[3] -gt 0) { $quadrantSums[3] / $quadrantCounts[3] } else { 0.0 }
        $quadrantMeans = @($quadrantMean0, $quadrantMean1, $quadrantMean2, $quadrantMean3)
        $measurements[$mode] = [pscustomobject]@{
            StandardDeviation = [Math]::Sqrt($variance)
            LumaRange = $maximum - $minimum
            MeanSaturation = $saturationSum / $count
            QuadrantLumaRange = ($quadrantMeans | Measure-Object -Maximum).Maximum -
                ($quadrantMeans | Measure-Object -Minimum).Minimum
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

foreach ($mode in $modeFiles.Keys) {
    $measurement = $measurements[$mode]
    if ($measurement.StandardDeviation -lt 2.0 -or $measurement.LumaRange -lt 15.0) {
        throw "$mode Terrain corner evidence is too flat (std=$([Math]::Round($measurement.StandardDeviation, 2)), range=$([Math]::Round($measurement.LumaRange, 2)))."
    }
}

foreach ($mode in @("Material Preview", "Rendered")) {
    $measurement = $measurements[$mode]
    if ($measurement.MeanSaturation -lt 5.0) {
        throw "$mode Terrain corner evidence does not communicate material identity (mean saturation=$([Math]::Round($measurement.MeanSaturation, 2)))."
    }
    if ($measurement.QuadrantLumaRange -lt 1.0) {
        throw "$mode Terrain corner evidence does not show spatially varied scene content (quadrant luma range=$([Math]::Round($measurement.QuadrantLumaRange, 2)))."
    }
}

$preview = [System.Drawing.Bitmap]::new((Join-Path $InputDirectory $modeFiles["Material Preview"]))
$rendered = [System.Drawing.Bitmap]::new((Join-Path $InputDirectory $modeFiles.Rendered))
try {
    $left = [int]($preview.Width * 0.28)
    $right = [int]($preview.Width * 0.70)
    $top = [int]($preview.Height * 0.12)
    $bottom = [int]($preview.Height * 0.88)
    $step = [Math]::Max(1, [int][Math]::Floor($preview.Width / 160.0))
    [double]$differenceSum = 0.0
    [int]$differenceCount = 0
    for ($y = $top; $y -lt $bottom; $y += $step) {
        for ($x = $left; $x -lt $right; $x += $step) {
            $previewPixel = $preview.GetPixel($x, $y)
            $renderedPixel = $rendered.GetPixel($x, $y)
            $differenceSum += (
                [Math]::Abs($previewPixel.R - $renderedPixel.R) +
                [Math]::Abs($previewPixel.G - $renderedPixel.G) +
                [Math]::Abs($previewPixel.B - $renderedPixel.B)) / 3.0
            ++$differenceCount
        }
    }
    [double]$meanDifference = $differenceSum / $differenceCount
}
finally {
    $preview.Dispose()
    $rendered.Dispose()
}

if ($meanDifference -lt 4.0) {
    throw "Rendered Terrain corner evidence is not materially distinct from Material Preview (mean RGB difference=$([Math]::Round($meanDifference, 2)))."
}

$summary = @(
    "Terrain corner visual evidence validation: passed",
    "Viewport sample: normalized x=0.28..0.70, y=0.12..0.88",
    "Solid: std=$([Math]::Round($measurements.Solid.StandardDeviation, 2)) range=$([Math]::Round($measurements.Solid.LumaRange, 2))",
    "Material Preview: std=$([Math]::Round($measurements['Material Preview'].StandardDeviation, 2)) range=$([Math]::Round($measurements['Material Preview'].LumaRange, 2))",
    "Rendered: std=$([Math]::Round($measurements.Rendered.StandardDeviation, 2)) range=$([Math]::Round($measurements.Rendered.LumaRange, 2))",
    "Material semantic checks: mean saturation >= 5 and quadrant luma range >= 1 for Material Preview and Rendered",
    "Rendered vs Material Preview mean RGB difference: $([Math]::Round($meanDifference, 2))",
    "Status: automated evidence only; human visual corner QA remains required"
)
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "terrain-corner-visual-validation.txt")
$summary | ForEach-Object { Write-Output $_ }
