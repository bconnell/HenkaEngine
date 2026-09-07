[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,

    [Parameter(Mandatory = $true)]
    [string]$CandidatePath,

    [switch]$Remove,

    [string[]]$RequireIncludedPath = @(),

    [string[]]$RequireExcludedPath = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$git = Get-HenkaGitPath
$repository = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$candidate = [System.IO.Path]::GetFullPath($CandidatePath)

function Assert-DirectoryIsSafe {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Label
    )

    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if (-not $item.PSIsContainer) {
        throw "$Label is not a directory: $Path"
    }
    if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "$Label is a reparse point; refusing to operate on it: $Path"
    }
}

function Invoke-RepositoryGit {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    # Git writes the normal worktree preparation notice to stderr. Capture it
    # as command output so PowerShell's Stop policy does not misclassify a
    # successful exact-candidate materialization as a harness failure.
    $output = @(& $git -C $repository @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        $details = ($output | ForEach-Object { [string]$_ }) -join "`n"
        throw "Git command failed: git -C $repository $($Arguments -join ' ')`n$details"
    }
    return @($output | ForEach-Object { ([string]$_).TrimEnd() })
}

function Normalize-RepositoryPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "Repository path requirements cannot be empty."
    }
    $normalized = $Path.Trim().Replace("\", "/").TrimStart("/")
    if ([System.IO.Path]::IsPathRooted($Path) -or $normalized.Contains(":") -or
        @($normalized -split "/" | Where-Object { $_ -eq ".." }).Count -gt 0) {
        throw "Repository path requirements must be relative and confined to the repository: $Path"
    }
    return $normalized
}

function Get-TreePaths {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Commit,

        [Parameter(Mandatory = $true)]
        [string]$RepositoryPath
    )

    return @(Invoke-RepositoryGit @("ls-tree", "-r", "--name-only", $Commit, "--", $RepositoryPath))
}

function Test-TreeContainsPath {
    param(
        [AllowEmptyCollection()]
        [Parameter(Mandatory = $true)]
        [string[]]$TreePaths,

        [Parameter(Mandatory = $true)]
        [string]$RepositoryPath
    )

    foreach ($treePath in $TreePaths) {
        if ($treePath -eq $RepositoryPath -or $treePath.StartsWith($RepositoryPath + "/")) {
            return $true
        }
    }
    return $false
}

function Assert-RequirementsAgainstTree {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Commit
    )

    foreach ($requiredPath in @($RequireIncludedPath)) {
        $normalized = Normalize-RepositoryPath $requiredPath
        $treePaths = @(Get-TreePaths -Commit $Commit -RepositoryPath $normalized)
        if ($null -eq $treePaths) {
            $treePaths = @()
        }
        if (-not (Test-TreeContainsPath -TreePaths $treePaths -RepositoryPath $normalized)) {
            throw "Required included path is not present in the staged candidate tree: $normalized"
        }
    }

    foreach ($excludedPath in @($RequireExcludedPath)) {
        $normalized = Normalize-RepositoryPath $excludedPath
        $treePaths = @(Get-TreePaths -Commit $Commit -RepositoryPath $normalized)
        if ($null -eq $treePaths) {
            $treePaths = @()
        }
        if (Test-TreeContainsPath -TreePaths $treePaths -RepositoryPath $normalized) {
            throw "Required excluded path is present in the staged candidate tree: $normalized"
        }
    }
}

function Assert-CandidateBlobMatchesIndex {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CandidateCommit,

        [Parameter(Mandatory = $true)]
        [string]$RepositoryPath
    )

    $indexBlob = (@(Invoke-RepositoryGit @("rev-parse", ":$RepositoryPath")))[0]
    $candidateBlob = (@(Invoke-RepositoryGit @("rev-parse", "$CandidateCommit`:$RepositoryPath")))[0]
    if ($indexBlob -ne $candidateBlob) {
        throw "Candidate blob differs from the staged index for $RepositoryPath."
    }
}

function Assert-WorktreeIsCleanDetached {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CandidateCommit
    )

    $candidateHead = @(& $git -C $candidate rev-parse HEAD)
    if ($LASTEXITCODE -ne 0 -or $candidateHead.Count -ne 1 -or
        ([string]$candidateHead[0]).Trim() -ne $CandidateCommit) {
        throw "Exact candidate HEAD does not match the temporary candidate commit $CandidateCommit."
    }
    $status = @(& $git -C $candidate status --porcelain --branch)
    if ($LASTEXITCODE -ne 0 -or $status.Count -ne 1 -or
        ([string]$status[0]).Trim() -ne "## HEAD (no branch)") {
        throw "Exact candidate worktree is not clean and detached: $($status -join ' | ')"
    }
}

Assert-DirectoryIsSafe -Path $repository -Label "Repository root"

if ([System.StringComparer]::OrdinalIgnoreCase.Equals(
        $repository.TrimEnd("\"), $candidate.TrimEnd("\"))) {
    throw "Candidate path must not be the canonical repository root."
}

if ($Remove) {
    if (@($RequireIncludedPath).Count -gt 0 -or @($RequireExcludedPath).Count -gt 0) {
        throw "-Remove cannot be combined with path requirements."
    }
    if (-not (Test-Path -LiteralPath $candidate -PathType Container)) {
        throw "Candidate worktree does not exist: $candidate"
    }
    Assert-DirectoryIsSafe -Path $candidate -Label "Candidate worktree"

    $worktreeRecords = @(Invoke-RepositoryGit @("worktree", "list", "--porcelain"))
    $registered = $false
    foreach ($record in $worktreeRecords) {
        if ($record.StartsWith("worktree ")) {
            $registeredPath = [System.IO.Path]::GetFullPath($record.Substring(9))
            if ([System.StringComparer]::OrdinalIgnoreCase.Equals(
                    $registeredPath.TrimEnd("\"), $candidate.TrimEnd("\"))) {
                $registered = $true
                break
            }
        }
    }
    if (-not $registered) {
        throw "Candidate path is not a registered Git worktree; refusing to remove it: $candidate"
    }

    $removeOutput = @(& $git -C $repository worktree remove $candidate)
    if ($LASTEXITCODE -ne 0) {
        $details = ($removeOutput | ForEach-Object { [string]$_ }) -join "`n"
        throw "Git refused non-force candidate removal: $details"
    }
    if (Test-Path -LiteralPath $candidate) {
        throw "Candidate worktree still exists after removal: $candidate"
    }
    Write-Host "[pass] Removed exact candidate worktree without force: $candidate"
    return
}

if (Test-Path -LiteralPath $candidate) {
    throw "Candidate path already exists; refusing to overwrite it: $candidate"
}
$candidateParent = Split-Path -Parent $candidate
if (-not (Test-Path -LiteralPath $candidateParent -PathType Container)) {
    throw "Candidate parent directory must already exist: $candidateParent"
}
Assert-DirectoryIsSafe -Path $candidateParent -Label "Candidate parent directory"

$head = (@(Invoke-RepositoryGit @("rev-parse", "HEAD")))[0]
$tree = (@(Invoke-RepositoryGit @("write-tree")))[0]
if ([string]::IsNullOrWhiteSpace($tree)) {
    throw "git write-tree returned no index tree."
}
$commitOutput = @(Invoke-RepositoryGit @(
        "commit-tree",
        $tree,
        "-p",
        $head,
        "-m",
        "Henka exact validation candidate"
    ))
$candidateCommit = $commitOutput[-1]
if ($candidateCommit -notmatch "^[0-9a-f]{40,64}$") {
    throw "git commit-tree returned an invalid candidate commit: $candidateCommit"
}

Assert-RequirementsAgainstTree -Commit $candidateCommit

$stagedPaths = @(Invoke-RepositoryGit @("diff", "--cached", "--name-only"))
foreach ($stagedPath in $stagedPaths) {
    $normalizedStagedPath = Normalize-RepositoryPath $stagedPath
    Assert-CandidateBlobMatchesIndex -CandidateCommit $candidateCommit -RepositoryPath $normalizedStagedPath
}

$candidateCreated = $false
try {
    # A successful worktree add writes a progress notice to stderr. Keep the
    # native diagnostic observable without allowing PowerShell Stop semantics
    # to convert it into a terminating error before the exit code is checked.
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $worktreeOutput = @(& $git -C $repository worktree add --detach $candidate $candidateCommit 2>&1)
        $worktreeExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($worktreeExitCode -ne 0) {
        $details = ($worktreeOutput | ForEach-Object { [string]$_ }) -join "`n"
        throw "Git could not create the exact candidate worktree: $details"
    }
    $candidateCreated = $true
    Assert-WorktreeIsCleanDetached -CandidateCommit $candidateCommit

    Write-Host "EXACT_CANDIDATE_READY commit=$candidateCommit tree=$tree path=$candidate staged_paths=$($stagedPaths.Count)"
}
catch {
    if ($candidateCreated -and (Test-Path -LiteralPath $candidate)) {
        & $git -C $repository worktree remove $candidate | Out-Null
    }
    throw
}
