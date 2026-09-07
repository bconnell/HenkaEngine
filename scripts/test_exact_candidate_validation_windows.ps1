[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$wrapper = Join-Path $PSScriptRoot "validate_exact_candidate_windows.ps1"
$fixture = Join-Path $repoRoot ("build\test_tmp\exact-candidate-validation-fixture-" + [guid]::NewGuid().ToString("N"))
$candidate = Join-Path $fixture "candidate"
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
param([ValidateSet("Debug", "Release")][string]`$Configuration = "Debug", [string]`$DependencyRoot = "", [string]`$TestFilter = "")
Add-Content -LiteralPath '$logLiteral' -Value ("test|" + `$Configuration + "|" + `$DependencyRoot + "|" + `$TestFilter)
exit 0
"@
    Write-FixtureScript -Path (Join-Path $candidateScripts "package_sandbox3d_windows.ps1") -Body @"
param([ValidateSet("Debug", "Release")][string]`$Configuration = "Debug")
Add-Content -LiteralPath '$logLiteral' -Value ("package|" + `$Configuration)
exit 0
"@

    $wrapperArguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $wrapper,
        "-RepositoryRoot", $repoRoot,
        "-CandidatePath", $candidate,
        "-Configuration", "Debug",
        "-DependencyRoot", $dependencyRoot
    )
    $success = Invoke-ValidationWrapper -Arguments $wrapperArguments
    if ($success.ExitCode -ne 0) {
        throw "The exact-candidate wrapper unexpectedly failed: $($success.Output -join [Environment]::NewLine)"
    }

    $expectedCalls = @(
        "build|Debug|$dependencyRoot|",
        "test|Debug|$dependencyRoot|",
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
        "test|Debug|$dependencyRoot|^henka_tests$"
    )
    for ($index = 0; $index -lt $expectedFocusedCalls.Count; $index++) {
        if ($focusedCalls[$index] -ne $expectedFocusedCalls[$index]) {
            throw "Unexpected focused exact-candidate stage $($index): '$($focusedCalls[$index])'. Expected '$($expectedFocusedCalls[$index])'."
        }
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

    Write-Host "Exact-candidate orchestration regression passed."
} finally {
    if (Test-Path -LiteralPath $fixture) {
        Remove-Item -LiteralPath $fixture -Recurse -Force
    }
}
