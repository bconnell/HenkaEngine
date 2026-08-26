param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "PBR energy reference directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory "INDEX.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    throw "PBR energy reference evidence is missing its capture index."
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
if ($indexText -notmatch "(?m)^Evidence profile: PBR_ENERGY_REFERENCE\s*$") {
    throw "PBR energy reference evidence does not declare its dedicated profile."
}

$metadataPattern = "(?m)CAPTURE_READY_ENERGY_REFERENCE mode=(?<mode>solid|material_preview|rendered) view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) reference_texture_edge=(?<texture_edge>\d+) .*energy_reference=1 energy_dielectric_count=3 energy_metallic_count=3 energy_transmission_count=3 energy_clipped_channel_limit=254 .*reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) draw_expected=1"
$metadata = @([regex]::Matches($indexText, $metadataPattern))
if ($metadata.Count -ne 3) {
    throw "PBR energy reference evidence must contain exactly three readiness records."
}
$expectedView = $metadata[0].Groups["view"].Value
$expectedLayout = if ($expectedView -eq "close") { "close_grid" } else { "wide_row" }
$canonical = $metadata[0].Value -replace 'mode=(solid|material_preview|rendered)', 'mode=shared'
foreach ($entry in $metadata) {
    if ($entry.Groups["view"].Value -ne $expectedView -or
        $entry.Groups["layout"].Value -ne $expectedLayout -or
        ($entry.Value -replace 'mode=(solid|material_preview|rendered)', 'mode=shared') -ne $canonical -or
        [int]$entry.Groups["texture_edge"].Value -lt 32 -or
        [int]$entry.Groups["count"].Value -ne 9 -or
        [int]$entry.Groups["settled"].Value -lt 3) {
        throw "PBR energy reference metadata is inconsistent or incomplete."
    }
}

Add-Type -AssemblyName System.Drawing

function Get-PixelStatistics {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][int]$Step
    )

    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        [double]$sum = 0.0
        [double]$sumSquares = 0.0
        [int]$clipped = 0
        [int]$count = 0
        for ($y = 0; $y -lt $bitmap.Height; $y += $Step) {
            for ($x = 0; $x -lt $bitmap.Width; $x += $Step) {
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
            Mean = $mean
            StandardDeviation = [Math]::Sqrt([Math]::Max(0.0, ($sumSquares / $count) - ($mean * $mean)))
            ClippedFraction = $clipped / [double]$count
            Width = $bitmap.Width
            Height = $bitmap.Height
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-MeanLuma {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][double]$CenterX,
        [Parameter(Mandatory = $true)][double]$CenterY
    )

    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        $centerPixelX = [int][Math]::Round($bitmap.Width * $CenterX)
        $centerPixelY = [int][Math]::Round($bitmap.Height * $CenterY)
        $radius = [Math]::Max(12, [int][Math]::Floor([Math]::Min($bitmap.Width, $bitmap.Height) * 0.035))
        [double]$sum = 0.0
        [int]$count = 0
        $minX = [Math]::Max(0, $centerPixelX - $radius)
        $maxX = [Math]::Min($bitmap.Width, $centerPixelX + $radius)
        $minY = [Math]::Max(0, $centerPixelY - $radius)
        $maxY = [Math]::Min($bitmap.Height, $centerPixelY + $radius)
        for ($y = $minY; $y -lt $maxY; ++$y) {
            for ($x = $minX; $x -lt $maxX; ++$x) {
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

$renderedPath = Join-Path $InputDirectory "energy-reference-$expectedView-rendered.png"
$previewPath = Join-Path $InputDirectory "energy-reference-$expectedView-material-preview.png"
foreach ($path in @($renderedPath, $previewPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "PBR energy reference evidence is missing: $path"
    }
}

$rendered = Get-PixelStatistics -Path $renderedPath -Step 4
$preview = Get-PixelStatistics -Path $previewPath -Step 4
if ($rendered.Width -lt 160 -or $rendered.Height -lt 120 -or
    $rendered.Width -ne $preview.Width -or $rendered.Height -ne $preview.Height) {
    throw "PBR energy reference images have invalid or inconsistent dimensions."
}
if ($rendered.ClippedFraction -gt 0.08 -or $rendered.Mean -lt 8.0 -or
    $rendered.Mean -gt 238.0 -or $rendered.StandardDeviation -lt 12.0) {
    throw "PBR energy Rendered image is clipped, empty, or too flat (mean=$([Math]::Round($rendered.Mean, 2)), standard-deviation=$([Math]::Round($rendered.StandardDeviation, 2)), clipped=$([Math]::Round($rendered.ClippedFraction, 4)))."
}
if ($preview.StandardDeviation -lt 8.0) {
    throw "PBR energy Material Preview image is too flat."
}

$centers = if ($expectedView -eq "close") {
    @(
        @(0.319, 0.235), @(0.506, 0.235), @(0.693, 0.235),
        @(0.319, 0.543), @(0.506, 0.543), @(0.693, 0.543),
        @(0.319, 0.851), @(0.506, 0.851), @(0.693, 0.851)
    )
}
else {
    @(
        @(0.115, 0.500), @(0.211, 0.500), @(0.307, 0.500),
        @(0.403, 0.500), @(0.499, 0.500), @(0.595, 0.500),
        @(0.691, 0.500), @(0.787, 0.500), @(0.883, 0.500)
    )
}
$subjectLuma = @()
foreach ($center in $centers) {
    $subjectLuma += Get-MeanLuma -Path $renderedPath -CenterX $center[0] -CenterY $center[1]
}
$dielectric = @($subjectLuma[0], $subjectLuma[1], $subjectLuma[2])
$metallic = @($subjectLuma[3], $subjectLuma[4], $subjectLuma[5])
$secondary = @($subjectLuma[6], $subjectLuma[7], $subjectLuma[8])
foreach ($group in @($dielectric, $metallic, $secondary)) {
    foreach ($value in $group) {
        if ($value -lt 5.0 -or $value -gt 250.0) {
            throw "PBR energy subject response is outside the bounded visible range."
        }
    }
}
$groupMeans = @(
    ($dielectric | Measure-Object -Average).Average,
    ($metallic | Measure-Object -Average).Average,
    ($secondary | Measure-Object -Average).Average
)
$groupRange = ($groupMeans | Measure-Object -Maximum).Maximum - ($groupMeans | Measure-Object -Minimum).Minimum
if ($groupRange -lt 5.0) {
    throw "PBR energy dielectric, metallic, and secondary-response groups are not distinguishable."
}

Write-Output "PBR energy reference validation: passed (rendered-mean=$([Math]::Round($rendered.Mean, 2)), rendered-sd=$([Math]::Round($rendered.StandardDeviation, 2)), clipped-fraction=$([Math]::Round($rendered.ClippedFraction, 4)), group-range=$([Math]::Round($groupRange, 2)))."
