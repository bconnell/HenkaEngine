$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$capture = Get-Content (Join-Path $repoRoot 'scripts/capture_visual_evidence_windows.ps1') -Raw
$start = $capture.IndexOf('function Assert-HenkaReferenceCaptureMetadata')
$end = $capture.IndexOf('function Assert-HenkaLightingReferenceCaptureMetadata')
if ($start -lt 0 -or $end -le $start) {
    throw 'Generic realism reference metadata validator was not found.'
}
$referenceValidator = $capture.Substring($start, $end - $start)

if ($capture -notmatch 'if \(\$EvidenceProfile -eq "REALISM_REFERENCE"\)') {
    throw 'REALISM_REFERENCE capture profile routing is missing.'
}
if ($capture -notmatch 'Assert-HenkaReferenceCaptureMetadata') {
    throw 'REALISM_REFERENCE capture profile does not use its dedicated metadata validator.'
}
if ($referenceValidator -notmatch 'reference_count=\(\?<count>\\d\+\).*settled_frames=\(\?<sf>\\d\+\)') {
    throw 'Generic realism metadata validation does not require nine settled subjects.'
}
foreach ($probeField in @('enabled', 'captured', 'generation', 'failures')) {
    if ($referenceValidator -match '\$match\.Groups\["' + $probeField + '"\]') {
        throw "Generic realism metadata validator incorrectly requires scene-probe field '$probeField'."
    }
}

Write-Output 'Generic realism reference capture validator contract test passed.'
