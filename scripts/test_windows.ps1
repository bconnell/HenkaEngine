Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$buildRoot = Join-Path $repoRoot "build"
$cmake = Get-HenkaCMakePath
$ctest = Get-HenkaCTestPath -CMakePath $cmake

Write-Host "cmake: $cmake"
Write-Host "ctest: $ctest"
Write-Host "repo: $repoRoot"

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments @("-S", $repoRoot, "-B", $buildRoot) `
    -WorkingDirectory $repoRoot `
    -Label "Configure Henka Engine for tests"

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments @("--build", $buildRoot, "--config", "Debug") `
    -WorkingDirectory $repoRoot `
    -Label "Build Henka Engine tests"

Invoke-HenkaNative `
    -FilePath $ctest `
    -Arguments @("--test-dir", $buildRoot, "--output-on-failure", "-C", "Debug") `
    -WorkingDirectory $repoRoot `
    -Label "Run Henka Engine tests"
