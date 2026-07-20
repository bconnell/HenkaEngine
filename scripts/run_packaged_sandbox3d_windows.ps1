Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$packageRoot = Join-Path $repoRoot "out\HenkaSandbox3D"
$packagedExe = Join-Path $packageRoot "HenkaSandbox3D.exe"
$packageInfoPath = Join-Path $packageRoot "PACKAGE_INFO.txt"

if (-not (Test-Path -LiteralPath $packagedExe -PathType Leaf) -or
    -not (Test-Path -LiteralPath $packageInfoPath -PathType Leaf)) {
    throw "The packaged sandbox is incomplete. Run .\scripts\package_sandbox3d_windows.ps1 first."
}

$hashMatch = Select-String `
    -LiteralPath $packageInfoPath `
    -Pattern '^Packaged executable SHA-256:\s*([0-9a-f]{64})$' |
    Select-Object -First 1
if ($null -eq $hashMatch) {
    throw "The package marker does not contain a valid executable hash."
}
$expectedHash = $hashMatch.Matches[0].Groups[1].Value
$actualHash = (Get-FileHash -LiteralPath $packagedExe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($expectedHash -ne $actualHash) {
    throw "The packaged executable hash does not match PACKAGE_INFO.txt."
}

$process = Start-HenkaProcess -FilePath $packagedExe -WorkingDirectory $packageRoot
Write-Host "Launched packaged sandbox:"
Write-Host "  $packagedExe"
Write-Host "Process ID: $($process.Id)"
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
