$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw
$capture = Get-Content (Join-Path $repoRoot 'scripts/capture_visual_evidence_windows.ps1') -Raw
$checkerPath = Join-Path $repoRoot 'scripts/check_pbr_normal_map_reference_windows.ps1'
$missing = @()

if ($sandbox -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_NORMAL_MAP') {
    $missing += 'normal-map reference kind'
}
if ($sandbox -notmatch 'strcmp\(value, "normal_map"\)') {
    $missing += 'normal-map capture parser'
}
if ($sandbox -notmatch 'normal_map_reference=1') {
    $missing += 'normal-map readiness metadata'
}
if ($capture -notmatch 'PBR_NORMAL_MAP_REFERENCE') {
    $missing += 'normal-map evidence profile'
}
if (-not (Test-Path -LiteralPath $checkerPath -PathType Leaf)) {
    $missing += 'normal-map evidence checker'
}
else {
    $checker = Get-Content $checkerPath -Raw
    if ($checker -notmatch 'normal_map_flat' -or
        $checker -notmatch 'normal_map_mapped' -or
        $checker -notmatch 'MeanNeighborDifference') {
        $missing += 'matched flat/mapped image comparison'
    }
}

if ($missing.Count -gt 0) {
    throw "PBR normal-map reference contract is incomplete: $($missing -join ', ')"
}

Write-Output 'PBR normal-map reference source contract test passed.'
