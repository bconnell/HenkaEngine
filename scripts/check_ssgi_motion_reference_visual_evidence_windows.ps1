param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "SSGI motion reference evidence directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory "INDEX.txt"
$stdoutPath = Join-Path $InputDirectory "ssgi_motion_reference.stdout.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf) -or
    -not (Test-Path -LiteralPath $stdoutPath -PathType Leaf)) {
    throw "SSGI motion reference evidence is missing its index or stdout record."
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
$stdoutText = Get-Content -LiteralPath $stdoutPath -Raw
if ($indexText -notmatch "(?m)^Evidence profile: SSGI_MOTION_REFERENCE\s*$" -or
    $stdoutText -notmatch "Realism reference capture: debug grid hidden\.") {
    throw "SSGI motion reference evidence does not prove the dedicated background-safe capture profile."
}

$pattern = "(?m)CAPTURE_READY_SSGI_MOTION_REFERENCE phase=(?<phase>before|after) mode=rendered view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) reference_texture_edge=(?<texture_edge>\d+) reference_exposure_stops=(?<exposure>[-0-9.]+) reference_ssgi_active=1 viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>\d+) aspect=(?<aspect>[-0-9.]+) camera_position=(?<px>[-0-9.]+),(?<py>[-0-9.]+),(?<pz>[-0-9.]+) yaw=(?<yaw>[-0-9.]+) pitch=(?<pitch>[-0-9.]+) roll=(?<roll>[-0-9.]+) fov=(?<fov>[-0-9.]+) reference_bounds=(?<cx>[-0-9.]+),(?<cy>[-0-9.]+),(?<cz>[-0-9.]+),(?<ex>[-0-9.]+),(?<ey>[-0-9.]+),(?<ez>[-0-9.]+) reference_midpoint=(?<mx>[-0-9.]+),(?<my>[-0-9.]+) reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) draw_expected=1"
$pattern = $pattern -replace 'reference_ssgi_active=1 ', 'reference_ssgi_active=1 reference_probe_diffuse_active=1 reference_probe_prefilter_active=1 reference_probe_blend_active=1 temporal_history_ready=(?<history_ready>[01]) temporal_history_valid=(?<history_valid>[01]) temporal_fallback_active=(?<fallback_active>[01]) temporal_resolve_count=(?<resolve_count>\d+) temporal_invalidation_count=(?<invalidation_count>\d+) temporal_fallback_frame_count=(?<fallback_frame_count>\d+) temporal_history_allocation_failures=(?<allocation_failures>\d+) motion_vectors_ready=(?<motion_vectors_ready>[01]) temporal_jitter_enabled=(?<jitter_enabled>[01]) reference_probe_enabled_count=\d+ reference_probe_captured_count=\d+ reference_probe_capture_generation=\d+ reference_probe_capture_failures=0 '
$matches = [regex]::Matches($indexText, $pattern)
if ($matches.Count -ne 2) {
    throw "SSGI motion reference evidence must contain exactly one before and one after readiness record."
}
$before = $matches | Where-Object { $_.Groups["phase"].Value -eq "before" }
$after = $matches | Where-Object { $_.Groups["phase"].Value -eq "after" }
if ($null -eq $before -or $null -eq $after) {
    throw "SSGI motion reference evidence must contain both motion phases."
}
$expectedView = $before.Groups["view"].Value
$expectedLayout = if ($expectedView -eq "close") { "close_grid" } else { "wide_row" }
foreach ($record in @($before, $after)) {
    $probeHealth = [regex]::Match($record.Value, 'reference_probe_enabled_count=(?<enabled>\d+) reference_probe_captured_count=(?<captured>\d+) reference_probe_capture_generation=(?<generation>\d+) reference_probe_capture_failures=(?<failures>\d+)')
    if (-not $probeHealth.Success -or
        [int]$probeHealth.Groups['enabled'].Value -lt 2 -or
        [int]$probeHealth.Groups['captured'].Value -lt 2 -or
        [uint64]$probeHealth.Groups['generation'].Value -eq 0 -or
        [int]$probeHealth.Groups['failures'].Value -ne 0) {
        throw 'SSGI motion metadata did not prove two current captured probes without failures for both phases.'
    }
    if ([int]$record.Groups['history_ready'].Value -ne 1 -or
        [int]$record.Groups['allocation_failures'].Value -ne 0 -or
        ($record.Groups['phase'].Value -eq 'after' -and
            ([int]$record.Groups['history_valid'].Value -ne 1 -or
             [int]$record.Groups['fallback_active'].Value -ne 0 -or
             [uint64]$record.Groups['resolve_count'].Value -eq 0 -or
             [int]$record.Groups['motion_vectors_ready'].Value -ne 1 -or
             [int]$record.Groups['jitter_enabled'].Value -ne 1))) {
        throw 'SSGI motion metadata did not prove a healthy temporal history and resolved moved phase.'
    }
    if ($record.Groups["view"].Value -ne $expectedView -or
        $record.Groups["layout"].Value -ne $expectedLayout -or
        [int]$record.Groups["texture_edge"].Value -lt 32 -or
        [int]$record.Groups["count"].Value -ne 9 -or
        [int]$record.Groups["settled"].Value -lt $(if ($record.Groups["phase"].Value -eq "after") { 1 } else { 3 })) {
        throw "SSGI motion reference metadata did not prove two stable nine-subject phases."
    }
}
$cameraDeltaX = [Math]::Abs([double]$after.Groups["px"].Value - [double]$before.Groups["px"].Value)
$cameraDeltaZ = [Math]::Abs([double]$after.Groups["pz"].Value - [double]$before.Groups["pz"].Value)
if ($cameraDeltaX -lt 0.30 -or $cameraDeltaX -gt 0.40 -or
    $cameraDeltaZ -lt 0.15 -or $cameraDeltaZ -gt 0.25) {
    throw "SSGI motion reference did not prove the bounded deterministic camera translation."
}

Add-Type -AssemblyName System.Drawing
function Get-SsgiMotionImageStats {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][double[]]$CenterXs,
        [Parameter(Mandatory = $true)][double[]]$CenterYs,
        [Parameter(Mandatory = $true)][double]$SubjectRadius
    )

    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        if ($bitmap.Width -lt 160 -or $bitmap.Height -lt 120) {
            throw "SSGI motion reference image has invalid dimensions ($($bitmap.Width)x$($bitmap.Height))."
        }
        [double]$sum = 0.0
        [double]$sumSquares = 0.0
        [int]$sampleCount = 0
        [int]$clipped = 0
        [int]$bright = 0
        $step = [Math]::Max(1, [int][Math]::Floor($bitmap.Width / 160.0))
        for ($y = 0; $y -lt $bitmap.Height; $y += $step) {
            for ($x = 0; $x -lt $bitmap.Width; $x += $step) {
                $pixel = $bitmap.GetPixel($x, $y)
                [double]$luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                $sum += $luma
                $sumSquares += $luma * $luma
                if ($luma -ge 250.0) { ++$clipped }
                if ($luma -ge 240.0) { ++$bright }
                ++$sampleCount
            }
        }
        $mean = $sum / $sampleCount
        $standardDeviation = [Math]::Sqrt([Math]::Max(0.0, ($sumSquares / $sampleCount) - ($mean * $mean)))
        [double]$evaluatedSum = 0.0
        [double]$evaluatedSumSquares = 0.0
        [int]$evaluatedSampleCount = 0
        [int]$evaluatedClipped = 0
        [int]$evaluatedBright = 0
        [int]$visibleSubjectCount = 0
        foreach ($centerY in $CenterYs) {
            foreach ($centerX in $CenterXs) {
                $centerPixelX = $bitmap.Width * $centerX
                $centerPixelY = $bitmap.Height * $centerY
                $minimumX = [Math]::Max(0, [int][Math]::Floor($centerPixelX - $SubjectRadius))
                $maximumX = [Math]::Min($bitmap.Width - 1, [int][Math]::Ceiling($centerPixelX + $SubjectRadius))
                $minimumY = [Math]::Max(0, [int][Math]::Floor($centerPixelY - $SubjectRadius))
                $maximumY = [Math]::Min($bitmap.Height - 1, [int][Math]::Ceiling($centerPixelY + $SubjectRadius))
                [double]$subjectSum = 0.0
                [int]$subjectCount = 0
                for ($subjectY = $minimumY; $subjectY -le $maximumY; $subjectY += $step) {
                    for ($subjectX = $minimumX; $subjectX -le $maximumX; $subjectX += $step) {
                        $dx = $subjectX - $centerPixelX
                        $dy = $subjectY - $centerPixelY
                        if (($dx * $dx) + ($dy * $dy) -gt ($SubjectRadius * $SubjectRadius)) {
                            continue
                        }
                        $subjectPixel = $bitmap.GetPixel($subjectX, $subjectY)
                        [double]$subjectLuma = 0.2126 * $subjectPixel.R + 0.7152 * $subjectPixel.G + 0.0722 * $subjectPixel.B
                        $evaluatedSum += $subjectLuma
                        $evaluatedSumSquares += $subjectLuma * $subjectLuma
                        ++$evaluatedSampleCount
                        $subjectSum += $subjectLuma
                        ++$subjectCount
                        if ($subjectLuma -ge 250.0) { ++$evaluatedClipped }
                        if ($subjectLuma -ge 240.0) { ++$evaluatedBright }
                    }
                }
                if ($subjectCount -gt 0 -and ($subjectSum / $subjectCount) -ge 8.0) {
                    ++$visibleSubjectCount
                }
            }
        }
        if ($evaluatedSampleCount -eq 0) {
            throw "SSGI motion reference did not produce evaluated subject samples."
        }
        $evaluatedMean = $evaluatedSum / $evaluatedSampleCount
        $evaluatedStandardDeviation = [Math]::Sqrt([Math]::Max(0.0, ($evaluatedSumSquares / $evaluatedSampleCount) - ($evaluatedMean * $evaluatedMean)))
        [pscustomobject]@{
            Width = $bitmap.Width
            Height = $bitmap.Height
            Mean = $mean
            StandardDeviation = $standardDeviation
            ClippedFraction = $clipped / [double]$sampleCount
            BrightFraction = $bright / [double]$sampleCount
            EvaluatedMean = $evaluatedMean
            EvaluatedStandardDeviation = $evaluatedStandardDeviation
            EvaluatedClippedFraction = $evaluatedClipped / [double]$evaluatedSampleCount
            EvaluatedBrightFraction = $evaluatedBright / [double]$evaluatedSampleCount
            VisibleSubjectCount = $visibleSubjectCount
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-SsgiMotionImageDifference {
    param(
        [Parameter(Mandatory = $true)][string]$BeforePath,
        [Parameter(Mandatory = $true)][string]$AfterPath
    )

    $beforeBitmap = [System.Drawing.Bitmap]::new($BeforePath)
    $afterBitmap = [System.Drawing.Bitmap]::new($AfterPath)
    try {
        if ($beforeBitmap.Width -ne $afterBitmap.Width -or
            $beforeBitmap.Height -ne $afterBitmap.Height) {
            throw "SSGI motion reference images changed dimensions before pixel comparison."
        }
        [double]$sumAbsoluteLumaDifference = 0.0
        [int]$changedSampleCount = 0
        [int]$sampleCount = 0
        $step = [Math]::Max(1, [int][Math]::Floor($beforeBitmap.Width / 160.0))
        for ($y = 0; $y -lt $beforeBitmap.Height; $y += $step) {
            for ($x = 0; $x -lt $beforeBitmap.Width; $x += $step) {
                $beforePixel = $beforeBitmap.GetPixel($x, $y)
                $afterPixel = $afterBitmap.GetPixel($x, $y)
                [double]$beforeLuma = 0.2126 * $beforePixel.R + 0.7152 * $beforePixel.G + 0.0722 * $beforePixel.B
                [double]$afterLuma = 0.2126 * $afterPixel.R + 0.7152 * $afterPixel.G + 0.0722 * $afterPixel.B
                $difference = [Math]::Abs($afterLuma - $beforeLuma)
                $sumAbsoluteLumaDifference += $difference
                if ($difference -gt 0.5) { ++$changedSampleCount }
                ++$sampleCount
            }
        }
        [pscustomobject]@{
            MeanAbsoluteLumaDifference = $sumAbsoluteLumaDifference / $sampleCount
            ChangedFraction = $changedSampleCount / [double]$sampleCount
        }
    }
    finally {
        $beforeBitmap.Dispose()
        $afterBitmap.Dispose()
    }
}

$beforePath = Join-Path $InputDirectory ("ssgi-motion-reference-" + $expectedView + "-before.png")
$afterPath = Join-Path $InputDirectory ("ssgi-motion-reference-" + $expectedView + "-after.png")
$baseSubjectCentersX = if ($expectedView -eq "close") {
    @(0.3125, 0.5, 0.6875)
}
else {
    @(0.118, 0.229, 0.338, 0.445, 0.499, 0.555, 0.663, 0.771, 0.879)
}
$subjectCentersY = if ($expectedView -eq "close") {
    @(0.194, 0.5, 0.806)
}
else {
    @(0.5)
}
$midpointDeltaX = ([double]$after.Groups["mx"].Value - [double]$before.Groups["mx"].Value) / [double]$before.Groups["vw"].Value
$beforeSubjectCentersX = [double[]]$baseSubjectCentersX
$afterSubjectCentersX = [double[]]@($baseSubjectCentersX | ForEach-Object { $_ + $midpointDeltaX })
$subjectRadius = if ($expectedView -eq "close") {
    [double]$before.Groups["vw"].Value * 0.075
}
else {
    [double]$before.Groups["vw"].Value * 0.035
}
$beforeStats = Get-SsgiMotionImageStats -Path $beforePath -CenterXs $beforeSubjectCentersX -CenterYs $subjectCentersY -SubjectRadius $subjectRadius
$afterStats = Get-SsgiMotionImageStats -Path $afterPath -CenterXs $afterSubjectCentersX -CenterYs $subjectCentersY -SubjectRadius $subjectRadius
if ($beforeStats.Width -ne $afterStats.Width -or $beforeStats.Height -ne $afterStats.Height) {
    throw "SSGI motion reference phases changed capture dimensions."
}
foreach ($stats in @($beforeStats, $afterStats)) {
    if ($stats.StandardDeviation -lt 2.0 -or
        $stats.EvaluatedStandardDeviation -lt 2.0 -or
        $stats.EvaluatedClippedFraction -gt 0.20 -or
        $stats.EvaluatedBrightFraction -gt 0.30 -or
        $stats.VisibleSubjectCount -ne 9) {
        throw "SSGI motion reference contains a flat, clipped, over-bright, or illegible evaluated subject phase (global_std=$([Math]::Round($stats.StandardDeviation, 3)) evaluated_std=$([Math]::Round($stats.EvaluatedStandardDeviation, 3)) evaluated_clipped=$([Math]::Round($stats.EvaluatedClippedFraction, 3)) evaluated_bright=$([Math]::Round($stats.EvaluatedBrightFraction, 3)) visible=$($stats.VisibleSubjectCount)/9)."
    }
}
$meanDelta = [Math]::Abs($afterStats.Mean - $beforeStats.Mean)
$deviationDelta = [Math]::Abs($afterStats.StandardDeviation - $beforeStats.StandardDeviation)
if ($meanDelta -gt 35.0 -or $deviationDelta -gt 35.0) {
    throw "SSGI motion reference changed its bounded luminance distribution excessively."
}
$difference = Get-SsgiMotionImageDifference -BeforePath $beforePath -AfterPath $afterPath
if ($difference.MeanAbsoluteLumaDifference -le 0.5 -or $difference.ChangedFraction -le 0.01) {
    throw "SSGI motion reference before/after images are effectively identical; the camera change was not visually captured."
}

$summary = @(
    "SSGI motion reference visual evidence validation: passed",
    "Reference view: $expectedView",
    "Camera motion: deterministic in-process translation dx=$([Math]::Round($cameraDeltaX, 4)) dz=$([Math]::Round($cameraDeltaZ, 4))",
    "Before/after luminance distribution deltas: mean=$([Math]::Round($meanDelta, 3)) stddev=$([Math]::Round($deviationDelta, 3))",
    "Evaluated subjects: before=$($beforeStats.VisibleSubjectCount)/9 after=$($afterStats.VisibleSubjectCount)/9 subject_bright_fraction_before=$([Math]::Round($beforeStats.EvaluatedBrightFraction, 3)) subject_bright_fraction_after=$([Math]::Round($afterStats.EvaluatedBrightFraction, 3))",
    "Before/after sampled pixel difference: mean_absolute_luma=$([Math]::Round($difference.MeanAbsoluteLumaDifference, 3)) changed_fraction=$([Math]::Round($difference.ChangedFraction, 3))",
    "Temporal status: the static baseline may begin in fallback; settled and moved perspective phases report explicit history, motion-vector, jitter, and resolve state",
    "Status: automated SSGI camera-motion stability guard passed; human visual inspection remains required"
)
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "ssgi-motion-reference-visual-validation.txt")
$summary
