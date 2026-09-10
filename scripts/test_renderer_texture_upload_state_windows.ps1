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

function Test-TextureUploadFunction {
    param(
        [string]$Name,
        [string]$Body
    )

    $missing = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $Body) {
        $missing.Add("$Name function")
        return $missing
    }

    foreach ($pattern in @(
        'previous_active_texture',
        'previous_texture_binding',
        'previous_unpack_alignment',
        'GL_ACTIVE_TEXTURE',
        'g_gl\.ActiveTexture\(GL_TEXTURE0\)',
        'GL_TEXTURE_BINDING_2D',
        'GL_UNPACK_ALIGNMENT',
        'glPixelStorei\(GL_UNPACK_ALIGNMENT, 1\)',
        'restore_result\s*=\s*henka_opengl_restore_texture_binding\('
    )) {
        if ($Body -notmatch $pattern) {
            $missing.Add("${Name}: $pattern")
        }
    }

    $captureMatch = [regex]::Match(
        $Body,
        'glGetIntegerv\s*\(\s*GL_UNPACK_ALIGNMENT\s*,')
    $captureIndex = $captureMatch.Index
    $uploadIndex = $Body.IndexOf('glPixelStorei(GL_UNPACK_ALIGNMENT, 1)')
    $restoreMatch = [regex]::Match(
        $Body,
        'restore_result\s*=\s*henka_opengl_restore_texture_binding\s*\(')
    $restoreIndex = $restoreMatch.Index
    if ($captureIndex -lt 0 -or $uploadIndex -le $captureIndex -or $restoreIndex -le $uploadIndex) {
        $missing.Add("${Name}: capture, upload, and restore order")
    }

    if ($Body -notmatch '(?s)henka_opengl_restore_texture_binding\(\s*previous_active_texture\s*,\s*previous_texture_binding\s*,\s*previous_unpack_alignment\s*\)') {
        $missing.Add("${Name}: complete binding/alignment restoration arguments")
    }

    return $missing
}

function Test-RendererTextureUploadStateContract {
    param([string]$Source)

    $missing = [System.Collections.Generic.List[string]]::new()
    $pixelUpload = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_create_texture_from_pixels'
    $ktxUpload = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_renderer_create_texture_from_ktx2_memory_with_mip_limit'

    foreach ($entry in @(
        @{ Name = 'decoded pixel upload'; Body = $pixelUpload },
        @{ Name = 'KTX2 upload'; Body = $ktxUpload }
    )) {
        foreach ($failure in (Test-TextureUploadFunction -Name $entry.Name -Body $entry.Body)) {
            $missing.Add($failure)
        }
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$missing = Test-RendererTextureUploadStateContract -Source $renderer
if ($missing.Count -gt 0) {
    throw "Renderer texture-upload state contract failed: $($missing -join ', ')"
}

$negative = $renderer.Replace(
    '    restore_result = henka_opengl_restore_texture_binding(',
    '    /* deliberate negative-control omission */')
if ($negative -eq $renderer) {
    throw 'Renderer texture-upload state negative control did not alter the source.'
}
$negativeMissing = Test-RendererTextureUploadStateContract -Source $negative
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer texture-upload state negative control unexpectedly passed.'
}

Write-Output 'Renderer texture-upload state contract passed.'
Write-Output 'Renderer texture-upload state negative control passed.'
