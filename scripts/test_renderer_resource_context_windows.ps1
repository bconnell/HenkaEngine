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
        "(?s)(?:static\s+)?[^\r\n;]*\b$([regex]::Escape($FunctionName))\s*\([^;]*\)\s*\{")
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

function Test-ContextGuardedDeletion {
    param(
        [string]$Body,
        [string]$DeletePattern,
        [string]$ResourceName
    )

    if ($null -eq $Body) {
        return @("$ResourceName destruction function")
    }

    $missing = [System.Collections.Generic.List[string]]::new()
    foreach ($pattern in @(
        'henka_opengl_renderer_state\* state',
        'henka_opengl_texture_context_guard context_guard',
        'henka_result context_result',
        'henka_opengl_begin_texture_context',
        'henka_opengl_end_texture_context',
        'context_result == HENKA_SUCCESS'
    )) {
        if ($Body -notmatch $pattern) {
            $missing.Add($pattern)
        }
    }

    $begin = $Body.IndexOf('henka_opengl_begin_texture_context')
    $firstDelete = [regex]::Match($Body, $DeletePattern).Index
    $lastDeleteMatch = [regex]::Matches($Body, $DeletePattern)
    $lastDelete = if ($lastDeleteMatch.Count -gt 0) {
        $lastDeleteMatch[$lastDeleteMatch.Count - 1].Index
    } else {
        -1
    }
    $end = $Body.LastIndexOf('henka_opengl_end_texture_context')
    if ($begin -lt 0 -or $firstDelete -lt 0 -or $lastDelete -lt 0 -or $end -lt 0 -or
        $begin -ge $firstDelete -or $end -le $lastDelete) {
        $missing.Add("context guard surrounds $ResourceName GL deletion")
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw

$meshBody = Get-FunctionBody -Source $renderer -FunctionName 'henka_opengl_renderer_destroy_mesh'
$shaderBody = Get-FunctionBody -Source $renderer -FunctionName 'henka_opengl_renderer_destroy_shader'
$missing = [System.Collections.Generic.List[string]]::new()
foreach ($item in (Test-ContextGuardedDeletion -Body $meshBody -DeletePattern 'g_gl\.Delete(?:Buffers|VertexArrays)' -ResourceName 'mesh')) {
    $missing.Add([string]$item)
}
foreach ($item in (Test-ContextGuardedDeletion -Body $shaderBody -DeletePattern 'g_gl\.DeleteProgram' -ResourceName 'shader')) {
    $missing.Add([string]$item)
}
if ($missing.Count -gt 0) {
    throw "Renderer resource context contract failed: $($missing -join ', ')"
}

$negative = $renderer.Replace(
    'henka_opengl_begin_texture_context(',
    '/* deliberate negative-control omission */(')
if ($negative -eq $renderer) {
    throw 'Renderer resource context negative control did not alter the source.'
}
$negativeMesh = Get-FunctionBody -Source $negative -FunctionName 'henka_opengl_renderer_destroy_mesh'
$negativeShader = Get-FunctionBody -Source $negative -FunctionName 'henka_opengl_renderer_destroy_shader'
$negativeMissing = [System.Collections.Generic.List[string]]::new()
foreach ($item in (Test-ContextGuardedDeletion -Body $negativeMesh -DeletePattern 'g_gl\.Delete(?:Buffers|VertexArrays)' -ResourceName 'mesh')) {
    $negativeMissing.Add([string]$item)
}
foreach ($item in (Test-ContextGuardedDeletion -Body $negativeShader -DeletePattern 'g_gl\.DeleteProgram' -ResourceName 'shader')) {
    $negativeMissing.Add([string]$item)
}
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer resource context negative control unexpectedly passed.'
}

Write-Output 'Renderer resource context contract passed.'
Write-Output 'Renderer resource context negative control passed.'
