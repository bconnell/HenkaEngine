param(
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot,

    [Parameter(Mandatory = $true)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,

    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$CMakePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$git = Get-HenkaGitPath

function Invoke-GitCapture {
    param([string[]]$Arguments)

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "SilentlyContinue"
        $output = @(& $git -C $RepoRoot @Arguments 2>$null)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }

    if ($exitCode -ne 0) {
        throw "Git provenance query failed: git $($Arguments -join ' ')"
    }
    return @($output | ForEach-Object { [string]$_ })
}

$repoPath = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd([char[]]@("\", "/"))
$repoPrefix = $repoPath + [System.IO.Path]::DirectorySeparatorChar
$exePath = [System.IO.Path]::GetFullPath($ExecutablePath)
$cmakePath = [System.IO.Path]::GetFullPath($CMakePath)

if (-not (Test-Path -LiteralPath (Join-Path $repoPath ".git") -PathType Container) -or
    -not (Test-Path -LiteralPath (Join-Path $repoPath "CMakeLists.txt") -PathType Leaf)) {
    throw "The provenance repository root is not a Henka checkout."
}
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "The built sandbox executable was not found: $exePath"
}
if (-not $exePath.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "The built sandbox executable is outside the repository."
}
if (-not (Test-Path -LiteralPath $cmakePath -PathType Leaf)) {
    throw "The resolved CMake executable was not found: $cmakePath"
}

$commitLines = @(Invoke-GitCapture @("rev-parse", "HEAD"))
$branchLines = @(Invoke-GitCapture @("branch", "--show-current"))
$statusLines = @(Invoke-GitCapture @("status", "--porcelain=v1", "--untracked-files=all"))
if ($commitLines.Count -ne 1 -or $branchLines.Count -gt 1) {
    throw "Git provenance output had an unexpected shape."
}

$detachedHead = $branchLines.Count -eq 0 -or [string]::IsNullOrWhiteSpace([string]$branchLines[0])
$branch = if ($detachedHead) { "detached" } else { ([string]$branchLines[0]).Trim() }
$gitRef = if (-not [string]::IsNullOrWhiteSpace($env:GITHUB_REF)) {
    $env:GITHUB_REF
}
elseif (-not [string]::IsNullOrWhiteSpace($env:GITHUB_REF_NAME)) {
    $env:GITHUB_REF_NAME
}
else {
    $branch
}
$headRef = if ([string]::IsNullOrWhiteSpace($env:GITHUB_HEAD_REF)) { "" } else { $env:GITHUB_HEAD_REF }

$cmakeVersionLines = @(& $cmakePath --version 2>$null)
if ($LASTEXITCODE -ne 0 -or $cmakeVersionLines.Count -lt 1) {
    throw "CMake version could not be read for build provenance."
}

$relativeExe = $exePath.Substring($repoPrefix.Length).Replace("\", "/")
$manifest = [ordered]@{
    schema_version = 2
    commit_sha = ([string]$commitLines[0]).Trim()
    branch = $branch
    git_ref = $gitRef
    head_ref = $headRef
    detached_head = $detachedHead
    source_state = if ($statusLines.Count -eq 0) { "clean" } else { "working-tree" }
    configuration = $Configuration
    architecture = if ([string]::IsNullOrWhiteSpace($env:PROCESSOR_ARCHITECTURE)) { "unknown" } else { $env:PROCESSOR_ARCHITECTURE }
    cmake_path = $cmakePath
    cmake_version = ([string]$cmakeVersionLines[0]).Trim()
    executable_relative_path = $relativeExe
    executable_sha256 = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash.ToLowerInvariant()
    executable_last_write_utc = (Get-Item -LiteralPath $exePath).LastWriteTimeUtc.ToString("o")
    generated_utc = [DateTime]::UtcNow.ToString("o")
}

$manifestPath = Join-Path $repoPath "build\henka-build-info.json"
$tempPath = $manifestPath + "." + [Guid]::NewGuid().ToString("N") + ".tmp"
$backupPath = $manifestPath + "." + [Guid]::NewGuid().ToString("N") + ".bak"
try {
    Write-HenkaUtf8NoBom `
        -Path $tempPath `
        -Text (($manifest | ConvertTo-Json -Depth 4) + [Environment]::NewLine)

    if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        [System.IO.File]::Replace($tempPath, $manifestPath, $backupPath, $true)
        Remove-Item -LiteralPath $backupPath -Force -ErrorAction SilentlyContinue
    }
    else {
        [System.IO.File]::Move($tempPath, $manifestPath)
    }
}
finally {
    Remove-Item -LiteralPath $tempPath -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $backupPath -Force -ErrorAction SilentlyContinue
}

Write-Host "Build provenance ready:"
Write-Host "  $manifestPath"
