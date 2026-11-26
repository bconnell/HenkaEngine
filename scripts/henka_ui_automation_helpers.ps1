Set-StrictMode -Version Latest

if (-not ("HenkaUiAutomationNative" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public static class HenkaUiAutomationNative
{
    public const int GWL_STYLE = -16;
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X; public int Y; }
    public delegate bool EnumWindowsProc(IntPtr handle, IntPtr parameter);

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

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetWindowRect(IntPtr handle, out RECT rect);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetClientRect(IntPtr handle, out RECT rect);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ClientToScreen(IntPtr handle, ref POINT point);

    [DllImport("user32.dll")]
    public static extern int GetWindowText(IntPtr handle, StringBuilder text, int capacity);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool IsWindowVisible(IntPtr handle);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool IsIconic(IntPtr handle);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool PrintWindow(IntPtr handle, IntPtr hdcBlt, uint flags);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetWindowPos(IntPtr handle, IntPtr insertAfter,
        int x, int y, int width, int height, uint flags);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW", SetLastError = true)]
    private static extern IntPtr GetWindowLongPtr64(IntPtr handle, int index);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongW", SetLastError = true)]
    private static extern int GetWindowLong32(IntPtr handle, int index);

    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW", SetLastError = true)]
    private static extern IntPtr SetWindowLongPtr64(IntPtr handle, int index, IntPtr value);

    [DllImport("user32.dll", EntryPoint = "SetWindowLongW", SetLastError = true)]
    private static extern int SetWindowLong32(IntPtr handle, int index, int value);

    [DllImport("user32.dll")]
    public static extern int GetSystemMetrics(int index);

    private static IntPtr GetWindowStyle(IntPtr handle)
    {
        return IntPtr.Size == 8
            ? GetWindowLongPtr64(handle, GWL_STYLE)
            : new IntPtr(GetWindowLong32(handle, GWL_STYLE));
    }

    private static IntPtr SetWindowStyle(IntPtr handle, IntPtr value)
    {
        return IntPtr.Size == 8
            ? SetWindowLongPtr64(handle, GWL_STYLE, value)
            : new IntPtr(SetWindowLong32(handle, GWL_STYLE, value.ToInt32()));
    }

    public static bool SetBorderlessDesktopViewportNoActivate(
        IntPtr handle, int width, int height)
    {
        const long WS_OVERLAPPEDWINDOW = 0x00CF0000L;
        const long WS_POPUP = 0x80000000L;
        const uint SWP_NOACTIVATE = 0x0010;
        const uint SWP_NOZORDER = 0x0004;
        const uint SWP_FRAMECHANGED = 0x0020;
        IntPtr previousStyle;
        long style;
        long candidateStyle;
        IntPtr candidateStylePointer;
        int screenWidth;
        int screenHeight;

        if (handle == IntPtr.Zero || width <= 0 || height <= 0)
            return false;
        screenWidth = GetSystemMetrics(0);
        screenHeight = GetSystemMetrics(1);
        if (screenWidth != width || screenHeight != height)
            return false;

        previousStyle = GetWindowStyle(handle);
        if (previousStyle == IntPtr.Zero)
            return false;
        style = previousStyle.ToInt64();
        candidateStyle = (style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP;
        candidateStylePointer = IntPtr.Size == 8
            ? new IntPtr(candidateStyle)
            : new IntPtr(unchecked((int)candidateStyle));
        if (SetWindowStyle(handle, candidateStylePointer) == IntPtr.Zero)
            return false;
        if (GetWindowStyle(handle).ToInt64() != candidateStyle)
            return false;
        return SetWindowPos(
            handle, IntPtr.Zero, 0, 0, width, height,
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    public static IntPtr FindWindowForProcess(uint targetProcessId, string titlePart)
    {
        return FindWindowForProcess(targetProcessId, titlePart, false);
    }

    public static IntPtr FindWindowForProcess(
        uint targetProcessId, string titlePart, bool includeHidden)
    {
        IntPtr result = IntPtr.Zero;
        EnumWindows(delegate(IntPtr handle, IntPtr parameter) {
            uint owner;
            GetWindowThreadProcessId(handle, out owner);
            if (owner == targetProcessId &&
                (includeHidden || IsWindowVisible(handle))) {
                StringBuilder title = new StringBuilder(512);
                GetWindowText(handle, title, title.Capacity);
                if (title.ToString().IndexOf(titlePart,
                    StringComparison.OrdinalIgnoreCase) >= 0) {
                    result = handle;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }

}
"@
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

function Assert-HenkaAutomationFramebufferRect {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y,
        [Parameter(Mandatory = $true)][double]$Width,
        [Parameter(Mandatory = $true)][double]$Height
    )

    foreach ($value in @($X, $Y, $Width, $Height)) {
        if ([double]::IsNaN($value) -or [double]::IsInfinity($value)) {
            throw "$Name reported a non-finite framebuffer rectangle."
        }
    }
    if ($FramebufferWidth -le 0 -or $FramebufferHeight -le 0) {
        throw "Framebuffer dimensions must be positive for geometry validation."
    }
    if ($Width -le 0.0 -or $Height -le 0.0) {
        throw "$Name reported a non-positive framebuffer rectangle."
    }
    if ($X -lt 0.0 -or $Y -lt 0.0 -or
        $X + $Width -gt [double]$FramebufferWidth -or
        $Y + $Height -gt [double]$FramebufferHeight) {
        throw (
            "$Name is outside the reported framebuffer: " +
            "rect=($X,$Y,$Width,$Height), " +
            "framebuffer=$($FramebufferWidth)x$($FramebufferHeight).")
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

function Send-HenkaAutomationScroll {
    param(
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y,
        [Parameter(Mandatory = $true)][double]$WheelDelta
    )

    # WheelDelta is passed through to the application as SDL-style wheel-notch
    # units (for example, -1.0), not Win32 WHEEL_DELTA units (for example, -120).
    if ([double]::IsNaN($X) -or [double]::IsInfinity($X) -or
        [double]::IsNaN($Y) -or [double]::IsInfinity($Y) -or
        $X -lt 0.0 -or $Y -lt 0.0 -or $X -gt 65536.0 -or $Y -gt 65536.0 -or
        [double]::IsNaN($WheelDelta) -or [double]::IsInfinity($WheelDelta) -or
        $WheelDelta -eq 0.0 -or $WheelDelta -lt -1024.0 -or $WheelDelta -gt 1024.0) {
        throw "The Henka automation wheel delta was invalid."
    }
    $xText = Format-HenkaAutomationFloat -Value $X
    $yText = Format-HenkaAutomationFloat -Value $Y
    $wheelText = Format-HenkaAutomationFloat -Value $WheelDelta
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("move {0} {1}" -f $xText, $yText)
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("wheel 0 {0}" -f $wheelText)
}

function Convert-HenkaUiLogicalPointToWindowPoint {
    param(
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y,
        [Parameter(Mandatory = $true)]
        [ValidateSet(100, 125, 150, 200)]
        [int]$EditorScalePercent
    )

    if ([double]::IsNaN($X) -or [double]::IsInfinity($X) -or
        [double]::IsNaN($Y) -or [double]::IsInfinity($Y) -or
        $X -lt 0.0 -or $Y -lt 0.0 -or $X -gt 65536.0 -or $Y -gt 65536.0) {
        throw "The Henka logical UI point was invalid."
    }

    # Product-reported control rectangles are in editor logical units. Convert
    # those points to window coordinates using only the persisted Editor UI
    # Scale. The normal application path performs the independent window-to-
    # framebuffer and OS display-scale conversion after this input is consumed.
    $scale = [double]$EditorScalePercent / 100.0
    return [pscustomobject]@{
        X = $X * $scale
        Y = $Y * $scale
    }
}

function Send-HenkaAutomationUiClick {
    param(
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y,
        [Parameter(Mandatory = $true)]
        [ValidateSet(100, 125, 150, 200)]
        [int]$EditorScalePercent,
        [ValidateSet("left", "right", "middle")][string]$Button = "left"
    )

    $windowPoint = Convert-HenkaUiLogicalPointToWindowPoint `
        -X $X -Y $Y -EditorScalePercent $EditorScalePercent
    Send-HenkaAutomationClick `
        -EventPath $EventPath `
        -X $windowPoint.X `
        -Y $windowPoint.Y `
        -Button $Button
}

function Send-HenkaAutomationUiDrag {
    param(
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][double]$StartX,
        [Parameter(Mandatory = $true)][double]$StartY,
        [Parameter(Mandatory = $true)][double]$EndX,
        [Parameter(Mandatory = $true)][double]$EndY,
        [Parameter(Mandatory = $true)]
        [ValidateSet(100, 125, 150, 200)]
        [int]$EditorScalePercent,
        [ValidateSet("left", "right", "middle")][string]$Button = "left"
    )

    $startPoint = Convert-HenkaUiLogicalPointToWindowPoint `
        -X $StartX -Y $StartY -EditorScalePercent $EditorScalePercent
    $endPoint = Convert-HenkaUiLogicalPointToWindowPoint `
        -X $EndX -Y $EndY -EditorScalePercent $EditorScalePercent
    foreach ($point in @($startPoint, $endPoint)) {
        if ($point.X -lt 0.0 -or $point.Y -lt 0.0 -or
            $point.X -gt 65536.0 -or $point.Y -gt 65536.0) {
            throw "The scaled Henka UI drag point was outside the bounded input range."
        }
    }
    if ($startPoint.X -eq $endPoint.X -and $startPoint.Y -eq $endPoint.Y) {
        throw "The Henka UI drag must move to a distinct endpoint."
    }

    $startXText = Format-HenkaAutomationFloat -Value $startPoint.X
    $startYText = Format-HenkaAutomationFloat -Value $startPoint.Y
    $endXText = Format-HenkaAutomationFloat -Value $endPoint.X
    $endYText = Format-HenkaAutomationFloat -Value $endPoint.Y
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("move {0} {1}" -f $startXText, $startYText)
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("button {0} down {1} {2}" -f $Button, $startXText, $startYText)
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("move {0} {1}" -f $endXText, $endYText)
    Send-HenkaAutomationEvent -EventPath $EventPath -EventLine ("button {0} up {1} {2}" -f $Button, $endXText, $endYText)
}

function Send-HenkaAutomationUiScroll {
    param(
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y,
        [Parameter(Mandatory = $true)][double]$WheelDelta,
        [Parameter(Mandatory = $true)]
        [ValidateSet(100, 125, 150, 200)]
        [int]$EditorScalePercent
    )

    $windowPoint = Convert-HenkaUiLogicalPointToWindowPoint `
        -X $X -Y $Y -EditorScalePercent $EditorScalePercent
    Send-HenkaAutomationScroll `
        -EventPath $EventPath `
        -X $windowPoint.X `
        -Y $windowPoint.Y `
        -WheelDelta $WheelDelta
}

function Find-HenkaAutomationWindow {
    param(
        [Parameter(Mandatory = $true)][int]$ProcessId,
        [Parameter(Mandatory = $true)][string]$TitleContains,
        [switch]$IncludeHidden
    )

    if ($ProcessId -le 0 -or [string]::IsNullOrWhiteSpace($TitleContains)) {
        throw "The bounded Henka window lookup arguments were invalid."
    }
    return [HenkaUiAutomationNative]::FindWindowForProcess(
        [uint32]$ProcessId,
        $TitleContains,
        [bool]$IncludeHidden)
}

function Get-HenkaAutomationWindowGeometry {
    param([Parameter(Mandatory = $true)][System.IntPtr]$Handle)

    $window = New-Object HenkaUiAutomationNative+RECT
    $client = New-Object HenkaUiAutomationNative+RECT
    $clientOrigin = New-Object HenkaUiAutomationNative+POINT
    if ($Handle -eq [IntPtr]::Zero -or
        -not [HenkaUiAutomationNative]::GetWindowRect($Handle, [ref]$window) -or
        -not [HenkaUiAutomationNative]::GetClientRect($Handle, [ref]$client) -or
        -not [HenkaUiAutomationNative]::ClientToScreen($Handle, [ref]$clientOrigin)) {
        throw "The Henka automation window geometry could not be read."
    }
    return [pscustomobject]@{
        X = $window.Left
        Y = $window.Top
        Width = $window.Right - $window.Left
        Height = $window.Bottom - $window.Top
        ClientWidth = $client.Right - $client.Left
        ClientHeight = $client.Bottom - $client.Top
        ClientOffsetX = $clientOrigin.X - $window.Left
        ClientOffsetY = $clientOrigin.Y - $window.Top
    }
}

function Test-HenkaAutomationRectOutsideVirtualScreen {
    param(
        [Parameter(Mandatory = $true)][int]$X,
        [Parameter(Mandatory = $true)][int]$Y,
        [Parameter(Mandatory = $true)][int]$Width,
        [Parameter(Mandatory = $true)][int]$Height
    )

    if ($Width -le 0 -or $Height -le 0) {
        return $false
    }

    [long]$screenLeft = [HenkaUiAutomationNative]::GetSystemMetrics(76)
    [long]$screenTop = [HenkaUiAutomationNative]::GetSystemMetrics(77)
    [long]$screenRight = $screenLeft +
        [HenkaUiAutomationNative]::GetSystemMetrics(78)
    [long]$screenBottom = $screenTop +
        [HenkaUiAutomationNative]::GetSystemMetrics(79)
    [long]$windowRight = [long]$X + [long]$Width
    [long]$windowBottom = [long]$Y + [long]$Height

    return ($windowRight -le $screenLeft -or
        [long]$X -ge $screenRight -or
        $windowBottom -le $screenTop -or
        [long]$Y -ge $screenBottom)
}

function Show-HenkaAutomationWindowOffscreen {
    param([Parameter(Mandatory = $true)][System.IntPtr]$Handle)

    if ($Handle -eq [IntPtr]::Zero) {
        throw "The Henka automation window handle is invalid."
    }

    $bounds = New-Object HenkaUiAutomationNative+RECT
    if (-not [HenkaUiAutomationNative]::GetWindowRect($Handle, [ref]$bounds)) {
        throw "The Henka automation window bounds could not be read before off-screen placement."
    }
    $width = $bounds.Right - $bounds.Left
    $height = $bounds.Bottom - $bounds.Top
    if ($width -le 0 -or $height -le 0) {
        throw "The Henka automation window has invalid bounds before off-screen placement."
    }

    [long]$virtualLeft = [HenkaUiAutomationNative]::GetSystemMetrics(76)
    [long]$virtualTop = [HenkaUiAutomationNative]::GetSystemMetrics(77)
    [long]$virtualWidth = [HenkaUiAutomationNative]::GetSystemMetrics(78)
    [long]$virtualHeight = [HenkaUiAutomationNative]::GetSystemMetrics(79)
    if ($virtualWidth -le 0 -or $virtualHeight -le 0) {
        throw "Windows did not report valid virtual-desktop bounds for background capture."
    }
    [long]$targetX = $virtualLeft + $virtualWidth + 64
    [long]$targetY = $virtualTop + 64
    if ($targetX -gt [int]::MaxValue -or $targetX -lt [int]::MinValue -or
        $targetY -gt [int]::MaxValue -or $targetY -lt [int]::MinValue) {
        throw "The off-screen capture position exceeded the supported native coordinate range."
    }

    $positionFlags = [uint32](0x0010 -bor 0x0004 -bor 0x0200) # NOACTIVATE | NOZORDER | NOOWNERZORDER
    if (-not [HenkaUiAutomationNative]::SetWindowPos(
            $Handle,
            [IntPtr]::Zero,
            [int]$targetX,
            [int]$targetY,
            $width,
            $height,
            $positionFlags)) {
        throw "The native capture window could not be moved outside the virtual desktop without activation."
    }

    # Show only after its complete frame has been moved beyond every monitor.
    [void][HenkaUiAutomationNative]::ShowWindowAsync($Handle, 4) # SW_SHOWNOACTIVATE
    $deadline = (Get-Date).AddSeconds(2)
    do {
        $bounds = New-Object HenkaUiAutomationNative+RECT
        if (-not [HenkaUiAutomationNative]::GetWindowRect($Handle, [ref]$bounds)) {
            throw "The native capture window bounds could not be read after off-screen placement."
        }
        $outside = Test-HenkaAutomationRectOutsideVirtualScreen `
            -X $bounds.Left `
            -Y $bounds.Top `
            -Width ($bounds.Right - $bounds.Left) `
            -Height ($bounds.Bottom - $bounds.Top)
        if ($outside -and
            [HenkaUiAutomationNative]::IsWindowVisible($Handle) -and
            -not [HenkaUiAutomationNative]::IsIconic($Handle)) {
            break
        }
        Start-Sleep -Milliseconds 25
    } while ((Get-Date) -lt $deadline)

    if (-not $outside -or
        -not [HenkaUiAutomationNative]::IsWindowVisible($Handle) -or
        [HenkaUiAutomationNative]::IsIconic($Handle)) {
        throw "The capture window did not become visible entirely outside the virtual desktop."
    }
    if ([HenkaUiAutomationNative]::GetForegroundWindow() -eq $Handle) {
        throw "The off-screen capture window unexpectedly acquired foreground focus."
    }

    return [pscustomobject]@{
        X = $bounds.Left
        Y = $bounds.Top
        Width = $bounds.Right - $bounds.Left
        Height = $bounds.Bottom - $bounds.Top
        VirtualDesktopLeft = $virtualLeft
        VirtualDesktopTop = $virtualTop
        VirtualDesktopWidth = $virtualWidth
        VirtualDesktopHeight = $virtualHeight
    }
}

function Set-HenkaAutomationWindowClientSize {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$Width,
        [Parameter(Mandatory = $true)][int]$Height,
        [switch]$BorderlessViewport
    )

    if ($Width -lt 640 -or $Height -lt 480 -or $Width -gt 4096 -or $Height -gt 2160) {
        throw "The requested Henka client size is outside the supported evidence bounds."
    }
    if ($BorderlessViewport) {
        if ($Width -ne [HenkaUiAutomationNative]::GetSystemMetrics(0) -or
            $Height -ne [HenkaUiAutomationNative]::GetSystemMetrics(1)) {
            throw "A borderless typography viewport must exactly match the current desktop dimensions."
        }
        if (-not [HenkaUiAutomationNative]::SetBorderlessDesktopViewportNoActivate(
                $Handle, $Width, $Height)) {
            throw "The Henka validation window could not enter a borderless desktop viewport without activation."
        }
        $borderlessGeometry = Get-HenkaAutomationWindowGeometry -Handle $Handle
        if ($borderlessGeometry.ClientWidth -ne $Width -or
            $borderlessGeometry.ClientHeight -ne $Height) {
            throw "The borderless Henka viewport reached client size $($borderlessGeometry.ClientWidth)x$($borderlessGeometry.ClientHeight), expected ${Width}x${Height}."
        }
        return $borderlessGeometry
    }

    $before = Get-HenkaAutomationWindowGeometry -Handle $Handle
    $outerWidth = $Width + ($before.Width - $before.ClientWidth)
    $outerHeight = $Height + ($before.Height - $before.ClientHeight)
    $flags = [uint32](0x0010 -bor 0x0004) # SWP_NOACTIVATE | SWP_NOZORDER
    if (-not [HenkaUiAutomationNative]::SetWindowPos(
            $Handle, [IntPtr]::Zero, $before.X, $before.Y,
            $outerWidth, $outerHeight, $flags)) {
        throw "The Henka window could not be resized without activation."
    }
    $deadline = (Get-Date).AddSeconds(5)
    do {
        $after = Get-HenkaAutomationWindowGeometry -Handle $Handle
        if ($after.ClientWidth -eq $Width -and $after.ClientHeight -eq $Height) {
            return $after
        }
        Start-Sleep -Milliseconds 50
    } while ((Get-Date) -lt $deadline)
    throw "The Henka client size did not reach the requested ${Width}x${Height}; actual client size is $($after.ClientWidth)x$($after.ClientHeight), desktop is $([HenkaUiAutomationNative]::GetSystemMetrics(0))x$([HenkaUiAutomationNative]::GetSystemMetrics(1))."
}

function Get-HenkaNativeAuthoringSourceRowGeometry {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [string]$Name,
        [string]$NamePrefix,
        [switch]$Latest
    )

    if ([string]::IsNullOrWhiteSpace($Name) -eq [string]::IsNullOrWhiteSpace($NamePrefix)) {
        throw 'Specify exactly one native authoring row name or name prefix.'
    }
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }

    $nameExpression = if (-not [string]::IsNullOrWhiteSpace($Name)) {
        '(?<name>' + [Regex]::Escape($Name) + ')'
    }
    else {
        '(?<name>' + [Regex]::Escape($NamePrefix) + '[^\r\n]+?)'
    }
    $number = '-?(?:[0-9]+(?:\.[0-9]+)?|\.[0-9]+)'
    $pattern = '(?m)^Native authoring source row: name=' + $nameExpression +
        ' x=(?<x>' + $number + ') y=(?<y>' + $number +
        ') width=(?<width>' + $number + ') height=(?<height>' + $number + ')\.\r?$'
    $text = Read-HenkaSharedText -Path $Path
    $matches = [Regex]::Matches($text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }
    $match = if ($Latest) { $matches[$matches.Count - 1] } else { $matches[0] }
    $culture = [Globalization.CultureInfo]::InvariantCulture
    return [pscustomobject]@{
        Name = $match.Groups['name'].Value
        X = [double]::Parse($match.Groups['x'].Value, $culture)
        Y = [double]::Parse($match.Groups['y'].Value, $culture)
        Width = [double]::Parse($match.Groups['width'].Value, $culture)
        Height = [double]::Parse($match.Groups['height'].Value, $culture)
    }
}

function New-HenkaDetachedBitmapFromFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [ValidateRange(1, 8192)][int]$MaxDimension = 8192,
        [ValidateRange(1, 16777216)][long]$MaxPixels = 16777216
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "The captured image does not exist: $Path"
    }
    Add-Type -AssemblyName System.Drawing
    $source = $null
    $copy = $null
    $graphics = $null
    $published = $false
    try {
        $source = New-Object System.Drawing.Bitmap -ArgumentList $Path
        if ($source.Width -le 0 -or $source.Height -le 0 -or
            $source.Width -gt $MaxDimension -or $source.Height -gt $MaxDimension -or
            ([long]$source.Width * [long]$source.Height) -gt $MaxPixels) {
            throw "The captured image dimensions exceed the bounded bitmap contract."
        }

        # Bitmap.Clone can retain the source file handle even after disposing
        # the file-backed Bitmap. Draw into a newly allocated image so callers
        # can retire the temporary capture while keeping the returned pixels.
        $copy = New-Object System.Drawing.Bitmap -ArgumentList `
            $source.Width, `
            $source.Height, `
            ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($copy)
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.DrawImageUnscaled($source, 0, 0)
        $graphics.Dispose()
        $graphics = $null
        $source.Dispose()
        $source = $null
        $published = $true
        return $copy
    }
    finally {
        if ($null -ne $graphics) {
            $graphics.Dispose()
        }
        if ($null -ne $source) {
            $source.Dispose()
        }
        if (-not $published -and $null -ne $copy) {
            $copy.Dispose()
        }
    }
}

function New-HenkaAutomationProductFramebufferBitmap {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$OutputDirectory,
        [Parameter(Mandatory = $true)][string]$RequestFile,
        [Parameter(Mandatory = $true)][string]$Description,
        [ValidateRange(1000, 60000)][int]$TimeoutMilliseconds = 10000
    )

    if ($Handle -eq [IntPtr]::Zero -or
        [HenkaUiAutomationNative]::GetForegroundWindow() -eq $Handle) {
        throw "$Description refused to capture an invalid or foreground Henka window."
    }
    if (-not (Test-Path -LiteralPath $StdoutPath -PathType Leaf) -or
        -not (Test-Path -LiteralPath $OutputDirectory -PathType Container)) {
        throw "$Description has no existing product capture log or output directory."
    }

    $outputFullPath = [System.IO.Path]::GetFullPath($OutputDirectory).TrimEnd([char[]]@([char]'\', [char]'/'))
    $requestFullPath = [System.IO.Path]::GetFullPath($RequestFile)
    $requestParent = [System.IO.Path]::GetDirectoryName($requestFullPath).TrimEnd([char[]]@([char]'\', [char]'/'))
    $outputParent = [System.IO.Path]::GetDirectoryName($outputFullPath).TrimEnd([char[]]@([char]'\', [char]'/'))
    $outputItem = Get-Item -LiteralPath $outputFullPath -Force -ErrorAction Stop
    if (($outputItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or
        $requestParent -ine $outputParent -or
        (Test-Path -LiteralPath $requestFullPath)) {
        throw "$Description requires an ordinary capture directory and a fresh sibling request path."
    }

    $label = "ui" + [Guid]::NewGuid().ToString("N").Substring(0, 16)
    $requestTemporaryPath = $requestFullPath + "." + $label + ".tmp"
    try {
        [System.IO.File]::WriteAllText(
            $requestTemporaryPath,
            ($label + "`n"),
            (New-Object System.Text.UTF8Encoding($false)))
        [System.IO.File]::Move($requestTemporaryPath, $requestFullPath)
    }
    finally {
        if (Test-Path -LiteralPath $requestTemporaryPath -PathType Leaf) {
            Remove-Item -LiteralPath $requestTemporaryPath -Force -ErrorAction SilentlyContinue
        }
    }

    $requestPattern = 'HENKA_AUTOMATION_DIAGNOSTIC app_frame_capture status=requested sequence=(?<sequence>[1-9][0-9]*) label=' +
        [Regex]::Escape($label) + '(?:\r?$)'
    if (-not (Wait-FileContains `
            -Path $StdoutPath `
            -Pattern $requestPattern `
            -TimeoutMilliseconds $TimeoutMilliseconds)) {
        if ($Process.HasExited) {
            throw "$Description process exited before acknowledging its product framebuffer request."
        }
        throw "$Description was not acknowledged by the product framebuffer capture path."
    }
    $captureMatch = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $requestPattern
    if ($null -eq $captureMatch) {
        throw "$Description has no matching product framebuffer sequence."
    }
    $sequence = [uint32]::Parse(
        $captureMatch.Groups['sequence'].Value,
        [Globalization.CultureInfo]::InvariantCulture)
    $framePath = Join-Path $outputFullPath ("frame-{0:D4}-{1}.bmp" -f $sequence, $label)
    $deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    $previousLength = -1L
    $stableSamples = 0
    while ((Get-Date) -lt $deadline) {
        if ($Process.HasExited) {
            throw "$Description process exited before its product framebuffer was complete."
        }
        if (Test-Path -LiteralPath $framePath -PathType Leaf) {
            $currentLength = (Get-Item -LiteralPath $framePath -ErrorAction Stop).Length
            if ($currentLength -gt 54 -and $currentLength -eq $previousLength) {
                $stableSamples++
                if ($stableSamples -ge 2) {
                    break
                }
            }
            else {
                $stableSamples = 0
            }
            $previousLength = $currentLength
        }
        Start-Sleep -Milliseconds 75
    }
    if ($stableSamples -lt 2) {
        throw "$Description did not produce a complete product framebuffer image."
    }

    $bitmap = New-HenkaDetachedBitmapFromFile `
        -Path $framePath `
        -MaxDimension 8192 `
        -MaxPixels 16777216
    try {
        Remove-Item -LiteralPath $framePath -Force -ErrorAction Stop
    }
    catch {
        $bitmap.Dispose()
        throw
    }
    return $bitmap
}

function Get-HenkaAutomationScreenshotContentSummary {
    param(
        [Parameter(Mandatory = $true)][System.Drawing.Bitmap]$Bitmap,
        [int]$RegionX = 0,
        [int]$RegionY = 0,
        [int]$RegionWidth = 0,
        [int]$RegionHeight = 0,
        [ValidateRange(8, 512)][int]$MaximumSamplesPerAxis = 128
    )

    if ($Bitmap.Width -le 0 -or $Bitmap.Height -le 0) {
        throw 'A Henka automation screenshot must have positive dimensions.'
    }
    if ($RegionWidth -eq 0) { $RegionWidth = $Bitmap.Width - $RegionX }
    if ($RegionHeight -eq 0) { $RegionHeight = $Bitmap.Height - $RegionY }
    if ($RegionX -lt 0 -or $RegionY -lt 0 -or $RegionWidth -le 0 -or $RegionHeight -le 0 -or
        $RegionX -gt ($Bitmap.Width - $RegionWidth) -or
        $RegionY -gt ($Bitmap.Height - $RegionHeight)) {
        throw 'The Henka screenshot content region was outside the captured image.'
    }

    $stepX = [Math]::Max(1, [int][Math]::Ceiling($RegionWidth / $MaximumSamplesPerAxis))
    $stepY = [Math]::Max(1, [int][Math]::Ceiling($RegionHeight / $MaximumSamplesPerAxis))
    $colorCounts = [System.Collections.Generic.Dictionary[int, int]]::new()
    $sampleCount = 0
    for ($y = $RegionY; $y -lt ($RegionY + $RegionHeight); $y += $stepY) {
        for ($x = $RegionX; $x -lt ($RegionX + $RegionWidth); $x += $stepX) {
            $color = $Bitmap.GetPixel($x, $y).ToArgb()
            if ($colorCounts.ContainsKey($color)) {
                $colorCounts[$color] += 1
            }
            else {
                $colorCounts[$color] = 1
            }
            $sampleCount += 1
        }
    }

    $dominantCount = 0
    foreach ($count in $colorCounts.Values) {
        if ($count -gt $dominantCount) {
            $dominantCount = $count
        }
    }
    $nonDominantSamples = $sampleCount - $dominantCount
    $minimumNonDominantSamples = [Math]::Max(32, [int][Math]::Ceiling($sampleCount * 0.002))
    return [pscustomobject]@{
        SampleCount = $sampleCount
        RegionX = $RegionX
        RegionY = $RegionY
        RegionWidth = $RegionWidth
        RegionHeight = $RegionHeight
        DistinctColorCount = $colorCounts.Count
        NonDominantSamples = $nonDominantSamples
        MinimumNonDominantSamples = $minimumNonDominantSamples
        HasVisibleContent = ($colorCounts.Count -ge 4 -and
            $nonDominantSamples -ge $minimumNonDominantSamples)
    }
}

function Save-HenkaAutomationWindowScreenshot {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Description,
        [switch]$AllowHidden,
        [switch]$RequireVisibleContent
    )

    $isVisible = $Handle -ne [IntPtr]::Zero -and
        [HenkaUiAutomationNative]::IsWindowVisible($Handle)
    if ($Handle -eq [IntPtr]::Zero -or
        (-not $isVisible -and -not $AllowHidden) -or
        ($isVisible -and [HenkaUiAutomationNative]::IsIconic($Handle))) {
        throw "$Description requires a valid non-minimized Henka window in an allowed visibility state; no focus change was attempted."
    }
    Add-Type -AssemblyName System.Drawing
    $geometry = Get-HenkaAutomationWindowGeometry -Handle $Handle
    if ($geometry.Width -le 0 -or $geometry.Height -le 0) {
        throw "$Description has invalid window dimensions."
    }
    $parent = Split-Path -Parent $Path
    [System.IO.Directory]::CreateDirectory($parent) | Out-Null
    $bitmap = New-Object System.Drawing.Bitmap($geometry.Width, $geometry.Height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $dc = $graphics.GetHdc()
        try {
            if (-not [HenkaUiAutomationNative]::PrintWindow($Handle, $dc, 2)) {
                throw "$Description could not be captured by background-safe PrintWindow."
            }
        }
        finally {
            $graphics.ReleaseHdc($dc) | Out-Null
        }
        if ($RequireVisibleContent) {
            $content = Get-HenkaAutomationScreenshotContentSummary `
                -Bitmap $bitmap `
                -RegionX $geometry.ClientOffsetX `
                -RegionY $geometry.ClientOffsetY `
                -RegionWidth $geometry.ClientWidth `
                -RegionHeight $geometry.ClientHeight
            if (-not $content.HasVisibleContent) {
                $contentMessage = ("{0} returned a blank or uniform capture; sampled={1}, distinct_colors={2}, " +
                    "non_dominant_samples={3}, minimum={4}.")
                throw ($contentMessage -f
                    $Description,
                    $content.SampleCount,
                    $content.DistinctColorCount,
                    $content.NonDominantSamples,
                    $content.MinimumNonDominantSamples)
            }
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
    $file = Get-Item -LiteralPath $Path -ErrorAction Stop
    if ($file.Length -le 0) {
        throw "$Description produced an empty screenshot: $Path"
    }
    return [pscustomobject]@{
        Path = $file.FullName
        Length = $file.Length
        Width = $geometry.Width
        Height = $geometry.Height
        ClientWidth = $geometry.ClientWidth
        ClientHeight = $geometry.ClientHeight
        ContentSummary = if ($RequireVisibleContent) { $content } else { $null }
    }
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
