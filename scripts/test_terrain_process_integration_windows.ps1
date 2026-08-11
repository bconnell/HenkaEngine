Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$cmake = Get-HenkaCMakePath
$serverExe = Join-Path $repoRoot "build\examples\dedicated_server\Debug\henka_dedicated_server.exe"
$clientExe = Join-Path $repoRoot "build\tests\Debug\henka_terrain_process_client.exe"
$outRoot = Join-Path $repoRoot "out"
$evidenceRoot = Join-Path $outRoot "terrain-process-integration"
$saveRoot = Join-Path $evidenceRoot "save"

[System.IO.Directory]::CreateDirectory($outRoot) | Out-Null
function Remove-GeneratedIntegrationTree {
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $outPath = [System.IO.Path]::GetFullPath($outRoot)
    $stableRoot = [System.IO.Path]::Combine($outPath, "terrain-process-integration")
    $leaf = [System.IO.Path]::GetFileName($fullPath)
    if ($fullPath -ne $stableRoot -and
        -not ($fullPath.StartsWith($outPath + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -and
            $leaf -match '^terrain-process-integration-[0-9]{8}_[0-9]{6}$')) {
        throw "Refusing to remove a non-owned Terrain integration output: $fullPath"
    }
    if (Test-Path -LiteralPath $fullPath) {
        Remove-Item -LiteralPath $fullPath -Recurse -Force -ErrorAction Stop
    }
}

$legacyEvidenceRoots = @(
    Get-ChildItem -LiteralPath $outRoot -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^terrain-process-integration-[0-9]{8}_[0-9]{6}$' }
)
foreach ($legacyRoot in $legacyEvidenceRoots) {
    Remove-GeneratedIntegrationTree -Path $legacyRoot.FullName
}
if ($legacyEvidenceRoots.Count -gt 0) {
    Write-Host "Retired $($legacyEvidenceRoots.Count) superseded Terrain integration output(s)."
}
Remove-GeneratedIntegrationTree -Path $evidenceRoot
[System.IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null
Invoke-HenkaNative -FilePath $cmake -Arguments @(
    "--build", (Join-Path $repoRoot "build"), "--config", "Debug",
    "--target", "henka_dedicated_server", "henka_terrain_process_client", "--parallel", "8"
) -WorkingDirectory $repoRoot -Label "Build process Terrain integration targets" -TimeoutMilliseconds 600000

if (-not (Test-Path -LiteralPath $serverExe -PathType Leaf) -or
    -not (Test-Path -LiteralPath $clientExe -PathType Leaf)) {
    throw "Process Terrain integration executables were not produced."
}

function Wait-HenkaCaptured {
    param(
        [Parameter(Mandatory = $true)] $Process,
        [Parameter(Mandatory = $true)] [int]$TimeoutMilliseconds,
        [Parameter(Mandatory = $true)] [string]$Label
    )
    if (-not $Process.WaitForExit($TimeoutMilliseconds)) {
        Stop-HenkaProcessTree -ProcessId $Process.Process.Id
        throw "$Label exceeded its timeout."
    }
    if ($Process.Process.ExitCode -ne 0) {
        throw "$Label failed with exit code $($Process.Process.ExitCode)."
    }
}

function Start-ProcessClient {
    param(
        [Parameter(Mandatory = $true)] [string]$Name,
        [Parameter(Mandatory = $true)] [string]$Port,
        [Parameter(Mandatory = $true)] [string]$DelayMilliseconds,
        [Parameter(Mandatory = $true)] [string]$Nonce,
        [Parameter(Mandatory = $false)] [string]$Mode = "edit"
    )
    return Start-HenkaCapturedProcess `
        -FilePath $clientExe `
        -Arguments @($Port, $DelayMilliseconds, $Nonce, $Mode) `
        -WorkingDirectory $repoRoot `
        -StdoutPath (Join-Path $evidenceRoot ($Name + ".out.txt")) `
        -StderrPath (Join-Path $evidenceRoot ($Name + ".err.txt")) `
        -CreateNoWindow
}

$server = $null
$clientA = $null
$clientB = $null
$lateClient = $null
$reconnectClient = $null
$restartServer = $null
$restartClient = $null
try {
    $server = Start-HenkaCapturedProcess -FilePath $serverExe -Arguments @(
        "--bind", "127.0.0.1", "--port", "7831", "--max-clients", "8",
        "--save-root", $saveRoot, "--run-for-ms", "60000"
    ) -WorkingDirectory $repoRoot -StdoutPath (Join-Path $evidenceRoot "server.out.txt") -StderrPath (Join-Path $evidenceRoot "server.err.txt") -CreateNoWindow
    Start-Sleep -Milliseconds 500
    $clientA = Start-ProcessClient -Name "client_a" -Port "7831" -DelayMilliseconds "250" -Nonce "1001"
    $clientB = Start-ProcessClient -Name "client_b" -Port "7831" -DelayMilliseconds "0" -Nonce "1002"
    Wait-HenkaCaptured -Process $clientA -TimeoutMilliseconds 15000 -Label "Terrain process client A"
    Wait-HenkaCaptured -Process $clientB -TimeoutMilliseconds 15000 -Label "Terrain process client B"
    Close-HenkaCapturedProcess $clientA; $clientA = $null
    Close-HenkaCapturedProcess $clientB; $clientB = $null

    $firstOutput = (Get-Content -LiteralPath (Join-Path $evidenceRoot "client_a.out.txt") -Raw) +
        (Get-Content -LiteralPath (Join-Path $evidenceRoot "client_b.out.txt") -Raw)
    if ($firstOutput -notmatch "terrain process client connected=" -or
        $firstOutput -notmatch "accepted=1" -or $firstOutput -notmatch "rejected=1") {
        throw "The two independent clients did not prove one accepted and one stale Terrain edit."
    }

    $acceptedOutput = @(
        (Get-Content -LiteralPath (Join-Path $evidenceRoot "client_a.out.txt") -Raw),
        (Get-Content -LiteralPath (Join-Path $evidenceRoot "client_b.out.txt") -Raw)
    ) | Where-Object { $_ -match "accepted=1" } | Select-Object -First 1
    $acceptedChecksumMatch = [Regex]::Match($acceptedOutput, "checksum=([0-9]+)")
    $acceptedRevisionMatch = [Regex]::Match($acceptedOutput, "revision=([0-9]+)")
    if (-not $acceptedChecksumMatch.Success -or -not $acceptedRevisionMatch.Success -or
        [uint64]$acceptedRevisionMatch.Groups[1].Value -eq 0) {
        throw "The accepted Terrain process result did not report the expected committed revision and checksum."
    }
    $acceptedRevision = [uint64]$acceptedRevisionMatch.Groups[1].Value

    $lateClient = Start-ProcessClient -Name "late_client" -Port "7831" -DelayMilliseconds "0" -Nonce "1003" -Mode "observe"
    Wait-HenkaCaptured -Process $lateClient -TimeoutMilliseconds 15000 -Label "Terrain late-join client"
    Close-HenkaCapturedProcess $lateClient; $lateClient = $null
    $lateOutput = Get-Content -LiteralPath (Join-Path $evidenceRoot "late_client.out.txt") -Raw
    $lateChecksumMatch = [Regex]::Match($lateOutput, "checksum=([0-9]+)")
    if ($lateOutput -notmatch "mode=observe" -or $lateOutput -notmatch "snapshots=1" -or
        $lateOutput -notmatch ("revision=" + $acceptedRevision) -or -not $lateChecksumMatch.Success -or
        $lateChecksumMatch.Groups[1].Value -ne $acceptedChecksumMatch.Groups[1].Value) {
        throw "The late-join Terrain client did not converge to the accepted resident region."
    }

    $reconnectClient = Start-ProcessClient -Name "reconnect_client" -Port "7831" -DelayMilliseconds "0" -Nonce "1004" -Mode "reconnect"
    Wait-HenkaCaptured -Process $reconnectClient -TimeoutMilliseconds 15000 -Label "Terrain reconnect client"
    Close-HenkaCapturedProcess $reconnectClient; $reconnectClient = $null
    $reconnectOutput = Get-Content -LiteralPath (Join-Path $evidenceRoot "reconnect_client.out.txt") -Raw
    if ($reconnectOutput -notmatch "mode=reconnect" -or
        $reconnectOutput -notmatch "connected=2" -or
        $reconnectOutput -notmatch "accepted=1" -or
        $reconnectOutput -notmatch ("revision=" + ($acceptedRevision + 1))) {
        throw "The reconnecting Terrain client did not complete a fresh connection after its accepted edit."
    }
    $reconnectChecksumMatch = [Regex]::Match($reconnectOutput, "checksum=([0-9]+)")
    if (-not $reconnectChecksumMatch.Success) {
        throw "The reconnecting Terrain client did not report a resident checksum."
    }
    Wait-HenkaCaptured -Process $server -TimeoutMilliseconds 70000 -Label "dedicated Terrain server process"
    Close-HenkaCapturedProcess $server; $server = $null

    $restartServer = Start-HenkaCapturedProcess -FilePath $serverExe -Arguments @(
        "--bind", "127.0.0.1", "--port", "7832", "--max-clients", "8",
        "--save-root", $saveRoot, "--run-for-ms", "7000"
    ) -WorkingDirectory $repoRoot -StdoutPath (Join-Path $evidenceRoot "restart_server.out.txt") -StderrPath (Join-Path $evidenceRoot "restart_server.err.txt") -CreateNoWindow
    Start-Sleep -Milliseconds 500
    $restartClient = Start-ProcessClient -Name "restart_client" -Port "7832" -DelayMilliseconds "0" -Nonce "1005" -Mode "observe"
    Wait-HenkaCaptured -Process $restartClient -TimeoutMilliseconds 12000 -Label "Terrain restart client"
    Wait-HenkaCaptured -Process $restartServer -TimeoutMilliseconds 12000 -Label "Terrain restart server"
    Close-HenkaCapturedProcess $restartClient; $restartClient = $null
    Close-HenkaCapturedProcess $restartServer; $restartServer = $null

    $restartOutput = Get-Content -LiteralPath (Join-Path $evidenceRoot "restart_client.out.txt") -Raw
    $restartChecksumMatch = [Regex]::Match($restartOutput, "checksum=([0-9]+)")
    if ($restartOutput -notmatch "mode=observe" -or
        $restartOutput -notmatch "snapshots=1" -or
        $restartOutput -notmatch ("revision=" + ($acceptedRevision + 1)) -or
        -not $restartChecksumMatch.Success -or
        $restartChecksumMatch.Groups[1].Value -ne $reconnectChecksumMatch.Groups[1].Value) {
        throw "The restarted Terrain server did not restore the reconnect client's committed region exactly."
    }
    Write-Host "[pass] Two-client authority, late join, explicit reconnect, exact checksum convergence, clean duration shutdown, and restart persistence passed."
    Write-Host "Evidence: $evidenceRoot"
}
finally {
    foreach ($process in @($clientA, $clientB, $lateClient, $reconnectClient, $server, $restartClient, $restartServer)) {
        if ($null -ne $process) { try { Close-HenkaCapturedProcess $process } catch { } }
    }
}
