param(
    [switch]$NoLocalProviders
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

function Write-Step {
    param([string]$Message)
    Write-Host "[template] $Message"
}

function Invoke-ExternalNativeDirect {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Label,

        [int]$TimeoutMilliseconds = 600000
    )

    Write-Host ""
    Write-Host "==> $Label"
    Write-Host "    $FilePath $($Arguments -join ' ')"
    $process = $null
    Push-Location $WorkingDirectory
    try {
        $process = Start-HenkaProcess `
            -FilePath $FilePath `
            -Arguments $Arguments `
            -WorkingDirectory $WorkingDirectory `
            -CreateNoWindow
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            Stop-HenkaProcessTree -ProcessId $process.Id
            throw "$Label exceeded timeout ${TimeoutMilliseconds}ms and its process tree was terminated."
        }
        $exitCode = $process.ExitCode
    }
    finally {
        if ($null -ne $process) {
            $process.Dispose()
        }
        Pop-Location
    }
    if ($exitCode -ne 0) {
        throw "$Label failed with exit code $exitCode."
    }
}

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$templateRoot = Join-Path $repoRoot "templates\external_game_minimal"
$validationRoot = Join-Path $repoRoot ("build\tv\ext_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
$validationSource = Join-Path $validationRoot "external_game_minimal_src"
$validationBuild = Join-Path $validationRoot "external_game_minimal_build"
$cmake = Get-HenkaCMakePath
$localSdlSource = Join-Path $repoRoot "build\_deps\sdl3-src"
$localKtxSource = Join-Path $repoRoot "build\_deps\ktxsoftware-src"
$localEnetSource = Join-Path $repoRoot "build\_deps\enet-src"
$offlineProviderCount = 0
$configureArguments = @(
    "-S", $validationSource,
    "-B", $validationBuild,
    "-DHENKA_ENGINE_DIR=$repoRoot"
)

if (-not $NoLocalProviders -and (Test-Path -LiteralPath $localSdlSource -PathType Container)) {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_SDL3=$localSdlSource"
    $offlineProviderCount += 1
    Write-Host "SDL3 provider: repository-local populated source"
}
else {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_SDL3="
    $configureArguments += "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF"
    Write-Host "SDL3 provider: FetchContent network fallback"
}

if (-not $NoLocalProviders -and (Test-Path -LiteralPath $localKtxSource -PathType Container)) {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_KTXSOFTWARE=$localKtxSource"
    $offlineProviderCount += 1
    Write-Host "KTX-Software provider: repository-local populated source"
}
else {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_KTXSOFTWARE="
    $configureArguments += "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF"
    Write-Host "KTX-Software provider: FetchContent network fallback"
}
if (-not $NoLocalProviders -and (Test-Path -LiteralPath $localEnetSource -PathType Container)) {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_ENET=$localEnetSource"
    $offlineProviderCount += 1
    Write-Host "ENet provider: repository-local populated source"
}
else {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_ENET="
    $configureArguments += "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF"
    Write-Host "ENet provider: FetchContent network fallback"
}

if ($offlineProviderCount -eq 3) {
    $configureArguments += "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    Write-Host "FetchContent mode: fully disconnected because all repository-local providers are present"
}
else {
    Write-Host "FetchContent mode: normal network-capable fallback for missing providers"
}

# The nested SDL project uses configure-time source globs. Suppress the
# generated ALL_BUILD regeneration target after configure so a stable source
# tree cannot re-enter that nested custom rule during the consumer build.
$configureArguments += "-DCMAKE_SUPPRESS_REGENERATION=ON"

Write-Host "cmake: $cmake"
Write-Host "repo: $repoRoot"

Write-Step "Preparing repo-local template validation folder"
[System.IO.Directory]::CreateDirectory($validationRoot) | Out-Null
Copy-Item -LiteralPath $templateRoot -Destination $validationSource -Recurse

Invoke-ExternalNativeDirect `
    -FilePath $cmake `
    -Arguments $configureArguments `
    -WorkingDirectory $repoRoot `
    -Label "Configure external game template"

Invoke-ExternalNativeDirect `
    -FilePath $cmake `
    -Arguments @("--build", $validationBuild, "--config", "Debug", "--parallel", "8") `
    -WorkingDirectory $repoRoot `
    -Label "Build external game template"

$templateExe = Join-Path $validationBuild "Debug\external_game_minimal.exe"
if (-not (Test-Path -LiteralPath $templateExe -PathType Leaf)) {
    throw "The external game template executable was not produced: $templateExe"
}

$result = Invoke-HenkaNativeCapture `
    -FilePath $templateExe `
    -WorkingDirectory (Split-Path -Parent $templateExe) `
    -Label "Run external game template smoke test" `
    -TimeoutMilliseconds 30000

if ($result.Stdout -notmatch "External game template initialized\." -or
    $result.Stdout -notmatch "External Terrain material, edit, collision, render-data, save, and restart workflow passed\.") {
    throw "The external game template Terrain workflow did not complete its expected public-API checks."
}

Write-Host "[pass] External game template configured, built, and ran successfully."
