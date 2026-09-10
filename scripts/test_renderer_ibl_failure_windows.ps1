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

function Test-RendererIblFailureContract {
    param([string]$Source)

    $sync = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_sync_ibl_resources'
    $delete = Get-FunctionBody -Source $Source -FunctionName 'henka_opengl_delete_ibl_resources'
    if ($null -eq $sync) {
        return @('IBL resource synchronization function')
    }
    if ($null -eq $delete) {
        return @('IBL resource deletion function')
    }

    $buildCall = 'if (henka_opengl_build_ibl_resources(state, scene) != HENKA_SUCCESS)'
    $buildStart = $sync.IndexOf($buildCall, [System.StringComparison]::Ordinal)
    if ($buildStart -lt 0) {
        return @('IBL rebuild failure branch')
    }

    $elseStart = $sync.IndexOf('    else', $buildStart, [System.StringComparison]::Ordinal)
    if ($elseStart -lt 0) {
        return @('IBL rebuild success boundary')
    }

    $failureBranch = $sync.Substring($buildStart, $elseStart - $buildStart)
    $missing = [System.Collections.Generic.List[string]]::new()
    if ($failureBranch.Contains('henka_opengl_delete_ibl_resources(state);')) {
        $missing.Add('failure path preserves the prior derived IBL resources')
    }
    if ($failureBranch -notmatch 'previous_ibl_ready') {
        $missing.Add('failure path preserves the prior IBL readiness state')
    }
    if ($failureBranch -notmatch 'ibl_failed_source_texture') {
        $missing.Add('failure path records the failed source identity')
    }
    if ($failureBranch -notmatch 'ibl_failure_reason') {
        $missing.Add('failure path records an IBL failure reason')
    }
    foreach ($field in @(
        'state->ibl_failed_source_texture = NULL;',
        'state->ibl_failed_source_revision = 0U;',
        'state->ibl_failed_source_rotation = 0.0f;')) {
        if ($delete.IndexOf($field, [System.StringComparison]::Ordinal) -lt 0) {
            $missing.Add("resource teardown clears failed-candidate state: $field")
        }
    }
    if ($delete.IndexOf("state->ibl_failure_reason[0] = '\0';", [System.StringComparison]::Ordinal) -lt 0) {
        $missing.Add('resource teardown clears the stale IBL failure reason')
    }
    return $missing
}

function Test-RendererDerivedTextureAllocationContract {
    param([string]$Source)

    $missing = [System.Collections.Generic.List[string]]::new()
    foreach ($entry in @(
        @{ Name = 'IBL cubemap allocator'; Function = 'henka_opengl_allocate_ibl_cube'; Discard = 'IBL cube texture allocation'; Collect = 'IBL cube texture storage' },
        @{ Name = 'reflection-probe cubemap allocator'; Function = 'henka_opengl_allocate_reflection_probe_cube'; Discard = 'reflection-probe cube texture allocation'; Collect = 'reflection-probe cube texture storage' }
    )) {
        $body = Get-FunctionBody -Source $Source -FunctionName $entry.Function
        if ($null -eq $body) {
            $missing.Add("$($entry.Name) function")
            continue
        }

        $discardPattern = 'henka_opengl_discard_prior_texture_errors\(\s*"' + [regex]::Escape($entry.Discard) + '"\s*\);'
        if ($body -notmatch $discardPattern) {
            $missing.Add("$($entry.Name) must discard stale texture errors before allocation")
        }
        $collectPattern = 'henka_opengl_collect_texture_errors\(\s*"' + [regex]::Escape($entry.Collect) + '"\s*\)'
        if ($body -notmatch $collectPattern) {
            $missing.Add("$($entry.Name) must collect texture-storage errors")
        }
        if ($body -notmatch 'henka_opengl_capture_texture_binding_state\(\s*&texture_state\s*\)' -or
            $body -notmatch 'henka_opengl_restore_texture_binding_state\(\s*&texture_state\s*\)') {
            $missing.Add("$($entry.Name) must restore texture binding state")
        }
        if ($body -notmatch 'glDeleteTextures\(\s*1\s*,\s*&texture\s*\)') {
            $missing.Add("$($entry.Name) must delete a failed candidate texture")
        }
    }

    return $missing
}

$rendererPath = Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c'
$renderer = Get-Content -LiteralPath $rendererPath -Raw
$missing = Test-RendererIblFailureContract -Source $renderer
if ($missing.Count -gt 0) {
    throw "Renderer IBL failure cleanup contract failed: $($missing -join ', ')"
}

$negative = $renderer -replace '(?m)^    bool previous_ibl_ready;\r?\n', ''
$negative = $negative -replace '(?m)^    previous_ibl_ready = state->ibl_ready;\r?\n', ''
$negative = $negative -replace '(?m)^        state->ibl_ready = previous_ibl_ready;\r?\n', "        /* deliberate negative-control omission */`r`n        state->ibl_ready = false;`r`n"
$negativeMissing = Test-RendererIblFailureContract -Source $negative
if ($negativeMissing.Count -eq 0) {
    throw 'Renderer IBL failure cleanup negative control unexpectedly passed.'
}

$allocationMissing = Test-RendererDerivedTextureAllocationContract -Source $renderer
if ($allocationMissing.Count -gt 0) {
    throw "Renderer derived texture allocation contract failed: $($allocationMissing -join ', ')"
}

$negativeAllocation = $renderer.Replace(
    'henka_opengl_discard_prior_texture_errors("IBL cube texture allocation");',
    '/* deliberate negative-control omission */')
$negativeAllocationMissing = Test-RendererDerivedTextureAllocationContract -Source $negativeAllocation
if ($negativeAllocationMissing.Count -eq 0) {
    throw 'Renderer derived texture allocation negative control unexpectedly passed.'
}

Write-Output 'Renderer IBL failure cleanup contract passed.'
Write-Output 'Renderer IBL failure cleanup negative control passed.'
Write-Output 'Renderer derived texture allocation contract passed.'
Write-Output 'Renderer derived texture allocation negative control passed.'
