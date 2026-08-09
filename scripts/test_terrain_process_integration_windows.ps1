Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$cmake = Get-HenkaCMakePath
$serverExe = Join-Path $repoRoot "build\examples\dedicated_server\Debug\henka_dedicated_server.exe"
$clientExe = Join-Path $repoRoot "build\tests\Debug\henka_terrain_process_client.exe"
$evidenceRoot = Join-Path $repoRoot ("out\terrain-process-integration-" + (Get-Date -Format "yyyyMMdd_HHmmss"))
$saveRoot = Join-Path $evidenceRoot "save"

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
        [Parameter(Mandatory = $true)] [string]$Nonce
    )
    return Start-HenkaCapturedProcess `
        -FilePath $clientExe `
        -Arguments @($Port, $DelayMilliseconds, $Nonce) `
        -WorkingDirectory $repoRoot `
        -StdoutPath (Join-Path $evidenceRoot ($Name + ".out.txt")) `
        -StderrPath (Join-Path $evidenceRoot ($Name + ".err.txt")) `
        -CreateNoWindow
}

$server = $null
$clientA = $null
$clientB = $null
$restartServer = $null
$restartClient = $null
try {
    $server = Start-HenkaCapturedProcess -FilePath $serverExe -Arguments @(
        "--bind", "127.0.0.1", "--port", "7831", "--max-clients", "4",
        "--save-root", $saveRoot, "--run-for-ms", "15000"
    ) -WorkingDirectory $repoRoot -StdoutPath (Join-Path $evidenceRoot "server.out.txt") -StderrPath (Join-Path $evidenceRoot "server.err.txt") -CreateNoWindow
    Start-Sleep -Milliseconds 500
    $clientA = Start-ProcessClient -Name "client_a" -Port "7831" -DelayMilliseconds "250" -Nonce "1001"
    $clientB = Start-ProcessClient -Name "client_b" -Port "7831" -DelayMilliseconds "0" -Nonce "1002"
    Wait-HenkaCaptured -Process $clientA -TimeoutMilliseconds 12000 -Label "Terrain process client A"
    Wait-HenkaCaptured -Process $clientB -TimeoutMilliseconds 12000 -Label "Terrain process client B"
    Wait-HenkaCaptured -Process $server -TimeoutMilliseconds 20000 -Label "dedicated Terrain server process"
    Close-HenkaCapturedProcess $clientA; $clientA = $null
    Close-HenkaCapturedProcess $clientB; $clientB = $null
    Close-HenkaCapturedProcess $server; $server = $null

    $firstOutput = (Get-Content -LiteralPath (Join-Path $evidenceRoot "client_a.out.txt") -Raw) +
        (Get-Content -LiteralPath (Join-Path $evidenceRoot "client_b.out.txt") -Raw)
    if ($firstOutput -notmatch "terrain process client connected snapshots=1" -or
        $firstOutput -notmatch "accepted=1" -or $firstOutput -notmatch "rejected=1") {
        throw "The two independent clients did not prove one accepted and one stale Terrain edit."
    }

    $restartServer = Start-HenkaCapturedProcess -FilePath $serverExe -Arguments @(
        "--bind", "127.0.0.1", "--port", "7832", "--max-clients", "4",
        "--save-root", $saveRoot, "--run-for-ms", "7000"
    ) -WorkingDirectory $repoRoot -StdoutPath (Join-Path $evidenceRoot "restart_server.out.txt") -StderrPath (Join-Path $evidenceRoot "restart_server.err.txt") -CreateNoWindow
    Start-Sleep -Milliseconds 500
    $restartClient = Start-ProcessClient -Name "restart_client" -Port "7832" -DelayMilliseconds "0" -Nonce "1003"
    Wait-HenkaCaptured -Process $restartClient -TimeoutMilliseconds 12000 -Label "Terrain restart client"
    Wait-HenkaCaptured -Process $restartServer -TimeoutMilliseconds 12000 -Label "Terrain restart server"
    Close-HenkaCapturedProcess $restartClient; $restartClient = $null
    Close-HenkaCapturedProcess $restartServer; $restartServer = $null

    $restartOutput = Get-Content -LiteralPath (Join-Path $evidenceRoot "restart_client.out.txt") -Raw
    if ($restartOutput -notmatch "terrain process client connected snapshots=1" -or
        $restartOutput -notmatch "accepted=1" -or $restartOutput -notmatch "revision=2") {
        throw "The restarted Terrain server did not restore and accept from the committed region state."
    }
    Write-Host "[pass] Two-process Terrain authority, stale edit rejection, clean duration shutdown, and restart persistence passed."
    Write-Host "Evidence: $evidenceRoot"
}
finally {
    foreach ($process in @($clientA, $clientB, $server, $restartClient, $restartServer)) {
        if ($null -ne $process) { try { Close-HenkaCapturedProcess $process } catch { } }
    }
}
