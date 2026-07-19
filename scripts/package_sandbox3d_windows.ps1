param(
    [switch]$ResetUserData,

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Resolve-Path (Join-Path $PSScriptRoot "..")))
$packageRoot = Join-Path $repoRoot "out\HenkaSandbox3D"
$packageUserDir = Join-Path $packageRoot "user"
$preservedUserDir = Join-Path $repoRoot "out\.henka_sandbox3d_user_preserve"
$manifestPath = Join-Path $repoRoot "build\henka-build-info.json"
$expectedExe = Join-Path $repoRoot "build\examples\sandbox3d\$Configuration\henka_sandbox3d.exe"
$packagedExe = Join-Path $packageRoot "HenkaSandbox3D.exe"
$packageDocsDir = Join-Path $packageRoot "docs"
$packageHelpDir = Join-Path $packageRoot "docs\help"
$packageInfoPath = Join-Path $packageRoot "PACKAGE_INFO.txt"
$runGuidePath = Join-Path $packageRoot "README.txt"
$packagedProcessName = "HenkaSandbox3D"

function Invoke-GitSingleLine {
    param([string[]]$Arguments)
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "SilentlyContinue"
        $lines = @(& git.exe -C $repoRoot @Arguments 2>$null)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($exitCode -ne 0 -or $lines.Count -ne 1) {
        throw "Git package provenance query failed: git $($Arguments -join ' ')"
    }
    return ([string]$lines[0]).Trim()
}

function Get-GitSourceState {
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "SilentlyContinue"
        $lines = @(& git.exe -C $repoRoot status --porcelain=v1 --untracked-files=all 2>$null)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($exitCode -ne 0) {
        throw "Git source-state query failed."
    }
    if ($lines.Count -eq 0) {
        return "clean"
    }
    return "working-tree"
}

function Remove-HenkaDirectoryTree {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return }
    Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
}

if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Build provenance was not found. Run .\scripts\build_windows.ps1 -Configuration $Configuration first."
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema_version -ne 1 -or $manifest.configuration -ne $Configuration) {
    throw "Build provenance does not match configuration $Configuration."
}
if ([System.IO.Path]::GetFullPath((Join-Path $repoRoot $manifest.executable_relative_path)) -ne [System.IO.Path]::GetFullPath($expectedExe)) {
    throw "Build provenance points to a different executable."
}
if (-not (Test-Path -LiteralPath $expectedExe -PathType Leaf)) {
    throw "The $Configuration sandbox executable was not found."
}

$currentCommit = Invoke-GitSingleLine @("rev-parse", "HEAD")
$currentState = Get-GitSourceState
$currentHash = (Get-FileHash -LiteralPath $expectedExe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($manifest.commit_sha -ne $currentCommit) {
    throw "Build provenance commit does not match current HEAD. Rebuild before packaging."
}
if ($manifest.source_state -ne $currentState) {
    throw "Build provenance source state does not match the current working tree. Rebuild before packaging."
}
if ($manifest.executable_sha256 -ne $currentHash) {
    throw "Built executable hash does not match build provenance. Rebuild before packaging."
}

if (Get-Process -Name $packagedProcessName -ErrorAction SilentlyContinue) {
    throw "The packaged sandbox is still running. Close HenkaSandbox3D.exe before refreshing the package."
}

if (Test-Path -LiteralPath $packageRoot) {
    if ((-not $ResetUserData) -and (Test-Path -LiteralPath $packageUserDir)) {
        if (Test-Path -LiteralPath $preservedUserDir) { Remove-HenkaDirectoryTree $preservedUserDir }
        Move-Item -LiteralPath $packageUserDir -Destination $preservedUserDir
    }
    Remove-HenkaDirectoryTree $packageRoot
}

[System.IO.Directory]::CreateDirectory($packageRoot) | Out-Null
[System.IO.Directory]::CreateDirectory($packageDocsDir) | Out-Null
[System.IO.Directory]::CreateDirectory($packageHelpDir) | Out-Null
Copy-Item -LiteralPath $expectedExe -Destination $packagedExe
Copy-Item -LiteralPath (Join-Path $repoRoot "assets") -Destination $packageRoot -Recurse
Copy-Item -LiteralPath (Join-Path $repoRoot "docs\help\sandbox3d.md") -Destination (Join-Path $packageHelpDir "sandbox3d.md")

if ((-not $ResetUserData) -and (Test-Path -LiteralPath $preservedUserDir)) {
    Move-Item -LiteralPath $preservedUserDir -Destination $packageUserDir
}

$sourceDir = Split-Path $expectedExe
foreach ($dll in @(Get-ChildItem -LiteralPath $sourceDir -Filter *.dll -File -ErrorAction SilentlyContinue)) {
    Copy-Item -LiteralPath $dll.FullName -Destination (Join-Path $packageRoot $dll.Name)
}

$packagedHash = (Get-FileHash -LiteralPath $packagedExe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($packagedHash -ne $currentHash) {
    throw "The packaged executable does not match the validated build artifact."
}
$packageRefreshedUtc = [DateTime]::UtcNow.ToString("o")

@"
Henka Engine Sandbox 3D

Double-click HenkaSandbox3D.exe to launch the packaged sandbox.
The in-window panels open automatically so Controls and Physics QA are visible without pressing F4 first.
Press F4 to hide or show the in-window panels.
Press F5 to cycle View, Inspect, and Full Tools.
The scene renders inside its own docked viewport when panels are visible.
Starts have no selected scene object until you select one.
Select an object in the viewport or Scene Objects panel, then use Select, Orbit, Pan, Move, Rotate, and Scale from the Viewport Tool section.
Use M or G, R, and S for action-based transforms. X, Y, and Z constrain an active transform; Enter applies it and Escape cancels it.
Use the in-window utilities for help, legend, paths, settings, diagnostics, Transform QA, and Physics QA.
Selected real scene objects show a viewport highlight until selection is cleared.
Ground selection uses one bounded Scene View highlight, and viewport overlays do not draw over panels.
Physics QA explains Static, Dynamic, and Kinematic bodies. Make Dynamic + Drop activates only the selected supported body; Enable starts the full arranged demonstration.
DRAG marks a live panel header. Release over a valid left or right outline to dock there, or release away from the outlines to open a separate native tool window.
Open Native Panel Test from Controls to exercise a separate OS-level validation window.
Close a detached tool window to return its panel to the last valid dock.
Use Reset Layout to recover panels and default dock sizes.
Watch the small in-window status area for recent actions and warnings.

Keep these folders beside the executable:
- assets
- docs

Offline help:
- docs\help\sandbox3d.md

Local settings:
- user\sandbox3d.settings
"@ | Set-Content -LiteralPath $runGuidePath

@"
Henka Engine Sandbox 3D package
Package schema: 2
Package refreshed UTC: $packageRefreshedUtc
Build generated UTC: $($manifest.generated_utc)
Source commit: $currentCommit
Source state: $currentState
Build configuration: $Configuration
Architecture: $($manifest.architecture)
CMake version: $($manifest.cmake_version)
Source executable: $($manifest.executable_relative_path)
Source executable SHA-256: $currentHash
Packaged executable SHA-256: $packagedHash
Executable: HenkaSandbox3D.exe
Runtime mode: Packaged
"@ | Set-Content -LiteralPath $packageInfoPath

Write-Host "Packaged sandbox ready:"
Write-Host "  $packagedExe"
Write-Host "Package marker:"
Write-Host "  $packageInfoPath"
Write-Host "Source commit: $currentCommit"
Write-Host "Source state: $currentState"
Write-Host "Configuration: $Configuration"
Write-Host "Executable SHA-256: $packagedHash"
