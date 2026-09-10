[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
$helper = Join-Path $PSScriptRoot "materialize_exact_candidate_windows.ps1"
$fixtureRoot = Join-Path $repoRoot ("build\test_tmp\ecl-" + [Guid]::NewGuid().ToString("N"))
$fixtureBuildRoot = Join-Path $fixtureRoot "build\test_tmp"
$registeredCandidate = Join-Path $fixtureBuildRoot "registered-candidate"
$unregisteredCandidate = Join-Path $fixtureBuildRoot "unregistered-candidate"
$sibling = Join-Path $fixtureBuildRoot "unrelated-sibling"
$unproven = Join-Path $fixtureBuildRoot "unproven-target"
$reparse = Join-Path $fixtureBuildRoot "reparse-target"
$reparseTarget = Join-Path $fixtureBuildRoot "reparse-target-content"
$outsideCandidate = Join-Path $fixtureRoot "outside-candidate"

function Invoke-FixtureGit {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    $output = @(& (Get-HenkaGitPath) -C $fixtureRoot @Arguments 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "Fixture Git command failed: git -C $fixtureRoot $($Arguments -join ' ')`n$($output -join "`n")"
    }
    return @($output | ForEach-Object { [string]$_ })
}

function Invoke-Helper {
    param(
        [Parameter(Mandatory = $true)][string]$CandidatePath,
        [switch]$Remove
    )
    $arguments = @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $helper,
        "-RepositoryRoot", $fixtureRoot, "-CandidatePath", $CandidatePath)
    if ($Remove) { $arguments += "-Remove" }
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(& powershell.exe @arguments 2>&1)
        return [pscustomobject]@{
            ExitCode = [int]$LASTEXITCODE
            Output = (($output | ForEach-Object { [string]$_ }) -join "`n")
        }
    }
    finally { $ErrorActionPreference = $previous }
}

function Assert-Condition {
    param([Parameter(Mandatory = $true)][bool]$Condition, [Parameter(Mandatory = $true)][string]$Message)
    if (-not $Condition) { throw $Message }
}

function Get-RegisteredWorktreePaths {
    return @(
        (& (Get-HenkaGitPath) -C $fixtureRoot worktree list --porcelain 2>$null) |
            Where-Object { ([string]$_).StartsWith("worktree ") } |
            ForEach-Object { [System.IO.Path]::GetFullPath(([string]$_).Substring(9)) })
}

function Remove-FixtureWorktreeAdminRecord {
    param([Parameter(Mandatory = $true)][string]$CandidatePath)
    $git = Get-HenkaGitPath
    $common = [string](& $git -C $fixtureRoot rev-parse --git-common-dir 2>$null)
    if ($LASTEXITCODE -ne 0) { throw "Could not resolve fixture Git common directory." }
    $common = [System.IO.Path]::GetFullPath((Join-Path $fixtureRoot $common.Trim()))
    $adminRoot = Join-Path $common "worktrees"
    foreach ($admin in @(Get-ChildItem -LiteralPath $adminRoot -Directory -Force -ErrorAction Stop)) {
        $gitdirFile = Join-Path $admin.FullName "gitdir"
        if (-not (Test-Path -LiteralPath $gitdirFile -PathType Leaf)) { continue }
        $gitdir = [System.IO.File]::ReadAllText($gitdirFile).Trim()
        if ($gitdir.EndsWith("/.git", [System.StringComparison]::OrdinalIgnoreCase)) {
            $gitdir = $gitdir.Substring(0, $gitdir.Length - 5)
        }
        if ([System.StringComparer]::OrdinalIgnoreCase.Equals(
                [System.IO.Path]::GetFullPath($gitdir).TrimEnd("\"),
                [System.IO.Path]::GetFullPath($CandidatePath).TrimEnd("\"))) {
            Remove-Item -LiteralPath $admin.FullName -Recurse -Force -ErrorAction Stop
            return
        }
    }
    throw "Fixture worktree admin record was not found for $CandidatePath."
}

function New-LongGeneratedFile {
    param([Parameter(Mandatory = $true)][string]$CandidatePath)
    $deep = $CandidatePath
    for ($index = 0; $index -lt 9; $index++) {
        $deep = Join-Path $deep ("generated-segment-" + ("x" * 24))
    }
    $longFile = Join-Path $deep "generated-output.bin"
    $longPrefix = "\\?\"
    [System.IO.Directory]::CreateDirectory($longPrefix + $deep) | Out-Null
    [System.IO.File]::WriteAllText($longPrefix + $longFile, "bounded generated output")
    return $longFile
}

try {
    New-Item -ItemType Directory -Path $fixtureBuildRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $sibling -Force | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $sibling "must-survive.txt"), "sibling")
    New-Item -ItemType Directory -Path $outsideCandidate -Force | Out-Null

    Invoke-FixtureGit @("init", "--quiet") | Out-Null
    Invoke-FixtureGit @("config", "user.name", "Henka lifecycle regression") | Out-Null
    Invoke-FixtureGit @("config", "user.email", "henka-lifecycle@example.invalid") | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $fixtureRoot "source.txt"), "bounded source")
    Invoke-FixtureGit @("add", "source.txt") | Out-Null
    Invoke-FixtureGit @("commit", "--quiet", "-m", "baseline") | Out-Null

    $registered = Invoke-Helper -CandidatePath $registeredCandidate
    Assert-Condition ($registered.ExitCode -eq 0) "Registered candidate creation failed: $($registered.Output)"
    Assert-Condition ((Get-RegisteredWorktreePaths) -contains ([System.IO.Path]::GetFullPath($registeredCandidate)) ) `
        "The lifecycle fixture candidate was not registered as a Git worktree."
    $registeredRemoval = Invoke-Helper -CandidatePath $registeredCandidate -Remove
    Assert-Condition ($registeredRemoval.ExitCode -eq 0) "Registered candidate removal failed: $($registeredRemoval.Output)"
    Assert-Condition (-not (Test-Path -LiteralPath $registeredCandidate)) "Registered candidate still exists after removal."
    Assert-Condition ((Get-RegisteredWorktreePaths) -notcontains ([System.IO.Path]::GetFullPath($registeredCandidate))) `
        "Registered candidate remained in the Git worktree registry."

    $unregistered = Invoke-Helper -CandidatePath $unregisteredCandidate
    Assert-Condition ($unregistered.ExitCode -eq 0) "Unregistered-remnant candidate creation failed: $($unregistered.Output)"
    $longFile = New-LongGeneratedFile -CandidatePath $unregisteredCandidate
    Assert-Condition ($longFile.Length -gt 260) "The lifecycle regression did not create a long generated path."
    Remove-FixtureWorktreeAdminRecord -CandidatePath $unregisteredCandidate
    Assert-Condition ((Get-RegisteredWorktreePaths) -notcontains ([System.IO.Path]::GetFullPath($unregisteredCandidate))) `
        "The fixture failed to create an already-unregistered candidate remnant."
    $unregisteredRemoval = Invoke-Helper -CandidatePath $unregisteredCandidate -Remove
    Assert-Condition ($unregisteredRemoval.ExitCode -eq 0) `
        "Long-path unregistered candidate removal failed: $($unregisteredRemoval.Output)"
    Assert-Condition (-not (Test-Path -LiteralPath $unregisteredCandidate)) "Long-path candidate still exists after removal."
    Assert-Condition (Test-Path -LiteralPath (Join-Path $sibling "must-survive.txt") -PathType Leaf) `
        "Removing the candidate damaged unrelated sibling content."
    Assert-Condition ((Get-RegisteredWorktreePaths) -notcontains ([System.IO.Path]::GetFullPath($unregisteredCandidate))) `
        "Removed unregistered candidate unexpectedly remained registered."

    New-Item -ItemType Directory -Path $unproven -Force | Out-Null
    $unprovenRemoval = Invoke-Helper -CandidatePath $unproven -Remove
    Assert-Condition ($unprovenRemoval.ExitCode -ne 0 -and $unprovenRemoval.Output -match "provenance") `
        "An unproven target was not rejected by the lifecycle remover: $($unprovenRemoval.Output)"

    New-Item -ItemType Directory -Path $reparseTarget -Force | Out-Null
    $junctionCreated = $false
    try {
        New-Item -ItemType Junction -Path $reparse -Target $reparseTarget -ErrorAction Stop | Out-Null
        $junctionCreated = $true
    }
    catch { Write-Host "[skip] Reparse-point creation is unavailable in this environment." }
    if ($junctionCreated) {
        $reparseRemoval = Invoke-Helper -CandidatePath $reparse -Remove
        Assert-Condition ($reparseRemoval.ExitCode -ne 0 -and $reparseRemoval.Output -match "reparse") `
            "A reparse-point target was not rejected: $($reparseRemoval.Output)"
    }
    $escapeRemoval = Invoke-Helper -CandidatePath (Join-Path $fixtureBuildRoot "..\..\outside-candidate") -Remove
    Assert-Condition ($escapeRemoval.ExitCode -ne 0 -and $escapeRemoval.Output -match "approved|root") `
        "A path outside the approved generated root was not rejected: $($escapeRemoval.Output)"

    Write-Output "Exact candidate lifecycle regression passed."
}
finally {
    $previous = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        foreach ($candidate in @($registeredCandidate, $unregisteredCandidate)) {
            if (Test-Path -LiteralPath $candidate -PathType Container) {
                $result = Invoke-Helper -CandidatePath $candidate -Remove
                if ($result.ExitCode -ne 0) {
                    Remove-Item -LiteralPath $candidate -Recurse -Force -ErrorAction SilentlyContinue
                }
            }
        }
        if (Test-Path -LiteralPath $reparse) {
            $item = Get-Item -LiteralPath $reparse -Force -ErrorAction SilentlyContinue
            if ($null -ne $item -and ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                [System.IO.Directory]::Delete($reparse, $false)
            }
        }
        if (Test-Path -LiteralPath $fixtureRoot) {
            Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    finally { $ErrorActionPreference = $previous }
}
exit 0
