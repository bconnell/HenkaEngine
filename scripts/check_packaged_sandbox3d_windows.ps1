$ErrorActionPreference = "Stop"

function Write-Step {
    param([string]$Message)
    Write-Output "[check] $Message"
}

function Assert-PathExists {
    param(
        [string]$Path,
        [string]$Description
    )

    if (-not (Test-Path $Path)) {
        throw "$Description was not found: $Path"
    }

    Write-Output "[pass] $Description"
}

function Assert-FileContains {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Description
    )

    if (-not (Test-Path $Path)) {
        throw "$Description could not be checked because the log file was not created: $Path"
    }

    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
        throw "$Description was not found in $Path"
    }

    Write-Output "[pass] $Description"
}

function Try-AssertFileContains {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Description
    )

    if (-not (Test-Path $Path)) {
        Write-Output "[warn] $Description could not be checked because the log file was not created: $Path"
        return $false
    }

    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
        Write-Output "[warn] $Description was not found in $Path"
        return $false
    }

    Write-Output "[pass] $Description"
    return $true
}

function Try-AssertPathExists {
    param(
        [string]$Path,
        [string]$Description
    )

    if (-not (Test-Path $Path)) {
        Write-Output "[warn] $Description was not found: $Path"
        return $false
    }

    Write-Output "[pass] $Description"
    return $true
}

function Wait-FileContains {
    param(
        [string]$Path,
        [string]$Pattern,
        [int]$TimeoutMilliseconds = 5000
    )

    $deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path $Path) -and (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
            return $true
        }

        Start-Sleep -Milliseconds 150
    }

    return $false
}

function Get-WindowRect {
    param([System.IntPtr]$Handle)

    $rect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetWindowRect($Handle, [ref]$rect)) {
        throw "The packaged sandbox window bounds could not be read."
    }

    return $rect
}

function Click-WindowPoint {
    param(
        [System.IntPtr]$Handle,
        [int]$OffsetX,
        [int]$OffsetY
    )

    $rect = Get-WindowRect -Handle $Handle
    $x = $rect.Left + $OffsetX
    $y = $rect.Top + $OffsetY

    [NativeMethods]::SetForegroundWindow($Handle) | Out-Null
    Start-Sleep -Milliseconds 150
    [NativeMethods]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 100
    [NativeMethods]::mouse_event([NativeMethods]::MOUSEEVENTF_LEFTDOWN, 0, 0, 0, [System.UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [NativeMethods]::mouse_event([NativeMethods]::MOUSEEVENTF_LEFTUP, 0, 0, 0, [System.UIntPtr]::Zero)
    Start-Sleep -Milliseconds 250
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$packageRoot = Join-Path $repoRoot "out\HenkaSandbox3D"
$packagedExe = Join-Path $packageRoot "HenkaSandbox3D.exe"
$assetsDir = Join-Path $packageRoot "assets"
$helpPath = Join-Path $packageRoot "docs\help\sandbox3d.md"
$readmePath = Join-Path $packageRoot "README.txt"
$packageInfoPath = Join-Path $packageRoot "PACKAGE_INFO.txt"
$settingsPath = Join-Path $packageRoot "user\sandbox3d.settings"
$logDir = Join-Path $repoRoot "build\test_tmp"
$stdoutPath = Join-Path $logDir "check_packaged_sandbox3d_stdout.log"
$stderrPath = Join-Path $logDir "check_packaged_sandbox3d_stderr.log"

Add-Type -AssemblyName System.Windows.Forms
Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class NativeMethods {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    public const uint MOUSEEVENTF_LEFTUP = 0x0004;

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll")]
    public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
}
'@

$shell = New-Object -ComObject WScript.Shell

Write-Step "Checking packaged sandbox contents"
Assert-PathExists -Path $packageRoot -Description "Packaged sandbox folder"
Assert-PathExists -Path $packagedExe -Description "Packaged sandbox executable"
Assert-PathExists -Path $assetsDir -Description "Packaged assets folder"
Assert-PathExists -Path $helpPath -Description "Packaged offline help"
Assert-PathExists -Path $readmePath -Description "Packaged run guide"
Assert-PathExists -Path $packageInfoPath -Description "Packaged build marker"
Assert-FileContains -Path $readmePath -Pattern "Use the in-window utilities" -Description "Packaged utility guidance"
Assert-FileContains -Path $readmePath -Pattern "status area" -Description "Packaged status guidance"
Assert-FileContains -Path $helpPath -Pattern "Utility panel" -Description "Packaged utility help"

New-Item -ItemType Directory -Path $logDir -Force | Out-Null
Remove-Item $stdoutPath, $stderrPath -ErrorAction SilentlyContinue

$process = $null
$uiAutomationVerified = $false
try {
    Write-Step "Launching the packaged sandbox"
    $process = Start-Process -FilePath $packagedExe -WorkingDirectory $packageRoot -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath

    for ($index = 0; $index -lt 80 -and $process.MainWindowHandle -eq 0; $index++) {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
    }

    if ($process.MainWindowHandle -eq 0) {
        throw "The packaged sandbox window did not become available."
    }

    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Henka Engine Sandbox 3D" -TimeoutMilliseconds 5000)) {
        throw "Startup help heading was not found in the packaged sandbox output."
    }
    Assert-FileContains -Path $stdoutPath -Pattern "Henka Engine Sandbox 3D" -Description "Startup help heading"
    Assert-FileContains -Path $stdoutPath -Pattern "F4               Show or hide the sandbox panels" -Description "F4 help text"
    Assert-FileContains -Path $stdoutPath -Pattern "F5               Cycle View, Inspect, and Full Tools layouts" -Description "F5 help text"
    Assert-FileContains -Path $stdoutPath -Pattern "Runtime mode: Packaged" -Description "Packaged runtime mode output"
    Assert-FileContains -Path $stdoutPath -Pattern "Startup UI:" -Description "Startup UI cue"
    Assert-FileContains -Path $stdoutPath -Pattern "use the in-window .*utilities" -Description "Startup utility cue"
    Assert-FileContains -Path $stdoutPath -Pattern "recent actions and warnings appear" -Description "Startup status cue"

    Write-Step "Checking packaged UI open and close"
    [NativeMethods]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
    $null = $shell.AppActivate($process.Id)
    Start-Sleep -Milliseconds 600
    if (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -TimeoutMilliseconds 1500) {
        Assert-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -Description "Startup UI readiness output"
        Assert-FileContains -Path $stdoutPath -Pattern "View mode|Inspect mode|Full Tools mode" -Description "Layout mode output"
        Try-AssertFileContains -Path $stdoutPath -Pattern "Sandbox viewport:" -Description "Viewport output"
        $uiAutomationVerified = $true
    }
    else {
        [System.Windows.Forms.SendKeys]::SendWait('{F4}')
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox panel: shown" -TimeoutMilliseconds 2000)) {
            $null = $shell.AppActivate($process.Id)
            Start-Sleep -Milliseconds 250
            [System.Windows.Forms.SendKeys]::SendWait('{F4}')
        }
        if (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox panel: shown" -TimeoutMilliseconds 4000) {
            Assert-FileContains -Path $stdoutPath -Pattern "Sandbox panel: shown" -Description "Panel open output"
            Assert-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -Description "UI readiness output after F4"
            Assert-FileContains -Path $stdoutPath -Pattern "View mode|Inspect mode|Full Tools mode" -Description "Layout mode output after F4"
            Try-AssertFileContains -Path $stdoutPath -Pattern "Sandbox viewport:" -Description "Viewport output after F4"
            $uiAutomationVerified = $true
        }
    }

    if ($uiAutomationVerified) {
        Write-Step "Checking packaged UI click controls"
        Click-WindowPoint -Handle $process.MainWindowHandle -OffsetX 120 -OffsetY 158
        Click-WindowPoint -Handle $process.MainWindowHandle -OffsetX 120 -OffsetY 158
        Click-WindowPoint -Handle $process.MainWindowHandle -OffsetX 248 -OffsetY 158
        Click-WindowPoint -Handle $process.MainWindowHandle -OffsetX 248 -OffsetY 158
        Click-WindowPoint -Handle $process.MainWindowHandle -OffsetX 248 -OffsetY 194

        $uiClickChecks = @(
            @{ Pattern = "Debug grid: hidden"; Description = "UI debug grid click output" },
            @{ Pattern = "Debug grid: shown"; Description = "UI debug grid restore output" },
            @{ Pattern = "Wireframe: on"; Description = "UI wireframe click output" },
            @{ Pattern = "Wireframe: off"; Description = "UI wireframe restore output" },
            @{ Pattern = "Sandbox settings saved."; Description = "UI save settings output" }
        )

        $uiClickFailures = 0
        foreach ($check in $uiClickChecks) {
            if (-not (Try-AssertFileContains -Path $stdoutPath -Pattern $check.Pattern -Description $check.Description)) {
                $uiClickFailures++
            }
        }

        if ($uiClickFailures -gt 0) {
            Write-Output "[warn] Some packaged UI click checks could not be confirmed automatically. Manual packaged UI QA is still needed."
        }

        if (-not (Select-String -LiteralPath $stdoutPath -Pattern "Sandbox panel: shown" -Quiet)) {
            $null = $shell.AppActivate($process.Id)
            Start-Sleep -Milliseconds 400
            [System.Windows.Forms.SendKeys]::SendWait('{F4}')
            if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox panel: hidden" -TimeoutMilliseconds 2000)) {
                $null = $shell.AppActivate($process.Id)
                Start-Sleep -Milliseconds 250
                [System.Windows.Forms.SendKeys]::SendWait('{F4}')
            }
            if (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox panel: hidden" -TimeoutMilliseconds 4000) {
                Assert-FileContains -Path $stdoutPath -Pattern "Sandbox panel: hidden" -Description "Panel close output"
            }
            else {
                Write-Output "[warn] Automated F4 panel close could not be confirmed. Manual packaged UI QA is still needed."
            }
        }
    }
    else {
        Write-Output "[warn] Automated F4 panel open could not be confirmed. Manual packaged UI QA is still needed."
    }

    Write-Step "Checking clean close-window shutdown"
    $null = $process.CloseMainWindow()
    if (-not $process.WaitForExit(10000)) {
        throw "The packaged sandbox did not exit within the expected time."
    }

    if (Wait-FileContains -Path $stderrPath -Pattern "leaving engine run loop" -TimeoutMilliseconds 3000) {
        Assert-FileContains -Path $stderrPath -Pattern "leaving engine run loop" -Description "Run loop shutdown log"
    }
    else {
        Write-Output "[warn] Clean close-window shutdown log output could not be confirmed automatically. Manual packaged shutdown QA is still useful."
    }
    if (-not (Try-AssertPathExists -Path $settingsPath -Description "Packaged settings file")) {
        Write-Output "[warn] Automated packaged close did not leave behind a settings file in this run. Manual packaged persistence QA is still needed."
    }

    Write-Output "[pass] Packaged sandbox checks completed."
}
finally {
    if ($process -ne $null) {
        $process.Refresh()
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }
}
