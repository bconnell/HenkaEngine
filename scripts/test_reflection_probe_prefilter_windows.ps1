$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$renderer = Get-Content (Join-Path $repoRoot 'engine/src/renderer/renderer_opengl.c') -Raw
$engineHeader = Get-Content (Join-Path $repoRoot 'engine/include/henka/engine.h') -Raw

$missing = @()
if ($renderer -notmatch 'HENKA_REFLECTION_PROBE_PREFILTER_LEVELS\s+5') {
    $missing += 'five-level reflection-probe prefilter contract'
}
if ($renderer -notmatch 'GL_LINEAR_MIPMAP_LINEAR') {
    $missing += 'trilinear reflection-probe filtering'
}
if ($renderer -notmatch 'g_gl\.GenerateMipmap\(GL_TEXTURE_CUBE_MAP\)') {
    $missing += 'generated reflection-probe mip chain'
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
