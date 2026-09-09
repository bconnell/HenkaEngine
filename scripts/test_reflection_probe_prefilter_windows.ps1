$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$renderer = Get-Content (Join-Path $repoRoot 'engine/src/renderer/renderer_opengl.c') -Raw
$shader = Get-Content (Join-Path $repoRoot 'assets/shaders/basic_lit.frag') -Raw
$engineHeader = Get-Content (Join-Path $repoRoot 'engine/include/henka/engine.h') -Raw

$missing = @()
if ($renderer -notmatch 'HENKA_REFLECTION_PROBE_PREFILTER_LEVELS\s+7') {
    $missing += 'full seven-level reflection-probe prefilter contract'
}
if ($shader -notmatch 'uniform float iblPrefilterMaxLod;' -or
    $shader -notmatch 'float environmentPrefilterLod =\s*clamp\(surfaceRoughness, 0\.0, 1\.0\) \*\s*clamp\(iblPrefilterMaxLod, 0\.0, 1024\.0\);' -or
    $shader -notmatch 'textureLod\(\s*reflectionProbeMap,\s*blurredReflectionDirection,\s*environmentPrefilterLod\)' -or
    $shader -notmatch 'textureLod\(\s*reflectionProbeMapSecondary,\s*secondaryReflectionDirection,\s*environmentPrefilterLod\)') {
    $missing += 'full-range local-probe roughness LOD selection'
}
if ($renderer -notmatch 'GL_LINEAR_MIPMAP_LINEAR') {
    $missing += 'trilinear reflection-probe filtering'
}
if ($renderer -notmatch 'henka_opengl_prefilter_reflection_probe' -or
    $renderer -notmatch 'importanceSampleGGX' -or
    $renderer -notmatch 'ibl_prefilter_program') {
    $missing += 'roughness-aware GGX reflection-probe prefiltering'
}
if ($renderer -notmatch 'henka_opengl_prefilter_reflection_probe\s*\(\s*henka_opengl_renderer_state\*\s*state\s*,\s*GLuint\s+source_texture\s*,\s*GLuint\s+destination_texture' -or
    $renderer -notmatch 'source_texture\s*==\s*destination_texture' -or
    $renderer -notmatch 'glBindTexture\(GL_TEXTURE_CUBE_MAP,\s*source_texture\)' -or
    $renderer -notmatch 'FramebufferTexture2D\(\s*GL_FRAMEBUFFER,\s*GL_COLOR_ATTACHMENT0,\s*GL_TEXTURE_CUBE_MAP_POSITIVE_X\s*\+\s*face,\s*destination_texture' -or
    $renderer -notmatch 'henka_opengl_allocate_reflection_probe_cube\(&source,\s*1\)' -or
    $renderer -notmatch '"sourceMipMaxLod",\s*0\.0f' -or
    $renderer -notmatch 'GL_CURRENT_PROGRAM' -or
    $renderer -notmatch 'GL_VERTEX_ARRAY_BINDING' -or
    $renderer -notmatch 'UseProgram\(\(GLuint\)previous_program\)' -or
    $renderer -notmatch 'BindVertexArray\(\(GLuint\)previous_vertex_array\)' -or
    $renderer -notmatch 'henka_opengl_prefilter_reflection_probe\(state,\s*source,\s*candidate\)') {
    $missing += 'separate source and destination cubemaps for local-probe prefiltering'
}
if ($renderer -notmatch 'HENKA_IBL_PREFILTER_RESOLUTION 256') {
    $missing += 'high-resolution IBL prefilter storage'
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
