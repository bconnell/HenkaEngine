param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "Terrain visual evidence directory was not found: $InputDirectory"
}

Add-Type -AssemblyName System.Drawing

$modeFiles = [ordered]@{
    Solid = "terrain-same-camera-solid.png"
    "Material Preview" = "terrain-same-camera-material-preview.png"
    Rendered = "terrain-same-camera-rendered.png"
}
$measurements = [ordered]@{}
$referenceWidth = 0
$referenceHeight = 0

foreach ($mode in $modeFiles.Keys) {
    $path = Join-Path $InputDirectory $modeFiles[$mode]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "$mode terrain evidence is missing: $path"
    }

    $bitmap = [System.Drawing.Bitmap]::new($path)
    try {
        if ($bitmap.Width -le 0 -or $bitmap.Height -le 0) {
            throw "$mode terrain evidence has invalid dimensions."
        }
        if ($referenceWidth -eq 0) {
            $referenceWidth = $bitmap.Width
            $referenceHeight = $bitmap.Height
        }
        elseif ($bitmap.Width -ne $referenceWidth -or $bitmap.Height -ne $referenceHeight) {
            throw "$mode terrain evidence dimensions do not match the other modes."
        }

        # The capture window is application-only. This normalized rectangle is
        # the Scene View interior for the bounded default layout, leaving out
        # title bars, docks, header controls, and the diagnostics strip.
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
                ++$count
            }
        }

        if ($count -le 0) {
            throw "$mode terrain evidence contained no sampled Scene View pixels."
        }
        [double]$mean = $sum / $count
        [double]$variance = [Math]::Max(0.0, ($sumSquares / $count) - ($mean * $mean))
        $measurements[$mode] = [pscustomobject]@{
            Mean = $mean
            StandardDeviation = [Math]::Sqrt($variance)
            LumaRange = $maximum - $minimum
            MeanSaturation = $saturationSum / $count
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

foreach ($mode in $modeFiles.Keys) {
    $measurement = $measurements[$mode]
    if ($measurement.StandardDeviation -lt 2.0 -or $measurement.LumaRange -lt 15.0) {
        throw "$mode terrain evidence is too flat in the Scene View (std=$([Math]::Round($measurement.StandardDeviation, 2)), range=$([Math]::Round($measurement.LumaRange, 2)))."
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
    throw "Rendered terrain evidence is not materially distinct from Material Preview (mean RGB difference=$([Math]::Round($meanDifference, 2)))."
}

$summary = @(
    "Terrain visual evidence validation: passed",
    "Viewport sample: normalized x=0.28..0.70, y=0.12..0.88",
    "Solid: std=$([Math]::Round($measurements.Solid.StandardDeviation, 2)) range=$([Math]::Round($measurements.Solid.LumaRange, 2))",
    "Material Preview: std=$([Math]::Round($measurements['Material Preview'].StandardDeviation, 2)) range=$([Math]::Round($measurements['Material Preview'].LumaRange, 2))",
    "Rendered: std=$([Math]::Round($measurements.Rendered.StandardDeviation, 2)) range=$([Math]::Round($measurements.Rendered.LumaRange, 2))",
    "Rendered vs Material Preview mean RGB difference: $([Math]::Round($meanDifference, 2))",
    "Status: automated evidence only; human visual QA remains required"
)
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "terrain-visual-validation.txt")
$summary | ForEach-Object { Write-Output $_ }
