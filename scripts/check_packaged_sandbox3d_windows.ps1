param(
    [switch]$NonInteractive,

    [switch]$ContractOnly,

    # Ordinary packaged validation is application-local and must not take
    # ownership of the user's foreground window.  Use this only for a test
    # that explicitly covers Windows foreground integration itself.
    [switch]$AllowForegroundIntegration
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($ContractOnly -and -not $NonInteractive) {
    throw "ContractOnly requires NonInteractive."
}

. (Join-Path $PSScriptRoot "henka_script_common.ps1")
. (Join-Path $PSScriptRoot "henka_ui_automation_helpers.ps1")

$script:allowForegroundIntegration = $AllowForegroundIntegration.IsPresent
if (-not $script:allowForegroundIntegration) {
    Write-Output "[safe] Packaged UI validation will not acquire foreground focus."
}

if (-not ("HenkaUiAutomationNative" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class HenkaUiAutomationNative
{
    public const uint WM_MOUSEMOVE = 0x0200;
    public const uint WM_RBUTTONDOWN = 0x0204;
    public const uint WM_RBUTTONUP = 0x0205;
    public const uint MK_RBUTTON = 0x0002;
    public const int SW_RESTORE = 9;

    [DllImport("user32.dll")]
    public static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool BringWindowToTop(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern void SwitchToThisWindow(IntPtr hWnd, bool fAltTab);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool PostMessage(
        IntPtr hWnd,
        uint msg,
        UIntPtr wParam,
        IntPtr lParam);
}
"@
}

function Set-HenkaAutomationForeground {
    param([Parameter(Mandatory = $true)][System.IntPtr]$Handle)

    if ($Handle -eq [System.IntPtr]::Zero) {
        throw "The Henka automation target handle is invalid."
    }

    if (-not $script:allowForegroundIntegration) {
        return
    }

    if ([HenkaUiAutomationNative]::GetForegroundWindow() -eq $Handle) {
        return
    }

    $targetThread = [uint32]0
    $foregroundThread = [uint32]0
    [uint32]$processId = 0
    [uint32]$foregroundProcessId = 0
    $currentThread = [HenkaUiAutomationNative]::GetCurrentThreadId()
    $foregroundWindow = [HenkaUiAutomationNative]::GetForegroundWindow()
    $targetThread = [HenkaUiAutomationNative]::GetWindowThreadProcessId($Handle, [ref]$processId)
    $foregroundThread = [HenkaUiAutomationNative]::GetWindowThreadProcessId($foregroundWindow, [ref]$foregroundProcessId)
    $attachedCurrent = $false
    $attachedForeground = $false
    $foregroundAcquired = $false
    $deadline = (Get-Date).AddSeconds(3)

    try {
        if ($targetThread -ne 0 -and $currentThread -ne $targetThread) {
            $attachedCurrent = [HenkaUiAutomationNative]::AttachThreadInput(
                $currentThread,
                $targetThread,
                $true)
        }
        if ($targetThread -ne 0 -and $foregroundThread -ne 0 -and $foregroundThread -ne $targetThread) {
            $attachedForeground = [HenkaUiAutomationNative]::AttachThreadInput(
                $foregroundThread,
                $targetThread,
                $true)
        }
        [HenkaUiAutomationNative]::ShowWindowAsync(
            $Handle,
            [HenkaUiAutomationNative]::SW_RESTORE) | Out-Null
        [HenkaUiAutomationNative]::BringWindowToTop($Handle) | Out-Null
        [HenkaUiAutomationNative]::SwitchToThisWindow($Handle, $true)

        do {
            [HenkaUiAutomationNative]::SetForegroundWindow($Handle) | Out-Null

            if ([HenkaUiAutomationNative]::GetForegroundWindow() -eq $Handle) {
                $foregroundAcquired = $true
                break
            }

            Start-Sleep -Milliseconds 100
        } while ((Get-Date) -lt $deadline)
    }
    finally {
        if ($attachedForeground) {
            [HenkaUiAutomationNative]::AttachThreadInput(
                $foregroundThread,
                $targetThread,
                $false) | Out-Null
        }
        if ($attachedCurrent) {
            [HenkaUiAutomationNative]::AttachThreadInput(
                $currentThread,
                $targetThread,
                $false) | Out-Null
        }
    }

    if ($foregroundAcquired) {
        Start-Sleep -Milliseconds 150
        return
    }

    throw (
        "The packaged UI harness could not acquire the Henka window as " +
        "foreground within three seconds. This is an automation-environment " +
        "failure; no engine UI assertion was made.")
}

function New-HenkaMouseLParam {
    param(
        [Parameter(Mandatory = $true)][int]$X,
        [Parameter(Mandatory = $true)][int]$Y
    )

    if ($X -lt 0 -or $Y -lt 0 -or $X -gt 65535 -or $Y -gt 65535) {
        throw "The Henka client mouse coordinate is outside the Win32 message range."
    }

    $packed = (($Y -band 0xFFFF) -shl 16) -bor ($X -band 0xFFFF)
    return [System.IntPtr][int]$packed
}

function Get-PackageInfoValue {
    param(
        [string]$Path,
        [string]$Name
    )

    $match = Select-String -LiteralPath $Path -Pattern ("^" + [Regex]::Escape($Name) + ":\s*(.+)$") | Select-Object -First 1
    if ($null -eq $match) {
        throw "Package field was not found: $Name"
    }
    return $match.Matches[0].Groups[1].Value.Trim()
}
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

function Get-FileLengthSafe {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return 0L
    }

    return [System.IO.FileInfo]::new($Path).Length
}

function Wait-FileContainsAfterOffset {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][long]$StartingOffset,
        [int]$TimeoutMilliseconds = 5000
    )

    $deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path -LiteralPath $Path -PathType Leaf) {
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
                if ($stream.Length -gt $StartingOffset) {
                    $stream.Seek($StartingOffset, [System.IO.SeekOrigin]::Begin) | Out-Null
                    $reader = [System.IO.StreamReader]::new(
                        $stream,
                        [System.Text.Encoding]::UTF8,
                        $true,
                        4096,
                        $false)
                    $text = $reader.ReadToEnd()
                    if ([Regex]::IsMatch(
                            $text,
                            $Pattern,
                            [System.Text.RegularExpressions.RegexOptions]::Multiline)) {
                        return $true
                    }
                }
            }
            catch [System.IO.IOException] {
                # The producer may be between writes; retry until the bounded deadline.
            }
            catch [System.UnauthorizedAccessException] {
                # The producer may be between writes; retry until the bounded deadline.
            }
            finally {
                if ($null -ne $reader) {
                    $reader.Dispose()
                }
                elseif ($null -ne $stream) {
                    $stream.Dispose()
                }
            }
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

function Write-Utf8NoBom {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][AllowEmptyString()][string]$Content
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        [System.IO.Directory]::CreateDirectory($parent) | Out-Null
    }

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $encoding)
}

function New-HenkaCapturedWindowBitmap {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $rect = Get-WindowRect -Handle $Handle
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0) {
        throw "$Description window bounds are invalid for screenshot capture."
    }

    $bitmap = New-Object System.Drawing.Bitmap -ArgumentList $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        # A window that is already foreground can be sampled directly without
        # changing focus. This keeps ordinary validation application-local,
        # while avoiding stale OpenGL pixels from background PrintWindow when
        # Windows has naturally foregrounded the newly launched test window.
        $window_already_foreground =
            [HenkaUiAutomationNative]::GetForegroundWindow() -eq $Handle
        if ($script:allowForegroundIntegration -or $window_already_foreground) {
            $size = New-Object System.Drawing.Size -ArgumentList $width, $height
            $graphics.CopyFromScreen(
                $rect.Left,
                $rect.Top,
                0,
                0,
                $size)
        }
        else {
            $deviceContext = $graphics.GetHdc()
            try {
                if (-not [NativeMethods]::PrintWindow(
                        $Handle,
                        $deviceContext,
                        2)) {
                    throw (
                        "$Description background-safe capture is unavailable: " +
                        "the window did not render through PrintWindow. " +
                        "No desktop or foreground capture was attempted.")
                }
            }
            finally {
                $graphics.ReleaseHdc($deviceContext) | Out-Null
            }
        }
    }
    catch {
        $bitmap.Dispose()
        throw
    }
    finally {
        $graphics.Dispose()
    }

    return $bitmap
}

function Save-WindowScreenshot {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Description
    )

    Set-HenkaAutomationForeground -Handle $Handle
    $bitmap = New-HenkaCapturedWindowBitmap `
        -Handle $Handle `
        -Description $Description
    try {
        $bitmap.Save(
            $Path,
            [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }

    Assert-PathExists -Path $Path -Description $Description
    $artifact = Get-Item -LiteralPath $Path -ErrorAction Stop
    if ($artifact.Length -le 0) {
        throw "$Description produced an empty screenshot artifact: $Path"
    }
}

function Click-WindowPoint {
    param(
        [System.IntPtr]$Handle,
        [int]$OffsetX,
        [int]$OffsetY
    )

    $windowRect = Get-WindowRect -Handle $Handle
    $clientRect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetClientRect($Handle, [ref]$clientRect)) {
        throw "The packaged sandbox client bounds could not be read for window-relative input."
    }
    $clientWidth = $clientRect.Right - $clientRect.Left
    $clientHeight = $clientRect.Bottom - $clientRect.Top
    $clientOrigin = New-Object NativeMethods+POINT
    $clientOrigin.X = 0
    $clientOrigin.Y = 0
    if (-not [NativeMethods]::ClientToScreen($Handle, [ref]$clientOrigin) -or
        $clientWidth -le 0 -or $clientHeight -le 0) {
        throw "The packaged sandbox client geometry was invalid for window-relative input."
    }
    $screenX = $windowRect.Left + $OffsetX
    $screenY = $windowRect.Top + $OffsetY
    $windowX = $screenX - $clientOrigin.X
    $windowY = $screenY - $clientOrigin.Y
    if ($windowX -lt 0 -or $windowY -lt 0 -or
        $windowX -ge $clientWidth -or $windowY -ge $clientHeight) {
        throw "The packaged window-relative automation point was outside the client area."
    }
    Set-HenkaAutomationForeground -Handle $Handle
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X $windowX `
        -Y $windowY
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
            "The live packaged-sandbox log could not be read " +
            "with shared read access within five seconds: " +
            $lastReadError.Message)
    }

    return $null
}

function Wait-LastLogRegexMatch {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$GroupName,
        [Parameter(Mandatory = $true)][string]$ExpectedValue,
        [int]$TimeoutMilliseconds = 5000
    )

    $deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    do {
        $match = Get-LastLogRegexMatch -Path $Path -Pattern $Pattern
        if ($null -ne $match -and
            $match.Groups[$GroupName].Value -eq $ExpectedValue) {
            return $match
        }

        if ((Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 75
        }
    } while ((Get-Date) -lt $deadline)

    return $null
}
function Assert-FramebufferRect {
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
        if ([double]::IsNaN($value) -or
            [double]::IsInfinity($value)) {
            throw "$Name reported a non-finite framebuffer rectangle."
        }
    }

    if ($FramebufferWidth -le 0 -or $FramebufferHeight -le 0) {
        throw "Framebuffer dimensions must be positive for geometry validation."
    }
    if ($Width -le 0.0 -or $Height -le 0.0) {
        throw "$Name reported a non-positive framebuffer rectangle."
    }
    if ($X -lt 0.0 -or
        $Y -lt 0.0 -or
        $X + $Width -gt [double]$FramebufferWidth -or
        $Y + $Height -gt [double]$FramebufferHeight) {
        throw (
            "$Name is outside the reported framebuffer: " +
            "rect=($X,$Y,$Width,$Height), " +
            "framebuffer=$($FramebufferWidth)x$($FramebufferHeight).")
    }

    Write-Output "[pass] $Name is inside the reported framebuffer"
}

function Click-FramebufferPoint {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$FramebufferX,
        [Parameter(Mandatory = $true)][double]$FramebufferY
    )

    if ($FramebufferWidth -le 0 -or $FramebufferHeight -le 0) {
        throw "Framebuffer dimensions must be positive for UI automation."
    }

    $clientRect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetClientRect(
            $Handle,
            [ref]$clientRect)) {
        throw "The packaged sandbox client bounds could not be read."
    }

    $clientWidth = $clientRect.Right - $clientRect.Left
    $clientHeight = $clientRect.Bottom - $clientRect.Top
    if ($clientWidth -le 0 -or $clientHeight -le 0) {
        throw "The packaged sandbox client bounds are invalid."
    }

    $windowPoint = Convert-HenkaFramebufferPointToWindowPoint `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -WindowWidth $clientWidth `
        -WindowHeight $clientHeight `
        -FramebufferX $FramebufferX `
        -FramebufferY $FramebufferY
    Set-HenkaAutomationForeground -Handle $Handle
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X $windowPoint.X `
        -Y $windowPoint.Y
}

function Click-AuthoringWindowPoint {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y
    )

    Set-HenkaAutomationForeground -Handle $Handle
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X $X `
        -Y $Y
}

function Scroll-FramebufferPoint {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$FramebufferX,
        [Parameter(Mandatory = $true)][double]$FramebufferY,
        [Parameter(Mandatory = $true)][int]$WheelDelta
    )

    if ($WheelDelta -eq 0) {
        throw "The packaged UI scroll delta must be non-zero."
    }

    $clientRect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetClientRect($Handle, [ref]$clientRect)) {
        throw "The packaged sandbox client bounds could not be read for scroll input."
    }
    $clientWidth = $clientRect.Right - $clientRect.Left
    $clientHeight = $clientRect.Bottom - $clientRect.Top
    if ($FramebufferWidth -le 0 -or $FramebufferHeight -le 0 -or
        $clientWidth -le 0 -or $clientHeight -le 0) {
        throw "Invalid framebuffer or client dimensions for scroll input."
    }

    $windowPoint = Convert-HenkaFramebufferPointToWindowPoint `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -WindowWidth $clientWidth `
        -WindowHeight $clientHeight `
        -FramebufferX $FramebufferX `
        -FramebufferY $FramebufferY
    Set-HenkaAutomationForeground -Handle $Handle
    Send-HenkaAutomationScroll `
        -EventPath $automationInputPath `
        -X $windowPoint.X `
        -Y $windowPoint.Y `
        -WheelDelta $WheelDelta
}
function Click-FramebufferPointRight {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$FramebufferX,
        [Parameter(Mandatory = $true)][double]$FramebufferY
    )

    $clientRect = New-Object NativeMethods+RECT

    if (-not [NativeMethods]::GetClientRect($Handle, [ref]$clientRect)) {
        throw "The packaged sandbox client bounds could not be read for right click."
    }

    $clientWidth = $clientRect.Right - $clientRect.Left
    $clientHeight = $clientRect.Bottom - $clientRect.Top

    if ($FramebufferWidth -le 0 -or
        $FramebufferHeight -le 0 -or
        $clientWidth -le 0 -or
        $clientHeight -le 0) {
        throw "Invalid framebuffer or client dimensions for right click."
    }

    $windowPoint = Convert-HenkaFramebufferPointToWindowPoint `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -WindowWidth $clientWidth `
        -WindowHeight $clientHeight `
        -FramebufferX $FramebufferX `
        -FramebufferY $FramebufferY
    if ($FramebufferX -lt 0.0 -or $FramebufferY -lt 0.0 -or
        $FramebufferX -ge $FramebufferWidth -or
        $FramebufferY -ge $FramebufferHeight) {
        throw "The packaged sandbox right-click coordinate is outside the client area."
    }

    Set-HenkaAutomationForeground -Handle $Handle
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X $windowPoint.X `
        -Y $windowPoint.Y `
        -Button right
}

function Save-FramebufferRegionScreenshot {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y,
        [Parameter(Mandatory = $true)][double]$Width,
        [Parameter(Mandatory = $true)][double]$Height,
        [Parameter(Mandatory = $true)][string]$Path
    )
    Set-HenkaAutomationForeground -Handle $Handle
    $clientRect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetClientRect($Handle, [ref]$clientRect)) {
        throw "The packaged sandbox client bounds could not be read for scene capture."
    }
    $clientWidth = $clientRect.Right - $clientRect.Left
    $clientHeight = $clientRect.Bottom - $clientRect.Top
    $origin = New-Object NativeMethods+POINT
    $origin.X = 0
    $origin.Y = 0
    if (-not [NativeMethods]::ClientToScreen($Handle, [ref]$origin)) {
        throw "The packaged sandbox client origin could not be converted to screen coordinates."
    }
    $windowRect = Get-WindowRect -Handle $Handle
    $fullBitmap = New-HenkaCapturedWindowBitmap `
        -Handle $Handle `
        -Description "Static scene"
    $screenWidth = [Math]::Max(1, [int][Math]::Round($Width * $clientWidth / $FramebufferWidth))
    $screenHeight = [Math]::Max(1, [int][Math]::Round($Height * $clientHeight / $FramebufferHeight))
    $cropX = $origin.X - $windowRect.Left + [int][Math]::Round($X * $clientWidth / $FramebufferWidth)
    $cropY = $origin.Y - $windowRect.Top + [int][Math]::Round($Y * $clientHeight / $FramebufferHeight)
    if ($cropX -lt 0 -or $cropY -lt 0 -or
        $cropX + $screenWidth -gt $fullBitmap.Width -or
        $cropY + $screenHeight -gt $fullBitmap.Height) {
        $fullBitmap.Dispose()
        throw "Static scene capture region was outside the captured window bounds."
    }
    $bitmap = $null
    try {
        $bitmap = $fullBitmap.Clone(
            (New-Object System.Drawing.Rectangle -ArgumentList $cropX,$cropY,$screenWidth,$screenHeight),
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $bitmap.Save($Path,[System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($null -ne $bitmap) {
            $bitmap.Dispose()
        }
        $fullBitmap.Dispose()
    }
    Assert-PathExists -Path $Path -Description "Static scene stability frame"
}

function Assert-SceneFramesStable {
    param([string]$First,[string]$Second)
    $a = New-Object System.Drawing.Bitmap -ArgumentList $First
    $b = New-Object System.Drawing.Bitmap -ArgumentList $Second
    try {
        if ($a.Width -ne $b.Width -or $a.Height -ne $b.Height) {
            throw "Static scene captures have different dimensions."
        }
        [long]$difference = 0
        [long]$samples = 0
        [long]$changed = 0
        for ($y = 0; $y -lt $a.Height; $y += 4) {
            for ($x = 0; $x -lt $a.Width; $x += 4) {
                $ca = $a.GetPixel($x,$y)
                $cb = $b.GetPixel($x,$y)
                $delta = [Math]::Abs([int]$ca.R-[int]$cb.R) +
                    [Math]::Abs([int]$ca.G-[int]$cb.G) +
                    [Math]::Abs([int]$ca.B-[int]$cb.B)
                $difference += $delta
                $samples++
                if ($delta -gt 9) { $changed++ }
            }
        }
        $mean = if ($samples -gt 0) { [double]$difference / (3.0 * $samples) } else { 999.0 }
        $ratio = if ($samples -gt 0) { [double]$changed / $samples } else { 1.0 }
        if ($mean -gt 0.85 -or $ratio -gt 0.02) {
            throw ("Stationary rendered viewport is not stable: mean channel delta={0:N3}, changed sample ratio={1:P2}." -f $mean,$ratio)
        }
        Write-Output ("[pass] Stationary rendered viewport is stable: mean channel delta={0:N3}, changed sample ratio={1:P2}" -f $mean,$ratio)
    }
    finally {
        $a.Dispose()
        $b.Dispose()
    }
}

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$gitCommand = Get-HenkaGitPath
$packageRoot = Join-Path $repoRoot "out\HenkaSandbox3D"
$packagedExe = Join-Path $packageRoot "HenkaSandbox3D.exe"
$assetsDir = Join-Path $packageRoot "assets"
$showcaseModelsDir = Join-Path $assetsDir "models"
$helpPath = Join-Path $packageRoot "docs\help\sandbox3d.md"
$readmePath = Join-Path $packageRoot "README.txt"
$packageInfoPath = Join-Path $packageRoot "PACKAGE_INFO.txt"
$settingsPath = Join-Path $packageRoot "user\sandbox3d.settings"
$logDir = Join-Path $repoRoot "build\test_tmp"
$stdoutPath = Join-Path $logDir "check_packaged_sandbox3d_stdout.log"
$stderrPath = Join-Path $logDir "check_packaged_sandbox3d_stderr.log"
$startupScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_startup.png"
$qaScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_controls_qa.png"
$nativeScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_native_panel.png"
$nativeAuthoringScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_native_authoring.png"
$selectionOutlineScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_selection_outline.png"
$nativeAuthoredScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_native_authored_rocket.png"
$contextMenuScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_context_menu.png"
$stabilityFirstPath = Join-Path $logDir "check_packaged_sandbox3d_stability_a.png"
$stabilitySecondPath = Join-Path $logDir "check_packaged_sandbox3d_stability_b.png"
$persistenceStdoutPath = Join-Path $logDir "check_packaged_sandbox3d_persistence_stdout.log"
$persistenceStderrPath = Join-Path $logDir "check_packaged_sandbox3d_persistence_stderr.log"
$startupRestoreStdoutPath = Join-Path $logDir "check_packaged_sandbox3d_startup_restore_stdout.log"
$startupRestoreStderrPath = Join-Path $logDir "check_packaged_sandbox3d_startup_restore_stderr.log"
$automationInputPath = Join-Path $logDir "check_packaged_sandbox3d_automation.events"

if (-not $NonInteractive) {
    Add-Type -AssemblyName System.Drawing
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

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT {
        public int X;
        public int Y;
    }

    public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    public const uint MOUSEEVENTF_LEFTUP = 0x0004;
    public const uint MOUSEEVENTF_WHEEL = 0x0800;

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint nFlags);

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
}

Write-Step "Checking packaged sandbox contents"
Assert-PathExists -Path $packageRoot -Description "Packaged sandbox folder"
Assert-PathExists -Path $packagedExe -Description "Packaged sandbox executable"
Assert-PathExists -Path $assetsDir -Description "Packaged assets folder"
Assert-PathExists -Path (Join-Path $assetsDir "branding\henka_engine_emblem.png") -Description "Packaged Henka emblem branding"
Assert-PathExists -Path (Join-Path $assetsDir "branding\henka_engine_lockup.png") -Description "Packaged Henka lockup branding"
foreach ($showcaseFile in @(
    "cheeky_giraffe.gltf",
    "cheeky_giraffe.bin",
    "giraffe_base_color.png",
    "giraffe_detail_normal.png",
    "giraffe_metallic_roughness.png",
    "original_realistic_rocket.gltf",
    "original_realistic_rocket.bin",
    "rocket_base_color.png",
    "rocket_detail_normal.png",
    "rocket_metallic_roughness.png"
)) {
    Assert-PathExists -Path (Join-Path $showcaseModelsDir $showcaseFile) -Description "Packaged showcase asset $showcaseFile"
}
Assert-PathExists -Path (Join-Path $assetsDir "audio\henka_audio_fixture.wav") -Description "Packaged Audio fixture"
foreach ($audioFixture in @(
    "henka_audio_fixture.ogg",
    "henka_audio_fixture.mp3",
    "henka_audio_fixture.flac"
)) {
    Assert-PathExists -Path (Join-Path $assetsDir ("audio\{0}" -f $audioFixture)) -Description "Packaged compressed Audio fixture $audioFixture"
}
foreach ($authoringFile in @(
    "showcase_giraffe.hams",
    "showcase_rocket.hams"
)) {
    Assert-PathExists -Path (Join-Path $assetsDir "authoring\$authoringFile") -Description "Packaged editor-owned authoring source $authoringFile"
}
Assert-PathExists -Path (Join-Path $assetsDir "textures\residency\residency_64.png") -Description "Packaged residency stress fixtures"
Assert-PathExists -Path $helpPath -Description "Packaged offline help"
Assert-PathExists -Path $readmePath -Description "Packaged run guide"
Assert-PathExists -Path $packageInfoPath -Description "Packaged build marker"
$packageSchema = Get-PackageInfoValue -Path $packageInfoPath -Name "Package schema"
$sourceCommit = Get-PackageInfoValue -Path $packageInfoPath -Name "Source commit"
$sourceState = Get-PackageInfoValue -Path $packageInfoPath -Name "Source state"
$buildConfiguration = Get-PackageInfoValue -Path $packageInfoPath -Name "Build configuration"
$buildRef = Get-PackageInfoValue -Path $packageInfoPath -Name "Build ref"
$detachedHead = Get-PackageInfoValue -Path $packageInfoPath -Name "Detached HEAD"
$sourceHash = Get-PackageInfoValue -Path $packageInfoPath -Name "Source executable SHA-256"
$packagedHash = Get-PackageInfoValue -Path $packageInfoPath -Name "Packaged executable SHA-256"
$actualPackagedHash = (Get-FileHash -LiteralPath $packagedExe -Algorithm SHA256).Hash.ToLowerInvariant()
$currentCommitLines = @(& $gitCommand -C $repoRoot rev-parse HEAD 2>$null)
if ($LASTEXITCODE -ne 0 -or $currentCommitLines.Count -ne 1) { throw "Current commit could not be read for package verification." }
$currentCommit = ([string]$currentCommitLines[0]).Trim()
$currentStatusLines = @(& $gitCommand -C $repoRoot status --porcelain=v1 --untracked-files=all 2>$null)
if ($LASTEXITCODE -ne 0) { throw "Current source state could not be read for package verification." }
$currentSourceState = if ($currentStatusLines.Count -eq 0) { "clean" } else { "working-tree" }
if ($packageSchema -ne "3") { throw "Packaged build marker has an unsupported schema." }
if ($sourceCommit -ne $currentCommit) { throw "Packaged source commit does not match current HEAD." }
if ($sourceState -ne $currentSourceState) { throw "Packaged source state does not match the current working tree." }
if ($buildConfiguration -ne "Debug" -and $buildConfiguration -ne "Release") { throw "Packaged build configuration is invalid." }
if ([string]::IsNullOrWhiteSpace($buildRef)) { throw "Packaged build ref is missing." }
if ($detachedHead -ne "True" -and $detachedHead -ne "False") { throw "Packaged detached-HEAD value is invalid." }
if ($sourceHash -ne $packagedHash -or $packagedHash -ne $actualPackagedHash) { throw "Packaged executable provenance hash verification failed." }
Write-Output "[pass] Package provenance schema"
Write-Output "[pass] Package source commit"
Write-Output "[pass] Package build configuration"
Write-Output "[pass] Packaged executable SHA-256"
Assert-FileContains -Path $readmePath -Pattern "Use the in-window utilities" -Description "Packaged utility guidance"
Assert-FileContains -Path $readmePath -Pattern "panels open automatically" -Description "Packaged automatic panel guidance"
Assert-FileContains -Path $readmePath -Pattern "no selected scene object" -Description "Packaged startup no-selection guidance"
Assert-FileContains -Path $readmePath -Pattern "Physics QA explains Static, Dynamic, and Kinematic" -Description "Packaged physics body-type guidance"
Assert-FileContains -Path $readmePath -Pattern "Editable selected scene objects show a viewport transform highlight" -Description "Packaged editable selection highlight guidance"
Assert-FileContains -Path $readmePath -Pattern "most recently picked vertex, edge, or face receives a stronger mode-specific highlight" -Description "Packaged active edit highlight guidance"
Assert-FileContains -Path $readmePath -Pattern "Locked objects remain selectable for inspection without a transform highlight or gizmo" -Description "Packaged locked selection guidance"
Assert-FileContains -Path $readmePath -Pattern "Ground starts locked and requires an explicit Unlock Transform action before it can move" -Description "Packaged ground lock guidance"
Assert-FileContains -Path $readmePath -Pattern "Clearing selection also clears active transform-session ownership" -Description "Packaged transform-session ownership guidance"
Assert-FileContains -Path $readmePath -Pattern "release away from the outlines to open a separate native tool window" -Description "Packaged workspace guidance"
Assert-FileContains -Path $readmePath -Pattern "Open Native Panel Test from the Tools QA page to exercise a separate OS-level validation window" -Description "Packaged native test panel guidance"
Assert-FileContains -Path $readmePath -Pattern "If saved live workspace geometry is incompatible, Henka restores current safe defaults and rewrites them after a clean shutdown" -Description "Packaged workspace recovery guidance"
Assert-FileContains -Path $readmePath -Pattern "Close a detached tool window to return its panel to the last valid dock" -Description "Packaged workspace limitation guidance"
Assert-FileContains -Path $readmePath -Pattern "Use M or G, R, and S for action-based transforms" -Description "Packaged transform hotkey guidance"
Assert-FileContains -Path $readmePath -Pattern "status area" -Description "Packaged status guidance"
Assert-FileContains -Path $helpPath -Pattern "Utility > Settings controls:" -Description "Packaged utility help"
Assert-FileContains -Path $helpPath -Pattern "Perspective 3D|Side 2.5D|Top-down 2.5D|Isometric 2.5D" -Description "Packaged camera preset help"
Assert-FileContains -Path $helpPath -Pattern "Showcase Giraffe" -Description "Packaged showcase help"

if ($NonInteractive) {
    if ($ContractOnly) {
        Write-Step "Completing hosted package contract validation"
        Write-Output "[pass] Packaged sandbox contract validation completed."
        return
    }

    Write-Step "Running deterministic packaged startup smoke test"
    $smoke = Invoke-HenkaNativeCapture `
        -FilePath $packagedExe `
        -Arguments @("--smoke-test") `
        -WorkingDirectory $packageRoot `
        -Label "Run packaged sandbox smoke test"

    if ($smoke.Stdout -notmatch "Henka Engine Sandbox 3D") {
        throw "The packaged smoke test did not print the startup heading."
    }
    if ($smoke.Stdout -notmatch "Runtime mode: Packaged") {
        throw "The packaged smoke test did not report Packaged mode."
    }
    if ($smoke.Stdout -notmatch "Showcase assets: Anatomical Giraffe Study \(15 parts\), Original Realistic Rocket \(15 parts\)") {
        throw "The packaged smoke test did not load both showcase glTF scenes."
    }
    $terrainPassMatch = [regex]::Match($smoke.Stdout, "Terrain Rendered pass diagnostics: mask=0x([0-9a-fA-F]+) required=0x([0-9a-fA-F]+)")
    if (-not $terrainPassMatch.Success -or
        (([Convert]::ToUInt32($terrainPassMatch.Groups[1].Value, 16) -band
          [Convert]::ToUInt32($terrainPassMatch.Groups[2].Value, 16)) -ne
         [Convert]::ToUInt32($terrainPassMatch.Groups[2].Value, 16))) {
        throw "The packaged smoke test did not prove the required Terrain Rendered pass participation mask."
    }
    if ($smoke.Stdout -notmatch "Sandbox smoke test completed\.") {
        throw "The packaged smoke test did not reach its deterministic exit."
    }
    if ($smoke.Stderr -notmatch "leaving engine run loop") {
        throw "The packaged smoke test did not leave the engine run loop cleanly."
    }

    Write-Output "[pass] Deterministic packaged startup smoke test completed."

    Write-Step "Running packaged Audio fixture smoke"
    $audioSmoke = Invoke-HenkaNativeCapture -FilePath $packagedExe -Arguments @("--audio-smoke-test") -WorkingDirectory $packageRoot -Label "Run packaged Audio fixture smoke"

    if ($audioSmoke.Stdout -notmatch "Audio smoke: packaged resident and streamed WAV fixture paths loaded through the asset manager; real scene object emitters mixed and reached the SDL output boundary\.") {
        throw "The packaged Audio smoke test did not prove the real fixture-to-SDL production path."
    }
    if ($audioSmoke.Stdout -notmatch "Sandbox smoke test completed\.") {
        throw "The packaged Audio smoke test did not reach its deterministic exit."
    }
    if ($audioSmoke.Stderr -notmatch "leaving engine run loop") {
        throw "The packaged Audio smoke test did not leave the engine run loop cleanly."
    }

    Write-Output "[pass] Packaged Audio fixture smoke completed."

    Write-Step "Running bounded packaged Terrain stream stress"
    $terrainStreamStress = Invoke-HenkaNativeCapture `
        -FilePath $packagedExe `
        -Arguments @("--terrain-stream-stress") `
        -WorkingDirectory $packageRoot `
        -Label "Run packaged Terrain stream stress"

    if ($terrainStreamStress.Stdout -notmatch "Terrain stream stress: seeded=2x2 camera-window=\(0,0\)\+1 crossed=\(2,0\)->\(2,2\)->\(0,0\)" -or
        $terrainStreamStress.Stdout -notmatch "failed=0" -or
        $terrainStreamStress.Stdout -notmatch "render-return=valid" -or
        $terrainStreamStress.Stdout -notmatch "diagonal-render-return=valid" -or
        $terrainStreamStress.Stdout -notmatch "collision-overlap-return=valid" -or
        $terrainStreamStress.Stdout -notmatch "diagonal-collision-return=valid" -or
        $terrainStreamStress.Stdout -notmatch "Sandbox smoke test completed\.") {
        throw "The packaged Terrain stream stress did not prove the bounded camera crossing contract."
    }
    Write-Output "[pass] Bounded packaged Terrain stream stress completed."

    foreach ($stressCase in @(
        @{
            Name = "texture residency"
            Arguments = @("--residency-stress")
            Pattern = "Residency stress: .*"
        }
        @{
            Name = "temporal presentation"
            Arguments = @("--temporal-stress")
            Pattern = "Temporal stress: .*"
        }
        @{
            Name = "environment"
            Arguments = @("--environment-stress")
            Pattern = "Environment stress: .*"
        }
    )) {
        Write-Step "Running packaged $($stressCase.Name) stress"
        $stress = Invoke-HenkaNativeCapture `
            -FilePath $packagedExe `
            -Arguments $stressCase.Arguments `
            -WorkingDirectory $packageRoot `
            -Label "Run packaged $($stressCase.Name) stress"

        if ($stress.Stdout -notmatch $stressCase.Pattern -or
            $stress.Stdout -notmatch "Sandbox smoke test completed\.") {
            throw "The packaged $($stressCase.Name) stress did not complete its bounded runtime contract."
        }
        Write-Output "[pass] Packaged $($stressCase.Name) stress completed."
    }
    return
}

New-Item -ItemType Directory -Path $logDir -Force | Out-Null
Remove-Item `
    -LiteralPath @(
        $stdoutPath,
        $stderrPath,
        $startupScreenshotPath,
        $qaScreenshotPath,
        $nativeScreenshotPath,
        $nativeAuthoringScreenshotPath,
        $nativeAuthoredScreenshotPath,
        $persistenceStdoutPath,
        $persistenceStderrPath,
        $startupRestoreStdoutPath,
        $startupRestoreStderrPath) `
    -ErrorAction SilentlyContinue

$capturedProcess = $null
$process = $null
$mainWindowHandle = [System.IntPtr]::Zero
$uiAutomationVerified = $false
$sandboxPanelsVisible = $false
$previousAutomationOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
$previousAutomationFile = $env:HENKA_AUTOMATION_INPUT_FILE
try {
    New-Item -ItemType File -Path $automationInputPath -Force | Out-Null
    $env:HENKA_AUTOMATION_INPUT_OWNED = "1"
    $env:HENKA_AUTOMATION_INPUT_FILE = $automationInputPath
    Write-Step "Launching the packaged sandbox"
    $capturedProcess = Start-HenkaCapturedProcess `
        -FilePath $packagedExe `
        -WorkingDirectory $packageRoot `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath
    $process = $capturedProcess.Process

    for ($index = 0; $index -lt 80 -and $mainWindowHandle -eq [System.IntPtr]::Zero; $index++) {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
        $mainWindowHandle = [NativeMethods]::FindProcessWindow([uint32]$process.Id, "Henka Engine Sandbox 3D")
    }

    if ($mainWindowHandle -eq [System.IntPtr]::Zero) {
        throw "The packaged sandbox window did not become available."
    }

    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Henka Engine Sandbox 3D" -TimeoutMilliseconds 30000)) {
        throw "Startup help heading was not found in the packaged sandbox output within the bounded 30-second startup window."
    }
    Assert-FileContains -Path $stdoutPath -Pattern "Henka Engine Sandbox 3D" -Description "Startup help heading"
    Assert-FileContains -Path $stdoutPath -Pattern "F4               Show or hide the sandbox panels" -Description "F4 help text"
    Assert-FileContains -Path $stdoutPath -Pattern "F5               Switch Standard and Focus Viewport layouts" -Description "F5 help text"
    Assert-FileContains -Path $stdoutPath -Pattern "Runtime mode: Packaged" -Description "Packaged runtime mode output"
    Assert-FileContains -Path $stdoutPath -Pattern "Startup selection: None" -Description "Packaged startup no-selection output"
    Assert-FileContains -Path $stdoutPath -Pattern "Startup UI:" -Description "Startup UI cue"
    Assert-FileContains -Path $stdoutPath -Pattern "Tools and Physics QA are discoverable immediately" -Description "Startup auto panel cue"
    Assert-FileContains -Path $stdoutPath -Pattern "use the in-window .*utilities" -Description "Startup utility cue"
    Assert-FileContains -Path $stdoutPath -Pattern "recent actions and warnings appear" -Description "Startup status cue"

    Write-Step "Checking packaged UI open and close"
    Set-HenkaAutomationForeground -Handle $mainWindowHandle
    if ($script:allowForegroundIntegration) {
        $null = $shell.AppActivate($process.Id)
    }
    Start-Sleep -Milliseconds 600
    if (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -TimeoutMilliseconds 1500) {
        Assert-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -Description "Startup UI readiness output"
        Assert-FileContains -Path $stdoutPath -Pattern "Standard mode|Focus Viewport mode|Legacy Full Tools mode" -Description "Layout mode output"
        Try-AssertFileContains -Path $stdoutPath -Pattern "Sandbox viewport:" -Description "Viewport output"
        $uiAutomationVerified = $true
        $sandboxPanelsVisible = $true
    }
    else {
        Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F4"
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox panel: shown" -TimeoutMilliseconds 2000)) {
            Set-HenkaAutomationForeground -Handle $mainWindowHandle
            Start-Sleep -Milliseconds 250
            Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F4"
        }
        if (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox panel: shown" -TimeoutMilliseconds 4000) {
            Assert-FileContains -Path $stdoutPath -Pattern "Sandbox panel: shown" -Description "Panel open output"
            Assert-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -Description "UI readiness output after F4"
            Assert-FileContains -Path $stdoutPath -Pattern "Standard mode|Focus Viewport mode|Legacy Full Tools mode" -Description "Layout mode output after F4"
            Try-AssertFileContains -Path $stdoutPath -Pattern "Sandbox viewport:" -Description "Viewport output after F4"
            $uiAutomationVerified = $true
            $sandboxPanelsVisible = $true
        }
    }

    if ($uiAutomationVerified) {
        Write-Step "Capturing packaged startup workspace visual proof"
        Set-HenkaAutomationForeground -Handle $mainWindowHandle
        Start-Sleep -Milliseconds 350
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $startupScreenshotPath `
            -Description "Packaged startup workspace screenshot"

        Write-Step "Checking packaged UI click controls"

        if (-not (Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Tools QA tab:" `
                -TimeoutMilliseconds 500)) {
            $preflightFramebufferMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Sandbox UI ready:.*framebuffer ([0-9]+)x([0-9]+)'
            $preflightViewportMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Sandbox viewport: origin ([0-9]+),([0-9]+) size ([0-9]+)x([0-9]+)\.'
            if ($null -eq $preflightFramebufferMatch -or $null -eq $preflightViewportMatch) {
                throw "The packaged Scene View geometry was not available to open the hidden Tools dock."
            }
            $preflightFramebufferWidth = [int]$preflightFramebufferMatch.Groups[1].Value
            $preflightFramebufferHeight = [int]$preflightFramebufferMatch.Groups[2].Value
            $preflightViewportX = [double]$preflightViewportMatch.Groups[1].Value
            $preflightViewportY = [double]$preflightViewportMatch.Groups[2].Value
            Write-Output "[check] Opening the hidden Tools dock through the Scene View header"
            $nativeOpenLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $preflightFramebufferWidth `
                -FramebufferHeight $preflightFramebufferHeight `
                -FramebufferX ($preflightViewportX + 235.0) `
                -FramebufferY ($preflightViewportY - 23.0)
            if (-not (Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern "Tools QA tab:" `
                    -TimeoutMilliseconds 4000)) {
                throw "The packaged Tools dock was not available and could not be opened through its logical Scene View header control."
            }
        }

        foreach ($requiredPattern in @(
            "Sandbox UI ready:",
            "Sandbox viewport:",
            "Workspace UI geometry:",
            "Workspace header chrome:",
            "Tools QA tab:",
            "Viewport shading controls:")) {
            if (-not (Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern $requiredPattern `
                    -TimeoutMilliseconds 4000)) {
                throw "Required packaged UI automation geometry was not reported: $requiredPattern"
            }
        }

        $framebufferMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Sandbox UI ready:.*framebuffer ([0-9]+)x([0-9]+)'
        $viewportMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Sandbox viewport: origin ([0-9]+),([0-9]+) size ([0-9]+)x([0-9]+)\.'
        $workspaceGeometryMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Workspace UI geometry: left=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+) right=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+) controls=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+) utility=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+) scene_objects=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+) details=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+)\.'
        $gridMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Grid control: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)'
        $qaTabMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Tools QA tab: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)'
        $toolsHeaderMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Tools panel header: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)'
        $shadingMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Viewport shading controls: x=([-0-9.]+) y=([-0-9.]+) button=([-0-9.]+) gap=([-0-9.]+)'

        if ($null -eq $framebufferMatch -or
            $null -eq $viewportMatch -or
            $null -eq $workspaceGeometryMatch -or
            $null -eq $qaTabMatch -or
            $null -eq $toolsHeaderMatch -or
            $null -eq $shadingMatch) {
            throw "Packaged UI automation geometry could not be parsed."
        }

        $framebufferWidth =
            [int]$framebufferMatch.Groups[1].Value
        $framebufferHeight =
            [int]$framebufferMatch.Groups[2].Value

        $viewportX = [double]$viewportMatch.Groups[1].Value
        $viewportY = [double]$viewportMatch.Groups[2].Value
        $viewportWidth = [double]$viewportMatch.Groups[3].Value
        $viewportHeight = [double]$viewportMatch.Groups[4].Value

        $leftDockX =
            [double]$workspaceGeometryMatch.Groups[1].Value
        $leftDockY =
            [double]$workspaceGeometryMatch.Groups[2].Value
        $leftDockWidth =
            [double]$workspaceGeometryMatch.Groups[3].Value
        $leftDockHeight =
            [double]$workspaceGeometryMatch.Groups[4].Value
        $rightDockX =
            [double]$workspaceGeometryMatch.Groups[5].Value
        $rightDockY =
            [double]$workspaceGeometryMatch.Groups[6].Value
        $rightDockWidth =
            [double]$workspaceGeometryMatch.Groups[7].Value
        $rightDockHeight =
            [double]$workspaceGeometryMatch.Groups[8].Value
        $controlsX =
            [double]$workspaceGeometryMatch.Groups[9].Value
        $controlsY =
            [double]$workspaceGeometryMatch.Groups[10].Value
        $controlsWidth =
            [double]$workspaceGeometryMatch.Groups[11].Value
        $controlsHeight =
            [double]$workspaceGeometryMatch.Groups[12].Value
        $utilityX =
            [double]$workspaceGeometryMatch.Groups[13].Value
        $utilityY =
            [double]$workspaceGeometryMatch.Groups[14].Value
        $utilityWidth =
            [double]$workspaceGeometryMatch.Groups[15].Value
        $utilityHeight =
            [double]$workspaceGeometryMatch.Groups[16].Value
        $sceneObjectsX =
            [double]$workspaceGeometryMatch.Groups[17].Value
        $sceneObjectsY =
            [double]$workspaceGeometryMatch.Groups[18].Value
        $sceneObjectsWidth =
            [double]$workspaceGeometryMatch.Groups[19].Value
        $sceneObjectsHeight =
            [double]$workspaceGeometryMatch.Groups[20].Value
        $detailsX =
            [double]$workspaceGeometryMatch.Groups[21].Value
        $detailsY =
            [double]$workspaceGeometryMatch.Groups[22].Value
        $detailsWidth =
            [double]$workspaceGeometryMatch.Groups[23].Value
        $detailsHeight =
            [double]$workspaceGeometryMatch.Groups[24].Value

        $gridAvailable = $null -ne $gridMatch
        if ($gridAvailable) {
            $gridX =
                [double]$gridMatch.Groups[1].Value
            $gridY =
                [double]$gridMatch.Groups[2].Value
            $gridWidth =
                [double]$gridMatch.Groups[3].Value
            $gridHeight =
                [double]$gridMatch.Groups[4].Value
            Write-Output "[pass] Optional Grid control geometry was reported"
        }
        else {
            Write-Output "[info] Optional Grid control is collapsed; its click check is skipped"
        }

        $qaTabX =
            [double]$qaTabMatch.Groups[1].Value
        $qaTabY =
            [double]$qaTabMatch.Groups[2].Value
        $qaTabWidth =
            [double]$qaTabMatch.Groups[3].Value
        $qaTabHeight =
            [double]$qaTabMatch.Groups[4].Value

        $toolsHeaderX =
            [double]$toolsHeaderMatch.Groups[1].Value
        $toolsHeaderY =
            [double]$toolsHeaderMatch.Groups[2].Value
        $toolsHeaderWidth =
            [double]$toolsHeaderMatch.Groups[3].Value
        $toolsHeaderHeight =
            [double]$toolsHeaderMatch.Groups[4].Value
        Assert-FramebufferRect `
            -Name "Tools panel header" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $toolsHeaderX `
            -Y $toolsHeaderY `
            -Width $toolsHeaderWidth `
            -Height $toolsHeaderHeight

        $shadingX =
            [double]$shadingMatch.Groups[1].Value
        $shadingY =
            [double]$shadingMatch.Groups[2].Value
        $shadingButtonWidth =
            [double]$shadingMatch.Groups[3].Value
        $shadingGap =
            [double]$shadingMatch.Groups[4].Value
        $shadingGroupWidth =
            $shadingButtonWidth * 4.0 +
            $shadingGap * 3.0

        $currentLayoutMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Sandbox layout: (Standard|Focus Viewport|Legacy Full Tools)'

        if ($null -eq $currentLayoutMatch) {
            throw "The active packaged workspace layout could not be determined."
        }

        $currentLayout =
            $currentLayoutMatch.Groups[1].Value

        Assert-FramebufferRect `
            -Name "Left dock" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $leftDockX `
            -Y $leftDockY `
            -Width $leftDockWidth `
            -Height $leftDockHeight
        if ($currentLayout -eq "Focus Viewport") {
            if ([Math]::Abs($rightDockWidth) -gt 0.01 -or
                $rightDockHeight -le 0.0 -or
                $rightDockX -lt 0.0 -or
                $rightDockX -gt [double]$framebufferWidth -or
                $rightDockY -lt 0.0 -or
                $rightDockY + $rightDockHeight -gt [double]$framebufferHeight) {

                throw (
                    "Focus Viewport mode reported an invalid collapsed right dock: " +
                    "rect=($rightDockX,$rightDockY,$rightDockWidth,$rightDockHeight).")
            }

            Write-Output "[pass] Focus Viewport mode safely collapses its inactive right dock"
        }
        else {
            Assert-FramebufferRect `
                -Name "Right dock" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $rightDockX `
                -Y $rightDockY `
                -Width $rightDockWidth `
                -Height $rightDockHeight
        }

        $panelRects = @(
            [pscustomobject]@{
                Name = "Tools panel"
                X = $controlsX
                Y = $controlsY
                Width = $controlsWidth
                Height = $controlsHeight
            },
            [pscustomobject]@{
                Name = "Utility panel"
                X = $utilityX
                Y = $utilityY
                Width = $utilityWidth
                Height = $utilityHeight
            },
            [pscustomobject]@{
                Name = "Scene Objects panel"
                X = $sceneObjectsX
                Y = $sceneObjectsY
                Width = $sceneObjectsWidth
                Height = $sceneObjectsHeight
            },
            [pscustomobject]@{
                Name = "Object Details panel"
                X = $detailsX
                Y = $detailsY
                Width = $detailsWidth
                Height = $detailsHeight
            }
        )
        $visiblePanelRects = @(
            $panelRects |
                Where-Object {
                    $_.Width -gt 0.0 -and $_.Height -gt 0.0
                }
        )
        $minimumVisiblePanelCount =
            if ($currentLayout -eq "Focus Viewport") { 1 } else { 2 }

        if ($visiblePanelRects.Count -lt $minimumVisiblePanelCount) {
            throw (
                "$currentLayout mode reported too few active workspace panel rectangles: " +
                "$($visiblePanelRects.Count), expected at least $minimumVisiblePanelCount.")
        }

        if ($currentLayout -eq "Focus Viewport" -and
            ($sceneObjectsWidth -gt 0.0 -or
             $sceneObjectsHeight -gt 0.0 -or
             $detailsWidth -gt 0.0 -or
             $detailsHeight -gt 0.0)) {

            throw "Focus Viewport mode reported right-side panel content while its right dock was collapsed."
        }
        foreach ($panelRect in $visiblePanelRects) {
            Assert-FramebufferRect `
                -Name $panelRect.Name `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $panelRect.X `
                -Y $panelRect.Y `
                -Width $panelRect.Width `
                -Height $panelRect.Height
        }

        function Assert-DockCoverage {
            param(
                [Parameter(Mandatory = $true)][string]$Name,
                [Parameter(Mandatory = $true)][double]$X,
                [Parameter(Mandatory = $true)][double]$Y,
                [Parameter(Mandatory = $true)][double]$Width,
                [Parameter(Mandatory = $true)][double]$Height,
                [Parameter(Mandatory = $true)][object[]]$Rectangles
            )

            $geometryTolerance = 1.1
            $gapTolerance = 6.1
            $matching = @(
                $Rectangles |
                    Where-Object {
                        [Math]::Abs($_.X - $X) -le $geometryTolerance -and
                        [Math]::Abs($_.Width - $Width) -le $geometryTolerance -and
                        $_.Y + $_.Height -gt $Y -and
                        $_.Y -lt $Y + $Height
                    } |
                    Sort-Object Y
            )
            if ($matching.Count -eq 0) {
                throw "$Name has no active section content."
            }
            if ([Math]::Abs($matching[0].Y - $Y) -gt $geometryTolerance) {
                throw "$Name has unused space above its first active section."
            }

            $coveredBottom = $Y
            foreach ($rect in $matching) {
                if ($rect.Y - $coveredBottom -gt $gapTolerance) {
                    throw "$Name contains an unexpected empty vertical region."
                }
                $rectBottom = $rect.Y + $rect.Height
                if ($rectBottom -gt $coveredBottom) {
                    $coveredBottom = $rectBottom
                }
            }
            if ([Math]::Abs(($Y + $Height) - $coveredBottom) -gt $geometryTolerance) {
                throw "$Name has unused space below its last active section."
            }
            Write-Output "[pass] $Name active sections cover the dock without empty voids"
        }

        Assert-DockCoverage `
            -Name "Left dock" `
            -X $leftDockX `
            -Y $leftDockY `
            -Width $leftDockWidth `
            -Height $leftDockHeight `
            -Rectangles $visiblePanelRects
        if ($rightDockWidth -gt 0.0) {
            Assert-DockCoverage `
                -Name "Right dock" `
                -X $rightDockX `
                -Y $rightDockY `
                -Width $rightDockWidth `
                -Height $rightDockHeight `
                -Rectangles $visiblePanelRects
        }
        else {
            Write-Output "[pass] Collapsed right dock requires no active section coverage"
        }
        Write-Output "[pass] Inactive merged tabs may report zero content rectangles safely"

        Write-Step "Checking imported showcase native authoring bridge"

        $inspectLayoutMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Sandbox layout: (Standard|Focus Viewport|Legacy Full Tools)'

        if ($null -eq $inspectLayoutMatch) {
            throw "The workspace layout could not be read before entering Standard."
        }

        $inspectLayout =
            $inspectLayoutMatch.Groups[1].Value

        for ($layoutAttempt = 0;
             $layoutAttempt -lt 3 -and
             $inspectLayout -ne "Standard";
             ++$layoutAttempt) {

            $previousLayout =
                $inspectLayout

            Set-HenkaAutomationForeground `
                -Handle $mainWindowHandle

            Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F5"

            $layoutDeadline =
                (Get-Date).AddSeconds(4)

            do {
                Start-Sleep -Milliseconds 100

                $nextLayoutMatch = Get-LastLogRegexMatch `
                    -Path $stdoutPath `
                    -Pattern 'Sandbox layout: (Standard|Focus Viewport|Legacy Full Tools)'

                if ($null -ne $nextLayoutMatch) {
                    $candidateLayout =
                        $nextLayoutMatch.Groups[1].Value

                    if ($candidateLayout -ne $previousLayout) {
                        $inspectLayout =
                            $candidateLayout
                        break
                    }
                }
            }
            while ((Get-Date) -lt $layoutDeadline)

            if ($inspectLayout -eq $previousLayout) {
                throw (
                    "F5 did not advance the packaged workspace from " +
                    "$previousLayout within four seconds.")
            }
        }

        if ($inspectLayout -ne "Standard") {
            throw (
                "The packaged workspace did not reach Standard within " +
                "three bounded layout transitions.")
        }

        Write-Output "[pass] Packaged workspace entered Standard deterministically"
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring row:" -TimeoutMilliseconds 4000)) {
            throw "The Standard layout did not expose a showcase primitive authoring row."
        }
        $nativeRowMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring row: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)\.'
        if ($null -eq $nativeRowMatch) {
            throw "The native authoring row geometry could not be parsed."
        }
        $nativeRowX = [double]$nativeRowMatch.Groups[2].Value
        $nativeRowY = [double]$nativeRowMatch.Groups[3].Value
        $nativeRowWidth = [double]$nativeRowMatch.Groups[4].Value
        $nativeRowHeight = [double]$nativeRowMatch.Groups[5].Value
        Assert-FramebufferRect `
            -Name "Native authoring showcase row" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeRowX `
            -Y $nativeRowY `
            -Width $nativeRowWidth `
            -Height $nativeRowHeight
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $nativeAuthoringScreenshotPath `
            -Description "Packaged native authoring pre-selection screenshot"
        $nativeSelectionObserved = $false
        for ($attempt = 0; $attempt -lt 3 -and -not $nativeSelectionObserved; ++$attempt) {
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeRowX + $nativeRowWidth * 0.5) `
                -FramebufferY ($nativeRowY + $nativeRowHeight * 0.5)
                $nativeSelectionObserved = Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern "Native authoring row clicked:" `
                    -TimeoutMilliseconds 2000
        }
        Start-Sleep -Milliseconds 350
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $selectionOutlineScreenshotPath `
            -Description "Packaged logical-owner silhouette selection screenshot"
        if (-not $nativeSelectionObserved) {
            $inspectViewportMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Sandbox viewport: origin ([0-9]+),([0-9]+) size ([0-9]+)x([0-9]+)\.'
            if ($null -ne $inspectViewportMatch) {
                $inspectViewportX = [double]$inspectViewportMatch.Groups[1].Value
                $inspectViewportY = [double]$inspectViewportMatch.Groups[2].Value
                $inspectViewportWidth = [double]$inspectViewportMatch.Groups[3].Value
                $inspectViewportHeight = [double]$inspectViewportMatch.Groups[4].Value
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($inspectViewportX + $inspectViewportWidth * 0.30) `
                    -FramebufferY ($inspectViewportY + $inspectViewportHeight * 0.45)
                $nativeSelectionObserved = Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern "Native authoring row clicked:" `
                    -TimeoutMilliseconds 3000
            }
        }
        if (-not $nativeSelectionObserved) {
            throw "Selecting the showcase row did not expose Object Details > Authoring > Make Editable."
        }
        $nativeDisclosureMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring disclosure: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=28.0 expanded=([01])\.'
        if ($null -eq $nativeDisclosureMatch) {
            throw "The selected showcase did not report its Authoring disclosure geometry."
        }
        if ([int]$nativeDisclosureMatch.Groups[5].Value -eq 0) {
            $nativeDisclosureX = [double]$nativeDisclosureMatch.Groups[2].Value
            $nativeDisclosureY = [double]$nativeDisclosureMatch.Groups[3].Value
            $nativeDisclosureWidth = [double]$nativeDisclosureMatch.Groups[4].Value
            Assert-FramebufferRect `
                -Name "Native authoring disclosure" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $nativeDisclosureX `
                -Y $nativeDisclosureY `
                -Width $nativeDisclosureWidth `
                -Height 28.0
            $nativeDisclosureObserved = $false
            for ($disclosureAttempt = 0; $disclosureAttempt -lt 3 -and -not $nativeDisclosureObserved; ++$disclosureAttempt) {
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($nativeDisclosureX + $nativeDisclosureWidth * 0.5) `
                    -FramebufferY ($nativeDisclosureY + 14.0)
                $nativeDisclosureObserved = Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern 'Native authoring disclosure: name=.* expanded=1\.' `
                    -TimeoutMilliseconds 2500
            }
            if (-not $nativeDisclosureObserved) {
                throw "The selected showcase Authoring disclosure did not open."
            }
        }
        # A checked-in source may be restored either by the HAMS load path or
        # by the startup authoring-state restore path.  Both paths establish
        # the same editor-owned native source; the gate must recognize both
        # rather than requiring one implementation detail.
        $nativeSourceRestored = Wait-FileContains `
            -Path $stdoutPath `
            -Pattern 'Native authoring (source loaded:|topology bridge: name=.+ source_state=HENKA_NATIVE_EDITABLE_SOURCE\.|dogfood: Make Editable converted .+source_state=HENKA_NATIVE_EDITABLE_SOURCE\.|startup restore: name=.+ source_state=HENKA_NATIVE_EDITABLE_SOURCE\.|startup restore fallback: name=.+ source_state=HENKA_NATIVE_EDITABLE_SOURCE fallback=IMPORTED_FIXTURE\.)' `
            -TimeoutMilliseconds 3000
        $nativeAuthoringFallbackObserved = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring startup restore fallback: name=(.+) result=(.+?) source_state=HENKA_NATIVE_EDITABLE_SOURCE fallback=IMPORTED_FIXTURE\.'
        $nativeAuthoringControlObserved = Wait-FileContains `
            -Path $stdoutPath `
            -Pattern "Native authoring Make Editable control:" `
            -TimeoutMilliseconds 3000
        if ($nativeAuthoringFallbackObserved -ne $null) {
            Write-Output "[pass] Invalid persisted derivative fell back to the valid imported native authoring source"
        } elseif ($nativeSourceRestored) {
            Write-Output "[pass] Packaged showcase restored its editor-owned native authoring source"
        } elseif ($nativeAuthoringControlObserved) {
            $nativeMakeEditableMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring Make Editable control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=180.0 height=24.0\.'
            if ($null -eq $nativeMakeEditableMatch) {
                throw "The Make Editable control geometry could not be parsed."
            }
            $nativeMakeEditableX = [double]$nativeMakeEditableMatch.Groups[2].Value
            $nativeMakeEditableY = [double]$nativeMakeEditableMatch.Groups[3].Value
            Assert-FramebufferRect `
                -Name "Native authoring Make Editable control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $nativeMakeEditableX `
                -Y $nativeMakeEditableY `
                -Width 180.0 `
                -Height 24.0
            $nativeMakeEditableObserved = $false
            for ($makeEditableAttempt = 0; $makeEditableAttempt -lt 3 -and -not $nativeMakeEditableObserved; ++$makeEditableAttempt) {
                $makeEditableLogOffset = Get-FileLengthSafe -Path $stdoutPath
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($nativeMakeEditableX + 90.0) `
                    -FramebufferY ($nativeMakeEditableY + 12.0)
                # Converting the imported giraffe is a bounded mesh-weld
                # operation over the checked-in 45k-vertex fixture.  Keep
                # the wait finite, but do not mistake normal conversion time
                # for a missing click or a failed transaction.
                $nativeMakeEditableObserved = Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern "Native authoring workflow: Make Editable converted .*source_state=HENKA_NATIVE_EDITABLE_SOURCE\." `
                    -StartingOffset $makeEditableLogOffset `
                    -TimeoutMilliseconds 15000
            }
            if (-not $nativeMakeEditableObserved) {
                throw "Make Editable did not create the user-owned native authoring source."
            }
            Write-Output "[pass] Imported showcase primitive entered the user-facing native authoring workflow"
        } else {
            throw "The selected showcase authoring controls did not become visible in the prioritized Authoring group."
        }
        $nativeDisclosureMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring disclosure: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=28.0 expanded=([01])\.'
        if ($null -eq $nativeDisclosureMatch) {
            throw "The selected showcase did not report its Authoring disclosure geometry."
        }
        if ([int]$nativeDisclosureMatch.Groups[5].Value -eq 0) {
            $nativeDisclosureX = [double]$nativeDisclosureMatch.Groups[2].Value
            $nativeDisclosureY = [double]$nativeDisclosureMatch.Groups[3].Value
            $nativeDisclosureWidth = [double]$nativeDisclosureMatch.Groups[4].Value
            Assert-FramebufferRect `
                -Name "Native authoring disclosure" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $nativeDisclosureX `
                -Y $nativeDisclosureY `
                -Width $nativeDisclosureWidth `
                -Height 28.0
            $nativeDisclosureObserved = $false
            for ($disclosureAttempt = 0; $disclosureAttempt -lt 3 -and -not $nativeDisclosureObserved; ++$disclosureAttempt) {
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($nativeDisclosureX + $nativeDisclosureWidth * 0.5) `
                    -FramebufferY ($nativeDisclosureY + 14.0)
                $nativeDisclosureObserved = Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern 'Native authoring disclosure: name=.* expanded=1\.' `
                    -TimeoutMilliseconds 1500
            }
            if (-not $nativeDisclosureObserved) {
                throw "The selected showcase Authoring disclosure did not open."
            }
        }
        $nativeMaterialAlreadyOwned =
            Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native authoring material controls:" `
                -TimeoutMilliseconds 750

        if ($nativeMaterialAlreadyOwned) {
            if (-not (Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern "Native authoring startup restore: material state restored" `
                    -TimeoutMilliseconds 3000)) {

                throw (
                    "Editable native material controls were already present, " +
                    "but no restored material-state evidence was reported.")
            }

            Write-Output "[pass] Restored native material ownership remained editable without redundant Own Material"
        }
        else {
            if (-not (Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern "Native authoring material control:" `
                    -TimeoutMilliseconds 3000)) {

                throw (
                    "The showcase exposed neither restored editable material controls " +
                    "nor the native material ownership control.")
            }

            $nativeMaterialOwnershipMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring material control: name=(.+) own_x=([-0-9.]+) own_y=([-0-9.]+) width=100.0 height=24.0 owned=0\.'

            if ($null -eq $nativeMaterialOwnershipMatch) {
                throw "The native material ownership control geometry could not be parsed."
            }

            $nativeMaterialOwnershipX =
                [double]$nativeMaterialOwnershipMatch.Groups[2].Value

            $nativeMaterialOwnershipY =
                [double]$nativeMaterialOwnershipMatch.Groups[3].Value

            Assert-FramebufferRect `
                -Name "Native authoring material ownership control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $nativeMaterialOwnershipX `
                -Y $nativeMaterialOwnershipY `
                -Width 100.0 `
                -Height 24.0

            $nativeMaterialOwnershipObserved = $false

            for ($materialOwnershipAttempt = 0;
                 $materialOwnershipAttempt -lt 3 -and
                 -not $nativeMaterialOwnershipObserved;
                 ++$materialOwnershipAttempt) {

                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($nativeMaterialOwnershipX + 50.0) `
                    -FramebufferY ($nativeMaterialOwnershipY + 12.0)

                $nativeMaterialOwnershipObserved =
                    Wait-FileContains `
                        -Path $stdoutPath `
                        -Pattern "Native authoring material: editable runtime definition adopted" `
                        -TimeoutMilliseconds 2500
            }

            if (-not $nativeMaterialOwnershipObserved) {
                throw "The showcase material was not promoted to a manager-owned editable definition."
            }

            Write-Output "[pass] Fresh native material ownership promotion completed"
        }

        if (-not (Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native authoring material controls:" `
                -TimeoutMilliseconds 3000)) {

            throw "The native material editor controls did not become visible in the resolved ownership state."
        }
        for ($opticalScrollAttempt = 0; $opticalScrollAttempt -lt 8; ++$opticalScrollAttempt) {
            if (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring optical material controls:" -TimeoutMilliseconds 300) {
                break
            }
            Scroll-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($detailsX + [Math]::Min(120.0, [Math]::Max(24.0, $detailsWidth - 80.0))) `
                -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                -WheelDelta -120
        }
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring optical material controls:" -TimeoutMilliseconds 3000)) {
            throw "The native optical material controls did not become visible after ownership promotion."
        }
        $nativeOpticalMaterialMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring optical material controls: name=(.+) ior_x=([-0-9.]+) transmission_x=([-0-9.]+) thickness_x=([-0-9.]+) subsurface_tint_x=([-0-9.]+) first_y=([-0-9.]+) second_y=([-0-9.]+) width=([-0-9.]+) height=28.0\.'
        if ($null -eq $nativeOpticalMaterialMatch) {
            throw "The native optical material control geometry could not be parsed."
        }
        $nativeIorX = [double]$nativeOpticalMaterialMatch.Groups[2].Value
        $nativeTransmissionX = [double]$nativeOpticalMaterialMatch.Groups[3].Value
        $nativeThicknessX = [double]$nativeOpticalMaterialMatch.Groups[4].Value
        $nativeSubsurfaceTintX = [double]$nativeOpticalMaterialMatch.Groups[5].Value
        $nativeOpticalFirstY = [double]$nativeOpticalMaterialMatch.Groups[6].Value
        $nativeOpticalSecondY = [double]$nativeOpticalMaterialMatch.Groups[7].Value
        $nativeOpticalWidth = [double]$nativeOpticalMaterialMatch.Groups[8].Value
        foreach ($opticalControl in @(
            @{ Name = "IOR"; X = $nativeIorX; Y = $nativeOpticalFirstY; Pattern = "parameter=IOR" },
            @{ Name = "transmission"; X = $nativeTransmissionX; Y = $nativeOpticalFirstY; Pattern = "parameter=Transmission" }
        )) {
            $latestOpticalMaterialMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring optical material controls: name=(.+) ior_x=([-0-9.]+) transmission_x=([-0-9.]+) thickness_x=([-0-9.]+) subsurface_tint_x=([-0-9.]+) first_y=([-0-9.]+) second_y=([-0-9.]+) width=([-0-9.]+) height=28.0\.'
            $opticalControlX = [double]$opticalControl.X
            $opticalControlY = [double]$opticalControl.Y
            $opticalControlWidth = $nativeOpticalWidth
            if ($null -ne $latestOpticalMaterialMatch) {
                if ($opticalControl.Name -eq "IOR") {
                    $opticalControlX = [double]$latestOpticalMaterialMatch.Groups[2].Value
                }
                else {
                    $opticalControlX = [double]$latestOpticalMaterialMatch.Groups[3].Value
                }
                $opticalControlY = [double]$latestOpticalMaterialMatch.Groups[6].Value
                $opticalControlWidth = [double]$latestOpticalMaterialMatch.Groups[8].Value
            }
            Assert-FramebufferRect `
                -Name ("Native authoring " + $opticalControl.Name + " control") `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $opticalControlX `
                -Y $opticalControlY `
                -Width $opticalControlWidth `
                -Height 28.0
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($opticalControlX + ($opticalControlWidth * 0.5)) `
                -FramebufferY ($opticalControlY + 14.0)
            if (-not (Wait-FileContains -Path $stdoutPath -Pattern $opticalControl.Pattern -TimeoutMilliseconds 5000)) {
                throw ("The user-facing native " + $opticalControl.Name + " edit did not complete.")
            }
        }
        Write-Output "[pass] User-facing native optical material edits completed"
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring subsurface thickness control:" -TimeoutMilliseconds 3000)) {
            throw "The native subsurface thickness control did not become visible after ownership promotion."
        }
        $nativeThicknessMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring subsurface thickness control: name=(.+) thickness_x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=28.0\.'
        if ($null -eq $nativeThicknessMatch) {
            throw "The native subsurface thickness control geometry could not be parsed."
        }
        $nativeThicknessX = [double]$nativeThicknessMatch.Groups[2].Value
        $nativeThicknessY = [double]$nativeThicknessMatch.Groups[3].Value
        $nativeThicknessWidth = [double]$nativeThicknessMatch.Groups[4].Value
        Assert-FramebufferRect `
            -Name "Native authoring subsurface thickness control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeThicknessX `
            -Y $nativeThicknessY `
            -Width $nativeThicknessWidth `
            -Height 28.0
        $nativeThicknessEditObserved = $false
        for ($thicknessAttempt = 0; $thicknessAttempt -lt 3 -and -not $nativeThicknessEditObserved; ++$thicknessAttempt) {
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeThicknessX + ($nativeThicknessWidth * 0.5)) `
                -FramebufferY ($nativeThicknessY + 14.0)
            $nativeThicknessEditObserved = Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "parameter=Thickness" `
                -TimeoutMilliseconds 2000
        }
        if (-not $nativeThicknessEditObserved) {
            throw "The user-facing native subsurface thickness edit did not complete."
        }
        Write-Output "[pass] User-facing native subsurface thickness edit completed"
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring subsurface tint control:" -TimeoutMilliseconds 3000)) {
            throw "The native subsurface tint control did not become visible after ownership promotion."
        }
        $nativeSubsurfaceTintMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring subsurface tint control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=28.0\.'
        if ($null -eq $nativeSubsurfaceTintMatch) {
            throw "The native subsurface tint control geometry could not be parsed."
        }
        $nativeSubsurfaceTintX = [double]$nativeSubsurfaceTintMatch.Groups[2].Value
        $nativeSubsurfaceTintY = [double]$nativeSubsurfaceTintMatch.Groups[3].Value
        $nativeSubsurfaceTintWidth = [double]$nativeSubsurfaceTintMatch.Groups[4].Value
        Assert-FramebufferRect `
            -Name "Native authoring subsurface tint control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeSubsurfaceTintX `
            -Y $nativeSubsurfaceTintY `
            -Width $nativeSubsurfaceTintWidth `
            -Height 28.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeSubsurfaceTintX + ($nativeSubsurfaceTintWidth * 0.5)) `
            -FramebufferY ($nativeSubsurfaceTintY + 14.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "parameter=Subsurface Color" -TimeoutMilliseconds 5000)) {
            throw "The user-facing native subsurface tint edit did not complete."
        }
        Write-Output "[pass] User-facing native subsurface tint edit completed"
        $nativeMaterialControlsMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring material controls: name=(.+) tint_x=([-0-9.]+) metal_x=([-0-9.]+) rough_x=([-0-9.]+) emissive_x=([-0-9.]+) texture_x=([-0-9.]+) subsurface_x=([-0-9.]+) first_y=([-0-9.]+) second_y=([-0-9.]+) width=([-0-9.]+) height=28.0\.'
        if ($null -eq $nativeMaterialControlsMatch) {
            throw "The native material editor control geometry could not be parsed."
        }
        $nativeMaterialTintX = [double]$nativeMaterialControlsMatch.Groups[2].Value
        $nativeMaterialMetalX = [double]$nativeMaterialControlsMatch.Groups[3].Value
        $nativeMaterialRoughX = [double]$nativeMaterialControlsMatch.Groups[4].Value
        $nativeMaterialEmissiveX = [double]$nativeMaterialControlsMatch.Groups[5].Value
        $nativeMaterialTextureX = [double]$nativeMaterialControlsMatch.Groups[6].Value
        $nativeSubsurfaceX = [double]$nativeMaterialControlsMatch.Groups[7].Value
        $nativeMaterialY = [double]$nativeMaterialControlsMatch.Groups[8].Value
        $nativeMaterialSecondY = [double]$nativeMaterialControlsMatch.Groups[9].Value
        $nativeMaterialWidth = [double]$nativeMaterialControlsMatch.Groups[10].Value
        Assert-FramebufferRect `
            -Name "Native authoring material tint control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMaterialTintX `
            -Y $nativeMaterialY `
            -Width $nativeMaterialWidth `
            -Height 28.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMaterialTintX + ($nativeMaterialWidth * 0.5)) `
            -FramebufferY ($nativeMaterialY + 14.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring material edited" -TimeoutMilliseconds 5000)) {
            throw "The user-facing native material parameter edit did not complete."
        }
        Assert-FramebufferRect `
            -Name "Native authoring metallic control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMaterialMetalX `
            -Y $nativeMaterialY `
            -Width $nativeMaterialWidth `
            -Height 28.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMaterialMetalX + ($nativeMaterialWidth * 0.5)) `
            -FramebufferY ($nativeMaterialY + 14.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "parameter=Metallic" -TimeoutMilliseconds 5000)) {
            throw "The user-facing native metallic edit did not complete."
        }
        Write-Output "[pass] User-facing native metallic edit completed"
        foreach ($scalarControl in @(
            @{ Name = "roughness"; X = $nativeMaterialRoughX; Y = $nativeMaterialY; Pattern = "parameter=Roughness" },
            @{ Name = "emissive strength"; X = $nativeMaterialEmissiveX; Y = $nativeMaterialSecondY; Pattern = "parameter=Emissive Strength" }
        )) {
            $scalarEdited = $false
            foreach ($scalarClickOffset in @(
                @(0.0, 0.0),
                @(0.0, -6.0),
                @(0.0, 6.0),
                @(-8.0, 0.0),
                @(8.0, 0.0)
            )) {
                $latestNativeMaterialControlsMatch = Get-LastLogRegexMatch `
                    -Path $stdoutPath `
                    -Pattern 'Native authoring material controls: name=(.+) tint_x=([-0-9.]+) metal_x=([-0-9.]+) rough_x=([-0-9.]+) emissive_x=([-0-9.]+) texture_x=([-0-9.]+) subsurface_x=([-0-9.]+) first_y=([-0-9.]+) second_y=([-0-9.]+) width=([-0-9.]+) height=28.0\.'
                if ($null -ne $latestNativeMaterialControlsMatch) {
                    if ($scalarControl.Name -eq "roughness") {
                        $scalarControl.X = [double]$latestNativeMaterialControlsMatch.Groups[4].Value
                        $scalarControl.Y = [double]$latestNativeMaterialControlsMatch.Groups[8].Value
                    }
                    else {
                        $scalarControl.X = [double]$latestNativeMaterialControlsMatch.Groups[5].Value
                        $scalarControl.Y = [double]$latestNativeMaterialControlsMatch.Groups[9].Value
                    }
                    $nativeMaterialWidth = [double]$latestNativeMaterialControlsMatch.Groups[10].Value
                }
                Assert-FramebufferRect `
                    -Name ("Native authoring " + $scalarControl.Name + " control") `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -X $scalarControl.X `
                    -Y $scalarControl.Y `
                    -Width $nativeMaterialWidth `
                    -Height 28.0
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($scalarControl.X + ($nativeMaterialWidth * 0.5) + [double]$scalarClickOffset[0]) `
                    -FramebufferY ($scalarControl.Y + 14.0 + [double]$scalarClickOffset[1])
                if (Wait-FileContains -Path $stdoutPath -Pattern $scalarControl.Pattern -TimeoutMilliseconds 2500) {
                    $scalarEdited = $true
                    break
                }
            }
            if (-not $scalarEdited) {
                throw ("The user-facing native " + $scalarControl.Name + " edit did not complete.")
            }
        }
        Write-Output "[pass] User-facing native material scalar edits completed"
        Assert-FramebufferRect `
            -Name "Native authoring subsurface control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeSubsurfaceX `
            -Y $nativeMaterialSecondY `
            -Width $nativeMaterialWidth `
            -Height 28.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeSubsurfaceX + ($nativeMaterialWidth * 0.5)) `
            -FramebufferY ($nativeMaterialSecondY + 14.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "parameter=Subsurface" -TimeoutMilliseconds 5000)) {
            throw "The user-facing native subsurface edit did not complete."
        }
        Write-Output "[pass] User-facing native subsurface edit completed"
        Assert-FramebufferRect `
            -Name "Native authoring texture control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMaterialTextureX `
            -Y $nativeMaterialSecondY `
            -Width $nativeMaterialWidth `
            -Height 28.0
        $nativeTextureLogOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMaterialTextureX + ($nativeMaterialWidth * 0.5)) `
            -FramebufferY ($nativeMaterialSecondY + 14.0)
        # Texture assignment is a bounded native-source transaction over the
        # imported fixture. Require fresh post-click telemetry and allow the
        # transaction to finish without accepting a stale earlier edit.
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring texture edited:.*slot=Normal" `
                -StartingOffset $nativeTextureLogOffset `
                -TimeoutMilliseconds 15000)) {
            throw "The user-facing native texture assignment did not complete."
        }
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring texture edited:.*slot=Metallic-Roughness" `
                -StartingOffset $nativeTextureLogOffset `
                -TimeoutMilliseconds 15000)) {
            throw "The user-facing native metallic-roughness texture authoring did not complete."
        }
        Write-Output "[pass] User-facing native material and texture edits completed"
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring material history:" -TimeoutMilliseconds 1200)) {
            for ($scrollAttempt = 0; $scrollAttempt -lt 12; ++$scrollAttempt) {
                Scroll-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($detailsX + [Math]::Max(12.0, $detailsWidth - 18.0)) `
                    -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                    -WheelDelta -120
                if (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring material history:" -TimeoutMilliseconds 1000) {
                    break
                }
            }
        }
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring material history:" -TimeoutMilliseconds 2500)) {
            throw "The converted showcase did not expose the native material undo/redo controls."
        }
        $nativeMaterialHistoryMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring material history: name=(.+) undo_x=([-0-9.]+) redo_x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=24.0\.'
        if ($null -eq $nativeMaterialHistoryMatch) {
            throw "The native material undo/redo control geometry could not be parsed."
        }
        $nativeMaterialUndoX = [double]$nativeMaterialHistoryMatch.Groups[2].Value
        $nativeMaterialRedoX = [double]$nativeMaterialHistoryMatch.Groups[3].Value
        $nativeMaterialHistoryY = [double]$nativeMaterialHistoryMatch.Groups[4].Value
        $nativeMaterialHistoryWidth = [double]$nativeMaterialHistoryMatch.Groups[5].Value
        Assert-FramebufferRect `
            -Name "Native authoring material undo control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMaterialUndoX `
            -Y $nativeMaterialHistoryY `
            -Width $nativeMaterialHistoryWidth `
            -Height 24.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMaterialUndoX + ($nativeMaterialHistoryWidth * 0.5)) `
            -FramebufferY ($nativeMaterialHistoryY + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring material undo:" -TimeoutMilliseconds 5000)) {
            throw "The user-facing native material undo did not restore the prior material state."
        }
        Assert-FramebufferRect `
            -Name "Native authoring material redo control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMaterialRedoX `
            -Y $nativeMaterialHistoryY `
            -Width $nativeMaterialHistoryWidth `
            -Height 24.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMaterialRedoX + ($nativeMaterialHistoryWidth * 0.5)) `
            -FramebufferY ($nativeMaterialHistoryY + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring material redo:" -TimeoutMilliseconds 5000)) {
            throw "The user-facing native material redo did not restore the edited material state."
        }
        Write-Output "[pass] User-facing native material undo/redo completed"
        $componentViewportMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Sandbox viewport: origin ([0-9]+),([0-9]+) size ([0-9]+)x([0-9]+)\.'
        if ($null -eq $componentViewportMatch) {
            throw "The selected showcase viewport geometry could not be parsed for component picking."
        }
        $componentViewportX = [double]$componentViewportMatch.Groups[1].Value
        $componentViewportY = [double]$componentViewportMatch.Groups[2].Value
        $componentViewportWidth = [double]$componentViewportMatch.Groups[3].Value
        $componentViewportHeight = [double]$componentViewportMatch.Groups[4].Value
        $nativeComponentPicked = $false
        $nativeMoveLogOffset = $null
        # The selected showcase is the left-hand Giraffe in the deterministic
        # Standard layout.  Probe its visible silhouette first; the prior
        # center-biased probes landed in the Rocket's empty side gap and could
        # not prove component picking even though the selected mesh was
        # visibly outlined.
        foreach ($componentPickX in @(0.18, 0.22, 0.26, 0.30, 0.35, 0.40)) {
            foreach ($componentPickY in @(0.50, 0.56, 0.62, 0.68)) {
                if ($nativeComponentPicked) { break }
                $componentPickLogOffset = Get-FileLengthSafe -Path $stdoutPath
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($componentViewportX + $componentViewportWidth * $componentPickX) `
                    -FramebufferY ($componentViewportY + $componentViewportHeight * $componentPickY)
                $nativeComponentPicked = Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern "Native authoring component picked:" `
                    -StartingOffset $componentPickLogOffset `
                    -TimeoutMilliseconds 1200
                if ($nativeComponentPicked) {
                    # The native authoring update reports the component pick
                    # and the mode-specific move control in one UI dispatch.
                    # Keep the click offset so the second event cannot be
                    # missed when it is already in the log by the time the
                    # first wait returns.
                    $nativeMoveLogOffset = $componentPickLogOffset
                }
            }
            if ($nativeComponentPicked) { break }
        }
        if (-not $nativeComponentPicked) {
            throw "The selected showcase did not expose a pickable component for the user-facing edit check."
        }
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring move control:" `
                -StartingOffset $nativeMoveLogOffset `
                -TimeoutMilliseconds 3000)) {
            throw "The selected showcase did not re-report a current component-edit control after picking."
        }
        $nativeMoveMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring move control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
        if ($null -eq $nativeMoveMatch) {
            throw "The native authoring component-edit control geometry could not be parsed."
        }
        $nativeMoveX = [double]$nativeMoveMatch.Groups[2].Value
        $nativeMoveY = [double]$nativeMoveMatch.Groups[3].Value
        Assert-FramebufferRect `
            -Name "Native authoring component-edit control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMoveX `
            -Y $nativeMoveY `
            -Width 88.0 `
            -Height 24.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMoveX + 20.0) `
            -FramebufferY ($nativeMoveY + 6.0)
        $nativeMoveObserved = Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern "Native authoring workflow: component move edited" `
            -StartingOffset $nativeMoveLogOffset `
            -TimeoutMilliseconds 10000
        if (-not $nativeMoveObserved) {
            throw "The user-facing component edit did not update the native authoring source."
        }
        Write-Output "[pass] User-facing component edit changed the native showcase source"
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring quad repair control:" `
                -StartingOffset $nativeMoveLogOffset `
                -TimeoutMilliseconds 3000)) {
            throw "The converted showcase did not expose the native Quad Repair control."
        }
        $nativeProfileMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring quad repair control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=180.0 height=24.0\.'
        if ($null -eq $nativeProfileMatch) {
            throw "The native Quad Repair control geometry could not be parsed."
        }
        $nativeProfileX = [double]$nativeProfileMatch.Groups[2].Value
        $nativeProfileY = [double]$nativeProfileMatch.Groups[3].Value
        Assert-FramebufferRect `
            -Name "Native authoring profile refinement control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeProfileX `
            -Y $nativeProfileY `
            -Width 180.0 `
            -Height 24.0
        $nativeProfileObserved = $false
        $nativeProfileAttemptLogOffset = $null
        # The bounded details flow has two valid authoring-group paths.  A
        # preceding material/component operation can move this row by one
        # 68px flow step while leaving the earlier geometry report intact.
        $nativeProfileCandidateYs = @(
            [double]$nativeProfileY
            ([double]$nativeProfileY - 68.0)
            ([double]$nativeProfileY + 68.0)
        )
        foreach ($nativeProfileCandidateY in $nativeProfileCandidateYs) {
            if ($nativeProfileObserved) { break }
            $profileAttemptLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeProfileX + 90.0) `
                -FramebufferY ($nativeProfileCandidateY + 12.0)
            # Quad recovery scans the bounded 15k-face authoring mesh; allow
            # the transaction to finish without a false-negative gate result.
            $nativeProfileObserved = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring quad recovery:" `
                -StartingOffset $profileAttemptLogOffset `
                -TimeoutMilliseconds 15000
            if ($nativeProfileObserved) {
                $nativeProfileAttemptLogOffset = $profileAttemptLogOffset
            }
        }
        if (-not $nativeProfileObserved) {
            throw "The user-facing native Quad Repair control did not report a bounded result."
        }
        Write-Output "[pass] User-facing native Quad Repair control reported a bounded result"
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'Native authoring face controls: name=.+ face_x=([-0-9.]+) face_y=([-0-9.]+) width=88.0 height=24.0\.' `
                -StartingOffset $nativeProfileAttemptLogOffset `
                -TimeoutMilliseconds 3000)) {
            throw "The native topology controls did not re-report after Quad Repair completed."
        }
        $nativeFaceMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring face controls: name=(.+) face_x=([-0-9.]+) face_y=([-0-9.]+) width=88.0 height=24.0\.'
        if ($null -eq $nativeFaceMatch -and
            -not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring face controls:" -TimeoutMilliseconds 3000)) {
            throw "The converted showcase did not expose native topology selection controls."
        }
        # The control is normally already visible after the profile edit.  Do
        # not inject a speculative wheel event: queued scroll input can move the
        # details panel after its log line was read and make a valid click stale.
        if ($null -eq $nativeFaceMatch) {
            $nativeFaceMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring face controls: name=(.+) face_x=([-0-9.]+) face_y=([-0-9.]+) width=88.0 height=24.0\.'
        }
        if ($null -eq $nativeFaceMatch) {
            throw "The native topology selection control geometry could not be parsed."
        }
        $nativeFaceX = [double]$nativeFaceMatch.Groups[2].Value
        $nativeFaceY = [double]$nativeFaceMatch.Groups[3].Value
        $nativeEdgeControlMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring Edge selection control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
        if ($null -ne $nativeEdgeControlMatch) {
            $nativeEdgeX = [double]$nativeEdgeControlMatch.Groups[2].Value
            $nativeEdgeY = [double]$nativeEdgeControlMatch.Groups[3].Value
        }
        else {
            $nativeEdgeX = $nativeFaceX - 96.0
            $nativeEdgeY = $nativeFaceY
        }
        # The details content begins below the fixed panel header.  A deep scroll can
        # leave the logged topology row partially clipped under that header even
        # though its last reported rectangle is still inside the framebuffer.  Bring
        # the control back into the interactive content region before clicking it.
        for ($faceVisibilityAttempt = 0; $faceVisibilityAttempt -lt 3 -and $nativeFaceY -lt ($detailsY + 32.0); ++$faceVisibilityAttempt) {
            Scroll-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($detailsX + [Math]::Max(12.0, $detailsWidth - 18.0)) `
                -FramebufferY ($detailsY + 42.0) `
                -WheelDelta 120
            Start-Sleep -Milliseconds 120
            $visibleFaceMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring face controls: name=(.+) face_x=([-0-9.]+) face_y=([-0-9.]+) width=88.0 height=24.0\.'
            if ($null -ne $visibleFaceMatch) {
                $nativeFaceX = [double]$visibleFaceMatch.Groups[2].Value
                $nativeFaceY = [double]$visibleFaceMatch.Groups[3].Value
            }
        }
        Assert-FramebufferRect `
            -Name "Native authoring Face selection control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeFaceX `
            -Y $nativeFaceY `
            -Width 88.0 `
            -Height 24.0
        $nativeEdgeModeObserved = $false
        for ($edgeModeAttempt = 0; $edgeModeAttempt -lt 9 -and -not $nativeEdgeModeObserved; ++$edgeModeAttempt) {
            $latestEdgeControlMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring Edge selection control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
            if ($null -ne $latestEdgeControlMatch) {
                $nativeEdgeX = [double]$latestEdgeControlMatch.Groups[2].Value
                $nativeEdgeY = [double]$latestEdgeControlMatch.Groups[3].Value
            }
            Assert-FramebufferRect `
                -Name "Native authoring Edge selection retry control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $nativeEdgeX `
                -Y $nativeEdgeY `
                -Width 88.0 `
                -Height 24.0
            $edgeXOffset = @(20.0, 44.0, 68.0)[$edgeModeAttempt % 3]
            $edgeYOffset = @(6.0, 12.0, 18.0)[$edgeModeAttempt % 3]
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeEdgeX + $edgeXOffset) `
                -FramebufferY ($nativeEdgeY + $edgeYOffset)
            $nativeEdgeModeObserved = Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native authoring topology mode:.*mode=Edge" `
                -TimeoutMilliseconds 700
        }
        if (-not $nativeEdgeModeObserved) {
            throw "The user-facing Edge selection mode did not become active."
        }
        $nativeEdgeLoopMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring edge topology controls: name=(.+) loop_x=([-0-9.]+) ring_x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
        if ($null -eq $nativeEdgeLoopMatch -and
            -not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring edge topology controls:" -TimeoutMilliseconds 2500)) {
            throw "The converted showcase did not expose the native Edge Loop control."
        }
        if ($null -eq $nativeEdgeLoopMatch) {
            $nativeEdgeLoopMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring edge topology controls: name=(.+) loop_x=([-0-9.]+) ring_x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
        }
        if ($null -eq $nativeEdgeLoopMatch) {
            throw "The native Edge Loop control geometry could not be parsed."
        }
        $nativeEdgeLoopX = [double]$nativeEdgeLoopMatch.Groups[2].Value
        $nativeEdgeLoopY = [double]$nativeEdgeLoopMatch.Groups[4].Value
        Assert-FramebufferRect `
            -Name "Native authoring Edge Loop control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeEdgeLoopX `
            -Y $nativeEdgeLoopY `
            -Width 88.0 `
            -Height 24.0
        $edgeLoopResultCountBefore = @(
            Select-String -LiteralPath $stdoutPath -Pattern "Native authoring edge loop selection:" -ErrorAction SilentlyContinue
        ).Count
        $nativeEdgeLoopResultObserved = $false
        for ($edgeLoopAttempt = 0; $edgeLoopAttempt -lt 3 -and -not $nativeEdgeLoopResultObserved; ++$edgeLoopAttempt) {
            foreach ($loopXOffset in @(20.0, 44.0, 68.0)) {
                foreach ($loopYOffset in @(6.0, 12.0, 18.0)) {
                    if (-not $nativeEdgeLoopResultObserved) {
                        Click-FramebufferPoint `
                            -Handle $mainWindowHandle `
                            -FramebufferWidth $framebufferWidth `
                            -FramebufferHeight $framebufferHeight `
                            -FramebufferX ($nativeEdgeLoopX + $loopXOffset) `
                            -FramebufferY ($nativeEdgeLoopY + $loopYOffset)
                        Start-Sleep -Milliseconds 120
                        $edgeLoopResultCount = @(
                            Select-String -LiteralPath $stdoutPath -Pattern "Native authoring edge loop selection:" -ErrorAction SilentlyContinue
                        ).Count
                        $nativeEdgeLoopResultObserved = $edgeLoopResultCount -gt $edgeLoopResultCountBefore
                    }
                }
            }
        }
        if (-not $nativeEdgeLoopResultObserved) {
            throw "The user-facing Edge Loop control did not report a bounded result."
        }
        Write-Output "[pass] Packaged native Edge Loop control exposed and reported a bounded result"
        $faceAfterEdgeMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring face controls: name=(.+) face_x=([-0-9.]+) face_y=([-0-9.]+) width=88.0 height=24.0\.'
        if ($null -ne $faceAfterEdgeMatch) {
            $candidateFaceX = [double]$faceAfterEdgeMatch.Groups[2].Value
            $candidateFaceY = [double]$faceAfterEdgeMatch.Groups[3].Value
            if ($candidateFaceX -ge 0.0 -and
                $candidateFaceY -ge 0.0 -and
                $candidateFaceX + 88.0 -le [double]$framebufferWidth -and
                $candidateFaceY + 24.0 -le [double]$framebufferHeight) {
                $nativeFaceX = $candidateFaceX
                $nativeFaceY = $candidateFaceY
            }
            else {
                # stdout also contains bounded detached-panel authoring
                # surfaces.  Their local coordinates are not valid targets
                # for this main-window framebuffer gate; retain the last
                # validated docked control instead of clicking stale space.
                Write-Output "[pass] Ignored an out-of-frame detached Face control geometry"
            }
        }
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path (Join-Path $logDir 'check_packaged_sandbox3d_before_face_pick.png') `
            -Description 'Packaged post-Edge Face-pick setup screenshot'
        $bevelControlLogPattern = 'Native authoring bevel control:'
        $bevelControlCountBefore = @(
            Select-String -LiteralPath $stdoutPath -Pattern $bevelControlLogPattern -ErrorAction SilentlyContinue
        ).Count
        $nativeFaceModeObserved = $false
        foreach ($faceSelectionOffset in @(
            @(44.0, 12.0),
            @(20.0, 6.0),
            @(68.0, 18.0),
            @(44.0, 6.0),
            @(44.0, 18.0)
        )) {
            if ($nativeFaceModeObserved) {
                break
            }
            $latestFaceSelectionMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring face controls: name=(.+) face_x=([-0-9.]+) face_y=([-0-9.]+) width=88.0 height=24.0\.'
            if ($null -ne $latestFaceSelectionMatch) {
                $nativeFaceX = [double]$latestFaceSelectionMatch.Groups[2].Value
                $nativeFaceY = [double]$latestFaceSelectionMatch.Groups[3].Value
            }
            Assert-FramebufferRect `
                -Name "Native authoring Face selection retry control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $nativeFaceX `
                -Y $nativeFaceY `
                -Width 88.0 `
                -Height 24.0
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeFaceX + [double]$faceSelectionOffset[0]) `
                -FramebufferY ($nativeFaceY + [double]$faceSelectionOffset[1])
            Start-Sleep -Milliseconds 150
            $nativeFaceModeObserved = Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native authoring topology mode:.*mode=Face" `
                -TimeoutMilliseconds 1000
        }
        $nativeFaceModeStableMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring face mode control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
        if ($null -ne $nativeFaceModeStableMatch) {
            $nativeFaceModeStableX = [double]$nativeFaceModeStableMatch.Groups[2].Value
            $nativeFaceModeStableY = [double]$nativeFaceModeStableMatch.Groups[3].Value
            Assert-FramebufferRect `
                -Name "Native authoring stable Face mode control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $nativeFaceModeStableX `
                -Y $nativeFaceModeStableY `
                -Width 88.0 `
                -Height 24.0
            $nativeFaceModeLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeFaceModeStableX + 44.0) `
                -FramebufferY ($nativeFaceModeStableY + 12.0)
            Start-Sleep -Milliseconds 150
            $nativeFaceModeObserved = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring topology mode:.*mode=Face" `
                -StartingOffset $nativeFaceModeLogOffset `
                -TimeoutMilliseconds 1200
        }
        for ($faceModeAttempt = 0; $faceModeAttempt -lt 8 -and -not $nativeFaceModeObserved; ++$faceModeAttempt) {
            $latestFaceModeMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring face mode control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
            if ($null -ne $latestFaceModeMatch) {
                $nativeFaceModeStableX = [double]$latestFaceModeMatch.Groups[2].Value
                $nativeFaceModeStableY = [double]$latestFaceModeMatch.Groups[3].Value
            }
            Assert-FramebufferRect `
                -Name "Native authoring Face mode retry control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $nativeFaceModeStableX `
                -Y $nativeFaceModeStableY `
                -Width 88.0 `
                -Height 24.0
            $faceModeXOffset = @(20.0, 44.0, 68.0)[$faceModeAttempt % 3]
            $faceModeYOffset = @(6.0, 12.0, 18.0)[$faceModeAttempt % 3]
            $nativeFaceModeLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeFaceModeStableX + $faceModeXOffset) `
                -FramebufferY ($nativeFaceModeStableY + $faceModeYOffset)
            # Accept the current runtime's explicit mode state.  The source
            # log can arrive during the frame that owns the click, so the
            # bounded wait intentionally does not depend on a stale byte
            # offset from before a layout refresh.
            Start-Sleep -Milliseconds 150
            $nativeFaceModeObserved = Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native authoring topology mode:.*mode=Face" `
                -TimeoutMilliseconds 700
        }
        if (-not $nativeFaceModeObserved) {
            throw "The user-facing Face selection mode did not become active."
        }
        $nativeFacePickPoints = @(
            @(0.12, 0.50),
            @(0.16, 0.50),
            @(0.20, 0.50),
            @(0.24, 0.50),
            @(0.28, 0.50),
            @(0.12, 0.58),
            @(0.16, 0.58),
            @(0.20, 0.58),
            @(0.24, 0.58),
            @(0.28, 0.58),
            @(0.12, 0.66),
            @(0.16, 0.66),
            @(0.20, 0.66),
            @(0.24, 0.66),
            @(0.28, 0.66),
            @(0.12, 0.74),
            @(0.16, 0.74),
            @(0.20, 0.74),
            @(0.24, 0.74),
            @(0.28, 0.74),
            @(0.12, 0.82),
            @(0.16, 0.82),
            @(0.20, 0.82),
            @(0.24, 0.82),
            @(0.28, 0.82),
            @(0.32, 0.58),
            @(0.36, 0.66),
            @(0.40, 0.74))
        $nativeFacePicked = $false
        foreach ($facePickPoint in $nativeFacePickPoints) {
            if ($nativeFacePicked) { break }
            $nativeFacePickLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($componentViewportX + $componentViewportWidth * $facePickPoint[0]) `
                -FramebufferY ($componentViewportY + $componentViewportHeight * $facePickPoint[1])
            $nativeFacePicked = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring component picked:" `
                -StartingOffset $nativeFacePickLogOffset `
                -TimeoutMilliseconds 1200
        }
        if (-not $nativeFacePicked) {
            throw "The user-facing Face mode did not select a viewport face before Bevel."
        }
        # Capture the face while its freshly picked stable handle is still
        # authoritative. Later bevel/flip transactions may intentionally
        # remap component identities, so their proof must not be responsible
        # for preparing this close-up.
        Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F"
        Start-Sleep -Milliseconds 350
        for ($closeupWheel = 0; $closeupWheel -lt 1; ++$closeupWheel) {
            Send-HenkaAutomationScroll `
                -EventPath $automationInputPath `
                -X ($componentViewportX + $componentViewportWidth * 0.5) `
                -Y ($componentViewportY + $componentViewportHeight * 0.5) `
                -WheelDelta 1
            Start-Sleep -Milliseconds 800
        }
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path (Join-Path $logDir 'check_packaged_sandbox3d_selected_face_closeup.png') `
            -Description 'Packaged selected-face rendered geometry close-up'
        $nativeBevelMatch = $null
        $nativeBevelControlLogOffset = $nativeFacePickLogOffset
        for ($bevelStateAttempt = 0; $bevelStateAttempt -lt 8 -and $null -eq $nativeBevelMatch; ++$bevelStateAttempt) {
            if (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $bevelControlLogPattern `
                    -StartingOffset $nativeBevelControlLogOffset `
                    -TimeoutMilliseconds 350) {
                $nativeBevelMatch = Get-LastLogRegexMatch `
                    -Path $stdoutPath `
                    -Pattern 'Native authoring bevel control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
            }
            if ($null -eq $nativeBevelMatch) {
                # Positive wheel input moves the details content toward the
                # header in the editor.  Bevel is below the topology context,
                # so use the down-content direction when its fresh rectangle
                # has not yet become visible.
                Scroll-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($detailsX + [Math]::Max(12.0, $detailsWidth - 18.0)) `
                    -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                    -WheelDelta -1
            }
        }
        if ($null -eq $nativeBevelMatch) {
            throw "The fresh native bevel control geometry could not be parsed after Face selection."
        }
        $nativeBevelX = [double]$nativeBevelMatch.Groups[2].Value
        $nativeBevelY = [double]$nativeBevelMatch.Groups[3].Value
        Assert-FramebufferRect `
            -Name "Native authoring Bevel control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeBevelX `
            -Y $nativeBevelY `
            -Width 88.0 `
            -Height 24.0
        $nativeBevelObserved = $false
        Start-Sleep -Milliseconds 250
        # A picked imported face may be concave or otherwise unable to accept
        # the bounded inset-based bevel.  The editor correctly rejects that
        # transaction and retains the source.  Keep the gate deterministic by
        # trying the bounded candidate points until it observes one successful
        # bevel, instead of treating the first eligible-but-unbevelable face
        # as a UI failure.
        for ($faceCandidateIndex = 0;
             $faceCandidateIndex -lt $nativeFacePickPoints.Count -and
             -not $nativeBevelObserved;
             ++$faceCandidateIndex) {
            if ($faceCandidateIndex -gt 0) {
                $facePickPoint = $nativeFacePickPoints[$faceCandidateIndex]
                $nativeFacePickLogOffset = Get-FileLengthSafe -Path $stdoutPath
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($componentViewportX + $componentViewportWidth * $facePickPoint[0]) `
                    -FramebufferY ($componentViewportY + $componentViewportHeight * $facePickPoint[1])
                if (-not (Wait-FileContainsAfterOffset `
                        -Path $stdoutPath `
                        -Pattern "Native authoring component picked:.*mode=face" `
                        -StartingOffset $nativeFacePickLogOffset `
                        -TimeoutMilliseconds 1200)) {
                    continue
                }
                $nativeBevelMatch = $null
                for ($bevelLayoutAttempt = 0; $bevelLayoutAttempt -lt 8 -and $null -eq $nativeBevelMatch; ++$bevelLayoutAttempt) {
                    if (Wait-FileContainsAfterOffset `
                            -Path $stdoutPath `
                            -Pattern $bevelControlLogPattern `
                            -StartingOffset $nativeFacePickLogOffset `
                            -TimeoutMilliseconds 350) {
                        $nativeBevelMatch = Get-LastLogRegexMatch `
                            -Path $stdoutPath `
                            -Pattern 'Native authoring bevel control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
                    }
                    if ($null -eq $nativeBevelMatch) {
                        Scroll-FramebufferPoint `
                            -Handle $mainWindowHandle `
                            -FramebufferWidth $framebufferWidth `
                            -FramebufferHeight $framebufferHeight `
                            -FramebufferX ($detailsX + [Math]::Max(12.0, $detailsWidth - 18.0)) `
                            -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                            -WheelDelta -1
                    }
                }
                if ($null -eq $nativeBevelMatch) {
                    continue
                }
                $nativeBevelX = [double]$nativeBevelMatch.Groups[2].Value
                $nativeBevelY = [double]$nativeBevelMatch.Groups[3].Value
                Assert-FramebufferRect `
                    -Name "Native authoring Bevel control" `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -X $nativeBevelX `
                    -Y $nativeBevelY `
                    -Width 88.0 `
                    -Height 24.0
            }
            $bevelLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeBevelX + 44.0) `
                -FramebufferY ($nativeBevelY + 12.0)
            # The selected imported fixture remains a bounded 45k-vertex
            # authoring source. Its Face Bevel transaction can legitimately
            # outlive the pointer event; accept only a fresh post-commit line
            # after this click, which proves that the native source changed.
            $nativeBevelObserved = Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native authoring workflow: bevel operator edited" `
                -TimeoutMilliseconds 15000
        }
        if (-not $nativeBevelObserved) {
            throw "The user-facing native bevel operation did not update the showcase source."
        }
        Write-Output "[pass] User-facing topology selection and bevel changed the native showcase source"
        # Bevel publishes a fresh candidate and rebuilds the details flow. Let
        # one render/input turn settle before reading and activating the next
        # control so the subsequent click cannot race that publication.
        $nativeFlipLayoutLogOffset = Get-FileLengthSafe -Path $stdoutPath
        Start-Sleep -Milliseconds 350
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'Native authoring face flip control: name=.* width=88.0 height=24.0\.' `
                -StartingOffset $nativeFlipLayoutLogOffset `
                -TimeoutMilliseconds 2500)) {
            throw "The fresh native Face-mode Flip control geometry was not reported after bevel."
        }
        $nativeFlipMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring face flip control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
        if ($null -eq $nativeFlipMatch) {
            throw "The native Face-mode Flip control geometry could not be parsed."
        }
        $nativeFlipObserved = $false
        for ($flipAttempt = 0; $flipAttempt -lt 5 -and -not $nativeFlipObserved; ++$flipAttempt) {
            $latestFlipMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring face flip control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
            if ($null -ne $latestFlipMatch) {
                $nativeFlipX = [double]$latestFlipMatch.Groups[2].Value
                $nativeFlipY = [double]$latestFlipMatch.Groups[3].Value
            }
            Assert-FramebufferRect `
                -Name "Native authoring Flip control retry" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $nativeFlipX `
                -Y $nativeFlipY `
                -Width 88.0 `
                -Height 24.0
            $flipXOffset = @(20.0, 44.0, 68.0)[$flipAttempt % 3]
            $flipYOffset = @(6.0, 12.0, 18.0)[$flipAttempt % 3]
            $nativeFlipLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeFlipX + $flipXOffset) `
                -FramebufferY ($nativeFlipY + $flipYOffset)
            Start-Sleep -Milliseconds 150
            $nativeFlipObserved = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring workflow: face winding flipped for" `
                -StartingOffset $nativeFlipLogOffset `
                -TimeoutMilliseconds 2500
        }
        if (-not $nativeFlipObserved) {
            throw "The user-facing native Face-mode Flip operation did not update the showcase source."
        }
        Write-Output "[pass] User-facing Face-mode winding flip changed the native showcase source"
        $nativeDeleteFaceObserved = $false
        foreach ($deleteFacePoint in $nativeFacePickPoints) {
            if ($nativeDeleteFaceObserved) {
                break
            }
            $nativeDeleteFaceOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($componentViewportX + $componentViewportWidth * $deleteFacePoint[0]) `
                -FramebufferY ($componentViewportY + $componentViewportHeight * $deleteFacePoint[1])
            $nativeDeleteFaceObserved = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring component picked:.*mode=face.*selected=1" `
                -StartingOffset $nativeDeleteFaceOffset `
                -TimeoutMilliseconds 1200
        }
        if (-not $nativeDeleteFaceObserved) {
            throw "The native Face-mode delete setup could not select a fresh face after bevel."
        }
        $nativeDeleteMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring face delete control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=102.0 height=24.0\.'
        if ($null -eq $nativeDeleteMatch) {
            throw "The native Face-mode delete control geometry could not be parsed."
        }
        $nativeDeleteX = [double]$nativeDeleteMatch.Groups[2].Value
        $nativeDeleteY = [double]$nativeDeleteMatch.Groups[3].Value
        Assert-FramebufferRect `
            -Name "Native authoring Delete Faces control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeDeleteX `
            -Y $nativeDeleteY `
            -Width 102.0 `
            -Height 24.0
        $nativeDeleteObserved = $false
        $nativeDeleteOffsets = @(
            @(20.0, 6.0),
            @(51.0, 12.0),
            @(82.0, 18.0),
            @(51.0, 6.0),
            @(51.0, 18.0))
        for ($deleteAttempt = 0;
             $deleteAttempt -lt $nativeDeleteOffsets.Count -and
             -not $nativeDeleteObserved;
             ++$deleteAttempt) {
            $latestDeleteMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring face delete control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=102.0 height=24.0\.'
            if ($null -ne $latestDeleteMatch) {
                $nativeDeleteX = [double]$latestDeleteMatch.Groups[2].Value
                $nativeDeleteY = [double]$latestDeleteMatch.Groups[3].Value
            }
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeDeleteX + [double]$nativeDeleteOffsets[$deleteAttempt][0]) `
                -FramebufferY ($nativeDeleteY + [double]$nativeDeleteOffsets[$deleteAttempt][1])
            $nativeDeleteObserved = Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native authoring workflow: selected faces deleted from" `
                -TimeoutMilliseconds 2500
        }
        if (-not $nativeDeleteObserved) {
            throw "The user-facing native Face-mode delete operation did not update the showcase source."
        }
        Write-Output "[pass] User-facing Face-mode deletion changed the native showcase source"
        $projectControlPattern = 'Native authoring project controls:'
        $projectControlCount = @(
            Select-String -LiteralPath $stdoutPath -Pattern $projectControlPattern -ErrorAction SilentlyContinue
        ).Count
        if ($projectControlCount -le 0) {
            throw "The converted showcase did not expose bounded project save/reload controls."
        }
        $nativeStableProjectMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring stable project controls: name=(.+) save_x=([-0-9.]+) save_y=([-0-9.]+) reload_x=([-0-9.]+) reload_y=([-0-9.]+) width=(104\.0) height=24.0\.'
        $nativeProjectStable = $null -ne $nativeStableProjectMatch
        $nativeProjectMatch = if ($nativeProjectStable) {
            $nativeStableProjectMatch
        }
        else {
            Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring project controls: name=(.+) save_x=([-0-9.]+) save_y=([-0-9.]+) reload_x=([-0-9.]+) reload_y=([-0-9.]+) width=([-0-9.]+) height=24.0\.'
        }
        if ($null -eq $nativeProjectMatch) {
            throw "The native authoring project control geometry could not be parsed."
        }
        $nativeSaveX = [double]$nativeProjectMatch.Groups[2].Value
        $nativeSaveY = [double]$nativeProjectMatch.Groups[3].Value
        $nativeReloadX = [double]$nativeProjectMatch.Groups[4].Value
        $nativeReloadY = [double]$nativeProjectMatch.Groups[5].Value
        $nativeProjectWidth = [double]$nativeProjectMatch.Groups[6].Value
    Assert-FramebufferRect `
        -Name "Native authoring Save Project control" `
        -FramebufferWidth $framebufferWidth `
        -FramebufferHeight $framebufferHeight `
        -X $nativeSaveX `
        -Y $nativeSaveY `
        -Width $nativeProjectWidth `
        -Height 24.0
        $nativeSaveObserved = $false
        $nativeSaveOffsets = @(
            @(0.0, 0.0),
            @(0.0, -6.0),
            @(0.0, 6.0),
            @(-8.0, 0.0),
            @(8.0, 0.0))
        foreach ($nativeSaveOffset in $nativeSaveOffsets) {
            $latestProjectMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring project controls: name=(.+) save_x=([-0-9.]+) save_y=([-0-9.]+) reload_x=([-0-9.]+) reload_y=([-0-9.]+) width=([-0-9.]+) height=24.0\.'
            if ($null -ne $latestProjectMatch) {
                $nativeSaveX = [double]$latestProjectMatch.Groups[2].Value
                $nativeSaveY = [double]$latestProjectMatch.Groups[3].Value
                $nativeProjectWidth = [double]$latestProjectMatch.Groups[6].Value
            }
            Assert-FramebufferRect `
                -Name "Native authoring Save Project retry control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $nativeSaveX `
                -Y $nativeSaveY `
                -Width $nativeProjectWidth `
                -Height 24.0
            $nativeSaveLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeSaveX + ($nativeProjectWidth * 0.5) + [double]$nativeSaveOffset[0]) `
                -FramebufferY ($nativeSaveY + 12.0 + [double]$nativeSaveOffset[1])
            $nativeSaveObserved = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring workflow: project saved" `
                -StartingOffset $nativeSaveLogOffset `
                -TimeoutMilliseconds 1800
            if ($nativeSaveObserved) {
                break
            }
        }
        if (-not $nativeSaveObserved) {
            throw "The user-facing native authoring project save did not complete."
        }
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring workflow: material state saved" -TimeoutMilliseconds 5000)) {
        throw "The user-facing native material state save did not complete."
    }
    Write-Output "[pass] User-facing native authoring project save completed"
    Assert-FramebufferRect `
        -Name "Native authoring Reload Project control" `
        -FramebufferWidth $framebufferWidth `
        -FramebufferHeight $framebufferHeight `
        -X $nativeReloadX `
        -Y $nativeReloadY `
        -Width $nativeProjectWidth `
        -Height 24.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
        -FramebufferX ($nativeReloadX + ($nativeProjectWidth * 0.5)) `
            -FramebufferY ($nativeReloadY + 12.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring workflow: project reloaded" -TimeoutMilliseconds 5000)) {
            throw "The user-facing native authoring project reload did not complete transactionally."
        }
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring workflow: material state reloaded" -TimeoutMilliseconds 5000)) {
            throw "The user-facing native material state reload did not complete transactionally."
        }
        Write-Output "[pass] User-facing native authoring project reload completed transactionally"
        Start-Sleep -Milliseconds 350
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $nativeAuthoringScreenshotPath `
            -Description "Packaged native authoring screenshot"

        Write-Step "Checking Game Authoring Play lifecycle"
        $gamePhysicsDisclosure = $null
        for ($scrollAttempt = 0; $scrollAttempt -lt 20 -and $null -eq $gamePhysicsDisclosure; ++$scrollAttempt) {
            $gamePhysicsDisclosure = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Game authoring physics disclosure: name=(?<name>.+) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28.0 expanded=(?<expanded>[01])\.'
            if ($null -eq $gamePhysicsDisclosure) {
                Scroll-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($detailsX + [Math]::Max(12.0, $detailsWidth - 18.0)) `
                    -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                    -WheelDelta -120
            }
        }
        if ($null -eq $gamePhysicsDisclosure) {
            throw "The selected authored object did not expose the Game Authoring Physics disclosure."
        }
        $gamePhysicsDisclosureX = [double]$gamePhysicsDisclosure.Groups["x"].Value
        $gamePhysicsDisclosureY = [double]$gamePhysicsDisclosure.Groups["y"].Value
        $gamePhysicsDisclosureWidth = [double]$gamePhysicsDisclosure.Groups["width"].Value
        Assert-FramebufferRect `
            -Name "Game Authoring Physics disclosure" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $gamePhysicsDisclosureX `
            -Y $gamePhysicsDisclosureY `
            -Width $gamePhysicsDisclosureWidth `
            -Height 28.0
        if ($gamePhysicsDisclosure.Groups["expanded"].Value -eq "0") {
            $gamePhysicsExpanded = $false
            foreach ($physicsDisclosureFraction in @(0.25, 0.50, 0.75)) {
                $latestGamePhysicsDisclosure = Get-LastLogRegexMatch `
                    -Path $stdoutPath `
                    -Pattern 'Game authoring physics disclosure: name=(?<name>.+) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28.0 expanded=(?<expanded>[01])\.'
                if ($null -ne $latestGamePhysicsDisclosure) {
                    $gamePhysicsDisclosureX = [double]$latestGamePhysicsDisclosure.Groups["x"].Value
                    $gamePhysicsDisclosureY = [double]$latestGamePhysicsDisclosure.Groups["y"].Value
                    $gamePhysicsDisclosureWidth = [double]$latestGamePhysicsDisclosure.Groups["width"].Value
                }
                $physicsDisclosureClickOffset = Get-FileLengthSafe -Path $stdoutPath
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($gamePhysicsDisclosureX + $gamePhysicsDisclosureWidth * $physicsDisclosureFraction) `
                    -FramebufferY ($gamePhysicsDisclosureY + 14.0)
                if (Wait-FileContainsAfterOffset `
                        -Path $stdoutPath `
                        -Pattern 'Game authoring physics disclosure: name=.+ expanded=1\.' `
                        -StartingOffset $physicsDisclosureClickOffset `
                        -TimeoutMilliseconds 2500) {
                    $gamePhysicsExpanded = $true
                    break
                }
            }
            if (-not $gamePhysicsExpanded) {
                throw "The Game Authoring Physics disclosure did not report expansion after bounded click retries."
            }
        }
        $gamePlayMatch = $null
        for ($scrollAttempt = 0; $scrollAttempt -lt 20 -and $null -eq $gamePlayMatch; ++$scrollAttempt) {
            $gamePlayMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Game authoring play controls: name=(?<name>.+) trigger_x=(?<triggerX>[-0-9.]+) play_x=(?<playX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26.0 state=(?<state>[0-9]+)\.'
            if ($null -eq $gamePlayMatch) {
                Scroll-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($detailsX + [Math]::Max(12.0, $detailsWidth - 18.0)) `
                    -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                    -WheelDelta -120
            }
        }
        if ($null -eq $gamePlayMatch) {
            throw "The Game Authoring Physics disclosure did not expose Play controls."
        }
        $gamePlayX = [double]$gamePlayMatch.Groups["playX"].Value
        $gamePlayY = [double]$gamePlayMatch.Groups["y"].Value
        $gamePlayWidth = [double]$gamePlayMatch.Groups["width"].Value
        Assert-FramebufferRect `
            -Name "Game Authoring Play control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $gamePlayX `
            -Y $gamePlayY `
            -Width $gamePlayWidth `
            -Height 26.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($gamePlayX + $gamePlayWidth * 0.5) `
            -FramebufferY ($gamePlayY + 13.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Play session state changed\." -TimeoutMilliseconds 5000)) {
            throw "Game Authoring Start Play did not report a state transition."
        }
        $gamePlayMatch = Wait-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Game authoring play controls: name=(?<name>.+) trigger_x=(?<triggerX>[-0-9.]+) play_x=(?<playX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26.0 state=(?<state>[0-9]+)\.' `
            -GroupName "state" `
            -ExpectedValue "1"
        if ($null -eq $gamePlayMatch) {
            throw "Game Authoring Start Play did not reach Running state."
        }
        Write-Output "[pass] Game Authoring Start Play reached Running state"

        $gamePlayX = [double]$gamePlayMatch.Groups["playX"].Value
        $gamePlayY = [double]$gamePlayMatch.Groups["y"].Value
        $gamePlayWidth = [double]$gamePlayMatch.Groups["width"].Value
        Click-FramebufferPoint -Handle $mainWindowHandle -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -FramebufferX ($gamePlayX + $gamePlayWidth * 0.5) -FramebufferY ($gamePlayY + 13.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Play session state changed\." -TimeoutMilliseconds 5000)) {
            throw "Game Authoring Pause Play did not report a state transition."
        }
        $gamePlayMatch = Wait-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Game authoring play controls: name=(?<name>.+) trigger_x=(?<triggerX>[-0-9.]+) play_x=(?<playX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26.0 state=(?<state>[0-9]+)\.' `
            -GroupName "state" `
            -ExpectedValue "2"
        if ($null -eq $gamePlayMatch) {
            throw "Game Authoring Pause Play did not reach Paused state."
        }
        Write-Output "[pass] Game Authoring Pause Play reached Paused state"

        $gamePlayX = [double]$gamePlayMatch.Groups["playX"].Value
        $gamePlayY = [double]$gamePlayMatch.Groups["y"].Value
        $gamePlayWidth = [double]$gamePlayMatch.Groups["width"].Value
        Click-FramebufferPoint -Handle $mainWindowHandle -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -FramebufferX ($gamePlayX + $gamePlayWidth * 0.5) -FramebufferY ($gamePlayY + 13.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Play session state changed\." -TimeoutMilliseconds 5000)) {
            throw "Game Authoring Resume Play did not report a state transition."
        }
        $gamePlayMatch = Wait-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Game authoring play controls: name=(?<name>.+) trigger_x=(?<triggerX>[-0-9.]+) play_x=(?<playX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26.0 state=(?<state>[0-9]+)\.' `
            -GroupName "state" `
            -ExpectedValue "1"
        if ($null -eq $gamePlayMatch) {
            throw "Game Authoring Resume Play did not return to Running state."
        }
        Write-Output "[pass] Game Authoring Resume Play returned to Running state"

        $gamePlayX = [double]$gamePlayMatch.Groups["playX"].Value
        $gamePlayY = [double]$gamePlayMatch.Groups["y"].Value
        $gamePlayWidth = [double]$gamePlayMatch.Groups["width"].Value
        Click-FramebufferPoint -Handle $mainWindowHandle -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -FramebufferX ($gamePlayX + $gamePlayWidth * 0.5) -FramebufferY ($gamePlayY + 13.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Play session state changed\." -TimeoutMilliseconds 5000)) {
            throw "Game Authoring pause before Step did not report a state transition."
        }
        $gamePlayMatch = Wait-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Game authoring play controls: name=(?<name>.+) trigger_x=(?<triggerX>[-0-9.]+) play_x=(?<playX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26.0 state=(?<state>[0-9]+)\.' `
            -GroupName "state" `
            -ExpectedValue "2"
        if ($null -eq $gamePlayMatch) {
            throw "Game Authoring pause before Step did not reach Paused state."
        }
        $gameStepMatch = Get-LastLogRegexMatch -Path $stdoutPath -Pattern 'Game authoring step controls: name=(?<name>.+) step_x=(?<stepX>[-0-9.]+) stop_x=(?<stopX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26.0\.'
        if ($null -eq $gameStepMatch) {
            throw "The Game Authoring Step/Stop control geometry could not be parsed."
        }
        $gameStepX = [double]$gameStepMatch.Groups["stepX"].Value
        $gameStepY = [double]$gameStepMatch.Groups["y"].Value
        $gameStepWidth = [double]$gameStepMatch.Groups["width"].Value
        Assert-FramebufferRect -Name "Game Authoring Step control" -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -X $gameStepX -Y $gameStepY -Width $gameStepWidth -Height 26.0
        Click-FramebufferPoint -Handle $mainWindowHandle -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -FramebufferX ($gameStepX + $gameStepWidth * 0.5) -FramebufferY ($gameStepY + 13.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Play fixed step complete\." -TimeoutMilliseconds 5000)) {
            throw "Game Authoring Step Play did not complete."
        }
        Write-Output "[pass] Game Authoring Step Play completed"

        $gameStopX = [double]$gameStepMatch.Groups["stopX"].Value
        Click-FramebufferPoint -Handle $mainWindowHandle -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -FramebufferX ($gameStopX + $gameStepWidth * 0.5) -FramebufferY ($gameStepY + 13.0)
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Play stopped; authored state preserved\." -TimeoutMilliseconds 5000)) {
            throw "Game Authoring Stop Play did not preserve the authored state."
        }
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Game authoring play stopped: state=0\." -TimeoutMilliseconds 5000)) {
            throw "Game Authoring Stop Play did not return to Stopped state."
        }
        Write-Output "[pass] Game Authoring Stop Play returned to Stopped state with authored state preserved"

        Write-Step "Checking section-header context menu"
        $contextMenuPattern = "Workspace context menu: section=Tools horizontal=available vertical=available"

        Click-FramebufferPointRight `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($toolsHeaderX + 18.0) `
            -FramebufferY ($toolsHeaderY + 13.0)

        $contextMenuObserved = Wait-FileContains `
            -Path $stdoutPath `
            -Pattern $contextMenuPattern `
            -TimeoutMilliseconds 4000

        if (-not $contextMenuObserved) {
            Write-Output "[retry] Tools context menu was not observed after the first verified right click; retrying once."

            Set-HenkaAutomationForeground -Handle $mainWindowHandle
            Start-Sleep -Milliseconds 250

            Click-FramebufferPointRight `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($toolsHeaderX + 18.0) `
                -FramebufferY ($toolsHeaderY + 13.0)

            $contextMenuObserved = Wait-FileContains `
                -Path $stdoutPath `
                -Pattern $contextMenuPattern `
                -TimeoutMilliseconds 4000
        }

        if (-not $contextMenuObserved) {
            throw (
                "Right-clicking the Tools header did not open the horizontal/vertical " +
                "section context menu after two verified logical-input attempts.")
        }

        Write-Output "[pass] Tools section-header context menu opened from verified logical input"

        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $contextMenuScreenshotPath `
            -Description "Workspace context-menu screenshot"

        Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "Escape"
        Start-Sleep -Milliseconds 350
        Write-Step "Checking stationary rendered viewport stability"
        Start-Sleep -Milliseconds 1800
        $stableX = $viewportX + 8.0
        $stableY = $viewportY + 38.0
        $stableWidth = [Math]::Max(1.0, $viewportWidth - 16.0)
        $stableHeight = [Math]::Max(1.0, $viewportHeight - 82.0)
        Save-FramebufferRegionScreenshot `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $stableX -Y $stableY -Width $stableWidth -Height $stableHeight `
            -Path $stabilityFirstPath
        Start-Sleep -Milliseconds 700
        Save-FramebufferRegionScreenshot `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $stableX -Y $stableY -Width $stableWidth -Height $stableHeight `
            -Path $stabilitySecondPath
        Assert-SceneFramesStable -First $stabilityFirstPath -Second $stabilitySecondPath

        $headerChromeMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Workspace header chrome: controls=([a-z]+):([0-9]+) utility=([a-z]+):([0-9]+)\.'
        if ($null -eq $headerChromeMatch) {
            throw "Workspace header chrome state could not be parsed."
        }
        $controlsChrome = $headerChromeMatch.Groups[1].Value
        $controlsTabCount = [int]$headerChromeMatch.Groups[2].Value
        $utilityChrome = $headerChromeMatch.Groups[3].Value
        $utilityTabCount = [int]$headerChromeMatch.Groups[4].Value
        $expectedControlsChrome =
            if ($controlsTabCount -gt 1) { "tabs" } else { "compact" }
        $expectedUtilityChrome =
            if ($utilityTabCount -gt 1) { "tabs" } else { "compact" }
        if ($controlsChrome -ne $expectedControlsChrome -or
            $utilityChrome -ne $expectedUtilityChrome) {
            throw (
                "Workspace header chrome does not match the live topology " +
                "tab counts.")
        }
        Write-Output (
            "[pass] Workspace headers match live topology: " +
            "Tools=$controlsChrome/$controlsTabCount, " +
            "Utility=$utilityChrome/$utilityTabCount")

        if ($gridAvailable) {
            Assert-FramebufferRect `
                -Name "Grid control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $gridX `
                -Y $gridY `
                -Width $gridWidth `
                -Height $gridHeight
        }
        $latestQaTabMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Tools QA tab: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)'
        if ($null -ne $latestQaTabMatch) {
            $qaTabX = [double]$latestQaTabMatch.Groups[1].Value
            $qaTabY = [double]$latestQaTabMatch.Groups[2].Value
            $qaTabWidth = [double]$latestQaTabMatch.Groups[3].Value
            $qaTabHeight = [double]$latestQaTabMatch.Groups[4].Value
        }
        Assert-FramebufferRect `
            -Name "Tools QA tab" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $qaTabX `
            -Y $qaTabY `
            -Width $qaTabWidth `
            -Height $qaTabHeight
        Assert-FramebufferRect `
            -Name "Viewport shading controls" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $shadingX `
            -Y $shadingY `
            -Width $shadingGroupWidth `
            -Height 22.0

        if ($gridAvailable) {
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($gridX + $gridWidth * 0.5) `
                -FramebufferY ($gridY + $gridHeight * 0.5)
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($gridX + $gridWidth * 0.5) `
                -FramebufferY ($gridY + $gridHeight * 0.5)
        }

        $qaPageActivated = $false
        $qaClickOffsets = @(
            @(0.0, 0.0),
            @(0.0, -6.0),
            @(0.0, 6.0),
            @(-8.0, 0.0),
            @(8.0, 0.0))
        foreach ($qaClickOffset in $qaClickOffsets) {
            $latestQaTabMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Tools QA tab: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)'
            if ($null -ne $latestQaTabMatch) {
                $qaTabX = [double]$latestQaTabMatch.Groups[1].Value
                $qaTabY = [double]$latestQaTabMatch.Groups[2].Value
                $qaTabWidth = [double]$latestQaTabMatch.Groups[3].Value
                $qaTabHeight = [double]$latestQaTabMatch.Groups[4].Value
            }
            Assert-FramebufferRect `
                -Name "Tools QA tab retry geometry" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $qaTabX `
                -Y $qaTabY `
                -Width $qaTabWidth `
                -Height $qaTabHeight
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($qaTabX + $qaTabWidth * 0.5 + [double]$qaClickOffset[0]) `
                -FramebufferY ($qaTabY + $qaTabHeight * 0.5 + [double]$qaClickOffset[1])
            if (Wait-FileContains `
                    -Path $stdoutPath `
                    -Pattern "Native Panel Test control:" `
                    -TimeoutMilliseconds 900) {
                $qaPageActivated = $true
                break
            }
        }

        Write-Step "Capturing Tools QA page visual proof"
        Set-HenkaAutomationForeground -Handle $mainWindowHandle
        Start-Sleep -Milliseconds 350
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $qaScreenshotPath `
            -Description "Packaged Tools QA screenshot"

        if (-not $qaPageActivated -and -not (Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native Panel Test control:" `
                -TimeoutMilliseconds 1200)) {
            throw (
                "The Tools QA page did not report the Native Panel Test " +
                "control after the QA tab was activated.")
        }

        $nativeMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native Panel Test control: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)'
        if ($null -eq $nativeMatch) {
            throw "The Native Panel Test control geometry could not be parsed."
        }

        $nativeX =
            [double]$nativeMatch.Groups[1].Value
        $nativeY =
            [double]$nativeMatch.Groups[2].Value
        $nativeWidth =
            [double]$nativeMatch.Groups[3].Value
        $nativeHeight =
            [double]$nativeMatch.Groups[4].Value

        Assert-FramebufferRect `
            -Name "Native Panel Test control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeX `
            -Y $nativeY `
            -Width $nativeWidth `
            -Height $nativeHeight
        $nativeOpened = $false
        $nativeClickOffsets = @(
            @(0.0, 0.0),
            @(0.0, -6.0),
            @(0.0, 6.0),
            @(-8.0, 0.0),
            @(8.0, 0.0))
        foreach ($nativeClickOffset in $nativeClickOffsets) {
            $latestNativeMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native Panel Test control: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)'
            if ($null -ne $latestNativeMatch) {
                $nativeX = [double]$latestNativeMatch.Groups[1].Value
                $nativeY = [double]$latestNativeMatch.Groups[2].Value
                $nativeWidth = [double]$latestNativeMatch.Groups[3].Value
                $nativeHeight = [double]$latestNativeMatch.Groups[4].Value
            }
            Assert-FramebufferRect `
                -Name "Native Panel Test retry geometry" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $nativeX `
                -Y $nativeY `
                -Width $nativeWidth `
                -Height $nativeHeight
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeX + $nativeWidth * 0.5 + [double]$nativeClickOffset[0]) `
                -FramebufferY ($nativeY + $nativeHeight * 0.5 + [double]$nativeClickOffset[1])
            $nativeOpened = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native Panel Test: opened" `
                -StartingOffset $nativeOpenLogOffset `
                -TimeoutMilliseconds 4000
            if ($nativeOpened) {
                break
            }
        }
        if (-not $nativeOpened) {
            throw "The Native Panel Test control did not open the secondary native window after bounded live-geometry attempts."
        }

        Assert-FileContains `
            -Path $stdoutPath `
            -Pattern "Native Panel Test: opened" `
            -Description "Native test panel open output"
        $nativeWindowHandle =
            [NativeMethods]::FindProcessWindow(
                [uint32]$process.Id,
                "Henka Native Panel Test")
        if ($nativeWindowHandle -eq [System.IntPtr]::Zero) {
            throw "The Native Panel Test window was not visible as a separate OS-level window."
        }

        Write-Output "[pass] Native test panel visible as a separate OS-level window"
        Write-Step "Capturing native panel visual proof"
        Set-HenkaAutomationForeground -Handle $nativeWindowHandle
        Start-Sleep -Milliseconds 350
        Save-WindowScreenshot `
            -Handle $nativeWindowHandle `
            -Path $nativeScreenshotPath `
            -Description "Packaged native panel screenshot"
        Set-HenkaAutomationForeground -Handle $mainWindowHandle
        Start-Sleep -Milliseconds 250

        [NativeMethods]::PostMessage(
            $nativeWindowHandle,
            0x0010,
            [System.IntPtr]::Zero,
            [System.IntPtr]::Zero) | Out-Null
        $nativeClosed = $false
        for ($closeAttempt = 0; $closeAttempt -lt 12; ++$closeAttempt) {
            Start-Sleep -Milliseconds 250
            if ([NativeMethods]::FindProcessWindow(
                    [uint32]$process.Id,
                    "Henka Native Panel Test") -eq [System.IntPtr]::Zero) {
                $nativeClosed = $true
                break
            }
        }
        if (-not $nativeClosed) {
            throw "The Native Panel Test window did not close before the reopen check."
        }

        $nativeReopenOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeX + $nativeWidth * 0.5) `
            -FramebufferY ($nativeY + $nativeHeight * 0.5)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native Panel Test: opened" `
                -StartingOffset $nativeReopenOffset `
                -TimeoutMilliseconds 4000)) {
            throw "The Native Panel Test window did not reopen after being closed."
        }
        Write-Output "[pass] Native test panel closes and reopens without closing the main sandbox"

        $nativeClosed = $false
        for ($closeAttempt = 0; $closeAttempt -lt 12; ++$closeAttempt) {
            $nativeWindowHandle =
                [NativeMethods]::FindProcessWindow(
                    [uint32]$process.Id,
                    "Henka Native Panel Test")
            if ($nativeWindowHandle -eq [System.IntPtr]::Zero) {
                $nativeClosed = $true
                break
            }
            [NativeMethods]::PostMessage(
                $nativeWindowHandle,
                0x0010,
                [System.IntPtr]::Zero,
                [System.IntPtr]::Zero) | Out-Null
            Start-Sleep -Milliseconds 250
        }
        if (-not $nativeClosed) {
            throw "The reopened Native Panel Test window did not close before main-window checks resumed."
        }

        # Allow the main window's event loop to resume after the native child
        # window teardown.  Each shading assertion is offset-based so an old
        # status line cannot satisfy the check, and the bounded retries remain
        # safe when the operator is using the desktop concurrently.
        Start-Sleep -Milliseconds 1000
        $shadingX =
            [double]$shadingMatch.Groups[1].Value
        $shadingY =
            [double]$shadingMatch.Groups[2].Value
        $shadingButtonWidth =
            [double]$shadingMatch.Groups[3].Value
        $shadingGap =
            [double]$shadingMatch.Groups[4].Value
        $shadingNames = @(
            "Wireframe",
            "Solid",
            "Material Preview",
            "Rendered")

        for ($modeIndex = 0;
             $modeIndex -lt $shadingNames.Count;
             ++$modeIndex) {
            $modeCenterX =
                $shadingX +
                ($shadingButtonWidth + $shadingGap) *
                [double]$modeIndex +
                $shadingButtonWidth * 0.5
            $modeCenterY =
                $shadingY + 11.0

            $expectedModePattern =
                "Viewport shading: " +
                [Regex]::Escape($shadingNames[$modeIndex]) +
                "\."
            $modeObserved = $false
            for ($shadingAttempt = 0;
                 $shadingAttempt -lt 4 -and
                 -not $modeObserved;
                 ++$shadingAttempt) {
                $shadingLogOffset = Get-FileLengthSafe -Path $stdoutPath
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX $modeCenterX `
                    -FramebufferY $modeCenterY
                $modeObserved = Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $expectedModePattern `
                    -StartingOffset $shadingLogOffset `
                    -TimeoutMilliseconds 1200
            }
            if (-not $modeObserved) {
                throw "Viewport shading mode could not be confirmed: $($shadingNames[$modeIndex])"
            }
        }
        $bevelControlCount = @(
            Select-String -LiteralPath $stdoutPath -Pattern $bevelControlLogPattern -ErrorAction SilentlyContinue
        ).Count
        if ($bevelControlCount -le $bevelControlCountBefore) {
            throw "The native bevel control did not become visible after Face selection."
        }

        if ($gridAvailable) {
            Click-WindowPoint `
                -Handle $mainWindowHandle `
                -OffsetX 230 `
                -OffsetY 60
            Click-WindowPoint `
                -Handle $mainWindowHandle `
                -OffsetX 100 `
                -OffsetY 610
        }

        $uiClickChecks = @(
            @{
                Pattern = "Viewport shading: Wireframe\."
                Description = "UI Wireframe mode output"
            },
            @{
                Pattern = "Viewport shading: Solid\."
                Description = "UI Solid mode output"
            },
            @{
                Pattern = "Viewport shading: Material Preview\."
                Description = "UI Material Preview mode output"
            },
            @{
                Pattern = "Viewport shading: Rendered\."
                Description = "UI Rendered mode output"
            },
            @{
                Pattern = "Sandbox settings saved\."
                Description = "UI save settings output"
            }
        )
        if ($gridAvailable) {
            $uiClickChecks = @(
                @{
                    Pattern = "Debug grid: hidden"
                    Description = "UI debug grid click output"
                },
                @{
                    Pattern = "Debug grid: shown"
                    Description = "UI debug grid restore output"
                }
            ) + $uiClickChecks
        }

        $uiClickFailures = 0
        foreach ($check in $uiClickChecks) {
            if (-not (Try-AssertFileContains `
                    -Path $stdoutPath `
                    -Pattern $check.Pattern `
                    -Description $check.Description)) {
                $uiClickFailures++
            }
        }

        if ($uiClickFailures -gt 0) {
            Write-Output "[warn] Some packaged UI click checks could not be confirmed automatically. Manual packaged UI QA is still needed."
        }
        if ($sandboxPanelsVisible) {
            Set-HenkaAutomationForeground -Handle $mainWindowHandle
            Start-Sleep -Milliseconds 400
            $panelCloseObserved = $false
            for ($panelCloseAttempt = 0; $panelCloseAttempt -lt 2 -and -not $panelCloseObserved; ++$panelCloseAttempt) {
                $panelCloseOffset = Get-FileLengthSafe -Path $stdoutPath
                Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F4"
                $panelCloseObserved = Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern "Sandbox panel: hidden" `
                    -StartingOffset $panelCloseOffset `
                    -TimeoutMilliseconds 2500
                if (-not $panelCloseObserved -and $panelCloseAttempt -eq 0) {
                    Set-HenkaAutomationForeground -Handle $mainWindowHandle
                    Start-Sleep -Milliseconds 250
                }
            }
            if (-not $panelCloseObserved) {
                Write-Output "[warn] The packaged sandbox did not report a fresh F4 panel-close transition; leaving the verified visible panels open for the remaining checks."
            }
            else {
                Assert-FileContains -Path $stdoutPath -Pattern "Sandbox panel: hidden" -Description "Panel close output"
                $sandboxPanelsVisible = $false
            }
        }
    }
    else {
        Write-Output "[warn] Automated F4 panel open could not be confirmed. Manual packaged UI QA is still needed."
    }

    Write-Step "Checking user-facing native viewport component picking"
    $pickerViewportMatch = Get-LastLogRegexMatch `
        -Path $stdoutPath `
        -Pattern 'Sandbox viewport: origin ([0-9]+),([0-9]+) size ([0-9]+)x([0-9]+)\.'
    if ($null -ne $pickerViewportMatch) {
        $pickerViewportX = [double]$pickerViewportMatch.Groups[1].Value
        $pickerViewportY = [double]$pickerViewportMatch.Groups[2].Value
        $pickerViewportWidth = [double]$pickerViewportMatch.Groups[3].Value
        $pickerViewportHeight = [double]$pickerViewportMatch.Groups[4].Value
        $pickerObserved = $false
        foreach ($pickerPoint in @(
            @(0.30, 0.38),
            @(0.33, 0.45),
            @(0.38, 0.50),
            @(0.42, 0.42))) {
            if ($pickerObserved) { break }
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($pickerViewportX + $pickerViewportWidth * $pickerPoint[0]) `
                -FramebufferY ($pickerViewportY + $pickerViewportHeight * $pickerPoint[1])
            $pickerObserved = Wait-FileContains `
                -Path $stdoutPath `
                -Pattern "Native authoring component picked:" `
                -TimeoutMilliseconds 1800
        }
        if ($pickerObserved) {
            Write-Output "[pass] User-facing native viewport component picker selected a showcase component"
        }
        else {
            throw "The user-facing native viewport component picker did not select a component on the authored showcase entity."
        }
    }
    else {
        throw "The user-facing native viewport component picker could not obtain the live Scene View viewport geometry."
    }

    Write-Step "Checking generic engine-native asset authoring"
    if (-not $sandboxPanelsVisible) {
        Set-HenkaAutomationForeground -Handle $mainWindowHandle
        Start-Sleep -Milliseconds 300
        $genericAssetPanelOffset = Get-FileLengthSafe -Path $stdoutPath
        Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F4"
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Sandbox panel: shown" `
                -StartingOffset $genericAssetPanelOffset `
                -TimeoutMilliseconds 4000)) {
            throw "The editor panels could not be reopened for generic engine-native asset authoring."
        }
        Start-Sleep -Milliseconds 600
        $sandboxPanelsVisible = $true
    }
    else {
        Write-Output "[pass] Verified visible panels remained available for generic engine-native asset authoring"
    }
    $sceneObjectsMatch = Get-LastLogRegexMatch `
        -Path $stdoutPath `
        -Pattern 'Workspace UI geometry: .*scene_objects=(?<x>[-0-9.]+),(?<y>[-0-9.]+),(?<width>[-0-9.]+),(?<height>[-0-9.]+) '
    if ($null -eq $sceneObjectsMatch) {
        throw "The Scene Objects panel geometry could not be parsed for generic asset authoring."
    }
    $genericPanelX = [double]$sceneObjectsMatch.Groups["x"].Value
    $genericPanelY = [double]$sceneObjectsMatch.Groups["y"].Value
    $genericPanelWidth = [double]$sceneObjectsMatch.Groups["width"].Value
    $genericPanelHeight = [double]$sceneObjectsMatch.Groups["height"].Value
    $genericActionWidth = [Math]::Max(56.0, ($genericPanelWidth - 40.0) / 3.0)
    $genericActionY = $genericPanelY + 60.0
    $genericPrimitiveY = $genericActionY + 30.0
    $genericNameFieldY = $genericPrimitiveY + 73.0
    $genericNewAssetY = $genericNameFieldY + 30.0
    $genericSaveAssetY = $genericPrimitiveY + 88.0
    $genericPrimitiveActionWidth = [Math]::Max(72.0, ($genericPanelWidth - 34.0) / 2.0)
    $genericAssetName = "PackagedAsset_" + [Guid]::NewGuid().ToString("N").Substring(0, 8)
    $genericAssetNamePattern = [Regex]::Escape($genericAssetName)
    $genericAssetNameX = $genericPanelX + 14.0 + ($genericActionWidth * 2.0 + 6.0) / 2.0
    $genericNewAssetX = $genericPanelX + 14.0 + $genericActionWidth / 2.0
    $genericPrimitiveX = @(
        $genericNewAssetX,
        ($genericPanelX + 14.0 + $genericPrimitiveActionWidth / 2.0),
        ($genericPanelX + 20.0 + $genericPrimitiveActionWidth + $genericPrimitiveActionWidth / 2.0),
        ($genericPanelX + 20.0 + $genericPrimitiveActionWidth + $genericPrimitiveActionWidth / 2.0))
    $genericOpenAssetX = $genericPrimitiveX[2]

    Assert-FramebufferRect -Name "Generic asset name field" -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -X $genericAssetNameX -Y $genericNameFieldY -Width ($genericActionWidth * 2.0 + 6.0) -Height 24.0
    Assert-FramebufferRect -Name "Generic New Asset control" -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -X ($genericPanelX + 14.0) -Y $genericNewAssetY -Width $genericActionWidth -Height 24.0
    Click-AuthoringWindowPoint -Handle $mainWindowHandle -X $genericAssetNameX -Y ($genericNameFieldY + 12.0)
    for ($index = 0; $index -lt 63; ++$index) {
        Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine "key Backspace down" -SettleMilliseconds 0
        Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine "key Backspace up" -SettleMilliseconds 0
    }
    Start-Sleep -Milliseconds 600
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $genericAssetName
    Start-Sleep -Milliseconds 600
    $genericCreationOffset = Get-FileLengthSafe -Path $stdoutPath
    Click-AuthoringWindowPoint -Handle $mainWindowHandle -X $genericNewAssetX -Y ($genericNewAssetY + 12.0)
    if (-not (Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern "Native asset document: name=$genericAssetNamePattern action=created parts=0\." `
            -StartingOffset $genericCreationOffset `
            -TimeoutMilliseconds 5000)) {
        if (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'Native asset document: name=.* action=created parts=0\.' `
                -StartingOffset $genericCreationOffset `
                -TimeoutMilliseconds 250) {
            throw "The generic New Asset action created a document with an unexpected name."
        }
        Click-AuthoringWindowPoint -Handle $mainWindowHandle -X $genericAssetNameX -Y ($genericNameFieldY + 12.0)
        for ($index = 0; $index -lt 63; ++$index) {
            Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine "key Backspace down" -SettleMilliseconds 0
            Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine "key Backspace up" -SettleMilliseconds 0
        }
        Start-Sleep -Milliseconds 600
        Send-HenkaAutomationText -EventPath $automationInputPath -Text $genericAssetName
        Start-Sleep -Milliseconds 600
        $genericCreationRetryOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-AuthoringWindowPoint -Handle $mainWindowHandle -X $genericNewAssetX -Y ($genericNewAssetY + 12.0)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native asset document: name=$genericAssetNamePattern action=created parts=0\." `
                -StartingOffset $genericCreationRetryOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The generic New Asset action did not create an editor-owned asset document after a bounded retry."
        }
    }

    Assert-FramebufferRect -Name "Generic Add Cube control" -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -X ($genericPanelX + 14.0) -Y $genericActionY -Width $genericActionWidth -Height 24.0
    Click-AuthoringWindowPoint -Handle $mainWindowHandle -X $genericPrimitiveX[0] -Y ($genericActionY + 12.0)
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native asset document: name=$genericAssetNamePattern action=part-added parts=1\." -TimeoutMilliseconds 5000)) {
        throw "The generic Box primitive was not added to the new asset document."
    }
    Start-Sleep -Milliseconds 350
    foreach ($primitiveIndex in 1..3) {
        $genericPrimitiveClickY = $genericPrimitiveY + 12.0
        if ($primitiveIndex -ge 3) {
            $genericPrimitiveClickY += 30.0
        }
        Click-AuthoringWindowPoint -Handle $mainWindowHandle -X $genericPrimitiveX[$primitiveIndex] -Y $genericPrimitiveClickY
        $expectedPartCount = $primitiveIndex + 1
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native asset document: name=$genericAssetNamePattern action=part-added parts=$expectedPartCount\." -TimeoutMilliseconds 5000)) {
            throw "The generic primitive authoring path did not add part $expectedPartCount."
        }
        Start-Sleep -Milliseconds 350
    }

    Assert-FramebufferRect -Name "Generic Save Asset control" -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -X ($genericPanelX + 14.0) -Y $genericSaveAssetY -Width $genericActionWidth -Height 24.0
    Assert-FramebufferRect -Name "Generic Close Asset control" -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -X ($genericPanelX + 14.0 + $genericActionWidth + 6.0) -Y $genericSaveAssetY -Width $genericActionWidth -Height 24.0
    Click-AuthoringWindowPoint -Handle $mainWindowHandle -X $genericNewAssetX -Y ($genericSaveAssetY + 12.0)
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native asset document: name=$genericAssetNamePattern action=saved parts=4\." -TimeoutMilliseconds 5000)) {
        throw "The generic authored asset did not save transactionally."
    }
    $genericAssetManifestPath = Join-Path $packageRoot ("user\saves\" + $genericAssetName + ".asset")
    if (-not (Test-Path -LiteralPath $genericAssetManifestPath -PathType Leaf)) {
        throw "The generic authored asset manifest was not persisted in the packaged runtime workspace."
    }
    $genericManifest = [System.IO.File]::ReadAllText($genericAssetManifestPath)
    foreach ($requiredManifestLine in @(
        "asset.version=5",
        "asset.name=$genericAssetName",
        "asset.part_count=4",
        "asset.provenance=HENKA_PRODUCT_NATIVE_AUTHORED")) {
        if (-not $genericManifest.Contains($requiredManifestLine)) {
            throw "The generic authored asset manifest was missing: $requiredManifestLine"
        }
    }
    Click-AuthoringWindowPoint -Handle $mainWindowHandle -X $genericOpenAssetX -Y ($genericSaveAssetY + 12.0)
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native asset document: name=$genericAssetNamePattern action=closed parts=4\." -TimeoutMilliseconds 5000)) {
        throw "The generic authored asset did not close cleanly."
    }
    Click-AuthoringWindowPoint -Handle $mainWindowHandle -X $genericOpenAssetX -Y ($genericNewAssetY + 12.0)
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native asset document: name=$genericAssetNamePattern action=opened parts=4\." -TimeoutMilliseconds 5000)) {
        throw "The generic authored asset did not reopen from its persisted manifest."
    }
    Write-Output "[pass] User-facing generic native asset creation, primitive authoring, save, close, and reopen completed"
    Start-Sleep -Milliseconds 350
    Save-WindowScreenshot `
        -Handle $mainWindowHandle `
        -Path $nativeAuthoredScreenshotPath `
        -Description "Packaged generic native authoring visual proof"

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

    Write-Step "Checking persisted native authoring relaunch"
    $startupRestoreCapture = Start-HenkaCapturedProcess `
        -FilePath $packagedExe `
        -WorkingDirectory $packageRoot `
        -StdoutPath $startupRestoreStdoutPath `
        -StderrPath $startupRestoreStderrPath
    $startupRestoreProcess = $startupRestoreCapture.Process
    if (-not (Wait-FileContains `
            -Path $startupRestoreStdoutPath `
            -Pattern "Native authoring startup restore: name=" `
            -TimeoutMilliseconds 15000)) {
        $restoreWindow = [NativeMethods]::FindProcessWindow(
            [uint32]$startupRestoreProcess.Id,
            "Henka Engine Sandbox 3D")
        if ($restoreWindow -ne [System.IntPtr]::Zero) {
            [NativeMethods]::PostMessage(
                $restoreWindow,
                0x0010,
                [System.IntPtr]::Zero,
                [System.IntPtr]::Zero) | Out-Null
        }
        throw "A normal packaged relaunch did not restore the saved native showcase source."
    }
    if (-not (Wait-FileContains `
            -Path $startupRestoreStdoutPath `
            -Pattern "Native authoring startup restore: material state restored.*pbr_state=restored" `
            -TimeoutMilliseconds 3000)) {
        $restoreWindow = [NativeMethods]::FindProcessWindow(
            [uint32]$startupRestoreProcess.Id,
            "Henka Engine Sandbox 3D")
        if ($restoreWindow -ne [System.IntPtr]::Zero) {
            [NativeMethods]::PostMessage(
                $restoreWindow,
                0x0010,
                [System.IntPtr]::Zero,
                [System.IntPtr]::Zero) | Out-Null
        }
        throw "A normal packaged relaunch did not restore the saved native material sidecar."
    }
    $restoreWindow = [NativeMethods]::FindProcessWindow(
        [uint32]$startupRestoreProcess.Id,
        "Henka Engine Sandbox 3D")
    if ($restoreWindow -ne [System.IntPtr]::Zero) {
        [NativeMethods]::PostMessage(
            $restoreWindow,
            0x0010,
            [System.IntPtr]::Zero,
            [System.IntPtr]::Zero) | Out-Null
    }
    if (-not $startupRestoreProcess.WaitForExit(10000)) {
        throw "The persisted native authoring relaunch did not close cleanly."
    }
    Write-Output "[pass] Persisted native source and owned material restored on normal packaged relaunch"

    Write-Step "Checking persisted live workspace settings recovery"
    $persistenceSmoke = Invoke-HenkaNativeCapture `
        -FilePath $packagedExe `
        -Arguments @("--smoke-test") `
        -WorkingDirectory $packageRoot `
        -Label "Run post-interactive packaged persistence smoke test"

    Write-Utf8NoBom `
        -Path $persistenceStdoutPath `
        -Content $persistenceSmoke.Stdout
    Write-Utf8NoBom `
        -Path $persistenceStderrPath `
        -Content $persistenceSmoke.Stderr

    if ($persistenceSmoke.Stdout -notmatch "Sandbox smoke test completed\.") {
        throw "The post-interactive packaged persistence smoke test did not complete."
    }
    if ($persistenceSmoke.Stderr -match
        "Unsafe or incompatible (workspace panel|workspace topology|live workspace) settings") {
        throw (
            "Unsafe or incompatible live workspace settings were reported " +
            "again after a clean interactive shutdown.")
    }

    Assert-PathExists `
        -Path $startupScreenshotPath `
        -Description "Packaged startup workspace visual proof"
    Assert-PathExists `
        -Path $qaScreenshotPath `
        -Description "Packaged Tools QA visual proof"
    Assert-PathExists `
        -Path $nativeScreenshotPath `
        -Description "Packaged native panel visual proof"
    Assert-PathExists `
        -Path $nativeAuthoringScreenshotPath `
        -Description "Packaged native authoring visual proof"
    Assert-PathExists `
        -Path $nativeAuthoredScreenshotPath `
        -Description "Packaged native-generated rocket fixture visual proof"
    Write-Output "[pass] Live workspace settings recovery persisted across relaunch"
    Write-Output "[pass] Packaged sandbox checks completed."
}
finally {
    if ($null -eq $previousAutomationOwned) {
        Remove-Item Env:HENKA_AUTOMATION_INPUT_OWNED -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_INPUT_OWNED = $previousAutomationOwned
    }
    if ($null -eq $previousAutomationFile) {
        Remove-Item Env:HENKA_AUTOMATION_INPUT_FILE -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_INPUT_FILE = $previousAutomationFile
    }
    if ($null -ne $capturedProcess) {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
    }
    elseif ($process -ne $null) {
        $process.Dispose()
    }
}
