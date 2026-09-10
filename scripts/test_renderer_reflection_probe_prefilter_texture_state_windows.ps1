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

function Test-PrefilterTextureStateContract {
    param([string]$FunctionBody)

    $missing = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $FunctionBody) {
        $missing.Add('reflection-probe prefilter function')
        return $missing
    }

    foreach ($required in @(
        'henka_opengl_texture_binding_state texture_state',
        'henka_opengl_capture_texture_binding_state(&texture_state);',
        'g_gl.ActiveTexture(GL_TEXTURE0);',
        'henka_opengl_restore_texture_binding_state(&texture_state);')) {
        if ($FunctionBody.IndexOf($required, [System.StringComparison]::Ordinal) -lt 0) {
            $missing.Add($required)
        }
    }

    $captureIndex = $FunctionBody.IndexOf(
        'henka_opengl_capture_texture_binding_state(&texture_state);',
        [System.StringComparison]::Ordinal)
    $switchIndex = $FunctionBody.IndexOf(
        'g_gl.ActiveTexture(GL_TEXTURE0);',
        [System.StringComparison]::Ordinal)
    $restoreIndex = $FunctionBody.IndexOf(
        'henka_opengl_restore_texture_binding_state(&texture_state);',
        [System.StringComparison]::Ordinal)
    if ($captureIndex -lt 0 -or $switchIndex -lt 0 -or $restoreIndex -lt 0 -or
        -not ($captureIndex -lt $switchIndex -and $switchIndex -lt $restoreIndex)) {
        $missing.Add('texture-unit capture, GL_TEXTURE0 use, and restoration ordering')
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$prefilter = Get-FunctionBody -Source $renderer -FunctionName 'henka_opengl_prefilter_reflection_probe'
$missing = Test-PrefilterTextureStateContract -FunctionBody $prefilter
if ($missing.Count -gt 0) {
    throw "Reflection-probe prefilter texture-state contract failed: $($missing -join ', ')"
}

$negative = $prefilter.Replace(
    '    henka_opengl_restore_texture_binding_state(&texture_state);',
    '    /* deliberate negative-control omission */')
if ($negative -eq $prefilter) {
    throw 'Reflection-probe prefilter texture-state negative control could not remove restoration.'
}
$negativeMissing = Test-PrefilterTextureStateContract -FunctionBody $negative
if ($negativeMissing.Count -eq 0) {
    throw 'Reflection-probe prefilter texture-state negative control unexpectedly passed.'
}

Write-Output 'Reflection-probe prefilter texture-state contract passed.'
Write-Output 'Reflection-probe prefilter texture-state negative control passed.'
