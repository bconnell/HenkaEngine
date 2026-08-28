$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw
$generator = Get-Content (Join-Path $repoRoot 'scripts/generate_showcase_assets.ps1') -Raw
$checker = Get-Content (Join-Path $repoRoot 'scripts/check_realism_reference_visual_evidence_windows.ps1') -Raw
$missing = @()

if ($sandbox -notmatch '#define SANDBOX3D_REALISM_TEXTURE_EDGE 128U') {
    $missing += '128-pixel realism texture edge'
}
if ($sandbox -match 'unsigned char detail_normal_pixels\[SANDBOX3D_REALISM_TEXTURE_PIXEL_COUNT\]' -or
    $sandbox -match 'unsigned char macro_variation_pixels\[SANDBOX3D_REALISM_TEXTURE_PIXEL_COUNT\]' -or
    $sandbox -match 'unsigned char wood_grain_pixels\[SANDBOX3D_REALISM_TEXTURE_PIXEL_COUNT\]' -or
    $sandbox -match 'unsigned char wet_dry_roughness_pixels\[SANDBOX3D_REALISM_TEXTURE_PIXEL_COUNT\]' -or
    $sandbox -match 'unsigned char color_space_reference_pixels\[SANDBOX3D_REALISM_TEXTURE_PIXEL_COUNT\]') {
    $missing += 'stack-backed high-resolution realism buffers'
}
if ($sandbox -notmatch 'detail_normal_pixels\s*=\s*\(unsigned char\*\)henka_calloc\(' -or
    $sandbox -notmatch 'macro_variation_pixels\s*=\s*\(unsigned char\*\)henka_calloc\(' -or
    $sandbox -notmatch 'wood_grain_pixels\s*=\s*\(unsigned char\*\)henka_calloc\(' -or
    $sandbox -notmatch 'wet_dry_roughness_pixels\s*=\s*\(unsigned char\*\)henka_calloc\(') {
    $missing += 'heap-backed realism texture ownership'
}
foreach ($buffer in @('detail_normal_pixels', 'macro_variation_pixels', 'wood_grain_pixels', 'wet_dry_roughness_pixels', 'color_space_reference_pixels')) {
    if ($sandbox -notmatch ('henka_free\(' + $buffer + '\)')) {
        $missing += "$buffer cleanup"
    }
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
if ($sandbox -match '101U, 3U, 3U' -or
    $sandbox -match '131U, 7U, 3U' -or
    $sandbox -match '157U, 5U, 9U') {
    $missing += 'former very-low-frequency detail octaves'
}
if ($generator -notmatch 'function Get-ShowcaseTileableNoise') {
    $missing += 'tileable deterministic showcase texture noise helper'
}
if ($generator -notmatch '(?s)elseif \(\$Kind -eq "normal"\).*?Get-ShowcaseDetailNoise') {
    $missing += 'noise-driven generated normal detail for showcase fixtures'
}
if ($generator -notmatch '(?s)else \{\s*\$variation = .*?Get-ShowcaseDetailNoise.*?Get-ShowcaseTileableNoise') {
    $missing += 'noise-driven generated roughness detail for showcase fixtures'
}
if ($generator -match '\$frequency = if \(\$Subject -eq "giraffe"\)' -or
    $generator -match '\$variation = 0\.5 \+ \(0\.5 \* \[Math\]::Sin\(\(\$x \+ 2\) \* 0\.24') {
    $missing += 'single-frequency showcase normal or roughness bands'
}
if ($checker -notmatch '\[int\]\$metadata\[0\]\.Groups\["texture_edge"\]\.Value -lt 128') {
    $missing += 'visual gate 128-pixel minimum texture resolution'
}

if ($missing.Count -gt 0) {
    throw "Realism texture detail contract is incomplete: $($missing -join ', ')"
}

Write-Output 'Realism texture detail source contract test passed.'
