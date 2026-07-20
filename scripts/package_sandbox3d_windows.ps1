param(
    [switch]$ResetUserData,

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = [System.IO.Path]::GetFullPath((Resolve-Path (Join-Path $PSScriptRoot "..")))
$git = Get-HenkaGitPath
$outRoot = Join-Path $repoRoot "out"
$packageRoot = Join-Path $outRoot "HenkaSandbox3D"
$packageUserDir = Join-Path $packageRoot "user"
$manifestPath = Join-Path $repoRoot "build\henka-build-info.json"
$expectedExe = Join-Path $repoRoot "build\examples\sandbox3d\$Configuration\henka_sandbox3d.exe"
$packagedProcessName = "HenkaSandbox3D"
$transactionId = [Guid]::NewGuid().ToString("N")
$stagingRoot = Join-Path $outRoot (".HenkaSandbox3D-staging-" + $transactionId)
$backupRoot = Join-Path $outRoot (".HenkaSandbox3D-backup-" + $transactionId)
$activated = $false

function Invoke-GitLines {
    param([string[]]$Arguments)

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "SilentlyContinue"
        $lines = @(& $git -C $repoRoot @Arguments 2>$null)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($exitCode -ne 0) {
        throw "Git package provenance query failed: git $($Arguments -join ' ')"
    }
    return @($lines | ForEach-Object { [string]$_ })
}

function Invoke-GitSingleLine {
    param([string[]]$Arguments)

    $lines = @(Invoke-GitLines -Arguments $Arguments)
    if ($lines.Count -ne 1 -or [string]::IsNullOrWhiteSpace([string]$lines[0])) {
        throw "Git package provenance query returned an unexpected shape: git $($Arguments -join ' ')"
    }
    return ([string]$lines[0]).Trim()
}

function Get-GitSourceState {
    $lines = @(Invoke-GitLines @("status", "--porcelain=v1", "--untracked-files=all"))
    if ($lines.Count -eq 0) {
        return "clean"
    }
    return "working-tree"
}

function Remove-HenkaDirectoryTree {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }
    Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
}

function Assert-NoReparsePoints {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    $rootItem = Get-Item -LiteralPath $Path -Force
    $items = New-Object System.Collections.Generic.List[System.IO.FileSystemInfo]
    $items.Add($rootItem)
    if ($rootItem.PSIsContainer) {
        foreach ($child in @(Get-ChildItem -LiteralPath $Path -Recurse -Force -ErrorAction Stop)) {
            $items.Add($child)
        }
    }
    foreach ($item in $items) {
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "$Description contains a reparse point and will not be packaged: $($item.FullName)"
        }
    }
}

[System.IO.Directory]::CreateDirectory($outRoot) | Out-Null
$staleTransactions = @(
    Get-ChildItem -LiteralPath $outRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name.StartsWith(".HenkaSandbox3D-backup-", [System.StringComparison]::OrdinalIgnoreCase) -or
            $_.Name.StartsWith(".HenkaSandbox3D-staging-", [System.StringComparison]::OrdinalIgnoreCase)
        }
)
if ($staleTransactions.Count -gt 0) {
    throw "A prior package transaction is still present. Inspect it before packaging again: $($staleTransactions[0].FullName)"
}
if (Get-Process -Name $packagedProcessName -ErrorAction SilentlyContinue) {
    throw "The packaged sandbox is still running. Close HenkaSandbox3D.exe before refreshing the package."
}
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Build provenance was not found. Run .\scripts\build_windows.ps1 -Configuration $Configuration first."
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema_version -ne 2 -or $manifest.configuration -ne $Configuration) {
    throw "Build provenance does not match the current package contract or configuration $Configuration."
}

$expectedExeFull = [System.IO.Path]::GetFullPath($expectedExe)
$manifestExeFull = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $manifest.executable_relative_path))
if ($manifestExeFull -ne $expectedExeFull) {
    throw "Build provenance points to a different executable."
}
if (-not (Test-Path -LiteralPath $expectedExeFull -PathType Leaf)) {
    throw "The $Configuration sandbox executable was not found."
}

$currentCommit = Invoke-GitSingleLine @("rev-parse", "HEAD")
$currentState = Get-GitSourceState
$currentHash = (Get-FileHash -LiteralPath $expectedExeFull -Algorithm SHA256).Hash.ToLowerInvariant()
if ($manifest.commit_sha -ne $currentCommit) {
    throw "Build provenance commit does not match current HEAD. Rebuild before packaging."
}
if ($manifest.source_state -ne $currentState) {
    throw "Build provenance source state does not match the current working tree. Rebuild before packaging."
}
if ($manifest.executable_sha256 -ne $currentHash) {
    throw "Built executable hash does not match build provenance. Rebuild before packaging."
}

$assetsSource = Join-Path $repoRoot "assets"
$helpSource = Join-Path $repoRoot "docs\help\sandbox3d.md"
Assert-NoReparsePoints -Path $expectedExeFull -Description "Sandbox executable input"
Assert-NoReparsePoints -Path $assetsSource -Description "Asset input"
Assert-NoReparsePoints -Path $helpSource -Description "Offline help input"
if ((-not $ResetUserData) -and (Test-Path -LiteralPath $packageUserDir)) {
    Assert-NoReparsePoints -Path $packageUserDir -Description "Packaged user data"
}

try {
    [System.IO.Directory]::CreateDirectory($stagingRoot) | Out-Null
    $stagingDocsDir = Join-Path $stagingRoot "docs"
    $stagingHelpDir = Join-Path $stagingDocsDir "help"
    $stagingExe = Join-Path $stagingRoot "HenkaSandbox3D.exe"
    $stagingInfo = Join-Path $stagingRoot "PACKAGE_INFO.txt"
    $stagingReadme = Join-Path $stagingRoot "README.txt"
    [System.IO.Directory]::CreateDirectory($stagingHelpDir) | Out-Null

    Copy-Item -LiteralPath $expectedExeFull -Destination $stagingExe
    Copy-Item -LiteralPath $assetsSource -Destination $stagingRoot -Recurse
    Copy-Item -LiteralPath $helpSource -Destination (Join-Path $stagingHelpDir "sandbox3d.md")

    if ((-not $ResetUserData) -and (Test-Path -LiteralPath $packageUserDir)) {
        Copy-Item -LiteralPath $packageUserDir -Destination (Join-Path $stagingRoot "user") -Recurse
    }

    $sourceDir = Split-Path $expectedExeFull
    foreach ($dll in @(Get-ChildItem -LiteralPath $sourceDir -Filter *.dll -File -ErrorAction SilentlyContinue)) {
        Assert-NoReparsePoints -Path $dll.FullName -Description "Runtime library input"
        Copy-Item -LiteralPath $dll.FullName -Destination (Join-Path $stagingRoot $dll.Name)
    }

    $packagedHash = (Get-FileHash -LiteralPath $stagingExe -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($packagedHash -ne $currentHash) {
        throw "The staged executable does not match the validated build artifact."
    }

    $runGuide = @"
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
"@
    Write-HenkaUtf8NoBom -Path $stagingReadme -Text ($runGuide.TrimStart() + [Environment]::NewLine)

    $packageInfo = @"
Henka Engine Sandbox 3D package
Package schema: 3
Package refreshed UTC: $([DateTime]::UtcNow.ToString("o"))
Build generated UTC: $($manifest.generated_utc)
Source commit: $currentCommit
Source state: $currentState
Build configuration: $Configuration
Build branch: $($manifest.branch)
Build ref: $($manifest.git_ref)
Detached HEAD: $($manifest.detached_head)
Architecture: $($manifest.architecture)
CMake version: $($manifest.cmake_version)
Source executable: $($manifest.executable_relative_path)
Source executable SHA-256: $currentHash
Packaged executable SHA-256: $packagedHash
Executable: HenkaSandbox3D.exe
Runtime mode: Packaged
"@
    Write-HenkaUtf8NoBom -Path $stagingInfo -Text ($packageInfo.TrimStart() + [Environment]::NewLine)

    foreach ($requiredPath in @(
        $stagingExe,
        (Join-Path $stagingRoot "assets"),
        (Join-Path $stagingHelpDir "sandbox3d.md"),
        $stagingReadme,
        $stagingInfo
    )) {
        if (-not (Test-Path -LiteralPath $requiredPath)) {
            throw "The staged package is incomplete: $requiredPath"
        }
    }

    if (Test-Path -LiteralPath $packageRoot) {
        Move-Item -LiteralPath $packageRoot -Destination $backupRoot
    }

    Move-Item -LiteralPath $stagingRoot -Destination $packageRoot
    $activated = $true

    if (Test-Path -LiteralPath $backupRoot) {
        try {
            Remove-HenkaDirectoryTree -Path $backupRoot
        }
        catch {
            Write-Warning "The new package is active, but the old package backup could not be removed: $backupRoot"
        }
    }
}
catch {
    if (-not $activated -and
        (Test-Path -LiteralPath $backupRoot) -and
        -not (Test-Path -LiteralPath $packageRoot)) {
        Move-Item -LiteralPath $backupRoot -Destination $packageRoot
    }
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-HenkaDirectoryTree -Path $stagingRoot
    }
    throw
}

$packagedExe = Join-Path $packageRoot "HenkaSandbox3D.exe"
$packageInfoPath = Join-Path $packageRoot "PACKAGE_INFO.txt"
$finalHash = (Get-FileHash -LiteralPath $packagedExe -Algorithm SHA256).Hash.ToLowerInvariant()

Write-Host "Packaged sandbox ready:"
Write-Host "  $packagedExe"
Write-Host "Package marker:"
Write-Host "  $packageInfoPath"
Write-Host "Source commit: $currentCommit"
Write-Host "Source state: $currentState"
Write-Host "Configuration: $Configuration"
Write-Host "Executable SHA-256: $finalHash"
