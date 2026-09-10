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
$launcherScript = Join-Path $fixtureRoot "launcher.ps1"
$markerPath = Join-Path $fixtureRoot "child-complete.txt"
New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null

try {
    $childScriptText = @'
param([Parameter(Mandatory = $true)][string]$MarkerPath)
Start-Sleep -Milliseconds 1200
[System.IO.File]::WriteAllText($MarkerPath, "child-complete" + [Environment]::NewLine)
'@
    Write-HenkaUtf8NoBom -Path $childScript -Text $childScriptText

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
} finally {
    $cleanupDeadline = [DateTime]::UtcNow.AddSeconds(5)
    while (-not (Test-Path -LiteralPath $markerPath -PathType Leaf) -and [DateTime]::UtcNow -lt $cleanupDeadline) {
        Start-Sleep -Milliseconds 50
    }
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
