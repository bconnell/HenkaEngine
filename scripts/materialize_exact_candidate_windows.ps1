[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,

    [Parameter(Mandatory = $true)]
    [string]$CandidatePath,

    [switch]$Remove,

    [string]$LegacyCandidateCommit = "",

    [string[]]$RequireIncludedPath = @(),

    [string[]]$RequireExcludedPath = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$git = Get-HenkaGitPath
$repository = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$candidate = [System.IO.Path]::GetFullPath($CandidatePath)

# A Windows PowerShell 5.1 process cannot bind the same array parameter more
# than once. The caller may therefore serialize multiple repository-relative
# paths as one pipe-delimited argument. The pipe character is not valid in a
# Windows filename, while direct in-process callers may continue passing a
# normal [string[]].
function Expand-DelimitedRepositoryPaths {
    param(
        [AllowEmptyCollection()]
        [Parameter(Mandatory = $true)]
        [string[]]$Paths
    )

    $expanded = @()
    foreach ($path in @($Paths)) {
        $expanded += @($path -split "\|")
    }
    return $expanded
}

$RequireIncludedPath = @(Expand-DelimitedRepositoryPaths -Paths $RequireIncludedPath)
$RequireExcludedPath = @(Expand-DelimitedRepositoryPaths -Paths $RequireExcludedPath)

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

function Assert-CandidatePathIsApproved {
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd("\")
    $approved = @(
        [System.IO.Path]::GetFullPath((Join-Path $repository "build")).TrimEnd("\"),
        [System.IO.Path]::GetFullPath((Join-Path $repository "out")).TrimEnd("\"))
    foreach ($root in $approved) {
        if ($fullPath -eq $root) {
            throw "Candidate path must be one exact generated child, not an approved root: $Path"
        }
        if ($fullPath.StartsWith($root + [System.IO.Path]::DirectorySeparatorChar,
                [System.StringComparison]::OrdinalIgnoreCase)) {
            return
        }
    }
    throw "Candidate path is outside the approved generated roots (build or out): $Path"
}

function ConvertTo-HenkaLongPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    if ($fullPath.StartsWith("\\?\", [System.StringComparison]::Ordinal)) {
        return $fullPath
    }
    if ($fullPath.StartsWith("\\", [System.StringComparison]::Ordinal)) {
        return "\\?\UNC\" + $fullPath.TrimStart("\").Replace("/", "\")
    }
    return "\\?\" + $fullPath.Replace("/", "\")
}

function Get-ExactCandidateMarkerPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $digest = $sha256.ComputeHash([System.Text.Encoding]::UTF8.GetBytes(
            ([System.IO.Path]::GetFullPath($Path)).ToLowerInvariant()))
    }
    finally { $sha256.Dispose() }
    $suffix = -join ($digest | ForEach-Object { $_.ToString("x2") })
    return Join-Path (Split-Path -Parent $Path) (".henka-exact-candidate-" + $suffix + ".json")
}

function Get-DirectoryEntriesLongPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    return @([System.IO.Directory]::EnumerateFileSystemEntries((ConvertTo-HenkaLongPath -Path $Path)))
}

function Assert-NoReparseDescendants {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stack = New-Object System.Collections.Generic.Stack[string]
    $stack.Push([System.IO.Path]::GetFullPath($Path))
    while ($stack.Count -gt 0) {
        $current = $stack.Pop()
        foreach ($entry in @(Get-DirectoryEntriesLongPath -Path $current)) {
            $attributes = [System.IO.File]::GetAttributes((ConvertTo-HenkaLongPath -Path $entry))
            if (($attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Refusing to operate through a reparse point: $entry"
            }
            if (($attributes -band [System.IO.FileAttributes]::Directory) -ne 0) {
                $stack.Push($entry)
            }
        }
    }
}

function Clear-ReadonlyAttributes {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stack = New-Object System.Collections.Generic.Stack[string]
    $stack.Push([System.IO.Path]::GetFullPath($Path))
    while ($stack.Count -gt 0) {
        $current = $stack.Pop()
        $longCurrent = ConvertTo-HenkaLongPath -Path $current
        $attributes = [System.IO.File]::GetAttributes($longCurrent)
        if (($attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing to change attributes on a reparse point: $current"
        }
        [System.IO.File]::SetAttributes($longCurrent, $attributes -band (-bnot [System.IO.FileAttributes]::ReadOnly))
        foreach ($entry in @(Get-DirectoryEntriesLongPath -Path $current)) {
            $entryAttributes = [System.IO.File]::GetAttributes((ConvertTo-HenkaLongPath -Path $entry))
            if (($entryAttributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Refusing to change attributes on a reparse point: $entry"
            }
            [System.IO.File]::SetAttributes(
                (ConvertTo-HenkaLongPath -Path $entry),
                $entryAttributes -band (-bnot [System.IO.FileAttributes]::ReadOnly))
            if (($entryAttributes -band [System.IO.FileAttributes]::Directory) -ne 0) {
                $stack.Push($entry)
            }
        }
    }
}

function Remove-HenkaCandidateDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)

    Assert-NoReparseDescendants -Path $Path
    Clear-ReadonlyAttributes -Path $Path
    [System.IO.Directory]::Delete((ConvertTo-HenkaLongPath -Path $Path), $true)
    if ([System.IO.Directory]::Exists((ConvertTo-HenkaLongPath -Path $Path))) {
        throw "Exact candidate filesystem removal did not complete: $Path"
    }
}

function Find-WorktreeAdminRecord {
    param([Parameter(Mandatory = $true)][string]$CandidatePath)

    $common = [string](& $git -C $repository rev-parse --git-common-dir 2>$null)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($common)) {
        throw "Could not resolve the repository Git common directory."
    }
    $commonPath = if ([System.IO.Path]::IsPathRooted($common)) {
        [System.IO.Path]::GetFullPath($common)
    }
    else {
        [System.IO.Path]::GetFullPath((Join-Path $repository $common))
    }
    $adminRoot = Join-Path $commonPath "worktrees"
    if (-not (Test-Path -LiteralPath $adminRoot -PathType Container)) {
        return $null
    }
    foreach ($admin in @(Get-ChildItem -LiteralPath $adminRoot -Directory -Force)) {
        $gitdirFile = Join-Path $admin.FullName "gitdir"
        if (-not (Test-Path -LiteralPath $gitdirFile -PathType Leaf)) { continue }
        $gitdir = [System.IO.File]::ReadAllText($gitdirFile).Trim()
        if ($gitdir.EndsWith("/.git", [System.StringComparison]::OrdinalIgnoreCase) -or
            $gitdir.EndsWith("\\.git", [System.StringComparison]::OrdinalIgnoreCase)) {
            $gitdir = $gitdir.Substring(0, $gitdir.Length - 5)
        }
        if ([System.StringComparer]::OrdinalIgnoreCase.Equals(
                [System.IO.Path]::GetFullPath($gitdir).TrimEnd("\"),
                [System.IO.Path]::GetFullPath($CandidatePath).TrimEnd("\"))) {
            return $admin
        }
    }
    return $null
}

function Test-WorktreeRegistered {
    param([Parameter(Mandatory = $true)][string]$CandidatePath)

    $expected = [System.IO.Path]::GetFullPath($CandidatePath).TrimEnd("\")
    foreach ($record in @(Invoke-RepositoryGit @("worktree", "list", "--porcelain"))) {
        if ($record.StartsWith("worktree ")) {
            $registeredPath = [System.IO.Path]::GetFullPath($record.Substring(9)).TrimEnd("\")
            if ([System.StringComparer]::OrdinalIgnoreCase.Equals($registeredPath, $expected)) {
                return $true
            }
        }
    }
    return $false
}

function Assert-ExactCandidateMarker {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$MarkerPath
    )

    if (-not (Test-Path -LiteralPath $MarkerPath -PathType Leaf)) {
        throw "Unregistered exact candidate has no repository-owned provenance marker: $Path"
    }
    try { $marker = Get-Content -LiteralPath $MarkerPath -Raw | ConvertFrom-Json }
    catch { throw "Exact candidate provenance marker is not valid JSON: $MarkerPath" }
    foreach ($property in @("schema_version", "repository_root", "candidate_path", "candidate_commit", "tree")) {
        if ($marker.PSObject.Properties.Name -notcontains $property) {
            throw "Exact candidate provenance marker is missing '$property': $MarkerPath"
        }
    }
    if ([int]$marker.schema_version -ne 1 -or
        -not [System.StringComparer]::OrdinalIgnoreCase.Equals(
            [System.IO.Path]::GetFullPath([string]$marker.repository_root).TrimEnd("\"),
            $repository.TrimEnd("\")) -or
        -not [System.StringComparer]::OrdinalIgnoreCase.Equals(
            [System.IO.Path]::GetFullPath([string]$marker.candidate_path).TrimEnd("\"),
            [System.IO.Path]::GetFullPath($Path).TrimEnd("\")) -or
        [string]$marker.candidate_commit -notmatch "^[0-9a-fA-F]{40,64}$" -or
        [string]$marker.tree -notmatch "^[0-9a-fA-F]{40,64}$") {
        throw "Exact candidate provenance marker does not match the requested repository candidate: $MarkerPath"
    }
    return $marker
}

function Assert-LegacyCandidateMatchesCommit {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Commit
    )

    if ($Commit -notmatch "^[0-9a-fA-F]{40,64}$") {
        throw "Legacy candidate retirement requires an exact commit ID."
    }
    $type = ([string](& $git -C $repository cat-file -t $Commit 2>$null)).Trim()
    if ($LASTEXITCODE -ne 0 -or $type -ne "commit") {
        throw "Legacy candidate commit is not present in the repository object database: $Commit"
    }
    $relative = [System.IO.Path]::GetFullPath($Path).Substring($repository.Length).TrimStart("\").Replace("\", "/")
    if (-not $relative.StartsWith("build/", [System.StringComparison]::OrdinalIgnoreCase) -and
        -not $relative.StartsWith("out/", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Legacy candidate retirement is confined to build or out: $Path"
    }
    $treePaths = @(& $git -C $repository ls-tree -r --name-only $Commit 2>$null)
    if ($LASTEXITCODE -ne 0 -or $treePaths.Count -eq 0) {
        throw "Could not inspect the legacy candidate source tree: $Commit"
    }
    $missingCount = 0
    $comparedCount = 0
    foreach ($treePath in $treePaths) {
        $candidateFile = Join-Path $Path ([string]$treePath -replace "/", "\")
        if (-not (Test-Path -LiteralPath $candidateFile -PathType Leaf)) {
            $missingCount++
            continue
        }
        $expectedBlob = ([string](& $git -C $repository rev-parse "$Commit`:$treePath" 2>$null)).Trim()
        $actualBlob = ([string](& $git -C $repository hash-object -- $candidateFile 2>$null)).Trim()
        if ($LASTEXITCODE -ne 0 -or $expectedBlob -ne $actualBlob) {
            throw "Legacy candidate content differs from commit $Commit at '$treePath'."
        }
        $comparedCount++
    }
    if ($comparedCount -eq 0) {
        throw "Legacy candidate contains no remaining committed files to establish provenance: $Path"
    }
    if ($missingCount -gt 0) {
        Write-Host "[info] Retiring a partially deleted legacy candidate remnant: missing_committed_files=$missingCount; compared_committed_files=$comparedCount"
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
    Assert-CandidatePathIsApproved -Path $candidate
    if (-not (Test-Path -LiteralPath $candidate -PathType Container)) {
        throw "Candidate worktree does not exist: $candidate"
    }
    Assert-DirectoryIsSafe -Path $candidate -Label "Candidate worktree"
    $markerPath = Get-ExactCandidateMarkerPath -Path $candidate
    $adminRecord = Find-WorktreeAdminRecord -CandidatePath $candidate
    $registered = Test-WorktreeRegistered -CandidatePath $candidate
    if ($registered) {
        $marker = if (Test-Path -LiteralPath $markerPath -PathType Leaf) {
            Assert-ExactCandidateMarker -Path $candidate -MarkerPath $markerPath
        }
        else { $null }
        $trackedStatus = @(& $git -C $candidate status --porcelain=v1 --untracked-files=no 2>&1)
        if ($LASTEXITCODE -ne 0 -or $trackedStatus.Count -ne 0) {
            throw "Refusing to remove an exact candidate with modified tracked files: $candidate"
        }
        $removeOutput = @(& $git -C $repository worktree remove $candidate 2>&1)
        if ($LASTEXITCODE -eq 0 -and -not (Test-Path -LiteralPath $candidate)) {
            if (Test-Path -LiteralPath $markerPath -PathType Leaf) {
                [System.IO.File]::Delete((ConvertTo-HenkaLongPath -Path $markerPath))
            }
            if (Test-WorktreeRegistered -CandidatePath $candidate) {
                throw "Git reported candidate removal but the worktree registry still contains it: $candidate"
            }
            Write-Host "[pass] Removed exact candidate worktree without force: $candidate"
            return
        }
        if ($null -eq $marker) {
            $details = ($removeOutput | ForEach-Object { [string]$_ }) -join "`n"
            throw "Git refused non-force candidate removal and no fallback provenance marker exists: $details"
        }
        if ($null -eq $adminRecord) {
            throw "Registered exact candidate has no matching Git worktree admin record: $candidate"
        }
        Remove-HenkaCandidateDirectory -Path $candidate
        Remove-HenkaCandidateDirectory -Path $adminRecord.FullName
    }
    elseif (Test-Path -LiteralPath $markerPath -PathType Leaf) {
        [void](Assert-ExactCandidateMarker -Path $candidate -MarkerPath $markerPath)
        Remove-HenkaCandidateDirectory -Path $candidate
    }
    elseif (-not [string]::IsNullOrWhiteSpace($LegacyCandidateCommit)) {
        Assert-LegacyCandidateMatchesCommit -Path $candidate -Commit $LegacyCandidateCommit
        Remove-HenkaCandidateDirectory -Path $candidate
    }
    else {
        throw "Candidate path is not a registered Git worktree and has no proven provenance marker; refusing to remove it: $candidate"
    }
    if (Test-Path -LiteralPath $candidate) {
        throw "Candidate worktree still exists after safe removal: $candidate"
    }
    if (Test-WorktreeRegistered -CandidatePath $candidate) {
        throw "Candidate path remains in the Git worktree registry after removal: $candidate"
    }
    if (Test-Path -LiteralPath $markerPath -PathType Leaf) {
        [System.IO.File]::Delete((ConvertTo-HenkaLongPath -Path $markerPath))
    }
    Write-Host "[pass] Removed exact candidate tree through the repository-owned long-path cleanup: $candidate"
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

    $markerPath = Get-ExactCandidateMarkerPath -Path $candidate
    $marker = [ordered]@{
        schema_version = 1
        repository_root = $repository
        candidate_path = $candidate
        candidate_commit = $candidateCommit
        tree = $tree
        source_head = $head
        created_utc = [DateTime]::UtcNow.ToString("o")
    }
    Write-HenkaUtf8NoBom -Path $markerPath -Text (($marker | ConvertTo-Json -Depth 3) + [Environment]::NewLine)

    Write-Host "EXACT_CANDIDATE_READY commit=$candidateCommit tree=$tree path=$candidate staged_paths=$($stagedPaths.Count)"
}
catch {
    if ($candidateCreated -and (Test-Path -LiteralPath $candidate)) {
        & $git -C $repository worktree remove $candidate | Out-Null
    }
    throw
}
