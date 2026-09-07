param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$DependencyRoot = "",

    [string]$TestFilter = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$buildRoot = Join-Path $repoRoot "build"
$cmake = Get-HenkaCMakePath
$ctest = Get-HenkaCTestPath -CMakePath $cmake
$provenanceScript = Join-Path $PSScriptRoot "write_build_provenance.ps1"
$executablePath = Join-Path $buildRoot "examples\sandbox3d\$Configuration\henka_sandbox3d.exe"
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
Write-Host "ctest: $ctest"
Write-Host "repo: $repoRoot"
Write-Host "dependency root: $resolvedDependencyRoot"

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments $configureArguments `
    -WorkingDirectory $repoRoot `
    -Label "Configure Henka Engine for tests"

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments @("--build", $buildRoot, "--config", $Configuration) `
    -WorkingDirectory $repoRoot `
    -Label "Build Henka Engine tests"

$softwareOpenGLRoot = [string]$env:HENKA_CI_SOFTWARE_OPENGL_ROOT
if (-not [string]::IsNullOrWhiteSpace($softwareOpenGLRoot)) {
    $softwareOpenGLInstaller = Join-Path $PSScriptRoot "install_windows_software_opengl.ps1"
    $softwareOpenGLTargets = @(
        (Join-Path $buildRoot "tests\$Configuration")
    )
    $sandboxDirectory = Join-Path $buildRoot "examples\sandbox3d\$Configuration"
    if (Test-Path -LiteralPath $sandboxDirectory -PathType Container) {
        $softwareOpenGLTargets += $sandboxDirectory
    }
    foreach ($targetDirectory in $softwareOpenGLTargets) {
        Invoke-HenkaNative `
            -FilePath "powershell.exe" `
            -Arguments @(
                "-NoProfile",
                "-ExecutionPolicy", "Bypass",
                "-File", $softwareOpenGLInstaller,
                "-SourceDirectory", $softwareOpenGLRoot,
                "-TargetDirectory", $targetDirectory) `
            -WorkingDirectory $repoRoot `
            -Label "Install CI-only OpenGL runtime for $Configuration tests and Sandbox3D"
    }
}

$ctestArguments = @("--test-dir", $buildRoot, "--output-on-failure", "-C", $Configuration)
if (-not [string]::IsNullOrWhiteSpace($TestFilter)) {
    $ctestArguments += @("-R", $TestFilter)
}

Invoke-HenkaNative `
    -FilePath $ctest `
    -Arguments $ctestArguments `
    -WorkingDirectory $repoRoot `
    -Label "Run Henka Engine tests"

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
