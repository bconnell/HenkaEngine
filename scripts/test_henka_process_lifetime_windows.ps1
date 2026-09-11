param(
    [string]$RepositoryRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
} else {
    $RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
}

$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("henka-process-lifetime-" + $PID)
$childScript = Join-Path $fixtureRoot "child.ps1"
$failureChildScript = Join-Path $fixtureRoot "failure-child.ps1"
$launcherScript = Join-Path $fixtureRoot "launcher.ps1"
$failureLauncherScript = Join-Path $fixtureRoot "failure-launcher.ps1"
$markerPath = Join-Path $fixtureRoot "child-complete.txt"
$failureMarkerPath = Join-Path $fixtureRoot "failure-child-complete.txt"
$failurePidPath = Join-Path $fixtureRoot "failure-child.pid"
New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null

try {
    $childScriptText = @'
param([Parameter(Mandatory = $true)][string]$MarkerPath)
Start-Sleep -Milliseconds 1200
[System.IO.File]::WriteAllText($MarkerPath, "child-complete" + [Environment]::NewLine)
'@
    Write-HenkaUtf8NoBom -Path $childScript -Text $childScriptText

    $failureChildScriptText = @'
param(
    [Parameter(Mandatory = $true)][string]$MarkerPath,
    [Parameter(Mandatory = $true)][string]$PidPath
)
[System.IO.File]::WriteAllText($PidPath, [string]$PID + [Environment]::NewLine)
Write-Output "process-lifetime-stdout"
Write-Error "process-lifetime-stderr"
Start-Sleep -Milliseconds 400
[System.IO.File]::WriteAllText($MarkerPath, "failure-child-complete" + [Environment]::NewLine)
exit 17
'@
    Write-HenkaUtf8NoBom -Path $failureChildScript -Text $failureChildScriptText

    $launcherTemplate = @'
$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = "cmd.exe"
$startInfo.Arguments = '/c start "" /b powershell.exe -NoProfile -ExecutionPolicy Bypass -File "__CHILD_SCRIPT__" -MarkerPath "__MARKER_PATH__"'
$startInfo.WorkingDirectory = (Get-Location).Path
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$child = [System.Diagnostics.Process]::Start($startInfo)
if ($null -eq $child) {
    throw "The controlled child process did not start."
}
$child.Dispose()
'@
    $launcherScriptText = $launcherTemplate.Replace("__CHILD_SCRIPT__", $childScript).Replace("__MARKER_PATH__", $markerPath)
    Write-HenkaUtf8NoBom -Path $launcherScript -Text $launcherScriptText

    $failureLauncherTemplate = @'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File "__FAILURE_CHILD_SCRIPT__" -MarkerPath "__FAILURE_MARKER_PATH__" -PidPath "__FAILURE_PID_PATH__"
exit $LASTEXITCODE
'@
    $failureLauncherScriptText = $failureLauncherTemplate.Replace(
        "__FAILURE_CHILD_SCRIPT__", $failureChildScript).Replace(
        "__FAILURE_MARKER_PATH__", $failureMarkerPath).Replace(
        "__FAILURE_PID_PATH__", $failurePidPath)
    Write-HenkaUtf8NoBom -Path $failureLauncherScript -Text $failureLauncherScriptText

    $waitStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Invoke-HenkaNative `
        -FilePath "powershell.exe" `
        -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $launcherScript) `
        -WorkingDirectory $RepositoryRoot `
        -Label "Run owned child process lifetime regression"
    $waitStopwatch.Stop()

    if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf) -or
        $waitStopwatch.ElapsedMilliseconds -lt 900) {
        throw "The process authority returned before its owned child completed (elapsed=$($waitStopwatch.ElapsedMilliseconds)ms)."
    }

    Write-Output "[pass] Native process authority waited for the owned child process before returning (elapsed=$($waitStopwatch.ElapsedMilliseconds)ms)."

    $failureResult = Invoke-HenkaExpectedFailure `
        -FilePath "powershell.exe" `
        -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $failureLauncherScript) `
        -WorkingDirectory $RepositoryRoot `
        -Label "Run owned child expected-failure capture regression" `
        -ReturnOutput
    if ($failureResult.ExitCode -ne 17) {
        throw "The expected-failure process did not propagate the child exit code: $($failureResult.ExitCode)."
    }
    if ($failureResult.Stdout -notmatch "process-lifetime-stdout" -or
        $failureResult.Stderr -notmatch "process-lifetime-stderr") {
        throw "The expected-failure process did not preserve child stdout/stderr capture."
    }
    if (-not (Test-Path -LiteralPath $failureMarkerPath -PathType Leaf) -or
        -not (Test-Path -LiteralPath $failurePidPath -PathType Leaf)) {
        throw "The expected-failure child did not finish its bounded work before return."
    }
    $failurePid = [int](Get-Content -LiteralPath $failurePidPath -Raw).Trim()
    if (Get-Process -Id $failurePid -ErrorAction SilentlyContinue) {
        throw "The expected-failure child process remained alive after the owned command returned: $failurePid."
    }
    Write-Output "[pass] Expected child failure propagated exit code and stdout/stderr capture without an orphan (exit=17)."
} finally {
    $cleanupDeadline = [DateTime]::UtcNow.AddSeconds(5)
    while (-not (Test-Path -LiteralPath $markerPath -PathType Leaf) -and [DateTime]::UtcNow -lt $cleanupDeadline) {
        Start-Sleep -Milliseconds 50
    }
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
