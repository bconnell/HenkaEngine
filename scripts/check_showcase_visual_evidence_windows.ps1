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
if ($indexText -notmatch "(?m)^Evidence profile: FULL_SHOWCASE\s*$") {
    throw "Showcase visual evidence does not declare the FULL_SHOWCASE profile."
}

function Get-CaptureMetadata {
    param(
        [Parameter(Mandatory = $true)][string]$Mode
    )

    $match = [regex]::Match(
        $indexText,
        "(?m)CAPTURE_READY mode=$Mode viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>\d+) aspect=(?<aspect>[-0-9.]+) .* pitch=(?<pitch>[-0-9.]+) roll=(?<roll>[-0-9.]+) .* giraffe_screen=(?<gminx>[-0-9.]+),(?<gminy>[-0-9.]+),(?<gmaxx>[-0-9.]+),(?<gmaxy>[-0-9.]+) rocket_screen=(?<rminx>[-0-9.]+),(?<rminy>[-0-9.]+),(?<rmaxx>[-0-9.]+),(?<rmaxy>[-0-9.]+) combined_midpoint=(?<mx>[-0-9.]+),(?<my>[-0-9.]+) giraffe_parts=(?<gp>\d+) rocket_parts=(?<rp>\d+) giraffe_sss_regions=(?<sss>\d+) giraffe_normal_texture_regions=(?<normal>\d+) giraffe_normal_texture_loaded=(?<loaded>\d+) giraffe_normal_texture_fallbacks=(?<fallback>\d+) giraffe_thickness_texture_regions=(?<thickness>\d+) giraffe_thickness_texture_loaded=(?<thicknessLoaded>\d+) giraffe_thickness_texture_fallbacks=(?<thicknessFallback>\d+) settled_frames=(?<sf>\d+) giraffe_provenance=(?<giraffeProvenance>[A-Z_]+) rocket_provenance=(?<rocketProvenance>[A-Z_]+) preset_applied=(?<presetApplied>[01]) draw_expected=1")
    if (-not $match.Success) {
        throw "Showcase evidence is missing valid CAPTURE_READY metadata for $Mode."
    }
    return $match
}

$captureMetadata = @(
    Get-CaptureMetadata "solid"
    Get-CaptureMetadata "material_preview"
    Get-CaptureMetadata "rendered"
)
$canonicalCaptureMetadata = $captureMetadata[0].Value -replace 'mode=(solid|material_preview|rendered)', 'mode=shared'
foreach ($metadata in $captureMetadata) {
    if (($metadata.Value -replace 'mode=(solid|material_preview|rendered)', 'mode=shared') -ne $canonicalCaptureMetadata) {
        throw "Showcase composition metadata diverges across shading modes."
    }
    if ($metadata.Groups["giraffeProvenance"].Value -notin @("GENERATED_TEST_FIXTURE", "IMPORT_COMPATIBILITY_ASSET", "HENKA_NATIVE_GENERATED_FIXTURE", "HENKA_NATIVE_EDITED_FIXTURE", "HENKA_NATIVE_AUTHORED") -or
        $metadata.Groups["rocketProvenance"].Value -notin @("GENERATED_TEST_FIXTURE", "IMPORT_COMPATIBILITY_ASSET", "HENKA_NATIVE_GENERATED_FIXTURE", "HENKA_NATIVE_EDITED_FIXTURE", "HENKA_NATIVE_AUTHORED") -or
        $metadata.Groups["presetApplied"].Value -ne "0") {
        throw "Showcase evidence contains unknown or preset-determined geometry provenance."
    }
}
foreach ($requiredMode in @("solid", "material_preview", "rendered")) {
    $metadata = Get-CaptureMetadata $requiredMode
    $width = [int]$metadata.Groups["vw"].Value
    $height = [int]$metadata.Groups["vh"].Value
    $pitch = [double]::Parse($metadata.Groups["pitch"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $roll = [double]::Parse($metadata.Groups["roll"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $midpointX = [double]::Parse($metadata.Groups["mx"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $midpointY = [double]::Parse($metadata.Groups["my"].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ([Math]::Abs($pitch) -gt 0.001 -or [Math]::Abs($roll) -gt 0.001) {
        throw "Showcase $requiredMode metadata is not level."
    }
    if ([Math]::Abs($midpointX - ($width / 2.0)) -gt ($width * 0.02) -or
        [Math]::Abs($midpointY - ($height / 2.0)) -gt ($height * 0.02)) {
        throw "Showcase $requiredMode metadata does not center the combined midpoint."
    }
    foreach ($prefix in @("g", "r")) {
        $minX = [double]::Parse($metadata.Groups["$($prefix)minx"].Value, [Globalization.CultureInfo]::InvariantCulture)
        $minY = [double]::Parse($metadata.Groups["$($prefix)miny"].Value, [Globalization.CultureInfo]::InvariantCulture)
        $maxX = [double]::Parse($metadata.Groups["$($prefix)maxx"].Value, [Globalization.CultureInfo]::InvariantCulture)
        $maxY = [double]::Parse($metadata.Groups["$($prefix)maxy"].Value, [Globalization.CultureInfo]::InvariantCulture)
        if ($minX -lt ($width * 0.04) -or $minY -lt ($height * 0.04) -or
            $maxX -gt ($width * 0.96) -or $maxY -gt ($height * 0.96) -or
            $maxX -le $minX -or $maxY -le $minY) {
            throw "Showcase $requiredMode metadata reports a cropped subject."
        }
    }
    if ([int]$metadata.Groups["gp"].Value -lt 13 -or
        [int]$metadata.Groups["rp"].Value -lt 13 -or
        [int]$metadata.Groups["sss"].Value -lt 1 -or
        [int]$metadata.Groups["normal"].Value -lt 1 -or
        [int]$metadata.Groups["normal"].Value -ne [int]$metadata.Groups["sss"].Value -or
        [int]$metadata.Groups["loaded"].Value -ne [int]$metadata.Groups["normal"].Value -or
        [int]$metadata.Groups["fallback"].Value -ne 0 -or
        [int]$metadata.Groups["thickness"].Value -ne [int]$metadata.Groups["sss"].Value -or
        [int]$metadata.Groups["thicknessLoaded"].Value -ne [int]$metadata.Groups["thickness"].Value -or
        [int]$metadata.Groups["thicknessFallback"].Value -ne 0 -or
        [int]$metadata.Groups["sf"].Value -lt 3) {
        throw "Showcase $requiredMode metadata does not prove subjects, material dependencies, subsurface setup, and settled frames."
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
    "pair Solid" = "same-camera-solid.png"
    "pair Material Preview" = "same-camera-material-preview.png"
    "pair Rendered" = "same-camera-rendered.png"
    "giraffe front Rendered" = "giraffe-front-rendered.png"
    "giraffe three-quarter Rendered" = "giraffe-three-quarter-rendered.png"
    "giraffe profile Rendered" = "giraffe-profile-rendered.png"
    "giraffe wide Rendered" = "giraffe-wide-rendered.png"
    "giraffe front Material Preview" = "giraffe-front-material-preview.png"
    "rocket front Rendered" = "rocket-front-rendered.png"
    "rocket three-quarter Rendered" = "rocket-three-quarter-rendered.png"
    "rocket profile Rendered" = "rocket-profile-rendered.png"
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
    (Join-Path $InputDirectory $required["giraffe front Material Preview"]) `
    (Join-Path $InputDirectory $required["giraffe front Rendered"])
if ($difference -lt 2.0) {
    throw "Front Rendered evidence is not materially distinct from Material Preview (mean RGB difference=$([Math]::Round($difference, 2)))."
}

$summary = @(
    "Showcase visual evidence validation: passed",
    "Application-only source: Henka Sandbox executable identified in INDEX.txt",
    "Required views: startup, close front, close three-quarter, close profile, wide silhouette",
    "Giraffe Rendered and Material Preview front mean RGB difference: $([Math]::Round($difference, 2))",
    "Objective guards: non-flat, chromatic, dimension-valid frames for every required view",
    "Status: automated evidence guard passed; human visual inspection remains required"
)
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "showcase-visual-validation.txt")
$summary | ForEach-Object { Write-Output $_ }
