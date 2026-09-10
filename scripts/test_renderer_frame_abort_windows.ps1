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

function Test-RendererFrameAbortContract {
    param([string]$Source)

    $helper = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_reset_frame_texture_bindings'
    $abort = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_renderer_abort_frame'
    $missing = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $helper) {
        $missing.Add('shared frame texture-binding reset helper')
    } else {
        if ($helper -notmatch 'HENKA_OPENGL_FRAME_TEXTURE_UNIT_COUNT') {
            $missing.Add('bounded renderer texture-unit count')
        }
        if ($helper -notmatch 'GL_TEXTURE_2D' -or $helper -notmatch 'GL_TEXTURE_CUBE_MAP') {
            $missing.Add('2D and cubemap binding cleanup')
        }
        if ($helper -notmatch 'GL_TEXTURE0\s*\+\s*\(GLenum\)unit') {
            $missing.Add('deterministic texture-unit traversal')
        }
        if ($helper -notmatch 'g_gl\.ActiveTexture\(GL_TEXTURE0\);') {
            $missing.Add('canonical active texture reset')
        }
    }
    if ($null -eq $abort -or $abort -notmatch 'henka_opengl_reset_frame_texture_bindings\(\);') {
        $missing.Add('abort-frame texture-binding cleanup')
    }
    if ([regex]::Matches($Source, 'henka_opengl_reset_frame_texture_bindings\(\);').Count -lt 2) {
        $missing.Add('normal and abort frame cleanup use the same helper')
    }
    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$missing = Test-RendererFrameAbortContract -Source $renderer
if ($missing.Count -gt 0) {
    throw "Renderer frame-abort cleanup contract failed: $($missing -join ', ')"
}

$negative = $renderer.Replace(
    '    henka_opengl_reset_frame_texture_bindings();',
    '    /* deliberate negative-control omission */')
if ($negative -eq $renderer) {
    throw 'Renderer frame-abort cleanup negative control could not remove the helper call.'
}
$negativeMissing = Test-RendererFrameAbortContract -Source $negative
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer frame-abort cleanup negative control unexpectedly passed.'
}

Write-Output 'Renderer frame-abort cleanup contract passed.'
Write-Output 'Renderer frame-abort cleanup negative control passed.'
