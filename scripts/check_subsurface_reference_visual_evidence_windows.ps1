param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "Subsurface reference evidence directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory "INDEX.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf)) {
    throw "Subsurface reference evidence is missing its capture index."
}
$indexText = Get-Content -LiteralPath $indexPath -Raw
if ($indexText -notmatch "(?m)^Evidence profile: SUBSURFACE_REFERENCE\s*$") {
    throw "Subsurface reference evidence does not declare the SUBSURFACE_REFERENCE profile."
}

foreach ($stderrPath in Get-ChildItem -LiteralPath $InputDirectory -Filter "sss_reference_*.stderr.txt" -File) {
    $stderrText = Get-Content -LiteralPath $stderrPath.FullName -Raw
    if ($stderrText -match "Realism reference capture could not frame its bounded fixture\.") {
        throw "Subsurface reference capture reported a framing failure in $($stderrPath.Name)."
    }
}
foreach ($stdoutPath in Get-ChildItem -LiteralPath $InputDirectory -Filter "sss_reference_*.stdout.txt" -File) {
    $stdoutText = Get-Content -LiteralPath $stdoutPath.FullName -Raw
    if ($stdoutText -notmatch "Realism reference capture: debug grid hidden\.") {
        throw "Subsurface reference capture did not prove that the debug grid was hidden in $($stdoutPath.Name)."
    }
}

$pattern = "(?m)CAPTURE_READY_SSS_REFERENCE mode=rendered view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) reference_texture_edge=(?<texture_edge>\d+) reference_exposure_stops=(?<exposure>[-0-9.]+) reference_sss_variant=(?<variant>opaque|thin|thick) viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>-?\d+) aspect=(?<aspect>[-0-9.]+) camera_position=(?<px>[-0-9.]+),(?<py>[-0-9.]+),(?<pz>[-0-9.]+) yaw=(?<yaw>[-0-9.]+) pitch=(?<pitch>[-0-9.]+) roll=(?<roll>[-0-9.]+) fov=(?<fov>[-0-9.]+) reference_bounds=(?<cx>[-0-9.]+),(?<cy>[-0-9.]+),(?<cz>[-0-9.]+),(?<ex>[-0-9.]+),(?<ey>[-0-9.]+),(?<ez>[-0-9.]+) reference_midpoint=(?<mx>[-0-9.]+),(?<my>[-0-9.]+) reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) draw_expected=1"
$records = [regex]::Matches($indexText, $pattern)
if ($records.Count -ne 3) {
    throw "Subsurface reference evidence must contain exactly three rendered readiness records."
}

$view = $records[0].Groups["view"].Value
$expectedLayout = if ($view -eq "close") { "close_grid" } else { "wide_row" }
$canonical = $null
$variants = @{}
foreach ($record in $records) {
    $variant = $record.Groups["variant"].Value
    if ($variants.ContainsKey($variant) -or
        $record.Groups["view"].Value -ne $view -or
        $record.Groups["layout"].Value -ne $expectedLayout -or
        [int]$record.Groups["texture_edge"].Value -lt 32 -or
        [int]$record.Groups["count"].Value -ne 9 -or
        [int]$record.Groups["settled"].Value -lt 3) {
        throw "Subsurface reference metadata diverged, duplicated a variant, or did not prove nine settled subjects."
    }
    $variants[$variant] = $true
    $entryCanonical = $record.Value -replace 'reference_sss_variant=[^ ]+', 'reference_sss_variant=varied'
    if ($null -eq $canonical) {
        $canonical = $entryCanonical
    }
    elseif ($entryCanonical -ne $canonical) {
        throw "Subsurface reference composition metadata diverged between material variants."
    }
}
foreach ($requiredVariant in @("opaque", "thin", "thick")) {
    if (-not $variants.ContainsKey($requiredVariant)) {
        throw "Subsurface reference evidence is missing the $requiredVariant variant."
    }
}

Add-Type -AssemblyName System.Drawing

function Get-ImageMetrics {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Subsurface reference evidence is missing: $Path"
    }
    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        if ($bitmap.Width -lt 160 -or $bitmap.Height -lt 120) {
            throw "Subsurface reference evidence has invalid dimensions ($($bitmap.Width)x$($bitmap.Height))."
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
        return [pscustomobject]@{
            Mean = $mean
            StandardDeviation = [Math]::Sqrt($variance)
        }
    }
    finally { $bitmap.Dispose() }
}

function Get-MeanSubjectColor {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][double]$CenterX,
        [Parameter(Mandatory = $true)][double]$CenterY,
        [int]$Radius = 42
    )
    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        $centerPixelX = [int][Math]::Round($bitmap.Width * $CenterX)
        $centerPixelY = [int][Math]::Round($bitmap.Height * $CenterY)
        [double]$red = 0.0
        [double]$green = 0.0
        [double]$blue = 0.0
        [int]$count = 0
        for ($y = $centerPixelY - $Radius; $y -le $centerPixelY + $Radius; $y += 2) {
            for ($x = $centerPixelX - $Radius; $x -le $centerPixelX + $Radius; $x += 2) {
                $dx = $x - $centerPixelX
                $dy = $y - $centerPixelY
                if (($dx * $dx) + ($dy * $dy) -gt ($Radius * $Radius)) { continue }
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
            Luma = (0.2126 * $red + 0.7152 * $green + 0.0722 * $blue) / $count
        }
    }
    finally { $bitmap.Dispose() }
}

function Get-MeanImageDifference {
    param(
        [Parameter(Mandatory = $true)][string]$LeftPath,
        [Parameter(Mandatory = $true)][string]$RightPath
    )
    $left = [System.Drawing.Bitmap]::new($LeftPath)
    $right = [System.Drawing.Bitmap]::new($RightPath)
    try {
        if ($left.Width -ne $right.Width -or $left.Height -ne $right.Height) {
            throw "Subsurface reference variants have different image dimensions."
        }
        [double]$difference = 0.0
        [int]$count = 0
        $step = [Math]::Max(1, [int][Math]::Floor($left.Width / 160.0))
        for ($y = 0; $y -lt $left.Height; $y += $step) {
            for ($x = 0; $x -lt $left.Width; $x += $step) {
                $a = $left.GetPixel($x, $y)
                $b = $right.GetPixel($x, $y)
                $difference += ([Math]::Abs($a.R - $b.R) + [Math]::Abs($a.G - $b.G) + [Math]::Abs($a.B - $b.B)) / 3.0
                ++$count
            }
        }
        return $difference / $count
    }
    finally {
        $left.Dispose()
        $right.Dispose()
    }
}

$fileBase = "sss-reference-$view"
$files = @{
    opaque = Join-Path $InputDirectory "$fileBase-opaque.png"
    thin = Join-Path $InputDirectory "$fileBase-thin.png"
    thick = Join-Path $InputDirectory "$fileBase-thick.png"
}
$opaqueMetrics = Get-ImageMetrics $files.opaque
$thinMetrics = Get-ImageMetrics $files.thin
$thickMetrics = Get-ImageMetrics $files.thick
if ($opaqueMetrics.StandardDeviation -lt 2.0 -or $thinMetrics.StandardDeviation -lt 2.0 -or $thickMetrics.StandardDeviation -lt 2.0) {
    throw "Subsurface reference captures are too flat to prove a stable rendered image."
}

$centerX = if ($view -eq "close") { 0.50 } else { 0.50 }
$centerY = if ($view -eq "close") { 0.50 } else { 0.52 }
$opaqueSubject = Get-MeanSubjectColor $files.opaque $centerX $centerY
$thinSubject = Get-MeanSubjectColor $files.thin $centerX $centerY
$thickSubject = Get-MeanSubjectColor $files.thick $centerX $centerY
$opaqueThinDifference = Get-MeanImageDifference $files.opaque $files.thin
$opaqueThickDifference = Get-MeanImageDifference $files.opaque $files.thick
$subjectLumaRange = [Math]::Max(
    [Math]::Abs($opaqueSubject.Luma - $thinSubject.Luma),
    [Math]::Abs($opaqueSubject.Luma - $thickSubject.Luma))
$subjectWarmthRange = [Math]::Max(
    [Math]::Abs(($opaqueSubject.Red - $opaqueSubject.Green) - ($thinSubject.Red - $thinSubject.Green)),
    [Math]::Abs(($opaqueSubject.Red - $opaqueSubject.Green) - ($thickSubject.Red - $thickSubject.Green)))
if ($opaqueThinDifference -lt 1.0 -or $opaqueThickDifference -lt 1.0 -or
    $subjectLumaRange -lt 2.0 -or $subjectWarmthRange -lt 2.0) {
    throw "Subsurface variants did not produce a measurable bounded response (image differences opaque/thin=$([Math]::Round($opaqueThinDifference, 2)) opaque/thick=$([Math]::Round($opaqueThickDifference, 2)) subject luma range=$([Math]::Round($subjectLumaRange, 2)) warmth range=$([Math]::Round($subjectWarmthRange, 2)))."
}

$summary = @(
    "Subsurface reference visual evidence validation: passed",
    "Reference view: $view",
    "Nine deterministic subjects: same camera, layout, and settled geometry across opaque/thin/thick captures",
    "Center subject luma opaque/thin/thick: $([Math]::Round($opaqueSubject.Luma, 2)) / $([Math]::Round($thinSubject.Luma, 2)) / $([Math]::Round($thickSubject.Luma, 2))",
    "Mean image difference opaque/thin and opaque/thick: $([Math]::Round($opaqueThinDifference, 2)) / $([Math]::Round($opaqueThickDifference, 2))",
    "Status: automated bounded subsurface response guard passed; human visual inspection remains required"
)
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "subsurface-reference-visual-validation.txt")
$summary
