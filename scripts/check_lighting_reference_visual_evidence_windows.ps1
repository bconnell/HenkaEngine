param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "Lighting reference evidence directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory "INDEX.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    throw "Lighting reference evidence is missing its capture index."
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
if ($indexText -notmatch "(?m)^Evidence profile: LIGHTING_REFERENCE\s*$") {
    throw "Lighting reference evidence does not declare the LIGHTING_REFERENCE profile."
}

foreach ($stderrPath in Get-ChildItem -LiteralPath $InputDirectory -Filter "lighting_reference_*.stderr.txt" -File) {
    $stderrText = Get-Content -LiteralPath $stderrPath.FullName -Raw
    if ($stderrText -match "Realism reference capture could not frame its bounded fixture\.") {
        throw "Lighting reference capture reported a framing failure in $($stderrPath.Name)."
    }
}

foreach ($stdoutPath in Get-ChildItem -LiteralPath $InputDirectory -Filter "lighting_reference_*.stdout.txt" -File) {
    $stdoutText = Get-Content -LiteralPath $stdoutPath.FullName -Raw
    if ($stdoutText -notmatch "Realism reference capture: debug grid hidden\.") {
        throw "Lighting reference capture did not prove that the debug grid was hidden in $($stdoutPath.Name)."
    }
}

function Get-ReferenceMetadata {
    param([Parameter(Mandatory = $true)][string]$Mode)

    $pattern = "(?m)CAPTURE_READY_LIGHTING_REFERENCE mode=$Mode view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) reference_texture_edge=(?<texture_edge>\d+) reference_exposure_stops=(?<exposure>[-0-9.]+) viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>\d+) aspect=(?<aspect>[-0-9.]+) .* pitch=(?<pitch>[-0-9.]+) roll=(?<roll>[-0-9.]+) .* reference_bounds=(?<cx>[-0-9.]+),(?<cy>[-0-9.]+),(?<cz>[-0-9.]+),(?<ex>[-0-9.]+),(?<ey>[-0-9.]+),(?<ez>[-0-9.]+) reference_midpoint=(?<mx>[-0-9.]+),(?<my>[-0-9.]+) reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) draw_expected=1"
    $pattern = $pattern.Replace(
        'reference_exposure_stops=(?<exposure>[-0-9.]+) viewport=',
        'reference_exposure_stops=(?<exposure>[-0-9.]+)(?<shadow> shadow_reference=1 shadow_maps_ready=1)? viewport=')
    $match = [regex]::Match($indexText, $pattern)
    if (-not $match.Success) {
        throw "Lighting reference evidence is missing valid readiness metadata for $Mode."
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
foreach ($entry in $metadata) {
    if ($entry.Groups["view"].Value -ne $view -or
        $entry.Groups["layout"].Value -ne $expectedLayout -or
        [int]$entry.Groups["count"].Value -ne 9 -or
        [int]$entry.Groups["settled"].Value -lt 3) {
        throw "Lighting reference metadata diverged or did not prove nine settled subjects."
    }
}
if (-not $metadata[2].Groups["shadow"].Success) {
    throw "Lighting Rendered evidence did not prove that directional, cascade, and point shadow targets were ready."
}
$canonical = ($metadata[0].Value -replace 'mode=(solid|material_preview|rendered)', 'mode=shared') -replace ' shadow_reference=1 shadow_maps_ready=1', ''
foreach ($entry in $metadata) {
    if ((($entry.Value -replace 'mode=(solid|material_preview|rendered)', 'mode=shared') -replace ' shadow_reference=1 shadow_maps_ready=1', '') -ne $canonical) {
        throw "Lighting reference composition metadata diverged between shading modes."
    }
}

Add-Type -AssemblyName System.Drawing

function Get-ImageMetrics {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Lighting reference evidence is missing: $Path"
    }
    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        if ($bitmap.Width -lt 160 -or $bitmap.Height -lt 120) {
            throw "Lighting reference evidence has invalid dimensions ($($bitmap.Width)x$($bitmap.Height))."
        }
        [double]$sum = 0.0
        [double]$sumSquares = 0.0
        [double]$minimum = 255.0
        [double]$maximum = 0.0
        [double]$saturation = 0.0
        [int]$count = 0
        $step = [Math]::Max(1, [int][Math]::Floor($bitmap.Width / 160.0))
        for ($y = 0; $y -lt $bitmap.Height; $y += $step) {
            for ($x = 0; $x -lt $bitmap.Width; $x += $step) {
                $pixel = $bitmap.GetPixel($x, $y)
                [double]$luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                $high = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B))
                $low = [Math]::Min($pixel.R, [Math]::Min($pixel.G, $pixel.B))
                $sum += $luma
                $sumSquares += $luma * $luma
                $minimum = [Math]::Min($minimum, $luma)
                $maximum = [Math]::Max($maximum, $luma)
                $saturation += $high - $low
                ++$count
            }
        }
        $mean = $sum / $count
        $variance = [Math]::Max(0.0, ($sumSquares / $count) - ($mean * $mean))
        return [pscustomobject]@{
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
            throw "Lighting reference shading-mode dimensions do not match."
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
        $minimumX = [Math]::Max(0, [int][Math]::Round($bitmap.Width * $X))
        $minimumY = [Math]::Max(0, [int][Math]::Round($bitmap.Height * $Y))
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
            throw "Lighting reference luma region contained no pixels."
        }
        return $sum / $count
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-MeanSubjectLuma {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][double]$CenterX,
        [Parameter(Mandatory = $true)][double]$CenterY,
        [int]$Radius = 40
    )

    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        $centerPixelX = [int][Math]::Round($bitmap.Width * $CenterX)
        $centerPixelY = [int][Math]::Round($bitmap.Height * $CenterY)
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
            throw "Lighting reference subject region contained no pixels."
        }
        return $sum / $count
    }
    finally {
        $bitmap.Dispose()
    }
}

$files = @{
    solid = Join-Path $InputDirectory "lighting-reference-$view-solid.png"
    material_preview = Join-Path $InputDirectory "lighting-reference-$view-material-preview.png"
    rendered = Join-Path $InputDirectory "lighting-reference-$view-rendered.png"
}
$measurements = @()
foreach ($mode in @("solid", "material_preview", "rendered")) {
    $metrics = Get-ImageMetrics $files[$mode]
    if ($metrics.StandardDeviation -lt 2.0 -or $metrics.LumaRange -lt 18.0) {
        throw "Lighting reference $mode evidence is too flat (std=$([Math]::Round($metrics.StandardDeviation, 2)), range=$([Math]::Round($metrics.LumaRange, 2)))."
    }
    $measurements += $metrics
}

$difference = Get-MeanRgbDifference $files.material_preview $files.rendered
if ($difference -lt 2.0) {
    throw "Lighting Rendered evidence is not materially distinct from Material Preview (mean RGB difference=$([Math]::Round($difference, 2)))."
}

$renderedLeftLuma = if ($view -eq "close") {
    Get-MeanSubjectLuma $files.rendered 0.319 0.235
}
else {
    Get-MeanSubjectLuma $files.rendered 0.125 0.520 18
}
$renderedRightLuma = if ($view -eq "close") {
    Get-MeanSubjectLuma $files.rendered 0.693 0.235
}
else {
    Get-MeanSubjectLuma $files.rendered 0.884 0.520 18
}
if ([Math]::Abs($renderedLeftLuma - $renderedRightLuma) -lt 12.0) {
    throw "Lighting Rendered evidence does not show a measurable key/fill/rim response (left=$([Math]::Round($renderedLeftLuma, 2)), right=$([Math]::Round($renderedRightLuma, 2)))."
}

$summary = @(
    "Lighting reference visual evidence validation: passed",
    "Reference view: $view",
    "Nine deterministic same-material lighting subjects: settled and centered",
    "Rendered versus Material Preview mean RGB difference: $([Math]::Round($difference, 2))",
    "Rendered key/fill/rim response is distinguishable: left=$([Math]::Round($renderedLeftLuma, 2)), right=$([Math]::Round($renderedRightLuma, 2))",
    "Status: automated lighting reference guard passed; human visual inspection remains required"
)
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "lighting-reference-visual-validation.txt")
$summary | ForEach-Object { Write-Output $_ }
