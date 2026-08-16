Set-StrictMode -Version Latest

if (-not ("HenkaUiAutomationNative" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class HenkaUiAutomationNative
{
    public const int SW_RESTORE = 9;

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool BringWindowToTop(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern void SwitchToThisWindow(IntPtr hWnd, bool fAltTab);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();
}
"@
}

function Set-HenkaAutomationForeground {
    param([Parameter(Mandatory = $true)][System.IntPtr]$Handle)

    if ($Handle -eq [System.IntPtr]::Zero) {
        throw "The Henka automation target handle is invalid."
    }

    if ([HenkaUiAutomationNative]::GetForegroundWindow() -eq $Handle) {
        return
    }

    [HenkaUiAutomationNative]::ShowWindowAsync(
        $Handle,
        [HenkaUiAutomationNative]::SW_RESTORE) | Out-Null
    [HenkaUiAutomationNative]::BringWindowToTop($Handle) | Out-Null
    [HenkaUiAutomationNative]::SwitchToThisWindow($Handle, $true)

    $deadline = (Get-Date).AddSeconds(3)
    do {
        [HenkaUiAutomationNative]::SetForegroundWindow($Handle) | Out-Null
        if ([HenkaUiAutomationNative]::GetForegroundWindow() -eq $Handle) {
            Start-Sleep -Milliseconds 150
            return
        }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)

    throw (
        "The Henka UI harness could not acquire its target window as " +
        "foreground within three seconds. No UI assertion was made.")
}

function Wait-FileContains {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [int]$TimeoutMilliseconds = 5000
    )

    $deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    while ((Get-Date) -lt $deadline) {
        if ((Test-Path -LiteralPath $Path) -and
            (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
            return $true
        }
        Start-Sleep -Milliseconds 150
    }
    return $false
}
