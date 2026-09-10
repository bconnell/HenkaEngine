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

function Test-TemporalFailureContract {
    param([string]$Source)

    $create = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_create_temporal_history'
    $failure = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_mark_temporal_history_failure'
    $missing = [System.Collections.Generic.List[string]]::new()

    if ($null -eq $create) {
        $missing.Add('temporal-history creation function')
    }
    if ($null -eq $failure) {
        $missing.Add('shared temporal-history failure transition')
    }
    if ($missing.Count -gt 0) {
        return $missing
    }

    if ($failure -notmatch 'temporal_history_valid\s*=\s*false' -or
        $failure -notmatch 'temporal_fallback_active\s*=\s*true' -or
        $failure -notmatch 'temporal_previous_history_retained\s*=\s*previous_history_ready') {
        $missing.Add('failure transition invalidates accumulation, enables fallback, and records prior-resource retention')
    }
    if ($create -notmatch 'if\s*\(texture\s*==\s*0U\s*\|\|\s*depth_texture\s*==\s*0U\s*\|\|\s*depth_framebuffer\s*==\s*0U\s*\)\s*\{(?s:.*?)henka_opengl_mark_temporal_history_failure\s*\(') {
        $missing.Add('GPU-object allocation failure uses the failure transition')
    }
    if ($create -notmatch 'if\s*\(texture_error\s*!=\s*GL_NO_ERROR\)\s*\{(?s:.*?)henka_opengl_mark_temporal_history_failure\s*\(') {
        $missing.Add('texture or framebuffer failure uses the failure transition')
    }
    if ($create -notmatch 'GLenum\s+depth_texture_error\s*;' -or
        $create -notmatch 'depth_texture_error\s*=\s*glGetError\(\)\s*;' -or
        $create -notmatch 'texture_error\s*=\s*texture_error\s*==\s*GL_NO_ERROR\s*\?\s*depth_texture_error\s*:\s*texture_error\s*;') {
        $missing.Add('depth texture allocation always drains the GL error queue while preserving the first allocation error')
    }
    if ($create -match 'henka_opengl_delete_temporal_history\s*\(state\)\s*;(?s:.*?)if\s*\(texture_error\s*!=\s*GL_NO_ERROR\)') {
        $missing.Add('failed replacement retires no prior temporal history')
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$missing = Test-TemporalFailureContract -Source $renderer
if ($missing.Count -gt 0) {
    throw "Renderer temporal failure contract failed: $($missing -join ', ')"
}

$createBody = Get-FunctionBody -Source $renderer -FunctionName 'henka_opengl_create_temporal_history'
$negativeCreate = $createBody.Replace(
    'henka_opengl_mark_temporal_history_failure(',
    '/* deliberate negative-control omission */(')
if ($negativeCreate -eq $createBody) {
    throw 'Renderer temporal failure negative control could not remove the failure transition.'
}
$negativeRenderer = $renderer.Replace($createBody, $negativeCreate)
$negativeMissing = Test-TemporalFailureContract -Source $negativeRenderer
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer temporal failure negative control unexpectedly passed.'
}

$failureBody = Get-FunctionBody -Source $renderer -FunctionName 'henka_opengl_mark_temporal_history_failure'
$negativeFailure = $failureBody.Replace(
    'state->temporal_history_valid = false;',
    '/* deliberate negative-control omission */')
if ($negativeFailure -eq $failureBody) {
    throw 'Renderer temporal failure state negative control could not remove invalidation.'
}
$negativeFailureRenderer = $renderer.Replace($failureBody, $negativeFailure)
$negativeFailureMissing = Test-TemporalFailureContract -Source $negativeFailureRenderer
if ($negativeFailureMissing.Count -eq 0) {
    throw 'Renderer temporal failure state negative control unexpectedly passed.'
}

$negativeErrorDrain = $renderer.Replace(
    '    depth_texture_error = glGetError();',
    '    /* deliberate negative-control omission */')
if ($negativeErrorDrain -eq $renderer) {
    throw 'Renderer temporal error-drain negative control could not remove the second error read.'
}
$negativeErrorDrainMissing = Test-TemporalFailureContract -Source $negativeErrorDrain
if ($negativeErrorDrainMissing.Count -eq 0) {
    throw 'Renderer temporal error-drain negative control unexpectedly passed.'
}

Write-Output 'Renderer temporal failure contract passed.'
Write-Output 'Renderer temporal failure transition negative control passed.'
Write-Output 'Renderer temporal invalidation negative control passed.'
Write-Output 'Renderer temporal GL error-drain negative control passed.'
