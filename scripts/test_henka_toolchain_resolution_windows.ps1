[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$common = Join-Path $PSScriptRoot "henka_script_common.ps1"
if (-not (Test-Path -LiteralPath $common -PathType Leaf)) {
    throw "Henka script common module is missing: $common"
}
. $common

$repo = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$toolchain = Get-HenkaToolchain
if ($null -eq $toolchain) {
    throw "The Henka toolchain resolver returned no toolchain."
}

foreach ($property in @("CMakePath", "CTestPath", "CMakeVersion", "CTestVersion")) {
    if ([string]::IsNullOrWhiteSpace([string]$toolchain.$property)) {
        throw "The resolved Henka toolchain is missing $property."
    }
}

foreach ($path in @($toolchain.CMakePath, $toolchain.CTestPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Resolved Henka toolchain executable does not exist: $path"
    }
}

$cmakeDirectory = [System.IO.Path]::GetDirectoryName([string]$toolchain.CMakePath)
$ctestDirectory = [System.IO.Path]::GetDirectoryName([string]$toolchain.CTestPath)
if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals($cmakeDirectory, $ctestDirectory)) {
    throw "CMake and CTest were resolved from different toolchain directories."
}
if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals(
        [string](Get-HenkaCMakePath), [string]$toolchain.CMakePath)) {
    throw "Get-HenkaCMakePath did not use the resolved toolchain authority."
}
if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals(
        [string](Get-HenkaCTestPath -CMakePath $toolchain.CMakePath), [string]$toolchain.CTestPath)) {
    throw "Get-HenkaCTestPath did not use the resolved CMake/CTest pair."
}

$cmakeVersionOutput = @(& $toolchain.CMakePath --version 2>&1)
if ($LASTEXITCODE -ne 0 -or $cmakeVersionOutput.Count -eq 0) {
    throw "Resolved CMake did not report a valid version."
}
$ctestVersionOutput = @(& $toolchain.CTestPath --version 2>&1)
if ($LASTEXITCODE -ne 0 -or $ctestVersionOutput.Count -eq 0) {
    throw "Resolved CTest did not report a valid version."
}
if ([string]$toolchain.CMakeVersion -notmatch "^cmake version\s+\S+" -or
    [string]$toolchain.CTestVersion -notmatch "^ctest version\s+\S+") {
    throw "The resolver recorded malformed CMake/CTest version output."
}
if (([string]$toolchain.CMakeVersion -replace "^cmake version\s+", "") -ne
    ([string]$toolchain.CTestVersion -replace "^ctest version\s+", "")) {
    throw "CMake and CTest did not report the same toolchain version."
}

$fakeRoot = Join-Path $repo "build\test_tmp\toolchain-resolution-negative-control"
$fakeCMake = Join-Path $fakeRoot "cmake.exe"
try {
    New-Item -ItemType Directory -Path $fakeRoot -Force | Out-Null
    [System.IO.File]::WriteAllText($fakeCMake, "not an executable")
    $failed = $false
    try {
        Get-HenkaCTestPath -CMakePath $fakeCMake | Out-Null
    }
    catch {
        $failed = $true
    }
    if (-not $failed) {
        throw "The resolver accepted a CMake path without its paired CTest executable."
    }
}
finally {
    if (Test-Path -LiteralPath $fakeRoot) {
        Remove-Item -LiteralPath $fakeRoot -Recurse -Force -ErrorAction Stop
    }
}

Write-Output ("Henka toolchain resolution passed: cmake={0}; ctest={1}; version={2}" -f
    $toolchain.CMakePath, $toolchain.CTestPath, $toolchain.CMakeVersion)
Write-Output "Henka toolchain resolution negative control passed."
exit 0
