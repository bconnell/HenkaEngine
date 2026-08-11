param(
    [switch]$RunExternalTemplateTwice
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot

function Get-HenkaTreeStats {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        return [pscustomobject]@{ Files = 0; Bytes = [int64]0 }
    }
    $files = @(Get-ChildItem -LiteralPath $Path -File -Recurse -Force -ErrorAction Stop)
    $measure = $files | Measure-Object -Property Length -Sum
    return [pscustomobject]@{
        Files = [int64]$measure.Count
        Bytes = [int64]($measure.Sum -as [int64])
    }
}

function Assert-HenkaGeneratedRoot {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][int64]$MaximumBytes,
        [Parameter(Mandatory = $true)][int64]$MaximumFiles,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $stats = Get-HenkaTreeStats -Path $Path
    Write-Host "${Description}: files=$($stats.Files) bytes=$($stats.Bytes) root=$Path"
    if ($stats.Bytes -gt $MaximumBytes -or $stats.Files -gt $MaximumFiles) {
        throw "$Description exceeded its generated-storage bound. Inspect $Path before continuing."
    }
}

function Assert-NoLegacyValidationTrees {
    $validationParent = Join-Path $repoRoot "build\tv"
    if (-not (Test-Path -LiteralPath $validationParent -PathType Container)) {
        return
    }
    $legacy = @(
        Get-ChildItem -LiteralPath $validationParent -Directory |
            Where-Object { $_.Name -match '^(ext|server_ext)_[0-9]{8}_[0-9]{6}$' }
    )
    if ($legacy.Count -ne 0) {
        throw "Timestamped external validation trees remain under the owned scratch root: $($legacy[0].FullName)"
    }
}

Assert-NoLegacyValidationTrees
Assert-HenkaGeneratedRoot `
    -Path (Join-Path $repoRoot "build\tv") `
    -MaximumBytes (16GB) `
    -MaximumFiles 400000 `
    -Description "External-template scratch"
Assert-HenkaGeneratedRoot `
    -Path (Join-Path $repoRoot "out\terrain-process-integration") `
    -MaximumBytes (128MB) `
    -MaximumFiles 20000 `
    -Description "Terrain process integration output"

if ($RunExternalTemplateTwice) {
    $script = Join-Path $PSScriptRoot "test_external_game_template_windows.ps1"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -NoLocalProviders
    if ($LASTEXITCODE -ne 0) { throw "The first external-template lifecycle run failed." }
    $first = Get-HenkaTreeStats -Path (Join-Path $repoRoot "build\tv\external_game_minimal")
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $script -NoLocalProviders
    if ($LASTEXITCODE -ne 0) { throw "The second external-template lifecycle run failed." }
    $second = Get-HenkaTreeStats -Path (Join-Path $repoRoot "build\tv\external_game_minimal")
    if ($second.Bytes -gt 2 * $first.Bytes -or $second.Files -gt 2 * $first.Files) {
        throw "Repeated external-template validation grew the stable scratch tree unexpectedly."
    }
    Assert-NoLegacyValidationTrees
    Assert-HenkaGeneratedRoot `
        -Path (Join-Path $repoRoot "build\tv") `
        -MaximumBytes (16GB) `
        -MaximumFiles 400000 `
        -Description "External-template scratch after repeated validation"
}

Write-Host "[pass] Generated-output lifecycle bounds and recursive-validation checks passed."
