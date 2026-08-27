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

$stdoutPaths = Get-ChildItem -LiteralPath $InputDirectory -Filter "realism_reference_*.stdout.txt" -File
foreach ($stdoutPath in $stdoutPaths) {
    $stdoutText = Get-Content -LiteralPath $stdoutPath.FullName -Raw
    if ($stdoutText -notmatch "Realism reference capture: debug grid hidden\.") {
        throw "Realism reference capture did not prove that the debug grid was hidden in $($stdoutPath.Name)."
    }
}

function Get-ReferenceMetadata {
    param(
        [Parameter(Mandatory = $true)][string]$Mode
    )

    $match = [regex]::Match(
        $indexText,
        "(?m)CAPTURE_READY_REFERENCE mode=$Mode view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) reference_texture_edge=(?<texture_edge>\d+) reference_exposure_stops=(?<exposure>[-0-9.]+) viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>\d+) aspect=(?<aspect>[-0-9.]+) .* pitch=(?<pitch>[-0-9.]+) roll=(?<roll>[-0-9.]+) .* reference_bounds=(?<cx>[-0-9.]+),(?<cy>[-0-9.]+),(?<cz>[-0-9.]+),(?<ex>[-0-9.]+),(?<ey>[-0-9.]+),(?<ez>[-0-9.]+) reference_midpoint=(?<mx>[-0-9.]+),(?<my>[-0-9.]+) reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) draw_expected=1")
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
$expectedLayout = if ($view -eq "close") { "close_grid" } else { "wide_row" }
if ($metadata[0].Groups["layout"].Value -ne $expectedLayout) {
    throw "Realism reference metadata declared an unexpected layout: $($metadata[0].Groups["layout"].Value)."
}
if ([int]$metadata[0].Groups["texture_edge"].Value -lt 64) {
    throw "Realism reference textures are below the minimum detail resolution."
}
foreach ($stdoutPath in $stdoutPaths) {
    $stdoutText = Get-Content -LiteralPath $stdoutPath.FullName -Raw
    if ($stdoutText -notmatch "reference_layout=$expectedLayout(?:\s|$)") {
        throw "Realism reference capture did not prove the expected $expectedLayout layout in $($stdoutPath.Name)."
    }
}
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

function Get-SmoothSubjectNeighborDifference {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][ValidateSet("wide", "close")][string]$View
    )

    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        $centers = if ($View -eq "close") {
            @(
                [pscustomobject]@{ X = 0.319; Y = 0.235 }
                [pscustomobject]@{ X = 0.506; Y = 0.235 }
                [pscustomobject]@{ X = 0.693; Y = 0.235 }
                [pscustomobject]@{ X = 0.319; Y = 0.543 }
                [pscustomobject]@{ X = 0.506; Y = 0.543 }
                [pscustomobject]@{ X = 0.693; Y = 0.543 }
                [pscustomobject]@{ X = 0.319; Y = 0.851 }
                [pscustomobject]@{ X = 0.506; Y = 0.851 }
                [pscustomobject]@{ X = 0.693; Y = 0.851 }
            )
        }
        else {
            @(
                [pscustomobject]@{ X = 0.125; Y = 0.520 }
                [pscustomobject]@{ X = 0.234; Y = 0.520 }
                [pscustomobject]@{ X = 0.344; Y = 0.520 }
                [pscustomobject]@{ X = 0.453; Y = 0.520 }
            )
        }
        [double]$sum = 0.0
        [int]$count = 0
        foreach ($center in $centers) {
            $centerX = [int][Math]::Round($bitmap.Width * $center.X)
            $centerY = [int][Math]::Round($bitmap.Height * $center.Y)
            for ($y = $centerY - 22; $y -lt $centerY + 22; $y += 2) {
                for ($x = $centerX - 22; $x -lt $centerX + 22; $x += 2) {
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
        }
        return $sum / $count
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-SubjectLumaMetrics {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][double]$CenterX,
        [Parameter(Mandatory = $true)][double]$CenterY
    )

    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        $centerPixelX = [int][Math]::Round($bitmap.Width * $CenterX)
        $centerPixelY = [int][Math]::Round($bitmap.Height * $CenterY)
        $radius = 55
        [double]$sum = 0.0
        [int]$count = 0
        for ($y = $centerPixelY - $radius; $y -le $centerPixelY + $radius; $y += 2) {
            for ($x = $centerPixelX - $radius; $x -le $centerPixelX + $radius; $x += 2) {
                $dx = $x - $centerPixelX
                $dy = $y - $centerPixelY
                if (($dx * $dx) + ($dy * $dy) -gt ($radius * $radius)) {
                    continue
                }
                $pixel = $bitmap.GetPixel($x, $y)
                $sum += 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                ++$count
            }
        }
        if ($count -le 0) {
            throw "Reference subject region contained no pixels."
        }
        return $sum / $count
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-MeanLumaRect {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y,
        [Parameter(Mandatory = $true)][double]$Width,
        [Parameter(Mandatory = $true)][double]$Height
    )

    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        $minimumX = [int][Math]::Round($bitmap.Width * $X)
        $minimumY = [int][Math]::Round($bitmap.Height * $Y)
        $maximumX = [Math]::Min($bitmap.Width, [int][Math]::Round($bitmap.Width * ($X + $Width)))
        $maximumY = [Math]::Min($bitmap.Height, [int][Math]::Round($bitmap.Height * ($Y + $Height)))
        [double]$sum = 0.0
        [int]$count = 0
        for ($y = $minimumY; $y -lt $maximumY; $y += 2) {
            for ($x = $minimumX; $x -lt $maximumX; $x += 2) {
                $pixel = $bitmap.GetPixel($x, $y)
                $sum += 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                ++$count
            }
        }
        if ($count -le 0) {
            throw "Reference luma region contained no pixels."
        }
        return $sum / $count
    }
    finally {
        $bitmap.Dispose()
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

$renderedSmoothSubjectDifference = Get-SmoothSubjectNeighborDifference `
    (Join-Path $InputDirectory $files["rendered"]) `
    $view
if ($renderedSmoothSubjectDifference -gt 1.25) {
    throw "Realism Rendered smooth subjects contain excessive structured variation (mean neighbor RGB difference=$([Math]::Round($renderedSmoothSubjectDifference, 2)))."
}

if ($view -eq "close") {
    $renderedRoughMetalLuma = Get-SubjectLumaMetrics `
        (Join-Path $InputDirectory $files["rendered"]) 0.319 0.235
    $renderedPolishedMetalLuma = Get-SubjectLumaMetrics `
        (Join-Path $InputDirectory $files["rendered"]) 0.506 0.235
    if ($renderedRoughMetalLuma -le ($renderedPolishedMetalLuma + 3.0)) {
        throw "Realism Rendered matched-metal roughness response was not distinguishable under the reference lighting (rough=$([Math]::Round($renderedRoughMetalLuma, 2)), polished=$([Math]::Round($renderedPolishedMetalLuma, 2)))."
    }

    $renderedShadowLuma = Get-MeanLumaRect `
        (Join-Path $InputDirectory $files["rendered"]) 0.133 0.896 0.266 0.080
    $renderedGroundControlLuma = Get-MeanLumaRect `
        (Join-Path $InputDirectory $files["rendered"]) 0.703 0.896 0.219 0.080
    if ($renderedGroundControlLuma -le ($renderedShadowLuma + 12.0)) {
        throw "Realism Rendered contact-shadow contrast was not preserved (shadow=$([Math]::Round($renderedShadowLuma, 2)), ground_control=$([Math]::Round($renderedGroundControlLuma, 2)))."
    }
}

$summary = @(
    "Realism reference visual evidence validation: passed",
    "Reference view: $view",
    "Nine deterministic PBR material subjects: settled and centered",
    "Rendered versus Material Preview mean RGB difference: $([Math]::Round($difference, 2))",
    "Rendered smooth-subject mean neighbor RGB difference: $([Math]::Round($renderedSmoothSubjectDifference, 2))",
    $(if ($view -eq "close") { "Rendered matched neutral-metal roughness response is distinguishable" }),
    $(if ($view -eq "close") { "Rendered contact-shadow contrast is present" }),
    "Status: automated reference-scene guard passed; human visual inspection remains required"
)
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "realism-reference-visual-validation.txt")
$summary | ForEach-Object { Write-Output $_ }
