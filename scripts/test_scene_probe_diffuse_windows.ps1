$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$shaderPath = Join-Path $repoRoot "assets/shaders/basic_lit.frag"
$rendererPath = Join-Path $repoRoot "engine/src/renderer/renderer_opengl.c"
$engineHeaderPath = Join-Path $repoRoot "engine/include/henka/engine.h"

$shader = Get-Content -LiteralPath $shaderPath -Raw
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$engineHeader = Get-Content -LiteralPath $engineHeaderPath -Raw

$missing = [System.Collections.Generic.List[string]]::new()
if ($shader -notmatch 'uniform bool useReflectionProbeDiffuse') {
    $missing.Add("basic_lit.frag declares the scene-probe diffuse activation uniform")
}
if ($shader -notmatch 'sampleSceneProbeDiffuse') {
    $missing.Add("basic_lit.frag contains the bounded scene-probe diffuse sampling function")
}
if ($renderer -notmatch 'useReflectionProbeDiffuse') {
    $missing.Add("the OpenGL material binding owns the scene-probe diffuse uniform")
}
if ($renderer -notmatch 'reflection_probe_diffuse_active') {
    $missing.Add("the OpenGL renderer records scene-probe diffuse activation")
}
if ($engineHeader -notmatch 'rendered_reflection_probe_diffuse_active') {
    $missing.Add("public diagnostics expose scene-probe diffuse activation")
}

if ($missing.Count -gt 0) {
    throw ("Scene-probe diffuse contract is incomplete:`n - " + ($missing -join "`n - "))
}

Write-Output "Scene-probe diffuse source contract test passed."
