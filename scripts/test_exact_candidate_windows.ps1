Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$git = Get-HenkaGitPath
$helper = Join-Path $repoRoot "scripts\materialize_exact_candidate_windows.ps1"
$fixtureParent = Join-Path $repoRoot "build\test_tmp"
$fixtureRoot = Join-Path $fixtureParent ("exact-candidate-fixture-" + [Guid]::NewGuid().ToString("N"))
$candidatePath = Join-Path $fixtureParent ("exact-candidate-checkout-" + [Guid]::NewGuid().ToString("N"))
$invalidCandidatePath = Join-Path $fixtureParent ("exact-candidate-invalid-" + [Guid]::NewGuid().ToString("N"))

function Invoke-FixtureGit {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $output = @(& $git -C $fixtureRoot @Arguments)
    if ($LASTEXITCODE -ne 0) {
        throw "Fixture Git command failed: git -C $fixtureRoot $($Arguments -join ' ')"
    }
    return @($output | ForEach-Object { [string]$_ })
}

function Assert-Condition {
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

function Invoke-ExactCandidate {
    param(
        [Parameter(Mandatory = $true)]
        [string]$TargetPath,

        [switch]$Remove,

        [string[]]$RequireIncludedPath,

        [string[]]$RequireExcludedPath
    )

    $arguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $helper,
        "-RepositoryRoot", $fixtureRoot,
        "-CandidatePath", $TargetPath
    )
    if ($Remove) {
        $arguments += "-Remove"
    }
    if ($null -ne $RequireIncludedPath) {
        foreach ($path in @($RequireIncludedPath)) {
            $arguments += @("-RequireIncludedPath", $path)
        }
    }
    if ($null -ne $RequireExcludedPath) {
        foreach ($path in @($RequireExcludedPath)) {
            $arguments += @("-RequireExcludedPath", $path)
        }
    }

    $childOutput = @(& powershell.exe @arguments 2>&1)
    $childOutput | ForEach-Object { Write-Host ([string]$_) }
    return [int]$LASTEXITCODE
}

try {
    Assert-Condition (Test-Path -LiteralPath $helper -PathType Leaf) `
        "The exact candidate helper is missing: $helper"
    New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $fixtureParent -Force | Out-Null

    Invoke-FixtureGit @("init", "--quiet") | Out-Null
    Invoke-FixtureGit @("config", "user.name", "Henka exact-candidate regression") | Out-Null
    Invoke-FixtureGit @("config", "user.email", "henka-exact-candidate@example.invalid") | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $fixtureRoot "shared.txt"), "baseline")
    Invoke-FixtureGit @("add", "shared.txt") | Out-Null
    Invoke-FixtureGit @("commit", "--quiet", "-m", "baseline") | Out-Null

    [System.IO.File]::WriteAllText((Join-Path $fixtureRoot "shared.txt"), "staged")
    Invoke-FixtureGit @("add", "shared.txt") | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $fixtureRoot "shared.txt"), "unstaged")
    [System.IO.File]::WriteAllText((Join-Path $fixtureRoot "untracked.txt"), "must not enter candidate")

    $createExit = Invoke-ExactCandidate `
        -TargetPath $candidatePath `
        -RequireIncludedPath "shared.txt" `
        -RequireExcludedPath "untracked.txt"
    Assert-Condition ($createExit -eq 0) `
        "Exact candidate creation failed with exit code $createExit."
    Assert-Condition (Test-Path -LiteralPath $candidatePath -PathType Container) `
        "Exact candidate checkout was not created."
    Assert-Condition (([System.IO.File]::ReadAllText((Join-Path $candidatePath "shared.txt"))) -eq "staged") `
        "Candidate used the unstaged file content instead of the staged blob."
    Assert-Condition (-not (Test-Path -LiteralPath (Join-Path $candidatePath "untracked.txt"))) `
        "Candidate unexpectedly included an untracked path."

    $candidateStatus = @(& $git -C $candidatePath status --short --branch)
    Assert-Condition (($candidateStatus -join "`n") -match "^## HEAD \(no branch\)$") `
        "Exact candidate checkout is not a clean detached worktree: $($candidateStatus -join ' | ')"

    $removeExit = Invoke-ExactCandidate -TargetPath $candidatePath -Remove
    Assert-Condition ($removeExit -eq 0) `
        "Exact candidate removal failed with exit code $removeExit."
    Assert-Condition (-not (Test-Path -LiteralPath $candidatePath)) `
        "Exact candidate checkout still exists after removal."

    $invalidExit = Invoke-ExactCandidate `
        -TargetPath $invalidCandidatePath `
        -RequireIncludedPath "missing.txt"
    Assert-Condition ($invalidExit -ne 0) `
        "Exact candidate creation unexpectedly succeeded with a missing required path."
    Assert-Condition (-not (Test-Path -LiteralPath $invalidCandidatePath)) `
        "Invalid exact candidate request created a checkout before failing closed."

Write-Host "[pass] Git-object exact candidate materialization uses staged content, excludes untracked content, verifies detached cleanliness, and fails closed for unmet path contracts."
}
finally {
    if (Test-Path -LiteralPath $candidatePath) {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $helper `
            -RepositoryRoot $fixtureRoot -CandidatePath $candidatePath -Remove
    }
    if (Test-Path -LiteralPath $invalidCandidatePath) {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $helper `
            -RepositoryRoot $fixtureRoot -CandidatePath $invalidCandidatePath -Remove
    }
    if (Test-Path -LiteralPath $fixtureRoot) {
        $cleanupEntries = @(Get-ChildItem -LiteralPath $fixtureRoot -Recurse -Force |
            Sort-Object -Property @{ Expression = { $_.FullName.Length }; Descending = $true })
        foreach ($cleanupEntry in $cleanupEntries) {
            if (($cleanupEntry.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Refusing to clean a reparse point in the exact-candidate regression fixture: $($cleanupEntry.FullName)"
            }
            $cleanupEntry.Attributes = [System.IO.FileAttributes]::Normal
        }
        Get-ChildItem -LiteralPath $fixtureRoot -Recurse -Force | ForEach-Object {
            $_.Attributes = [System.IO.FileAttributes]::Normal
        }
        (Get-Item -LiteralPath $fixtureRoot -Force).Attributes = [System.IO.FileAttributes]::Normal
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction Stop
    }
}
exit 0
