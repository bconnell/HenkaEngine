param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$DependencyRoot = "",

    [string]$BuildTarget = "",

    [string]$BuildRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$buildRoot = if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    Get-HenkaBuildRoot -RepositoryRoot $repoRoot
} else {
    Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $BuildRoot
}
$resolvedDependencyRoot = $DependencyRoot
$dependencyRootWasExplicit = -not [string]::IsNullOrWhiteSpace($resolvedDependencyRoot)
if ($dependencyRootWasExplicit) {
    $resolvedDependencyRoot = Resolve-HenkaDependencyRoot `
        -RepositoryRoot $repoRoot `
        -DependencyRoot $resolvedDependencyRoot
} else {
    $defaultDependencyRoot = Join-Path $buildRoot "_deps"
    if (Test-Path -LiteralPath $defaultDependencyRoot -PathType Container) {
        $resolvedDependencyRoot = Resolve-HenkaDependencyRoot `
            -RepositoryRoot $repoRoot `
            -DependencyRoot $defaultDependencyRoot
    } else {
        $resolvedDependencyRoot = ""
        Write-Host "No populated local dependency root was found; FetchContent network fallback remains enabled."
    }
}

# Validate all caller-controlled storage paths before creating or marking a
# build root, so a rejected external dependency root leaves no generated state.
$null = New-HenkaLocalDirectory -RepositoryRoot $repoRoot -Path $buildRoot
$null = Write-HenkaGeneratedRootMarker `
    -RepoRoot $repoRoot `
    -Path $buildRoot `
    -Purpose "CMake build output" `
    -RetentionClass "REBUILDABLE" `
    -Active $false `
    -CleanupEligible $true `
    -CleanupCondition "source inputs are preserved and no validation process is using this build root" `
    -Configuration "multi-configuration"
$toolchain = Get-HenkaToolchain
$cmake = $toolchain.CMakePath
$artifact = Get-HenkaBuildArtifact -BuildRoot $buildRoot -Configuration $Configuration -BuildTarget $BuildTarget
$provenanceScript = Join-Path $PSScriptRoot "write_build_provenance.ps1"
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
$dependencyDescription = if ([string]::IsNullOrWhiteSpace($resolvedDependencyRoot)) {
    "<none; FetchContent fallback>"
} else {
    $resolvedDependencyRoot
}
Write-Host "dependency root: $dependencyDescription"

$buildStateLock = Enter-HenkaBuildStateLock
try {
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
            "-BuildRoot", $buildRoot,
            "-Configuration", $Configuration,
            "-ExecutablePath", $artifact.Path,
            "-ArtifactKind", $artifact.Kind,
            "-CMakePath", $cmake) `
        -WorkingDirectory $repoRoot `
        -Label "Record build provenance"
} finally {
    Exit-HenkaBuildStateLock -Lock $buildStateLock
}
