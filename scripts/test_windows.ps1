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
$ctest = Get-HenkaCTestPath -CMakePath $cmake
$localSdlSource = Join-Path $buildRoot "_deps\sdl3-src"
$localKtxSource = Join-Path $buildRoot "_deps\ktxsoftware-src"
$localEnetSource = Join-Path $buildRoot "_deps\enet-src"
$offlineProviderCount = 0
$configureArguments = @("-S", $repoRoot, "-B", $buildRoot)
$provenanceScript = Join-Path $PSScriptRoot "write_build_provenance.ps1"
$executablePath = Join-Path $buildRoot "examples\sandbox3d\$Configuration\henka_sandbox3d.exe"
if (Test-Path -LiteralPath (Join-Path $localSdlSource "CMakeLists.txt")) {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_SDL3=$localSdlSource"
    $offlineProviderCount += 1
    Write-Host "SDL3 provider: repository-local populated source"
} else {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_SDL3="
    $configureArguments += "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF"
    Write-Host "SDL3 provider: FetchContent network fallback"
}
if (Test-Path -LiteralPath (Join-Path $localKtxSource "CMakeLists.txt")) {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_KTXSOFTWARE=$localKtxSource"
    $offlineProviderCount += 1
    Write-Host "KTX-Software provider: repository-local populated source"
} else {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_KTXSOFTWARE="
    $configureArguments += "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF"
    Write-Host "KTX-Software provider: FetchContent network fallback"
}
if (Test-Path -LiteralPath (Join-Path $localEnetSource "CMakeLists.txt")) {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_ENET=$localEnetSource"
    $offlineProviderCount += 1
    Write-Host "ENet provider: repository-local populated source"
} else {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_ENET="
    $configureArguments += "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF"
    Write-Host "ENet provider: FetchContent network fallback"
}
if ($offlineProviderCount -eq 3) {
    $configureArguments += "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    Write-Host "FetchContent mode: fully disconnected because all repository-local providers are present"
} else {
    Write-Host "FetchContent mode: normal network-capable fallback for missing providers"
}

Write-Host "cmake: $cmake"
Write-Host "ctest: $ctest"
Write-Host "repo: $repoRoot"

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

Invoke-HenkaNative `
    -FilePath $ctest `
    -Arguments @("--test-dir", $buildRoot, "--output-on-failure", "-C", $Configuration) `
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
