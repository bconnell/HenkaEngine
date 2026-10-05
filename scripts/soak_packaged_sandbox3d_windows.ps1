param(
    [ValidateRange(1, 100)]
    [int]$Iterations = 10,

    [Alias("IterationTimeoutMilliseconds")]
    [ValidateRange(1, 3600000)]
    [int]$NoProgressTimeoutMilliseconds = 30000,

    [ValidateRange(1000, 3600000)]
    [int]$HardTimeoutMilliseconds = 120000,

    # Hosted Windows runners can build the package without exposing an
    # OpenGL-capable desktop video driver. Keep local runs strict; CI may
    # explicitly record that infrastructure limitation as a skip.
    [switch]$AllowHeadlessUnavailable
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "henka_script_common.ps1")
. (Join-Path $PSScriptRoot "henka_packaged_startup_readiness.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$executable = Join-Path $repoRoot "out\HenkaSandbox3D\HenkaSandbox3D.exe"
$logRoot = Join-Path $repoRoot "build\test_tmp\packaged-soak"
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Packaged sandbox executable is missing: $executable"
}
[System.IO.Directory]::CreateDirectory($logRoot) | Out-Null

for ($iteration = 1; $iteration -le $Iterations; ++$iteration) {
    $stdoutPath = Join-Path $logRoot ("iteration-{0}.stdout.log" -f $iteration)
    $stderrPath = Join-Path $logRoot ("iteration-{0}.stderr.log" -f $iteration)
    Remove-Item -LiteralPath $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
    $capturedProcess = $null
    try {
        $previousAutomationDiagnostics = [Environment]::GetEnvironmentVariable(
            "HENKA_AUTOMATION_DIAGNOSTICS",
            [EnvironmentVariableTarget]::Process)
        $previousEveryFrameDiagnostics = [Environment]::GetEnvironmentVariable(
            "HENKA_AUTOMATION_DIAGNOSTICS_EVERY_FRAME",
            [EnvironmentVariableTarget]::Process)
        try {
            [Environment]::SetEnvironmentVariable(
                "HENKA_AUTOMATION_DIAGNOSTICS",
                "1",
                [EnvironmentVariableTarget]::Process)
            [Environment]::SetEnvironmentVariable(
                "HENKA_AUTOMATION_DIAGNOSTICS_EVERY_FRAME",
                "1",
                [EnvironmentVariableTarget]::Process)
            $capturedProcess = Start-HenkaCapturedProcess `
                -FilePath $executable `
                -Arguments @("--smoke-test") `
                -WorkingDirectory (Split-Path -Parent $executable) `
                -StdoutPath $stdoutPath `
                -StderrPath $stderrPath `
                -CreateNoWindow
        }
        finally {
            [Environment]::SetEnvironmentVariable(
                "HENKA_AUTOMATION_DIAGNOSTICS",
                $previousAutomationDiagnostics,
                [EnvironmentVariableTarget]::Process)
            [Environment]::SetEnvironmentVariable(
                "HENKA_AUTOMATION_DIAGNOSTICS_EVERY_FRAME",
                $previousEveryFrameDiagnostics,
                [EnvironmentVariableTarget]::Process)
        }

        $smokeProgress = $null
        $waitFailure = $null
        try {
            $smokeProgress = Wait-HenkaPackagedSmokeProgress `
                -StdoutPath $stdoutPath `
                -StderrPath $stderrPath `
                -ProcessId $capturedProcess.Process.Id `
                -NoProgressTimeoutMilliseconds $NoProgressTimeoutMilliseconds `
                -HardTimeoutMilliseconds $HardTimeoutMilliseconds
        }
        catch {
            $waitFailure = $_.Exception.Message
        }

        if ($null -ne $waitFailure) {
            Stop-HenkaProcessTree -ProcessId $capturedProcess.Process.Id
            [void]$capturedProcess.Process.WaitForExit(5000)
            $output = (Get-HenkaPackagedStartupLogText -Path $stdoutPath) +
                (Get-HenkaPackagedStartupLogText -Path $stderrPath)
            $diagnostic = (($output -replace "\s+", " ").Trim())
            if ($diagnostic.Length -gt 2048) {
                $diagnostic = $diagnostic.Substring($diagnostic.Length - 2048)
            }
            throw "$waitFailure Captured logs: stdout=$stdoutPath; stderr=$stderrPath. Output tail: $diagnostic"
        }

        [void]$capturedProcess.Process.WaitForExit()
        $exitCode = $capturedProcess.Process.ExitCode
        $output = (Get-HenkaPackagedStartupLogText -Path $stdoutPath) +
            (Get-HenkaPackagedStartupLogText -Path $stderrPath)
    }
    finally {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
    }
    if ($exitCode -ne 0) {
        $headlessUnavailable = $output -match "SDL_CreateWindow failed|Could not load EGL library|failed to load OpenGL function|platform initialization failed|Unable to start the sandbox: platform error"
        if ($AllowHeadlessUnavailable -and $headlessUnavailable) {
            Write-Warning "Packaged sandbox smoke iteration $iteration could not run because the host has no OpenGL-capable desktop video driver; recording an infrastructure skip."
            continue
        }
        $diagnostic = (($output -replace "\s+", " ").Trim())
        if ($diagnostic.Length -gt 2048) {
            $diagnostic = $diagnostic.Substring($diagnostic.Length - 2048)
        }
        throw "Packaged sandbox smoke iteration $iteration failed with exit code $exitCode. Captured logs: stdout=$stdoutPath; stderr=$stderrPath. Output tail: $diagnostic"
    }
    if (-not $smokeProgress.ApplicationProgressSeen) {
        throw "Packaged sandbox smoke iteration $iteration exited without application-owned frame progress telemetry. Captured logs: stdout=$stdoutPath; stderr=$stderrPath."
    }
    if ($output -notmatch "Sandbox smoke test completed\.") {
        throw "Packaged sandbox smoke iteration $iteration did not reach its completion marker."
    }
    if ($output -notmatch "memory shutdown clean: no active allocations tracked") {
        throw "Packaged sandbox smoke iteration $iteration did not report a clean memory shutdown."
    }
    Write-Host (
        "[pass] Packaged smoke iteration {0} exited after application frame seq={1} phase={2} in {3}ms." -f
        $iteration,
        $smokeProgress.LastProgressFrame,
        $smokeProgress.LastProgressPhase,
        $smokeProgress.ElapsedMilliseconds)
}

if ($AllowHeadlessUnavailable) {
    Write-Host "[pass] Packaged sandbox bounded soak completed: $Iterations iterations; local runs required clean memory shutdown, and explicitly headless hosts were recorded as infrastructure skips."
}
else {
    Write-Host "[pass] Packaged sandbox bounded soak completed: $Iterations iterations; clean memory shutdown reported for each run."
}
