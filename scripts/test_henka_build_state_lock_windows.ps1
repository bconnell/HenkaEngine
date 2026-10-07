param(
    [string]$RepositoryRoot = "",
    [switch]$Worker,
    [string]$SignalPath = "",
    [string]$ReleaseSignalPath = "",
    [int]$HoldSeconds = 0,
    [int]$TimeoutSeconds = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
} else {
    $RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
}

if ($Worker) {
    $lock = $null
    try {
        $lock = Enter-HenkaBuildStateLock -TimeoutSeconds $TimeoutSeconds
        if (-not [string]::IsNullOrWhiteSpace($SignalPath)) {
            [System.IO.File]::WriteAllText(
                [System.IO.Path]::GetFullPath($SignalPath),
                "acquired" + [Environment]::NewLine)
        }
        if (-not [string]::IsNullOrWhiteSpace($ReleaseSignalPath)) {
            if ($HoldSeconds -le 0) {
                throw "A lock-worker release handshake requires a positive maximum hold time."
            }
            $releaseDeadline = [DateTime]::UtcNow.AddSeconds($HoldSeconds)
            while (-not (Test-Path -LiteralPath $ReleaseSignalPath -PathType Leaf) -and
                [DateTime]::UtcNow -lt $releaseDeadline) {
                Start-Sleep -Milliseconds 50
            }
            if (-not (Test-Path -LiteralPath $ReleaseSignalPath -PathType Leaf)) {
                throw "The lock-worker release handshake timed out after $HoldSeconds seconds."
            }
        }
        elseif ($HoldSeconds -gt 0) {
            Start-Sleep -Seconds $HoldSeconds
        }
        Write-Output "[pass] Build-state lock worker acquired and released the shared lock."
        exit 0
    } finally {
        if ($null -ne $lock) {
            Exit-HenkaBuildStateLock -Lock $lock
        }
    }
}

$fixtureDirectory = Join-Path (Get-HenkaTestTemporaryRoot -RepositoryRoot $RepositoryRoot) "build-state-lock-regression-$PID"
$signalPath = Join-Path $fixtureDirectory "worker-acquired.txt"
$releaseSignalPath = Join-Path $fixtureDirectory "worker-release.txt"
New-Item -ItemType Directory -Path $fixtureDirectory -Force | Out-Null

$workerProcess = $null
try {
    $workerArguments = @(
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-File", $PSCommandPath,
        "-RepositoryRoot", $RepositoryRoot,
        "-Worker",
        "-SignalPath", $signalPath,
        "-ReleaseSignalPath", $releaseSignalPath,
        "-HoldSeconds", "30",
        "-TimeoutSeconds", "10")
    $workerProcess = Start-HenkaProcess `
        -FilePath "powershell.exe" `
        -Arguments $workerArguments `
        -WorkingDirectory $RepositoryRoot `
        -CreateNoWindow

    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    while (-not (Test-Path -LiteralPath $signalPath -PathType Leaf) -and
        -not $workerProcess.HasExited -and [DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 100
        $workerProcess.Refresh()
    }
    if (-not (Test-Path -LiteralPath $signalPath -PathType Leaf)) {
        throw "The lock worker did not acquire the shared lock before the deadline."
    }

    $contenderResult = $null
    try {
        $contenderResult = Invoke-HenkaExpectedFailure `
            -FilePath "powershell.exe" `
            -Arguments @(
                "-NoProfile",
                "-ExecutionPolicy", "Bypass",
                "-File", $PSCommandPath,
                "-RepositoryRoot", $RepositoryRoot,
                "-Worker",
                "-TimeoutSeconds", "0") `
            -WorkingDirectory $RepositoryRoot `
            -Label "Run concurrent build-state lock negative control" `
            -ReturnOutput
    }
    finally {
        [System.IO.File]::WriteAllText($releaseSignalPath, "release" + [Environment]::NewLine)
    }
    $contenderExitCode = $contenderResult.ExitCode
    $contenderText = ($contenderResult.Stdout + [Environment]::NewLine + $contenderResult.Stderr)
    if ($contenderExitCode -eq 0) {
        throw "A concurrent build-state worker acquired the lock unexpectedly."
    }
    if ($contenderText -notmatch "shared generated build-state lock") {
        throw "The concurrent lock rejection did not identify the shared generated build-state lock: $contenderText"
    }

    Wait-Process -Id $workerProcess.Id -Timeout 15
    $workerProcess.Refresh()
    if (-not $workerProcess.HasExited -or $workerProcess.ExitCode -ne 0) {
        throw "The lock worker did not exit successfully."
    }

    Write-Output "[pass] Concurrent generated build-state access is rejected by the shared lock."
} finally {
    if ($null -ne $workerProcess -and -not $workerProcess.HasExited) {
        Stop-HenkaProcessTree -ProcessId $workerProcess.Id
    }
    if (Test-Path -LiteralPath $fixtureDirectory -PathType Container) {
        Remove-Item -LiteralPath $fixtureDirectory -Recurse -Force
    }
}
