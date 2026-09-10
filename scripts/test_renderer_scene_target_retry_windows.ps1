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
        "(?smi)^[\t ]*(?:static[\t ]+)?[A-Za-z_][A-Za-z0-9_\t ]*\b$([regex]::Escape($FunctionName))\s*\([^;]*\)\s*\{")
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

function Test-RendererSceneTargetRetryContract {
    param([string]$Source)

    $draw = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_renderer_draw_scene'
    $sync = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_renderer_sync_scene_target'
    $ready = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_scene_target_is_ready'
    $hdrReady = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_renderer_is_hdr_ready'
    $missing = [System.Collections.Generic.List[string]]::new()

    if ($null -eq $draw) {
        $missing.Add('OpenGL scene draw function')
    }
    if ($null -eq $sync) {
        $missing.Add('scene-target synchronization function')
    }
    if ($null -eq $ready) {
        $missing.Add('shared scene-target readiness helper')
    }
    if ($null -eq $hdrReady) {
        $missing.Add('HDR readiness helper')
    }
    if ($missing.Count -gt 0) {
        return $missing
    }

    if ($ready -notmatch 'henka_opengl_scene_target_requires_sync\s*\(') {
        $missing.Add('readiness helper delegates to the complete target policy')
    }
    if ($sync -notmatch 'henka_opengl_scene_target_is_ready\s*\(\s*state\s*,\s*viewport\s*\)') {
        $missing.Add('synchronization uses the shared readiness helper')
    }
    if ($sync -notmatch 'henka_opengl_scene_target_requires_hdr_sync\s*\(\s*&policy\s*\)' -or
        $sync -notmatch 'henka_opengl_scene_target_requires_bloom_sync\s*\(\s*&policy\s*\)' -or
        $sync -notmatch 'henka_opengl_scene_target_requires_temporal_sync\s*\(\s*&policy\s*\)') {
        $missing.Add('synchronization retries only the scene targets that are unavailable or stale')
    }
    if ($hdrReady -notmatch 'henka_renderer_get_scene_viewport\s*\(\s*renderer\s*\)' -or
        $hdrReady -notmatch 'hdr_width\s*==\s*viewport\.width' -or
        $hdrReady -notmatch 'hdr_height\s*==\s*viewport\.height') {
        $missing.Add('HDR readiness requires a complete target matching the active Scene View dimensions')
    }
    if ($draw -notmatch 'henka_opengl_scene_target_is_ready\s*\(\s*state\s*,\s*scene_viewport\s*\)\s*\)\s*\{\s*henka_opengl_renderer_sync_scene_target\s*\(\s*renderer\s*\)\s*;') {
        $missing.Add('draw path retries every incomplete scene target, not only HDR dimensions')
    }
    if ($draw -match 'state->hdr_width\s*!=\s*scene_viewport\.width') {
        $missing.Add('draw path retains the HDR-only synchronization guard')
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$missing = Test-RendererSceneTargetRetryContract -Source $renderer
if ($missing.Count -gt 0) {
    throw "Renderer scene-target retry contract failed: $($missing -join ', ')"
}

$drawBody = Get-FunctionBody -Source $renderer -FunctionName 'henka_opengl_renderer_draw_scene'
$negativeDrawBody = $drawBody.Replace(
    'henka_opengl_renderer_sync_scene_target(renderer);',
    '/* deliberate negative-control omission */')
if ($negativeDrawBody -eq $drawBody) {
    throw 'Renderer scene-target retry negative control could not remove the draw retry.'
}
$negative = $renderer.Replace($drawBody, $negativeDrawBody)
$negativeMissing = Test-RendererSceneTargetRetryContract -Source $negative
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer scene-target retry negative control unexpectedly passed.'
}

$syncBody = Get-FunctionBody -Source $renderer -FunctionName 'henka_opengl_renderer_sync_scene_target'
$negativeSyncBody = $syncBody.Replace(
    'if (henka_opengl_scene_target_requires_hdr_sync(&policy))',
    'if (true)')
if ($negativeSyncBody -eq $syncBody) {
    throw 'Renderer scene-target per-target retry negative control could not remove the HDR policy guard.'
}
$negativeSyncRenderer = $renderer.Replace($syncBody, $negativeSyncBody)
$negativeSyncMissing = Test-RendererSceneTargetRetryContract -Source $negativeSyncRenderer
if ($negativeSyncMissing.Count -eq 0) {
    throw 'Renderer scene-target per-target retry negative control unexpectedly passed.'
}

$hdrReadyBody = Get-FunctionBody -Source $renderer -FunctionName 'henka_opengl_renderer_is_hdr_ready'
$negativeHdrReadyBody = $hdrReadyBody -replace
    '(?s)\s*&&\s*state->hdr_width\s*==\s*viewport\.width\s*&&\s*state->hdr_height\s*==\s*viewport\.height',
    ' /* deliberate negative-control omission */'
if ($negativeHdrReadyBody -eq $hdrReadyBody) {
    throw 'Renderer HDR readiness negative control could not remove the viewport match.'
}
$negativeHdrReadyRenderer = $renderer.Replace($hdrReadyBody, $negativeHdrReadyBody)
$negativeHdrReadyMissing = Test-RendererSceneTargetRetryContract -Source $negativeHdrReadyRenderer
if ($negativeHdrReadyMissing.Count -eq 0) {
    throw 'Renderer HDR readiness negative control unexpectedly passed.'
}

Write-Output 'Renderer scene-target retry contract passed.'
Write-Output 'Renderer scene-target retry negative control passed.'
Write-Output 'Renderer scene-target per-target retry negative control passed.'
Write-Output 'Renderer HDR readiness dimension negative control passed.'
