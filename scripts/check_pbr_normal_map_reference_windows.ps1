param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "PBR normal-map reference directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory "INDEX.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    throw "PBR normal-map reference evidence is missing its capture index."
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
if ($indexText -notmatch "(?m)^Evidence profile: PBR_NORMAL_MAP_REFERENCE\s*$") {
    throw "PBR normal-map reference evidence does not declare its dedicated profile."
}

$metadataPattern = "(?m)CAPTURE_READY_NORMAL_MAP_REFERENCE mode=(?<mode>solid|material_preview|rendered) view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) reference_texture_edge=(?<texture_edge>\d+) .*normal_map_reference=1 normal_map_flat_count=4 normal_map_mapped_count=5 .*reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) draw_expected=1"
$metadata = @([regex]::Matches($indexText, $metadataPattern))
if ($metadata.Count -ne 3) {
    throw "PBR normal-map reference evidence must contain exactly three readiness records."
}
$expectedView = $metadata[0].Groups["view"].Value
$expectedLayout = if ($expectedView -eq "close") { "close_grid" } else { "wide_row" }
foreach ($entry in $metadata) {
    if ($entry.Groups["view"].Value -ne $expectedView -or
        $entry.Groups["layout"].Value -ne $expectedLayout -or
        [int]$entry.Groups["texture_edge"].Value -lt 32 -or
        [int]$entry.Groups["count"].Value -ne 9 -or
        [int]$entry.Groups["settled"].Value -lt 3) {
        throw "PBR normal-map reference metadata is inconsistent or incomplete."
    }
}

Add-Type -AssemblyName System.Drawing

function Get-MeanNeighborDifference {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][double]$CenterX,
        [Parameter(Mandatory = $true)][double]$CenterY
    )

    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        $centerPixelX = [int][Math]::Round($bitmap.Width * $CenterX)
        $centerPixelY = [int][Math]::Round($bitmap.Height * $CenterY)
        $radius = 42
        [double]$sum = 0.0
        [int]$count = 0
        for ($y = $centerPixelY - $radius; $y -lt $centerPixelY + $radius; $y += 2) {
            for ($x = $centerPixelX - $radius; $x -lt $centerPixelX + $radius; $x += 2) {
                $pixel = $bitmap.GetPixel($x, $y)
                $right = $bitmap.GetPixel($x + 2, $y)
                $down = $bitmap.GetPixel($x, $y + 2)
                $sum += ([Math]::Abs($pixel.R - $right.R) +
                    [Math]::Abs($pixel.G - $right.G) +
                    [Math]::Abs($pixel.B - $right.B)) / 3.0
                $sum += ([Math]::Abs($pixel.R - $down.R) +
                    [Math]::Abs($pixel.G - $down.G) +
                    [Math]::Abs($pixel.B - $down.B)) / 3.0
                $count += 2
            }
        }
        return $sum / $count
    }
    finally {
        $bitmap.Dispose()
    }
}

$renderedPath = Join-Path $InputDirectory "normal-map-reference-$expectedView-rendered.png"
if (-not (Test-Path -LiteralPath $renderedPath -PathType Leaf)) {
    throw "PBR normal-map rendered evidence is missing: $renderedPath"
}
$centers = @(
    [pscustomobject]@{ X = 0.319; Y = 0.235; Mapped = $false }
    [pscustomobject]@{ X = 0.506; Y = 0.235; Mapped = $true }
    [pscustomobject]@{ X = 0.693; Y = 0.235; Mapped = $false }
    [pscustomobject]@{ X = 0.319; Y = 0.543; Mapped = $true }
    [pscustomobject]@{ X = 0.506; Y = 0.543; Mapped = $false }
    [pscustomobject]@{ X = 0.693; Y = 0.543; Mapped = $true }
    [pscustomobject]@{ X = 0.319; Y = 0.851; Mapped = $false }
    [pscustomobject]@{ X = 0.506; Y = 0.851; Mapped = $true }
    [pscustomobject]@{ X = 0.693; Y = 0.851; Mapped = $true }
)
[double]$flatSum = 0.0
[double]$mappedSum = 0.0
[int]$flatCount = 0
[int]$mappedCount = 0
foreach ($center in $centers) {
    $difference = Get-MeanNeighborDifference $renderedPath $center.X $center.Y
    if ($center.Mapped) {
        $mappedSum += $difference
        ++$mappedCount
    }
    else {
        $flatSum += $difference
        ++$flatCount
    }
}
$flatMean = $flatSum / $flatCount
$mappedMean = $mappedSum / $mappedCount
if ($mappedMean -le ($flatMean + 0.08)) {
    throw "Rendered normal-map response was not distinguishable from the matched flat controls (flat=$([Math]::Round($flatMean, 3)), mapped=$([Math]::Round($mappedMean, 3)))."
}

Write-Output "PBR normal-map reference validation: passed (flat=$([Math]::Round($flatMean, 3)), mapped=$([Math]::Round($mappedMean, 3)))."
