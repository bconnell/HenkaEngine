[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$wrapper = Join-Path $PSScriptRoot "validate_exact_candidate_windows.ps1"
$fixture = Join-Path (Get-HenkaTestTemporaryRoot -RepositoryRoot $repoRoot) ("exact-candidate-validation-fixture-" + [guid]::NewGuid().ToString("N"))
$exactCandidateRoot = Get-HenkaExactCandidateRoot -RepositoryRoot $repoRoot
$candidate = Join-Path $exactCandidateRoot ("e2cv-" + [guid]::NewGuid().ToString("N"))
$nestedCandidateFixture = Join-Path $exactCandidateRoot ("e2nested-" + [guid]::NewGuid().ToString("N"))
$candidateScripts = Join-Path $candidate "scripts"
$dependencyRoot = Join-Path $fixture "dependencies"
$logPath = Join-Path $fixture "calls.log"

function Write-FixtureScript {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Body
    )
    [System.IO.File]::WriteAllText($Path, $Body, [System.Text.UTF8Encoding]::new($false))
}

function Invoke-ValidationWrapper {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(& powershell.exe @Arguments 2>&1 | ForEach-Object { [string]$_ })
        $exitCode = [int]$LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = $output
    }
}

try {
    New-Item -ItemType Directory -Path $candidateScripts -Force | Out-Null
    New-Item -ItemType Directory -Path $dependencyRoot -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "henka_script_common.ps1") -Destination (Join-Path $candidateScripts "henka_script_common.ps1")

    $logLiteral = $logPath.Replace("'", "''")
    Write-FixtureScript -Path (Join-Path $candidateScripts "build_windows.ps1") -Body @"
param([ValidateSet("Debug", "Release")][string]`$Configuration = "Debug", [string]`$DependencyRoot = "", [string]`$BuildTarget = "")
Add-Content -LiteralPath '$logLiteral' -Value ("build|" + `$Configuration + "|" + `$DependencyRoot + "|" + `$BuildTarget)
exit 0
"@
    Write-FixtureScript -Path (Join-Path $candidateScripts "test_windows.ps1") -Body @"
param([ValidateSet("Debug", "Release")][string]`$Configuration = "Debug", [string]`$DependencyRoot = "", [string]`$TestFilter = "", [switch]`$SkipBuild)
Add-Content -LiteralPath '$logLiteral' -Value ("test|" + `$Configuration + "|" + `$DependencyRoot + "|" + `$TestFilter + "|skipbuild=" + [bool]`$SkipBuild)
exit 0
"@
    Write-FixtureScript -Path (Join-Path $candidateScripts "package_sandbox3d_windows.ps1") -Body @"
param([ValidateSet("Debug", "Release")][string]`$Configuration = "Debug")
Add-Content -LiteralPath '$logLiteral' -Value ("package|" + `$Configuration)
exit 0
"@

    # The candidate scripts emit generated-root provenance while capturing
    # subprocess output, so the fixture must be a real local Git repository,
    # not an unversioned directory pretending to be an exact candidate.
    $git = Get-HenkaGitPath
    & $git -C $candidate init --quiet 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Could not initialize the isolated exact-candidate test fixture." }
    & $git -C $candidate -c core.autocrlf=false -c user.name="Henka storage regression" -c user.email="henka-storage-regression@invalid" add -- scripts 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Could not stage the isolated exact-candidate test fixture scripts." }
    & $git -C $candidate -c user.name="Henka storage regression" -c user.email="henka-storage-regression@invalid" commit --quiet -m "test fixture" 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Could not establish source provenance for the exact-candidate test fixture." }

    $wrapperArguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $wrapper,
        "-RepositoryRoot", $repoRoot,
        "-CandidatePath", $candidate,
        "-Configuration", "Debug",
        "-DependencyRoot", $dependencyRoot,
        "-StageTimeoutMilliseconds", "5000"
    )
    $success = Invoke-ValidationWrapper -Arguments $wrapperArguments
    if ($success.ExitCode -ne 0) {
        throw "The exact-candidate wrapper unexpectedly failed: $($success.Output -join [Environment]::NewLine)"
    }

    $expectedCalls = @(
        "build|Debug|$dependencyRoot|",
        "test|Debug|$dependencyRoot||skipbuild=True",
        "package|Debug"
    )
    $actualCalls = @(Get-Content -LiteralPath $logPath)
    if ($actualCalls.Count -ne $expectedCalls.Count) {
        throw "The exact-candidate wrapper did not invoke the expected number of stages."
    }
    for ($index = 0; $index -lt $expectedCalls.Count; $index++) {
        if ($actualCalls[$index] -ne $expectedCalls[$index]) {
            throw "Unexpected exact-candidate stage $($index): '$($actualCalls[$index])'. Expected '$($expectedCalls[$index])'."
        }
    }

    $externalDependencyProbe = Join-Path $env:SystemDrive ("henka-exact-dependency-negative-" + [guid]::NewGuid().ToString("N"))
    $externalDependencyArguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $wrapper,
        "-RepositoryRoot", $repoRoot,
        "-CandidatePath", $candidate,
        "-Configuration", "Debug",
        "-DependencyRoot", $externalDependencyProbe,
        "-StageTimeoutMilliseconds", "5000"
    )
    $externalDependencyFailure = Invoke-ValidationWrapper -Arguments $externalDependencyArguments
    if ($externalDependencyFailure.ExitCode -eq 0 -or
        ($externalDependencyFailure.Output -join [Environment]::NewLine) -notmatch "outside|escape|local root|canonical" -or
        (Get-Content -LiteralPath $logPath).Count -ne $actualCalls.Count -or
        (Test-Path -LiteralPath $externalDependencyProbe)) {
        throw "The exact-candidate wrapper did not reject its external dependency root before invoking a validation stage or creating it. Output: $($externalDependencyFailure.Output -join [Environment]::NewLine)"
    }

    $nestedCandidate = Join-Path $nestedCandidateFixture "candidate"
    $nestedCandidateScripts = Join-Path $nestedCandidate "scripts"
    New-Item -ItemType Directory -Path $nestedCandidateScripts -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "henka_script_common.ps1") -Destination (Join-Path $nestedCandidateScripts "henka_script_common.ps1")
    $nestedCandidateArguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $wrapper,
        "-RepositoryRoot", $repoRoot,
        "-CandidatePath", $nestedCandidate,
        "-Configuration", "Debug",
        "-DependencyRoot", $dependencyRoot,
        "-StageTimeoutMilliseconds", "5000"
    )
    $nestedCandidateFailure = Invoke-ValidationWrapper -Arguments $nestedCandidateArguments
    if ($nestedCandidateFailure.ExitCode -eq 0 -or
        ($nestedCandidateFailure.Output -join [Environment]::NewLine) -notmatch "direct child.*exact-candidates" -or
        (Get-Content -LiteralPath $logPath).Count -ne $actualCalls.Count) {
        throw "The validator did not reject a nested candidate that is not a direct child of _local\\exact-candidates before staging or invoking it. Output: $($nestedCandidateFailure.Output -join [Environment]::NewLine)"
    }

    $focusedWrapperArguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $wrapper,
        "-RepositoryRoot", $repoRoot,
        "-CandidatePath", $candidate,
        "-Configuration", "Debug",
        "-DependencyRoot", $dependencyRoot,
        "-BuildTarget", "henka_tests",
        "-TestFilter", "^henka_tests$",
        "-SkipPackage"
    )
    $focused = Invoke-ValidationWrapper -Arguments $focusedWrapperArguments
    if ($focused.ExitCode -ne 0) {
        throw "The focused exact-candidate wrapper unexpectedly failed: $($focused.Output -join [Environment]::NewLine)"
    }
    $focusedCalls = @(Get-Content -LiteralPath $logPath | Select-Object -Last 2)
    $expectedFocusedCalls = @(
        "build|Debug|$dependencyRoot|henka_tests",
        "test|Debug|$dependencyRoot|^henka_tests$|skipbuild=True"
    )
    for ($index = 0; $index -lt $expectedFocusedCalls.Count; $index++) {
        if ($focusedCalls[$index] -ne $expectedFocusedCalls[$index]) {
            throw "Unexpected focused exact-candidate stage $($index): '$($focusedCalls[$index])'. Expected '$($expectedFocusedCalls[$index])'."
        }
    }

    $targetAwareArguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $wrapper,
        "-RepositoryRoot", $repoRoot,
        "-CandidatePath", $candidate,
        "-Configuration", "Debug",
        "-DependencyRoot", $dependencyRoot,
        "-BuildTarget", "henka_tests",
        "-TestFilter", "^henka_tests$"
    )
    $targetAware = Invoke-ValidationWrapper -Arguments $targetAwareArguments
    if ($targetAware.ExitCode -ne 0) {
        throw "The target-aware exact-candidate wrapper unexpectedly failed: $($targetAware.Output -join [Environment]::NewLine)"
    }
    $targetAwareCalls = @(Get-Content -LiteralPath $logPath | Select-Object -Last 2)
    $expectedTargetAwareCalls = @(
        "build|Debug|$dependencyRoot|henka_tests",
        "test|Debug|$dependencyRoot|^henka_tests$|skipbuild=True"
    )
    for ($index = 0; $index -lt $expectedTargetAwareCalls.Count; $index++) {
        if ($targetAwareCalls[$index] -ne $expectedTargetAwareCalls[$index]) {
            throw "Unexpected target-aware exact-candidate stage $($index): '$($targetAwareCalls[$index])'. Expected '$($expectedTargetAwareCalls[$index])'."
        }
    }
    if (($targetAware.Output -join [Environment]::NewLine) -notmatch "package: skipped") {
        throw "A focused build target unexpectedly required unrelated package staging."
    }

    $invalidTargetArguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $wrapper,
        "-RepositoryRoot", $repoRoot,
        "-CandidatePath", $candidate,
        "-Configuration", "Debug",
        "-DependencyRoot", $dependencyRoot,
        "-BuildTarget", "sandbox"
    )
    $invalidTarget = Invoke-ValidationWrapper -Arguments $invalidTargetArguments
    if ($invalidTarget.ExitCode -eq 0) {
        throw "The exact-candidate wrapper accepted the unsupported sandbox build target alias."
    }
    $invalidTargetText = $invalidTarget.Output -join [Environment]::NewLine
    if ($invalidTargetText -notmatch "No target-aware provenance artifact mapping exists") {
        throw "The unsupported build target failure did not identify the target mapping boundary: $invalidTargetText"
    }
    if ((Get-Content -LiteralPath $logPath).Count -ne $actualCalls.Count + $focusedCalls.Count + $targetAwareCalls.Count) {
        throw "The unsupported build target reached a candidate validation stage before failing preflight."
    }

    Write-FixtureScript -Path (Join-Path $candidateScripts "package_sandbox3d_windows.ps1") -Body @"
param([ValidateSet("Debug", "Release")][string]`$Configuration = "Debug")
Add-Content -LiteralPath '$logLiteral' -Value ("package-failure|" + `$Configuration)
exit 17
"@
    $failure = Invoke-ValidationWrapper -Arguments $wrapperArguments
    if ($failure.ExitCode -eq 0) {
        throw "The exact-candidate wrapper accepted a failing package stage."
    }
    if (-not (($failure.Output -join [Environment]::NewLine) -match "17")) {
        throw "The exact-candidate wrapper did not report the failing package stage exit code."
    }

    # The package failure is an intentional negative control. Do not leak its
    # expected native exit code into the enclosing PowerShell process.
    $global:LASTEXITCODE = 0

    Write-FixtureScript -Path (Join-Path $candidateScripts "build_windows.ps1") -Body @"
param([ValidateSet("Debug", "Release")][string]`$Configuration = "Debug", [string]`$DependencyRoot = "", [string]`$BuildTarget = "")
Write-Output "exact-candidate-timeout-probe-ready"
Start-Sleep -Seconds 30
exit 0
"@
    $timeoutArguments = @($wrapperArguments)
    $timeoutIndex = [Array]::IndexOf($timeoutArguments, "-StageTimeoutMilliseconds")
    if ($timeoutIndex -lt 0 -or $timeoutIndex + 1 -ge $timeoutArguments.Count) {
        throw "The exact-candidate timeout regression could not locate the stage timeout argument."
    }
    $timeoutArguments[$timeoutIndex + 1] = "1000"
    $timeoutStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $timeoutFailure = Invoke-ValidationWrapper -Arguments $timeoutArguments
    $timeoutStopwatch.Stop()
    $timeoutText = $timeoutFailure.Output -join [Environment]::NewLine
    if ($timeoutFailure.ExitCode -eq 0 -or
        $timeoutText -notmatch "Build exact Henka candidate exceeded timeout 1000ms" -or
        $timeoutStopwatch.ElapsedMilliseconds -gt 10000) {
        throw "The exact-candidate stage timeout did not fail closed within its cleanup budget: exit=$($timeoutFailure.ExitCode) elapsed=$($timeoutStopwatch.ElapsedMilliseconds)ms output=$timeoutText"
    }
    Write-Host "[pass] Exact-candidate stages terminate their process tree on timeout."

    # The timeout failure is an intentional negative control. Reset the native
    # process exit status so the enclosing PowerShell regression reports its
    # own assertion result instead of leaking the expected child failure.
    $global:LASTEXITCODE = 0

    Write-Host "Exact-candidate orchestration regression passed."
} finally {
    if (Test-Path -LiteralPath $nestedCandidateFixture) {
        Remove-Item -LiteralPath $nestedCandidateFixture -Recurse -Force
    }
    if (Test-Path -LiteralPath $fixture) {
        Remove-Item -LiteralPath $fixture -Recurse -Force
    }
    if (Test-Path -LiteralPath $candidate) {
        Remove-Item -LiteralPath $candidate -Recurse -Force
    }
}
