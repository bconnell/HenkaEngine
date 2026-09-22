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
}
"@
}

$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("henka-process-window-policy-" + $PID)
$fixturePath = Join-Path $fixtureRoot "window-fixture.ps1"
$stdoutPath = Join-Path $fixtureRoot "stdout.log"
$stderrPath = Join-Path $fixtureRoot "stderr.log"
$readyPath = Join-Path $fixtureRoot "ready.txt"
$capturedProcess = $null
$plainProcess = $null
$previousForeground = [HenkaWindowPolicyNative]::GetForegroundWindow()

New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null

try {
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
    Write-Output "[pass] Shared Henka process launch policy creates validation windows hidden, then exposes them minimized and unfocused."
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
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
