param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

$engineHeader = Get-Content (Join-Path $RepositoryRoot 'engine/include/henka/engine.h') -Raw
$rendererCore = Get-Content (Join-Path $RepositoryRoot 'engine/src/renderer/renderer.c') -Raw
$renderer = Get-Content (Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c') -Raw
$shader = Get-Content (Join-Path $RepositoryRoot 'assets/shaders/basic_lit.frag') -Raw
$sandbox = Get-Content (Join-Path $RepositoryRoot 'examples/sandbox3d/main.c') -Raw
$missing = @()

if ($engineHeader -notmatch 'HENKA_IBL_DIAGNOSTIC_EMPTY_ENVIRONMENT') {
    $missing += 'empty-environment diagnostic enum'
}
if ($engineHeader -notmatch 'henka_engine_set_ibl_diagnostic_prefilter_lod') {
    $missing += 'public prefilter LOD diagnostic setter'
}
if (-not $rendererCore.Contains('ibl_diagnostic_prefilter_lod = -1.0f')) {
    $missing += 'disabled prefilter LOD default'
}
if (-not $rendererCore.Contains('isfinite(lod)') -or
    -not $rendererCore.Contains('lod < -1.0f') -or
    -not $rendererCore.Contains('lod > 1024.0f')) {
    $missing += 'finite bounded prefilter LOD validation'
}
if ($renderer -notmatch 'iblDiagnosticPrefilterLod' -or
    $renderer -notmatch 'renderer->ibl_diagnostic_prefilter_lod') {
    $missing += 'exact diagnostic prefilter LOD shader binding'
}
if (-not $shader.Contains('uniform float iblDiagnosticPrefilterLod;') -or
    -not $shader.Contains('iblDiagnosticPrefilterLod >= 0.0')) {
    $missing += 'exact diagnostic prefilter LOD selection'
}
if ($sandbox -notmatch 'IBL_DIAGNOSTIC_EMPTY_ENVIRONMENT' -or
    $sandbox -notmatch 'ibl_empty' -or
    $sandbox -notmatch 'ibl_rotation' -or
    $sandbox -notmatch 'ibl_mip') {
    $missing += 'empty, rotation, and mip capture modes'
}
if (-not $sandbox.Contains('ibl_rotation_radians = state->ibl_rotation_degrees * HENKA_DEG_TO_RAD') -or
    -not $sandbox.Contains('environment.hdr_rotation = ibl_rotation_radians')) {
    $missing += 'environment rotation application'
}
if ($sandbox -notmatch 'henka_engine_set_ibl_diagnostic_prefilter_lod') {
    $missing += 'mip capture diagnostic installation'
}
if (-not $sandbox.Contains('SANDBOX3D_REALISM_REFERENCE_KIND_IBL_EMPTY_ENVIRONMENT) != HENKA_SUCCESS')) {
    $missing += 'empty-environment geometry isolation'
}

if ($missing.Count -gt 0) {
    throw "IBL diagnostic control contract failed: $($missing -join ', ')"
}

Write-Output 'IBL diagnostic control contract passed.'
