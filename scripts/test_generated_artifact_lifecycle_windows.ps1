Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$manager = Join-Path $PSScriptRoot "manage_generated_artifacts_windows.ps1"
$captureScript = Join-Path $PSScriptRoot "capture_visual_evidence_windows.ps1"
$buildRoot = Get-HenkaBuildRoot -RepositoryRoot $repoRoot
$localRoot = Get-HenkaLocalRoot -RepositoryRoot $repoRoot
$testTemporaryRoot = Get-HenkaTestTemporaryRoot -RepositoryRoot $repoRoot
$captureExecutable = Join-Path $buildRoot "examples\sandbox3d\Debug\henka_sandbox3d.exe"
$captureRuntimeRoot = $testTemporaryRoot
$testRoot = Join-Path $testTemporaryRoot ("gal-" + [Guid]::NewGuid().ToString("N"))
$outside = Join-Path $localRoot ("generated-artifact-lifecycle-outside-" + [Guid]::NewGuid().ToString("N"))
$junction = Join-Path $testRoot "junction"
$captureProbeNewPaths = @()

function Get-VisualCaptureRuntimePaths {
    if (-not (Test-Path -LiteralPath $captureRuntimeRoot -PathType Container)) {
        return @()
    }

    return @(
        Get-ChildItem -LiteralPath $captureRuntimeRoot -Directory -Filter "visual-evidence-runtime-*" -Force -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty FullName
    )
}

function Write-TestMarker {
    param(
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [string]$RetentionClass,
        [Parameter(Mandatory = $true)] [bool]$Active,
        [Parameter(Mandatory = $true)] [bool]$CleanupEligible,
        [string]$CleanupCondition = "",
        [string]$Purpose = "bounded cleanup contract regression",
        [string]$SourceSha = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    )

    $marker = [ordered]@{
        schema_version = 1
        owner = "generated-artifact-lifecycle-regression"
        purpose = $Purpose
        source_sha = $SourceSha
        configuration = "Debug"
        created_utc = [DateTime]::UtcNow.ToString("o")
        retention_class = $RetentionClass
        cleanup_owner = "generated-artifact-lifecycle-regression"
        cleanup_condition = if (-not [string]::IsNullOrWhiteSpace($CleanupCondition)) {
            $CleanupCondition
        } elseif ($CleanupEligible) { "proof_consumed" } else { "active_or_protected" }
        active = $Active
        cleanup_eligible = $CleanupEligible
    }
    $marker | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Path ".henka-generated.json")
}

function Invoke-Manager {
    param(
        [Parameter(Mandatory = $true)] [string[]]$Arguments
    )
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $manager @Arguments 2>&1)
        return [pscustomobject]@{
            ExitCode = $LASTEXITCODE
            Output = (($output | ForEach-Object { [string]$_ }) -join "`n")
        }
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
}

function Assert-ManagerFailure {
    param(
        [Parameter(Mandatory = $true)] [string[]]$Arguments,
        [Parameter(Mandatory = $true)] [string]$ExpectedText
    )
    $result = Invoke-Manager -Arguments $Arguments
    if ($result.ExitCode -eq 0 -or $result.Output -notmatch [regex]::Escape($ExpectedText)) {
        throw "Expected lifecycle manager failure containing '$ExpectedText'. Output: $($result.Output)"
    }
}

function New-CMakeFetchContentCleanupCandidate {
    param(
        [Parameter(Mandatory = $true)] [string]$Name,
        [Parameter(Mandatory = $true)] [string]$PinnedRevision,
        [switch]$DirtyDependency,
        [switch]$MismatchedDependency,
        [switch]$UnexpectedGitPath,
        [switch]$WrongOrigin
    )

    $candidate = Join-Path $testRoot $Name
    $sourceRoot = $repoRoot
    $dependencyRoot = Join-Path $candidate "_deps"
    $dependencyResolution = Resolve-HenkaBuildDependencyRoot `
        -RepositoryRoot $repoRoot `
        -BuildRoot $buildRoot `
        -Providers @("ENET")
    if ($null -eq $dependencyResolution) {
        throw "The canonical Henka build dependency resolver did not find its ENet fixture source."
    }
    $dependencyRootPath = switch ([string]$dependencyResolution.Mode) {
        "build-local" { [string]$dependencyResolution.Root; break }
        "shared" { Get-HenkaCMakeSharedDependencyRoot -RepositoryRoot $repoRoot; break }
        default { "" }
    }
    if ([string]::IsNullOrWhiteSpace($dependencyRootPath)) {
        throw "The canonical Henka build dependency resolver returned unsupported mode '$($dependencyResolution.Mode)' for the ENet fixture."
    }
    $resolvedDependency = Join-Path $dependencyRootPath "enet-src"
    if (-not (Test-Path -LiteralPath $resolvedDependency -PathType Container)) {
        throw "The resolved Henka ENet fixture source is unavailable: $resolvedDependency"
    }
    $dependencyClone = if ($UnexpectedGitPath.IsPresent) {
        Join-Path $candidate "unexpected-src"
    } else {
        Join-Path $dependencyRoot "enet-src"
    }
    New-Item -ItemType Directory -Path $candidate,$dependencyRoot -Force | Out-Null
    $gitPath = Get-HenkaGitPath
    $sourceSha = ([string](& $gitPath -C $sourceRoot rev-parse HEAD)).Trim()
    if ($LASTEXITCODE -ne 0 -or $sourceSha -notmatch "^[0-9a-f]{40}$") {
        throw "Could not identify the Henka CMake source fixture commit."
    }

    $fetchContentPins = Get-HenkaCMakeFetchContentPins -PinSourceRoot $sourceRoot
    if (-not $fetchContentPins.ContainsKey("ENET")) {
        throw "The Henka CMake source fixture has no ENet FetchContent declaration."
    }
    $fetchContentPin = $fetchContentPins["ENET"]
    if (-not $fetchContentPin.Immutable -or
        -not [string]::Equals([string]$fetchContentPin.Commit, $PinnedRevision, [System.StringComparison]::OrdinalIgnoreCase) -or
        [string]::IsNullOrWhiteSpace([string]$fetchContentPin.Repository)) {
        throw "The ENet test fixture does not match Henka's immutable FetchContent declaration."
    }
    $resolvedSourceValidation = Test-HenkaPinnedFetchContentSource `
        -RepositoryRoot $repoRoot `
        -SourceRoot $resolvedDependency `
        -Marker "CMakeLists.txt" `
        -PinnedCommit ([string]$fetchContentPin.Commit) `
        -PinnedRepository ([string]$fetchContentPin.Repository)
    if (-not $resolvedSourceValidation.Available) {
        throw "The resolved ENet source is not an exact clean mirror of Henka's pin: $($resolvedSourceValidation.RejectionReason)"
    }

    & $gitPath clone --quiet --local --no-hardlinks $resolvedDependency $dependencyClone
    if ($LASTEXITCODE -ne 0) { throw "Could not clone the shared pinned ENet dependency into the generated CMake fixture." }
    $fixtureOrigin = [string]$fetchContentPin.Repository
    if ($WrongOrigin.IsPresent) {
        $fixtureOrigin = $resolvedDependency
    }
    & $gitPath -C $dependencyClone remote set-url origin $fixtureOrigin
    if ($LASTEXITCODE -ne 0) { throw "Could not set the generated ENet fixture's origin metadata." }
    $actualFixtureOrigin = ([string](& $gitPath -C $dependencyClone remote get-url origin)).Trim()
    if ($LASTEXITCODE -ne 0 -or
        -not $actualFixtureOrigin.TrimEnd("/").Equals(
            $fixtureOrigin.TrimEnd("/"),
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "The generated ENet fixture origin does not match its requested test value."
    }
    if ($DirtyDependency.IsPresent) {
        Add-Content -LiteralPath (Join-Path $dependencyClone "CMakeLists.txt") -Value "`n# lifecycle dirty-source negative control"
    }
    if ($MismatchedDependency.IsPresent) {
        Add-Content -LiteralPath (Join-Path $dependencyClone "CMakeLists.txt") -Value "`n# lifecycle revision-mismatch negative control"
        & $gitPath -C $dependencyClone config user.name "Henka lifecycle regression"
        if ($LASTEXITCODE -ne 0) { throw "Could not configure the generated ENet dependency fixture identity." }
        & $gitPath -C $dependencyClone config user.email "henka-lifecycle-regression@example.invalid"
        if ($LASTEXITCODE -ne 0) { throw "Could not configure the generated ENet dependency fixture identity." }
        & $gitPath -C $dependencyClone add -- CMakeLists.txt
        if ($LASTEXITCODE -ne 0) { throw "Could not stage the generated ENet dependency fixture change." }
        & $gitPath -C $dependencyClone commit --quiet -m "fixture: mismatch pinned dependency revision"
        if ($LASTEXITCODE -ne 0) { throw "Could not create the generated ENet dependency mismatch." }
    }

    $cache = @(
        "CMAKE_HOME_DIRECTORY:INTERNAL=$($sourceRoot.Replace('\', '/'))",
        "FETCHCONTENT_FULLY_DISCONNECTED:BOOL=OFF"
    ) -join "`n"
    Set-Content -LiteralPath (Join-Path $candidate "CMakeCache.txt") -Value $cache -Encoding ASCII
    Write-TestMarker -Path $candidate -RetentionClass "REBUILDABLE" -Active $false -CleanupEligible $true `
        -CleanupCondition "source inputs are preserved and no validation process is using this build root" `
        -Purpose "CMake build output" -SourceSha $sourceSha
    return [pscustomobject]@{ Candidate = $candidate; SourceRoot = $sourceRoot; DependencyClone = $dependencyClone }
}

try {
    New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
    $eligible = Join-Path $testRoot "eligible"
    $active = Join-Path $testRoot "active"
    $unmarked = Join-Path $testRoot "unmarked"
    $legacyBuild = Join-Path $testRoot "legacy-rebuildable"
    $unknownCondition = Join-Path $testRoot "unknown-cleanup-condition"
    $commonMarkerRoot = Join-Path $testRoot "common-marker"
    New-Item -ItemType Directory -Path $eligible,$active,$unmarked,$legacyBuild,$unknownCondition,$outside -Force | Out-Null
    New-Item -ItemType Directory -Path $commonMarkerRoot -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $eligible "evidence.txt") -Value "eligible"
    Set-Content -LiteralPath (Join-Path $active "evidence.txt") -Value "active"
    Set-Content -LiteralPath (Join-Path $unmarked "evidence.txt") -Value "unmarked"
    Set-Content -LiteralPath (Join-Path $legacyBuild "evidence.txt") -Value "legacy but rebuildable"
    Set-Content -LiteralPath (Join-Path $unknownCondition "evidence.txt") -Value "must remain"
    Set-Content -LiteralPath (Join-Path $outside "evidence.txt") -Value "outside"
    $commonMarkerPath = Write-HenkaGeneratedRootMarker `
        -RepoRoot $repoRoot `
        -Path $commonMarkerRoot `
        -Purpose "common marker contract regression" `
        -RetentionClass "CACHE" `
        -Active $false `
        -CleanupEligible $false `
        -CleanupCondition "retained_for_test" `
        -Configuration "Debug"
    if (-not (Test-Path -LiteralPath $commonMarkerPath -PathType Leaf)) {
        throw "The common generated-root marker helper did not write its marker."
    }
    Write-TestMarker -Path $eligible -RetentionClass "PROOF_CONSUMED" -Active $false -CleanupEligible $true
    Write-TestMarker -Path $active -RetentionClass "ACTIVE_CANDIDATE" -Active $true -CleanupEligible $false
    Write-TestMarker -Path $legacyBuild -RetentionClass "REBUILDABLE" -Active $false -CleanupEligible $true `
        -CleanupCondition "source inputs are preserved and no validation process is using this build root"
    Write-TestMarker -Path $unknownCondition -RetentionClass "REBUILDABLE" -Active $false -CleanupEligible $true `
        -CleanupCondition "old directory; assume it can be deleted"

    $enetPins = Get-HenkaCMakeDependencyPins -RepositoryRoot $repoRoot -Providers @("ENET")
    $enetPin = [string]$enetPins.ENET
    if ($enetPin -notmatch "^[0-9a-f]{40}$") {
        throw "The lifecycle regression could not read Henka's pinned ENet revision."
    }
    $fetchContentCleanup = New-CMakeFetchContentCleanupCandidate -Name "cmfc-good" -PinnedRevision $enetPin
    $fetchContentDryRun = Invoke-Manager -Arguments @("-Mode", "DryRun", "-CandidatePath", $fetchContentCleanup.Candidate)
    if ($fetchContentDryRun.ExitCode -ne 0 -or
        $fetchContentDryRun.Output -notmatch "git_metadata_entries=1" -or
        $fetchContentDryRun.Output -notmatch "bytes=") {
        throw "A CMake build with an exact clean dependency mirror was not accepted for cleanup. Output: $($fetchContentDryRun.Output)"
    }
    if (-not (Test-Path -LiteralPath $fetchContentCleanup.Candidate -PathType Container)) {
        throw "CMake dependency cleanup dry-run altered its candidate."
    }
    $fetchContentCleanupResult = Invoke-Manager -Arguments @(
        "-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $fetchContentCleanup.Candidate
    )
    if ($fetchContentCleanupResult.ExitCode -ne 0 -or (Test-Path -LiteralPath $fetchContentCleanup.Candidate)) {
        throw "A safe CMake build dependency clone was not retired by exact cleanup. Output: $($fetchContentCleanupResult.Output)"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $localRoot "dependencies\enet-src") -PathType Container)) {
        throw "CMake build cleanup removed or damaged the shared dependency source."
    }

    $wrongOriginFetchContent = New-CMakeFetchContentCleanupCandidate -Name "cmfc-wrong-origin" `
        -PinnedRevision $enetPin -WrongOrigin
    Assert-ManagerFailure -Arguments @(
        "-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $wrongOriginFetchContent.Candidate
    ) -ExpectedText "origin does not match its pinned source declaration"

    $dirtyFetchContent = New-CMakeFetchContentCleanupCandidate -Name "cmfc-dirty" -PinnedRevision $enetPin -DirtyDependency
    Assert-ManagerFailure -Arguments @(
        "-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $dirtyFetchContent.Candidate
    ) -ExpectedText "dependency checkout is not a clean pinned source"

    $mismatchedFetchContent = New-CMakeFetchContentCleanupCandidate -Name "cmfc-mismatch" `
        -PinnedRevision $enetPin -MismatchedDependency
    Assert-ManagerFailure -Arguments @(
        "-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $mismatchedFetchContent.Candidate
    ) -ExpectedText "dependency checkout is not a clean pinned source"

    $unexpectedFetchContent = New-CMakeFetchContentCleanupCandidate -Name "cmfc-stray" `
        -PinnedRevision $enetPin -UnexpectedGitPath
    Assert-ManagerFailure -Arguments @(
        "-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $unexpectedFetchContent.Candidate
    ) -ExpectedText "Git metadata is outside a recognized CMake FetchContent source"

    if (-not (Test-Path -LiteralPath $captureExecutable -PathType Leaf) -or
        -not (Test-Path -LiteralPath (Join-Path (Split-Path -Parent $captureExecutable) "assets") -PathType Container)) {
        throw "The capture-runtime cleanup regression requires the built Debug Sandbox3D and its assets."
    }
    $captureBefore = @(Get-VisualCaptureRuntimePaths)
    $captureErrorActionPreference = $ErrorActionPreference
    try {
        # This subprocess is expected to fail its deliberate FULL_SHOWCASE
        # precondition. Keep the nonzero result inspectable on hosted Windows
        # PowerShell instead of promoting it to a terminating native-command
        # exception before the assertions below can run.
        $ErrorActionPreference = "Continue"
        $captureOutput = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $captureScript `
            -ExecutablePath $captureExecutable `
            -EvidenceProfile "FULL_SHOWCASE" `
            -OutputDirectory $testRoot 2>&1)
        $captureExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $captureErrorActionPreference
    }
    $captureAfter = @(Get-VisualCaptureRuntimePaths)
    $captureProbeNewPaths = @($captureAfter | Where-Object { $captureBefore -notcontains $_ })
    $captureOutputText = (($captureOutput | ForEach-Object { [string]$_ }) -join "`n")
    if ($captureExitCode -eq 0 -or
        $captureOutputText -notmatch "FULL_SHOWCASE evidence requires" -or
        $captureProbeNewPaths.Count -ne 0) {
        throw "Visual capture runtime cleanup regression failed. Exit=$captureExitCode; NewPaths=$($captureProbeNewPaths -join ', '); Output=$captureOutputText"
    }

    $dryRun = Invoke-Manager -Arguments @("-Mode", "DryRun", "-CandidatePath", $eligible)
    if ($dryRun.ExitCode -ne 0 -or $dryRun.Output -notmatch "cleanup_eligible=True" -or $dryRun.Output -notmatch "bytes=") {
        throw "Eligible generated artifact was not accepted by dry-run. Output: $($dryRun.Output)"
    }
    if (-not (Test-Path -LiteralPath $eligible -PathType Container)) {
        throw "Dry-run altered the eligible generated artifact."
    }
    $legacyDryRun = Invoke-Manager -Arguments @("-Mode", "DryRun", "-CandidatePath", $legacyBuild)
    if ($legacyDryRun.ExitCode -ne 0 -or $legacyDryRun.Output -notmatch "cleanup_eligible=True") {
        throw "The recognized legacy rebuildable marker was not accepted. Output: $($legacyDryRun.Output)"
    }
    $report = Invoke-Manager -Arguments @("-Mode", "Report", "-RootPath", $testRoot)
    if ($report.ExitCode -ne 0 -or $report.Output -notmatch "class=CACHE" -or $report.Output -notmatch "class=UNMARKED") {
        throw "The lifecycle report did not expose marked and unmarked roots. Output: $($report.Output)"
    }

    Assert-ManagerFailure -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $active) -ExpectedText "active"
    Assert-ManagerFailure -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $unmarked) -ExpectedText "unmarked"
    Assert-ManagerFailure -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $commonMarkerRoot) -ExpectedText "retention class 'CACHE' is not cleanup-eligible"
    Assert-ManagerFailure -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $unknownCondition) -ExpectedText "not an approved retirement condition"
    Assert-ManagerFailure -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $outside) -ExpectedText "canonical local or cleanup-only legacy generated roots"
    Assert-ManagerFailure -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", (Join-Path $localRoot "bld")) -ExpectedText "approved generated root itself"

    $cleanup = Invoke-Manager -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $eligible)
    if ($cleanup.ExitCode -ne 0 -or (Test-Path -LiteralPath $eligible)) {
        throw "Eligible generated artifact was not removed by exact cleanup. Output: $($cleanup.Output)"
    }
    $legacyCleanup = Invoke-Manager -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $legacyBuild)
    if ($legacyCleanup.ExitCode -ne 0 -or (Test-Path -LiteralPath $legacyBuild)) {
        throw "Recognized legacy rebuildable output was not retired by exact cleanup. Output: $($legacyCleanup.Output)"
    }
    if (-not (Test-Path -LiteralPath $active -PathType Container) -or
        -not (Test-Path -LiteralPath $unmarked -PathType Container) -or
        -not (Test-Path -LiteralPath $unknownCondition -PathType Container) -or
        -not (Test-Path -LiteralPath $outside -PathType Container)) {
        throw "Cleanup removed a protected, unmarked, or outside artifact."
    }

    $junctionTarget = Join-Path $testRoot "junction-target"
    New-Item -ItemType Directory -Path $junctionTarget -Force | Out-Null
    $junctionCreated = $false
    try {
        New-Item -ItemType Junction -Path $junction -Target $junctionTarget -ErrorAction Stop | Out-Null
        $junctionCreated = $true
    }
    catch {
        Write-Host "[skip] Reparse-point creation is unavailable in this environment; path rejection remains covered by the manager's safe-path contract."
    }
    if ($junctionCreated) {
        Write-TestMarker -Path $junctionTarget -RetentionClass "PROOF_CONSUMED" -Active $false -CleanupEligible $true
        Assert-ManagerFailure -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $junction) -ExpectedText "reparse"
    }

    Write-Output "Visual capture runtime cleanup regression passed."
    Write-Output "Generated artifact lifecycle regression tests passed."
}
finally {
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        if (Test-Path -LiteralPath $junction) {
            $junctionItem = Get-Item -LiteralPath $junction -Force -ErrorAction SilentlyContinue
            if ($null -ne $junctionItem -and
                ($junctionItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                [System.IO.Directory]::Delete($junction, $false)
            }
        }
        foreach ($capturePath in @($captureProbeNewPaths)) {
            if (Test-Path -LiteralPath $capturePath -PathType Container) {
                $captureItem = Get-Item -LiteralPath $capturePath -Force -ErrorAction SilentlyContinue
                if ($null -ne $captureItem -and
                    ($captureItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -eq 0) {
                    [System.IO.Directory]::Delete($capturePath, $true)
                }
            }
        }
        if (Test-Path -LiteralPath $testRoot) {
            foreach ($ownedFile in @(Get-ChildItem -LiteralPath $testRoot -File -Force -Recurse -ErrorAction SilentlyContinue)) {
                if (($ownedFile.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -eq 0 -and
                    ($ownedFile.Attributes -band [System.IO.FileAttributes]::ReadOnly) -ne 0) {
                    [System.IO.File]::SetAttributes(
                        $ownedFile.FullName,
                        $ownedFile.Attributes -band (-bnot [System.IO.FileAttributes]::ReadOnly)
                    )
                }
            }
            [System.IO.Directory]::Delete($testRoot, $true)
        }
        if (Test-Path -LiteralPath $outside) {
            [System.IO.Directory]::Delete($outside, $true)
        }
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
}

# Expected negative-control subprocesses leave a non-zero LASTEXITCODE in the
# hosting PowerShell session.  The regression itself is green only after its
# scoped cleanup has completed, so make that result explicit to CI callers.
exit 0
