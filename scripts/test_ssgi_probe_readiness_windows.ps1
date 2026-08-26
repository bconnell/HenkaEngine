$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sandbox = Get-Content -Raw (Join-Path $repoRoot 'examples/sandbox3d/main.c')
$capture = Get-Content -Raw (Join-Path $repoRoot 'scripts/capture_visual_evidence_windows.ps1')
$checker = Get-Content -Raw (Join-Path $repoRoot 'scripts/check_ssgi_reference_visual_evidence_windows.ps1')

function Assert-Contract([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "SSGI probe-readiness contract failed: $message"
    }
}

Assert-Contract ($sandbox -match '(?s)SANDBOX3D_REALISM_REFERENCE_KIND_SSGI.*?rendered_reflection_probe_enabled_count\s*<\s*2U') `
    'SSGI readiness must require at least two enabled probes.'
Assert-Contract ($sandbox -match '(?s)SANDBOX3D_REALISM_REFERENCE_KIND_SSGI.*?rendered_reflection_probe_captured_count\s*<\s*2U') `
    'SSGI readiness must require at least two current captured probes.'
Assert-Contract ($sandbox -match '(?s)SANDBOX3D_REALISM_REFERENCE_KIND_SSGI.*?rendered_reflection_probe_capture_generation\s*==\s*0U') `
    'SSGI readiness must require a nonzero probe capture generation.'
Assert-Contract ($sandbox -match '(?s)SANDBOX3D_REALISM_REFERENCE_KIND_SSGI.*?rendered_reflection_probe_capture_failure_count\s*!=\s*0U') `
    'SSGI readiness must fail closed on probe capture failures.'
Assert-Contract ($sandbox -match 'reference_probe_enabled_count=%u') `
    'SSGI readiness metadata must report the enabled probe count.'
Assert-Contract ($sandbox -match 'reference_probe_captured_count=%u') `
    'SSGI readiness metadata must report the captured probe count.'
Assert-Contract ($capture -match 'test_ssgi_probe_readiness_windows\.ps1' -or $capture -match 'reference_probe_enabled_count') `
    'SSGI evidence must retain the probe-health contract in its capture path.'
Assert-Contract ($checker -match 'reference_probe_enabled_count') `
    'SSGI image validation must parse the probe-health metadata.'

Write-Output 'SSGI probe-readiness source contract test passed.'
