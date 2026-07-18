Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$buildRoot = Join-Path $repoRoot "build"
$cmake = Get-HenkaCMakePath

Write-Host "cmake: $cmake"
Write-Host "repo: $repoRoot"

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments @("-S", $repoRoot, "-B", $buildRoot) `
    -WorkingDirectory $repoRoot `
    -Label "Configure Henka Engine"

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments @("--build", $buildRoot, "--config", "Debug") `
    -WorkingDirectory $repoRoot `
    -Label "Build Henka Engine Debug"
