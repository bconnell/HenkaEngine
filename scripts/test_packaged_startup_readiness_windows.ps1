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

if ($null -eq (Get-Command Wait-HenkaPackagedFrameRenderComplete -ErrorAction SilentlyContinue)) {
    throw "Packaged frame-render readiness helper is not implemented: $readinessHelper"
}
if ($null -eq (Get-Command Wait-HenkaPackagedNativeAuthoringRestore -ErrorAction SilentlyContinue)) {
    throw "Packaged native-authoring restore readiness helper is not implemented: $readinessHelper"
}

$engineSourcePath = Join-Path $RepositoryRoot "engine\src\core\engine.c"
$engineSource = Get-Content -LiteralPath $engineSourcePath -Raw
if ($engineSource.Contains("automation_diagnostic_frame_report_count")) {
    throw "Application frame-progress diagnostics must not stop after a fixed report count."
}
if ($engineSource -notmatch '(?s)report_automation_frame\s*=\s*automation_diagnostics_enabled\s*&&\s*\(\s*engine->time\.frame_index\s*<=\s*5U\s*\|\|\s*engine->time\.frame_index\s*%\s*30U\s*==\s*0U\s*\)') {
    throw "Application frame-progress diagnostics must remain periodically observable while automation diagnostics are enabled."
}

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
$renderProgressJob = $null
$postLongRunJob = $null
$nativeRestoreProgressJob = $null
$nativeRestoreStallJob = $null
$nativeRestoreHardLimitJob = $null

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
    $nativeRestoreProgressJob = Start-Job -ScriptBlock {
        param([string]$OutputPath, [string]$ErrorPath)

        Start-Sleep -Milliseconds 200
        [System.IO.File]::AppendAllText($ErrorPath, "engine startup complete`n")
        Start-Sleep -Milliseconds 250
        [System.IO.File]::AppendAllText($ErrorPath, "entering engine run loop`n")
        Start-Sleep -Milliseconds 250
        [System.IO.File]::AppendAllText($OutputPath, "Henka Engine Sandbox 3D`n")
        Start-Sleep -Milliseconds 450
        [System.IO.File]::AppendAllText(
            $OutputPath,
            "Native authoring topology bridge: name=Fixture source_state=HENKA_NATIVE_EDITABLE_SOURCE.`n")
        Start-Sleep -Milliseconds 450
        [System.IO.File]::AppendAllText(
            $OutputPath,
            "Native authoring startup restore: material state restored for name=Fixture source_state=HENKA_NATIVE_EDITABLE_MATERIAL_INSTANCE pbr_state=restored transmission=0.0 ior=1.5 subsurface=0.0 clearcoat=0.0 sheen=0.0.`n")
        Start-Sleep -Milliseconds 450
        [System.IO.File]::AppendAllText(
            $OutputPath,
            "Native authoring startup restore: name=Fixture vertices=8 faces=6 source_state=HENKA_NATIVE_EDITABLE_SOURCE.`n")
    } -ArgumentList $stdoutPath, $stderrPath

    $nativeRestoreResult = Wait-HenkaPackagedNativeAuthoringRestore `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath `
        -ProcessId $PID `
        -HardTimeoutMilliseconds 5000 `
        -NoProgressTimeoutMilliseconds 1000 `
        -PollMilliseconds 25
    Assert-Equal -Actual $nativeRestoreResult.Ready -Expected $true `
        -Description "Slow-but-progressing persisted native authoring restore was accepted"
    Assert-Equal -Actual $nativeRestoreResult.LastProgressStage -Expected "persisted native authoring source" `
        -Description "Native restore readiness reported the final source stage"
    Assert-Equal -Actual $nativeRestoreResult.ProgressStagesObserved -Expected 6 `
        -Description "Native restore readiness required all startup and restore stages"
    if ($nativeRestoreResult.ElapsedMilliseconds -lt 1500) {
        throw "The staged native restore fixture did not exercise a slow-but-progressing startup."
    }
    Write-Output "[pass] Slow-but-progressing persisted native authoring restore"

    [System.IO.File]::WriteAllText($stdoutPath, "")
    [System.IO.File]::WriteAllText(
        $stderrPath,
        "engine startup complete`nentering engine run loop`n")
    [System.IO.File]::WriteAllText(
        $stdoutPath,
        "Henka Engine Sandbox 3D`n" +
        "Native authoring topology bridge: name=Fixture source_state=HENKA_NATIVE_EDITABLE_SOURCE.`n")
    $nativeRestoreStallJob = Start-Job -ScriptBlock {
        param([string]$OutputPath)

        for ($tick = 0; $tick -lt 20; $tick++) {
            Start-Sleep -Milliseconds 50
            [System.IO.File]::AppendAllText(
                $OutputPath,
                "HENKA_AUTOMATION_DIAGNOSTIC frame seq=$tick phase=render-complete`n")
        }
    } -ArgumentList $stdoutPath

    $nativeRestoreStallFailure = $null
    try {
        $null = Wait-HenkaPackagedNativeAuthoringRestore `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath `
            -ProcessId $PID `
            -HardTimeoutMilliseconds 3000 `
            -NoProgressTimeoutMilliseconds 350 `
            -PollMilliseconds 25
    } catch {
        $nativeRestoreStallFailure = $_
    }
    if ($null -eq $nativeRestoreStallFailure -or
        $nativeRestoreStallFailure.Exception.Message -notmatch "made no recognized application progress.*native authoring topology bridge") {
        $actualFailure = if ($null -eq $nativeRestoreStallFailure) { "none" } else { $nativeRestoreStallFailure.Exception.Message }
        throw "Unrelated frame heartbeats concealed a stalled native authoring restore; observed '$actualFailure'."
    }
    Write-Output "[pass] Native authoring restore rejects unrelated heartbeat-only progress"

    [System.IO.File]::WriteAllText($stdoutPath, "")
    [System.IO.File]::WriteAllText($stderrPath, "")
    $nativeRestoreHardLimitJob = Start-Job -ScriptBlock {
        param([string]$OutputPath, [string]$ErrorPath)

        Start-Sleep -Milliseconds 100
        [System.IO.File]::AppendAllText($ErrorPath, "engine startup complete`n")
        Start-Sleep -Milliseconds 100
        [System.IO.File]::AppendAllText($ErrorPath, "entering engine run loop`n")
        Start-Sleep -Milliseconds 100
        [System.IO.File]::AppendAllText($OutputPath, "Henka Engine Sandbox 3D`n")
        Start-Sleep -Milliseconds 150
        [System.IO.File]::AppendAllText(
            $OutputPath,
            "Native authoring topology bridge: name=Fixture source_state=HENKA_NATIVE_EDITABLE_SOURCE.`n")
        Start-Sleep -Milliseconds 150
        [System.IO.File]::AppendAllText(
            $OutputPath,
            "Native authoring startup restore: material state restored for name=Fixture source_state=HENKA_NATIVE_EDITABLE_MATERIAL_INSTANCE pbr_state=restored transmission=0.0 ior=1.5 subsurface=0.0 clearcoat=0.0 sheen=0.0.`n")
        Start-Sleep -Milliseconds 150
        [System.IO.File]::AppendAllText(
            $OutputPath,
            "Native authoring startup restore: name=Fixture vertices=8 faces=6 source_state=HENKA_NATIVE_EDITABLE_SOURCE.`n")
    } -ArgumentList $stdoutPath, $stderrPath

    $nativeRestoreHardLimitFailure = $null
    try {
        $null = Wait-HenkaPackagedNativeAuthoringRestore `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath `
            -ProcessId $PID `
            -HardTimeoutMilliseconds 650 `
            -NoProgressTimeoutMilliseconds 1000 `
            -PollMilliseconds 25
    } catch {
        $nativeRestoreHardLimitFailure = $_
    }
    if ($null -eq $nativeRestoreHardLimitFailure -or
        $nativeRestoreHardLimitFailure.Exception.Message -notmatch "hard startup readiness limit") {
        $actualFailure = if ($null -eq $nativeRestoreHardLimitFailure) { "none" } else { $nativeRestoreHardLimitFailure.Exception.Message }
        throw "A progressing native authoring restore exceeded its hard upper limit without the expected failure; observed '$actualFailure'."
    }
    Write-Output "[pass] Native authoring restore enforces its absolute hard limit"

    [System.IO.File]::WriteAllText($stdoutPath, "")
    $renderProgressJob = Start-Job -ScriptBlock {
        param([string]$OutputPath)

        foreach ($line in @(
            "HENKA_AUTOMATION_DIAGNOSTIC frame seq=30 phase=loop-begin records=4",
            "HENKA_AUTOMATION_DIAGNOSTIC frame seq=30 phase=events-polled record_available=0",
            "HENKA_AUTOMATION_DIAGNOSTIC frame seq=30 phase=update-complete",
            "HENKA_AUTOMATION_DIAGNOSTIC frame seq=30 phase=render-begin",
            "HENKA_AUTOMATION_DIAGNOSTIC frame seq=30 phase=render-complete")) {
            Start-Sleep -Milliseconds 225
            [System.IO.File]::AppendAllText($OutputPath, $line + [Environment]::NewLine)
        }
    } -ArgumentList $stdoutPath

    $renderProgressResult = Wait-HenkaPackagedFrameRenderComplete `
        -StdoutPath $stdoutPath `
        -ProcessId $PID `
        -StartingOffset 0 `
        -AfterFrameSequence 29 `
        -HardTimeoutMilliseconds 6000 `
        -NoProgressTimeoutMilliseconds 1200 `
        -PollMilliseconds 50
    Assert-Equal -Actual $renderProgressResult.Ready -Expected $true `
        -Description "Slow-but-progressing render transition was accepted"
    Assert-Equal -Actual $renderProgressResult.FrameSequence -Expected 30 `
        -Description "Render readiness reported the completed application frame"
    Write-Output "[pass] Slow-but-progressing viewport render readiness"

    $oldFrameReports = [System.Text.StringBuilder]::new()
    for ($frame = 1; $frame -le 256; ++$frame) {
        $sequence = [long]$frame * 30L
        [void]$oldFrameReports.AppendLine(
            "HENKA_AUTOMATION_DIAGNOSTIC frame seq=$sequence phase=render-complete")
    }
    [System.IO.File]::WriteAllText($stdoutPath, $oldFrameReports.ToString())
    $postLongRunOffset = [System.IO.FileInfo]::new($stdoutPath).Length
    $postLongRunJob = Start-Job -ScriptBlock {
        param([string]$OutputPath)
        Start-Sleep -Milliseconds 200
        foreach ($stage in @("loop-begin", "events-polled", "update-complete", "render-begin", "render-complete")) {
            [System.IO.File]::AppendAllText(
                $OutputPath,
                "HENKA_AUTOMATION_DIAGNOSTIC frame seq=7560 phase=$stage" + [Environment]::NewLine)
            Start-Sleep -Milliseconds 150
        }
    } -ArgumentList $stdoutPath
    $postLongRunResult = Wait-HenkaPackagedFrameRenderComplete `
        -StdoutPath $stdoutPath `
        -ProcessId $PID `
        -StartingOffset $postLongRunOffset `
        -AfterFrameSequence 7530 `
        -HardTimeoutMilliseconds 6000 `
        -NoProgressTimeoutMilliseconds 1200 `
        -PollMilliseconds 50
    Assert-Equal -Actual $postLongRunResult.Ready -Expected $true `
        -Description "Frame readiness remains observable after 256 earlier reports"
    Assert-Equal -Actual $postLongRunResult.FrameSequence -Expected 7560 `
        -Description "Post-cap frame readiness reports the later application frame"
    Write-Output "[pass] Long-running automation frame readiness beyond the former 256-report cap"

    [System.IO.File]::WriteAllText(
        $stdoutPath,
        "HENKA_AUTOMATION_DIAGNOSTIC frame seq=30 phase=render-begin`n")
    $renderStallFailure = $null
    try {
        $null = Wait-HenkaPackagedFrameRenderComplete `
            -StdoutPath $stdoutPath `
            -ProcessId $PID `
            -StartingOffset 0 `
            -AfterFrameSequence 29 `
            -HardTimeoutMilliseconds 2000 `
            -NoProgressTimeoutMilliseconds 350 `
            -PollMilliseconds 25
    } catch {
        $renderStallFailure = $_
    }
    if ($null -eq $renderStallFailure -or
        $renderStallFailure.Exception.Message -notmatch "made no application frame progress") {
        throw "A stalled viewport render was not rejected by its stage-aware no-progress bound."
    }
    Write-Output "[pass] Viewport render readiness rejects a stalled render stage"

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
    if ($null -ne $renderProgressJob) {
        Stop-Job -Job $renderProgressJob -ErrorAction SilentlyContinue
        Remove-Job -Job $renderProgressJob -Force -ErrorAction SilentlyContinue
    }
    if ($null -ne $postLongRunJob) {
        Stop-Job -Job $postLongRunJob -ErrorAction SilentlyContinue
        Remove-Job -Job $postLongRunJob -Force -ErrorAction SilentlyContinue
    }
    if ($null -ne $nativeRestoreProgressJob) {
        Stop-Job -Job $nativeRestoreProgressJob -ErrorAction SilentlyContinue
        Remove-Job -Job $nativeRestoreProgressJob -Force -ErrorAction SilentlyContinue
    }
    if ($null -ne $nativeRestoreStallJob) {
        Stop-Job -Job $nativeRestoreStallJob -ErrorAction SilentlyContinue
        Remove-Job -Job $nativeRestoreStallJob -Force -ErrorAction SilentlyContinue
    }
    if ($null -ne $nativeRestoreHardLimitJob) {
        Stop-Job -Job $nativeRestoreHardLimitJob -ErrorAction SilentlyContinue
        Remove-Job -Job $nativeRestoreHardLimitJob -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}

exit 0
