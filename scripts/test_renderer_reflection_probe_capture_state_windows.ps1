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

function Test-ReflectionProbeCaptureStateContract {
    param([string]$FunctionBody)

    $missing = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $FunctionBody) {
        $missing.Add('reflection-probe capture function')
        return $missing
    }

    foreach ($capture in @(
        'glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);',
        'glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vertex_array);',
        'glGetIntegerv(GL_DRAW_BUFFER, &previous_draw_buffer);',
        'glGetIntegerv(GL_READ_BUFFER, &previous_read_buffer);',
        'glGetIntegerv(GL_POLYGON_MODE, previous_polygon_mode);',
        'previous_scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);',
        'previous_depth_enabled = glIsEnabled(GL_DEPTH_TEST);',
        'previous_cull_enabled = glIsEnabled(GL_CULL_FACE);',
        'previous_blend_enabled = glIsEnabled(GL_BLEND);',
        'glGetBooleanv(GL_DEPTH_WRITEMASK, &previous_depth_mask);')) {
        if ($FunctionBody.IndexOf($capture, [System.StringComparison]::Ordinal) -lt 0) {
            $missing.Add("state capture: $capture")
        }
    }

    foreach ($restore in @(
        'glDrawBuffer((GLenum)previous_draw_buffer);',
        'glReadBuffer((GLenum)previous_read_buffer);',
        'g_gl.UseProgram((GLuint)previous_program);',
        'g_gl.BindVertexArray((GLuint)previous_vertex_array);',
        'glPolygonMode(GL_FRONT, (GLenum)previous_polygon_mode[0]);',
        'glPolygonMode(GL_BACK, (GLenum)previous_polygon_mode[1]);',
        'glDepthMask(previous_depth_mask);')) {
        if ($FunctionBody.IndexOf($restore, [System.StringComparison]::Ordinal) -lt 0) {
            $missing.Add("state restoration: $restore")
        }
    }

    foreach ($capability in @(
        'previous_scissor_enabled',
        'previous_depth_enabled',
        'previous_cull_enabled',
        'previous_blend_enabled')) {
        if ($FunctionBody.IndexOf("if ($capability)", [System.StringComparison]::Ordinal) -lt 0 -or
            $FunctionBody.IndexOf("glDisable(", [System.StringComparison]::Ordinal) -lt 0) {
            $missing.Add("capability restoration: $capability")
        }
    }

    $drawSceneIndex = $FunctionBody.IndexOf(
        'henka_opengl_renderer_draw_scene(renderer, &capture_scene)',
        [System.StringComparison]::Ordinal)
    $programRestoreIndex = $FunctionBody.IndexOf(
        'g_gl.UseProgram((GLuint)previous_program);',
        [System.StringComparison]::Ordinal)
    $vertexRestoreIndex = $FunctionBody.IndexOf(
        'g_gl.BindVertexArray((GLuint)previous_vertex_array);',
        [System.StringComparison]::Ordinal)
    if ($drawSceneIndex -lt 0 -or $programRestoreIndex -lt 0 -or
        $vertexRestoreIndex -lt 0 -or $programRestoreIndex -lt $drawSceneIndex -or
        $vertexRestoreIndex -lt $drawSceneIndex) {
        $missing.Add('program and vertex-array restoration after recursive capture rendering')
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$capture = Get-FunctionBody -Source $renderer -FunctionName 'henka_opengl_capture_next_reflection_probe'
$missing = Test-ReflectionProbeCaptureStateContract -FunctionBody $capture
if ($missing.Count -gt 0) {
    throw "Reflection-probe capture state contract failed: $($missing -join ', ')"
}

$negative = $capture.Replace(
    '    g_gl.UseProgram((GLuint)previous_program);',
    '    /* deliberate negative-control omission */')
if ($negative -eq $capture) {
    throw 'Reflection-probe capture state negative control could not remove program restoration.'
}
$negativeMissing = Test-ReflectionProbeCaptureStateContract -FunctionBody $negative
if ($negativeMissing.Count -eq 0) {
    throw 'Reflection-probe capture state negative control unexpectedly passed.'
}

Write-Output 'Reflection-probe capture state contract passed.'
Write-Output 'Reflection-probe capture state negative control passed.'
