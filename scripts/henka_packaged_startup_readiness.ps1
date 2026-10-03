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

function Get-HenkaPackagedStartupLogTextAfterOffset {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][long]$StartingOffset
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [string]::Empty
    }

    $stream = $null
    $reader = $null
    try {
        $shareMode = [System.IO.FileShare](
            [int][System.IO.FileShare]::ReadWrite -bor
            [int][System.IO.FileShare]::Delete)
        $stream = [System.IO.FileStream]::new(
            $Path,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            $shareMode)
        if ($StartingOffset -lt 0 -or $StartingOffset -gt $stream.Length) {
            return [string]::Empty
        }

        $stream.Seek($StartingOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
        $reader = [System.IO.StreamReader]::new(
            $stream,
            [System.Text.Encoding]::UTF8,
            $true,
            4096,
            $false)
        return $reader.ReadToEnd()
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
}

function Wait-HenkaPackagedFrameRenderComplete {
    param(
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][int]$ProcessId,
        [Parameter(Mandatory = $true)][long]$StartingOffset,
        [Parameter(Mandatory = $true)][long]$AfterFrameSequence,
        [ValidateRange(1, 120000)][int]$HardTimeoutMilliseconds = 30000,
        [ValidateRange(1, 120000)][int]$NoProgressTimeoutMilliseconds = 8000,
        [ValidateRange(1, 5000)][int]$PollMilliseconds = 150
    )

    $effectiveNoProgressTimeout = [Math]::Min(
        $NoProgressTimeoutMilliseconds,
        $HardTimeoutMilliseconds)
    $phaseRank = @{
        "loop-begin" = 1
        "events-polled" = 2
        "update-complete" = 3
        "render-begin" = 4
        "render-complete" = 5
    }
    $lastSequence = $AfterFrameSequence
    $lastPhaseRank = 0
    $lastProgressStage = "waiting for the next application frame"
    $lastProgressMilliseconds = 0L
    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    $framePattern = [System.Text.RegularExpressions.Regex]::new(
        '(?m)^HENKA_AUTOMATION_DIAGNOSTIC frame seq=(?<sequence>[0-9]+) phase=(?<phase>loop-begin|events-polled|update-complete|render-begin|render-complete)(?:\s[^\r\n]*)?$')

    while ($true) {
        $elapsedMilliseconds = [long]$clock.ElapsedMilliseconds
        $logText = Get-HenkaPackagedStartupLogTextAfterOffset `
            -Path $StdoutPath `
            -StartingOffset $StartingOffset

        foreach ($match in $framePattern.Matches($logText)) {
            $sequence = [long]$match.Groups['sequence'].Value
            $phase = $match.Groups['phase'].Value
            $rank = [int]$phaseRank[$phase]
            if ($sequence -le $AfterFrameSequence -or
                ($sequence -lt $lastSequence) -or
                ($sequence -eq $lastSequence -and $rank -le $lastPhaseRank)) {
                continue
            }

            if ($sequence -gt $lastSequence) {
                $lastSequence = $sequence
                $lastPhaseRank = 0
            }
            $lastPhaseRank = $rank
            $lastProgressStage = "frame $sequence $phase"
            $lastProgressMilliseconds = $elapsedMilliseconds

            if ($phase -eq "render-complete" -and $sequence -gt $AfterFrameSequence) {
                return [pscustomobject]@{
                    Ready = $true
                    FrameSequence = $sequence
                    LastProgressStage = $lastProgressStage
                    ElapsedMilliseconds = $elapsedMilliseconds
                }
            }
        }

        if (-not (Test-HenkaPackagedStartupProcessAlive -ProcessId $ProcessId)) {
            throw (
                "Packaged sandbox exited before a post-transition render completed; " +
                "last application frame stage was '$lastProgressStage'.")
        }

        if ($elapsedMilliseconds -ge $HardTimeoutMilliseconds) {
            throw (
                "Packaged viewport render exceeded the hard frame-render limit of " +
                "$HardTimeoutMilliseconds ms; last application frame stage was " +
                "'$lastProgressStage'.")
        }

        if (($elapsedMilliseconds - $lastProgressMilliseconds) -ge $effectiveNoProgressTimeout) {
            throw (
                "Packaged viewport render made no application frame progress for " +
                "$effectiveNoProgressTimeout ms; last application frame stage was " +
                "'$lastProgressStage'.")
        }

        $remainingHardMilliseconds = $HardTimeoutMilliseconds - $elapsedMilliseconds
        $sleepMilliseconds = [Math]::Min($PollMilliseconds, $remainingHardMilliseconds)
        if ($sleepMilliseconds -gt 0) {
            Start-Sleep -Milliseconds $sleepMilliseconds
        }
    }
}

function Wait-HenkaPackagedStartupReady {
    param(
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$StderrPath,
        [Parameter(Mandatory = $true)][int]$ProcessId,
        [ValidateRange(1, 3600000)][int]$HardTimeoutMilliseconds = 120000,
        [ValidateRange(1, 3600000)][int]$NoProgressTimeoutMilliseconds = 45000,
        [ValidateRange(1, 5000)][int]$PollMilliseconds = 150,
        [switch]$RequireNativeAuthoringRestore
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
    if ($RequireNativeAuthoringRestore) {
        $stages += @(
            [pscustomobject]@{
                Name = "native authoring topology bridge"
                Pattern = 'Native authoring topology bridge: name=.+source_state=HENKA_NATIVE_EDITABLE_SOURCE\.'
                Source = "stdout"
            },
            [pscustomobject]@{
                Name = "persisted native material state"
                Pattern = 'Native authoring startup restore: material state restored .+pbr_state=restored(?:\s|$)'
                Source = "stdout"
            },
            [pscustomobject]@{
                Name = "persisted native authoring source"
                Pattern = 'Native authoring startup restore: name=.+source_state=HENKA_NATIVE_EDITABLE_SOURCE\.'
                Source = "stdout"
            }
        )
    }
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

        if ($elapsedMilliseconds -ge $HardTimeoutMilliseconds) {
            throw (
                "Packaged startup exceeded the hard startup readiness limit of " +
                "$HardTimeoutMilliseconds ms; last application stage was " +
                "'$lastProgressStage'.")
        }

        if ($stageIndex -eq $stages.Count) {
            return [pscustomobject]@{
                Ready = $true
                LastProgressStage = $lastProgressStage
                ProgressStagesObserved = $stageIndex
                ElapsedMilliseconds = $elapsedMilliseconds
            }
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

function Wait-HenkaPackagedNativeAuthoringRestore {
    param(
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$StderrPath,
        [Parameter(Mandatory = $true)][int]$ProcessId,
        [ValidateRange(1, 3600000)][int]$HardTimeoutMilliseconds = 120000,
        [ValidateRange(1, 3600000)][int]$NoProgressTimeoutMilliseconds = 45000,
        [ValidateRange(1, 5000)][int]$PollMilliseconds = 150
    )

    return Wait-HenkaPackagedStartupReady `
        -StdoutPath $StdoutPath `
        -StderrPath $StderrPath `
        -ProcessId $ProcessId `
        -HardTimeoutMilliseconds $HardTimeoutMilliseconds `
        -NoProgressTimeoutMilliseconds $NoProgressTimeoutMilliseconds `
        -PollMilliseconds $PollMilliseconds `
        -RequireNativeAuthoringRestore
}
