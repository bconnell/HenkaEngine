Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$manager = Join-Path $PSScriptRoot "manage_generated_artifacts_windows.ps1"
$testRoot = Join-Path $repoRoot ("build\test_tmp\generated-artifact-lifecycle-regression-" + [Guid]::NewGuid().ToString("N"))
$outside = Join-Path $repoRoot ("generated-artifact-lifecycle-outside-" + [Guid]::NewGuid().ToString("N"))
$junction = Join-Path $testRoot "junction"

function Write-TestMarker {
    param(
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [string]$RetentionClass,
        [Parameter(Mandatory = $true)] [bool]$Active,
        [Parameter(Mandatory = $true)] [bool]$CleanupEligible
    )

    $marker = [ordered]@{
        schema_version = 1
        owner = "generated-artifact-lifecycle-regression"
        purpose = "bounded cleanup contract regression"
        source_sha = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        configuration = "Debug"
        created_utc = [DateTime]::UtcNow.ToString("o")
        retention_class = $RetentionClass
        cleanup_owner = "generated-artifact-lifecycle-regression"
        cleanup_condition = if ($CleanupEligible) { "proof_consumed" } else { "active_or_protected" }
        active = $Active
        cleanup_eligible = $CleanupEligible
    }
    $marker | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Path ".henka-generated.json")
}

function Invoke-Manager {
    param(
        [Parameter(Mandatory = $true)] [string[]]$Arguments
    )
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $manager @Arguments 2>&1)
        return [pscustomobject]@{
            ExitCode = $LASTEXITCODE
            Output = (($output | ForEach-Object { [string]$_ }) -join "`n")
        }
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
}

function Assert-ManagerFailure {
    param(
        [Parameter(Mandatory = $true)] [string[]]$Arguments,
        [Parameter(Mandatory = $true)] [string]$ExpectedText
    )
    $result = Invoke-Manager -Arguments $Arguments
    if ($result.ExitCode -eq 0 -or $result.Output -notmatch [regex]::Escape($ExpectedText)) {
        throw "Expected lifecycle manager failure containing '$ExpectedText'. Output: $($result.Output)"
    }
}

try {
    New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
    $eligible = Join-Path $testRoot "eligible"
    $active = Join-Path $testRoot "active"
    $unmarked = Join-Path $testRoot "unmarked"
    $commonMarkerRoot = Join-Path $testRoot "common-marker"
    New-Item -ItemType Directory -Path $eligible,$active,$unmarked,$outside -Force | Out-Null
    New-Item -ItemType Directory -Path $commonMarkerRoot -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $eligible "evidence.txt") -Value "eligible"
    Set-Content -LiteralPath (Join-Path $active "evidence.txt") -Value "active"
    Set-Content -LiteralPath (Join-Path $unmarked "evidence.txt") -Value "unmarked"
    Set-Content -LiteralPath (Join-Path $outside "evidence.txt") -Value "outside"
    $commonMarkerPath = Write-HenkaGeneratedRootMarker `
        -RepoRoot $repoRoot `
        -Path $commonMarkerRoot `
        -Purpose "common marker contract regression" `
        -RetentionClass "CACHE" `
        -Active $false `
        -CleanupEligible $false `
        -CleanupCondition "retained_for_test" `
        -Configuration "Debug"
    if (-not (Test-Path -LiteralPath $commonMarkerPath -PathType Leaf)) {
        throw "The common generated-root marker helper did not write its marker."
    }
    Write-TestMarker -Path $eligible -RetentionClass "PROOF_CONSUMED" -Active $false -CleanupEligible $true
    Write-TestMarker -Path $active -RetentionClass "ACTIVE_CANDIDATE" -Active $true -CleanupEligible $false

    $dryRun = Invoke-Manager -Arguments @("-Mode", "DryRun", "-CandidatePath", $eligible)
    if ($dryRun.ExitCode -ne 0 -or $dryRun.Output -notmatch "cleanup_eligible=True" -or $dryRun.Output -notmatch "bytes=") {
        throw "Eligible generated artifact was not accepted by dry-run. Output: $($dryRun.Output)"
    }
    if (-not (Test-Path -LiteralPath $eligible -PathType Container)) {
        throw "Dry-run altered the eligible generated artifact."
    }
    $report = Invoke-Manager -Arguments @("-Mode", "Report", "-RootPath", $testRoot)
    if ($report.ExitCode -ne 0 -or $report.Output -notmatch "class=CACHE" -or $report.Output -notmatch "class=UNMARKED") {
        throw "The lifecycle report did not expose marked and unmarked roots. Output: $($report.Output)"
    }

    Assert-ManagerFailure -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $active) -ExpectedText "active"
    Assert-ManagerFailure -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $unmarked) -ExpectedText "unmarked"
    Assert-ManagerFailure -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $outside) -ExpectedText "approved generated root"
    Assert-ManagerFailure -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", (Join-Path $repoRoot "build\test_tmp\..\..\out")) -ExpectedText "approved generated root itself"

    $cleanup = Invoke-Manager -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $eligible)
    if ($cleanup.ExitCode -ne 0 -or (Test-Path -LiteralPath $eligible)) {
        throw "Eligible generated artifact was not removed by exact cleanup. Output: $($cleanup.Output)"
    }
    if (-not (Test-Path -LiteralPath $active -PathType Container) -or
        -not (Test-Path -LiteralPath $unmarked -PathType Container) -or
        -not (Test-Path -LiteralPath $outside -PathType Container)) {
        throw "Cleanup removed a protected, unmarked, or outside artifact."
    }

    $junctionTarget = Join-Path $testRoot "junction-target"
    New-Item -ItemType Directory -Path $junctionTarget -Force | Out-Null
    $junctionCreated = $false
    try {
        New-Item -ItemType Junction -Path $junction -Target $junctionTarget -ErrorAction Stop | Out-Null
        $junctionCreated = $true
    }
    catch {
        Write-Host "[skip] Reparse-point creation is unavailable in this environment; path rejection remains covered by the manager's safe-path contract."
    }
    if ($junctionCreated) {
        Write-TestMarker -Path $junctionTarget -RetentionClass "PROOF_CONSUMED" -Active $false -CleanupEligible $true
        Assert-ManagerFailure -Arguments @("-Mode", "Cleanup", "-ConfirmNoActiveProcess", "-CandidatePath", $junction) -ExpectedText "reparse"
    }

    Write-Output "Generated artifact lifecycle regression tests passed."
}
finally {
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        if (Test-Path -LiteralPath $junction) {
            $junctionItem = Get-Item -LiteralPath $junction -Force -ErrorAction SilentlyContinue
            if ($null -ne $junctionItem -and
                ($junctionItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                [System.IO.Directory]::Delete($junction, $false)
            }
        }
        if (Test-Path -LiteralPath $testRoot) {
            [System.IO.Directory]::Delete($testRoot, $true)
        }
        if (Test-Path -LiteralPath $outside) {
            [System.IO.Directory]::Delete($outside, $true)
        }
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
}

# Expected negative-control subprocesses leave a non-zero LASTEXITCODE in the
# hosting PowerShell session.  The regression itself is green only after its
# scoped cleanup has completed, so make that result explicit to CI callers.
exit 0
