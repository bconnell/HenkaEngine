param(
    [switch]$ResetSaveData,

    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$outRoot = Join-Path $repoRoot "out"
$packageRoot = Join-Path $outRoot "HenkaDedicatedServer"
$packageSaveRoot = Join-Path $packageRoot "save"
$expectedExe = Join-Path $repoRoot ("build\examples\dedicated_server\{0}\henka_dedicated_server.exe" -f $Configuration)
$configSource = Join-Path $repoRoot "examples\dedicated_server\server.conf.example"
$docsSource = Join-Path $repoRoot "docs\dedicated-server.md"
$git = Get-HenkaGitPath
$transactionId = [Guid]::NewGuid().ToString("N")
$stagingRoot = Join-Path $outRoot (".HenkaDedicatedServer-staging-" + $transactionId)
$backupRoot = Join-Path $outRoot (".HenkaDedicatedServer-backup-" + $transactionId)
$activated = $false

function Invoke-HenkaGitSingleLine {
    param([string[]]$Arguments)
    $lines = @(& $git -C $repoRoot @Arguments 2>$null)
    if ($LASTEXITCODE -ne 0 -or $lines.Count -ne 1 -or [string]::IsNullOrWhiteSpace([string]$lines[0])) {
        throw "Git provenance query failed: git $($Arguments -join ' ')"
    }
    return ([string]$lines[0]).Trim()
}

function Get-HenkaSourceState {
    $lines = @(& $git -C $repoRoot status --porcelain=v1 --untracked-files=all 2>$null)
    if ($LASTEXITCODE -ne 0) { throw "Git source-state query failed." }
    if ($lines.Count -eq 0) { return "clean" }
    return "working-tree"
}

function Remove-HenkaDirectoryTree {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
    }
}

function Assert-NoReparsePoints {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return }
    $root = Get-Item -LiteralPath $Path -Force
    $items = @($root)
    if ($root.PSIsContainer) { $items += @(Get-ChildItem -LiteralPath $Path -Recurse -Force) }
    foreach ($item in $items) {
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "A package input contains a reparse point: $($item.FullName)"
        }
    }
}

function Test-PackageComplete {
    param([string]$Path)
    foreach ($relative in @("henka_dedicated_server.exe", "server.conf.example", "README.txt", "docs\dedicated-server.md", "PACKAGE_INFO.txt", "save")) {
        if (-not (Test-Path -LiteralPath (Join-Path $Path $relative))) { return $false }
    }
    return $true
}

if (-not (Test-Path -LiteralPath $expectedExe -PathType Leaf)) {
    throw "The $Configuration dedicated server executable was not found. Build it before packaging."
}
foreach ($input in @($expectedExe, $configSource, $docsSource)) {
    if (-not (Test-Path -LiteralPath $input -PathType Leaf)) { throw "Required package input was not found: $input" }
    Assert-NoReparsePoints -Path $input
}
if (Get-Process -Name "henka_dedicated_server" -ErrorAction SilentlyContinue) {
    throw "The dedicated server is still running. Stop it before refreshing the package."
}

$currentCommit = Invoke-HenkaGitSingleLine @("rev-parse", "HEAD")
$sourceState = Get-HenkaSourceState
$sourceHash = (Get-FileHash -LiteralPath $expectedExe -Algorithm SHA256).Hash.ToLowerInvariant()
[System.IO.Directory]::CreateDirectory($outRoot) | Out-Null
$staleStaging = @(Get-ChildItem -LiteralPath $outRoot -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name.StartsWith(".HenkaDedicatedServer-staging-", [System.StringComparison]::OrdinalIgnoreCase) })
if ($staleStaging.Count -gt 0) { throw "A prior dedicated-server staging transaction remains: $($staleStaging[0].FullName)" }
$staleBackups = @(Get-ChildItem -LiteralPath $outRoot -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name.StartsWith(".HenkaDedicatedServer-backup-", [System.StringComparison]::OrdinalIgnoreCase) })
if ($staleBackups.Count -gt 0) {
    if (-not (Test-PackageComplete -Path $packageRoot)) { throw "A stale backup exists and the active package is not proven complete." }
    Assert-NoReparsePoints -Path $packageRoot
    foreach ($backup in $staleBackups) { Assert-NoReparsePoints -Path $backup.FullName; Remove-HenkaDirectoryTree -Path $backup.FullName }
}
if (Test-Path -LiteralPath $packageRoot) { Assert-NoReparsePoints -Path $packageRoot }

try {
    [System.IO.Directory]::CreateDirectory($stagingRoot) | Out-Null
    [System.IO.Directory]::CreateDirectory((Join-Path $stagingRoot "docs")) | Out-Null
    [System.IO.Directory]::CreateDirectory((Join-Path $stagingRoot "save")) | Out-Null
    Copy-Item -LiteralPath $expectedExe -Destination (Join-Path $stagingRoot "henka_dedicated_server.exe")
    Copy-Item -LiteralPath $configSource -Destination (Join-Path $stagingRoot "server.conf.example")
    Copy-Item -LiteralPath $docsSource -Destination (Join-Path $stagingRoot "docs\dedicated-server.md")
    if (-not $ResetSaveData -and (Test-Path -LiteralPath $packageSaveRoot)) {
        Assert-NoReparsePoints -Path $packageSaveRoot
        Copy-Item -LiteralPath $packageSaveRoot -Destination (Join-Path $stagingRoot "save") -Recurse -Force
    }
    $packagedHash = (Get-FileHash -LiteralPath (Join-Path $stagingRoot "henka_dedicated_server.exe") -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($packagedHash -ne $sourceHash) { throw "The staged dedicated server hash differs from the validated build artifact." }
    $readme = @"
Henka Engine dedicated server

Run henka_dedicated_server.exe with server.conf.example as a starting point.
The server is renderer-free and intended for infrastructure controlled by the
developer or game operator. Accepted Terrain edits are stored transactionally
under save/ before acknowledgement.

Example:
  henka_dedicated_server.exe --config server.conf.example
  henka_dedicated_server.exe --smoke --bind 127.0.0.1 --port 7813 --save-root save

See docs\dedicated-server.md for build, deployment, and validation details.
"@
    Write-HenkaUtf8NoBom -Path (Join-Path $stagingRoot "README.txt") -Text ($readme.TrimStart() + [Environment]::NewLine)
    $info = @"
Henka Engine dedicated server package
Package schema: 1
Package refreshed UTC: $([DateTime]::UtcNow.ToString("o"))
Source commit: $currentCommit
Source state: $sourceState
Build configuration: $Configuration
Source executable: build/examples/dedicated_server/$Configuration/henka_dedicated_server.exe
Source executable SHA-256: $sourceHash
Packaged executable SHA-256: $packagedHash
Runtime dependencies: Henka runtime and ENet are statically linked; no client renderer, SDL, OpenGL, or KTX runtime is packaged.
Executable: henka_dedicated_server.exe
Save root: save/
"@
    Write-HenkaUtf8NoBom -Path (Join-Path $stagingRoot "PACKAGE_INFO.txt") -Text ($info.TrimStart() + [Environment]::NewLine)
    if (-not (Test-PackageComplete -Path $stagingRoot)) { throw "The staged dedicated-server package is incomplete." }
    if (Test-Path -LiteralPath $packageRoot) { Move-Item -LiteralPath $packageRoot -Destination $backupRoot }
    Move-Item -LiteralPath $stagingRoot -Destination $packageRoot
    $activated = $true
    if (Test-Path -LiteralPath $backupRoot) { Remove-HenkaDirectoryTree -Path $backupRoot }
}
catch {
    if (-not $activated -and (Test-Path -LiteralPath $backupRoot) -and -not (Test-Path -LiteralPath $packageRoot)) {
        Move-Item -LiteralPath $backupRoot -Destination $packageRoot
    }
    if (Test-Path -LiteralPath $stagingRoot) { Remove-HenkaDirectoryTree -Path $stagingRoot }
    throw
}

Write-Host "Dedicated server package ready: $(Join-Path $packageRoot 'henka_dedicated_server.exe')"
Write-Host "Package marker: $(Join-Path $packageRoot 'PACKAGE_INFO.txt')"
Write-Host "Source commit: $currentCommit"
Write-Host "Source state: $sourceState"
Write-Host "Configuration: $Configuration"
Write-Host "Executable SHA-256: $sourceHash"
