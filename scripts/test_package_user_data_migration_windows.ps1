param(
    [string]$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")
. (Join-Path $PSScriptRoot "henka_package_user_data.ps1")

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$fixtureRoot = New-HenkaTemporaryDirectory -RepositoryRoot $RepositoryRoot -Purpose "package-user-data-migration-test"
$testJustification = "isolated migration contract fixture under canonical Henka _local"

try {
    $legacyRoot = Join-Path $fixtureRoot "legacy-package"
    $legacyUser = Join-Path $legacyRoot "user"
    $packageRoot = Join-Path $fixtureRoot "active-package"
    $stagingRoot = Join-Path $fixtureRoot "staging-preserve"
    [System.IO.Directory]::CreateDirectory($legacyUser) | Out-Null
    [System.IO.Directory]::CreateDirectory($packageRoot) | Out-Null
    [System.IO.Directory]::CreateDirectory($stagingRoot) | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $legacyUser "legacy.settings"), "legacy")

    $action = Copy-HenkaPackageUserDataForStaging `
        -RepositoryRoot $RepositoryRoot `
        -PackageRoot $packageRoot `
        -StagingRoot $stagingRoot `
        -LegacyPackageUserPath $legacyUser `
        -AllowTestOwnedLegacyPath `
        -TestOwnedPathJustification $testJustification
    Assert-Condition ($action -is [string]) "The migration helper returned file-copy pipeline output instead of one action string."
    Assert-Condition ($action -eq "legacy-copied") "First migration did not select the old package user directory."
    Assert-Condition ((Get-Content -LiteralPath (Join-Path $stagingRoot "user\legacy.settings") -Raw) -eq "legacy") `
        "Legacy package user data was not copied intact."
    Assert-Condition (Test-Path -LiteralPath (Join-Path $legacyUser "legacy.settings")) `
        "The migration modified or removed the old package user data."

    $statePath = Join-Path $stagingRoot ".henka-user-data-migration.json"
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    Assert-Condition ($state.action -eq "legacy-copied") "Migration state does not record the copy action."

    $resetPackageRoot = Join-Path $fixtureRoot "reset-package"
    $resetStagingRoot = Join-Path $fixtureRoot "staging-reset"
    [System.IO.Directory]::CreateDirectory($resetPackageRoot) | Out-Null
    [System.IO.Directory]::CreateDirectory($resetStagingRoot) | Out-Null
    $resetAction = Copy-HenkaPackageUserDataForStaging `
        -RepositoryRoot $RepositoryRoot `
        -PackageRoot $resetPackageRoot `
        -StagingRoot $resetStagingRoot `
        -LegacyPackageUserPath $legacyUser `
        -ResetUserData `
        -AllowTestOwnedLegacyPath `
        -TestOwnedPathJustification $testJustification
    Assert-Condition ($resetAction -eq "reset") "Explicit reset did not take precedence over legacy data."
    Assert-Condition (-not (Test-Path -LiteralPath (Join-Path $resetStagingRoot "user"))) `
        "Explicit reset unexpectedly copied legacy user data."

    $nextStagingRoot = Join-Path $fixtureRoot "staging-after-reset"
    [System.IO.Directory]::CreateDirectory($nextStagingRoot) | Out-Null
    $nextAction = Copy-HenkaPackageUserDataForStaging `
        -RepositoryRoot $RepositoryRoot `
        -PackageRoot $resetStagingRoot `
        -StagingRoot $nextStagingRoot `
        -LegacyPackageUserPath $legacyUser `
        -AllowTestOwnedLegacyPath `
        -TestOwnedPathJustification $testJustification
    Assert-Condition ($nextAction -eq "migration-already-complete") `
        "A later package refresh would silently restore legacy data after reset."
    Assert-Condition (-not (Test-Path -LiteralPath (Join-Path $nextStagingRoot "user"))) `
        "Legacy data reappeared after the persisted reset state."

    $preservedPackageRoot = Join-Path $fixtureRoot "package-with-current-user"
    $preservedUser = Join-Path $preservedPackageRoot "user"
    $preservedStagingRoot = Join-Path $fixtureRoot "staging-current-user"
    [System.IO.Directory]::CreateDirectory($preservedUser) | Out-Null
    [System.IO.Directory]::CreateDirectory($preservedStagingRoot) | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $preservedUser "current.settings"), "current")
    $preservedAction = Copy-HenkaPackageUserDataForStaging `
        -RepositoryRoot $RepositoryRoot `
        -PackageRoot $preservedPackageRoot `
        -StagingRoot $preservedStagingRoot `
        -LegacyPackageUserPath $legacyUser `
        -AllowTestOwnedLegacyPath `
        -TestOwnedPathJustification $testJustification
    Assert-Condition ($preservedAction -eq "preserved-package") "Existing package user data did not retain precedence."
    Assert-Condition ((Get-Content -LiteralPath (Join-Path $preservedStagingRoot "user\current.settings") -Raw) -eq "current") `
        "Current package user data was not copied intact."
    Assert-Condition (-not (Test-Path -LiteralPath (Join-Path $preservedStagingRoot "user\legacy.settings"))) `
        "Legacy data overwrote or merged into current package user data."

    Write-Output "[pass] Legacy package settings are copied once, current settings retain precedence, explicit reset persists, and the legacy source remains untouched."
}
finally {
    $item = Get-Item -LiteralPath $fixtureRoot -Force -ErrorAction SilentlyContinue
    if ($null -ne $item -and (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -eq 0)) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction Stop
    }
}
