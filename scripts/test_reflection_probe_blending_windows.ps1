$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$renderer = Get-Content (Join-Path $repoRoot 'engine/src/renderer/renderer_opengl.c') -Raw
$shader = Get-Content (Join-Path $repoRoot 'assets/shaders/basic_lit.frag') -Raw
$engineHeader = Get-Content (Join-Path $repoRoot 'engine/include/henka/engine.h') -Raw
$engineCore = Get-Content (Join-Path $repoRoot 'engine/src/core/engine.c') -Raw
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw

function Get-ReflectionProbeBlendingContractFailures {
    param(
        [Parameter(Mandatory = $true)] [string]$Renderer,
        [Parameter(Mandatory = $true)] [string]$Shader,
        [Parameter(Mandatory = $true)] [string]$EngineHeader,
        [Parameter(Mandatory = $true)] [string]$EngineCore,
        [Parameter(Mandatory = $true)] [string]$Sandbox
    )

    $missing = @()
    if ($Renderer -notmatch 'henka_opengl_select_reflection_probes') {
        $missing += 'bounded primary/secondary probe selection'
    }
    if ($Renderer -notmatch 'reflection_probe_blend_weight') {
        $missing += 'deterministic probe blend weight'
    }
    if ($Renderer -notmatch 'reflection_probe_blend_active') {
        $missing += 'runtime probe blend diagnostic'
    }
    if ($Renderer -notmatch 'reflectionProbeMapSecondary') {
        $missing += 'secondary reflection-probe sampler binding'
    }
    if ($Shader -notmatch 'useReflectionProbeMapSecondary') {
        $missing += 'secondary reflection-probe shader gate'
    }
    if ($Shader -notmatch 'reflectionProbeBlendWeight') {
        $missing += 'secondary reflection-probe shader blend'
    }
    if ($Shader -notmatch 'reflectionProbeMapSecondary') {
        $missing += 'secondary reflection-probe shader sample'
    }
    if ($EngineHeader -notmatch 'rendered_reflection_probe_blend_active') {
        $missing += 'public probe blend diagnostic'
    }
    if ($Renderer -notmatch 'henka_opengl_renderer_get_reflection_probe_blend_active') {
        $missing += 'renderer probe blend diagnostic getter'
    }
    if ($EngineCore -notmatch 'rendered_reflection_probe_blend_active\s*=\s*\r?\n\s*henka_opengl_renderer_get_reflection_probe_blend_active') {
        $missing += 'engine probe blend diagnostic propagation'
    }
    if ($Sandbox -notmatch 'rendered_reflection_probe_blend_active' -or
        $Sandbox -notmatch 'reference_probe_blend_active=1') {
        $missing += 'runtime probe blend readiness metadata'
    }
    if ($Shader -notmatch 'environmentSpecular = mix' -or
        $Shader -notmatch 'sceneProbeDiffuse = mix') {
        $missing += 'reflection-probe contribution blending'
    }

    return $missing
}

$missing = @(Get-ReflectionProbeBlendingContractFailures `
    -Renderer $renderer `
    -Shader $shader `
    -EngineHeader $engineHeader `
    -EngineCore $engineCore `
    -Sandbox $sandbox)

if ($missing.Count -gt 0) {
    throw "Reflection-probe blending contract is incomplete: $($missing -join ', ')"
}

Write-Output 'Reflection-probe blending source contract test passed.'

$negativeRenderer = $renderer.Replace(
    'reflection_probe_blend_weight',
    'reflection_probe_weight_removed_for_negative_control')
$negativeMissing = @(Get-ReflectionProbeBlendingContractFailures `
    -Renderer $negativeRenderer `
    -Shader $shader `
    -EngineHeader $engineHeader `
    -EngineCore $engineCore `
    -Sandbox $sandbox)
if ($negativeMissing.Count -eq 0) {
    throw 'Reflection-probe blending negative control unexpectedly passed.'
}

Write-Output 'Reflection-probe blending negative control passed.'
