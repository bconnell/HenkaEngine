$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$renderer = Get-Content (Join-Path $repoRoot 'engine/src/renderer/renderer_opengl.c') -Raw
$shader = Get-Content (Join-Path $repoRoot 'assets/shaders/basic_lit.frag') -Raw
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw
$missing = @()

if ($renderer -notmatch 'hdr_roughness_texture') {
    $missing += 'HDR roughness attachment ownership'
}
if ($renderer -notmatch 'GLuint roughness_texture = 0U') {
    $missing += 'transactional roughness texture allocation'
}
if ($renderer -notmatch 'GL_COLOR_ATTACHMENT3') {
    $missing += 'HDR roughness color attachment'
}
if ($renderer -notmatch 'DrawBuffers\(4') {
    $missing += 'HDR four-target draw-buffer declaration'
}
if ($renderer -notmatch 'roughnessTexture') {
    $missing += 'tone roughness sampler'
}
if ($renderer -notmatch 'materialRoughnessAt') {
    $missing += 'per-pixel roughness sampling'
}
if ($renderer -notmatch 'schlickFresnel' -or
    $renderer -notmatch 'fresnelConfidence' -or
    $renderer -notmatch 'screenReflectionSurface' -or
    $renderer -notmatch 'reflect\(normalize\(position\),normal\)' -or
    $renderer -notmatch 'roughnessConfidence=\(1\.0-materialRoughness\)\*\(1\.0-materialRoughness\)' -or
    $renderer -notmatch 'filteredScreenReflection') {
    $missing += 'validated Fresnel-weighted screen-space reflection surface and roughness safeguards'
}
if ($renderer -notmatch 'hdr_roughness_texture != 0U') {
    $missing += 'fail-closed SSR roughness readiness gate'
}
if ($renderer -notmatch 'hdr_framebuffer_complete = false') {
    $missing += 'fail-closed HDR target teardown state'
}
if ($shader -notmatch 'layout\(location\s*=\s*3\)\s*out\s+float\s+outRoughness') {
    $missing += 'material roughness output target'
}
if ($shader -notmatch 'outRoughness\s*=\s*clamp\(surfaceRoughness') {
    $missing += 'authored roughness output'
}
if ($sandbox -notmatch 'screen_space_reflections_active') {
    $missing += 'runtime reflection readiness evidence'
}

if ($missing.Count -gt 0) {
    throw "SSR roughness-buffer contract is incomplete: $($missing -join ', ')"
}

Write-Output 'SSR roughness-buffer source contract test passed.'
