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

$validationOnlyChanges = @(
    'docs/design/editor-typography.md',
    'docs/engine-completion-matrix.md',
    'docs/ui.md',
    'scripts/henka_ui_automation_helpers.ps1',
    'scripts/henka_typography_capture_policy.ps1',
    'scripts/test_editor_typography_windows.ps1',
    'scripts/test_editor_typography_capture_policy_windows.ps1',
    'scripts/test_henka_ui_automation_helpers_windows.ps1',
    'tests/check_sandbox3d_editor_typography_integration.cmake',
    'tests/test_sandbox3d_editor_typography_integration.cmake'
)
$validationOnly = Get-HenkaTypographyProductSourceEquivalenceViolation `
    -ChangedPaths $validationOnlyChanges
if ($null -ne $validationOnly) {
    throw "Approved documentation/test/capture changes should not invalidate the packaged product: $validationOnly"
}

$productChanges = @(
    'engine/src/ui/ui_font.c',
    'examples/sandbox3d/main.c',
    'assets/ui/fonts/AtkinsonHyperlegible-Regular.ttf',
    'CMakeLists.txt'
)
foreach ($productPath in $productChanges) {
    $productViolation = Get-HenkaTypographyProductSourceEquivalenceViolation `
        -ChangedPaths @($productPath)
    if ($null -eq $productViolation) {
        throw "A product/build input change must invalidate split provenance: $productPath"
    }
}

$buildScriptViolation = Get-HenkaTypographyProductSourceEquivalenceViolation `
    -ChangedPaths @('scripts/build_windows.ps1')
if ($null -eq $buildScriptViolation) {
    throw 'A build-tooling change must not be misclassified as validation-only.'
}

$traversalViolation = Get-HenkaTypographyProductSourceEquivalenceViolation `
    -ChangedPaths @('docs/design/../engine/src/ui.c')
if ($null -eq $traversalViolation) {
    throw 'A traversal-shaped changed path must fail closed.'
}

$windowsSeparators = Get-HenkaTypographyProductSourceEquivalenceViolation `
    -ChangedPaths @('docs\design\editor-typography.md')
if ($null -ne $windowsSeparators) {
    throw "Windows separators should normalize before comparing repository paths: $windowsSeparators"
}

Write-Output 'Typography capture scale-pair and product-source provenance policy tests passed.'
