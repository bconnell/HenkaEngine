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

function Get-LastLogRegexMatch {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }

    $deadline = (Get-Date).AddSeconds(5)
    $lastReadError = $null
    do {
        $stream = $null
        $reader = $null
        try {
            $shareMode = [System.IO.FileShare](
                [int][System.IO.FileShare]::ReadWrite -bor
                [int][System.IO.FileShare]::Delete)
            $stream = [System.IO.FileStream]::new(
                $Path,
                [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read,
                $shareMode)
            $reader = [System.IO.StreamReader]::new(
                $stream,
                [System.Text.Encoding]::UTF8,
                $true,
                4096,
                $false)
            $text = $reader.ReadToEnd()
            $reader.Dispose()
            $reader = $null
            $stream = $null
            $lastReadError = $null

            $matches = [Regex]::Matches(
                $text,
                $Pattern,
                [System.Text.RegularExpressions.RegexOptions]::Multiline)
            if ($matches.Count -gt 0) {
                return $matches[$matches.Count - 1]
            }
        }
        catch [System.IO.IOException] {
            $lastReadError = $_.Exception
        }
        catch [System.UnauthorizedAccessException] {
            $lastReadError = $_.Exception
        }
        finally {
            if ($null -ne $reader) {
                $reader.Dispose()
            }
            elseif ($null -ne $stream) {
                $stream.Dispose()
            }
        }

        if ((Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 100
        }
    } while ((Get-Date) -lt $deadline)

    if ($null -ne $lastReadError) {
        throw (
            "The live editor log could not be read with shared read access " +
            "within five seconds: " +
            $lastReadError.Message)
    }

    return $null
}
