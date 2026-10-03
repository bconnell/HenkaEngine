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

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(
        IntPtr hWnd,
        out uint processId);

    [DllImport("kernel32.dll")]
    public static extern uint GetCurrentThreadId();

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool AttachThreadInput(
        uint idAttach,
        uint idAttachTo,
        bool fAttach);
}
"@
}

function Assert-HenkaCaptureDimensions {
    param(
        [Parameter(Mandatory = $true)][int]$Width,
        [Parameter(Mandatory = $true)][int]$Height,
        [int]$MinimumWidth = 800,
        [int]$MinimumHeight = 600,
        [Parameter(Mandatory = $true)][string]$Description
    )

    if ($Width -le 0 -or $Height -le 0 -or
        $MinimumWidth -le 0 -or $MinimumHeight -le 0) {
        throw "$Description had non-positive capture dimensions or minimum bounds."
    }
    if ($Width -lt $MinimumWidth -or $Height -lt $MinimumHeight) {
        throw (
            "$Description measured ${Width}x${Height}, below the minimum capture bounds " +
            "${MinimumWidth}x${MinimumHeight}.")
    }
}

function Set-HenkaAutomationForeground {
    param([Parameter(Mandatory = $true)][System.IntPtr]$Handle)

    if ($Handle -eq [System.IntPtr]::Zero) {
        throw "The Henka automation target handle is invalid."
    }

    # Application-local automation uses Henka's event channel and must not
    # seize the user's desktop.  A foreground call is available only to an
    # explicitly selected Windows-integration test process.
    if ($env:HENKA_ALLOW_FOREGROUND_AUTOMATION -ne "1") {
        return
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

function Send-HenkaAutomationEvent {
    param(
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][string]$EventLine,
        [int]$SettleMilliseconds = 120
    )

    if ([string]::IsNullOrWhiteSpace($EventPath) -or
        [string]::IsNullOrWhiteSpace($EventLine) -or
        $EventLine.Contains("`r") -or
        $EventLine.Contains("`n") -or
        $EventLine.Length -gt 220) {
        throw "The bounded Henka automation event was invalid."
    }
    if ($SettleMilliseconds -lt 0 -or $SettleMilliseconds -gt 5000) {
        throw "The Henka automation settle interval was outside its bounded range."
    }

    $encoding = New-Object System.Text.UTF8Encoding($false)
    $bytes = $encoding.GetBytes($EventLine + [Environment]::NewLine)
    $stream = $null
    try {
        $stream = [System.IO.File]::Open(
            $EventPath,
            [System.IO.FileMode]::OpenOrCreate,
            [System.IO.FileAccess]::Write,
            [System.IO.FileShare]::ReadWrite)
        $stream.Seek(0, [System.IO.SeekOrigin]::End) | Out-Null
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush($true)
    }
    finally {
        if ($null -ne $stream) {
            $stream.Dispose()
        }
    }
    if ($SettleMilliseconds -gt 0) {
        Start-Sleep -Milliseconds $SettleMilliseconds
    }
}

function Format-HenkaAutomationFloat {
    param([Parameter(Mandatory = $true)][double]$Value)

    if ([double]::IsNaN($Value) -or [double]::IsInfinity($Value)) {
        throw "The Henka automation value was non-finite."
    }
    return $Value.ToString("R", [System.Globalization.CultureInfo]::InvariantCulture)
}

function Convert-HenkaFramebufferPointToWindowPoint {
    param(
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][int]$WindowWidth,
        [Parameter(Mandatory = $true)][int]$WindowHeight,
        [Parameter(Mandatory = $true)][double]$FramebufferX,
        [Parameter(Mandatory = $true)][double]$FramebufferY
    )

    if ($FramebufferWidth -le 0 -or $FramebufferHeight -le 0 -or
        $WindowWidth -le 0 -or $WindowHeight -le 0 -or
        [double]::IsNaN($FramebufferX) -or
        [double]::IsInfinity($FramebufferX) -or
        [double]::IsNaN($FramebufferY) -or
        [double]::IsInfinity($FramebufferY) -or
        $FramebufferX -lt 0.0 -or
        $FramebufferY -lt 0.0 -or
        $FramebufferX -ge [double]$FramebufferWidth -or
        $FramebufferY -ge [double]$FramebufferHeight) {
        throw "The Henka framebuffer automation point was outside its bounded client area."
    }

    $windowX = $FramebufferX * $WindowWidth / [double]$FramebufferWidth
    $windowY = $FramebufferY * $WindowHeight / [double]$FramebufferHeight
    if ([double]::IsNaN($windowX) -or [double]::IsInfinity($windowX) -or
        [double]::IsNaN($windowY) -or [double]::IsInfinity($windowY) -or
        $windowX -lt 0.0 -or $windowY -lt 0.0 -or
        $windowX -ge [double]$WindowWidth -or
        $windowY -ge [double]$WindowHeight) {
        throw "The Henka framebuffer automation point could not be converted to a bounded client point."
    }

    return [pscustomobject]@{
        X = $windowX
        Y = $windowY
    }
}

function Send-HenkaAutomationKey {
    param(
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][string]$KeyName
    )

    if ($KeyName -notmatch '^[A-Za-z0-9_]+$') {
        throw "The Henka automation key name was invalid."
    }
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("key {0} down" -f $KeyName)
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("key {0} up" -f $KeyName)
}

function Send-HenkaAutomationText {
    param(
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][string]$Text
    )

    if ([string]::IsNullOrEmpty($Text) -or
        $Text.Contains("`r") -or
        $Text.Contains("`n")) {
        throw "The Henka automation text was empty or contained a line break."
    }
    foreach ($character in $Text.ToCharArray()) {
        $codePoint = [int][char]$character
        if ($codePoint -lt 0x20 -or $codePoint -eq 0x7F) {
            throw "The Henka automation text contained a control character."
        }
    }
    $eventLine = "text " + $Text
    if (([System.Text.Encoding]::UTF8.GetByteCount($eventLine)) -gt 220) {
        throw "The bounded Henka automation text was too long."
    }
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine $eventLine
}

function Send-HenkaAutomationClick {
    param(
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y,
        [ValidateSet("left", "right", "middle")][string]$Button = "left"
    )

    if ([double]::IsNaN($X) -or [double]::IsInfinity($X) -or
        [double]::IsNaN($Y) -or [double]::IsInfinity($Y) -or
        $X -lt 0.0 -or $Y -lt 0.0 -or $X -gt 65536.0 -or $Y -gt 65536.0) {
        throw "The Henka automation pointer coordinate was invalid."
    }
    $xText = Format-HenkaAutomationFloat -Value $X
    $yText = Format-HenkaAutomationFloat -Value $Y
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("move {0} {1}" -f $xText, $yText)
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("button {0} down {1} {2}" -f $Button, $xText, $yText)
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("button {0} up {1} {2}" -f $Button, $xText, $yText)
}

function Get-HenkaViewportShadingControl {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)]
        [ValidateSet("Wireframe", "Solid", "Material Preview", "Rendered")]
        [string]$ModeName
    )

    $pattern = 'Viewport shading control: mode=' +
        [Regex]::Escape($ModeName) +
        ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+)'
    $match = Get-LastLogRegexMatch -Path $LogPath -Pattern $pattern
    if ($null -eq $match) {
        throw "The application did not report the individual $ModeName viewport shading control bounds."
    }

    $culture = [Globalization.CultureInfo]::InvariantCulture
    $control = [pscustomobject]@{
        X = [double]::Parse($match.Groups["x"].Value, $culture)
        Y = [double]::Parse($match.Groups["y"].Value, $culture)
        Width = [double]::Parse($match.Groups["width"].Value, $culture)
        Height = [double]::Parse($match.Groups["height"].Value, $culture)
    }
    if ($control.X -lt 0.0 -or $control.Y -lt 0.0 -or
        $control.Width -le 0.0 -or $control.Height -le 0.0 -or
        [double]::IsNaN($control.X) -or [double]::IsInfinity($control.X) -or
        [double]::IsNaN($control.Y) -or [double]::IsInfinity($control.Y) -or
        [double]::IsNaN($control.Width) -or [double]::IsInfinity($control.Width) -or
        [double]::IsNaN($control.Height) -or [double]::IsInfinity($control.Height)) {
        throw "The application reported invalid bounds for the $ModeName viewport shading control."
    }
    return $control
}

# WheelDelta is expressed in SDL wheel-step units consumed by the engine's
# automation channel (normally -1 or +1), not Win32's 120-unit wheel message.
function Send-HenkaAutomationScroll {
    param(
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y,
        [Parameter(Mandatory = $true)][double]$WheelDelta
    )

    if ([double]::IsNaN($X) -or [double]::IsInfinity($X) -or
        [double]::IsNaN($Y) -or [double]::IsInfinity($Y) -or
        $X -lt 0.0 -or $Y -lt 0.0 -or $X -gt 65536.0 -or $Y -gt 65536.0 -or
        [double]::IsNaN($WheelDelta) -or [double]::IsInfinity($WheelDelta) -or
        $WheelDelta -eq 0.0 -or $WheelDelta -lt -1024.0 -or $WheelDelta -gt 1024.0) {
        throw "The Henka automation wheel delta was invalid."
    }
    if ($WheelDelta -eq -120.0 -or $WheelDelta -eq 120.0) {
        throw "Henka automation scroll uses SDL wheel-step units; Win32's 120-unit wheel delta is not accepted."
    }
    $xText = Format-HenkaAutomationFloat -Value $X
    $yText = Format-HenkaAutomationFloat -Value $Y
    $wheelText = Format-HenkaAutomationFloat -Value $WheelDelta
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("move {0} {1}" -f $xText, $yText)
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("wheel 0 {0}" -f $wheelText)
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
