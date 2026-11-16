Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'henka_typography_capture_policy.ps1')

$validCaptures = @(
    [pscustomobject]@{
        font = 'atkinson'; ui_scale_percent = 100; client_width = 1280; client_height = 720
        os_display_scale_numerator = 3; os_display_scale_denominator = 2
    },
    [pscustomobject]@{
        font = 'noto'; ui_scale_percent = 100; client_width = 1280; client_height = 720
        os_display_scale_numerator = 6; os_display_scale_denominator = 4
    }
)
if ($null -ne (Get-HenkaTypographyCaptureScalePairViolation -Captures $validCaptures)) {
    throw 'Equivalent product-reported display-scale ratios should preserve a matched font comparison.'
}

$mismatchedCaptures = @(
    $validCaptures[0],
    [pscustomobject]@{
        font = 'noto'; ui_scale_percent = 100; client_width = 1280; client_height = 720
        os_display_scale_numerator = 1; os_display_scale_denominator = 1
    }
)
$mismatch = Get-HenkaTypographyCaptureScalePairViolation -Captures $mismatchedCaptures
if ($null -eq $mismatch -or $mismatch -notmatch 'different product-reported OS display scales') {
    throw 'The capture policy must reject Atkinson and Noto images made at different OS display scales.'
}

$missingScaleCapture = [pscustomobject]@{
    font = 'atkinson'; ui_scale_percent = 100; client_width = 1280; client_height = 720
}
$missingScale = Get-HenkaTypographyCaptureScalePairViolation -Captures @($missingScaleCapture)
if ($null -eq $missingScale -or $missingScale -notmatch 'missing its product-reported OS display scale') {
    throw 'The capture policy must reject evidence that omits product-reported OS display scale.'
}

Write-Output 'Typography capture scale-pair policy tests passed.'
