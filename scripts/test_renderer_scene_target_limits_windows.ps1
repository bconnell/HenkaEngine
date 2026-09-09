$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$renderer = Get-Content -Raw (Join-Path $repoRoot 'engine/src/renderer/renderer_opengl.c')

function Assert-Contract([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "Renderer scene-target limit contract failed: $message"
    }
}

$hdrFunction = [regex]::Match(
    $renderer,
    '(?s)static henka_result henka_opengl_create_hdr_target\(.*?\n\}\n\nstatic henka_result henka_opengl_create_shadow_target')
Assert-Contract $hdrFunction.Success `
    'The HDR scene-target creation function could not be located.'

$hdrBody = $hdrFunction.Value
Assert-Contract ($hdrBody -match 'width <= 0\s*\|\|\s*height <= 0\s*\|\|\s*width > 8192\s*\|\|\s*height > 8192') `
    'HDR allocation must reject dimensions above the shared 8192-pixel scene-target bound before issuing GPU allocations.'

Write-Output 'Renderer scene-target limit source contract test passed.'
