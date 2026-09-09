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

function Test-RendererTextureStateContract {
    param([string]$Source)

    $missing = [System.Collections.Generic.List[string]]::new()
    $capture = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_capture_texture_binding_state'
    $restore = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_restore_texture_binding_state'
    $ibl = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_build_ibl_resources'
    $point = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_create_point_shadow_target'

    if ($null -eq $capture) {
        $missing.Add('texture binding state capture helper')
    } else {
        foreach ($pattern in @(
            'GL_ACTIVE_TEXTURE',
            'GL_TEXTURE_BINDING_2D',
            'GL_TEXTURE_BINDING_CUBE_MAP',
            'g_gl\.ActiveTexture\(GL_TEXTURE0\)'
        )) {
            if ($capture -notmatch $pattern) {
                $missing.Add("capture helper: $pattern")
            }
        }
    }

    if ($null -eq $restore) {
        $missing.Add('texture binding state restore helper')
    } else {
        foreach ($pattern in @(
            'g_gl\.ActiveTexture\(GL_TEXTURE0\)',
            'glBindTexture\(GL_TEXTURE_2D',
            'glBindTexture\(GL_TEXTURE_CUBE_MAP',
            'previous_active_texture',
            'previous_texture_2d',
            'previous_cube_texture'
        )) {
            if ($restore -notmatch $pattern) {
                $missing.Add("restore helper: $pattern")
            }
        }
    }

    foreach ($entry in @(
        @{ Name = 'IBL derivation'; Body = $ibl },
        @{ Name = 'point-shadow target allocation'; Body = $point }
    )) {
        if ($null -eq $entry.Body) {
            $missing.Add("$($entry.Name) function")
            continue
        }
        if ($entry.Body -notmatch 'henka_opengl_capture_texture_binding_state\(') {
            $missing.Add("$($entry.Name): capture call")
        }
        if ([regex]::Matches($entry.Body, 'henka_opengl_restore_texture_binding_state\(').Count -lt 2) {
            $missing.Add("$($entry.Name): restore on success and failure")
        }
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$missing = Test-RendererTextureStateContract -Source $renderer
if ($missing.Count -gt 0) {
    throw "Renderer temporary texture-state contract failed: $($missing -join ', ')"
}

$negative = $renderer.Replace(
    '    henka_opengl_restore_texture_binding_state(&texture_state);',
    '    /* deliberate negative-control omission */')
$negativeMissing = Test-RendererTextureStateContract -Source $negative
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer temporary texture-state negative control unexpectedly passed.'
}

Write-Output 'Renderer temporary texture-state contract passed.'
Write-Output 'Renderer temporary texture-state negative control passed.'
