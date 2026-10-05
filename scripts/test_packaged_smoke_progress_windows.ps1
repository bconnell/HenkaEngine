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

. (Join-Path $RepositoryRoot "scripts\henka_script_common.ps1")
. (Join-Path $RepositoryRoot "scripts\henka_packaged_startup_readiness.ps1")

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
    "henka-packaged-smoke-progress-" + [Guid]::NewGuid().ToString("N"))
$fixturePath = Join-Path $temporaryRoot "smoke-progress-fixture.ps1"
$powerShellPath = (Get-Process -Id $PID).Path
$captured = $null

$fixture = @'
param([Parameter(Mandatory = $true)][string]$Mode)

function Write-ProgressRecord {
    param([long]$Frame, [string]$Phase)
    $line = "HENKA_AUTOMATION_DIAGNOSTIC frame seq=$Frame phase=$Phase"
    [Console]::Out.WriteLine($line)
    [Console]::Out.Flush()
}

if ($Mode -eq "slow-progress") {
    $records = @(
        "0|loop-begin",
        "0|events-polled",
        "0|update-complete",
        "0|render-begin",
        "0|render-complete",
        "1|loop-begin"
    )
    foreach ($record in $records) {
        $parts = $record.Split('|')
        Write-ProgressRecord -Frame ([long]$parts[0]) -Phase $parts[1]
        Start-Sleep -Milliseconds 900
    }
    [Console]::Out.WriteLine("Sandbox smoke test completed.")
    [Console]::Out.WriteLine("memory shutdown clean: no active allocations tracked")
    [Console]::Out.Flush()
}
elseif ($Mode -eq "noise-only") {
    for ($index = 0; $index -lt 100; ++$index) {
        [Console]::Out.WriteLine("child process is alive; no engine frame advanced")
        [Console]::Out.Flush()
        Start-Sleep -Milliseconds 100
    }
}
elseif ($Mode -eq "hard-limit") {
    for ($frame = 0; $frame -lt 100; ++$frame) {
        Write-ProgressRecord -Frame $frame -Phase "loop-begin"
        Start-Sleep -Milliseconds 150
    }
}
else {
    throw "Unknown fixture mode: $Mode"
}
'@

function Start-SmokeProgressFixture {
    param([Parameter(Mandatory = $true)][string]$Mode)

    $modeRoot = Join-Path $temporaryRoot $Mode
    [System.IO.Directory]::CreateDirectory($modeRoot) | Out-Null
    return Start-HenkaCapturedProcess `
        -FilePath $powerShellPath `
        -Arguments @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $fixturePath, $Mode) `
        -WorkingDirectory $modeRoot `
        -StdoutPath (Join-Path $modeRoot "stdout.log") `
        -StderrPath (Join-Path $modeRoot "stderr.log") `
        -CreateNoWindow
}

function Assert-SmokeProgressFailure {
    param(
        [Parameter(Mandatory = $true)][string]$Mode,
        [Parameter(Mandatory = $true)][string]$ExpectedMessage,
        [Parameter(Mandatory = $true)][int]$NoProgressMilliseconds,
        [Parameter(Mandatory = $true)][int]$HardTimeoutMilliseconds
    )

    $script:captured = Start-SmokeProgressFixture -Mode $Mode
    $stdoutPath = Join-Path (Join-Path $temporaryRoot $Mode) "stdout.log"
    $stderrPath = Join-Path (Join-Path $temporaryRoot $Mode) "stderr.log"
    $failure = $null
    try {
        $null = Wait-HenkaPackagedSmokeProgress `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath `
            -ProcessId $script:captured.Process.Id `
            -NoProgressTimeoutMilliseconds $NoProgressMilliseconds `
            -HardTimeoutMilliseconds $HardTimeoutMilliseconds `
            -PollMilliseconds 40
    }
    catch {
        $failure = $_
    }
    finally {
        Close-HenkaCapturedProcess -CapturedProcess $script:captured
        $script:captured = $null
    }

    if ($null -eq $failure) {
        throw "Smoke progress fixture '$Mode' did not fail at its required boundary."
    }
    if ($failure.Exception.Message -notmatch $ExpectedMessage) {
        throw "Smoke progress fixture '$Mode' failed for the wrong reason: $($failure.Exception.Message)"
    }
}

if ([string]::IsNullOrWhiteSpace($powerShellPath) -or
    -not (Test-Path -LiteralPath $powerShellPath -PathType Leaf)) {
    throw "Current PowerShell executable is unavailable for process-boundary regression: $powerShellPath"
}

[System.IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
[System.IO.File]::WriteAllText(
    $fixturePath,
    $fixture,
    [System.Text.UTF8Encoding]::new($false))

try {
    $slow = Start-SmokeProgressFixture -Mode "slow-progress"
    try {
        $slowStdout = Join-Path (Join-Path $temporaryRoot "slow-progress") "stdout.log"
        $slowStderr = Join-Path (Join-Path $temporaryRoot "slow-progress") "stderr.log"
        $slowResult = Wait-HenkaPackagedSmokeProgress `
            -StdoutPath $slowStdout `
            -StderrPath $slowStderr `
            -ProcessId $slow.Process.Id `
            -NoProgressTimeoutMilliseconds 1600 `
            -HardTimeoutMilliseconds 10000 `
            -PollMilliseconds 40
        [void]$slow.Process.WaitForExit()
        Assert-Equal -Actual $slow.Process.ExitCode -Expected 0 `
            -Description "Progressing smoke process exit status"
        if ($slowResult.ElapsedMilliseconds -le 1600) {
            throw "Slow smoke fixture did not exceed its inactivity interval."
        }
        if (-not $slowResult.ApplicationProgressSeen -or
            $slowResult.LastProgressFrame -lt 1) {
            throw "Smoke completion did not report advancing application frame state."
        }
        $slowOutput = (Get-HenkaPackagedStartupLogText -Path $slowStdout) +
            (Get-HenkaPackagedStartupLogText -Path $slowStderr)
        if ($slowOutput -notmatch "Sandbox smoke test completed\." -or
            $slowOutput -notmatch "memory shutdown clean: no active allocations tracked") {
            throw "Progressing smoke fixture did not preserve both production completion conditions."
        }
        Write-Output "[pass] Slow-but-progressing packaged smoke exits after application-owned frame progress"
    }
    catch {
        $stdout = Get-HenkaPackagedStartupLogText -Path $slowStdout
        $stderr = Get-HenkaPackagedStartupLogText -Path $slowStderr
        throw (
            "Slow-progress fixture failed: $($_.Exception.Message) " +
            "stdout='$stdout' stderr='$stderr'")
    }
    finally {
        Close-HenkaCapturedProcess -CapturedProcess $slow
    }

    Assert-SmokeProgressFailure `
        -Mode "noise-only" `
        -ExpectedMessage "no application frame progress" `
        -NoProgressMilliseconds 1500 `
        -HardTimeoutMilliseconds 6000
    Write-Output "[pass] Generic child output cannot mask a stalled engine frame"

    Assert-SmokeProgressFailure `
        -Mode "hard-limit" `
        -ExpectedMessage "hard packaged smoke limit" `
        -NoProgressMilliseconds 1000 `
        -HardTimeoutMilliseconds 2500
    Write-Output "[pass] Advancing smoke progress cannot exceed the absolute hard limit"
}
finally {
    if ($null -ne $captured) {
        Close-HenkaCapturedProcess -CapturedProcess $captured
    }
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}

exit 0
