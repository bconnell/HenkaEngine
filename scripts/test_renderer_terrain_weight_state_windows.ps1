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

function Test-TerrainWeightStateContract {
    param([string]$Source)

    $body = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_renderer_set_terrain_weights'
    if ($null -eq $body) {
        return @('terrain weight update function')
    }

    $missing = [System.Collections.Generic.List[string]]::new()
    foreach ($pattern in @(
        'previous_array_buffer',
        'previous_vertex_array',
        'GL_ARRAY_BUFFER_BINDING',
        'GL_VERTEX_ARRAY_BINDING',
        'g_gl\.BindVertexArray\(mesh_data->vao\)',
        'g_gl\.BindBuffer\(GL_ARRAY_BUFFER, candidate_buffer\)',
        'g_gl\.BindVertexArray\(\(GLuint\)previous_vertex_array\)',
        'g_gl\.BindBuffer\(GL_ARRAY_BUFFER, \(GLuint\)previous_array_buffer\)'
    )) {
        if ($body -notmatch $pattern) {
            $missing.Add($pattern)
        }
    }

    if ($body -match 'g_gl\.BindVertexArray\(0U\)' -or
        $body -match 'g_gl\.BindBuffer\(GL_ARRAY_BUFFER, 0U\)') {
        $missing.Add('no hard-coded zero state restoration')
    }

    $captureArray = $body.IndexOf('GL_ARRAY_BUFFER_BINDING')
    $captureVertex = $body.IndexOf('GL_VERTEX_ARRAY_BINDING')
    $firstMutation = $body.IndexOf('g_gl.GenBuffers')
    $bindVertex = $body.IndexOf('g_gl.BindVertexArray(mesh_data->vao)')
    $restoreVertex = $body.LastIndexOf('g_gl.BindVertexArray((GLuint)previous_vertex_array)')
    $restoreArray = $body.LastIndexOf('g_gl.BindBuffer(GL_ARRAY_BUFFER, (GLuint)previous_array_buffer)')
    if ($captureArray -lt 0 -or
        $captureVertex -lt 0 -or
        $firstMutation -le $captureArray -or
        $bindVertex -le $firstMutation -or
        $restoreVertex -le $bindVertex -or
        $restoreArray -le $restoreVertex) {
        $missing.Add('capture before mutation and restore after binding')
    }

    if (($body.Split('g_gl.BindVertexArray((GLuint)previous_vertex_array)').Count - 1) -lt 3 -or
        ($body.Split('g_gl.BindBuffer(GL_ARRAY_BUFFER, (GLuint)previous_array_buffer)').Count - 1) -lt 3) {
        $missing.Add('restore on upload failure, attribute failure, and success')
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$missing = Test-TerrainWeightStateContract -Source $renderer
if ($missing.Count -gt 0) {
    throw "Renderer terrain weight state contract failed: $($missing -join ', ')"
}

$negative = $renderer.Replace(
    '    g_gl.BindVertexArray((GLuint)previous_vertex_array);',
    '    /* deliberate negative-control omission */')
if ($negative -eq $renderer) {
    throw 'Renderer terrain weight state negative control did not alter the source.'
}
$negativeMissing = Test-TerrainWeightStateContract -Source $negative
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer terrain weight state negative control unexpectedly passed.'
}

Write-Output 'Renderer terrain weight state contract passed.'
Write-Output 'Renderer terrain weight state negative control passed.'
