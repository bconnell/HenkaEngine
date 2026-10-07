[CmdletBinding()]
param(
    [switch]$ConfirmNoActiveProcess
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$buildRoot = Get-HenkaBuildRoot -RepositoryRoot $repoRoot
$packageRoot = Get-HenkaPackageRoot -RepositoryRoot $repoRoot -PackageName "HenkaSandbox3D"
$manager = Join-Path $PSScriptRoot "manage_generated_artifacts_windows.ps1"

if (Test-Path -LiteralPath $buildRoot -PathType Container) {
    $marker = Join-Path $buildRoot ".henka-generated.json"
    if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) {
        throw "The canonical build root lacks lifecycle provenance and was preserved: $buildRoot"
    }
    if (-not $ConfirmNoActiveProcess) {
        throw "Refusing to remove the canonical build root without explicit -ConfirmNoActiveProcess confirmation that no build, test, or Sandbox process is using it: $buildRoot"
    }
    $output = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $manager `
        -Mode Cleanup -ConfirmNoActiveProcess -CandidatePath $buildRoot 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "The exact canonical build root was not safely retired. Output: $(($output | ForEach-Object { [string]$_ }) -join "`n")"
    }
}

if (Test-Path -LiteralPath $packageRoot -PathType Container) {
    Write-Host "Preserved the packaged Sandbox and its user data: $packageRoot"
}
Write-Host "No repository build/out trees or package-local user data were broadly removed."
