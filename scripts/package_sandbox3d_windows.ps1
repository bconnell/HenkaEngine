$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$packageRoot = Join-Path $repoRoot "out\HenkaSandbox3D"
$buildDebugExe = Join-Path $repoRoot "build\examples\sandbox3d\Debug\henka_sandbox3d.exe"
$buildReleaseExe = Join-Path $repoRoot "build\examples\sandbox3d\Release\henka_sandbox3d.exe"

if (Test-Path $buildDebugExe) {
    $sourceExe = $buildDebugExe
}
elseif (Test-Path $buildReleaseExe) {
    $sourceExe = $buildReleaseExe
}
else {
    throw "Sandbox build output was not found. Build the project first with .\scripts\build_windows.ps1."
}

$sourceDir = Split-Path $sourceExe
$packagedExe = Join-Path $packageRoot "HenkaSandbox3D.exe"
$packageHelpDir = Join-Path $packageRoot "docs\help"
$packageAssetsDir = Join-Path $packageRoot "assets"
$runGuidePath = Join-Path $packageRoot "README.txt"

if (Test-Path $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}

New-Item -ItemType Directory -Path $packageRoot | Out-Null
New-Item -ItemType Directory -Path $packageHelpDir | Out-Null

Copy-Item -LiteralPath $sourceExe -Destination $packagedExe
Copy-Item -LiteralPath (Join-Path $repoRoot "assets") -Destination $packageAssetsDir -Recurse
Copy-Item -LiteralPath (Join-Path $repoRoot "docs\help\sandbox3d.md") -Destination (Join-Path $packageHelpDir "sandbox3d.md")

$runtimeDlls = Get-ChildItem -LiteralPath $sourceDir -Filter *.dll -File -ErrorAction SilentlyContinue
foreach ($dll in $runtimeDlls) {
    Copy-Item -LiteralPath $dll.FullName -Destination (Join-Path $packageRoot $dll.Name)
}

@"
Henka Engine Sandbox 3D

Double-click HenkaSandbox3D.exe to launch the packaged sandbox.

Keep these folders beside the executable:
- assets
- docs

Offline help:
- docs\help\sandbox3d.md

If the sandbox does not start, rebuild the project and package it again from the repository root.
"@ | Set-Content -LiteralPath $runGuidePath

Write-Host "Packaged sandbox ready:"
Write-Host "  $packagedExe"
Write-Host ""
Write-Host "Next step:"
Write-Host "  Open out\HenkaSandbox3D and double-click HenkaSandbox3D.exe."
