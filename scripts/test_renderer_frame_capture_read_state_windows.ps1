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

function Test-RendererFrameCaptureReadStateContract {
    param([string]$FunctionBody)

    $missing = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $FunctionBody) {
        $missing.Add('renderer end-frame function')
        return $missing
    }

    $capture = 'glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer)'
    $force = 'glReadBuffer(GL_BACK)'
    $readPixels = 'glReadPixels('
    $restore = 'glReadBuffer((GLenum)previous_read_buffer)'
    foreach($required in @($capture, $force, $restore)) {
        if ($FunctionBody.IndexOf($required, [System.StringComparison]::Ordinal) -lt 0) {
            $missing.Add($required)
        }
    }

    $captureIndex = $FunctionBody.IndexOf($capture, [System.StringComparison]::Ordinal)
    $forceIndex = $FunctionBody.IndexOf($force, [System.StringComparison]::Ordinal)
    $readPixelsIndex = $FunctionBody.IndexOf($readPixels, [System.StringComparison]::Ordinal)
    $restoreIndex = $FunctionBody.IndexOf($restore, [System.StringComparison]::Ordinal)
    if ($captureIndex -lt 0 -or $forceIndex -lt 0 -or $readPixelsIndex -lt 0 -or $restoreIndex -lt 0 -or
        -not ($captureIndex -lt $forceIndex -and $forceIndex -lt $readPixelsIndex -and $readPixelsIndex -lt $restoreIndex)) {
        $missing.Add('read-buffer capture, forced readback, and restoration ordering')
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$endFrame = Get-FunctionBody -Source $renderer -FunctionName 'henka_opengl_renderer_end_frame'
$missing = Test-RendererFrameCaptureReadStateContract -FunctionBody $endFrame
if ($missing.Count -gt 0) {
    throw "Renderer frame-capture read-buffer state contract failed: $($missing -join ', ')"
}

$negative = $endFrame.Replace(
    '        glReadBuffer((GLenum)previous_read_buffer);',
    '        /* deliberate negative-control omission */')
if ($negative -eq $endFrame) {
    throw 'Renderer frame-capture read-buffer negative control could not remove the restoration.'
}
$negativeMissing = Test-RendererFrameCaptureReadStateContract -FunctionBody $negative
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer frame-capture read-buffer negative control unexpectedly passed.'
}

Write-Output 'Renderer frame-capture read-buffer state contract passed.'
Write-Output 'Renderer frame-capture read-buffer state negative control passed.'
