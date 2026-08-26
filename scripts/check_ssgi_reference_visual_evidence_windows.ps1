param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "SSGI reference evidence directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory "INDEX.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    throw "SSGI reference evidence is missing its capture index."
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
if ($indexText -notmatch "(?m)^Evidence profile: SSGI_REFERENCE\s*$") {
    throw "SSGI reference evidence does not declare the SSGI_REFERENCE profile."
}

$stdoutPath = Join-Path $InputDirectory "ssgi_reference.stdout.txt"
$stderrPath = Join-Path $InputDirectory "ssgi_reference.stderr.txt"
if (-not (Test-Path -LiteralPath $stdoutPath -PathType Leaf)) {
    throw "SSGI reference evidence is missing the capture stdout record."
}
if (Test-Path -LiteralPath $stderrPath -PathType Leaf) {
    $stderrText = Get-Content -LiteralPath $stderrPath -Raw
    if ($stderrText -match "Realism reference capture could not frame its bounded fixture\.") {
        throw "SSGI reference capture reported a framing failure."
    }
}
$stdoutText = Get-Content -LiteralPath $stdoutPath -Raw
if ($stdoutText -notmatch "Realism reference capture: debug grid hidden\.") {
    throw "SSGI reference capture did not prove that the debug grid was hidden."
}
$pattern = "(?m)CAPTURE_READY_SSGI_REFERENCE mode=rendered view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) reference_texture_edge=(?<texture_edge>\d+) reference_exposure_stops=(?<exposure>[-0-9.]+) reference_ssgi_active=1 viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>-?\d+) aspect=(?<aspect>[-0-9.]+) .* reference_bounds=(?<cx>[-0-9.]+),(?<cy>[-0-9.]+),(?<cz>[-0-9.]+),(?<ex>[-0-9.]+),(?<ey>[-0-9.]+),(?<ez>[-0-9.]+) reference_midpoint=(?<mx>[-0-9.]+),(?<my>[-0-9.]+) reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) draw_expected=1"
$records = [regex]::Matches($indexText, $pattern)
if ($records.Count -ne 1) {
    throw "SSGI reference evidence must contain exactly one rendered readiness record."
}
$record = $records[0]
$imagePath = Join-Path $InputDirectory ("ssgi-reference-" + $record.Groups["view"].Value + "-rendered.png")
$expectedLayout = if ($record.Groups["view"].Value -eq "close") { "close_grid" } else { "wide_row" }
if ($record.Groups["layout"].Value -ne $expectedLayout -or
    [int]$record.Groups["texture_edge"].Value -lt 32 -or
    [int]$record.Groups["count"].Value -ne 9 -or
    [int]$record.Groups["settled"].Value -lt 3) {
    throw "SSGI reference metadata did not prove one stable, centered, nine-subject capture."
}

Add-Type -AssemblyName System.Drawing
$bitmap = [System.Drawing.Bitmap]::new($imagePath)
try {
    if ($bitmap.Width -lt 160 -or $bitmap.Height -lt 120) {
        throw "SSGI reference image has invalid dimensions ($($bitmap.Width)x$($bitmap.Height))."
    }
    [double]$sum = 0.0
    [double]$sumSquares = 0.0
    [int]$count = 0
    $step = [Math]::Max(1, [int][Math]::Floor($bitmap.Width / 160.0))
    for ($y = 0; $y -lt $bitmap.Height; $y += $step) {
        for ($x = 0; $x -lt $bitmap.Width; $x += $step) {
            $pixel = $bitmap.GetPixel($x, $y)
            [double]$luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
            $sum += $luma
            $sumSquares += $luma * $luma
            ++$count
        }
    }
    $mean = $sum / $count
    $variance = [Math]::Max(0.0, ($sumSquares / $count) - ($mean * $mean))
    $standardDeviation = [Math]::Sqrt($variance)
    if ($standardDeviation -lt 2.0) {
        throw "SSGI reference image is too flat to prove a stable rendered result."
    }

    [int]$clippedPixelCount = 0
    [int]$brightPixelCount = 0
    [int]$haloSampleCount = 0
    [int]$haloBrightPixelCount = 0
    $sampleStep = [Math]::Max(1, [int][Math]::Floor($bitmap.Width / 160.0))
    $haloCentersX = @(0.328, 0.507, 0.694)
    $haloCentersY = @(0.213, 0.519, 0.814)
    $haloInnerRadius = $bitmap.Width * 0.082
    $haloOuterRadius = $bitmap.Width * 0.098
    for ($y = 0; $y -lt $bitmap.Height; $y += $sampleStep) {
        for ($x = 0; $x -lt $bitmap.Width; $x += $sampleStep) {
            $pixel = $bitmap.GetPixel($x, $y)
            [double]$luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
            if ($luma -ge 250.0) { ++$clippedPixelCount }
            if ($luma -ge 240.0) { ++$brightPixelCount }
        }
    }
    foreach ($centerY in $haloCentersY) {
        foreach ($centerX in $haloCentersX) {
            $centerPixelX = $bitmap.Width * $centerX
            $centerPixelY = $bitmap.Height * $centerY
            $minX = [Math]::Max(0, [int][Math]::Floor($centerPixelX - $haloOuterRadius))
            $maxX = [Math]::Min($bitmap.Width - 1, [int][Math]::Ceiling($centerPixelX + $haloOuterRadius))
            $minY = [Math]::Max(0, [int][Math]::Floor($centerPixelY - $haloOuterRadius))
            $maxY = [Math]::Min($bitmap.Height - 1, [int][Math]::Ceiling($centerPixelY + $haloOuterRadius))
            for ($y = $minY; $y -le $maxY; $y += $sampleStep) {
                for ($x = $minX; $x -le $maxX; $x += $sampleStep) {
                    $dx = $x - $centerPixelX
                    $dy = $y - $centerPixelY
                    $distance = [Math]::Sqrt(($dx * $dx) + ($dy * $dy))
                    if ($distance -lt $haloInnerRadius -or $distance -gt $haloOuterRadius) {
                        continue
                    }
                    $pixel = $bitmap.GetPixel($x, $y)
                    [double]$luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                    ++$haloSampleCount
                    if ($luma -ge 180.0) { ++$haloBrightPixelCount }
                }
            }
        }
    }
    $clippedFraction = $clippedPixelCount / [double]$count
    $brightFraction = $brightPixelCount / [double]$count
    $haloBrightFraction = if ($haloSampleCount -gt 0) {
        $haloBrightPixelCount / [double]$haloSampleCount
    } else {
        1.0
    }
    if ($clippedFraction -gt 0.20 -or $brightFraction -gt 0.30) {
        throw "SSGI reference image is excessively clipped or over-bright (clipped=$([Math]::Round(100.0 * $clippedFraction, 3))%, bright=$([Math]::Round(100.0 * $brightFraction, 3))%)."
    }
    if ($haloBrightFraction -gt 0.05) {
        throw "SSGI reference image has a bright subject-edge halo (halo=$([Math]::Round(100.0 * $haloBrightFraction, 3))%)."
    }
}
finally {
    $bitmap.Dispose()
}

$summary = @(
    "SSGI reference visual evidence validation: passed",
    "Reference view: $($record.Groups['view'].Value)",
    "Nine deterministic subjects: settled composition and bounded Rendered path",
    "Bounded image guard: clipped=$([Math]::Round(100.0 * $clippedFraction, 3))% bright=$([Math]::Round(100.0 * $brightFraction, 3))% subject_edge_halo=$([Math]::Round(100.0 * $haloBrightFraction, 3))%",
    "Status: automated SSGI activation and image-stability guard passed; human visual inspection remains required"
)
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "ssgi-reference-visual-validation.txt")
$summary
