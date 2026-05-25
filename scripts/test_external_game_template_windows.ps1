$ErrorActionPreference = "Stop"

function Get-CMakePath {
    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmakeCommand) {
        return $cmakeCommand.Source
    }

    $fallback = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $fallback) {
        return $fallback
    }

    throw "CMake was not found on PATH or in the expected Visual Studio location."
}

function Write-Step {
    param([string]$Message)
    Write-Host "[template] $Message"
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$templateRoot = Join-Path $repoRoot "templates\external_game_minimal"
$validationRoot = Join-Path $repoRoot ("build\template_validation\" + (Get-Date -Format "yyyyMMdd_HHmmss"))
$validationSource = Join-Path $validationRoot "external_game_minimal_src"
$validationBuild = Join-Path $validationRoot "external_game_minimal_build"
$cmake = Get-CMakePath

Write-Step "Preparing repo-local template validation folder"
New-Item -ItemType Directory -Path $validationRoot | Out-Null
Copy-Item -LiteralPath $templateRoot -Destination $validationSource -Recurse

Write-Step "Configuring the external game template"
& $cmake -S $validationSource -B $validationBuild -DHENKA_ENGINE_DIR="$repoRoot"

Write-Step "Building the external game template"
& $cmake --build $validationBuild --config Debug

$templateExe = Join-Path $validationBuild "Debug\external_game_minimal.exe"
if (-not (Test-Path $templateExe)) {
    throw "The external game template executable was not produced: $templateExe"
}

Write-Step "Running the external game template smoke test"
$outputLines = & $templateExe
if ($LASTEXITCODE -ne 0) {
    throw "The external game template executable exited with code $LASTEXITCODE."
}

if (($outputLines -join "`n") -notmatch "External game template placeholder\.") {
    throw "The external game template smoke test did not print the expected placeholder output."
}

Write-Host "[pass] External game template configured, built, and ran successfully."
