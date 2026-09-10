[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,

    [Parameter(Mandatory = $true)]
    [string]$CandidatePath,

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$DependencyRoot = "",

    [string]$BuildTarget = "",

    [string]$TestFilter = "",

    [switch]$RequirePackage,

    [switch]$SkipPackage
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repository = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $RepositoryRoot).Path)
$candidate = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $CandidatePath).Path)
if ($repository.TrimEnd('\', '/') -eq $candidate.TrimEnd('\', '/')) {
    throw "The exact candidate must be distinct from the canonical repository."
}
if (-not (Test-Path -LiteralPath $candidate -PathType Container)) {
    throw "The exact candidate directory was not found: $candidate"
}

$candidateCommon = Join-Path $candidate "scripts\henka_script_common.ps1"
if (-not (Test-Path -LiteralPath $candidateCommon -PathType Leaf)) {
    throw "The exact candidate is missing scripts\henka_script_common.ps1."
}
. $candidateCommon

$dependency = $DependencyRoot
if ([string]::IsNullOrWhiteSpace($dependency)) {
    $dependency = Join-Path $repository "build\_deps"
} else {
    $dependency = [System.IO.Path]::GetFullPath($dependency)
}
if (-not (Test-Path -LiteralPath $dependency -PathType Container)) {
    throw "The exact-candidate dependency root was not found: $dependency"
}

$buildScript = Join-Path $candidate "scripts\build_windows.ps1"
$testScript = Join-Path $candidate "scripts\test_windows.ps1"
$packageScript = Join-Path $candidate "scripts\package_sandbox3d_windows.ps1"
foreach ($script in @($buildScript, $testScript, $packageScript)) {
    if (-not (Test-Path -LiteralPath $script -PathType Leaf)) {
        throw "The exact candidate is missing required validation script: $script"
    }
}

$candidateTestTemporaryRoot = Join-Path $candidate "build\test_tmp"
if (-not (Test-Path -LiteralPath $candidateTestTemporaryRoot -PathType Container)) {
    New-Item -ItemType Directory -Path $candidateTestTemporaryRoot -Force | Out-Null
}

$buildArguments = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-Configuration", $Configuration,
    "-DependencyRoot", $dependency
)
if (-not [string]::IsNullOrWhiteSpace($BuildTarget)) {
    $buildArguments += @("-BuildTarget", $BuildTarget)
}

$testArguments = @(
    "-NoProfile",
    "-ExecutionPolicy", "Bypass",
    "-Configuration", $Configuration,
    "-DependencyRoot", $dependency
)
if (-not [string]::IsNullOrWhiteSpace($TestFilter)) {
    $testArguments += @("-TestFilter", $TestFilter)
}
$testArguments += "-SkipBuild"

Invoke-HenkaNative `
    -FilePath "powershell.exe" `
    -Arguments (@("-File", $buildScript) + $buildArguments) `
    -WorkingDirectory $candidate `
    -Label "Build exact Henka candidate"

Invoke-HenkaNative `
    -FilePath "powershell.exe" `
    -Arguments (@("-File", $testScript) + $testArguments) `
    -WorkingDirectory $candidate `
    -Label "Test exact Henka candidate"

if (-not $SkipPackage -and ([string]::IsNullOrWhiteSpace($BuildTarget) -or $RequirePackage)) {
    Invoke-HenkaNative `
        -FilePath "powershell.exe" `
        -Arguments @(
            "-File", $packageScript,
            "-Configuration", $Configuration) `
        -WorkingDirectory $candidate `
        -Label "Package exact Henka candidate"
}

Write-Host "Exact candidate validation passed."
Write-Host "candidate: $candidate"
Write-Host "configuration: $Configuration"
Write-Host "dependency root: $dependency"
if (-not [string]::IsNullOrWhiteSpace($BuildTarget)) {
    Write-Host "build target: $BuildTarget"
}
if (-not [string]::IsNullOrWhiteSpace($TestFilter)) {
    Write-Host "test filter: $TestFilter"
}
if ($SkipPackage -or (-not [string]::IsNullOrWhiteSpace($BuildTarget) -and -not $RequirePackage)) {
    Write-Host "package: skipped by explicit option"
}
