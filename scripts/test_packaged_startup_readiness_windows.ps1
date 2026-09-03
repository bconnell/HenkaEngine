param(
    [string]$RepositoryRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
}

$readinessHelper = Join-Path $RepositoryRoot "scripts\henka_packaged_startup_readiness.ps1"
if (-not (Test-Path -LiteralPath $readinessHelper -PathType Leaf)) {
    throw "Packaged startup readiness helper is not implemented: $readinessHelper"
}
. $readinessHelper

function Assert-Equal {
    param(
        [Parameter(Mandatory = $true)][object]$Actual,
        [Parameter(Mandatory = $true)][object]$Expected,
        [Parameter(Mandatory = $true)][string]$Description
    )

    if ($Actual -ne $Expected) {
        throw "$Description. Expected '$Expected', actual '$Actual'."
    }
}

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
    "henka-packaged-startup-readiness-" + [Guid]::NewGuid().ToString("N"))
$stdoutPath = Join-Path $temporaryRoot "stdout.log"
$stderrPath = Join-Path $temporaryRoot "stderr.log"
$slowProgressJob = $null

New-Item -ItemType Directory -Path $temporaryRoot -Force | Out-Null
try {
    [System.IO.File]::WriteAllText($stdoutPath, "")
    [System.IO.File]::WriteAllText($stderrPath, "")

    $slowProgressJob = Start-Job -ScriptBlock {
        param(
            [string]$OutputPath,
            [string]$ErrorPath
        )

        Start-Sleep -Milliseconds 250
        [System.IO.File]::AppendAllText($ErrorPath, "engine startup complete`n")
        Start-Sleep -Milliseconds 400
        [System.IO.File]::AppendAllText($ErrorPath, "entering engine run loop`n")
        Start-Sleep -Milliseconds 400
        [System.IO.File]::AppendAllText($OutputPath, "Henka Engine Sandbox 3D`n")
    } -ArgumentList $stdoutPath, $stderrPath

    $slowResult = Wait-HenkaPackagedStartupReady `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath `
        -ProcessId $PID `
        -HardTimeoutMilliseconds 6000 `
        -NoProgressTimeoutMilliseconds 3000 `
        -PollMilliseconds 50

    Assert-Equal -Actual $slowResult.Ready -Expected $true `
        -Description "Slow-but-progressing startup was accepted"
    Assert-Equal -Actual $slowResult.LastProgressStage -Expected "startup help" `
        -Description "Readiness reported the final application stage"
    Write-Output "[pass] Slow-but-progressing packaged startup readiness"

    [System.IO.File]::WriteAllText($stdoutPath, "")
    [System.IO.File]::WriteAllText($stderrPath, "")
    $hardLimitFailure = $null
    try {
        $null = Wait-HenkaPackagedStartupReady `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath `
            -ProcessId $PID `
            -HardTimeoutMilliseconds 350 `
            -NoProgressTimeoutMilliseconds 1000 `
            -PollMilliseconds 25
    } catch {
        $hardLimitFailure = $_
    }

    if ($null -eq $hardLimitFailure) {
        throw "A startup with no application progress exceeded its hard upper limit."
    }
    if ($hardLimitFailure.Exception.Message -notmatch "hard startup readiness limit") {
        throw "The startup readiness failure did not identify the hard upper limit: $($hardLimitFailure.Exception.Message)"
    }
    Write-Output "[pass] Packaged startup readiness hard upper limit"
} finally {
    if ($null -ne $slowProgressJob) {
        Stop-Job -Job $slowProgressJob -ErrorAction SilentlyContinue
        Remove-Job -Job $slowProgressJob -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}

exit 0
