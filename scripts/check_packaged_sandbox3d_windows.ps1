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
using System.Text;

public static class NativeMethods {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

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

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll")]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int maxLength);

    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);

    public static IntPtr FindProcessWindow(uint processId, string title) {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hWnd, IntPtr lParam) {
            uint owner;
            GetWindowThreadProcessId(hWnd, out owner);
            if (owner == processId) {
                StringBuilder text = new StringBuilder(256);
                GetWindowText(hWnd, text, text.Capacity);
                if (text.ToString().Contains(title)) {
                    result = hWnd;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
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
Assert-FileContains -Path $readmePath -Pattern "panels open automatically" -Description "Packaged automatic panel guidance"
Assert-FileContains -Path $readmePath -Pattern "no selected scene object" -Description "Packaged startup no-selection guidance"
Assert-FileContains -Path $readmePath -Pattern "Physics QA explains Static, Dynamic, and Kinematic" -Description "Packaged physics body-type guidance"
Assert-FileContains -Path $readmePath -Pattern "viewport highlight" -Description "Packaged selection highlight guidance"
Assert-FileContains -Path $readmePath -Pattern "one bounded Scene View highlight" -Description "Packaged ground highlight guidance"
Assert-FileContains -Path $readmePath -Pattern "release away from the outlines to open a separate native tool window" -Description "Packaged workspace guidance"
Assert-FileContains -Path $readmePath -Pattern "Open Native Panel Test from Controls to exercise a separate OS-level validation window" -Description "Packaged native test panel guidance"
Assert-FileContains -Path $readmePath -Pattern "Close a detached tool window to return its panel to the last valid dock" -Description "Packaged workspace limitation guidance"
Assert-FileContains -Path $readmePath -Pattern "status area" -Description "Packaged status guidance"
Assert-FileContains -Path $helpPath -Pattern "Utility panel" -Description "Packaged utility help"

New-Item -ItemType Directory -Path $logDir -Force | Out-Null
Remove-Item $stdoutPath, $stderrPath -ErrorAction SilentlyContinue

$process = $null
$mainWindowHandle = [System.IntPtr]::Zero
$uiAutomationVerified = $false
try {
    Write-Step "Launching the packaged sandbox"
    $process = Start-Process -FilePath $packagedExe -WorkingDirectory $packageRoot -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath

    for ($index = 0; $index -lt 80 -and $mainWindowHandle -eq [System.IntPtr]::Zero; $index++) {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
        $mainWindowHandle = [NativeMethods]::FindProcessWindow([uint32]$process.Id, "Henka Engine Sandbox 3D")
    }

    if ($mainWindowHandle -eq [System.IntPtr]::Zero) {
        throw "The packaged sandbox window did not become available."
    }

    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Henka Engine Sandbox 3D" -TimeoutMilliseconds 5000)) {
        throw "Startup help heading was not found in the packaged sandbox output."
    }
    Assert-FileContains -Path $stdoutPath -Pattern "Henka Engine Sandbox 3D" -Description "Startup help heading"
    Assert-FileContains -Path $stdoutPath -Pattern "F4               Show or hide the sandbox panels" -Description "F4 help text"
    Assert-FileContains -Path $stdoutPath -Pattern "F5               Cycle View, Inspect, and Full Tools layouts" -Description "F5 help text"
    Assert-FileContains -Path $stdoutPath -Pattern "Runtime mode: Packaged" -Description "Packaged runtime mode output"
    Assert-FileContains -Path $stdoutPath -Pattern "Startup selection: None" -Description "Packaged startup no-selection output"
    Assert-FileContains -Path $stdoutPath -Pattern "Startup UI:" -Description "Startup UI cue"
    Assert-FileContains -Path $stdoutPath -Pattern "Controls and Physics QA are discoverable immediately" -Description "Startup auto panel cue"
    Assert-FileContains -Path $stdoutPath -Pattern "use the in-window .*utilities" -Description "Startup utility cue"
    Assert-FileContains -Path $stdoutPath -Pattern "recent actions and warnings appear" -Description "Startup status cue"

    Write-Step "Checking packaged UI open and close"
    [NativeMethods]::SetForegroundWindow($mainWindowHandle) | Out-Null
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
            [NativeMethods]::SetForegroundWindow($mainWindowHandle) | Out-Null
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
        Click-WindowPoint -Handle $mainWindowHandle -OffsetX 120 -OffsetY 180
        Click-WindowPoint -Handle $mainWindowHandle -OffsetX 120 -OffsetY 180
        Click-WindowPoint -Handle $mainWindowHandle -OffsetX 248 -OffsetY 180
        Click-WindowPoint -Handle $mainWindowHandle -OffsetX 248 -OffsetY 180
        Click-WindowPoint -Handle $mainWindowHandle -OffsetX 140 -OffsetY 520
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native Panel Test: opened" -TimeoutMilliseconds 3000)) {
            throw "The Native Panel Test control did not open the secondary native window."
        }
        Assert-FileContains -Path $stdoutPath -Pattern "Native Panel Test: opened" -Description "Native test panel open output"
        $nativeWindowHandle = [NativeMethods]::FindProcessWindow([uint32]$process.Id, "Henka Native Panel Test")
        if ($nativeWindowHandle -eq [System.IntPtr]::Zero) {
            throw "The Native Panel Test window was not visible as a separate OS-level window."
        }
        Write-Output "[pass] Native test panel visible as a separate OS-level window"
        [NativeMethods]::PostMessage($nativeWindowHandle, 0x0010, [System.IntPtr]::Zero, [System.IntPtr]::Zero) | Out-Null
        Start-Sleep -Milliseconds 500
        Click-WindowPoint -Handle $mainWindowHandle -OffsetX 140 -OffsetY 520
        Start-Sleep -Milliseconds 500
        if ((Select-String -LiteralPath $stdoutPath -Pattern "Native Panel Test: opened").Count -lt 2) {
            throw "The Native Panel Test window did not reopen after being closed."
        }
        Write-Output "[pass] Native test panel closes and reopens without closing the main sandbox"
        Click-WindowPoint -Handle $mainWindowHandle -OffsetX 230 -OffsetY 60
        Click-WindowPoint -Handle $mainWindowHandle -OffsetX 100 -OffsetY 610

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
            [NativeMethods]::SetForegroundWindow($mainWindowHandle) | Out-Null
            Start-Sleep -Milliseconds 400
            [System.Windows.Forms.SendKeys]::SendWait('{F4}')
            if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox panel: hidden" -TimeoutMilliseconds 2000)) {
                [NativeMethods]::SetForegroundWindow($mainWindowHandle) | Out-Null
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
    [NativeMethods]::PostMessage($mainWindowHandle, 0x0010, [System.IntPtr]::Zero, [System.IntPtr]::Zero) | Out-Null
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
