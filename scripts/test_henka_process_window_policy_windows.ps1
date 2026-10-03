param(
    [string]$RepositoryRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")
. (Join-Path $PSScriptRoot "henka_ui_automation_helpers.ps1")

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
} else {
    $RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
}
$packagedCheckerPath = Join-Path $RepositoryRoot "scripts/check_packaged_sandbox3d_windows.ps1"
$checkerSource = Get-Content -Raw -LiteralPath $packagedCheckerPath

if (-not ("HenkaWindowPolicyNative" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class HenkaWindowPolicyNative
{
    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool IsIconic(IntPtr hWnd);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ShowWindow(IntPtr hWnd, int command);
}
"@
}

$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("henka-process-window-policy-" + $PID)
$fixturePath = Join-Path $fixtureRoot "window-fixture.ps1"
$stdoutPath = Join-Path $fixtureRoot "stdout.log"
$stderrPath = Join-Path $fixtureRoot "stderr.log"
$readyPath = Join-Path $fixtureRoot "ready.txt"
$capturedProcess = $null
$nonActivatingVisibleProcess = $null
$plainProcess = $null
$previousForeground = [HenkaWindowPolicyNative]::GetForegroundWindow()

Assert-HenkaCaptureDimensions `
    -Width 1296 `
    -Height 759 `
    -MinimumWidth 800 `
    -MinimumHeight 600 `
    -Description "Restored 1280x720 validation window"
$tinyCaptureFailure = $null
try {
    Assert-HenkaCaptureDimensions `
        -Width 160 `
        -Height 28 `
        -MinimumWidth 800 `
        -MinimumHeight 600 `
        -Description "Minimized validation window"
} catch {
    $tinyCaptureFailure = $_
}
if ($null -eq $tinyCaptureFailure -or
    $tinyCaptureFailure.Exception.Message -notmatch "below the minimum capture bounds") {
    throw "The capture-bound regression did not reject the minimized 160x28 title-bar image."
}
Write-Output "[pass] Capture bounds reject minimized title-bar-only evidence."

Assert-HenkaCaptureDimensions `
    -Width 436 `
    -Height 339 `
    -MinimumWidth 320 `
    -MinimumHeight 240 `
    -Description "Restored native tool window"
$nativeWindowCallOffset = $checkerSource.IndexOf('-Path $nativeScreenshotPath', [System.StringComparison]::Ordinal)
if ($nativeWindowCallOffset -lt 0) {
    throw "The packaged checker no longer captures the native tool window through the expected path."
}
$nativeWindowCallStart = $checkerSource.LastIndexOf('Save-WindowScreenshot', $nativeWindowCallOffset, [System.StringComparison]::Ordinal)
$nativeWindowCallEnd = $checkerSource.IndexOf('Set-HenkaAutomationForeground -Handle $mainWindowHandle', $nativeWindowCallOffset, [System.StringComparison]::Ordinal)
if ($nativeWindowCallStart -lt 0 -or $nativeWindowCallEnd -le $nativeWindowCallStart) {
    throw "The native tool screenshot call could not be isolated for its capture-size contract."
}
$nativeWindowCall = $checkerSource.Substring($nativeWindowCallStart, $nativeWindowCallEnd - $nativeWindowCallStart)
if ($nativeWindowCall -notmatch '(?m)^\s+-MinimumWidth\s+320\b' -or
    $nativeWindowCall -notmatch '(?m)^\s+-MinimumHeight\s+240\b') {
    throw "The native tool screenshot must use its bounded 320x240 minimum instead of the main-editor capture bound."
}
Write-Output "[pass] Restored native tool windows retain a bounded capture minimum without relaxing minimized-window rejection."

New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null

try {
    $shadingGeometryPath = Join-Path $fixtureRoot "shading-geometry.log"
    Write-HenkaUtf8NoBom -Path $shadingGeometryPath -Text @'
Viewport shading control: mode=Wireframe x=24.0 y=8.0 width=48.0 height=22.0
Viewport shading control: mode=Solid x=75.0 y=8.0 width=60.0 height=22.0
Viewport shading control: mode=Material Preview x=138.0 y=8.0 width=80.0 height=22.0
Viewport shading control: mode=Rendered x=221.0 y=8.0 width=82.0 height=22.0
'@
    $renderedShadingControl = Get-HenkaViewportShadingControl `
        -LogPath $shadingGeometryPath `
        -ModeName "Rendered"
    if ($renderedShadingControl.X -ne 221.0 -or
        $renderedShadingControl.Y -ne 8.0 -or
        $renderedShadingControl.Width -ne 82.0 -or
        $renderedShadingControl.Height -ne 22.0) {
        throw "The exact viewport shading control geometry was not parsed from its per-control record."
    }
    $missingShadingGeometryPath = Join-Path $fixtureRoot "missing-shading-geometry.log"
    Write-HenkaUtf8NoBom -Path $missingShadingGeometryPath -Text @'
Viewport shading control: mode=Wireframe x=24.0 y=8.0 width=48.0 height=22.0
Viewport shading control: mode=Solid x=75.0 y=8.0 width=60.0 height=22.0
'@
    $missingShadingControlFailure = $null
    try {
        $null = Get-HenkaViewportShadingControl `
            -LogPath $missingShadingGeometryPath `
            -ModeName "Rendered"
    } catch {
        $missingShadingControlFailure = $_
    }
    if ($null -eq $missingShadingControlFailure) {
        throw "A missing viewport shading control geometry record was accepted."
    }
    $invalidShadingGeometryPath = Join-Path $fixtureRoot "invalid-shading-geometry.log"
    Write-HenkaUtf8NoBom -Path $invalidShadingGeometryPath -Text `
        "Viewport shading control: mode=Rendered x=-1.0 y=8.0 width=82.0 height=22.0"
    $invalidShadingControlFailure = $null
    try {
        $null = Get-HenkaViewportShadingControl `
            -LogPath $invalidShadingGeometryPath `
            -ModeName "Rendered"
    } catch {
        $invalidShadingControlFailure = $_
    }
    if ($null -eq $invalidShadingControlFailure) {
        throw "Negative viewport shading control geometry was accepted."
    }
    Write-Output "[pass] Shading-mode automation uses actual per-control geometry and rejects missing or invalid bounds."

    $scrollEventsPath = Join-Path $fixtureRoot "scroll-events.txt"
    Send-HenkaAutomationScroll `
        -EventPath $scrollEventsPath `
        -X 24 `
        -Y 36 `
        -WheelDelta -1
    $scrollEvents = @(Get-Content -LiteralPath $scrollEventsPath)
    if ($scrollEvents.Count -ne 2 -or
        $scrollEvents[0] -ne "move 24 36" -or
        $scrollEvents[1] -ne "wheel 0 -1") {
        throw "SDL wheel-step automation did not serialize its pointer and one-step wheel records."
    }
    $win32WheelDeltaFailure = $null
    try {
        Send-HenkaAutomationScroll `
            -EventPath $scrollEventsPath `
            -X 24 `
            -Y 36 `
            -WheelDelta -120
    } catch {
        $win32WheelDeltaFailure = $_
    }
    if ($null -eq $win32WheelDeltaFailure -or
        $win32WheelDeltaFailure.Exception.Message -notmatch "SDL wheel-step units") {
        throw "The SDL wheel automation contract did not reject Win32's 120-unit delta."
    }
    $staleWin32WheelCall = Select-String `
        -LiteralPath $packagedCheckerPath `
        -Pattern '(?m)^\s+-WheelDelta\s+[-+]?120(?:\.0+)?\s*$' |
        Select-Object -First 1
    if ($null -ne $staleWin32WheelCall) {
        throw "The packaged UI checker still passes a Win32 120-unit delta at ${packagedCheckerPath}:$($staleWin32WheelCall.LineNumber)."
    }
    Write-Output "[pass] Automation scroll serializes SDL wheel steps and rejects Win32 120-unit deltas."

    $fixtureText = @'
param([Parameter(Mandatory = $true)][string]$ReadyPath)
Add-Type -AssemblyName System.Windows.Forms
$form = New-Object System.Windows.Forms.Form
$form.Text = "Henka launch policy regression"
$form.Width = 320
$form.Height = 120
$form.StartPosition = "Manual"
$form.add_Shown({
    [System.IO.File]::WriteAllText($ReadyPath, [string]$form.Handle)
})
[System.Windows.Forms.Application]::Run($form)
'@
    Write-HenkaUtf8NoBom -Path $fixturePath -Text $fixtureText

    $plainProcess = Start-HenkaProcess `
        -FilePath "powershell.exe" `
        -Arguments @("-NoProfile", "-Command", "Start-Sleep -Milliseconds 400") `
        -WorkingDirectory $RepositoryRoot
    if ($plainProcess.StartInfo.WindowStyle -ne [System.Diagnostics.ProcessWindowStyle]::Hidden) {
        throw "The ordinary Henka process helper did not select hidden non-activating startup by default."
    }
    $plainProcess.WaitForExit()
    $plainProcess.Dispose()
    $plainProcess = $null

    $capturedProcess = Start-HenkaCapturedProcess `
        -FilePath "powershell.exe" `
        -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $fixturePath,
            "-ReadyPath", $readyPath) `
        -WorkingDirectory $RepositoryRoot `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath

    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    while (-not (Test-Path -LiteralPath $readyPath -PathType Leaf) -and
        -not $capturedProcess.Process.HasExited -and
        [DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 50
    }

    if (-not (Test-Path -LiteralPath $readyPath -PathType Leaf)) {
        $diagnostics = ((Read-HenkaSharedText -Path $stdoutPath) + "`n" +
            (Read-HenkaSharedText -Path $stderrPath)).Trim()
        throw "The controlled window did not report readiness. Diagnostics: $diagnostics"
    }

    # Allow the shared launch policy's bounded startup monitor to finish its
    # first native-window pass before checking the stable user-facing state.
    Start-Sleep -Milliseconds 250
    $capturedProcess.Process.Refresh()
    $handle = $capturedProcess.Process.MainWindowHandle
    if ($handle -eq [IntPtr]::Zero -or
        -not [HenkaWindowPolicyNative]::IsWindowVisible($handle)) {
        throw "The controlled window was not visible after readiness."
    }
    if (-not [HenkaWindowPolicyNative]::IsIconic($handle)) {
        throw "A non-interactive Henka-launched window was not minimized at startup."
    }
    if ($handle -eq [HenkaWindowPolicyNative]::GetForegroundWindow()) {
        throw "A non-interactive Henka-launched window acquired foreground focus."
    }
    if ($previousForeground -ne [IntPtr]::Zero -and
        $previousForeground -eq [HenkaWindowPolicyNative]::GetForegroundWindow()) {
        Write-Output "[pass] Existing foreground window remained unchanged."
    } else {
        Write-Output "[pass] Child window did not acquire foreground focus."
    }

    $visibleReadyPath = Join-Path $fixtureRoot "visible-ready.txt"
    $nonActivatingVisibleProcess = Start-HenkaCapturedProcess `
        -FilePath "powershell.exe" `
        -Arguments @( `
            "-NoProfile", `
            "-ExecutionPolicy", "Bypass", `
            "-File", $fixturePath, `
            "-ReadyPath", $visibleReadyPath) `
        -WorkingDirectory $RepositoryRoot `
        -StdoutPath (Join-Path $fixtureRoot "visible-stdout.log") `
        -StderrPath (Join-Path $fixtureRoot "visible-stderr.log") `
        -StartMinimized:$false `
        -StartVisibleWithoutActivation

    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    while (-not (Test-Path -LiteralPath $visibleReadyPath -PathType Leaf) -and
        -not $nonActivatingVisibleProcess.Process.HasExited -and
        [DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 50
    }
    if (-not (Test-Path -LiteralPath $visibleReadyPath -PathType Leaf)) {
        throw "The non-activating visible control window did not report readiness."
    }
    Start-Sleep -Milliseconds 250
    $nonActivatingVisibleProcess.Process.Refresh()
    $visibleHandle = $nonActivatingVisibleProcess.Process.MainWindowHandle
    if ($visibleHandle -eq [IntPtr]::Zero -or
        -not [HenkaWindowPolicyNative]::IsWindowVisible($visibleHandle) -or
        [HenkaWindowPolicyNative]::IsIconic($visibleHandle)) {
        throw "The explicit non-activating visible window was not shown in a capturable state."
    }
    if ($visibleHandle -eq [HenkaWindowPolicyNative]::GetForegroundWindow()) {
        throw "An explicit non-activating visible window acquired foreground focus."
    }
    [void][HenkaWindowPolicyNative]::ShowWindow($visibleHandle, 6)
    Start-Sleep -Milliseconds 250
    $nonActivatingVisibleProcess.Process.Refresh()
    if (-not [HenkaWindowPolicyNative]::IsIconic($visibleHandle)) {
        throw "The bounded capture monitor undid a user's later minimize action."
    }
    Write-Output "[pass] Shared process policy supports a visible, non-minimized capture window without taking foreground focus."
    Write-Output "[pass] A later user minimize action remains respected by the capture-window policy."
    Write-Output "[pass] Shared Henka process launch policy creates validation windows hidden/minimized by default and supports bounded unfocused capture."
}
finally {
    if ($null -ne $plainProcess) {
        if (-not $plainProcess.HasExited) {
            Stop-HenkaProcessTree -ProcessId $plainProcess.Id
        }
        $plainProcess.Dispose()
    }
    if ($null -ne $capturedProcess) {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
    }
    if ($null -ne $nonActivatingVisibleProcess) {
        Close-HenkaCapturedProcess -CapturedProcess $nonActivatingVisibleProcess
    }
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
