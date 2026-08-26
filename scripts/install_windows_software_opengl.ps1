param(
    [Parameter(Mandatory = $true)][string]$SourceDirectory,
    [Parameter(Mandatory = $true)][string]$TargetDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$source = [System.IO.Path]::GetFullPath($SourceDirectory)
$target = [System.IO.Path]::GetFullPath($TargetDirectory)
if (-not (Test-Path -LiteralPath $source -PathType Container)) {
    throw "CI-only Mesa OpenGL runtime directory was not found: $source"
}
[System.IO.Directory]::CreateDirectory($target) | Out-Null
$dlls = @(Get-ChildItem -LiteralPath $source -Filter "*.dll" -File)
if ($dlls.Count -eq 0 -or -not (Test-Path -LiteralPath (Join-Path $source "opengl32.dll") -PathType Leaf)) {
    throw "CI-only Mesa OpenGL runtime is incomplete: $source"
}
foreach ($dll in $dlls) {
    Copy-Item -LiteralPath $dll.FullName -Destination (Join-Path $target $dll.Name) -Force
}
Write-Host "[pass] Installed CI-only Mesa OpenGL runtime into $target"
