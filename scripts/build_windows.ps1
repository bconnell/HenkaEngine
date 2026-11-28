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
$dependencySelection = Resolve-HenkaBuildDependencyRoot `
    -RepositoryRoot $repoRoot `
    -BuildRoot $buildRoot `
    -DependencyRoot $DependencyRoot
$resolvedDependencyRoot = $dependencySelection.Root

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
    -CleanupCondition "rebuildable" `
    -Configuration "multi-configuration"
$toolchain = Get-HenkaToolchain
$cmake = $toolchain.CMakePath
$artifact = Get-HenkaBuildArtifact -BuildRoot $buildRoot -Configuration $Configuration -BuildTarget $BuildTarget
$provenanceScript = Join-Path $PSScriptRoot "write_build_provenance.ps1"
$fetchContent = Get-HenkaCMakeFetchContentArguments `
    -DependencyRoot $resolvedDependencyRoot `
    -NoSharedDependencyCache:($dependencySelection.Mode -eq "explicit") `
    -Providers @("SDL3", "KTXSOFTWARE", "ENET", "LUA", "MINIAUDIO", "STB") `
    -PinnedRevisions (Get-HenkaCMakeDependencyPins -RepositoryRoot $repoRoot)
$configureArguments = @("-S", $repoRoot, "-B", $buildRoot) + @($fetchContent.Arguments)
Write-HenkaCMakeFetchContentStatus -Resolution $fetchContent

Write-Host "cmake: $cmake"
Write-Host "repo: $repoRoot"
Write-Host "configuration: $Configuration"
Write-Host "dependency selection: $($dependencySelection.Mode) ($($dependencySelection.AvailableProviders) pinned provider(s))"
$dependencyDescription = if ([string]::IsNullOrWhiteSpace($resolvedDependencyRoot)) {
    "<none; shared source cache checked, FetchContent fallback for missing pins>"
} else {
    $resolvedDependencyRoot
}
Write-Host "build-local dependency root: $dependencyDescription"

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
