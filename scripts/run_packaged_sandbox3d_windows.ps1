$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$packageRoot = Join-Path $repoRoot "out\HenkaSandbox3D"
$packagedExe = Join-Path $packageRoot "HenkaSandbox3D.exe"

if (-not (Test-Path $packagedExe)) {
    throw "The packaged sandbox was not found. Run .\scripts\package_sandbox3d_windows.ps1 first."
}

Push-Location $packageRoot
try {
    Start-Process -FilePath $packagedExe -WorkingDirectory $packageRoot
    Write-Host "Launched packaged sandbox:"
    Write-Host "  $packagedExe"
    Write-Host "Press F4 to open the in-window panels."
    Write-Host "Press F5 to cycle View, Inspect, and Full Tools."
    Write-Host "Use the in-window utilities for help, legend, paths, settings, and diagnostics."
}
finally {
    Pop-Location
}
