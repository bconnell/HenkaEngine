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

function Write-PackagedAudioFixture {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [Parameter(Mandatory = $true)][string]$DestinationPath,
        [Parameter(Mandatory = $true)][string]$ExpectedSha256,
        [Parameter(Mandatory = $true)][string]$Description
    )

    if (-not (Test-Path -LiteralPath $SourcePath -PathType Leaf)) {
        throw "$Description source is missing: $SourcePath"
    }
    $encoded = [System.IO.File]::ReadAllText($SourcePath).Trim()
    if ($encoded.Length -eq 0 -or $encoded.Length -gt 16777216) {
        throw "$Description source exceeds the bounded Base64 input contract."
    }
    if ($encoded.StartsWith('"') -and $encoded.EndsWith('"')) {
        $encoded = $encoded.Substring(1, $encoded.Length - 2)
    }
    try {
        $bytes = [Convert]::FromBase64String($encoded)
    }
    catch {
        throw "$Description source is not valid Base64: $SourcePath"
    }
    if ($bytes.Length -eq 0 -or $bytes.Length -gt 8388608) {
        throw "$Description decoded payload exceeds the bounded package contract."
    }
    [System.IO.File]::WriteAllBytes($DestinationPath, $bytes)
    $actualSha256 = (Get-FileHash -LiteralPath $DestinationPath -Algorithm SHA256).Hash
    if ($actualSha256 -ne $ExpectedSha256) {
        throw "$Description hash does not match its checked-in source contract."
    }
}

function Test-HenkaPackageDirectoryComplete {
    param([Parameter(Mandatory = $true)][string]$Path)

    foreach ($requiredRelativePath in @(
        "HenkaSandbox3D.exe",
        "assets",
        "assets\branding\henka_engine_emblem.png",
        "assets\branding\henka_engine_lockup.png",
        "assets\audio\henka_audio_fixture.ogg",
        "assets\audio\henka_audio_fixture.mp3",
        "assets\audio\henka_audio_fixture.flac",
        "docs\help\sandbox3d.md",
        "README.txt",
        "PACKAGE_INFO.txt"
    )) {
        if (-not (Test-Path -LiteralPath (Join-Path $Path $requiredRelativePath))) {
            return $false
        }
    }

    return $true
}

[System.IO.Directory]::CreateDirectory($outRoot) | Out-Null
$staleStagingDirectories = @(
    Get-ChildItem -LiteralPath $outRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name.StartsWith(".HenkaSandbox3D-staging-", [System.StringComparison]::OrdinalIgnoreCase)
        }
)
if ($staleStagingDirectories.Count -gt 0) {
    throw "A prior package staging transaction is still present. Inspect it before packaging again: $($staleStagingDirectories[0].FullName)"
}

$staleBackupDirectories = @(
    Get-ChildItem -LiteralPath $outRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name.StartsWith(".HenkaSandbox3D-backup-", [System.StringComparison]::OrdinalIgnoreCase)
        }
)
if ($staleBackupDirectories.Count -gt 0) {
    if (-not (Test-HenkaPackageDirectoryComplete -Path $packageRoot)) {
        throw "A prior package backup exists, but the active package cannot be proven complete: $($staleBackupDirectories[0].FullName)"
    }

    Assert-NoReparsePoints -Path $packageRoot -Description "Active package recovery input"
    foreach ($staleBackup in $staleBackupDirectories) {
        Assert-NoReparsePoints -Path $staleBackup.FullName -Description "Stale package backup"
        try {
            Remove-HenkaDirectoryTree -Path $staleBackup.FullName
        }
        catch {
            throw "The active package is complete, but a stale backup could not be removed safely: $($staleBackup.FullName)"
        }
    }

    Write-Host "Recovered validated stale package backup state."
}
if (Get-Process -Name $packagedProcessName -ErrorAction SilentlyContinue) {
    throw "The packaged sandbox is still running. Close HenkaSandbox3D.exe before refreshing the package."
}
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Build provenance was not found. Run .\scripts\build_windows.ps1 -Configuration $Configuration first."
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema_version -ne 3 -or $manifest.configuration -ne $Configuration) {
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
$currentSourceIdentity = Get-HenkaSourceIdentity -RepoRoot $repoRoot
$currentState = $currentSourceIdentity.source_state
$currentHash = (Get-FileHash -LiteralPath $expectedExeFull -Algorithm SHA256).Hash.ToLowerInvariant()
if ($manifest.commit_sha -ne $currentCommit) {
    throw "Build provenance commit does not match current HEAD. Rebuild before packaging."
}
if ($manifest.source_state -ne $currentState) {
    throw "Build provenance source state does not match the current working tree. Rebuild before packaging."
}
if ($manifest.source_identity -ne $currentSourceIdentity.source_identity) {
    throw "Build provenance source identity does not match the current candidate. Rebuild before packaging."
}
if ($manifest.executable_sha256 -ne $currentHash) {
    throw "Built executable hash does not match build provenance. Rebuild before packaging."
}

$assetsSource = Join-Path $repoRoot "assets"
$helpSource = Join-Path $repoRoot "docs\help\sandbox3d.md"
$residencyFixtureSource = Join-Path $repoRoot "build\examples\sandbox3d\$Configuration\assets\textures\residency"
$residencyGenerator = Join-Path $repoRoot "scripts\generate_residency_fixtures_windows.ps1"
$showcaseModelSource = Join-Path $repoRoot "build\examples\sandbox3d\$Configuration\assets\models"
Assert-NoReparsePoints -Path $expectedExeFull -Description "Sandbox executable input"
Assert-NoReparsePoints -Path $assetsSource -Description "Asset input"
Assert-NoReparsePoints -Path $helpSource -Description "Offline help input"
if (-not (Test-Path -LiteralPath $showcaseModelSource -PathType Container)) {
    throw "The $Configuration sandbox showcase model output was not found beside the validated executable. Rebuild before packaging."
}
Assert-NoReparsePoints -Path $showcaseModelSource -Description "Sandbox showcase model input"

Invoke-HenkaNative `
    -FilePath "powershell.exe" `
    -Arguments @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        $residencyGenerator,
        "-OutputDirectory",
        $residencyFixtureSource) `
    -WorkingDirectory $repoRoot `
    -Label "Generate opt-in Sandbox residency fixtures without rebuilding"

if (-not (Test-Path -LiteralPath $residencyFixtureSource -PathType Container)) {
    throw "The opt-in $Configuration sandbox residency fixtures were not generated."
}
Assert-NoReparsePoints -Path $residencyFixtureSource -Description "Sandbox residency fixture input"
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
    $stagingAudioDir = Join-Path $stagingRoot "assets\audio"
    [System.IO.Directory]::CreateDirectory($stagingAudioDir) | Out-Null
    foreach ($fixture in @(
        @{ Name = "henka_audio_fixture.ogg"; Source = "audio_fixture_ogg.b64"; Sha256 = "87900F83DE1ECCEF7D79199AB336C77186458CDF1718593E6A7E9518FCDA812A" },
        @{ Name = "henka_audio_fixture.mp3"; Source = "audio_fixture_mp3.b64"; Sha256 = "F252E5A6F10FFBEF00D80095AA6FFEFA231AD7232B8E37AEEA7CFFD760DE0713" },
        @{ Name = "henka_audio_fixture.flac"; Source = "audio_fixture_flac.b64"; Sha256 = "8730C5E7672781DBA7EF3105DD7BD222425537CAE3D3C5AB237CFDF918B86483" }
    )) {
        Write-PackagedAudioFixture `
            -SourcePath (Join-Path $repoRoot ("tests\fixtures\{0}" -f $fixture.Source)) `
            -DestinationPath (Join-Path $stagingAudioDir $fixture.Name) `
            -ExpectedSha256 $fixture.Sha256 `
            -Description ("Packaged Audio fixture {0}" -f $fixture.Name)
    }
    $stagingModelsDir = Join-Path $stagingRoot "assets\models"
    [System.IO.Directory]::CreateDirectory($stagingModelsDir) | Out-Null
    foreach ($showcaseFile in @(
        "cheeky_giraffe.gltf",
        "cheeky_giraffe.bin",
        "giraffe_base_color.png",
        "giraffe_detail_normal.png",
        "giraffe_metallic_roughness.png",
        "original_realistic_rocket.gltf",
        "original_realistic_rocket.bin",
        "rocket_base_color.png",
        "rocket_detail_normal.png",
        "rocket_metallic_roughness.png"
    )) {
        $showcaseSourceFile = Join-Path $showcaseModelSource $showcaseFile
        if (-not (Test-Path -LiteralPath $showcaseSourceFile -PathType Leaf)) {
            throw "The validated Sandbox showcase output is missing $showcaseFile. Rebuild before packaging."
        }
        Assert-NoReparsePoints -Path $showcaseSourceFile -Description "Sandbox showcase asset input"
        Copy-Item -LiteralPath $showcaseSourceFile -Destination $stagingModelsDir
    }
    Copy-Item -LiteralPath $helpSource -Destination (Join-Path $stagingHelpDir "sandbox3d.md")

    if ((-not $ResetUserData) -and (Test-Path -LiteralPath $packageUserDir)) {
        Copy-Item -LiteralPath $packageUserDir -Destination (Join-Path $stagingRoot "user") -Recurse
    }

    $sourceDir = Split-Path $expectedExeFull
    $stagingResidencyDir = Join-Path $stagingRoot "assets\textures\residency"
    [System.IO.Directory]::CreateDirectory($stagingResidencyDir) | Out-Null
    for ($residencyIndex = 0; $residencyIndex -lt 65; $residencyIndex++) {
        $residencySource = Join-Path $residencyFixtureSource ("residency_{0}.png" -f $residencyIndex)
        if (-not (Test-Path -LiteralPath $residencySource -PathType Leaf)) {
            throw "The generated residency fixture set is missing residency_$residencyIndex.png."
        }
        Copy-Item -LiteralPath $residencySource -Destination $stagingResidencyDir
    }
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
The in-window panels open automatically so Tools and Physics QA are visible without pressing F4 first.
Press F4 to hide or show the in-window panels.
Press F5 to switch Standard and Focus Viewport.
The scene renders inside its own docked viewport when panels are visible.
Starts have no selected scene object until you select one.
Select an object in the viewport or Scene Objects panel, then use Select, Orbit, Pan, Move, Rotate, and Scale from the Viewport Tool section.
Use M or G, R, and S for action-based transforms. X, Y, and Z constrain an active transform; Enter applies it and Escape cancels it.
Use the in-window utilities for help, legend, paths, settings, diagnostics, Transform QA, and Physics QA.
Editable selected scene objects show a viewport transform highlight until selection is cleared.
While native mesh editing is active, the most recently picked vertex, edge, or face receives a stronger mode-specific highlight while multi-selection remains visible.
Soft Move X+/Soft Move Y+/Soft Move Z+ applies a bounded one-ring linear falloff to the active topology selection to reduce hard seams while shaping imported fixture regions.
Locked objects remain selectable for inspection without a transform highlight or gizmo. Ground starts locked and requires an explicit Unlock Transform action before it can move.
Clearing selection also clears active transform-session ownership, and viewport overlays do not draw over panels.
Physics QA explains Static, Dynamic, and Kinematic bodies. Make Dynamic + Drop activates only the selected supported body; Enable starts the full arranged demonstration.
DRAG marks a live panel header. Release over a valid left or right outline to dock there, or release away from the outlines to open a separate native tool window.
Open Native Panel Test from the Tools QA page to exercise a separate OS-level validation window.
Close a detached tool window to return its panel to the last valid dock.
Use Reset Layout to recover panels and default dock sizes.
If saved live workspace geometry is incompatible, Henka restores current safe defaults and rewrites them after a clean shutdown.
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
Package schema: 4
Package refreshed UTC: $([DateTime]::UtcNow.ToString("o"))
Build generated UTC: $($manifest.generated_utc)
Source commit: $currentCommit
Source state: $currentState
Source identity: $($currentSourceIdentity.source_identity)
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
            throw "The new package is active, but backup cleanup failed. The next guarded run will recover this validated state: $backupRoot"
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
