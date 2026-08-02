Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$buildRoot = Join-Path $repoRoot "build"
$cmake = Get-HenkaCMakePath
$ctest = Get-HenkaCTestPath -CMakePath $cmake
$localSdlSource = Join-Path $buildRoot "_deps\sdl3-src"
$configureArguments = @("-S", $repoRoot, "-B", $buildRoot)
$provenanceScript = Join-Path $PSScriptRoot "write_build_provenance.ps1"
$executablePath = Join-Path $buildRoot "examples\sandbox3d\Debug\henka_sandbox3d.exe"
if (Test-Path -LiteralPath (Join-Path $localSdlSource "CMakeLists.txt")) {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_SDL3=$localSdlSource"
    $configureArguments += "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    Write-Host "SDL3 provider: repository-local populated source"
} else {
    Write-Host "SDL3 provider: FetchContent network fallback"
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
    -Arguments @("--build", $buildRoot, "--config", "Debug") `
    -WorkingDirectory $repoRoot `
    -Label "Build Henka Engine tests"

Invoke-HenkaNative `
    -FilePath $ctest `
    -Arguments @("--test-dir", $buildRoot, "--output-on-failure", "-C", "Debug") `
    -WorkingDirectory $repoRoot `
    -Label "Run Henka Engine tests"

Invoke-HenkaNative `
    -FilePath "powershell.exe" `
    -Arguments @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $provenanceScript,
        "-RepoRoot", $repoRoot,
        "-Configuration", "Debug",
        "-ExecutablePath", $executablePath,
        "-CMakePath", $cmake) `
    -WorkingDirectory $repoRoot `
    -Label "Record build provenance"
