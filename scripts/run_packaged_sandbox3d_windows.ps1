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
    Write-Host "The in-window panels open automatically; press F4 to hide or show them."
    Write-Host "Starts should show no selected scene object until you select one."
    Write-Host "Open Physics QA from Controls for body-type notes and quick drop tests."
    Write-Host "Press F5 to cycle View, Inspect, and Full Tools."
    Write-Host "The scene should render inside its own docked viewport when panels are visible."
    Write-Host "Select an object in the viewport or Scene Objects panel, then use Select, Move, Rotate, and Scale in the Transform section."
    Write-Host "Use M or G, R, and S for action-based transforms. X/Y/Z constrain, Enter applies, and Escape cancels."
    Write-Host "Release panels on valid left or right dock outlines to redock, or away from outlines to open a separate native tool window."
    Write-Host "Ground selection uses one bounded Scene View highlight, and viewport overlays should not draw over panels."
    Write-Host "Use the in-window utilities for help, legend, paths, settings, and diagnostics."
    Write-Host "Use Open Native Panel Test in Controls to exercise the separate-window foundation."
    Write-Host "Watch the in-window status area for recent actions and warnings."
    Write-Host "The packaged runtime should report Packaged mode at startup."
}
finally {
    Pop-Location
}
