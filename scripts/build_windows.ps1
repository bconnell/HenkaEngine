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
$localSdlSource = Join-Path $buildRoot "_deps\sdl3-src"
$configureArguments = @("-S", $repoRoot, "-B", $buildRoot)
if (Test-Path -LiteralPath (Join-Path $localSdlSource "CMakeLists.txt")) {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_SDL3=$localSdlSource"
    $configureArguments += "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    Write-Host "SDL3 provider: repository-local populated source"
} else {
    Write-Host "SDL3 provider: FetchContent network fallback"
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
