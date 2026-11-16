Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'henka_script_common.ps1')
. (Join-Path $PSScriptRoot 'henka_typography_package_policy.ps1')

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$cleanupManager = Join-Path $PSScriptRoot 'manage_generated_artifacts_windows.ps1'
$sourceFontDirectory = Join-Path $repoRoot 'assets\ui\fonts'
$scratchRoot = New-HenkaTemporaryDirectory `
    -RepositoryRoot $repoRoot `
    -Purpose 'editor-typography-package-policy'

function New-FontPackageFixture {
    param([Parameter(Mandatory = $true)][string]$Name)

    $root = Join-Path $scratchRoot $Name
    $fontDirectory = Join-Path $root 'assets\ui\fonts'
    [System.IO.Directory]::CreateDirectory($fontDirectory) | Out-Null
    foreach ($fileName in @('AtkinsonHyperlegible-Regular.ttf', 'Atkinson-OFL.txt')) {
        Copy-Item -LiteralPath (Join-Path $sourceFontDirectory $fileName) `
            -Destination (Join-Path $fontDirectory $fileName)
    }
    return [pscustomobject]@{ Root = $root; Fonts = $fontDirectory }
}

function Assert-PolicyResult {
    param(
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][string]$PackageRoot,
        [switch]$IncludeComparisonFonts,
        [Parameter(Mandatory = $true)][bool]$ExpectedValid
    )

    $violation = Get-HenkaTypographyPackageAssetViolation `
        -PackageRoot $PackageRoot `
        -IncludeComparisonFonts:$IncludeComparisonFonts
    $actualValid = $null -eq $violation
    if ($actualValid -ne $ExpectedValid) {
        throw "Typography package policy case '$Label' had unexpected result: valid=$actualValid; violation=$violation"
    }
    Write-Output "PASS typography package policy: $Label"
}

try {
    $production = New-FontPackageFixture -Name 'production'
    Assert-PolicyResult -Label 'selected face and license accepted' `
        -PackageRoot $production.Root -ExpectedValid $true

    Remove-Item -LiteralPath (Join-Path $production.Fonts 'AtkinsonHyperlegible-Regular.ttf')
    Assert-PolicyResult -Label 'production package rejects a missing selected font' `
        -PackageRoot $production.Root -ExpectedValid $false
    Copy-Item -LiteralPath (Join-Path $sourceFontDirectory 'AtkinsonHyperlegible-Regular.ttf') `
        -Destination (Join-Path $production.Fonts 'AtkinsonHyperlegible-Regular.ttf')

    foreach ($fileName in @('NotoSans-Regular.ttf', 'NotoSans-OFL.txt')) {
        Copy-Item -LiteralPath (Join-Path $sourceFontDirectory $fileName) `
            -Destination (Join-Path $production.Fonts $fileName)
    }
    Assert-PolicyResult -Label 'unrequested comparison face rejected by production policy' `
        -PackageRoot $production.Root -ExpectedValid $false

    $comparison = New-FontPackageFixture -Name 'comparison'
    foreach ($fileName in @('NotoSans-Regular.ttf', 'NotoSans-OFL.txt')) {
        Copy-Item -LiteralPath (Join-Path $sourceFontDirectory $fileName) `
            -Destination (Join-Path $comparison.Fonts $fileName)
    }
    Assert-PolicyResult -Label 'explicit comparison package accepts both verified faces' `
        -PackageRoot $comparison.Root -IncludeComparisonFonts -ExpectedValid $true

    Remove-Item -LiteralPath (Join-Path $comparison.Fonts 'NotoSans-OFL.txt')
    Assert-PolicyResult -Label 'comparison package rejects missing license' `
        -PackageRoot $comparison.Root -IncludeComparisonFonts -ExpectedValid $false

    Copy-Item -LiteralPath (Join-Path $sourceFontDirectory 'NotoSans-OFL.txt') `
        -Destination (Join-Path $comparison.Fonts 'NotoSans-OFL.txt')
    [System.IO.File]::WriteAllBytes(
        (Join-Path $comparison.Fonts 'AtkinsonHyperlegible-Regular.ttf'),
        [byte[]]@(0, 1, 2, 3))
    Assert-PolicyResult -Label 'comparison package rejects corrupt selected font' `
        -PackageRoot $comparison.Root -IncludeComparisonFonts -ExpectedValid $false

    Write-Output 'Editor typography package policy tests passed.'
}
finally {
    if (Test-Path -LiteralPath $scratchRoot -PathType Container) {
        $markerPath = Join-Path $scratchRoot '.henka-generated.json'
        if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) {
            throw "The owned typography package-policy scratch root has no lifecycle marker: $scratchRoot"
        }
        $null = Write-HenkaGeneratedRootMarker `
            -RepoRoot $repoRoot `
            -Path $scratchRoot `
            -Purpose 'editor-typography-package-policy' `
            -RetentionClass 'SCRATCH' `
            -Active $false `
            -CleanupEligible $true `
            -CleanupCondition 'proof_consumed' `
            -Configuration 'Debug'
        $cleanupOutput = @(
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $cleanupManager `
                -Mode Cleanup -ConfirmNoActiveProcess -CandidatePath $scratchRoot 2>&1)
        $cleanupExitCode = $LASTEXITCODE
        if ($cleanupExitCode -ne 0 -or (Test-Path -LiteralPath $scratchRoot)) {
            $cleanupText = (($cleanupOutput | ForEach-Object { [string]$_ }) -join "`n")
            throw "Owned typography package-policy scratch cleanup failed. Exit=$cleanupExitCode; Output=$cleanupText"
        }
    }
}
