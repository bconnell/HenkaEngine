[CmdletBinding()]
param(
    [ValidateSet("Report", "DryRun", "Cleanup")]
    [string]$Mode = "Report",

    [string]$RootPath = "",

    [string]$CandidatePath = "",

    [switch]$ConfirmNoActiveProcess
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$git = Get-HenkaGitPath
$markerName = ".henka-generated.json"
$allowedRetentionClasses = @("SCRATCH", "PROOF_CONSUMED", "SUPERSEDED", "REBUILDABLE")

function Get-AbsolutePath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location).Path $Path))
}

function Get-ApprovedRoots {
    return @(
        [System.IO.Path]::GetFullPath((Join-Path $repoRoot "build")).TrimEnd("\"),
        [System.IO.Path]::GetFullPath((Join-Path $repoRoot "out")).TrimEnd("\")
    )
}

function Test-PathWithin {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root
    )

    $normalizedPath = $Path.TrimEnd("\")
    $normalizedRoot = $Root.TrimEnd("\")
    return $normalizedPath.StartsWith(
        $normalizedRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)
}

function Test-PathAtOrWithin {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root
    )

    $normalizedPath = $Path.TrimEnd("\")
    $normalizedRoot = $Root.TrimEnd("\")
    return $normalizedPath.Equals($normalizedRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
        (Test-PathWithin -Path $normalizedPath -Root $normalizedRoot)
}

function Assert-NoReparseChain {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd("\")
    $pathFull = [System.IO.Path]::GetFullPath($Path).TrimEnd("\")
    $relative = $pathFull.Substring($rootFull.Length).TrimStart("\")
    $current = $rootFull
    if (-not (Test-Path -LiteralPath $current -PathType Container)) {
        throw "Approved generated root does not exist: $current"
    }
    $rootItem = Get-Item -LiteralPath $current -Force
    if (($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Approved generated root is a reparse point: $current"
    }
    foreach ($part in @($relative -split "\\" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })) {
        $current = Join-Path $current $part
        if (-not (Test-Path -LiteralPath $current)) {
            throw "Generated artifact path does not exist: $current"
        }
        $item = Get-Item -LiteralPath $current -Force
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing to operate through a reparse point: $current"
        }
    }
}

function Get-HenkaTreeStats {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [int64]0
    $files = [int64]0
    $reparse = [int64]0
    $stack = New-Object System.Collections.Generic.Stack[string]
    $stack.Push($Path)
    while ($stack.Count -gt 0) {
        $current = $stack.Pop()
        foreach ($item in @(Get-ChildItem -LiteralPath $current -Force -ErrorAction Stop)) {
            if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                $reparse++
                continue
            }
            if ($item.PSIsContainer) {
                $stack.Push($item.FullName)
            }
            else {
                $bytes += [int64]$item.Length
                $files++
            }
        }
    }
    return [pscustomobject]@{ Bytes = $bytes; Files = $files; ReparseEntries = $reparse }
}

function Get-ApprovedRootForPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    foreach ($root in @(Get-ApprovedRoots)) {
        if (Test-PathAtOrWithin -Path $Path -Root $root) {
            return $root
        }
    }
    throw "Path is outside an approved generated root (build or out): $Path"
}

function Get-RepositoryRelativePath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $repoFull = [System.IO.Path]::GetFullPath($repoRoot).TrimEnd("\")
    $pathFull = [System.IO.Path]::GetFullPath($Path).TrimEnd("\")
    if (-not (Test-PathWithin -Path $pathFull -Root $repoFull)) {
        throw "Generated artifact is not inside the repository: $Path"
    }
    return $pathFull.Substring($repoFull.Length).TrimStart("\").Replace("\", "/")
}

function Assert-ContainsNoTrackedFiles {
    param([Parameter(Mandatory = $true)][string]$Path)

    $relative = Get-RepositoryRelativePath -Path $Path
    $tracked = @(& $git -C $repoRoot ls-files -- $relative 2>$null)
    if ($LASTEXITCODE -ne 0) {
        throw "Could not inspect tracked-file state for generated artifact: $Path"
    }
    if ($tracked.Count -ne 0) {
        throw "Refusing to operate on a generated path containing tracked files: $Path"
    }
}

function Read-GeneratedMarker {
    param([Parameter(Mandatory = $true)][string]$Path)

    $markerPath = Join-Path $Path $markerName
    if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) {
        throw "Generated artifact is unmarked; refusing cleanup: $Path"
    }
    try {
        $marker = Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json
    }
    catch {
        throw "Generated artifact marker is not valid JSON: $markerPath"
    }
    $required = @(
        "schema_version", "owner", "purpose", "source_sha", "configuration",
        "created_utc", "retention_class", "cleanup_owner", "cleanup_condition",
        "active", "cleanup_eligible")
    foreach ($property in $required) {
        if ($marker.PSObject.Properties.Name -notcontains $property) {
            throw "Generated artifact marker is missing '$property': $markerPath"
        }
    }
    if ([int]$marker.schema_version -ne 1) {
        throw "Generated artifact marker schema is unsupported: $markerPath"
    }
    foreach ($valueName in @("owner", "purpose", "source_sha", "configuration", "created_utc", "retention_class", "cleanup_owner", "cleanup_condition")) {
        if ([string]::IsNullOrWhiteSpace([string]$marker.$valueName)) {
            throw "Generated artifact marker field '$valueName' is empty: $markerPath"
        }
    }
    if ([string]$marker.source_sha -notmatch "^(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})$") {
        throw "Generated artifact marker source_sha is not a Git commit or SHA-256: $markerPath"
    }
    try { [DateTime]::Parse([string]$marker.created_utc).ToUniversalTime() | Out-Null }
    catch { throw "Generated artifact marker created_utc is invalid: $markerPath" }
    return $marker
}

function Assert-GeneratedMarker {
    param([Parameter(Mandatory = $true)][string]$Path)

    $marker = Read-GeneratedMarker -Path $Path
    if ([bool]$marker.active) {
        throw "Generated artifact is marked active; refusing cleanup: $Path"
    }
    $retention = ([string]$marker.retention_class).ToUpperInvariant()
    if ($allowedRetentionClasses -notcontains $retention) {
        throw "Generated artifact retention class '$retention' is not cleanup-eligible: $(Join-Path $Path $markerName)"
    }
    if (-not [bool]$marker.cleanup_eligible) {
        throw "Generated artifact is not cleanup-eligible: $Path"
    }
    if (([string]$marker.cleanup_condition).ToLowerInvariant() -notin @("proof_consumed", "superseded", "rebuildable")) {
        throw "Generated artifact cleanup condition is not an approved retirement condition: $(Join-Path $Path $markerName)"
    }
    return $marker
}

function Assert-CandidateSafe {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "Generated artifact candidate is not a directory: $Path"
    }
    $root = Get-ApprovedRootForPath -Path $Path
    if ([System.IO.Path]::GetFullPath($Path).TrimEnd("\") -eq $root.TrimEnd("\")) {
        throw "Refusing to operate on an approved generated root itself; provide one exact child candidate: $Path"
    }
    Assert-NoReparseChain -Root $root -Path $Path
    Assert-ContainsNoTrackedFiles -Path $Path
    $marker = Assert-GeneratedMarker -Path $Path
    $stats = Get-HenkaTreeStats -Path $Path
    if ($stats.ReparseEntries -ne 0) {
        throw "Generated artifact contains reparse entries; refusing cleanup: $Path"
    }
    return [pscustomobject]@{ Root = $root; Marker = $marker; Stats = $stats }
}

function Write-Report {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        Write-Output ("GENERATED_ROOT|exists=False|path=" + $Path)
        return
    }
    $root = Get-ApprovedRootForPath -Path $Path
    Assert-NoReparseChain -Root $root -Path $Path
    $stats = Get-HenkaTreeStats -Path $Path
    Write-Output ("GENERATED_ROOT|exists=True|bytes={0}|gib={1}|files={2}|reparse_entries={3}|path={4}" -f $stats.Bytes, [math]::Round($stats.Bytes / 1GB, 3), $stats.Files, $stats.ReparseEntries, $Path)
    foreach ($child in @(Get-ChildItem -LiteralPath $Path -Directory -Force | Sort-Object Name)) {
        $markerPath = Join-Path $child.FullName $markerName
        if (Test-Path -LiteralPath $markerPath -PathType Leaf) {
            try {
                $marker = Read-GeneratedMarker -Path $child.FullName
                Write-Output ("GENERATED_CHILD|class={0}|active={1}|cleanup_eligible={2}|path={3}" -f $marker.retention_class, $marker.active, $marker.cleanup_eligible, $child.FullName)
            }
            catch {
                Write-Output ("GENERATED_CHILD|class=INVALID_MARKER|path={0}|error={1}" -f $child.FullName, $_.Exception.Message)
            }
        }
        else {
            Write-Output ("GENERATED_CHILD|class=UNMARKED|path=" + $child.FullName)
        }
    }
}

if ($Mode -eq "Report") {
    if (-not [string]::IsNullOrWhiteSpace($CandidatePath)) {
        throw "Report mode accepts RootPath, not CandidatePath."
    }
    if ([string]::IsNullOrWhiteSpace($RootPath)) {
        foreach ($root in @(Get-ApprovedRoots)) { Write-Report -Path $root }
    }
    else {
        Write-Report -Path (Get-AbsolutePath -Path $RootPath)
    }
    exit 0
}

if ([string]::IsNullOrWhiteSpace($CandidatePath)) {
    throw "$Mode mode requires one exact CandidatePath."
}
if (-not $ConfirmNoActiveProcess.IsPresent -and $Mode -eq "Cleanup") {
    throw "Cleanup requires -ConfirmNoActiveProcess after the caller verifies that no active process uses the exact candidate."
}

$candidate = Get-AbsolutePath -Path $CandidatePath
$candidateInfo = Assert-CandidateSafe -Path $candidate
if ($Mode -eq "DryRun") {
    Write-Output ("CLEANUP_PLAN|cleanup_eligible=True|retention_class={0}|bytes={1}|files={2}|path={3}" -f $candidateInfo.Marker.retention_class, $candidateInfo.Stats.Bytes, $candidateInfo.Stats.Files, $candidate)
    exit 0
}

Write-Output ("CLEANUP_BEGIN|retention_class={0}|bytes={1}|path={2}" -f $candidateInfo.Marker.retention_class, $candidateInfo.Stats.Bytes, $candidate)
Remove-Item -LiteralPath $candidate -Recurse -Force -ErrorAction Stop
if (Test-Path -LiteralPath $candidate) {
    throw "Exact generated artifact cleanup did not remove the candidate: $candidate"
}
Write-Output ("CLEANUP_COMPLETE|reclaimed_bytes={0}|path={1}" -f $candidateInfo.Stats.Bytes, $candidate)
