param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "Terrain close visual evidence directory was not found: $InputDirectory"
}

Add-Type -AssemblyName System.Drawing

$modeFiles = [ordered]@{
    Solid = "terrain-close-solid.png"
    "Material Preview" = "terrain-close-material-preview.png"
    Rendered = "terrain-close-rendered.png"
}
$measurements = [ordered]@{}
$referenceWidth = 0
$referenceHeight = 0

foreach ($mode in $modeFiles.Keys) {
    $path = Join-Path $InputDirectory $modeFiles[$mode]
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "$mode Terrain close evidence is missing: $path"
    }

    $bitmap = [System.Drawing.Bitmap]::new($path)
    try {
        if ($bitmap.Width -le 0 -or $bitmap.Height -le 0) {
            throw "$mode Terrain close evidence has invalid dimensions."
        }
        if ($referenceWidth -eq 0) {
            $referenceWidth = $bitmap.Width
            $referenceHeight = $bitmap.Height
        }
        elseif ($bitmap.Width -ne $referenceWidth -or $bitmap.Height -ne $referenceHeight) {
            throw "$mode Terrain close evidence dimensions do not match the other modes."
        }

        $left = [int]($bitmap.Width * 0.25)
        $right = [int]($bitmap.Width * 0.75)
        $top = [int]($bitmap.Height * 0.18)
        $bottom = [int]($bitmap.Height * 0.88)
        $step = [Math]::Max(1, [int][Math]::Floor($bitmap.Width / 160.0))
        [double]$sum = 0.0
        [double]$sumSquares = 0.0
        [double]$minimum = 255.0
        [double]$maximum = 0.0
        [int]$count = 0

        for ($y = $top; $y -lt $bottom; $y += $step) {
            for ($x = $left; $x -lt $right; $x += $step) {
                $pixel = $bitmap.GetPixel($x, $y)
                [double]$luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                $sum += $luma
                $sumSquares += $luma * $luma
                $minimum = [Math]::Min($minimum, $luma)
                $maximum = [Math]::Max($maximum, $luma)
                ++$count
            }
        }

        if ($count -le 0) {
            throw "$mode Terrain close evidence contained no sampled Scene View pixels."
        }
        [double]$mean = $sum / $count
        [double]$variance = [Math]::Max(0.0, ($sumSquares / $count) - ($mean * $mean))
        $measurements[$mode] = [pscustomobject]@{
            StandardDeviation = [Math]::Sqrt($variance)
            LumaRange = $maximum - $minimum
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

foreach ($mode in $modeFiles.Keys) {
    $measurement = $measurements[$mode]
    if ($measurement.StandardDeviation -lt 2.0 -or $measurement.LumaRange -lt 15.0) {
        throw "$mode Terrain close evidence is too flat (std=$([Math]::Round($measurement.StandardDeviation, 2)), range=$([Math]::Round($measurement.LumaRange, 2)))."
    }
}

$preview = [System.Drawing.Bitmap]::new((Join-Path $InputDirectory $modeFiles["Material Preview"]))
$rendered = [System.Drawing.Bitmap]::new((Join-Path $InputDirectory $modeFiles.Rendered))
try {
    $left = [int]($preview.Width * 0.25)
    $right = [int]($preview.Width * 0.75)
    $top = [int]($preview.Height * 0.18)
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
    throw "Rendered Terrain close evidence is not materially distinct from Material Preview (mean RGB difference=$([Math]::Round($meanDifference, 2)))."
}

$summary = @(
    "Terrain close visual evidence validation: passed",
    "Viewport sample: normalized x=0.25..0.75, y=0.18..0.88",
    "Solid: std=$([Math]::Round($measurements.Solid.StandardDeviation, 2)) range=$([Math]::Round($measurements.Solid.LumaRange, 2))",
    "Material Preview: std=$([Math]::Round($measurements['Material Preview'].StandardDeviation, 2)) range=$([Math]::Round($measurements['Material Preview'].LumaRange, 2))",
    "Rendered: std=$([Math]::Round($measurements.Rendered.StandardDeviation, 2)) range=$([Math]::Round($measurements.Rendered.LumaRange, 2))",
    "Rendered vs Material Preview mean RGB difference: $([Math]::Round($meanDifference, 2))",
    "Status: automated evidence only; human visual close-material QA remains required"
)
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "terrain-close-visual-validation.txt")
$summary | ForEach-Object { Write-Output $_ }
