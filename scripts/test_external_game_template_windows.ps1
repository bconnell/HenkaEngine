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
$validationParent = Join-Path $repoRoot "build\tv"
$validationRoot = Join-Path $validationParent "external_game_minimal"
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

function Remove-GeneratedValidationTree {
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $parentPath = [System.IO.Path]::GetFullPath($validationParent)
    $stableRoot = [System.IO.Path]::Combine($parentPath, "external_game_minimal")
    $stableSource = [System.IO.Path]::Combine($stableRoot, "external_game_minimal_src")
    if ($fullPath -ne $stableRoot -and $fullPath -ne $stableSource -and
        -not ($fullPath.StartsWith($parentPath + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -and
            [System.IO.Path]::GetFileName($fullPath) -match '^ext_[0-9]{8}_[0-9]{6}$')) {
        throw "Refusing to remove a non-owned external-template generated path: $fullPath"
    }
    if (Test-Path -LiteralPath $fullPath) {
        Remove-Item -LiteralPath $fullPath -Recurse -Force -ErrorAction Stop
    }
}

[System.IO.Directory]::CreateDirectory($validationParent) | Out-Null
$legacyValidationRoots = @(
    Get-ChildItem -LiteralPath $validationParent -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^ext_[0-9]{8}_[0-9]{6}$' }
)
foreach ($legacyRoot in $legacyValidationRoots) {
    Remove-GeneratedValidationTree -Path $legacyRoot.FullName
}
if ($legacyValidationRoots.Count -gt 0) {
    Write-Host "Retired $($legacyValidationRoots.Count) superseded external-game validation tree(s)."
}

Write-Step "Preparing repo-local template validation folder"
[System.IO.Directory]::CreateDirectory($validationRoot) | Out-Null
Remove-GeneratedValidationTree -Path $validationSource
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
    $result.Stdout -notmatch "External Terrain material, edit, collision, render-data, save, and restart workflow passed\." -or
    $result.Stdout -notmatch "External public authoring mesh, scene, collision, duplicate/delete, and reload handoff passed\." -or
    $result.Stdout -notmatch "External Terrain graphical Rendered path passed\.") {
    throw "The external game template authoring or Terrain workflow did not complete its expected public-API checks."
}

Write-Host "[pass] External game template configured, built, and ran successfully."
