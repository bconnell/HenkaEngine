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

function Get-BracedBlock {
    param(
        [string]$Source,
        [int]$StartIndex
    )

    if ($StartIndex -lt 0) {
        return $null
    }
    $openBrace = $Source.IndexOf('{', $StartIndex)
    if ($openBrace -lt 0) {
        return $null
    }

    $depth = 0
    for ($index = $openBrace; $index -lt $Source.Length; ++$index) {
        if ($Source[$index] -eq '{') {
            ++$depth
        } elseif ($Source[$index] -eq '}') {
            --$depth
            if ($depth -eq 0) {
                return $Source.Substring($StartIndex, $index - $StartIndex + 1)
            }
        }
    }

    return $null
}

function Test-AllocationFailureState {
    param(
        [string]$Name,
        [string]$Body,
        [string]$AllocationMarker,
        [string[]]$Patterns
    )

    $missing = [System.Collections.Generic.List[string]]::new()
    if ($null -eq $Body) {
        $missing.Add("$Name function")
        return $missing
    }

    $markerIndex = $Body.IndexOf($AllocationMarker)
    $block = Get-BracedBlock -Source $Body -StartIndex $markerIndex
    if ($null -eq $block) {
        $missing.Add("$Name allocation-failure branch")
        return $missing
    }

    foreach ($pattern in $Patterns) {
        if ($block -notmatch $pattern) {
            $missing.Add("$Name allocation-failure branch: $pattern")
        }
    }
    return $missing
}

function Test-RendererTargetFailureStateContract {
    param([string]$Source)

    $missing = [System.Collections.Generic.List[string]]::new()
    $common = @(
        'g_gl\.BindFramebuffer\(GL_FRAMEBUFFER, \(GLuint\)previous_framebuffer\);',
        'g_gl\.ActiveTexture\(\(GLenum\)previous_active_texture\);',
        'glBindTexture\(GL_TEXTURE_2D, \(GLuint\)previous_texture\);'
    )
    $drawRead = @(
        'glDrawBuffer\(\(GLenum\)previous_draw_buffer\);',
        'glReadBuffer\(\(GLenum\)previous_read_buffer\);'
    )

    foreach ($entry in @(
        @{ Name = 'HDR target'; Function = 'henka_opengl_create_hdr_target'; Marker = 'if (framebuffer == 0U || color_texture == 0U'; Patterns = $common + @('g_gl\.DeleteFramebuffers\(1, &framebuffer\);') },
        @{ Name = 'bloom target'; Function = 'henka_opengl_create_bloom_target'; Marker = 'if (framebuffer == 0U || blur_framebuffer == 0U'; Patterns = $common + @('g_gl\.DeleteFramebuffers\(1, &framebuffer\);') },
        @{ Name = 'directional shadow target'; Function = 'henka_opengl_create_shadow_target'; Marker = 'if (framebuffer == 0U || depth_texture == 0U'; Patterns = $common + $drawRead },
        @{ Name = 'local shadow target'; Function = 'henka_opengl_create_local_shadow_target'; Marker = 'if (framebuffer == 0U || depth_texture == 0U'; Patterns = $common + $drawRead },
        @{ Name = 'cascade shadow target'; Function = 'henka_opengl_create_cascade_shadow_target'; Marker = 'if (framebuffer == 0U || depth_texture == 0U'; Patterns = $common + $drawRead },
        @{ Name = 'point shadow target'; Function = 'henka_opengl_create_point_shadow_target'; Marker = 'if (framebuffer == 0U || depth_texture == 0U'; Patterns = @(
            'g_gl\.BindFramebuffer\(GL_FRAMEBUFFER, \(GLuint\)previous_framebuffer\);',
            'henka_opengl_restore_texture_binding_state\(&texture_state\);',
            'glDrawBuffer\(\(GLenum\)previous_draw_buffer\);',
            'glReadBuffer\(\(GLenum\)previous_read_buffer\);'
        ) }
    )) {
        $body = Get-FunctionBody -Source $Source -FunctionName $entry.Function
        foreach ($failure in (Test-AllocationFailureState -Name $entry.Name -Body $body -AllocationMarker $entry.Marker -Patterns $entry.Patterns)) {
            $missing.Add($failure)
        }
    }

    $bloomBody = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_create_bloom_target'
    $bloomMarkerIndex = $bloomBody.IndexOf('if (framebuffer == 0U || blur_framebuffer == 0U')
    $bloomAllocationFailure = Get-BracedBlock -Source $bloomBody -StartIndex $bloomMarkerIndex
    if ($null -ne $bloomAllocationFailure -and
        $bloomAllocationFailure -match 'state->bloom_ready\s*=\s*false\s*;') {
        $missing.Add('bloom allocation-failure branch must retain the prior ready target')
    }

    $bloomInvalidIndex = $bloomBody.IndexOf('if (state != NULL)')
    $bloomInvalidBranch = Get-BracedBlock -Source $bloomBody -StartIndex $bloomInvalidIndex
    if ($null -ne $bloomInvalidBranch -and
        $bloomInvalidBranch -match 'state->bloom_ready\s*=\s*false\s*;') {
        $missing.Add('bloom invalid-input branch must retain the prior ready target')
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$missing = Test-RendererTargetFailureStateContract -Source $renderer
if ($missing.Count -gt 0) {
    throw "Renderer target allocation-failure state contract failed: $($missing -join ', ')"
}

$hdrBody = Get-FunctionBody -Source $renderer -FunctionName 'henka_opengl_create_hdr_target'
$hdrMarkerIndex = $hdrBody.IndexOf('if (framebuffer == 0U || color_texture == 0U')
$hdrBlock = Get-BracedBlock -Source $hdrBody -StartIndex $hdrMarkerIndex
$negativeBlock = $hdrBlock.Replace(
    '    g_gl.BindFramebuffer(GL_FRAMEBUFFER, (GLuint)previous_framebuffer);',
    '    /* deliberate negative-control omission */')
if ($negativeBlock -eq $hdrBlock) {
    throw 'Renderer target allocation-failure negative control could not remove HDR state restoration.'
}
$negativeHdrBody = $hdrBody.Replace($hdrBlock, $negativeBlock)
$negativeRenderer = $renderer.Replace($hdrBody, $negativeHdrBody)
$negativeMissing = Test-RendererTargetFailureStateContract -Source $negativeRenderer
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer target allocation-failure negative control unexpectedly passed.'
}

Write-Output 'Renderer target allocation-failure state contract passed.'
Write-Output 'Renderer target allocation-failure state negative control passed.'
