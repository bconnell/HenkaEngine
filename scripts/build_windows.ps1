param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$buildRoot = Join-Path $repoRoot "build"
$cmake = Get-HenkaCMakePath
$executablePath = Join-Path $buildRoot "examples\sandbox3d\$Configuration\henka_sandbox3d.exe"
$provenanceScript = Join-Path $PSScriptRoot "write_build_provenance.ps1"
$fetchContent = Get-HenkaCMakeFetchContentArguments `
    -DependencyRoot (Join-Path $buildRoot "_deps") `
    -Providers @("SDL3", "KTXSOFTWARE", "ENET", "LUA")
$configureArguments = @("-S", $repoRoot, "-B", $buildRoot) + @($fetchContent.Arguments)
foreach ($provider in $fetchContent.ProviderStates) {
    if ($provider.Available) {
        Write-Host "$($provider.Label) provider: repository-local populated source"
    } else {
        Write-Host "$($provider.Label) provider: FetchContent network fallback"
    }
}
if ($fetchContent.FullyDisconnected) {
    Write-Host "FetchContent mode: fully disconnected because all repository-local providers are present"
} else {
    Write-Host "FetchContent mode: normal network-capable fallback for missing providers"
}

Write-Host "cmake: $cmake"
Write-Host "repo: $repoRoot"
Write-Host "configuration: $Configuration"

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments $configureArguments `
    -WorkingDirectory $repoRoot `
    -Label "Configure Henka Engine"

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments @("--build", $buildRoot, "--config", $Configuration) `
    -WorkingDirectory $repoRoot `
    -Label "Build Henka Engine $Configuration"

Invoke-HenkaNative `
    -FilePath "powershell.exe" `
    -Arguments @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $provenanceScript,
        "-RepoRoot", $repoRoot,
        "-Configuration", $Configuration,
        "-ExecutablePath", $executablePath,
        "-CMakePath", $cmake) `
    -WorkingDirectory $repoRoot `
    -Label "Record build provenance"
