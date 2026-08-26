param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "PBR color-space reference directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory "INDEX.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    throw "PBR color-space reference evidence is missing its capture index."
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
if ($indexText -notmatch "(?m)^Evidence profile: PBR_COLOR_SPACE_REFERENCE\s*$") {
    throw "PBR color-space reference evidence does not declare its dedicated profile."
}

$metadataPattern = "(?m)CAPTURE_READY_COLOR_SPACE_REFERENCE mode=(?<mode>solid|material_preview|rendered) view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) reference_texture_edge=(?<texture_edge>\d+) .*color_space_reference=1 color_space_srgb=1 color_space_linear=1 color_space_shared_source=1 color_space_srgb_count=4 color_space_linear_count=5 .*reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) draw_expected=1"
$metadata = @([regex]::Matches($indexText, $metadataPattern))
if ($metadata.Count -ne 3) {
    throw "PBR color-space reference evidence must contain exactly three readiness records."
}
$expectedView = $metadata[0].Groups["view"].Value
$expectedLayout = if ($expectedView -eq "close") { "close_grid" } else { "wide_row" }
foreach ($entry in $metadata) {
    if ($entry.Groups["view"].Value -ne $expectedView -or
        $entry.Groups["layout"].Value -ne $expectedLayout -or
        [int]$entry.Groups["texture_edge"].Value -lt 32 -or
        [int]$entry.Groups["count"].Value -ne 9 -or
        [int]$entry.Groups["settled"].Value -lt 3) {
        throw "PBR color-space reference metadata is inconsistent or incomplete."
    }
}

Add-Type -AssemblyName System.Drawing

function Get-MeanRgb {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][double]$CenterX,
        [Parameter(Mandatory = $true)][double]$CenterY
    )

    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        $centerPixelX = [int][Math]::Round($bitmap.Width * $CenterX)
        $centerPixelY = [int][Math]::Round($bitmap.Height * $CenterY)
        $radius = 38
        [double]$red = 0.0
        [double]$green = 0.0
        [double]$blue = 0.0
        [int]$count = 0
        for ($y = $centerPixelY - $radius; $y -lt $centerPixelY + $radius; ++$y) {
            for ($x = $centerPixelX - $radius; $x -lt $centerPixelX + $radius; ++$x) {
                $pixel = $bitmap.GetPixel($x, $y)
                $red += $pixel.R
                $green += $pixel.G
                $blue += $pixel.B
                ++$count
            }
        }
        return [pscustomobject]@{
            Red = $red / $count
            Green = $green / $count
            Blue = $blue / $count
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-MeanRgbDifference {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][double]$SrgbX,
        [Parameter(Mandatory = $true)][double]$SrgbY,
        [Parameter(Mandatory = $true)][double]$LinearX,
        [Parameter(Mandatory = $true)][double]$LinearY
    )

    $srgb = Get-MeanRgb $Path $SrgbX $SrgbY
    $linear = Get-MeanRgb $Path $LinearX $LinearY
    $srgbLuma = 0.2126 * $srgb.Red + 0.7152 * $srgb.Green + 0.0722 * $srgb.Blue
    $linearLuma = 0.2126 * $linear.Red + 0.7152 * $linear.Green + 0.0722 * $linear.Blue
    return [pscustomobject]@{
        RgbDifference = ([Math]::Abs($linear.Red - $srgb.Red) +
            [Math]::Abs($linear.Green - $srgb.Green) +
            [Math]::Abs($linear.Blue - $srgb.Blue)) / 3.0
        SrgbLuma = $srgbLuma
        LinearLuma = $linearLuma
    }
}

$renderedPath = Join-Path $InputDirectory "color-space-reference-$expectedView-rendered.png"
if (-not (Test-Path -LiteralPath $renderedPath -PathType Leaf)) {
    throw "PBR color-space rendered evidence is missing: $renderedPath"
}

$pairs = @(
    [pscustomobject]@{ SrgbX = 0.319; SrgbY = 0.235; LinearX = 0.506; LinearY = 0.235 }
    [pscustomobject]@{ SrgbX = 0.693; SrgbY = 0.235; LinearX = 0.319; LinearY = 0.543 }
    [pscustomobject]@{ SrgbX = 0.506; SrgbY = 0.543; LinearX = 0.693; LinearY = 0.543 }
    [pscustomobject]@{ SrgbX = 0.319; SrgbY = 0.851; LinearX = 0.506; LinearY = 0.851 }
)
$differences = @()
foreach ($pair in $pairs) {
    $differences += Get-MeanRgbDifference $renderedPath $pair.SrgbX $pair.SrgbY $pair.LinearX $pair.LinearY
}
$meanRgbDifference = ($differences | Measure-Object -Property RgbDifference -Average).Average
$srgbLuma = ($differences | Measure-Object -Property SrgbLuma -Average).Average
$linearLuma = ($differences | Measure-Object -Property LinearLuma -Average).Average
$linearHigherCount = @($differences | Where-Object { $_.LinearLuma -gt ($_.SrgbLuma + 6.0) }).Count

if ($meanRgbDifference -lt 8.0 -or
    $linearLuma -le ($srgbLuma + 6.0) -or
    $linearHigherCount -lt 3) {
    throw "Rendered color-space response was not distinguishable from the matched sRGB controls (sRGB-luma=$([Math]::Round($srgbLuma, 2)), linear-luma=$([Math]::Round($linearLuma, 2)), mean-rgb-difference=$([Math]::Round($meanRgbDifference, 2)), higher-pairs=$linearHigherCount)."
}

Write-Output "PBR color-space reference validation: passed (sRGB-luma=$([Math]::Round($srgbLuma, 2)), linear-luma=$([Math]::Round($linearLuma, 2)), mean-rgb-difference=$([Math]::Round($meanRgbDifference, 2)))."
