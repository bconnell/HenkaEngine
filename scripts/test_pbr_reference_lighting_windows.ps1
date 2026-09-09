$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sandbox = Get-Content -Raw (Join-Path $repoRoot 'examples/sandbox3d/main.c')

function Assert-Contract([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "PBR reference lighting contract failed: $message"
    }
}

$fillCondition = '(?s)if \(!sandbox3d_default_scene_requested\(state\) &&\s*!isolated_ibl_reference &&.*?SANDBOX3D_REALISM_REFERENCE_KIND_PBR.*?SANDBOX3D_REALISM_REFERENCE_KIND_SCENE_PROBE\).*?henka_scene_add_light'
Assert-Contract ($sandbox -match $fillCondition) `
    'PBR calibration profiles must use a dedicated fixture-only readability fill.'
Assert-Contract ($sandbox -match '(?s)fixture-only readability aid.*?HENKA_SCENE_LIGHT_POINT.*?\{0\.0f, 4\.0f, 1\.0f\}.*?\{0\.82f, 0\.86f, 0\.92f\}.*?true') `
    'The readability fill must be enabled, neutral, and bounded.'
$fillMatch = [regex]::Match($sandbox, $fillCondition)
$fillBlock = if ($fillMatch.Success) {
    $fillMatch.Value
} else {
    ''
}
Assert-Contract ($fillBlock -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_LIGHTING') `
    'The lighting reference must retain its direct spatial-light contract.'
Assert-Contract ($fillBlock -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_SSS') `
    'The subsurface reference must retain its dedicated back-light contract.'

Write-Output 'PBR reference lighting source contract test passed.'
