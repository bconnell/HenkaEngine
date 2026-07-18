Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

function Write-Step {
    param([string]$Message)
    Write-Host "[template] $Message"
}

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$templateRoot = Join-Path $repoRoot "templates\external_game_minimal"
$validationRoot = Join-Path $repoRoot ("build\template_validation\" + (Get-Date -Format "yyyyMMdd_HHmmss"))
$validationSource = Join-Path $validationRoot "external_game_minimal_src"
$validationBuild = Join-Path $validationRoot "external_game_minimal_build"
$cmake = Get-HenkaCMakePath

Write-Host "cmake: $cmake"
Write-Host "repo: $repoRoot"

Write-Step "Preparing repo-local template validation folder"
[System.IO.Directory]::CreateDirectory($validationRoot) | Out-Null
Copy-Item -LiteralPath $templateRoot -Destination $validationSource -Recurse

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments @("-S", $validationSource, "-B", $validationBuild, "-DHENKA_ENGINE_DIR=$repoRoot") `
    -WorkingDirectory $repoRoot `
    -Label "Configure external game template"

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments @("--build", $validationBuild, "--config", "Debug") `
    -WorkingDirectory $repoRoot `
    -Label "Build external game template"

$templateExe = Join-Path $validationBuild "Debug\external_game_minimal.exe"
if (-not (Test-Path -LiteralPath $templateExe -PathType Leaf)) {
    throw "The external game template executable was not produced: $templateExe"
}

$result = Invoke-HenkaNativeCapture `
    -FilePath $templateExe `
    -WorkingDirectory (Split-Path -Parent $templateExe) `
    -Label "Run external game template smoke test"

if ($result.Stdout -notmatch "External game template placeholder\.") {
    throw "The external game template smoke test did not print the expected placeholder output."
}

Write-Host "[pass] External game template configured, built, and ran successfully."
