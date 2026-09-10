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

function Test-RendererFramebufferStateContract {
    param([string]$Source)

    $build = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_build_ibl_resources'
    if ($null -eq $build) {
        return @('IBL resource build function')
    }

    $missing = [System.Collections.Generic.List[string]]::new()
    foreach ($capture in @(
        'glGetIntegerv\(GL_DRAW_BUFFER, &previous_draw_buffer\);',
        'glGetIntegerv\(GL_READ_BUFFER, &previous_read_buffer\);',
        'glGetIntegerv\(GL_CURRENT_PROGRAM, &previous_program\);',
        'glGetIntegerv\(GL_VERTEX_ARRAY_BINDING, &previous_vertex_array\);')) {
        if ($build -notmatch $capture) {
            $missing.Add("framebuffer state capture: $capture")
        }
    }
    if ([regex]::Matches($build, 'glDrawBuffer\(\(GLenum\)previous_draw_buffer\);').Count -lt 2) {
        $missing.Add('draw-buffer restoration on both success and failure')
    }
    if ([regex]::Matches($build, 'glReadBuffer\(\(GLenum\)previous_read_buffer\);').Count -lt 2) {
        $missing.Add('read-buffer restoration on both success and failure')
    }
    if ([regex]::Matches($build, 'g_gl\.UseProgram\(\(GLuint\)previous_program\);').Count -lt 2) {
        $missing.Add('program restoration on both success and failure')
    }
    if ([regex]::Matches($build, 'g_gl\.BindVertexArray\(\(GLuint\)previous_vertex_array\);').Count -lt 2) {
        $missing.Add('vertex-array restoration on both success and failure')
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$missing = Test-RendererFramebufferStateContract -Source $renderer
if ($missing.Count -gt 0) {
    throw "Renderer framebuffer-state contract failed: $($missing -join ', ')"
}

$negative = $renderer.Replace(
    '    glGetIntegerv(GL_DRAW_BUFFER, &previous_draw_buffer);',
    '    /* deliberate negative-control omission */')
$negative = $negative.Replace(
    '    glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer);',
    '    /* deliberate negative-control omission */')
$negativeMissing = Test-RendererFramebufferStateContract -Source $negative
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer framebuffer-state negative control unexpectedly passed.'
}

Write-Output 'Renderer framebuffer-state contract passed.'
Write-Output 'Renderer framebuffer-state negative control passed.'
