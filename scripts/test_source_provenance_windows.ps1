param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$git = Get-HenkaGitPath
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
    "henka-source-provenance-" + [Guid]::NewGuid().ToString("N"))
$repoA = Join-Path $tempRoot "repo-a"
$repoB = Join-Path $tempRoot "repo-b"

function Invoke-TestGit {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $output = @(& $git -C $WorkingDirectory @Arguments)
    if ($LASTEXITCODE -ne 0) {
        throw "Git command failed in ${WorkingDirectory}: git $($Arguments -join ' ')"
    }
    return @($output | ForEach-Object { [string]$_ })
}

function Assert-Test {
    param(
        [Parameter(Mandatory = $true)]
        [bool]$Condition,

        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

try {
    [System.IO.Directory]::CreateDirectory($tempRoot) | Out-Null
    $crossShellScript = Join-Path $tempRoot "cross-shell.ps1"
    Write-HenkaUtf8NoBom -Path $crossShellScript -Text @'
param(
    [Parameter(Mandatory = $true)][string]$RepoRoot,
    [Parameter(Mandatory = $true)][string]$CommonPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. $CommonPath
$identity = Get-HenkaSourceIdentity -RepoRoot $RepoRoot
Write-Output ("{0}|{1}|{2}" -f $identity.commit_sha,$identity.source_state,$identity.source_identity)
'@
    $crossShellOutput = @(
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $crossShellScript `
            -RepoRoot $repoRoot `
            -CommonPath (Join-Path $PSScriptRoot "henka_script_common.ps1") 2>&1 |
            ForEach-Object { [string]$_ }
    )
    $crossShellExitCode = $LASTEXITCODE
    Assert-Test ($crossShellExitCode -eq 0) `
        "The Windows PowerShell source identity probe failed: $($crossShellOutput -join [Environment]::NewLine)"
    $crossShellRecord = @($crossShellOutput | Where-Object { $_ -match '^[0-9a-f]{40}\|(clean|working-tree)\|[0-9a-f]{64}$' })
    Assert-Test ($crossShellRecord.Count -eq 1) `
        "The Windows PowerShell source identity probe returned an unexpected record: $($crossShellOutput -join [Environment]::NewLine)"
    $crossShellFields = $crossShellRecord[0].Split('|', 3)
    $currentIdentity = Get-HenkaSourceIdentity -RepoRoot $repoRoot
    Assert-Test ($crossShellFields[0] -eq $currentIdentity.commit_sha -and
        $crossShellFields[1] -eq $currentIdentity.source_state -and
        $crossShellFields[2] -eq $currentIdentity.source_identity) `
        "Source identity must be identical across the repository's supported PowerShell hosts."

    [System.IO.Directory]::CreateDirectory($repoA) | Out-Null
    Invoke-TestGit -WorkingDirectory $repoA -Arguments @("init", "--quiet") | Out-Null
    Invoke-TestGit -WorkingDirectory $repoA -Arguments @("config", "user.name", "Henka provenance test") | Out-Null
    Invoke-TestGit -WorkingDirectory $repoA -Arguments @("config", "user.email", "henka-provenance@example.invalid") | Out-Null
    Set-Content -LiteralPath (Join-Path $repoA "source.txt") -Value "base" -NoNewline
    Invoke-TestGit -WorkingDirectory $repoA -Arguments @("add", "source.txt") | Out-Null
    Invoke-TestGit -WorkingDirectory $repoA -Arguments @(
        "-c", "user.name=Henka provenance test",
        "-c", "user.email=henka-provenance@example.invalid",
        "commit", "--quiet", "-m", "base") | Out-Null
    Invoke-TestGit -WorkingDirectory $repoA -Arguments @("clone", "--quiet", $repoA, $repoB) | Out-Null

    $cleanA = Get-HenkaSourceIdentity -RepoRoot $repoA
    $cleanB = Get-HenkaSourceIdentity -RepoRoot $repoB
    Assert-Test ($cleanA.source_state -eq "clean" -and $cleanB.source_state -eq "clean") `
        "A clean checkout must report clean source state."
    Assert-Test ($cleanA.commit_sha -eq $cleanB.commit_sha) `
        "Cloned provenance fixtures must share the same HEAD."
    Assert-Test ($cleanA.source_identity -eq $cleanB.source_identity) `
        "Equivalent clean source trees must share a source identity."

    Set-Content -LiteralPath (Join-Path $repoA "source.txt") -Value "dirty-a" -NoNewline
    Set-Content -LiteralPath (Join-Path $repoB "source.txt") -Value "dirty-b" -NoNewline
    $dirtyA = Get-HenkaSourceIdentity -RepoRoot $repoA
    $dirtyB = Get-HenkaSourceIdentity -RepoRoot $repoB
    Assert-Test ($dirtyA.commit_sha -eq $dirtyB.commit_sha) `
        "Dirty provenance fixtures must keep the same base HEAD for this regression."
    Assert-Test ($dirtyA.source_state -eq "working-tree" -and $dirtyB.source_state -eq "working-tree") `
        "Dirty provenance fixtures must retain the working-tree state label."
    Assert-Test ($dirtyA.source_identity -ne $dirtyB.source_identity) `
        "Distinct dirty source trees must not share one provenance identity."
    Assert-Test ($dirtyA.source_identity -ne $cleanA.source_identity) `
        "A dirty source tree must not share the clean checkout identity."

    Set-Content -LiteralPath (Join-Path $repoA "untracked.txt") -Value "untracked" -NoNewline
    $untrackedA = Get-HenkaSourceIdentity -RepoRoot $repoA
    Assert-Test ($untrackedA.source_identity -ne $dirtyA.source_identity) `
        "Candidate identity must include untracked candidate files."

    foreach ($relativePath in @(
        "scripts\write_build_provenance.ps1",
        "scripts\package_sandbox3d_windows.ps1",
        "scripts\check_packaged_sandbox3d_windows.ps1",
        "scripts\package_dedicated_server_windows.ps1",
        "scripts\check_packaged_dedicated_server_windows.ps1")) {
        $path = Join-Path $repoRoot $relativePath
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Source provenance integration input is missing: $path"
        }
        $text = Get-Content -LiteralPath $path -Raw
        Assert-Test ($text.Contains("source_identity")) `
            "$relativePath must carry the exact source identity through its contract."
    }

    Write-Output "Source provenance identity regression passed."
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
