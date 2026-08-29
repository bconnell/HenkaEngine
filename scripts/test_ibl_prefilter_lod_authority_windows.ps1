param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

$shaderPath = Join-Path $RepositoryRoot 'assets/shaders/basic_lit.frag'
$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$shader = Get-Content -LiteralPath $shaderPath -Raw
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$missing = @()

if ($shader -match 'HENKA_PREFILTER_MAX_LOD') {
    $missing += 'shader-side hard-coded prefilter maximum LOD'
}
if ($shader -notmatch 'uniform float iblPrefilterMaxLod;') {
    $missing += 'runtime prefilter maximum LOD uniform'
}
if ($shader -notmatch 'clamp\(iblPrefilterMaxLod, 0\.0, 1024\.0\)') {
    $missing += 'bounded runtime prefilter maximum LOD use'
}
if ($renderer -notmatch 'uniform float sourceResolution' -or
    $renderer -notmatch 'uniform float sourceMipMaxLod') {
    $missing += 'source mip authority uniforms'
}
if ($renderer -match 'textureLod\(environmentCube,l,0\.0\)') {
    $missing += 'source-LOD-aware GGX sampling'
}
if ($renderer -notmatch 'omegaS' -or
    $renderer -notmatch 'omegaP' -or
    $renderer -notmatch 'log2') {
    $missing += 'GGX sample-to-texel solid-angle selection'
}
if ($renderer -notmatch '"iblPrefilterMaxLod"') {
    $missing += 'prefilter maximum LOD shader location'
}
if ($renderer -notmatch 'henka_set_uniform_float_owned\(\s*program,\s*shader_data,\s*"iblPrefilterMaxLod"') {
    $missing += 'per-draw prefilter maximum LOD binding'
}
if ($renderer -notmatch 'HENKA_REFLECTION_PROBE_PREFILTER_LEVELS - 1') {
    $missing += 'reflection-probe generated level authority'
}
if ($renderer -notmatch 'HENKA_IBL_PREFILTER_LEVELS - 1') {
    $missing += 'environment generated level authority'
}
if ($renderer -notmatch 'use_reflection_probe_map\s*\?\s*\(float\)\(HENKA_REFLECTION_PROBE_PREFILTER_LEVELS - 1\)') {
    $missing += 'selected reflection-probe LOD authority'
}
if ($renderer -notmatch 'henka_opengl_full_mip_count\(HENKA_IBL_ENVIRONMENT_RESOLUTION\)' -or
    ($renderer -notmatch '(?:g_gl\.)?GenerateMipmap\(GL_TEXTURE_CUBE_MAP\)')) {
    $missing += 'generated source environment mip chain'
}
if ($renderer -notmatch '"sourceResolution"' -or
    $renderer -notmatch '"sourceMipMaxLod"') {
    $missing += 'source mip shader bindings'
}

if ($missing.Count -gt 0) {
    throw "IBL prefilter LOD authority contract failed: $($missing -join ', ')"
}

Write-Output 'IBL prefilter LOD authority contract passed.'
