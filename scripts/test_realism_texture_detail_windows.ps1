$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw
$checker = Get-Content (Join-Path $repoRoot 'scripts/check_realism_reference_visual_evidence_windows.ps1') -Raw
$missing = @()

if ($sandbox -notmatch '#define SANDBOX3D_REALISM_TEXTURE_EDGE 64U') {
    $missing += '64-pixel realism texture edge'
}
if ($sandbox -notmatch 'static float sandbox3d_realism_value_noise\(') {
    $missing += 'tileable deterministic value noise helper'
}
if ($sandbox -notmatch 'sandbox3d_realism_value_noise\(u, v,') {
    $missing += 'noise-driven realism texture generation'
}
if ($sandbox -match 'const float macro_signal = 0\.5f \+ 0\.5f \* sinf' -or
    $sandbox -match 'const float grain_signal = 0\.5f \+ 0\.5f \* sinf') {
    $missing += 'legacy periodic macro or grain bands'
}
if ($checker -notmatch '\[int\]\$metadata\[0\]\.Groups\["texture_edge"\]\.Value -lt 64') {
    $missing += 'visual gate minimum texture resolution'
}

if ($missing.Count -gt 0) {
    throw "Realism texture detail contract is incomplete: $($missing -join ', ')"
}

Write-Output 'Realism texture detail source contract test passed.'
