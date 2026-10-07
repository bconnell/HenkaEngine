[CmdletBinding()]
param(
    [string]$RepositoryRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
}
$RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)

function Assert-StorageContract {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )
    if (-not $Condition) {
        throw $Message
    }
}

function Invoke-ArtifactLifecycleManager {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $manager @Arguments 2>&1)
        return [pscustomobject]@{
            ExitCode = $LASTEXITCODE
            Output = (($output | ForEach-Object { [string]$_ }) -join "`n")
        }
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
}

function Invoke-StorageProbeScript {
    param(
        [Parameter(Mandatory = $true)][string]$ScriptPath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $ScriptPath @Arguments 2>&1)
        return [pscustomobject]@{
            ExitCode = $LASTEXITCODE
            Output = (($output | ForEach-Object { [string]$_ }) -join "`n")
        }
    }
    finally { $ErrorActionPreference = $previousPreference }
}

foreach ($functionName in @(
        "Get-HenkaCanonicalRepositoryRoot",
        "Get-HenkaLocalRoot",
        "Resolve-HenkaLocalPath",
        "Resolve-HenkaDependencyRoot",
        "Get-HenkaBuildRoot",
        "Get-HenkaPackageRoot",
        "Get-HenkaWorktreeRoot",
        "Get-HenkaEvidenceRoot",
        "Get-HenkaExactCandidateRoot")) {
    if ($null -eq (Get-Command $functionName -CommandType Function -ErrorAction SilentlyContinue)) {
        throw "Canonical Henka storage contract is missing '$functionName'; generated development roots are not yet centrally resolved."
    }
}

$canonicalRepositoryRoot = [System.IO.Path]::GetFullPath((Get-HenkaCanonicalRepositoryRoot -RepositoryRoot $RepositoryRoot)).TrimEnd("\", "/")
$localRoot = [System.IO.Path]::GetFullPath((Get-HenkaLocalRoot -RepositoryRoot $RepositoryRoot)).TrimEnd("\", "/")
$expectedLocalRoot = [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $canonicalRepositoryRoot) "_local")).TrimEnd("\", "/")
Assert-StorageContract (Test-Path -LiteralPath (Join-Path $canonicalRepositoryRoot ".git") -PathType Container) `
    "The shared Git directory did not resolve to a normal canonical checkout."
Assert-StorageContract $localRoot.Equals(
    $expectedLocalRoot,
    [System.StringComparison]::OrdinalIgnoreCase) "The canonical _local root is not the sibling of the shared repository."
$buildRoot = [System.IO.Path]::GetFullPath((Get-HenkaBuildRoot -RepositoryRoot $RepositoryRoot))
$testTemporaryRoot = [System.IO.Path]::GetFullPath((Get-HenkaTestTemporaryRoot -RepositoryRoot $RepositoryRoot))
$packageRoot = [System.IO.Path]::GetFullPath((Get-HenkaPackageRoot -RepositoryRoot $RepositoryRoot))
$worktreeRoot = [System.IO.Path]::GetFullPath((Get-HenkaWorktreeRoot -RepositoryRoot $RepositoryRoot -Name "storage-guard-probe"))
$evidenceRoot = [System.IO.Path]::GetFullPath((Get-HenkaEvidenceRoot -RepositoryRoot $RepositoryRoot))
$candidateRoot = [System.IO.Path]::GetFullPath((Get-HenkaExactCandidateRoot -RepositoryRoot $RepositoryRoot))
$repositoryPath = [System.IO.Path]::GetFullPath($RepositoryRoot).TrimEnd("\", "/")
$candidatePrefix = $candidateRoot.TrimEnd("\", "/") + [System.IO.Path]::DirectorySeparatorChar
$isExactCandidateRepository = $repositoryPath.StartsWith(
    $candidatePrefix,
    [System.StringComparison]::OrdinalIgnoreCase)
$ordinaryBuildRoot = [System.IO.Path]::GetFullPath((Get-HenkaBuildRoot -RepositoryRoot $canonicalRepositoryRoot))

Assert-StorageContract $ordinaryBuildRoot.StartsWith(
    (Join-Path $localRoot "builds") + [System.IO.Path]::DirectorySeparatorChar,
    [System.StringComparison]::OrdinalIgnoreCase) "The shared ordinary build root is not beneath _local\builds."
if ($isExactCandidateRepository) {
    Assert-StorageContract $buildRoot.Equals(
        [System.IO.Path]::GetFullPath((Join-Path $repositoryPath "build")),
        [System.StringComparison]::OrdinalIgnoreCase) `
        "An exact candidate's build root is not beneath that exact candidate."
}
else {
    Assert-StorageContract $buildRoot.StartsWith(
        (Join-Path $localRoot "builds") + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase) "An ordinary checkout's build root is not beneath _local\builds."
}
Assert-StorageContract $testTemporaryRoot.StartsWith(
    $buildRoot.TrimEnd("\", "/") + [System.IO.Path]::DirectorySeparatorChar,
    [System.StringComparison]::OrdinalIgnoreCase) "The test temporary root is not beneath its canonical build root."
Assert-StorageContract $packageRoot.StartsWith(
    (Join-Path $localRoot "packages") + [System.IO.Path]::DirectorySeparatorChar,
    [System.StringComparison]::OrdinalIgnoreCase) "The package root is not beneath _local\packages."
Assert-StorageContract $worktreeRoot.StartsWith(
    (Join-Path $localRoot "worktrees") + [System.IO.Path]::DirectorySeparatorChar,
    [System.StringComparison]::OrdinalIgnoreCase) "The managed Henka worktree root is not beneath _local\worktrees."
Assert-StorageContract $evidenceRoot.StartsWith(
    (Join-Path $localRoot "evidence") + [System.IO.Path]::DirectorySeparatorChar,
    [System.StringComparison]::OrdinalIgnoreCase) "The evidence root is not beneath _local\evidence."
Assert-StorageContract $candidateRoot.Equals(
    [System.IO.Path]::GetFullPath((Join-Path $localRoot "exact-candidates")),
    [System.StringComparison]::OrdinalIgnoreCase) "The exact-candidate root does not match the canonical _local location."

foreach ($generatedRoot in @($buildRoot, $packageRoot, $evidenceRoot, $candidateRoot)) {
    $resolvedGeneratedRoot = Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot -Path $generatedRoot
    $insideLocalRoot = $resolvedGeneratedRoot.Equals($localRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
        $resolvedGeneratedRoot.StartsWith(
            $localRoot + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase)
    Assert-StorageContract $insideLocalRoot "Generated root escaped the canonical Henka local root: $resolvedGeneratedRoot"
}

$markerProbe = $null
$candidateProbe = $null
$managerProbe = $null
$gitMetadataProbe = $null
$reparseProbe = $null
try {
    $markerProbe = New-HenkaTemporaryDirectory -RepositoryRoot $RepositoryRoot -Purpose "storage-guard-contract"
    $markerPath = Join-Path $markerProbe ".henka-generated.json"
    Assert-StorageContract (Test-Path -LiteralPath $markerPath -PathType Leaf) `
        "A marker could not be written beneath the canonical Henka local root."

    $candidateProbe = Join-Path $candidateRoot ("storage-guard-candidate-" + [Guid]::NewGuid().ToString("N"))
    $null = New-HenkaLocalDirectory -RepositoryRoot $RepositoryRoot -Path $candidateProbe
    $candidateBuildRoot = Get-HenkaBuildRoot -RepositoryRoot $candidateProbe
    Assert-StorageContract ([System.IO.Path]::GetFullPath($candidateBuildRoot).Equals(
            [System.IO.Path]::GetFullPath((Join-Path $candidateProbe "build")),
            [System.StringComparison]::OrdinalIgnoreCase)) `
        "A direct exact-candidate checkout did not keep its build beneath that candidate."

    $nestedLocalProbe = Join-Path $markerProbe "_local\nested-project"
    $null = New-HenkaLocalDirectory -RepositoryRoot $RepositoryRoot -Path $nestedLocalProbe
    $nestedLocalResolution = Get-HenkaLocalRoot -RepositoryRoot $nestedLocalProbe
    Assert-StorageContract ([System.IO.Path]::GetFullPath($nestedLocalResolution).Equals(
            $localRoot,
            [System.StringComparison]::OrdinalIgnoreCase)) `
        "A nested directory named _local displaced the repository's canonical local-storage root: $nestedLocalResolution"

    $dependencyProbe = Join-Path $markerProbe "dependency-root-contract"
    $null = New-HenkaLocalDirectory -RepositoryRoot $RepositoryRoot -Path $dependencyProbe
    $resolvedDependencyProbe = Resolve-HenkaDependencyRoot `
        -RepositoryRoot $RepositoryRoot `
        -DependencyRoot $dependencyProbe
    Assert-StorageContract ($resolvedDependencyProbe.Equals(
            [System.IO.Path]::GetFullPath($dependencyProbe),
            [System.StringComparison]::OrdinalIgnoreCase)) `
        "A dependency root beneath canonical _local did not resolve unchanged."
    $missingDependencyProbe = Join-Path $markerProbe "missing-dependency-root"
    $missingInRootRejected = $false
    try {
        $null = Resolve-HenkaDependencyRoot -RepositoryRoot $RepositoryRoot -DependencyRoot $missingDependencyProbe
    }
    catch {
        $missingInRootRejected = $_.Exception.Message -match "not found|does not exist|missing"
        if (-not $missingInRootRejected) { throw }
    }
    Assert-StorageContract $missingInRootRejected `
        "A missing dependency directory under _local was not classified as missing after location validation."

    # This is an external-path value only. The negative control must never create it.
    $externalProbe = Join-Path $env:SystemDrive ("henka-storage-guard-negative-" + [Guid]::NewGuid().ToString("N"))
    $externalDependencyRejected = $false
    try {
        $null = Resolve-HenkaDependencyRoot -RepositoryRoot $RepositoryRoot -DependencyRoot $externalProbe
    }
    catch {
        $externalDependencyRejected = $_.Exception.Message -match "outside|escape|local root|canonical"
        if (-not $externalDependencyRejected) { throw }
    }
    Assert-StorageContract $externalDependencyRejected `
        "An explicit dependency root outside canonical _local was not rejected by the shared policy."
    Assert-StorageContract (-not (Test-Path -LiteralPath $externalProbe)) `
        "The rejected external dependency path was unexpectedly created: $externalProbe"

    foreach ($scriptName in @("build_windows.ps1", "test_windows.ps1")) {
        $scriptPath = Join-Path $PSScriptRoot $scriptName
        $scriptBuildProbe = Join-Path $localRoot ("storage-invalid-dependency-build-" + [Guid]::NewGuid().ToString("N"))
        $arguments = @("-BuildRoot", $scriptBuildProbe, "-DependencyRoot", $externalProbe)
        $scriptResult = Invoke-StorageProbeScript -ScriptPath $scriptPath -Arguments $arguments
        Assert-StorageContract ($scriptResult.ExitCode -ne 0 -and
            $scriptResult.Output -match "outside|escape|local root|canonical") `
            "$scriptName did not reject an explicit external dependency root before validation. Output: $($scriptResult.Output)"
        Assert-StorageContract (-not (Test-Path -LiteralPath $scriptBuildProbe)) `
            "$scriptName created a build root before rejecting its external dependency root: $scriptBuildProbe"
    }

    $externalRejected = $false
    try {
        $null = Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot -Path $externalProbe
    }
    catch {
        $externalRejected = $_.Exception.Message -match "outside|escape|local root|canonical"
        if (-not $externalRejected) {
            throw
        }
    }
    Assert-StorageContract $externalRejected "An external generated root was not rejected by the canonical path guard: $externalProbe"

    $creationRejected = $false
    try {
        $null = New-HenkaLocalDirectory -RepositoryRoot $RepositoryRoot -Path $externalProbe
    }
    catch {
        $creationRejected = $_.Exception.Message -match "outside|escape|local root|canonical"
        if (-not $creationRejected) {
            throw
        }
    }
    Assert-StorageContract $creationRejected "An external generated directory was not rejected before creation."

    $markerCreationRejected = $false
    try {
        $null = Write-HenkaGeneratedRootMarker `
            -RepoRoot $RepositoryRoot `
            -Path $externalProbe `
            -Purpose "storage guard negative control" `
            -RetentionClass "SCRATCH" `
            -Active $false `
            -CleanupEligible $true `
            -CleanupCondition "negative control must never create this path"
    }
    catch {
        $markerCreationRejected = $_.Exception.Message -match "outside|escape|local root|canonical"
        if (-not $markerCreationRejected) {
            throw
        }
    }
    Assert-StorageContract $markerCreationRejected "The generated-root marker helper accepted an external path."
    Assert-StorageContract (-not (Test-Path -LiteralPath $externalProbe)) `
        "The rejected external path was unexpectedly created: $externalProbe"

    $traversalProbe = Join-Path $localRoot "..\storage-guard-traversal-negative"
    $traversalRejected = $false
    try {
        $null = Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot -Path $traversalProbe
    }
    catch {
        $traversalRejected = $_.Exception.Message -match "outside|escape|local root|canonical"
        if (-not $traversalRejected) {
            throw
        }
    }
    Assert-StorageContract $traversalRejected "A parent-traversal path was not rejected by the canonical path guard."

    $reparseProbe = Join-Path $markerProbe "external-reparse-point-negative"
    $reparseTarget = Join-Path $env:SystemRoot "System32"
    $commandProcessor = Join-Path $env:SystemRoot "System32\cmd.exe"
    Assert-StorageContract (Test-Path -LiteralPath $reparseTarget -PathType Container) `
        "The read-only system directory needed for the reparse-point negative control is unavailable."
    Assert-StorageContract (Test-Path -LiteralPath $commandProcessor -PathType Leaf) `
        "The Windows command processor needed for the junction negative control is unavailable."
    $junctionCommand = 'mklink /J "{0}" "{1}"' -f $reparseProbe, $reparseTarget
    $junctionOutput = @(& $commandProcessor /d /c $junctionCommand 2>&1)
    $junctionExitCode = $LASTEXITCODE
    Assert-StorageContract ($junctionExitCode -eq 0 -and
        (Test-Path -LiteralPath $reparseProbe -PathType Container)) `
        "Could not establish the owned junction negative control. Exit=$junctionExitCode Output=$($junctionOutput -join ' ')"
    $junctionItem = Get-Item -LiteralPath $reparseProbe -Force
    Assert-StorageContract (($junctionItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) `
        "The junction negative control did not create a reparse point."
    $reparseRejected = $false
    try {
        $null = Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot `
            -Path (Join-Path $reparseProbe "drivers\etc\hosts")
    }
    catch {
        $reparseRejected = $_.Exception.Message -match "reparse point"
        if (-not $reparseRejected) {
            throw
        }
    }
    Assert-StorageContract $reparseRejected `
        "The canonical path guard accepted a path that crosses a junction to an external directory."
    # Remove only the junction itself. PowerShell 5.1 Remove-Item can throw
    # while handling a directory reparse point; recursive cleanup must never
    # traverse this link into the external target.
    [System.IO.Directory]::Delete($reparseProbe, $false)
    Assert-StorageContract (-not (Test-Path -LiteralPath $reparseProbe) -and
        (Test-Path -LiteralPath $reparseTarget -PathType Container) -and
        (Test-Path -LiteralPath $commandProcessor -PathType Leaf)) `
        "Removing the test junction did not preserve its external target."
    $reparseProbe = $null

    $manager = Join-Path $PSScriptRoot "manage_generated_artifacts_windows.ps1"
    $managerProbe = Join-Path $markerProbe "lifecycle-managed-local-root"
    $null = New-HenkaLocalDirectory -RepositoryRoot $RepositoryRoot -Path $managerProbe
    $null = Write-HenkaGeneratedRootMarker `
        -RepoRoot $RepositoryRoot `
        -Path $managerProbe `
        -Purpose "local storage lifecycle regression" `
        -RetentionClass "PROOF_CONSUMED" `
        -Active $false `
        -CleanupEligible $true `
        -CleanupCondition "proof_consumed"
    $managerDryRun = Invoke-ArtifactLifecycleManager -Arguments @(
        "-Mode", "DryRun", "-CandidatePath", $managerProbe)
    Assert-StorageContract ($managerDryRun.ExitCode -eq 0 -and
        $managerDryRun.Output -match "cleanup_eligible=True" -and
        $managerDryRun.Output -match [regex]::Escape($managerProbe)) `
        "The artifact lifecycle manager rejected an eligible generated root beneath canonical _local. Output: $($managerDryRun.Output)"
    Assert-StorageContract (Test-Path -LiteralPath $managerProbe -PathType Container) `
        "Lifecycle DryRun changed the generated root beneath canonical _local."
    $managerCleanup = Invoke-ArtifactLifecycleManager -Arguments @(
        "-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $managerProbe)
    Assert-StorageContract ($managerCleanup.ExitCode -eq 0 -and
        -not (Test-Path -LiteralPath $managerProbe)) `
        "The artifact lifecycle manager did not retire its exact eligible _local child. Output: $($managerCleanup.Output)"

    $gitMetadataProbe = Join-Path $markerProbe "lifecycle-git-metadata-negative"
    $null = New-HenkaLocalDirectory -RepositoryRoot $RepositoryRoot -Path $gitMetadataProbe
    $null = Write-HenkaGeneratedRootMarker `
        -RepoRoot $RepositoryRoot `
        -Path $gitMetadataProbe `
        -Purpose "local Git metadata preservation regression" `
        -RetentionClass "PROOF_CONSUMED" `
        -Active $false `
        -CleanupEligible $true `
        -CleanupCondition "proof_consumed"
    Set-Content -LiteralPath (Join-Path $gitMetadataProbe ".git") -Value "gitdir: protected-worktree"
    $gitMetadataResult = Invoke-ArtifactLifecycleManager -Arguments @(
        "-Mode", "DryRun", "-CandidatePath", $gitMetadataProbe)
    Assert-StorageContract ($gitMetadataResult.ExitCode -ne 0 -and $gitMetadataResult.Output -match "Git metadata") `
        "The artifact lifecycle manager did not reject a marked local root containing Git metadata. Output: $($gitMetadataResult.Output)"
    $gitMetadataCleanup = Invoke-ArtifactLifecycleManager -Arguments @(
        "-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $gitMetadataProbe)
    Assert-StorageContract ($gitMetadataCleanup.ExitCode -ne 0 -and $gitMetadataCleanup.Output -match "Git metadata" -and
        (Test-Path -LiteralPath $gitMetadataProbe -PathType Container)) `
        "Cleanup did not fail closed and preserve the local root containing Git metadata. Output: $($gitMetadataCleanup.Output)"
    Assert-StorageContract (Test-Path -LiteralPath (Join-Path $gitMetadataProbe ".git") -PathType Leaf) `
        "The Git metadata negative control was unexpectedly removed."

    Write-Output "[pass] Build, package, evidence, exact-candidate, and generated marker roots resolve beneath canonical _local."
    Write-Output "[pass] External root creation and traversal are rejected before filesystem mutation."
    Write-Output "[pass] Artifact lifecycle cleanup accepts exact generated children beneath _local and rejects Git metadata."
}
finally {
    if ($null -ne $reparseProbe -and (Test-Path -LiteralPath $reparseProbe)) {
        $reparseItem = Get-Item -LiteralPath $reparseProbe -Force -ErrorAction SilentlyContinue
        if ($null -ne $reparseItem -and
            ($reparseItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            [System.IO.Directory]::Delete($reparseProbe, $false)
        }
    }
    if ($null -ne $candidateProbe -and (Test-Path -LiteralPath $candidateProbe -PathType Container)) {
        Remove-Item -LiteralPath $candidateProbe -Recurse -Force
    }
    if ($null -ne $markerProbe -and (Test-Path -LiteralPath $markerProbe -PathType Container)) {
        Remove-Item -LiteralPath $markerProbe -Recurse -Force
    }
}
