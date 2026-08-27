$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$renderer = Get-Content (Join-Path $repoRoot 'engine/src/renderer/renderer_opengl.c') -Raw
$shader = Get-Content (Join-Path $repoRoot 'assets/shaders/basic_lit.frag') -Raw
$engineHeader = Get-Content (Join-Path $repoRoot 'engine/include/henka/engine.h') -Raw

$missing = @()
if ($renderer -notmatch 'HENKA_REFLECTION_PROBE_PREFILTER_LEVELS\s+7') {
    $missing += 'full seven-level reflection-probe prefilter contract'
}
if ($shader -notmatch 'surfaceRoughness\s*\*\s*6\.0') {
    $missing += 'full-range roughness LOD selection'
}
if ($renderer -notmatch 'GL_LINEAR_MIPMAP_LINEAR') {
    $missing += 'trilinear reflection-probe filtering'
}
if ($renderer -notmatch 'henka_opengl_prefilter_reflection_probe' -or
    $renderer -notmatch 'importanceSampleGGX' -or
    $renderer -notmatch 'ibl_prefilter_program') {
    $missing += 'roughness-aware GGX reflection-probe prefiltering'
}
if ($renderer -notmatch 'reflection_probe_prefilter_active') {
    $missing += 'reflection_probe_prefilter_active renderer diagnostic'
}
if ($engineHeader -notmatch 'rendered_reflection_probe_prefilter_active') {
    $missing += 'rendered_reflection_probe_prefilter_active public diagnostic'
}

if ($missing.Count -gt 0) {
    throw "Reflection-probe prefilter contract is incomplete: $($missing -join ', ')"
}

Write-Output 'Reflection-probe prefilter source contract test passed.'
