$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw
$renderer = Get-Content (Join-Path $repoRoot 'engine/src/renderer/renderer_opengl.c') -Raw
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
if ($capture -notmatch 'lighting-reference-\$ReferenceView-rendered-repeat\.png' -or
    $checker -notmatch 'rendered-repeat') {
    $missing += 'repeated Rendered stability capture/check'
}
if ($renderer -notmatch 'light_matrix = henka_opengl_get_light_matrix\(\s*scene,\s*12\.0f,\s*24\.0f\s*,\s*state->shadow_resolution\s*\);') {
    $missing += 'compact near directional shadow coverage'
}
if ($renderer -notmatch 'static henka_mat4 henka_opengl_get_light_matrix\(\s*const henka_scene\* scene,\s*float shadow_extent,\s*float shadow_distance,\s*int shadow_resolution\s*\)' -or
    $renderer -notmatch 'shadow_extent \* 2\.0f\) /\s*\(float\)\(shadow_resolution > 0') {
    $missing += 'resolution-aware near shadow stabilization'
}

if ($missing.Count -gt 0) {
    throw "Lighting shadow reference contract is incomplete: $($missing -join ', ')"
}

Write-Output 'Lighting shadow reference source contract test passed.'
