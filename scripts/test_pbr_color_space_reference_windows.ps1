$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw
$scene = Get-Content (Join-Path $repoRoot 'engine/src/scene/scene.c') -Raw
$texture = Get-Content (Join-Path $repoRoot 'engine/src/renderer/texture.c') -Raw
$capture = Get-Content (Join-Path $repoRoot 'scripts/capture_visual_evidence_windows.ps1') -Raw
$checkerPath = Join-Path $repoRoot 'scripts/check_pbr_color_space_reference_windows.ps1'
$missing = @()

if ($sandbox -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_COLOR_SPACE') {
    $missing += 'color-space reference kind'
}
if ($sandbox -notmatch 'strcmp\(\s*value, "color_space"\)') {
    $missing += 'color-space reference parser'
}
if ($sandbox -notmatch 'color_space_reference=1') {
    $missing += 'color-space readiness metadata'
}
if ($scene -notmatch 'henka_material_color_texture_matches' -or
    $scene -notmatch 'HENKA_TEXTURE_COLOR_SPACE_LINEAR') {
    $missing += 'linear base-color material contract'
}
if ($texture -notmatch 'source_byte_size\s*=\s*decoded_bytes') {
    $missing += 'runtime texture source-size metadata'
}
if ($capture -notmatch 'PBR_COLOR_SPACE_REFERENCE') {
    $missing += 'color-space evidence profile'
}
if (-not (Test-Path -LiteralPath $checkerPath -PathType Leaf)) {
    $missing += 'color-space evidence checker'
}
else {
    $checker = Get-Content -LiteralPath $checkerPath -Raw
    if ($checker -notmatch 'color_space_srgb' -or
        $checker -notmatch 'color_space_linear' -or
        $checker -notmatch 'MeanRgbDifference') {
        $missing += 'matched sRGB/linear image comparison'
    }
}

if ($missing.Count -gt 0) {
    throw "PBR color-space reference contract is incomplete: $($missing -join ', ')"
}

Write-Output 'PBR color-space reference source contract test passed.'
