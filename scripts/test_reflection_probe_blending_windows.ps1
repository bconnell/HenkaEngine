$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$renderer = Get-Content (Join-Path $repoRoot 'engine/src/renderer/renderer_opengl.c') -Raw
$shader = Get-Content (Join-Path $repoRoot 'assets/shaders/basic_lit.frag') -Raw
$engineHeader = Get-Content (Join-Path $repoRoot 'engine/include/henka/engine.h') -Raw
$engineCore = Get-Content (Join-Path $repoRoot 'engine/src/core/engine.c') -Raw
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw

$missing = @()
if ($renderer -notmatch 'henka_opengl_select_reflection_probes') {
    $missing += 'bounded primary/secondary probe selection'
}
if ($renderer -notmatch 'reflection_probe_blend_weight') {
    $missing += 'deterministic probe blend weight'
}
if ($renderer -notmatch 'reflection_probe_blend_active') {
    $missing += 'runtime probe blend diagnostic'
}
if ($renderer -notmatch 'reflectionProbeMapSecondary') {
    $missing += 'secondary reflection-probe sampler binding'
}
if ($shader -notmatch 'useReflectionProbeMapSecondary') {
    $missing += 'secondary reflection-probe shader gate'
}
if ($shader -notmatch 'reflectionProbeBlendWeight') {
    $missing += 'secondary reflection-probe shader blend'
}
if ($shader -notmatch 'reflectionProbeMapSecondary') {
    $missing += 'secondary reflection-probe shader sample'
}
if ($engineHeader -notmatch 'rendered_reflection_probe_blend_active') {
    $missing += 'public probe blend diagnostic'
}
if ($renderer -notmatch 'henka_opengl_renderer_get_reflection_probe_blend_active') {
    $missing += 'renderer probe blend diagnostic getter'
}
if ($engineCore -notmatch 'rendered_reflection_probe_blend_active\s*=\s*\r?\n\s*henka_opengl_renderer_get_reflection_probe_blend_active') {
    $missing += 'engine probe blend diagnostic propagation'
}
if ($sandbox -notmatch 'rendered_reflection_probe_blend_active' -or
    $sandbox -notmatch 'reference_probe_blend_active=1') {
    $missing += 'runtime probe blend readiness metadata'
}
if ($shader -notmatch 'environmentSpecular = mix' -or
    $shader -notmatch 'sceneProbeDiffuse = mix') {
    $missing += 'reflection-probe contribution blending'
}

if ($missing.Count -gt 0) {
    throw "Reflection-probe blending contract is incomplete: $($missing -join ', ')"
}

Write-Output 'Reflection-probe blending source contract test passed.'
