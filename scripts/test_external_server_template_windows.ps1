param(
    [switch]$NoLocalProviders
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

function Invoke-ExternalServerNative {
    param(
        [Parameter(Mandatory = $true)] [string]$FilePath,
        [string[]]$Arguments = @(),
        [Parameter(Mandatory = $true)] [string]$WorkingDirectory,
        [Parameter(Mandatory = $true)] [string]$Label,
        [int]$TimeoutMilliseconds = 600000
    )

    Write-Host ""
    Write-Host "==> $Label"
    Write-Host "    $FilePath $($Arguments -join ' ')"
    $process = $null
    Push-Location $WorkingDirectory
    try {
        $process = Start-HenkaProcess -FilePath $FilePath -Arguments $Arguments -WorkingDirectory $WorkingDirectory -CreateNoWindow
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            Stop-HenkaProcessTree -ProcessId $process.Id
            throw "$Label exceeded timeout ${TimeoutMilliseconds}ms and its process tree was terminated."
        }
        $exitCode = $process.ExitCode
    }
    finally {
        if ($null -ne $process) { $process.Dispose() }
        Pop-Location
    }
    if ($exitCode -ne 0) { throw "$Label failed with exit code $exitCode." }
}

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$templateRoot = Join-Path $repoRoot "templates\external_server_minimal"
$validationParent = Join-Path $repoRoot "build\tv"
$validationRoot = Join-Path $validationParent "external_server_minimal"
$validationSource = Join-Path $validationRoot "external_server_minimal_src"
$validationBuild = Join-Path $validationRoot "external_server_minimal_build"
$cmake = Get-HenkaCMakePath
$localEnetSource = Join-Path $repoRoot "build\_deps\enet-src"
$configureArguments = @(
    "-S", $validationSource,
    "-B", $validationBuild,
    "-DHENKA_ENGINE_DIR=$repoRoot",
    "-DCMAKE_SUPPRESS_REGENERATION=ON"
)

if (-not $NoLocalProviders -and (Test-Path -LiteralPath $localEnetSource -PathType Container)) {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_ENET=$localEnetSource"
    $configureArguments += "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    Write-Host "ENet provider: repository-local populated source"
}
else {
    $configureArguments += "-DFETCHCONTENT_SOURCE_DIR_ENET="
    $configureArguments += "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF"
    Write-Host "ENet provider: FetchContent network fallback"
}

Write-Host "cmake: $cmake"
Write-Host "repo: $repoRoot"

function Remove-GeneratedValidationTree {
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $parentPath = [System.IO.Path]::GetFullPath($validationParent)
    $stableRoot = [System.IO.Path]::Combine($parentPath, "external_server_minimal")
    $stableSource = [System.IO.Path]::Combine($stableRoot, "external_server_minimal_src")
    if ($fullPath -ne $stableRoot -and $fullPath -ne $stableSource -and
        -not ($fullPath.StartsWith($parentPath + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -and
            [System.IO.Path]::GetFileName($fullPath) -match '^server_ext_[0-9]{8}_[0-9]{6}$')) {
        throw "Refusing to remove a non-owned external-server generated path: $fullPath"
    }
    if (Test-Path -LiteralPath $fullPath) {
        Remove-Item -LiteralPath $fullPath -Recurse -Force -ErrorAction Stop
    }
}

[System.IO.Directory]::CreateDirectory($validationParent) | Out-Null
$legacyValidationRoots = @(
    Get-ChildItem -LiteralPath $validationParent -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^server_ext_[0-9]{8}_[0-9]{6}$' }
)
foreach ($legacyRoot in $legacyValidationRoots) {
    Remove-GeneratedValidationTree -Path $legacyRoot.FullName
}
if ($legacyValidationRoots.Count -gt 0) {
    Write-Host "Retired $($legacyValidationRoots.Count) superseded external-server validation tree(s)."
}

[System.IO.Directory]::CreateDirectory($validationRoot) | Out-Null
Remove-GeneratedValidationTree -Path $validationSource
Copy-Item -LiteralPath $templateRoot -Destination $validationSource -Recurse

Invoke-ExternalServerNative -FilePath $cmake -Arguments $configureArguments -WorkingDirectory $repoRoot -Label "Configure external server template"
Invoke-ExternalServerNative -FilePath $cmake -Arguments @("--build", $validationBuild, "--config", "Debug", "--parallel", "8") -WorkingDirectory $repoRoot -Label "Build external server template"

$templateExe = Join-Path $validationBuild "Debug\external_server_minimal.exe"
if (-not (Test-Path -LiteralPath $templateExe -PathType Leaf)) {
    throw "The external server template executable was not produced: $templateExe"
}
$result = Invoke-HenkaNativeCapture `
    -FilePath $templateExe `
    -WorkingDirectory (Split-Path -Parent $templateExe) `
    -Label "Run external server template smoke test" `
    -TimeoutMilliseconds 30000
if ($result.Stdout -notmatch "External server template initialized\.") {
    throw "The external server template smoke test did not print the expected initialization output."
}
Write-Host "[pass] External server template configured, built, and ran successfully."
