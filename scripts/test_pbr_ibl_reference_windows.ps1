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
$directLightIsolationPattern = 'if \(!sandbox3d_default_scene_requested\(state\) &&\s+!isolated_ibl_reference\)\s*\{\s*result = henka_scene_add_light'
if ($sandbox -notmatch 'const bool isolated_ibl_reference = sandbox3d_is_ibl_reference_kind\(' -or
    [regex]::Matches($sandbox, $directLightIsolationPattern).Count -lt 2) {
    $missing += 'IBL reference isolation from direct local lights'
}
if ($sandbox -notmatch 'henka_scene_set_light_intensity\(state->scene, 0\.0f\)') {
    $missing += 'IBL reference isolation from direct directional light'
}
if ($sandbox -notmatch 'henka_scene_get_environment\(state->scene, &capture_environment\)' -or
    $sandbox -notmatch 'capture_environment\.moon\.enabled = false' -or
    $sandbox -notmatch 'henka_scene_set_environment\(state->scene, capture_environment\)') {
    $missing += 'IBL reference isolation from the environment moon light'
}
if ($sandbox -notmatch 'ibl_direct_lighting=0' -or
    $sandbox -notmatch 'ibl_moon_lighting=0') {
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
    $studioSource -notmatch 'key_field = 0\.92f \+ 0\.08f \* cosf\(key_delta\)' -or
    $studioSource -notmatch 'fill_field = 0\.95f \+ 0\.05f \* cosf\(fill_delta\)' -or
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
if ($shader -notmatch '(?m)^\s*float alpha = surfaceRoughness;\s*$' -or
    $shader -notmatch '(?m)^\s*float clearcoatAlpha = surfaceClearcoatRoughness;\s*$' -or
    $shader -notmatch '(?m)^\s*float sheenAlpha = surfaceSheenRoughness;\s*$' -or
    $shader -match 'float alpha = surfaceRoughness \* surfaceRoughness;') {
    $missing += 'single roughness-to-alpha mapping for direct GGX lobes'
}
if ($renderer -notmatch 'float boundedRoughness=clamp\(roughness,0\.0,1\.0\); float alpha=max\(boundedRoughness,0\.001\);' -or
    $renderer -notmatch 'void main\(\)\{ float nDotV=clamp\(uv\.x,0\.0,1\.0\); float roughness=clamp\(1\.0-uv\.y,0\.0,1\.0\); outColor=vec4\(integrateBrdf\(nDotV,roughness\)') {
    $missing += 'single roughness-to-alpha mapping for IBL prefilter and BRDF generation'
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
        $checker -notmatch 'localizedBrightKnots' -or
        $checker -notmatch 'localized-bright-knots' -or
        $checker -notmatch '0\.225' -or
        $checker -notmatch '0\.520' -or
        $checker -notmatch '0\.815') {
        $missing += 'IBL response checks'
    }
}

if ($missing.Count -gt 0) {
    throw "PBR IBL reference contract is incomplete: $($missing -join ', ')"
}

# Exercise the checker with a deterministic close-layout image containing one
# compact bright reflection knot. Production capture tests run the same
# checker, but this keeps the visual regression executable in clean CI where
# no prior evidence directory is present.
Add-Type -AssemblyName System.Drawing
$negativeControlRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("henka-ibl-checker-negative-" + [Guid]::NewGuid().ToString('N'))
$negativeControlPassed = $false
try {
    New-Item -ItemType Directory -Path $negativeControlRoot -Force | Out-Null
    $negativeControlImage = Join-Path $negativeControlRoot 'ibl-reference-close-rendered.png'
    $negativeControlIndex = Join-Path $negativeControlRoot 'INDEX.txt'
    $bitmap = [System.Drawing.Bitmap]::new(1282, 752)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
            $graphics.Clear([System.Drawing.Color]::FromArgb(220, 214, 208))
            $graphics.FillRectangle(
                [System.Drawing.Brushes]::DarkSlateGray,
                0,
                528,
                $bitmap.Width,
                $bitmap.Height - 528)
            $centers = @(
                @(0.319, 0.225), @(0.506, 0.225), @(0.694, 0.225),
                @(0.319, 0.520), @(0.506, 0.520), @(0.694, 0.520),
                @(0.319, 0.815), @(0.506, 0.815), @(0.694, 0.815)
            )
            $sphereColors = @(
                [System.Drawing.Color]::FromArgb(90, 90, 90),
                [System.Drawing.Color]::FromArgb(112, 112, 112),
                [System.Drawing.Color]::FromArgb(134, 134, 134),
                [System.Drawing.Color]::FromArgb(148, 148, 148),
                [System.Drawing.Color]::FromArgb(170, 170, 170),
                [System.Drawing.Color]::FromArgb(192, 192, 192),
                [System.Drawing.Color]::FromArgb(202, 202, 202),
                [System.Drawing.Color]::FromArgb(180, 180, 180),
                [System.Drawing.Color]::FromArgb(158, 158, 158)
            )
            $sphereRadius = 94
            for ($index = 0; $index -lt $centers.Count; ++$index) {
                $centerX = [int][Math]::Round($bitmap.Width * $centers[$index][0])
                $centerY = [int][Math]::Round($bitmap.Height * $centers[$index][1])
                $brush = [System.Drawing.SolidBrush]::new($sphereColors[$index])
                try {
                    $graphics.FillEllipse(
                        $brush,
                        $centerX - $sphereRadius,
                        $centerY - $sphereRadius,
                        $sphereRadius * 2,
                        $sphereRadius * 2)
                }
                finally {
                    $brush.Dispose()
                }
            }

            # Place defects at checker sample locations on the first sphere.
            # Their four-neighbour contrast is intentionally local, so this
            # is not merely a broad roughness-ladder step.
            $knotBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(250, 250, 250))
            try {
                $firstCenterX = [int][Math]::Round($bitmap.Width * $centers[0][0])
                $firstCenterY = [int][Math]::Round($bitmap.Height * $centers[0][1])
                foreach ($offsetY in @(-50, 50)) {
                    $graphics.FillEllipse(
                        $knotBrush,
                        $firstCenterX - 7,
                        $firstCenterY + $offsetY - 7,
                        14,
                        14)
                }
            }
            finally {
                $knotBrush.Dispose()
            }
        }
        finally {
            $graphics.Dispose()
        }
        $bitmap.Save($negativeControlImage, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
    $metadata = 'CAPTURE_READY_IBL_REFERENCE mode=rendered view=close reference_layout=close_grid reference_texture_edge=128 reference_exposure_stops=0.0000 ibl_reference=1 ibl_diagnostic=none ibl_control=production_studio ibl_rotation_degrees=0.00 ibl_prefilter_lod_override=-1.00 ibl_direct_lighting=0 ibl_moon_lighting=0 ibl_roughness_ladder=1 ibl_roughness_samples=9 ibl_irradiance_resolution=32 ibl_prefilter_resolution=256 ibl_prefilter_levels=7 ibl_brdf_resolution=128 viewport=0,0,1280,720 aspect=1.777778 camera_position=0.0000,1.9000,0.7872 yaw=-1.570796 pitch=0.000000 roll=0.000000 fov=1.047198 reference_bounds=0.0000,1.9000,-2.8000,1.8500,1.7500,0.5000 reference_midpoint=640.00,360.00 reference_count=9 settled_frames=3 draw_expected=1'
    @('Same-camera viewport evidence', 'Evidence profile: PBR_IBL_REFERENCE', 'ibl_reference_rendered: ibl-reference-close-rendered.png', "ibl_reference_rendered metadata: $metadata") | Set-Content -LiteralPath $negativeControlIndex -Encoding UTF8
    $checkerFailure = $null
    try {
        & $checkerPath -InputDirectory $negativeControlRoot
    }
    catch {
        $checkerFailure = $_
    }
    if ($null -eq $checkerFailure) {
        throw 'IBL checker synthetic localized-knot negative control unexpectedly passed.'
    }
    if ($checkerFailure.Exception.Message -notmatch 'upper-center-lobes' -or
        $checkerFailure.Exception.Message -notmatch 'localized-bright-knots') {
        throw "IBL checker synthetic two-sided localized-knot negative control failed for an unexpected reason: $($checkerFailure.Exception.Message)"
    }
    $negativeControlPassed = $true
}
finally {
    if (Test-Path -LiteralPath $negativeControlRoot -PathType Container) {
        Remove-Item -LiteralPath $negativeControlRoot -Recurse -Force
    }
}
if (-not $negativeControlPassed) {
    throw 'IBL checker synthetic localized-knot negative control did not complete.'
}

Write-Output 'PBR IBL reference source contract test passed.'
Write-Output 'PBR IBL checker localized-knot negative control passed.'
