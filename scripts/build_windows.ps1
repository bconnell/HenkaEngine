param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$DependencyRoot = "",

    [string]$BuildTarget = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$buildRoot = Join-Path $repoRoot "build"
$cmake = Get-HenkaCMakePath
$executablePath = Join-Path $buildRoot "examples\sandbox3d\$Configuration\henka_sandbox3d.exe"
$provenanceScript = Join-Path $PSScriptRoot "write_build_provenance.ps1"
$resolvedDependencyRoot = $DependencyRoot
if ([string]::IsNullOrWhiteSpace($resolvedDependencyRoot)) {
    $resolvedDependencyRoot = Join-Path $buildRoot "_deps"
} else {
    $resolvedDependencyRoot = [System.IO.Path]::GetFullPath($resolvedDependencyRoot)
}
if (-not (Test-Path -LiteralPath $resolvedDependencyRoot -PathType Container)) {
    throw "Henka dependency root was not found: $resolvedDependencyRoot"
}
$fetchContent = Get-HenkaCMakeFetchContentArguments `
    -DependencyRoot $resolvedDependencyRoot `
    -Providers @("SDL3", "KTXSOFTWARE", "ENET", "LUA", "MINIAUDIO", "STB")
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
Write-Host "dependency root: $resolvedDependencyRoot"

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments $configureArguments `
    -WorkingDirectory $repoRoot `
    -Label "Configure Henka Engine"

$buildArguments = @("--build", $buildRoot, "--config", $Configuration)
if (-not [string]::IsNullOrWhiteSpace($BuildTarget)) {
    $buildArguments += @("--target", $BuildTarget)
}

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments $buildArguments `
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
