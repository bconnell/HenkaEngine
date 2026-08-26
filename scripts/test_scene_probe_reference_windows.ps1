$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw
$capture = Get-Content (Join-Path $repoRoot 'scripts/capture_visual_evidence_windows.ps1') -Raw
$docs = Get-Content (Join-Path $repoRoot 'docs/realism.md') -Raw
$checkerPath = Join-Path $repoRoot 'scripts/check_scene_probe_reference_windows.ps1'
$missing = @()

if ($sandbox -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_SCENE_PROBE') {
    $missing += 'scene-probe reference kind'
}
if ($sandbox -notmatch 'strcmp\(\s*value, "scene_probe"\)') {
    $missing += 'scene-probe reference parser'
}
if ($sandbox -notmatch 'CAPTURE_READY_SCENE_PROBE_REFERENCE') {
    $missing += 'scene-probe readiness prefix'
}
if ($sandbox -notmatch 'probe_reference=1' -or
    $sandbox -notmatch 'rendered_reflection_probe_diffuse_active' -or
    $sandbox -notmatch 'rendered_reflection_probe_prefilter_active' -or
    $sandbox -notmatch 'rendered_reflection_probe_blend_active' -or
    $sandbox -notmatch 'screen_space_reflections_active') {
    $missing += 'scene-probe readiness diagnostics'
}
if ($sandbox -notmatch 'rendered_reflection_probe_enabled_count' -or
    $sandbox -notmatch 'rendered_reflection_probe_captured_count') {
    $missing += 'scene-probe capture counts'
}
if ($sandbox -notmatch '--capture-realism-reference scene_probe wide\|close rendered') {
    $missing += 'scene-probe command help'
}
if ($capture -notmatch 'SCENE_PROBE_REFERENCE' -or
    $capture -notmatch '--capture-realism-reference", "scene_probe"') {
    $missing += 'scene-probe evidence profile'
}
if (-not (Test-Path -LiteralPath $checkerPath -PathType Leaf)) {
    $missing += 'scene-probe evidence checker'
}
else {
    $checker = Get-Content -LiteralPath $checkerPath -Raw
    if ($checker -notmatch 'probe_enabled_count' -or
        $checker -notmatch 'probe_captured_count' -or
        $checker -notmatch 'StandardDeviation') {
        $missing += 'scene-probe runtime image and capture-count checks'
    }
}
if ($docs -notmatch 'SCENE_PROBE_REFERENCE' -or
    $docs -notmatch 'probe[ -]grid') {
    $missing += 'scene-probe documentation boundary'
}

if ($missing.Count -gt 0) {
    throw "Scene-probe reference contract is incomplete: $($missing -join ', ')"
}

Write-Output 'Scene-probe reference source contract test passed.'
