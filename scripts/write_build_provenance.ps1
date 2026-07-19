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

function Invoke-GitCapture {
    param([string[]]$Arguments)

    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "SilentlyContinue"
        $output = @(& git.exe -C $RepoRoot @Arguments 2>$null)
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

$repoPath = [System.IO.Path]::GetFullPath($RepoRoot)
$exePath = [System.IO.Path]::GetFullPath($ExecutablePath)
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "The built sandbox executable was not found: $exePath"
}
if (-not $exePath.StartsWith($repoPath, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "The built sandbox executable is outside the repository."
}

$commitLines = @(Invoke-GitCapture @("rev-parse", "HEAD"))
$branchLines = @(Invoke-GitCapture @("branch", "--show-current"))
$statusLines = @(Invoke-GitCapture @("status", "--porcelain=v1", "--untracked-files=all"))
if ($commitLines.Count -ne 1 -or $branchLines.Count -ne 1) {
    throw "Git provenance output had an unexpected shape."
}

$cmakeVersionLines = @(& $CMakePath --version 2>$null)
if ($LASTEXITCODE -ne 0 -or $cmakeVersionLines.Count -lt 1) {
    throw "CMake version could not be read for build provenance."
}

$relativeExe = $exePath.Substring($repoPath.Length).TrimStart("\", "/").Replace("\", "/")
$manifest = [ordered]@{
    schema_version = 1
    commit_sha = ([string]$commitLines[0]).Trim()
    branch = ([string]$branchLines[0]).Trim()
    source_state = if ($statusLines.Count -eq 0) { "clean" } else { "working-tree" }
    configuration = $Configuration
    architecture = if ([string]::IsNullOrWhiteSpace($env:PROCESSOR_ARCHITECTURE)) { "unknown" } else { $env:PROCESSOR_ARCHITECTURE }
    cmake_path = $CMakePath
    cmake_version = ([string]$cmakeVersionLines[0]).Trim()
    executable_relative_path = $relativeExe
    executable_sha256 = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash.ToLowerInvariant()
    executable_last_write_utc = (Get-Item -LiteralPath $exePath).LastWriteTimeUtc.ToString("o")
    generated_utc = [DateTime]::UtcNow.ToString("o")
}

$manifestPath = Join-Path $repoPath "build\henka-build-info.json"
$encoding = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText(
    $manifestPath,
    (($manifest | ConvertTo-Json -Depth 4) + [Environment]::NewLine),
    $encoding)

Write-Host "Build provenance ready:"
Write-Host "  $manifestPath"
