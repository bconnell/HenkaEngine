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

function Test-RendererFrameCaptureStateContract {
    param([string]$Source)

    $endFrame = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_renderer_end_frame'
    $missing = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $endFrame) {
        $missing.Add('renderer end-frame function')
        return $missing
    }

    if ($endFrame -notmatch 'glGetIntegerv\(\s*GL_PACK_ALIGNMENT\s*,\s*&previous_pack_alignment\s*\)') {
        $missing.Add('capture of the caller pack alignment')
    }
    if ($endFrame -notmatch 'glPixelStorei\(\s*GL_PACK_ALIGNMENT\s*,\s*1\s*\)') {
        $missing.Add('tightly packed frame readback')
    }
    if ($endFrame -notmatch 'glPixelStorei\(\s*GL_PACK_ALIGNMENT\s*,\s*previous_pack_alignment\s*\)') {
        $missing.Add('pack alignment restoration')
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$missing = Test-RendererFrameCaptureStateContract -Source $renderer
if ($missing.Count -gt 0) {
    throw "Renderer frame-capture state contract failed: $($missing -join ', ')"
}

$negative = $renderer.Replace(
    '    glPixelStorei(GL_PACK_ALIGNMENT, 1);',
    '    /* deliberate negative-control omission */')
if ($negative -eq $renderer) {
    throw 'Renderer frame-capture state negative control could not remove the pack-alignment setup.'
}
$negativeMissing = Test-RendererFrameCaptureStateContract -Source $negative
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer frame-capture state negative control unexpectedly passed.'
}

Write-Output 'Renderer frame-capture state contract passed.'
Write-Output 'Renderer frame-capture state negative control passed.'
