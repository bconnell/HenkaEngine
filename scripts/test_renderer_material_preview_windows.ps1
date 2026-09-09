param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

function Get-FunctionBody {
    param(
        [string]$Source,
        [string]$FunctionName
    )

    $signature = [regex]::Match(
        $Source,
        "(?ms)^\s*(?:static\s+)?[\w\s*]+\b$([regex]::Escape($FunctionName))\s*\([^;]*\)\s*\{")
    if (-not $signature.Success) {
        return $null
    }

    $openBrace = $Source.IndexOf('{', $signature.Index)
    $depth = 0
    for ($index = $openBrace; $index -lt $Source.Length; ++$index) {
        if ($Source[$index] -eq '{') {
            ++$depth
        } elseif ($Source[$index] -eq '}') {
            --$depth
            if ($depth -eq 0) {
                return $Source.Substring($signature.Index, $index - $signature.Index + 1)
            }
        }
    }

    return $null
}

function Test-RendererMaterialPreviewContract {
    param(
        [string]$PolicySource,
        [string]$DrawSource
    )

    $missing = [System.Collections.Generic.List[string]]::new()
    $policy = Get-FunctionBody -Source $PolicySource -FunctionName 'henka_viewport_render_policy_resolve'
    $draw = Get-FunctionBody -Source $DrawSource -FunctionName 'henka_opengl_renderer_draw_scene'
    if ($null -eq $policy) {
        $missing.Add('viewport render policy function')
    }
    if ($null -eq $draw) {
        $missing.Add('OpenGL scene draw function')
    }
    if ($missing.Count -gt 0) {
        return $missing
    }

    $previewCase = [regex]::Match(
        $policy,
        '(?s)case\s+HENKA_VIEWPORT_SHADING_MATERIAL_PREVIEW\s*:\s*(?<body>.*?)case\s+HENKA_VIEWPORT_SHADING_RENDERED\s*:').Groups['body'].Value
    $renderedCase = [regex]::Match(
        $policy,
        '(?s)case\s+HENKA_VIEWPORT_SHADING_RENDERED\s*:\s*(?<body>.*?)case\s+HENKA_VIEWPORT_SHADING_COUNT\s*:').Groups['body'].Value
    if ([string]::IsNullOrWhiteSpace($previewCase)) {
        $missing.Add('Material Preview policy case')
    } elseif ($previewCase -notmatch 'policy\.use_scene_environment\s*=\s*false\s*;') {
        $missing.Add('Material Preview disables scene environment')
    }
    if ([string]::IsNullOrWhiteSpace($renderedCase)) {
        $missing.Add('Rendered policy case')
    } elseif ($renderedCase -notmatch 'policy\.use_scene_environment\s*=\s*true\s*;') {
        $missing.Add('Rendered enables scene environment')
    }

    foreach ($pattern in @(
        'if\s*\(\s*!policy\.use_scene_lighting\s*\)\s*\{\s*continue\s*;',
        'use_reflection_probe\s*=\s*policy\.use_scene_environment\s*&&',
        'if\s*\(\s*policy\.use_scene_environment\s*\)\s*\{\s*henka_opengl_capture_next_reflection_probe\s*\(\s*renderer\s*,\s*scene\s*\)\s*;',
        '"useEnvironment"\s*,\s*policy\.use_scene_environment\s*&&',
        '"useEnvironmentTexture"\s*,\s*policy\.use_scene_environment\s*&&',
        '"useIBL"\s*,\s*policy\.use_scene_environment\s*&&'
    )) {
        if ($draw -notmatch $pattern) {
            $missing.Add("scene-preview consumer: $pattern")
        }
    }

    return $missing
}

$policyPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer.c'
$drawPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$policySource = Get-Content -LiteralPath $policyPath -Raw
$drawSource = Get-Content -LiteralPath $drawPath -Raw
$missing = Test-RendererMaterialPreviewContract -PolicySource $policySource -DrawSource $drawSource
if ($missing.Count -gt 0) {
    throw "Renderer Material Preview contract failed: $($missing -join ', ')"
}

$negativeDraw = $drawSource.Replace(
    'policy.use_scene_environment &&',
    '/* deliberate negative-control omission */')
$negativeMissing = Test-RendererMaterialPreviewContract -PolicySource $policySource -DrawSource $negativeDraw
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer Material Preview negative control unexpectedly passed.'
}

Write-Output 'Renderer Material Preview contract passed.'
Write-Output 'Renderer Material Preview negative control passed.'
