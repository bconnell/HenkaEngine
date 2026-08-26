$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw
$capture = Get-Content (Join-Path $repoRoot 'scripts/capture_visual_evidence_windows.ps1') -Raw
$shader = Get-Content (Join-Path $repoRoot 'assets/shaders/basic_lit.frag') -Raw
$checkerPath = Join-Path $repoRoot 'scripts/check_pbr_energy_reference_windows.ps1'
$missing = @()

if ($sandbox -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_ENERGY') {
    $missing += 'energy reference kind'
}
if ($sandbox -notmatch 'strcmp\(\s*value, "energy"\)') {
    $missing += 'energy reference parser'
}
if ($sandbox -notmatch 'energy_reference=1') {
    $missing += 'energy readiness metadata'
}
if ($sandbox -notmatch '--capture-realism-reference energy wide\|close solid\|material_preview\|rendered') {
    $missing += 'energy command help'
}
if ($shader -notmatch 'diffuseEnergyWeight' -or
    $shader -notmatch 'f0\s*=') {
    $missing += 'bounded PBR energy path'
}
if ($capture -notmatch 'PBR_ENERGY_REFERENCE') {
    $missing += 'energy evidence profile'
}
if ($capture -notmatch '_ENERGY_REFERENCE') {
    $missing += 'energy readiness dispatch'
}
if (-not (Test-Path -LiteralPath $checkerPath -PathType Leaf)) {
    $missing += 'energy evidence checker'
}
else {
    $checker = Get-Content -LiteralPath $checkerPath -Raw
    if ($checker -notmatch 'dielectric' -or
        $checker -notmatch 'metallic' -or
        $checker -notmatch 'clipped') {
        $missing += 'energy group and clipping checks'
    }
}

if ($missing.Count -gt 0) {
    throw "PBR energy reference contract is incomplete: $($missing -join ', ')"
}

Write-Output 'PBR energy reference source contract test passed.'
