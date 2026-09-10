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

function Test-MeshStateContract {
    param([string]$Source)

    $body = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_renderer_create_mesh_from_data'
    if ($null -eq $body) {
        return @('mesh creation function')
    }

    $missing = [System.Collections.Generic.List[string]]::new()
    foreach ($pattern in @(
        'previous_array_buffer',
        'previous_vertex_array',
        'GL_ARRAY_BUFFER_BINDING',
        'GL_VERTEX_ARRAY_BINDING',
        'g_gl\.BindVertexArray\(mesh_data->vao\)',
        'g_gl\.BindBuffer\(GL_ARRAY_BUFFER, mesh_data->vertex_buffer\)',
        'g_gl\.BindVertexArray\(\(GLuint\)previous_vertex_array\)',
        'g_gl\.BindBuffer\(GL_ARRAY_BUFFER, \(GLuint\)previous_array_buffer\)'
    )) {
        if ($body -notmatch $pattern) {
            $missing.Add($pattern)
        }
    }

    $captureArray = $body.IndexOf('GL_ARRAY_BUFFER_BINDING')
    $captureVertex = $body.IndexOf('GL_VERTEX_ARRAY_BINDING')
    $bindVertex = $body.IndexOf('g_gl.BindVertexArray(mesh_data->vao)')
    $restoreVertex = $body.IndexOf('g_gl.BindVertexArray((GLuint)previous_vertex_array)')
    $restoreArray = $body.IndexOf('g_gl.BindBuffer(GL_ARRAY_BUFFER, (GLuint)previous_array_buffer)')
    if ($captureArray -lt 0 -or
        $captureVertex -lt 0 -or
        $bindVertex -le $captureVertex -or
        $restoreVertex -le $bindVertex -or
        $restoreArray -le $restoreVertex) {
        $missing.Add('capture, bind, and restore order')
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$missing = Test-MeshStateContract -Source $renderer
if ($missing.Count -gt 0) {
    throw "Renderer mesh state contract failed: $($missing -join ', ')"
}

$negative = $renderer.Replace(
    '    g_gl.BindVertexArray((GLuint)previous_vertex_array);',
    '    /* deliberate negative-control omission */')
if ($negative -eq $renderer) {
    throw 'Renderer mesh state negative control did not alter the source.'
}
$negativeMissing = Test-MeshStateContract -Source $negative
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer mesh state negative control unexpectedly passed.'
}

Write-Output 'Renderer mesh state contract passed.'
Write-Output 'Renderer mesh state negative control passed.'
