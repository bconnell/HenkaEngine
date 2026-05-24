$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$packageRoot = Join-Path $repoRoot "out\HenkaSandbox3D"
$packagedExe = Join-Path $packageRoot "HenkaSandbox3D.exe"

if (-not (Test-Path $packagedExe)) {
    throw "The packaged sandbox was not found. Run .\scripts\package_sandbox3d_windows.ps1 first."
}

Push-Location $packageRoot
try {
    Start-Process -FilePath ".\HenkaSandbox3D.exe" -WorkingDirectory $packageRoot
    Write-Host "Launched packaged sandbox:"
    Write-Host "  $packagedExe"
}
finally {
    Pop-Location
}
