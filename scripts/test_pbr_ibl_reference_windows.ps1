$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sandbox = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/main.c') -Raw
$capture = Get-Content (Join-Path $repoRoot 'scripts/capture_visual_evidence_windows.ps1') -Raw
$shader = Get-Content (Join-Path $repoRoot 'assets/shaders/basic_lit.frag') -Raw
$renderer = Get-Content (Join-Path $repoRoot 'engine/src/renderer/renderer_opengl.c') -Raw
$rendererCore = Get-Content (Join-Path $repoRoot 'engine/src/renderer/renderer.c') -Raw
$mesh = Get-Content (Join-Path $repoRoot 'engine/src/renderer/mesh.c') -Raw
$checkerPath = Join-Path $repoRoot 'scripts/check_pbr_ibl_reference_windows.ps1'
$missing = @()

if ($sandbox -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_IBL') {
    $missing += 'IBL reference kind'
}
if ($sandbox -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_IBL_NORMAL_COLOR' -or
    $sandbox -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_IBL_DIFFUSE_ONLY' -or
    $sandbox -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_IBL_SPECULAR_ONLY' -or
    $sandbox -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_IBL_SIMPLE_ENVIRONMENT') {
    $missing += 'IBL diagnostic reference kinds'
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
if ($sandbox -notmatch 'IBL Roughness 0\.05' -or
    $sandbox -notmatch 'ibl_roughness_ladder') {
    $missing += 'IBL roughness ladder fixture'
}
if ($sandbox -notmatch 'sandbox3d_is_ibl_reference_kind\(') {
    $missing += 'IBL reference isolation from local reflection probes'
}
if ($sandbox -notmatch 'if \(!sandbox3d_is_ibl_reference_kind\(') {
    $missing += 'IBL reference isolation from direct local lights'
}
if ($sandbox -notmatch 'henka_scene_set_light_intensity\(state->scene, 0\.0f\)') {
    $missing += 'IBL reference isolation from direct directional light'
}
if ($sandbox -notmatch 'ibl_direct_lighting=0') {
    $missing += 'IBL direct-light isolation metadata'
}
if ($sandbox -notmatch 'henka_mesh_create_uv_sphere\(engine, 0\.5f, 128, 64') {
    $missing += 'high-resolution smooth reference sphere fixture'
}
if ($sandbox -notmatch 'float\* studio_environment_pixels = \(float\*\)henka_calloc' -or
    $sandbox -notmatch 'henka_free\(studio_environment_pixels\)') {
    $missing += 'heap-owned high-resolution studio environment fixture'
}
$studioHeader = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/studio_environment.h') -Raw
if ($studioHeader -notmatch 'SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH\s+128U' -or
    $studioHeader -notmatch 'SANDBOX3D_STUDIO_ENVIRONMENT_HEIGHT\s+64U') {
    $missing += 'high-resolution smooth studio environment fixture'
}
$studioSource = Get-Content (Join-Path $repoRoot 'examples/sandbox3d/studio_environment.c') -Raw
if ($studioSource -notmatch 'lower_gradient = sandbox3d_smoothstep\(\(latitude - 0\.38f\) / 0\.62f\)' -or
    $studioSource -match '0\.055f \* ground|0\.075f \* ground|0\.105f \* ground') {
    $missing += 'continuous non-basin lower studio environment gradient'
}
if ($studioSource -notmatch 'key_delta = fabsf\(longitude - 4\.35f\)' -or
    $studioSource -notmatch 'fill_delta = fabsf\(longitude - 1\.05f\)' -or
    $studioSource -notmatch 'key_field = 0\.78f \+ 0\.22f \* cosf\(key_delta\)' -or
    $studioSource -notmatch 'fill_field = 0\.88f \+ 0\.12f \* cosf\(fill_delta\)' -or
    $studioSource -notmatch 'key_field \* 1\.75f') {
    $missing += 'broad cosine studio key/fill reflection field'
}
if ($sandbox -notmatch '--capture-realism-reference ibl wide\|close rendered') {
    $missing += 'IBL command help'
}
if ($shader -notmatch 'iblIrradianceMap' -or
    $shader -notmatch 'iblPrefilterMap' -or
    $shader -notmatch 'iblBrdfLut') {
    $missing += 'IBL shader inputs'
}
if ($shader -notmatch 'iblDiagnosticMode' -or
    $shader -notmatch 'IBL_DIAGNOSTIC_NORMAL_COLOR' -or
    $shader -notmatch 'IBL_DIAGNOSTIC_DIFFUSE_ONLY' -or
    $shader -notmatch 'IBL_DIAGNOSTIC_SPECULAR_ONLY' -or
    $shader -notmatch 'IBL_DIAGNOSTIC_SIMPLE_ENVIRONMENT') {
    $missing += 'IBL diagnostic shader controls'
}
if ($shader -notmatch 'uniform float iblPrefilterMaxLod;' -or
    $shader -notmatch 'float environmentPrefilterLod =\s*clamp\(surfaceRoughness, 0\.0, 1\.0\) \*\s*clamp\(iblPrefilterMaxLod, 0\.0, 1024\.0\);' -or
    $shader -notmatch 'vec3 blurredReflectionDirection = reflectionDirection;' -or
    $shader -match 'mix\(reflectionDirection, normal, surfaceRoughness' -or
    $shader -notmatch 'textureLod\(\s*iblPrefilterMap,\s*blurredReflectionDirection,\s*environmentPrefilterLod\)' -or
    $shader -match 'min\(surfaceRoughness \* 6\.0,\s*2\.0\)' -or
    $shader -match 'textureLod\(\s*iblPrefilterMap,\s*vec3\(0\.0,\s*0\.0,\s*1\.0\),\s*0\.0' ) {
    $missing += 'single-source roughness filtering for IBL reflection direction'
}
if ($shader -notmatch 'texture\(iblBrdfLut,\s*vec2\(nDotV,\s*1\.0\s*-\s*surfaceRoughness\)\)') {
    $missing += 'view-aware BRDF LUT sampling'
}
if ($mesh -notmatch 'indices\[index\+\+\] = top;\s*indices\[index\+\+\] = first \+ 1U;\s*indices\[index\+\+\] = first;' -or
    $mesh -notmatch 'indices\[index\+\+\] = first;\s*indices\[index\+\+\] = first \+ 1U;\s*indices\[index\+\+\] = second;\s*indices\[index\+\+\] = second;\s*indices\[index\+\+\] = first \+ 1U;\s*indices\[index\+\+\] = second \+ 1U;' -or
    $mesh -notmatch 'indices\[index\+\+\] = first;\s*indices\[index\+\+\] = first \+ 1U;\s*indices\[index\+\+\] = bottom;') {
    $missing += 'UV sphere winding matches authored outward normals'
}
if ($renderer -notmatch 'HENKA_IBL_ENVIRONMENT_RESOLUTION 128' -or
    $renderer -notmatch 'HENKA_IBL_PREFILTER_RESOLUTION 256' -or
    $renderer -notmatch 'HENKA_IBL_PREFILTER_LEVELS 7' -or
    $renderer -notmatch 'GL_RGBA16F, HENKA_IBL_BRDF_RESOLUTION' -or
    $renderer -notmatch 'BRDF LUT validation' -or
    $renderer -notmatch 'const uint sampleCount=128u' -or
    $renderer -notmatch 'reflect\(normalize\(position\),normal\)' -or
    $renderer -match 'reflect\(normalize\(-position\),normal\)') {
    $missing += 'validated IBL integration resources and non-self-intersecting SSR ray direction'
}
if ($renderer -notmatch 'iblDiagnosticMode' -or
    $rendererCore -notmatch 'henka_renderer_set_ibl_diagnostic_mode') {
    $missing += 'renderer-owned IBL diagnostic mode'
}
if ($capture -notmatch 'PBR_IBL_REFERENCE' -or
    $capture -notmatch '--capture-realism-reference", "ibl"') {
    $missing += 'IBL evidence profile'
}
if ($capture -notmatch 'PBR_IBL_DIAGNOSTICS' -or
    $capture -notmatch 'ibl_normal' -or
    $capture -notmatch 'ibl_diffuse' -or
    $capture -notmatch 'ibl_specular' -or
    $capture -notmatch 'ibl_simple') {
    $missing += 'IBL diagnostic evidence profile'
}
if (-not (Test-Path -LiteralPath $checkerPath -PathType Leaf)) {
    $missing += 'IBL evidence checker'
}
else {
    $checker = Get-Content -LiteralPath $checkerPath -Raw
    if ($checker -notmatch 'roughness' -or
        $checker -notmatch 'prefilter' -or
        $checker -notmatch 'irradiance' -or
        $checker -notmatch 'roughness ladder' -or
        $checker -notmatch '\$lumas \| Where-Object \{ \$_ -lt 8\.0 \}' -or
        $checker -notmatch 'lowerBlemishes' -or
        $checker -notmatch 'concentratedHighlights' -or
        $checker -notmatch 'lower-blemishes') {
        $missing += 'IBL response checks'
    }
}

if ($missing.Count -gt 0) {
    throw "PBR IBL reference contract is incomplete: $($missing -join ', ')"
}

Write-Output 'PBR IBL reference source contract test passed.'
