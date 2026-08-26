param(
    [Parameter(Mandatory = $true)]
    [string]$InputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $InputDirectory -PathType Container)) {
    throw "SSGI performance reference evidence directory was not found: $InputDirectory"
}

$indexPath = Join-Path $InputDirectory "INDEX.txt"
$stdoutPath = Join-Path $InputDirectory "ssgi_performance_reference.stdout.txt"
if (-not (Test-Path -LiteralPath $indexPath -PathType Leaf) -or
    -not (Test-Path -LiteralPath $stdoutPath -PathType Leaf)) {
    throw "SSGI performance reference evidence is missing its index or stdout record."
}

$indexText = Get-Content -LiteralPath $indexPath -Raw
$stdoutText = Get-Content -LiteralPath $stdoutPath -Raw
if ($indexText -notmatch "(?m)^Evidence profile: SSGI_PERFORMANCE_REFERENCE\s*$" -or
    $stdoutText -notmatch "Realism reference capture: debug grid hidden\.") {
    throw "SSGI performance reference evidence does not prove the dedicated reference capture profile."
}
if ($stdoutText -notmatch "(?m)CAPTURE_READY_SSGI_PERFORMANCE_REFERENCE .* reference_probe_prefilter_active=1 ") {
    throw "SSGI performance reference did not prove that local reflection-probe prefiltering was active."
}

$pattern = "(?m)CAPTURE_READY_SSGI_PERFORMANCE_REFERENCE mode=rendered view=(?<view>wide|close) reference_layout=(?<layout>[a-z_]+) reference_texture_edge=(?<texture_edge>\d+) reference_exposure_stops=(?<exposure>[-0-9.]+) reference_ssgi_active=1 reference_probe_diffuse_active=1 viewport=(?<vx>-?\d+),(?<vy>-?\d+),(?<vw>\d+),(?<vh>\d+) aspect=(?<aspect>[-0-9.]+) reference_count=(?<count>\d+) settled_frames=(?<settled>\d+) samples=(?<samples>\d+) gpu_samples=(?<gpu_samples>\d+) frame_mean_ms=(?<frame_mean>[-0-9.]+) frame_max_ms=(?<frame_max>[-0-9.]+) scene_cpu_mean_ms=(?<cpu_mean>[-0-9.]+) scene_cpu_max_ms=(?<cpu_max>[-0-9.]+) scene_gpu_mean_ms=(?<gpu_mean>[-0-9.]+) scene_gpu_max_ms=(?<gpu_max>[-0-9.]+) scene_gpu_timing=(?<timing>available|unavailable) draw_expected=1"
$pattern = $pattern -replace 'reference_probe_diffuse_active=1 ', 'reference_probe_diffuse_active=1 reference_probe_prefilter_active=1 '
$matches = [regex]::Matches($stdoutText, $pattern)
if ($matches.Count -ne 1) {
    throw "SSGI performance reference evidence must contain exactly one readiness record."
}
$record = $matches[0]
$expectedView = $record.Groups["view"].Value
$layout = $record.Groups["layout"].Value
$textureEdge = [int]$record.Groups["texture_edge"].Value
$width = [int]$record.Groups["vw"].Value
$height = [int]$record.Groups["vh"].Value
$count = [int]$record.Groups["count"].Value
$settled = [int]$record.Groups["settled"].Value
$samples = [int]$record.Groups["samples"].Value
$gpuSamples = [int]$record.Groups["gpu_samples"].Value
$frameMean = [double]$record.Groups["frame_mean"].Value
$frameMax = [double]$record.Groups["frame_max"].Value
$cpuMean = [double]$record.Groups["cpu_mean"].Value
$cpuMax = [double]$record.Groups["cpu_max"].Value
$gpuMean = [double]$record.Groups["gpu_mean"].Value
$gpuMax = [double]$record.Groups["gpu_max"].Value
$timing = $record.Groups["timing"].Value

if ($expectedView -notin @("wide", "close") -or $layout -notin @("wide_row", "close_grid") -or
    $textureEdge -lt 32 -or $width -ne 1280 -or $height -ne 720 -or $count -ne 9 -or $settled -lt 3) {
    throw "SSGI performance reference metadata did not prove the fixed, settled nine-subject 1280x720 reference."
}
if ($samples -ne 32 -or $gpuSamples -lt 8 -or $timing -ne "available") {
    throw "SSGI performance reference did not produce the required bounded GPU timing sample set."
}
foreach ($value in @($frameMean, $frameMax, $cpuMean, $cpuMax, $gpuMean, $gpuMax)) {
    if (-not [double]::IsFinite($value) -or $value -lt 0.0) {
        throw "SSGI performance reference reported a non-finite or negative timing value."
    }
}
if ($frameMax -lt $frameMean -or $cpuMax -lt $cpuMean -or $gpuMax -lt $gpuMean) {
    throw "SSGI performance reference timing maxima were lower than their means."
}
# This is a gross-regression guard for the supported 1280x720 OpenGL reference,
# not a universal frame-rate promise or an isolated SSGI shader measurement.
if ($gpuMax -gt 100.0) {
    throw "SSGI performance reference exceeded the bounded 100ms GPU budget (max=$gpuMax ms)."
}

$summary = @(
    "SSGI performance reference validation: passed",
    "Reference view: $expectedView",
    "Reference timing samples: $samples frames, $gpuSamples GPU samples",
    "Scene timing: GPU mean=$([Math]::Round($gpuMean, 3))ms max=$([Math]::Round($gpuMax, 3))ms; CPU mean=$([Math]::Round($cpuMean, 3))ms max=$([Math]::Round($cpuMax, 3))ms",
    "Status: automated bounded Rendered SSGI performance guard passed; this is not a universal frame-rate or isolated-shader-cost claim"
)
$summary | Write-Output
$summary | Set-Content -LiteralPath (Join-Path $InputDirectory "ssgi-performance-reference-validation.txt")
