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

$virtualLeft = [HenkaUiAutomationNative]::GetSystemMetrics(76)
$virtualTop = [HenkaUiAutomationNative]::GetSystemMetrics(77)
$virtualWidth = [HenkaUiAutomationNative]::GetSystemMetrics(78)
$virtualHeight = [HenkaUiAutomationNative]::GetSystemMetrics(79)
if ($virtualWidth -le 0 -or $virtualHeight -le 0) {
    throw "The virtual desktop bounds were not available for the background-window regression."
}
$onScreenRectangleRejected = -not (Test-HenkaAutomationRectOutsideVirtualScreen `
    -X ($virtualLeft + 8) -Y ($virtualTop + 8) -Width 320 -Height 120)
$offScreenRectangleAccepted = Test-HenkaAutomationRectOutsideVirtualScreen `
    -X ($virtualLeft + $virtualWidth + 64) `
    -Y ($virtualTop + 8) `
    -Width 320 `
    -Height 120
if (-not $onScreenRectangleRejected -or -not $offScreenRectangleAccepted) {
    throw "The desktop-overlap predicate did not distinguish visible desktop geometry from a fully off-screen capture window."
}
Write-Output "[pass] Window coverage geometry rejects an on-screen rectangle and accepts a fully off-screen rectangle."

function Assert-HenkaPackagedCheckerDoesNotActivateWindows {
    param([Parameter(Mandatory = $true)][string]$Source)

    if ($Source -match '(?im)\b(?:Set-HenkaAutomationForeground|SetForegroundWindow|BringWindowToTop|SwitchToThisWindow|AttachThreadInput|AppActivate)\s*(?:\(|\b)' -or
        $Source -match '(?im)\b(?:CopyFromScreen|PrintWindow)\s*\(' -or
        $Source -match '(?im)AllowForegroundIntegration') {
        throw "The ordinary packaged checker contains a foreground activation or desktop-capture path."
    }
}

function Assert-HenkaLauncherDoesNotRestoreForeground {
    param([Parameter(Mandatory = $true)][string]$Source)

    if ($Source -match '(?im)\b(?:SetForegroundWindow|BringWindowToTop|SwitchToThisWindow|AttachThreadInput|AppActivate)\s*(?:\(|\b)' -or
        $Source -match '(?im)CurrentForegroundWindow|previousForeground') {
        throw "The shared Henka launcher can reassert foreground ownership during background validation."
    }
}

function Assert-HenkaAutomationWorkflowUsesBackgroundCapture {
    param([Parameter(Mandatory = $true)][string]$Source)

    if ($Source -match '(?im)\b(?:Set-HenkaAutomationForeground|SetForegroundWindow|BringWindowToTop|SwitchToThisWindow|AttachThreadInput|AppActivate|CopyFromScreen)\s*(?:\(|\b)') {
        throw "A Henka automation workflow can activate a window or capture the owner's desktop."
    }
    if ($Source -notmatch '(?im)-StartOffscreenWithoutActivation\b' -or
        $Source -notmatch '(?im)HENKA_AUTOMATION_START_HIDDEN') {
        throw "A visible Henka automation workflow must start hidden and use the shared off-screen, non-activating launch path."
    }
}

$packagedCheckerSource = [System.IO.File]::ReadAllText(
    (Join-Path $RepositoryRoot "scripts\check_packaged_sandbox3d_windows.ps1"))

$packagedInteropMatch = [regex]::Match(
    $packagedCheckerSource,
    "(?s)Add-Type\s+@'\r?\n(?<definition>.*?)\r?\n'@")
if (-not $packagedInteropMatch.Success) {
    throw "The packaged checker Win32 interop declaration could not be located."
}
try {
    Add-Type -TypeDefinition $packagedInteropMatch.Groups["definition"].Value -ErrorAction Stop
}
catch {
    throw "The packaged checker Win32 interop declaration does not compile: $($_.Exception.Message)"
}

$duplicateImportProbe = @'
using System;
using System.Runtime.InteropServices;
public static class HenkaDuplicateDllImportNegativeControl
{
    [DllImport("user32.dll")]
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr handle, out int rect);
}
'@
$duplicateImportNegativeControlRejected = $false
try {
    Add-Type -TypeDefinition $duplicateImportProbe -ErrorAction Stop
}
catch {
    $duplicateImportNegativeControlRejected = $true
}
if (-not $duplicateImportNegativeControlRejected) {
    throw "The native interop regression did not reject duplicate DllImport attributes."
}
Write-Output "[pass] Packaged checker interop compiles and duplicate-import negative control is rejected."

Assert-HenkaPackagedCheckerDoesNotActivateWindows -Source $packagedCheckerSource
$activationNegativeControlRejected = $false
try {
    Assert-HenkaPackagedCheckerDoesNotActivateWindows `
        -Source 'Send-HenkaAutomationForeground($handle); $shell.AppActivate($process.Id)'
}
catch {
    $activationNegativeControlRejected = $true
}
if (-not $activationNegativeControlRejected) {
    throw "The packaged foreground-activation negative control was not rejected."
}
Write-Output "[pass] Packaged checker has no foreground activation or desktop capture path."
Write-Output "[pass] Foreground-activation negative control is rejected."
$windowCaptureNegativeControlRejected = $false
try {
    Assert-HenkaPackagedCheckerDoesNotActivateWindows -Source '$bitmap = PrintWindow($handle)'
}
catch {
    $windowCaptureNegativeControlRejected = $true
}
if (-not $windowCaptureNegativeControlRejected) {
    throw "The operating-system window-capture negative control was not rejected."
}
Write-Output "[pass] Operating-system window-capture negative control is rejected."
$sharedLauncherSource = [System.IO.File]::ReadAllText(
    (Join-Path $RepositoryRoot "scripts\henka_script_common.ps1"))
Assert-HenkaLauncherDoesNotRestoreForeground -Source $sharedLauncherSource
$foregroundRestoreNegativeControlRejected = $false
try {
    Assert-HenkaLauncherDoesNotRestoreForeground -Source 'SetForegroundWindow(previousForeground)'
}
catch {
    $foregroundRestoreNegativeControlRejected = $true
}
if (-not $foregroundRestoreNegativeControlRejected) {
    throw "The shared-launcher foreground-restoration negative control was not rejected."
}
Write-Output "[pass] Shared launch helper cannot reassert an earlier foreground window."
Write-Output "[pass] Foreground-restoration negative control is rejected."

$automationWorkflowPaths = @(
    "scripts\check_packaged_sandbox3d_windows.ps1",
    "scripts\author_showcase_assets_windows.ps1",
    "scripts\capture_compass_visual_evidence_windows.ps1",
    "scripts\capture_editor_owned_authoring_sources_windows.ps1")
foreach ($relativePath in $automationWorkflowPaths) {
    $workflowSource = [System.IO.File]::ReadAllText(
        (Join-Path $RepositoryRoot $relativePath))
    Assert-HenkaAutomationWorkflowUsesBackgroundCapture -Source $workflowSource
}
$packagedCheckerSource = [System.IO.File]::ReadAllText(
    (Join-Path $RepositoryRoot "scripts\check_packaged_sandbox3d_windows.ps1"))

function Assert-HenkaPackagedHiddenStartupExceptionPolicy {
    param([Parameter(Mandatory = $true)][string]$Source)

    if ($Source -notmatch '(?im)\[bool\]\$RequireHiddenWindowStartup\s*=\s*\$true' -or
        $Source -notmatch '(?im)if\s*\(\$RequireHiddenWindowStartup\s+-and\s+\$capture\.Stdout\s+-notmatch') {
        throw "The packaged checker must require hidden-startup confirmation by default."
    }

    $prefabSmokeBlock = [regex]::Match(
        $Source,
        '(?s)Write-Step "Running packaged Prefab public API smoke"(?<block>.*?)Write-Output "\[pass\] Packaged Prefab public API smoke completed\."')
    if (-not $prefabSmokeBlock.Success -or
        $prefabSmokeBlock.Groups['block'].Value -notmatch '(?im)-Arguments\s+@\("--prefab-smoke-test"\)' -or
        $prefabSmokeBlock.Groups['block'].Value -notmatch '(?im)-RequireHiddenWindowStartup:\$false') {
        throw "Only the no-window Prefab public API CLI smoke may skip UI-window startup telemetry."
    }

    $bypassCount = [regex]::Matches(
        $Source,
        '(?im)-RequireHiddenWindowStartup\s*:\s*\$false').Count
    if ($bypassCount -ne 1) {
        throw "Exactly one bounded no-window Prefab CLI call may skip hidden-window telemetry."
    }
}

Assert-HenkaPackagedHiddenStartupExceptionPolicy -Source $packagedCheckerSource
$unboundedWindowExceptionRejected = $false
try {
    Assert-HenkaPackagedHiddenStartupExceptionPolicy `
        -Source ($packagedCheckerSource + "`r`n-RequireHiddenWindowStartup:`$false")
}
catch {
    $unboundedWindowExceptionRejected = $true
}
if (-not $unboundedWindowExceptionRejected) {
    throw "The packaged checker policy accepted an additional hidden-startup bypass."
}
Write-Output "[pass] Hidden-startup confirmation is required for UI launches; the no-window Prefab CLI exception is singular and bounded."

if ($packagedCheckerSource -notmatch '(?im)\$env:HENKA_AUTOMATION_START_HIDDEN\s*=\s*"1"' -or
    $packagedCheckerSource -notmatch '(?im)HENKA_AUTOMATION_DIAGNOSTIC window_start_hidden=1') {
    throw "The packaged checker must request and verify hidden startup for every automated Sandbox interaction."
}
$missingHiddenStartupRejected = $false
try {
    Assert-HenkaAutomationWorkflowUsesBackgroundCapture -Source '-StartOffscreenWithoutActivation'
}
catch {
    $missingHiddenStartupRejected = $true
}
if (-not $missingHiddenStartupRejected) {
    throw "The background-window policy negative control did not reject missing hidden startup."
}
Write-Output "[pass] Packaged Sandbox automation requires hidden startup; missing-hidden negative control rejected."
$showcaseSource = [System.IO.File]::ReadAllText(
    (Join-Path $RepositoryRoot "scripts\author_showcase_assets_windows.ps1"))
$compassSource = [System.IO.File]::ReadAllText(
    (Join-Path $RepositoryRoot "scripts\capture_compass_visual_evidence_windows.ps1"))
if ($showcaseSource -notmatch 'Save-HenkaAutomationWindowScreenshot' -or
    $compassSource -notmatch 'Save-HenkaAutomationWindowScreenshot') {
    throw "Visual evidence workflows must use the shared per-window capture instead of copying screen pixels."
}
$automationHelperSource = [System.IO.File]::ReadAllText(
    (Join-Path $RepositoryRoot "scripts\henka_ui_automation_helpers.ps1"))
if ($automationHelperSource -match '(?im)\b(?:Set-HenkaAutomationForeground|SetForegroundWindow|BringWindowToTop|SwitchToThisWindow|AttachThreadInput)\s*(?:\(|\b)') {
    throw "The shared automation helper still exposes a foreground-activation path."
}
$backgroundLaunchNegativeControlRejected = $false
try {
    Assert-HenkaAutomationWorkflowUsesBackgroundCapture `
        -Source '$shell.AppActivate($process.Id); $graphics.CopyFromScreen(...)'
}
catch {
    $backgroundLaunchNegativeControlRejected = $true
}
if (-not $backgroundLaunchNegativeControlRejected) {
    throw "The automated-window activation/desktop-capture negative control was not rejected."
}
Write-Output "[pass] Showcase, Compass, and authoring capture workflows use hidden startup and off-screen non-activating capture."
Write-Output "[pass] Automated activation/desktop-capture negative control is rejected."

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

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool IsWindow(IntPtr hWnd);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern int GetSystemMetrics(int index);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ShowWindow(IntPtr hWnd, int command);
}
"@
}

$fixtureRoot = New-HenkaTemporaryDirectory -RepositoryRoot $RepositoryRoot -Purpose "process-window-policy-regression"
$fixturePath = Join-Path $fixtureRoot "window-fixture.ps1"
$stdoutPath = Join-Path $fixtureRoot "stdout.log"
$stderrPath = Join-Path $fixtureRoot "stderr.log"
$readyPath = Join-Path $fixtureRoot "ready.txt"
$hiddenReadyPath = Join-Path $fixtureRoot "hidden-ready.txt"
$capturedProcess = $null
$hiddenProcess = $null
$nonActivatingVisibleProcess = $null
$plainProcess = $null
$previousForeground = [HenkaWindowPolicyNative]::GetForegroundWindow()

New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null

try {
    $fixtureText = @'
param([Parameter(Mandatory = $true)][string]$ReadyPath, [switch]$KeepHidden)
Add-Type -AssemblyName System.Windows.Forms
Add-Type -TypeDefinition 'using System.Windows.Forms; public sealed class HenkaNoActivateForm : Form { protected override bool ShowWithoutActivation { get { return true; } } protected override CreateParams CreateParams { get { CreateParams parameters = base.CreateParams; parameters.ExStyle |= 0x08000000; return parameters; } } }' -ReferencedAssemblies ([System.Windows.Forms.Form].Assembly.Location)
$form = New-Object HenkaNoActivateForm
$form.Text = "Henka launch policy regression"
$form.Width = 320
$form.Height = 120
$form.StartPosition = "Manual"
$virtualDesktop = [System.Windows.Forms.SystemInformation]::VirtualScreen
$form.Location = [System.Drawing.Point]::new(
    [int]($virtualDesktop.Right + 64),
    [int]($virtualDesktop.Top + 64))
$form.add_HandleCreated({
    [System.IO.File]::WriteAllText($ReadyPath, [string]$form.Handle)
})
if ($KeepHidden) {
    $null = $form.Handle
    [System.Windows.Forms.Application]::Run((New-Object System.Windows.Forms.ApplicationContext))
} else {
    [System.Windows.Forms.Application]::Run($form)
}
'@
    Write-HenkaUtf8NoBom -Path $fixturePath -Text $fixtureText

    $plainProcess = Start-HenkaProcess `
        -FilePath "powershell.exe" `
        -Arguments @("-NoProfile", "-NonInteractive", "-Command", "Start-Sleep -Milliseconds 400; exit 0") `
        -WorkingDirectory $RepositoryRoot
    if ($plainProcess.StartInfo.WindowStyle -ne [System.Diagnostics.ProcessWindowStyle]::Hidden) {
        throw "The ordinary Henka process helper did not select hidden non-activating startup by default."
    }
    if (-not $plainProcess.WaitForExit(5000)) {
        Stop-HenkaProcessTree -ProcessId $plainProcess.Id
        throw "The ordinary process-policy fixture exceeded its bounded exit time."
    }
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
    $handle = [System.IntPtr]::new([Int64]([System.IO.File]::ReadAllText($readyPath)))
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

    $hiddenProcess = Start-HenkaCapturedProcess `
        -FilePath "powershell.exe" `
        -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $fixturePath,
            "-ReadyPath", $hiddenReadyPath,
            "-KeepHidden") `
        -WorkingDirectory $RepositoryRoot `
        -StdoutPath (Join-Path $fixtureRoot "hidden-stdout.log") `
        -StderrPath (Join-Path $fixtureRoot "hidden-stderr.log") `
        -StartMinimized:$false `
        -StartHidden
    if ($hiddenProcess.Process.StartInfo.WindowStyle -ne [System.Diagnostics.ProcessWindowStyle]::Hidden) {
        throw "The explicit hidden-window process mode did not set hidden startup before process creation."
    }

    $hiddenDeadline = [DateTime]::UtcNow.AddSeconds(10)
    while (-not (Test-Path -LiteralPath $hiddenReadyPath -PathType Leaf) -and
        -not $hiddenProcess.Process.HasExited -and
        [DateTime]::UtcNow -lt $hiddenDeadline) {
        Start-Sleep -Milliseconds 50
    }
    if (-not (Test-Path -LiteralPath $hiddenReadyPath -PathType Leaf)) {
        throw "The hidden-startup fixture did not report readiness."
    }
    Start-Sleep -Milliseconds 250
    $hiddenHandle = [System.IntPtr]::new([Int64]([System.IO.File]::ReadAllText($hiddenReadyPath)))
    if ($hiddenHandle -eq [IntPtr]::Zero -or
        [HenkaWindowPolicyNative]::IsWindowVisible($hiddenHandle) -or
        [HenkaWindowPolicyNative]::IsIconic($hiddenHandle) -or
        $hiddenHandle -eq [HenkaWindowPolicyNative]::GetForegroundWindow()) {
        throw ("The explicit hidden-startup fixture exposed a visible or foreground window " +
            "(visible={0} iconic={1} foreground={2} target={3})." -f
            [HenkaWindowPolicyNative]::IsWindowVisible($hiddenHandle),
            [HenkaWindowPolicyNative]::IsIconic($hiddenHandle),
            [HenkaWindowPolicyNative]::GetForegroundWindow().ToInt64(),
            $hiddenHandle.ToInt64())
    }
    Write-Output "[pass] Explicit hidden startup keeps a renderer-owned capture window hidden without minimizing or activating it."

    $typographyRunnerSource = [System.IO.File]::ReadAllText(
        (Join-Path $RepositoryRoot "scripts\test_editor_typography_windows.ps1"))
    if ($typographyRunnerSource -notmatch '(?im)-StartHidden\b' -or
        $typographyRunnerSource -notmatch '(?im)-StartMinimized\s*:\s*\$false') {
        throw "The typography harness must use hidden startup when it renders through the product framebuffer without showing a desktop window."
    }
    Write-Output "[pass] Typography automation selects the explicit hidden startup path."

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
        -StartOffscreenWithoutActivation

    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    while (-not (Test-Path -LiteralPath $visibleReadyPath -PathType Leaf) -and
        -not $nonActivatingVisibleProcess.Process.HasExited -and
        [DateTime]::UtcNow -lt $deadline) {
        Start-Sleep -Milliseconds 50
    }
    if (-not (Test-Path -LiteralPath $visibleReadyPath -PathType Leaf)) {
        throw "The non-activating visible control window did not report readiness."
    }
    $visibleHandle = [System.IntPtr]::new([Int64]([System.IO.File]::ReadAllText($visibleReadyPath)))
    if ($visibleHandle -eq [IntPtr]::Zero) {
        throw "The explicit non-activating visible window did not publish a valid native handle."
    }

    # Observe the launch helper's real off-screen presentation. Do not move or
    # show the window a second time here: that can race the bounded startup
    # monitor and make the test itself re-show a window after a minimize.
    $visibleDeadline = [DateTime]::UtcNow.AddSeconds(3)
    $visibleGeometry = $null
    $visibleOutside = $false
    do {
        $visibleBounds = New-Object HenkaWindowPolicyNative+RECT
        if (-not [HenkaWindowPolicyNative]::GetWindowRect(
                $visibleHandle, [ref]$visibleBounds)) {
            throw "The non-activating visible window bounds could not be observed."
        }
        $visibleGeometry = [pscustomobject]@{
            X = $visibleBounds.Left
            Y = $visibleBounds.Top
            Width = $visibleBounds.Right - $visibleBounds.Left
            Height = $visibleBounds.Bottom - $visibleBounds.Top
        }
        $visibleOutside = Test-HenkaAutomationRectOutsideVirtualScreen `
            -X $visibleGeometry.X `
            -Y $visibleGeometry.Y `
            -Width $visibleGeometry.Width `
            -Height $visibleGeometry.Height
        if ($visibleOutside -and
            [HenkaWindowPolicyNative]::IsWindowVisible($visibleHandle) -and
            -not [HenkaWindowPolicyNative]::IsIconic($visibleHandle)) {
            break
        }
        Start-Sleep -Milliseconds 25
    } while ([DateTime]::UtcNow -lt $visibleDeadline)

    if (-not $visibleOutside -or
        -not [HenkaWindowPolicyNative]::IsWindowVisible($visibleHandle) -or
        [HenkaWindowPolicyNative]::IsIconic($visibleHandle)) {
        throw "The launch helper did not present the window visibly outside the virtual desktop."
    }
    if ($visibleHandle -eq [HenkaWindowPolicyNative]::GetForegroundWindow()) {
        throw "An explicit non-activating visible window acquired foreground focus."
    }
    if (-not (Test-HenkaAutomationRectOutsideVirtualScreen `
            -X $visibleGeometry.X `
            -Y $visibleGeometry.Y `
            -Width $visibleGeometry.Width `
            -Height $visibleGeometry.Height)) {
        throw "The visible capture window overlaps the owner's virtual desktop."
    }
    $minimizePreviousVisibility = [HenkaWindowPolicyNative]::ShowWindow($visibleHandle, 6)
    $minimizeDeadline = [DateTime]::UtcNow.AddSeconds(2)
    while (-not [HenkaWindowPolicyNative]::IsIconic($visibleHandle) -and
        [DateTime]::UtcNow -lt $minimizeDeadline) {
        Start-Sleep -Milliseconds 25
    }
    if (-not [HenkaWindowPolicyNative]::IsIconic($visibleHandle)) {
        $nonActivatingVisibleProcess.Process.Refresh()
        $currentMainWindow = $nonActivatingVisibleProcess.Process.MainWindowHandle
        $visibleRectAfterMinimize = New-Object HenkaWindowPolicyNative+RECT
        $hasRectAfterMinimize = [HenkaWindowPolicyNative]::GetWindowRect(
            $visibleHandle, [ref]$visibleRectAfterMinimize)
        throw ("The visible capture window did not remain minimized after the explicit " +
            "later user minimize action. originalHwnd=$($visibleHandle.ToInt64()), " +
            "isWindow=$([HenkaWindowPolicyNative]::IsWindow($visibleHandle)), " +
            "isVisible=$([HenkaWindowPolicyNative]::IsWindowVisible($visibleHandle)), " +
            "isIconic=$([HenkaWindowPolicyNative]::IsIconic($visibleHandle)), " +
            "showWindowPreviousVisibility=$minimizePreviousVisibility, " +
            "processMainHwnd=$($currentMainWindow.ToInt64()), " +
            "rectAvailable=$hasRectAfterMinimize, " +
            "rect=$($visibleRectAfterMinimize.Left),$($visibleRectAfterMinimize.Top)," +
            "$($visibleRectAfterMinimize.Right),$($visibleRectAfterMinimize.Bottom).")
    }
    Write-Output "[pass] Shared process policy supports a visible, non-minimized capture window wholly outside every virtual desktop monitor without taking foreground focus."
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
    if ($null -ne $hiddenProcess) {
        Close-HenkaCapturedProcess -CapturedProcess $hiddenProcess
    }
    if ($null -ne $nonActivatingVisibleProcess) {
        Close-HenkaCapturedProcess -CapturedProcess $nonActivatingVisibleProcess
    }
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
