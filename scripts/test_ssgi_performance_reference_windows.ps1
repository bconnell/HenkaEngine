Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$fixtureRoot = Join-Path $repoRoot (".ssgi-performance-validator-test-" + [Guid]::NewGuid().ToString("N"))
[System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null

try {
    @(
        "Evidence profile: SSGI_PERFORMANCE_REFERENCE",
        "SSGI performance reference: bounded Rendered timing at 1280x720"
    ) | Set-Content -LiteralPath (Join-Path $fixtureRoot "INDEX.txt")
    @(
        "Realism reference capture: debug grid hidden.",
        "CAPTURE_READY_SSGI_PERFORMANCE_REFERENCE mode=rendered view=close reference_layout=close_grid reference_texture_edge=32 reference_exposure_stops=0.0000 reference_ssgi_active=1 reference_probe_diffuse_active=1 viewport=0,0,1280,720 aspect=1.777778 reference_count=9 settled_frames=3 samples=32 gpu_samples=32 frame_mean_ms=18.000 frame_max_ms=25.000 scene_cpu_mean_ms=32.000 scene_cpu_max_ms=47.000 scene_gpu_mean_ms=2.800 scene_gpu_max_ms=4.100 scene_gpu_timing=available draw_expected=1"
    ) | Set-Content -LiteralPath (Join-Path $fixtureRoot "ssgi_performance_reference.stdout.txt")
    (Get-Content -LiteralPath (Join-Path $fixtureRoot "ssgi_performance_reference.stdout.txt") -Raw) -replace "reference_probe_diffuse_active=1 ", "reference_probe_diffuse_active=1 reference_probe_prefilter_active=1 reference_probe_blend_active=1 " |
        Set-Content -LiteralPath (Join-Path $fixtureRoot "ssgi_performance_reference.stdout.txt")
    & (Join-Path $PSScriptRoot "check_ssgi_performance_reference_windows.ps1") -InputDirectory $fixtureRoot | Out-Null

    (Get-Content -LiteralPath (Join-Path $fixtureRoot "ssgi_performance_reference.stdout.txt") -Raw) -replace "scene_gpu_timing=available", "scene_gpu_timing=unavailable" |
        Set-Content -LiteralPath (Join-Path $fixtureRoot "ssgi_performance_reference.stdout.txt")
    $rejectedUnavailable = $false
    try {
        & (Join-Path $PSScriptRoot "check_ssgi_performance_reference_windows.ps1") -InputDirectory $fixtureRoot | Out-Null
    }
    catch {
        $rejectedUnavailable = $_.Exception.Message -match "GPU timing sample set"
    }
    if (-not $rejectedUnavailable) {
        throw "The SSGI performance validator did not reject unavailable GPU timing."
    }

    (Get-Content -LiteralPath (Join-Path $fixtureRoot "ssgi_performance_reference.stdout.txt") -Raw) -replace "scene_gpu_timing=unavailable", "scene_gpu_timing=available" -replace "scene_gpu_max_ms=4\.100", "scene_gpu_max_ms=100.001" |
        Set-Content -LiteralPath (Join-Path $fixtureRoot "ssgi_performance_reference.stdout.txt")
    $rejectedBudget = $false
    try {
        & (Join-Path $PSScriptRoot "check_ssgi_performance_reference_windows.ps1") -InputDirectory $fixtureRoot | Out-Null
    }
    catch {
        $rejectedBudget = $_.Exception.Message -match "100ms GPU budget"
    }
    if (-not $rejectedBudget) {
        throw "The SSGI performance validator did not reject a gross GPU budget regression."
    }

    Write-Output "SSGI performance reference validator tests passed."
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
}
