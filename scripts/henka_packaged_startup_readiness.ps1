Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-HenkaPackagedStartupLogText {
    param(
        [Parameter(Mandatory = $true)][string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [string]::Empty
    }

    $stream = $null
    $reader = $null
    $text = [string]::Empty
    try {
        $shareMode = [System.IO.FileShare](
            [int][System.IO.FileShare]::ReadWrite -bor
            [int][System.IO.FileShare]::Delete)
        $stream = [System.IO.FileStream]::new(
            $Path,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            $shareMode)
        $reader = [System.IO.StreamReader]::new(
            $stream,
            [System.Text.Encoding]::UTF8,
            $true,
            4096,
            $false)
        $text = $reader.ReadToEnd()
    }
    catch [System.IO.IOException] {
        return [string]::Empty
    }
    catch [System.UnauthorizedAccessException] {
        return [string]::Empty
    }
    finally {
        if ($null -ne $reader) {
            $reader.Dispose()
        }
        elseif ($null -ne $stream) {
            $stream.Dispose()
        }
    }

    return $text
}

function Test-HenkaPackagedStartupProcessAlive {
    param(
        [Parameter(Mandatory = $true)][int]$ProcessId
    )

    try {
        $process = Get-Process -Id $ProcessId -ErrorAction Stop
        return -not $process.HasExited
    }
    catch [System.ArgumentException] {
        return $false
    }
    catch [Microsoft.PowerShell.Commands.ProcessCommandException] {
        return $false
    }
}

function Wait-HenkaPackagedStartupReady {
    param(
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$StderrPath,
        [Parameter(Mandatory = $true)][int]$ProcessId,
        [ValidateRange(1, 3600000)][int]$HardTimeoutMilliseconds = 120000,
        [ValidateRange(1, 3600000)][int]$NoProgressTimeoutMilliseconds = 45000,
        [ValidateRange(1, 5000)][int]$PollMilliseconds = 150
    )

    if ($NoProgressTimeoutMilliseconds -gt $HardTimeoutMilliseconds) {
        # A longer no-progress interval is useful for the focused hard-limit
        # regression, but production callers normally keep both bounds finite.
        $effectiveNoProgressTimeout = $HardTimeoutMilliseconds
    }
    else {
        $effectiveNoProgressTimeout = $NoProgressTimeoutMilliseconds
    }

    $stages = @(
        [pscustomobject]@{
            Name = "engine startup"
            Pattern = "engine startup complete"
            Source = "stderr"
        },
        [pscustomobject]@{
            Name = "engine run loop"
            Pattern = "entering engine run loop"
            Source = "stderr"
        },
        [pscustomobject]@{
            Name = "startup help"
            Pattern = "Henka Engine Sandbox 3D"
            Source = "stdout"
        }
    )
    $stageIndex = 0
    $lastProgressStage = "process launch"
    $lastProgressElapsedMilliseconds = 0L
    $clock = [System.Diagnostics.Stopwatch]::StartNew()

    while ($true) {
        $elapsedMilliseconds = [long]$clock.ElapsedMilliseconds
        $stdout = Get-HenkaPackagedStartupLogText -Path $StdoutPath
        $stderr = Get-HenkaPackagedStartupLogText -Path $StderrPath

        while ($stageIndex -lt $stages.Count) {
            $stage = $stages[$stageIndex]
            $stageText = if ($stage.Source -eq "stdout") { $stdout } else { $stderr }
            if (-not [System.Text.RegularExpressions.Regex]::IsMatch(
                    $stageText,
                    $stage.Pattern,
                    [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)) {
                break
            }

            $lastProgressStage = $stage.Name
            $lastProgressElapsedMilliseconds = $elapsedMilliseconds
            $stageIndex++
        }

        if (-not (Test-HenkaPackagedStartupProcessAlive -ProcessId $ProcessId)) {
            throw (
                "Packaged sandbox exited before startup readiness; last " +
                "application stage was '$lastProgressStage'.")
        }

        if ($stageIndex -eq $stages.Count) {
            return [pscustomobject]@{
                Ready = $true
                LastProgressStage = $lastProgressStage
                ProgressStagesObserved = $stageIndex
                ElapsedMilliseconds = $elapsedMilliseconds
            }
        }

        if ($elapsedMilliseconds -ge $HardTimeoutMilliseconds) {
            throw (
                "Packaged startup exceeded the hard startup readiness limit of " +
                "$HardTimeoutMilliseconds ms; last application stage was " +
                "'$lastProgressStage'.")
        }

        if (($elapsedMilliseconds - $lastProgressElapsedMilliseconds) -ge $effectiveNoProgressTimeout) {
            throw (
                "Packaged startup made no recognized application progress for " +
                "$effectiveNoProgressTimeout ms; last application stage was " +
                "'$lastProgressStage'.")
        }

        $remainingHardMilliseconds = $HardTimeoutMilliseconds - $elapsedMilliseconds
        $sleepMilliseconds = [Math]::Min($PollMilliseconds, $remainingHardMilliseconds)
        if ($sleepMilliseconds -gt 0) {
            Start-Sleep -Milliseconds $sleepMilliseconds
        }
    }
}

function Get-HenkaPackagedSmokeProgress {
    param(
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$StdoutText,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$StderrText
    )

    $phaseOrder = @{
        "loop-begin" = 0
        "events-polled" = 1
        "update-complete" = 2
        "render-begin" = 3
        "render-complete" = 4
    }
    $latestFrame = -1L
    $latestPhaseOrder = -1
    $latestPhase = "none"
    $pattern = "HENKA_AUTOMATION_DIAGNOSTIC frame seq=(?<frame>[0-9]+) phase=(?<phase>loop-begin|events-polled|update-complete|render-begin|render-complete)\b"
    $combinedText = $StdoutText + [Environment]::NewLine + $StderrText
    $progressMatches = [System.Text.RegularExpressions.Regex]::Matches($combinedText, $pattern)

    foreach ($match in $progressMatches) {
        $frame = 0L
        if (-not [long]::TryParse($match.Groups["frame"].Value, [ref]$frame)) {
            continue
        }

        $phase = $match.Groups["phase"].Value
        $order = $phaseOrder[$phase]
        if ($frame -gt $latestFrame -or
            ($frame -eq $latestFrame -and $order -gt $latestPhaseOrder)) {
            $latestFrame = $frame
            $latestPhaseOrder = $order
            $latestPhase = $phase
        }
    }

    return [pscustomobject]@{
        Seen = ($latestFrame -ge 0L)
        Frame = $latestFrame
        PhaseOrder = $latestPhaseOrder
        Phase = $latestPhase
    }
}

function Wait-HenkaPackagedSmokeProgress {
    param(
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$StderrPath,
        [Parameter(Mandatory = $true)][int]$ProcessId,
        [ValidateRange(1, 3600000)][int]$NoProgressTimeoutMilliseconds = 30000,
        [ValidateRange(1, 3600000)][int]$HardTimeoutMilliseconds = 120000,
        [ValidateRange(1, 5000)][int]$PollMilliseconds = 100
    )

    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    $lastProgressElapsedMilliseconds = 0L
    $lastProgressFrame = -1L
    $lastProgressPhaseOrder = -1
    $lastProgressPhase = "none"

    while ($true) {
        $stdout = Get-HenkaPackagedStartupLogText -Path $StdoutPath
        $stderr = Get-HenkaPackagedStartupLogText -Path $StderrPath
        $progress = Get-HenkaPackagedSmokeProgress `
            -StdoutText $stdout `
            -StderrText $stderr
        $elapsedMilliseconds = [long]$clock.ElapsedMilliseconds

        if ($progress.Seen -and
            ($progress.Frame -gt $lastProgressFrame -or
                ($progress.Frame -eq $lastProgressFrame -and
                    $progress.PhaseOrder -gt $lastProgressPhaseOrder))) {
            $lastProgressElapsedMilliseconds = $elapsedMilliseconds
            $lastProgressFrame = $progress.Frame
            $lastProgressPhaseOrder = $progress.PhaseOrder
            $lastProgressPhase = $progress.Phase
        }

        if ($elapsedMilliseconds -ge $HardTimeoutMilliseconds) {
            throw (
                "Packaged sandbox smoke exceeded the hard packaged smoke limit " +
                "of $HardTimeoutMilliseconds ms; last application frame was " +
                "seq=$lastProgressFrame phase=$lastProgressPhase.")
        }

        if (-not (Test-HenkaPackagedStartupProcessAlive -ProcessId $ProcessId)) {
            return [pscustomobject]@{
                Exited = $true
                ApplicationProgressSeen = ($lastProgressFrame -ge 0L)
                LastProgressFrame = $lastProgressFrame
                LastProgressPhase = $lastProgressPhase
                ElapsedMilliseconds = $elapsedMilliseconds
            }
        }

        if (($elapsedMilliseconds - $lastProgressElapsedMilliseconds) -ge
            $NoProgressTimeoutMilliseconds) {
            throw (
                "Packaged sandbox smoke had no application frame progress for " +
                "$NoProgressTimeoutMilliseconds ms; last application frame was " +
                "seq=$lastProgressFrame phase=$lastProgressPhase.")
        }

        $remainingHardMilliseconds = $HardTimeoutMilliseconds - $elapsedMilliseconds
        $sleepMilliseconds = [Math]::Min($PollMilliseconds, $remainingHardMilliseconds)
        if ($sleepMilliseconds -gt 0) {
            Start-Sleep -Milliseconds $sleepMilliseconds
        }
    }
}
