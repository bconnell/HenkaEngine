param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "HDR range evidence directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory "INDEX.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    throw "HDR range evidence is missing its capture index."
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
if ($indexText -notmatch "(?m)^Evidence profile: HDR_RANGE_REFERENCE\s*$") {
    throw "HDR range evidence does not declare the HDR_RANGE_REFERENCE profile."
}

foreach ($stderrPath in Get-ChildItem -LiteralPath $InputDirectory -Filter "hdr_reference_*.stderr.txt" -File) {
    $stderrText = Get-Content -LiteralPath $stderrPath.FullName -Raw
    if ($stderrText -match "Realism reference capture could not frame its bounded fixture\.") {
        throw "HDR range capture reported a framing failure in $($stderrPath.Name)."
    }
}
foreach ($stdoutPath in Get-ChildItem -LiteralPath $InputDirectory -Filter "hdr_reference_*.stdout.txt" -File) {
    $stdoutText = Get-Content -LiteralPath $stdoutPath.FullName -Raw
    if ($stdoutText -notmatch "Realism reference capture: debug grid hidden\.") {
        throw "HDR range capture did not prove that the debug grid was hidden in $($stdoutPath.Name)."
    }
}

# The three captures use the same executable mode, so each file records a
# separate readiness line. Require the exact bounded exposure set and reject
# duplicate or missing settings.
$allHdrLines = [regex]::Matches(
    $indexText,
    "(?m)CAPTURE_READY_HDR_REFERENCE mode=rendered view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) reference_texture_edge=(?<texture_edge>\d+) reference_exposure_stops=(?<exposure>[-0-9.]+) viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>-?\d+) aspect=(?<aspect>[-0-9.]+) camera_position=(?<px>[-0-9.]+),(?<py>[-0-9.]+),(?<pz>[-0-9.]+) yaw=(?<yaw>[-0-9.]+) pitch=(?<pitch>[-0-9.]+) roll=(?<roll>[-0-9.]+) fov=(?<fov>[-0-9.]+) reference_bounds=(?<cx>[-0-9.]+),(?<cy>[-0-9.]+),(?<cz>[-0-9.]+),(?<ex>[-0-9.]+),(?<ey>[-0-9.]+),(?<ez>[-0-9.]+) reference_midpoint=(?<mx>[-0-9.]+),(?<my>[-0-9.]+) reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) draw_expected=1")
if ($allHdrLines.Count -ne 3) {
    throw "HDR range evidence must contain exactly three rendered readiness records."
}

$view = $allHdrLines[0].Groups["view"].Value
$expectedLayout = if ($view -eq "close") { "close_grid" } else { "wide_row" }
$canonical = $null
$seen = @{}
foreach ($entry in $allHdrLines) {
    $entryExposure = [double]::Parse($entry.Groups["exposure"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $key = '{0:0.0}' -f $entryExposure
    if ($key -in $seen.Keys -or $entry.Groups["view"].Value -ne $view -or
        $entry.Groups["layout"].Value -ne $expectedLayout -or
        [int]$entry.Groups["texture_edge"].Value -lt 32 -or
        [int]$entry.Groups["count"].Value -ne 9 -or
        [int]$entry.Groups["settled"].Value -lt 3) {
        throw "HDR range metadata diverged, duplicated an exposure, or did not prove nine settled subjects."
    }
    $seen[$key] = $true
    $entryCanonical = $entry.Value -replace 'reference_exposure_stops=[^ ]+', 'reference_exposure_stops=varied'
    if ($null -eq $canonical) {
        $canonical = $entryCanonical
    }
    elseif ($entryCanonical -ne $canonical) {
        throw "HDR range composition metadata diverged between exposure captures."
    }
}
foreach ($requiredExposure in @('-2.0', '0.0', '2.0')) {
    if (-not $seen.ContainsKey($requiredExposure)) {
        throw "HDR range evidence is missing exposure $requiredExposure."
    }
}

Add-Type -AssemblyName System.Drawing

function Get-ImageMetrics {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "HDR range evidence is missing: $Path"
    }
    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        if ($bitmap.Width -lt 160 -or $bitmap.Height -lt 120) {
            throw "HDR range evidence has invalid dimensions ($($bitmap.Width)x$($bitmap.Height))."
        }
        [double]$sum = 0.0
        [double]$sumSquares = 0.0
        [int]$clipped = 0
        [int]$count = 0
        $step = [Math]::Max(1, [int][Math]::Floor($bitmap.Width / 160.0))
        for ($y = 0; $y -lt $bitmap.Height; $y += $step) {
            for ($x = 0; $x -lt $bitmap.Width; $x += $step) {
                $pixel = $bitmap.GetPixel($x, $y)
                [double]$luma = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                $sum += $luma
                $sumSquares += $luma * $luma
                if ($pixel.R -ge 252 -and $pixel.G -ge 252 -and $pixel.B -ge 252) {
                    ++$clipped
                }
                ++$count
            }
        }
        $mean = $sum / $count
        $variance = [Math]::Max(0.0, ($sumSquares / $count) - ($mean * $mean))
        return [pscustomobject]@{
            Mean = $mean
            StandardDeviation = [Math]::Sqrt($variance)
            ClippedFraction = $clipped / [double]$count
        }
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
        for ($y = $centerPixelY - $Radius; $y -le $centerPixelY + $Radius; $y += 2) {
            for ($x = $centerPixelX - $Radius; $x -le $centerPixelX + $Radius; $x += 2) {
                $dx = $x - $centerPixelX
                $dy = $y - $centerPixelY
                if (($dx * $dx) + ($dy * $dy) -gt ($Radius * $Radius)) { continue }
                $pixel = $bitmap.GetPixel($x, $y)
                $sum += 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
                ++$count
            }
        }
        return $sum / $count
    }
    finally { $bitmap.Dispose() }
}

$fileBase = "hdr-reference-$view"
$files = @{
    minus = Join-Path $InputDirectory "$fileBase-minus2.png"
    base = Join-Path $InputDirectory "$fileBase-base.png"
    plus = Join-Path $InputDirectory "$fileBase-plus2.png"
}
$minusMetrics = Get-ImageMetrics $files.minus
$baseMetrics = Get-ImageMetrics $files.base
$plusMetrics = Get-ImageMetrics $files.plus
if ($minusMetrics.StandardDeviation -lt 2.0 -or $baseMetrics.StandardDeviation -lt 2.0 -or $plusMetrics.StandardDeviation -lt 2.0) {
    throw "HDR range captures are too flat to prove a stable image response."
}
if ($minusMetrics.Mean -ge ($baseMetrics.Mean - 3.0) -or $plusMetrics.Mean -le ($baseMetrics.Mean + 3.0)) {
    throw "HDR range exposure response is not monotonic (mean luma minus=$([Math]::Round($minusMetrics.Mean, 2)) base=$([Math]::Round($baseMetrics.Mean, 2)) plus=$([Math]::Round($plusMetrics.Mean, 2)))."
}
if ($plusMetrics.ClippedFraction -gt 0.75) {
    throw "HDR range +2 exposure capture is overwhelmingly clipped ($([Math]::Round($plusMetrics.ClippedFraction * 100.0, 1))%)."
}

$centerX = if ($view -eq "close") { 0.506 } else { 0.5 }
$centerY = if ($view -eq "close") { 0.5 } else { 0.52 }
$minusSubject = Get-MeanSubjectLuma $files.minus $centerX $centerY
$baseSubject = Get-MeanSubjectLuma $files.base $centerX $centerY
$plusSubject = Get-MeanSubjectLuma $files.plus $centerX $centerY
if ($minusSubject -ge ($baseSubject - 3.0) -or $plusSubject -le ($baseSubject + 3.0)) {
    throw "HDR range subject response is not monotonic (subject luma minus=$([Math]::Round($minusSubject, 2)) base=$([Math]::Round($baseSubject, 2)) plus=$([Math]::Round($plusSubject, 2)))."
}

$summary = @(
    "HDR range visual evidence validation: passed",
    "Reference view: $view",
    "Nine deterministic subjects: same camera, layout, and settled geometry across exposure captures",
    "Mean luma -2/0/+2: $([Math]::Round($minusMetrics.Mean, 2)) / $([Math]::Round($baseMetrics.Mean, 2)) / $([Math]::Round($plusMetrics.Mean, 2))",
    "Center subject luma -2/0/+2: $([Math]::Round($minusSubject, 2)) / $([Math]::Round($baseSubject, 2)) / $([Math]::Round($plusSubject, 2))",
    "+2 clipped fraction: $([Math]::Round($plusMetrics.ClippedFraction * 100.0, 1))%",
    "Status: automated exposure-range guard passed; human visual inspection remains required"
)
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "hdr-range-visual-validation.txt")
$summary
