param(
    [switch]$ResetUserData
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$packageRoot = Join-Path $repoRoot "out\HenkaSandbox3D"
$packageUserDir = Join-Path $packageRoot "user"
$preservedUserDir = Join-Path $repoRoot "out\.henka_sandbox3d_user_preserve"
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
$packageInfoPath = Join-Path $packageRoot "PACKAGE_INFO.txt"
$packageRefreshedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz"
$sourceExeTimestamp = (Get-Item $sourceExe).LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss zzz")
$gitCommit = "unknown"

try {
    $gitCommit = (git -C $repoRoot log -1 --format=%h 2>$null).Trim()
    if ([string]::IsNullOrWhiteSpace($gitCommit)) {
        $gitCommit = "unknown"
    }
}
catch {
    $gitCommit = "unknown"
}

if (Test-Path $packageRoot) {
    if ((-not $ResetUserData) -and (Test-Path $packageUserDir)) {
        if (Test-Path $preservedUserDir) {
            Remove-Item -LiteralPath $preservedUserDir -Recurse -Force
        }

        Move-Item -LiteralPath $packageUserDir -Destination $preservedUserDir
    }

    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}

New-Item -ItemType Directory -Path $packageRoot | Out-Null
New-Item -ItemType Directory -Path $packageHelpDir | Out-Null

Copy-Item -LiteralPath $sourceExe -Destination $packagedExe
Copy-Item -LiteralPath (Join-Path $repoRoot "assets") -Destination $packageAssetsDir -Recurse
Copy-Item -LiteralPath (Join-Path $repoRoot "docs\help\sandbox3d.md") -Destination (Join-Path $packageHelpDir "sandbox3d.md")

if ((-not $ResetUserData) -and (Test-Path $preservedUserDir)) {
    Move-Item -LiteralPath $preservedUserDir -Destination $packageUserDir
}

$runtimeDlls = Get-ChildItem -LiteralPath $sourceDir -Filter *.dll -File -ErrorAction SilentlyContinue
foreach ($dll in $runtimeDlls) {
    Copy-Item -LiteralPath $dll.FullName -Destination (Join-Path $packageRoot $dll.Name)
}

@"
Henka Engine Sandbox 3D

Double-click HenkaSandbox3D.exe to launch the packaged sandbox.
Press F4 to open the in-window panels.

Keep these folders beside the executable:
- assets
- docs

Offline help:
- docs\help\sandbox3d.md

Local settings:
- user\sandbox3d.settings

The package script preserves the user folder by default so local sandbox settings stay in place when you refresh the package.

If the sandbox does not start, rebuild the project and package it again from the repository root.
"@ | Set-Content -LiteralPath $runGuidePath

@"
Henka Engine Sandbox 3D package
Package refreshed: $packageRefreshedAt
Source executable build time: $sourceExeTimestamp
Source commit: $gitCommit
Executable: HenkaSandbox3D.exe
UI: Press F4 to open the in-window panels.
"@ | Set-Content -LiteralPath $packageInfoPath

Write-Host "Packaged sandbox ready:"
Write-Host "  $packagedExe"
Write-Host "Package marker:"
Write-Host "  $packageInfoPath"
Write-Host ""
if ($ResetUserData) {
    Write-Host "Local sandbox settings were reset for this package refresh."
}
else {
    Write-Host "Local sandbox settings were preserved if a user folder already existed."
}
Write-Host ""
Write-Host "Next step:"
Write-Host "  Open out\HenkaSandbox3D and double-click HenkaSandbox3D.exe."
Write-Host "  Press F4 to open the in-window panels."
