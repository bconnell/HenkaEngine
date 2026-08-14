param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "Showcase visual evidence directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory "INDEX.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    throw "Showcase visual evidence is missing its capture index."
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
if ($indexText -notmatch "Source: .*henka_sandbox3d\.exe") {
    throw "Showcase visual evidence does not identify the Henka Sandbox executable."
}

Add-Type -AssemblyName System.Drawing

function Get-ImageMetrics {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label evidence is missing: $Path"
    }
    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        if ($bitmap.Width -lt 160 -or $bitmap.Height -lt 120) {
            throw "$Label evidence has invalid dimensions ($($bitmap.Width)x$($bitmap.Height))."
        }
        [double]$sum = 0.0
        [double]$sumSquares = 0.0
        [double]$saturation = 0.0
        [double]$minimum = 255.0
        [double]$maximum = 0.0
        [int]$count = 0
        $step = [Math]::Max(1, [int][Math]::Floor($bitmap.Width / 160.0))
        for ($y = 0; $y -lt $bitmap.Height; $y += $step) {
            for ($x = 0; $x -lt $bitmap.Width; $x += $step) {
                $pixel = $bitmap.GetPixel($x, $y)
                [double]$luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                [double]$high = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B))
                [double]$low = [Math]::Min($pixel.R, [Math]::Min($pixel.G, $pixel.B))
                $sum += $luma
                $sumSquares += $luma * $luma
                $saturation += $high - $low
                $minimum = [Math]::Min($minimum, $luma)
                $maximum = [Math]::Max($maximum, $luma)
                ++$count
            }
        }
        [double]$mean = $sum / $count
        [double]$variance = [Math]::Max(0.0, ($sumSquares / $count) - ($mean * $mean))
        [pscustomobject]@{
            Label = $Label
            Width = $bitmap.Width
            Height = $bitmap.Height
            Mean = $mean
            StandardDeviation = [Math]::Sqrt($variance)
            LumaRange = $maximum - $minimum
            MeanSaturation = $saturation / $count
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-MeanRgbDifference {
    param(
        [Parameter(Mandatory = $true)][string]$LeftPath,
        [Parameter(Mandatory = $true)][string]$RightPath
    )

    $left = [System.Drawing.Bitmap]::new($LeftPath)
    $right = [System.Drawing.Bitmap]::new($RightPath)
    try {
        if ($left.Width -ne $right.Width -or $left.Height -ne $right.Height) {
            throw "Showcase preview and Rendered evidence dimensions do not match."
        }
        [double]$sum = 0.0
        [int]$count = 0
        $step = [Math]::Max(1, [int][Math]::Floor($left.Width / 160.0))
        for ($y = 0; $y -lt $left.Height; $y += $step) {
            for ($x = 0; $x -lt $left.Width; $x += $step) {
                $a = $left.GetPixel($x, $y)
                $b = $right.GetPixel($x, $y)
                $sum += ([Math]::Abs($a.R - $b.R) + [Math]::Abs($a.G - $b.G) + [Math]::Abs($a.B - $b.B)) / 3.0
                ++$count
            }
        }
        return $sum / $count
    }
    finally {
        $left.Dispose()
        $right.Dispose()
    }
}

$required = [ordered]@{
    "startup" = "startup-showcase.png"
    "front Rendered" = "giraffe-front-rendered.png"
    "three-quarter Rendered" = "giraffe-three-quarter-rendered.png"
    "profile Rendered" = "giraffe-profile-rendered.png"
    "wide Rendered" = "giraffe-wide-rendered.png"
    "front Material Preview" = "giraffe-front-material-preview.png"
}
$measurements = New-Object System.Collections.Generic.List[object]
foreach ($label in $required.Keys) {
    $metrics = Get-ImageMetrics (Join-Path $InputDirectory $required[$label]) $label
    if ($metrics.StandardDeviation -lt 2.0 -or $metrics.LumaRange -lt 18.0) {
        throw "$label evidence is too flat (std=$([Math]::Round($metrics.StandardDeviation, 2)), range=$([Math]::Round($metrics.LumaRange, 2)))."
    }
    if ($metrics.MeanSaturation -lt 4.0) {
        throw "$label evidence does not communicate material identity (mean saturation=$([Math]::Round($metrics.MeanSaturation, 2)))."
    }
    [void]$measurements.Add($metrics)
}

$difference = Get-MeanRgbDifference `
    (Join-Path $InputDirectory $required["front Material Preview"]) `
    (Join-Path $InputDirectory $required["front Rendered"])
if ($difference -lt 2.0) {
    throw "Front Rendered evidence is not materially distinct from Material Preview (mean RGB difference=$([Math]::Round($difference, 2)))."
}

$summary = @(
    "Showcase visual evidence validation: passed",
    "Application-only source: Henka Sandbox executable identified in INDEX.txt",
    "Required views: startup, close front, close three-quarter, close profile, wide silhouette",
    "Rendered and Material Preview front mean RGB difference: $([Math]::Round($difference, 2))",
    "Objective guards: non-flat, chromatic, dimension-valid frames for every required view",
    "Status: automated evidence guard passed; human visual inspection remains required"
)
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "showcase-visual-validation.txt")
$summary | ForEach-Object { Write-Output $_ }
