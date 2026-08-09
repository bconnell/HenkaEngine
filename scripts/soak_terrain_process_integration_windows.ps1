param(
    [ValidateRange(1, 16)]
    [int]$Iterations = 3
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$integrationScript = Join-Path $PSScriptRoot "test_terrain_process_integration_windows.ps1"

if (-not (Test-Path -LiteralPath $integrationScript -PathType Leaf)) {
    throw "The Terrain process integration script is missing: $integrationScript"
}

for ($iteration = 1; $iteration -le $Iterations; ++$iteration) {
    Write-Host "==> Terrain process integration soak iteration $iteration/$Iterations"
    & $integrationScript
}

Write-Host "[pass] Terrain process integration soak completed: $Iterations bounded sessions proved authority, stale-edit rejection, late join, reconnect, checksum convergence, and restart recovery."
