$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw
$capture = Get-Content (Join-Path $repoRoot 'scripts/capture_visual_evidence_windows.ps1') -Raw
$shader = Get-Content (Join-Path $repoRoot 'assets/shaders/basic_lit.frag') -Raw
$checkerPath = Join-Path $repoRoot 'scripts/check_pbr_ibl_reference_windows.ps1'
$missing = @()

if ($sandbox -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_IBL') {
    $missing += 'IBL reference kind'
}
if ($sandbox -notmatch 'strcmp\(\s*value, "ibl"\)') {
    $missing += 'IBL reference parser'
}
if ($sandbox -notmatch 'CAPTURE_READY_IBL_REFERENCE') {
    $missing += 'IBL readiness prefix'
}
if ($sandbox -notmatch 'ibl_reference=1' -or
    $sandbox -notmatch 'rendered_ibl_ready') {
    $missing += 'IBL readiness metadata'
}
if ($sandbox -notmatch '--capture-realism-reference ibl wide\|close rendered') {
    $missing += 'IBL command help'
}
if ($shader -notmatch 'iblIrradianceMap' -or
    $shader -notmatch 'iblPrefilterMap' -or
    $shader -notmatch 'iblBrdfLut') {
    $missing += 'IBL shader inputs'
}
if ($capture -notmatch 'PBR_IBL_REFERENCE' -or
    $capture -notmatch '--capture-realism-reference", "ibl"') {
    $missing += 'IBL evidence profile'
}
if (-not (Test-Path -LiteralPath $checkerPath -PathType Leaf)) {
    $missing += 'IBL evidence checker'
}
else {
    $checker = Get-Content -LiteralPath $checkerPath -Raw
    if ($checker -notmatch 'roughness' -or
        $checker -notmatch 'prefilter' -or
        $checker -notmatch 'irradiance') {
        $missing += 'IBL response checks'
    }
}

if ($missing.Count -gt 0) {
    throw "PBR IBL reference contract is incomplete: $($missing -join ', ')"
}

Write-Output 'PBR IBL reference source contract test passed.'
