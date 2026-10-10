param(
    [Parameter(Mandatory = $true)][string]$SourceDirectory,
    [Parameter(Mandatory = $true)][string]$TargetDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$source = Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $SourceDirectory
$target = Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $TargetDirectory
if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "CI-only Mesa OpenGL runtime directory was not found: $source"
}
$null = New-HenkaLocalDirectory -RepositoryRoot $repoRoot -Path $target
$dlls = @(Get-ChildItem -LiteralPath $source -Filter "*.dll" -File)
if ($dlls.Count -eq 0 -or -not (Test-Path -LiteralPath (Join-Path $source "opengl32.dll") -PathType Leaf)) {
    throw "CI-only Mesa OpenGL runtime is incomplete: $source"
}
foreach ($dll in $dlls) {
    Copy-Item -LiteralPath $dll.FullName -Destination (Join-Path $target $dll.Name) -Force
}
Write-Host "[pass] Installed CI-only Mesa OpenGL runtime into $target"
