$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw
$capture = Get-Content (Join-Path $repoRoot 'scripts/capture_visual_evidence_windows.ps1') -Raw
$checker = Get-Content (Join-Path $repoRoot 'scripts/check_lighting_reference_visual_evidence_windows.ps1') -Raw
$missing = @()

if ($sandbox -notmatch 'rendered_shadow_ready' -or
    $sandbox -notmatch 'rendered_cascade_shadow_framebuffer_complete' -or
    $sandbox -notmatch 'rendered_point_shadow_framebuffer_complete') {
    $missing += 'fail-closed directional/cascade/point shadow readiness'
}
if ($sandbox -notmatch 'shadow_reference=1') {
    $missing += 'shadow readiness metadata'
}
if ($capture -notmatch 'shadow_reference=1' -or
    $checker -notmatch 'shadow_reference=1') {
    $missing += 'shadow metadata capture/check contract'
}

if ($missing.Count -gt 0) {
    throw "Lighting shadow reference contract is incomplete: $($missing -join ', ')"
}

Write-Output 'Lighting shadow reference source contract test passed.'
