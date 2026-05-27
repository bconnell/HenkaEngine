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
$packageDocsDir = Join-Path $packageRoot "docs"
$packageHelpDir = Join-Path $packageRoot "docs\help"
$packageAssetsDir = Join-Path $packageRoot "assets"
$runGuidePath = Join-Path $packageRoot "README.txt"
$packageInfoPath = Join-Path $packageRoot "PACKAGE_INFO.txt"
$packageRefreshedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss zzz"
$sourceExeTimestamp = (Get-Item $sourceExe).LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss zzz")
$gitCommit = "unknown"
$packagedProcessName = "HenkaSandbox3D"

function Remove-HenkaDirectoryTree {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    try {
        Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
        return
    }
    catch {
        if (-not (Test-Path -LiteralPath $Path)) {
            return
        }
    }

    Get-ChildItem -LiteralPath $Path -Recurse -Force -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        ForEach-Object {
            Remove-Item -LiteralPath $_.FullName -Force -Recurse -ErrorAction SilentlyContinue
        }

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Force -Recurse -ErrorAction SilentlyContinue
    }
}

try {
    $gitCommit = (git -C $repoRoot log -1 --format=%h 2>$null).Trim()
    if ([string]::IsNullOrWhiteSpace($gitCommit)) {
        $gitCommit = "unknown"
    }
}
catch {
    $gitCommit = "unknown"
}

if (Get-Process -Name $packagedProcessName -ErrorAction SilentlyContinue) {
    throw "The packaged sandbox is still running. Close HenkaSandbox3D.exe before refreshing the package."
}

if (Test-Path $packageRoot) {
    if ((-not $ResetUserData) -and (Test-Path $packageUserDir)) {
        if (Test-Path $preservedUserDir) {
            Remove-HenkaDirectoryTree -Path $preservedUserDir
        }

        if (Test-Path -LiteralPath $packageUserDir) {
            Move-Item -LiteralPath $packageUserDir -Destination $preservedUserDir
        }
    }

    Remove-HenkaDirectoryTree -Path $packageRoot
}

[System.IO.Directory]::CreateDirectory($packageRoot) | Out-Null
[System.IO.Directory]::CreateDirectory($packageDocsDir) | Out-Null
[System.IO.Directory]::CreateDirectory($packageHelpDir) | Out-Null

Copy-Item -LiteralPath $sourceExe -Destination $packagedExe
Copy-Item -LiteralPath (Join-Path $repoRoot "assets") -Destination $packageRoot -Recurse
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
Press F5 to cycle View, Inspect, and Full Tools.
The scene renders inside its own docked viewport when panels are visible.
Select an object in the viewport or Scene Objects panel, then use Select, Orbit, Pan, Move, Rotate, and Scale from the Viewport Tool section.
Use the in-window utilities for help, legend, paths, settings, diagnostics, and Transform QA.
Drag a panel header to undock and move it, then drag a floating title again to reposition it or drag the lower-right grip to resize.
Use L, R, or Home on a floating panel to redock it, and drag dock splitters beside Scene View to resize occupied docks.
Use Reset Layout in Controls to recover all panels and default dock sizes.
Watch the small in-window status area for recent actions and warnings.
The packaged runtime reports Packaged mode automatically when PACKAGE_INFO.txt is present.

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
Layout: Press F5 to cycle View, Inspect, and Full Tools.
Runtime mode: Packaged is detected automatically from PACKAGE_INFO.txt.
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
Write-Host "  Press F5 to cycle View, Inspect, and Full Tools."
Write-Host "  The scene should render inside its own docked viewport when panels are visible."
Write-Host "  Select an object in the viewport or Scene Objects panel, then use Select, Orbit, Pan, Move, Rotate, and Scale in the Viewport Tool section."
Write-Host "  Use the in-window utilities for help, legend, paths, settings, diagnostics, and Transform QA."
Write-Host "  Use panel header dragging, resize grips, L/R/Home, and dock splitters to arrange the workspace."
Write-Host "  Use Reset Layout to recover all panels and default dock sizes."
Write-Host "  Watch the in-window status area for recent actions and warnings."
Write-Host "  The packaged runtime should report Packaged mode at startup."
