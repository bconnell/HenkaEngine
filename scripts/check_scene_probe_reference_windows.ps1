param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "Scene-probe reference evidence directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory 'INDEX.txt'
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    throw 'Scene-probe reference evidence is missing its capture index.'
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
if ($indexText -notmatch '(?m)^Evidence profile: SCENE_PROBE_REFERENCE\s*$') {
    throw 'Scene-probe reference evidence does not declare its dedicated profile.'
}

$stdoutPaths = Get-ChildItem -LiteralPath $InputDirectory -Filter 'scene_probe_reference_*.stdout.txt' -File
foreach ($stdoutPath in $stdoutPaths) {
    $stdoutText = Get-Content -LiteralPath $stdoutPath.FullName -Raw
    if ($stdoutText -notmatch 'Realism reference capture: debug grid hidden\.') {
        throw "Scene-probe reference did not prove that the debug grid was hidden in $($stdoutPath.Name)."
    }
}

$pattern = '(?m)CAPTURE_READY_SCENE_PROBE_REFERENCE mode=rendered view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) reference_texture_edge=(?<texture_edge>\d+) reference_exposure_stops=(?<exposure>[-0-9.]+) probe_reference=1 probe_diffuse_active=1 probe_prefilter_active=1 probe_blend_active=1 screen_space_reflections_active=1 probe_enabled_count=(?<enabled>\d+) probe_captured_count=(?<captured>\d+) probe_capture_generation=(?<generation>\d+) probe_capture_failures=(?<failures>\d+) viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>\d+) .*reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) draw_expected=1'
$metadata = @([regex]::Matches($indexText, $pattern))
if ($metadata.Count -ne 1) {
    throw 'Scene-probe reference evidence must contain exactly one rendered readiness record.'
}

$entry = $metadata[0]
$view = $entry.Groups['view'].Value
$expectedLayout = if ($view -eq 'close') { 'close_grid' } else { 'wide_row' }
if ($entry.Groups['layout'].Value -ne $expectedLayout -or
    [int]$entry.Groups['texture_edge'].Value -lt 32 -or
    [int]$entry.Groups['enabled'].Value -lt 2 -or
    [int]$entry.Groups['captured'].Value -lt 2 -or
    [uint64]$entry.Groups['generation'].Value -eq 0 -or
    [int]$entry.Groups['failures'].Value -ne 0 -or
    [int]$entry.Groups['count'].Value -ne 9 -or
    [int]$entry.Groups['settled'].Value -lt 3) {
    throw 'Scene-probe reference metadata did not prove two healthy captured probes and nine settled subjects.'
}

$renderedPath = Join-Path $InputDirectory "scene-probe-reference-$view-rendered.png"
if (-not (Test-Path -LiteralPath $renderedPath -PathType Leaf)) {
    throw "Scene-probe Rendered evidence is missing: $renderedPath"
}

Add-Type -AssemblyName System.Drawing
$bitmap = [System.Drawing.Bitmap]::new($renderedPath)
try {
    if ($bitmap.Width -lt 160 -or $bitmap.Height -lt 120) {
        throw "Scene-probe Rendered evidence has invalid dimensions ($($bitmap.Width)x$($bitmap.Height))."
    }
    [double]$sum = 0.0
    [double]$sumSquares = 0.0
    [int]$clipped = 0
    [int]$count = 0
    [int]$visibleSubjectCount = 0
    $step = [Math]::Max(1, [int][Math]::Floor($bitmap.Width / 160.0))
    for ($y = 0; $y -lt $bitmap.Height; $y += $step) {
        for ($x = 0; $x -lt $bitmap.Width; $x += $step) {
            $pixel = $bitmap.GetPixel($x, $y)
            [double]$luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
            $sum += $luma
            $sumSquares += $luma * $luma
            if ([Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B)) -ge 254) { ++$clipped }
            ++$count
        }
    }
    $subjectCentersX = @(0.3125, 0.5, 0.6875)
    $subjectCentersY = @(0.23, 0.54, 0.85)
    foreach ($centerY in $subjectCentersY) {
        foreach ($centerX in $subjectCentersX) {
            [double]$subjectSum = 0.0
            [int]$subjectCount = 0
            $centerPixelX = [int][Math]::Round($bitmap.Width * $centerX)
            $centerPixelY = [int][Math]::Round($bitmap.Height * $centerY)
            $minimumX = [Math]::Max(0, $centerPixelX - 24)
            $maximumX = [Math]::Min($bitmap.Width - 1, $centerPixelX + 24)
            $minimumY = [Math]::Max(0, $centerPixelY - 24)
            $maximumY = [Math]::Min($bitmap.Height - 1, $centerPixelY + 24)
            for ($y = $minimumY; $y -le $maximumY; ++$y) {
                for ($x = $minimumX; $x -le $maximumX; ++$x) {
                    $pixel = $bitmap.GetPixel($x, $y)
                    $subjectSum += 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                    ++$subjectCount
                }
            }
            if ($subjectCount -gt 0 -and ($subjectSum / $subjectCount) -ge 8.0) {
                ++$visibleSubjectCount
            }
        }
    }
    if ($visibleSubjectCount -ne 9) {
        throw "Scene-probe Rendered evidence did not keep all nine evaluated subjects visually legible (visible=$visibleSubjectCount/9)."
    }
    [double]$mean = $sum / $count
    [double]$standardDeviation = [Math]::Sqrt([Math]::Max(0.0, ($sumSquares / $count) - ($mean * $mean)))
    [double]$clippedFraction = $clipped / [double]$count
    if ($standardDeviation -lt 10.0 -or $mean -lt 8.0 -or $mean -gt 238.0 -or $clippedFraction -gt 0.12) {
        throw "Scene-probe Rendered evidence is empty, too flat, or excessively clipped (mean=$([Math]::Round($mean, 2)), standard-deviation=$([Math]::Round($standardDeviation, 2)), clipped=$([Math]::Round($clippedFraction, 4)))."
    }
}
finally {
    $bitmap.Dispose()
}

Write-Output "Scene-probe reference validation: passed (view=$view, enabled=$($entry.Groups['enabled'].Value), captured=$($entry.Groups['captured'].Value), generation=$($entry.Groups['generation'].Value), visible-subjects=$visibleSubjectCount/9, rendered-mean=$([Math]::Round($mean, 2)), rendered-sd=$([Math]::Round($standardDeviation, 2)), clipped-fraction=$([Math]::Round($clippedFraction, 4)))."
