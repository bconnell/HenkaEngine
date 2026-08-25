param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "Realism reference evidence directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory "INDEX.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    throw "Realism reference evidence is missing its capture index."
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
if ($indexText -notmatch "(?m)^Evidence profile: REALISM_REFERENCE\s*$") {
    throw "Realism reference evidence does not declare the REALISM_REFERENCE profile."
}

$stderrPaths = Get-ChildItem -LiteralPath $InputDirectory -Filter "realism_reference_*.stderr.txt" -File
foreach ($stderrPath in $stderrPaths) {
    $stderrText = Get-Content -LiteralPath $stderrPath.FullName -Raw
    if ($stderrText -match "Realism reference capture could not frame its bounded fixture\.") {
        throw "Realism reference capture reported a framing failure in $($stderrPath.Name)."
    }
}

function Get-ReferenceMetadata {
    param(
        [Parameter(Mandatory = $true)][string]$Mode
    )

    $match = [regex]::Match(
        $indexText,
        "(?m)CAPTURE_READY_REFERENCE mode=$Mode view=(?<view>wide|close) viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>\d+) aspect=(?<aspect>[-0-9.]+) .* pitch=(?<pitch>[-0-9.]+) roll=(?<roll>[-0-9.]+) .* reference_bounds=(?<cx>[-0-9.]+),(?<cy>[-0-9.]+),(?<cz>[-0-9.]+),(?<ex>[-0-9.]+),(?<ey>[-0-9.]+),(?<ez>[-0-9.]+) reference_midpoint=(?<mx>[-0-9.]+),(?<my>[-0-9.]+) reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) draw_expected=1")
    if (-not $match.Success) {
        throw "Realism reference evidence is missing valid CAPTURE_READY_REFERENCE metadata for $Mode."
    }
    return $match
}

$metadata = @(
    Get-ReferenceMetadata "solid"
    Get-ReferenceMetadata "material_preview"
    Get-ReferenceMetadata "rendered"
)
$view = $metadata[0].Groups["view"].Value
$canonical = $metadata[0].Value -replace 'mode=(solid|material_preview|rendered)', 'mode=shared'
foreach ($entry in $metadata) {
    if ($entry.Groups["view"].Value -ne $view) {
        throw "Realism reference capture view diverged between shading modes."
    }
    if (($entry.Value -replace 'mode=(solid|material_preview|rendered)', 'mode=shared') -ne $canonical) {
        throw "Realism reference composition metadata diverged between shading modes."
    }
}

foreach ($entry in $metadata) {
    $width = [int]$entry.Groups["vw"].Value
    $height = [int]$entry.Groups["vh"].Value
    $pitch = [double]::Parse($entry.Groups["pitch"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $roll = [double]::Parse($entry.Groups["roll"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $midpointX = [double]::Parse($entry.Groups["mx"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $midpointY = [double]::Parse($entry.Groups["my"].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ($width -le 0 -or $height -le 0 -or [Math]::Abs($pitch) -gt 0.001 -or [Math]::Abs($roll) -gt 0.001) {
        throw "Realism reference metadata is not a valid level capture."
    }
    if ([Math]::Abs($midpointX - ($width / 2.0)) -gt ($width * 0.03) -or
        [Math]::Abs($midpointY - ($height / 2.0)) -gt ($height * 0.03)) {
        throw "Realism reference bounds are not centered in the capture."
    }
    if ([int]$entry.Groups["count"].Value -ne 9 -or
        [int]$entry.Groups["settled"].Value -lt 3) {
        throw "Realism reference metadata does not prove all nine settled materials."
    }
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
        return [pscustomobject]@{
            Label = $Label
            Width = $bitmap.Width
            Height = $bitmap.Height
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
            throw "Realism reference preview and Rendered evidence dimensions do not match."
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

$files = @{
    "solid" = "realism-reference-$view-solid.png"
    "material_preview" = "realism-reference-$view-material-preview.png"
    "rendered" = "realism-reference-$view-rendered.png"
}
$measurements = New-Object System.Collections.Generic.List[object]
foreach ($mode in @("solid", "material_preview", "rendered")) {
    $metrics = Get-ImageMetrics (Join-Path $InputDirectory $files[$mode]) "realism reference $mode"
    if ($metrics.StandardDeviation -lt 2.0 -or $metrics.LumaRange -lt 18.0) {
        throw "Realism reference $mode evidence is too flat (std=$([Math]::Round($metrics.StandardDeviation, 2)), range=$([Math]::Round($metrics.LumaRange, 2)))."
    }
    if ($metrics.MeanSaturation -lt 4.0) {
        throw "Realism reference $mode evidence does not communicate material identity."
    }
    [void]$measurements.Add($metrics)
}

$difference = Get-MeanRgbDifference `
    (Join-Path $InputDirectory $files["material_preview"]) `
    (Join-Path $InputDirectory $files["rendered"])
if ($difference -lt 2.0) {
    throw "Realism Rendered evidence is not materially distinct from Material Preview (mean RGB difference=$([Math]::Round($difference, 2)))."
}

$summary = @(
    "Realism reference visual evidence validation: passed",
    "Reference view: $view",
    "Nine deterministic PBR material subjects: settled and centered",
    "Rendered versus Material Preview mean RGB difference: $([Math]::Round($difference, 2))",
    "Status: automated reference-scene guard passed; human visual inspection remains required"
)
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "realism-reference-visual-validation.txt")
$summary | ForEach-Object { Write-Output $_ }
