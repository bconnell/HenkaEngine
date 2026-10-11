param(
    [switch]$NonInteractive,

    [switch]$ContractOnly,

    [switch]$TerrainStartupOnly,

    [switch]$ProductStartupPrimitiveOnly,

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
. (Join-Path $PSScriptRoot "henka_packaged_startup_readiness.ps1")
. (Join-Path $PSScriptRoot "henka_packaged_scene_objects.ps1")

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

function Normalize-HenkaLogLineEndings {
    param([Parameter(Mandatory = $true)][string]$Text)

    return $Text.Replace("`r`n", "`n").Replace("`r", "`n")
}

function Get-LogPatternCount {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return 0
    }

    return @(
        Select-String -LiteralPath $Path -Pattern $Pattern -ErrorAction SilentlyContinue
    ).Count
}

function Assert-PackagedGeometryTelemetryStable {
    param(
        [Parameter(Mandatory = $true)][string]$Description,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [int]$MinimumRecords = 1,
        [int]$SettleMilliseconds = 350,
        [int]$IdleMilliseconds = 500
    )

    Start-Sleep -Milliseconds $SettleMilliseconds
    $before = Get-LogPatternCount -Path $stdoutPath -Pattern $Pattern
    if ($before -lt $MinimumRecords) {
        throw "$Description did not produce the required geometry telemetry ($before records)."
    }

    Start-Sleep -Milliseconds $IdleMilliseconds
    $after = Get-LogPatternCount -Path $stdoutPath -Pattern $Pattern
    if ($after -ne $before) {
        throw "$Description telemetry grew while the packaged editor was idle ($before -> $after records)."
    }

    Write-Output "[pass] $Description geometry telemetry remained stable while idle ($after records)"
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
                    $text = Normalize-HenkaLogLineEndings -Text $reader.ReadToEnd()
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
            $text = Normalize-HenkaLogLineEndings -Text $reader.ReadToEnd()
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

function Get-HenkaModelingToolbarFields {
    param([Parameter(Mandatory = $true)][string]$Path)

    $match = Get-LastLogRegexMatch `
        -Path $Path `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC modeling_toolbar (?<fields>[^\r\n]+)'
    if ($null -eq $match) {
        return $null
    }

    $fields = @{}
    foreach ($field in $match.Groups['fields'].Value.Split(
            ' ',
            [System.StringSplitOptions]::RemoveEmptyEntries)) {
        $separator = $field.IndexOf('=')
        if ($separator -gt 0) {
            $fields[$field.Substring(0, $separator)] = $field.Substring($separator + 1)
        }
    }
    return ,$fields
}

function Get-HenkaModelingToolbarRect {
    param(
        [Parameter(Mandatory = $true)][hashtable]$Fields,
        [Parameter(Mandatory = $true)][string]$Name
    )

    if (-not $Fields.ContainsKey($Name)) {
        throw "The product toolbar diagnostic omitted '$Name'."
    }
    $parts = $Fields[$Name].Split(',')
    if ($parts.Count -ne 4) {
        throw "The product toolbar diagnostic returned malformed geometry for '$Name'."
    }
    $culture = [Globalization.CultureInfo]::InvariantCulture
    return [pscustomobject]@{
        X = [double]::Parse($parts[0], $culture)
        Y = [double]::Parse($parts[1], $culture)
        Width = [double]::Parse($parts[2], $culture)
        Height = [double]::Parse($parts[3], $culture)
    }
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

function Invoke-PackagedProductGameAuthoringWorkflow {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$DetailsX,
        [Parameter(Mandatory = $true)][double]$DetailsY,
        [Parameter(Mandatory = $true)][double]$DetailsWidth,
        [Parameter(Mandatory = $true)][double]$DetailsHeight,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$ScreenshotPath
    )

    $toolsControl = Get-LastLogRegexMatch `
        -Path $StdoutPath `
        -Pattern 'Scene View Tools control: x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+)\.'
    if ($null -eq $toolsControl) {
        throw "The product-startup Scene View did not report its work-context control."
    }

    $toolsX = [double]$toolsControl.Groups["x"].Value
    $toolsY = [double]$toolsControl.Groups["y"].Value
    $toolsWidth = [double]$toolsControl.Groups["width"].Value
    if ([Math]::Abs($toolsWidth - 62.0) -lt 0.1) {
        $contextWidth = 210.0
    }
    elseif ([Math]::Abs($toolsWidth - 58.0) -lt 0.1) {
        $contextWidth = 176.0
    }
    else {
        throw "The product-startup Scene View reported unsupported Tools width $toolsWidth."
    }

    $contextSegmentWidth = $contextWidth / 3.0
    $gameContextX = $toolsX - $contextWidth - 4.0 + $contextSegmentWidth
    $contextOffset = Get-FileLengthSafe -Path $StdoutPath
    Click-FramebufferPoint `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -FramebufferX ($gameContextX + $contextSegmentWidth * 0.5) `
        -FramebufferY ($toolsY + 11.0)

    $disclosurePattern = '^Game authoring physics disclosure: name=(?<name>.+) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28\.0 expanded=(?<expanded>[01])\.'
    $disclosureVisiblePattern = '^Game authoring physics disclosure: name=.+ height=28\.0 expanded=[01]\.'
    $disclosureVisible = Wait-FileContainsAfterOffset `
        -Path $StdoutPath `
        -Pattern $disclosureVisiblePattern `
        -StartingOffset $contextOffset `
        -TimeoutMilliseconds 350
    $detailsScrollCount = 0
    for (; -not $disclosureVisible -and $detailsScrollCount -lt 12; ++$detailsScrollCount) {
        Scroll-FramebufferPointAndWaitForConsumption `
            -Handle $Handle `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -FramebufferX ($DetailsX + [Math]::Max(12.0, $DetailsWidth - 18.0)) `
            -FramebufferY ($DetailsY + [Math]::Max(30.0, $DetailsHeight * 0.55)) `
            -WheelDelta -1 `
            -TimeoutMilliseconds 3000
        $disclosureVisible = Wait-FileContainsAfterOffset `
            -Path $StdoutPath `
            -Pattern $disclosureVisiblePattern `
            -StartingOffset $contextOffset `
            -TimeoutMilliseconds 350
    }
    if (-not $disclosureVisible) {
        throw "The selected product-native Add Cube did not expose the Game Authoring Physics disclosure after 12 consumed Object Details scroll steps."
    }
    if ($detailsScrollCount -gt 0) {
        Write-Output ("[pass] Game Authoring Physics disclosure became visible after {0} consumed Object Details scroll step(s)" -f $detailsScrollCount)
    }
    $disclosure = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $disclosurePattern
    if ($null -eq $disclosure) {
        throw "The product-startup Game Authoring disclosure geometry could not be parsed."
    }
    $targetName = $disclosure.Groups["name"].Value.Trim()
    if ([string]::IsNullOrWhiteSpace($targetName) -or $targetName -eq "Ground") {
        throw "The Game Authoring workflow did not remain bound to the newly selected Add Cube object."
    }
    $escapedTargetName = [Regex]::Escape($targetName)
    $targetDisclosurePattern = '^Game authoring physics disclosure: name=' + $escapedTargetName + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28\.0 expanded=(?<expanded>[01])\.'

    if ($disclosure.Groups["expanded"].Value -eq "0") {
        $expanded = $false
        foreach ($fraction in @(0.25, 0.50, 0.75)) {
            $latestDisclosure = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $targetDisclosurePattern
            if ($null -eq $latestDisclosure) {
                throw "Game Authoring disclosure telemetry disappeared for '$targetName'."
            }
            $disclosureX = [double]$latestDisclosure.Groups["x"].Value
            $disclosureY = [double]$latestDisclosure.Groups["y"].Value
            $disclosureWidth = [double]$latestDisclosure.Groups["width"].Value
            $expandOffset = Get-FileLengthSafe -Path $StdoutPath
            Click-FramebufferPoint `
                -Handle $Handle `
                -FramebufferWidth $FramebufferWidth `
                -FramebufferHeight $FramebufferHeight `
                -FramebufferX ($disclosureX + $disclosureWidth * $fraction) `
                -FramebufferY ($disclosureY + 14.0)
            if (Wait-FileContainsAfterOffset `
                    -Path $StdoutPath `
                    -Pattern ('^Game authoring physics disclosure: name=' + $escapedTargetName + ' .* expanded=1\.') `
                    -StartingOffset $expandOffset `
                    -TimeoutMilliseconds 2500) {
                $expanded = $true
                break
            }
        }
        if (-not $expanded) {
            throw "The product-native Game Authoring disclosure for '$targetName' did not expand."
        }
    }

    $playPattern = '^Game authoring play controls: name=' + $escapedTargetName + ' trigger_x=(?<triggerX>[-0-9.]+) play_x=(?<playX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26\.0 state=(?<state>[0-9]+)\.'
    $stepPattern = '^Game authoring step controls: name=' + $escapedTargetName + ' step_x=(?<stepX>[-0-9.]+) stop_x=(?<stopX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26\.0\.'
    $playMatch = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $playPattern
    $stepMatch = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $stepPattern
    for ($attempt = 0; $attempt -lt 12 -and ($null -eq $playMatch -or $null -eq $stepMatch); ++$attempt) {
        Scroll-FramebufferPointAndWaitForConsumption `
            -Handle $Handle `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -FramebufferX ($DetailsX + [Math]::Max(12.0, $DetailsWidth - 18.0)) `
            -FramebufferY ($DetailsY + [Math]::Max(30.0, $DetailsHeight * 0.55)) `
            -WheelDelta -1 `
            -TimeoutMilliseconds 3000
        $playMatch = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $playPattern
        $stepMatch = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $stepPattern
    }
    if ($null -eq $playMatch -or $null -eq $stepMatch) {
        throw "Scrolling Object Details did not expose both Game Authoring Play and Step controls for '$targetName'."
    }
    Assert-PackagedGeometryTelemetryStable `
        -Description "Game Authoring Play control for $targetName" `
        -Pattern $playPattern
    Assert-PackagedGeometryTelemetryStable `
        -Description "Game Authoring Step/Stop control for $targetName" `
        -Pattern $stepPattern
    Save-WindowScreenshot `
        -Handle $Handle `
        -Path $ScreenshotPath `
        -Description "Packaged product-native Game Authoring controls"

    if ($playMatch.Groups["state"].Value -ne "0") {
        throw "The product-native Game Authoring target did not begin in Stopped state."
    }
    foreach ($transition in @(
        @{ Name = "Start Play"; State = "1" },
        @{ Name = "Pause Play"; State = "2" },
        @{ Name = "Resume Play"; State = "1" },
        @{ Name = "Pause before Step"; State = "2" }
    )) {
        $playMatch = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $playPattern
        if ($null -eq $playMatch) {
            throw "Game Authoring Play telemetry disappeared before $($transition.Name)."
        }
        $playX = [double]$playMatch.Groups["playX"].Value
        $playY = [double]$playMatch.Groups["y"].Value
        $playWidth = [double]$playMatch.Groups["width"].Value
        $transitionOffset = Get-FileLengthSafe -Path $StdoutPath
        Click-FramebufferPoint `
            -Handle $Handle `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -FramebufferX ($playX + $playWidth * 0.5) `
            -FramebufferY ($playY + 13.0)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $StdoutPath `
                -Pattern 'Play session state changed\.' `
                -StartingOffset $transitionOffset `
                -TimeoutMilliseconds 5000)) {
            throw "Game Authoring $($transition.Name) produced no product state-transition result."
        }
        $playMatch = Wait-LastLogRegexMatch `
            -Path $StdoutPath `
            -Pattern $playPattern `
            -GroupName "state" `
            -ExpectedValue $transition.State `
            -TimeoutMilliseconds 5000
        if ($null -eq $playMatch) {
            throw "Game Authoring $($transition.Name) did not reach state $($transition.State)."
        }
        Write-Output "[pass] Product-native Game Authoring $($transition.Name) reached state $($transition.State)"
    }

    $stepMatch = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $stepPattern
    if ($null -eq $stepMatch) {
        throw "Game Authoring Step/Stop geometry disappeared before the fixed step."
    }
    $stepX = [double]$stepMatch.Groups["stepX"].Value
    $stepY = [double]$stepMatch.Groups["y"].Value
    $stepWidth = [double]$stepMatch.Groups["width"].Value
    $stepOffset = Get-FileLengthSafe -Path $StdoutPath
    Click-FramebufferPoint `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -FramebufferX ($stepX + $stepWidth * 0.5) `
        -FramebufferY ($stepY + 13.0)
    if (-not (Wait-FileContainsAfterOffset `
            -Path $StdoutPath `
            -Pattern 'Play fixed step complete\.' `
            -StartingOffset $stepOffset `
            -TimeoutMilliseconds 5000)) {
        throw "Product-native Game Authoring Step Play did not complete."
    }
    $stopOffset = Get-FileLengthSafe -Path $StdoutPath
    Click-FramebufferPoint `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -FramebufferX ([double]$stepMatch.Groups["stopX"].Value + $stepWidth * 0.5) `
        -FramebufferY ($stepY + 13.0)
    if (-not (Wait-FileContainsAfterOffset `
            -Path $StdoutPath `
            -Pattern 'Play stopped; authored state preserved\.' `
            -StartingOffset $stopOffset `
            -TimeoutMilliseconds 5000) -or
        -not (Wait-FileContainsAfterOffset `
            -Path $StdoutPath `
            -Pattern 'Game authoring play stopped: state=0\.' `
            -StartingOffset $stopOffset `
            -TimeoutMilliseconds 5000)) {
        throw "Product-native Game Authoring Stop did not preserve authored state and return to Stopped."
    }
    Write-Output "[pass] Product-native Game Authoring Step and Stop preserved the Add Cube target"
}

function Scroll-FramebufferPointAndWaitForConsumption {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$FramebufferX,
        [Parameter(Mandatory = $true)][double]$FramebufferY,
        [Parameter(Mandatory = $true)][int]$WheelDelta,
        [int]$TimeoutMilliseconds = 3000
    )

    $existingRecordCount = [System.IO.File]::ReadAllLines($automationInputPath).Length
    $expectedWheelRecord = [long]$existingRecordCount + 2L
    if ($expectedWheelRecord -gt 512L) {
        throw "Packaged UI scroll record $expectedWheelRecord exceeds the bounded application-diagnostic window."
    }
    $diagnosticOffset = Get-FileLengthSafe -Path $stdoutPath
    Scroll-FramebufferPoint `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -FramebufferX $FramebufferX `
        -FramebufferY $FramebufferY `
        -WheelDelta $WheelDelta

    $consumedPattern = 'HENKA_AUTOMATION_DIAGNOSTIC input record={0} type=wheel button=none release_consumed=[01]' -f $expectedWheelRecord
    if (-not (Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern $consumedPattern `
            -StartingOffset $diagnosticOffset `
            -TimeoutMilliseconds $TimeoutMilliseconds)) {
        throw "The packaged Sandbox did not consume the expected scroll record $expectedWheelRecord within ${TimeoutMilliseconds} ms."
    }
    Write-Output "[pass] Packaged Sandbox consumed scroll input record $expectedWheelRecord"
}

function Send-HenkaBackgroundFramebufferClick {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$FramebufferX,
        [Parameter(Mandatory = $true)][double]$FramebufferY
    )

    $clientRect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetClientRect($Handle, [ref]$clientRect)) {
        throw "The packaged Sandbox client bounds could not be read for background automation."
    }
    $windowWidth = $clientRect.Right - $clientRect.Left
    $windowHeight = $clientRect.Bottom - $clientRect.Top
    $windowPoint = Convert-HenkaFramebufferPointToWindowPoint `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -WindowWidth $windowWidth `
        -WindowHeight $windowHeight `
        -FramebufferX $FramebufferX `
        -FramebufferY $FramebufferY
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X $windowPoint.X `
        -Y $windowPoint.Y
}

function Send-HenkaBackgroundFramebufferClickAndWait {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$FramebufferX,
        [Parameter(Mandatory = $true)][double]$FramebufferY,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $recordCount = [System.IO.File]::ReadAllLines($automationInputPath).Length
    $firstRecord = [long]$recordCount + 1L
    $lastRecord = $firstRecord + 2L
    if ($lastRecord -gt 512L) {
        throw "$Description input record $lastRecord exceeds the bounded product diagnostic window."
    }
    $diagnosticOffset = Get-FileLengthSafe -Path $StdoutPath
    Send-HenkaBackgroundFramebufferClick `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -FramebufferX $FramebufferX `
        -FramebufferY $FramebufferY

    $expectedEvents = @(
        @{ Record = $firstRecord; Type = "move"; Button = "none" },
        @{ Record = ($firstRecord + 1L); Type = "button-down"; Button = "left" },
        @{ Record = $lastRecord; Type = "button-up"; Button = "left" }
    )
    foreach ($expectedEvent in $expectedEvents) {
        $releaseConsumed = if ($expectedEvent.Type -eq "button-up") { "1" } else { "[01]" }
        $pattern = "HENKA_AUTOMATION_DIAGNOSTIC input record={0} type={1} button={2} release_consumed={3}" -f `
            $expectedEvent.Record, $expectedEvent.Type, $expectedEvent.Button, $releaseConsumed
        if (-not (Wait-FileContainsAfterOffset `
                -Path $StdoutPath `
                -Pattern $pattern `
                -StartingOffset $diagnosticOffset `
                -TimeoutMilliseconds 5000)) {
            throw "$Description did not consume its real pointer event record $($expectedEvent.Record)."
        }
    }
}

function Send-HenkaBackgroundFramebufferDrag {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$StartFramebufferX,
        [Parameter(Mandatory = $true)][double]$StartFramebufferY,
        [Parameter(Mandatory = $true)][double]$EndFramebufferX,
        [Parameter(Mandatory = $true)][double]$EndFramebufferY,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $clientRect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetClientRect($Handle, [ref]$clientRect)) {
        throw "$Description could not read the packaged Sandbox client bounds."
    }
    $windowWidth = $clientRect.Right - $clientRect.Left
    $windowHeight = $clientRect.Bottom - $clientRect.Top
    if ($windowWidth -le 0 -or $windowHeight -le 0) {
        throw "$Description found invalid packaged Sandbox client dimensions."
    }
    $startPoint = Convert-HenkaFramebufferPointToWindowPoint `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -WindowWidth $windowWidth `
        -WindowHeight $windowHeight `
        -FramebufferX $StartFramebufferX `
        -FramebufferY $StartFramebufferY
    $endPoint = Convert-HenkaFramebufferPointToWindowPoint `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -WindowWidth $windowWidth `
        -WindowHeight $windowHeight `
        -FramebufferX $EndFramebufferX `
        -FramebufferY $EndFramebufferY

    $recordCount = [System.IO.File]::ReadAllLines($automationInputPath).Length
    $firstRecord = [long]$recordCount + 1L
    $lastRecord = $firstRecord + 3L
    if ($lastRecord -gt 512L) {
        throw "$Description input record $lastRecord exceeds the bounded product diagnostic window."
    }
    $diagnosticOffset = Get-FileLengthSafe -Path $StdoutPath
    $startX = Format-HenkaAutomationFloat -Value $startPoint.X
    $startY = Format-HenkaAutomationFloat -Value $startPoint.Y
    $endX = Format-HenkaAutomationFloat -Value $endPoint.X
    $endY = Format-HenkaAutomationFloat -Value $endPoint.Y
    Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine "move $startX $startY" -SettleMilliseconds 0
    Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine "button left down $startX $startY" -SettleMilliseconds 0
    Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine "move $endX $endY" -SettleMilliseconds 0
    Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine "button left up $endX $endY" -SettleMilliseconds 0

    $expectedEvents = @(
        @{ Record = $firstRecord; Type = "move"; Button = "none" },
        @{ Record = ($firstRecord + 1L); Type = "button-down"; Button = "left" },
        @{ Record = ($firstRecord + 2L); Type = "move"; Button = "none" },
        @{ Record = $lastRecord; Type = "button-up"; Button = "left" }
    )
    foreach ($expectedEvent in $expectedEvents) {
        $releaseConsumed = if ($expectedEvent.Type -eq "button-up") { "1" } else { "[01]" }
        $pattern = "HENKA_AUTOMATION_DIAGNOSTIC input record={0} type={1} button={2} release_consumed={3}" -f `
            $expectedEvent.Record, $expectedEvent.Type, $expectedEvent.Button, $releaseConsumed
        if (-not (Wait-FileContainsAfterOffset `
                -Path $StdoutPath `
                -Pattern $pattern `
                -StartingOffset $diagnosticOffset `
                -TimeoutMilliseconds 5000)) {
            throw "$Description did not consume its real pointer event record $($expectedEvent.Record)."
        }
    }
}

function Get-HenkaWorkspaceTopologyReport {
    param(
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [switch]$RequireToolsVisible,
        [switch]$RequireDivider
    )

    $number = '[-0-9.]+'
    $pattern = '^HENKA_AUTOMATION_DIAGNOSTIC workspace_topology ' +
        'cause=(?<cause>[a-z_]+) tools_visible=(?<toolsVisible>[01]) ' +
        'divider_count=(?<dividerCount>[0-9]+) ' +
        'left_dock=(?<dockX>' + $number + '),(?<dockY>' + $number + '),(?<dockWidth>' + $number + '),(?<dockHeight>' + $number + ') ' +
        'scene_objects=(?<sceneX>' + $number + '),(?<sceneY>' + $number + '),(?<sceneWidth>' + $number + '),(?<sceneHeight>' + $number + ') ' +
        'controls=(?<controlsX>' + $number + '),(?<controlsY>' + $number + '),(?<controlsWidth>' + $number + '),(?<controlsHeight>' + $number + ') ' +
        'divider0=(?<dividerX>' + $number + '),(?<dividerY>' + $number + '),(?<dividerWidth>' + $number + '),(?<dividerHeight>' + $number + ')\.'
    $match = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $pattern
    if ($null -eq $match) {
        throw "The product did not report its canonical left-dock topology and divider hit rectangle."
    }

    $groups = $match.Groups
    $report = [PSCustomObject]@{
        Cause = $groups['cause'].Value
        ToolsVisible = [int]$groups['toolsVisible'].Value
        DividerCount = [int]$groups['dividerCount'].Value
        Dock = [PSCustomObject]@{
            X = [double]$groups['dockX'].Value
            Y = [double]$groups['dockY'].Value
            Width = [double]$groups['dockWidth'].Value
            Height = [double]$groups['dockHeight'].Value
        }
        SceneObjects = [PSCustomObject]@{
            X = [double]$groups['sceneX'].Value
            Y = [double]$groups['sceneY'].Value
            Width = [double]$groups['sceneWidth'].Value
            Height = [double]$groups['sceneHeight'].Value
        }
        Controls = [PSCustomObject]@{
            X = [double]$groups['controlsX'].Value
            Y = [double]$groups['controlsY'].Value
            Width = [double]$groups['controlsWidth'].Value
            Height = [double]$groups['controlsHeight'].Value
        }
        DividerHit = [PSCustomObject]@{
            X = [double]$groups['dividerX'].Value
            Y = [double]$groups['dividerY'].Value
            Width = [double]$groups['dividerWidth'].Value
            Height = [double]$groups['dividerHeight'].Value
        }
    }

    $null = Assert-FramebufferRect `
        -Name "Product-reported active workspace dock" `
        -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
        -X $report.Dock.X -Y $report.Dock.Y -Width $report.Dock.Width -Height $report.Dock.Height
    if ($RequireToolsVisible -and
        ($report.ToolsVisible -ne 1 -or
         $report.Controls.Width -le 0.0 -or $report.Controls.Height -le 0.0 -or
         $report.SceneObjects.Width -le 0.0 -or $report.SceneObjects.Height -le 0.0)) {
        throw "The product topology report does not show both Controls and Scene Objects in the active dock."
    }
    if ($RequireDivider -and $report.DividerCount -lt 1) {
        throw "Tools/Controls is visible, but the product layout reports no active Scene Objects/Controls topology divider."
    }
    if ($report.DividerCount -gt 0) {
        $null = Assert-FramebufferRect `
            -Name "Product-owned topology divider hit rectangle" `
            -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
            -X $report.DividerHit.X -Y $report.DividerHit.Y `
            -Width $report.DividerHit.Width -Height $report.DividerHit.Height
        if ($report.DividerHit.X -lt $report.Dock.X -or
            $report.DividerHit.Y -lt $report.Dock.Y -or
            $report.DividerHit.X + $report.DividerHit.Width -gt $report.Dock.X + $report.Dock.Width -or
            $report.DividerHit.Y + $report.DividerHit.Height -gt $report.Dock.Y + $report.Dock.Height) {
            throw "The product-owned topology divider hit rectangle lies outside the active dock."
        }
    }

    return $report
}

function Set-HenkaSceneObjectsPanelHeightThroughUi {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][PSCustomObject]$Topology,
        [Parameter(Mandatory = $true)][double]$TargetSceneObjectsBottomY,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$Description
    )

    if ($FramebufferWidth -ne 1280 -or $FramebufferHeight -ne 720) {
        throw "$Description was outside the supported packaged layout boundary."
    }
    if ($Topology.ToolsVisible -ne 1 -or $Topology.DividerCount -lt 1) {
        throw "$Description requires the product-reported visible Tools section and active topology divider."
    }

    $dividerX = $Topology.DividerHit.X + ($Topology.DividerHit.Width * 0.5)
    $dividerY = $Topology.DividerHit.Y + ($Topology.DividerHit.Height * 0.5)
    $sceneObjectsBottomY = $Topology.SceneObjects.Y + $Topology.SceneObjects.Height
    $dividerCenterToPanelEdgeOffset = $dividerY - $sceneObjectsBottomY
    if ([Math]::Abs($dividerCenterToPanelEdgeOffset) -gt
        ($Topology.DividerHit.Height * 0.5 + 0.01)) {
        throw "$Description could not reconcile the product-reported divider center with the Scene Objects panel edge."
    }
    $targetDividerY = $TargetSceneObjectsBottomY + $dividerCenterToPanelEdgeOffset
    if ($targetDividerY -lt $Topology.Dock.Y -or
        $targetDividerY -ge $Topology.Dock.Y + $Topology.Dock.Height) {
        throw "$Description target is outside the product-reported active dock."
    }
    Write-Output ("[pass] Derived the live divider endpoint from the product panel edge and center offset ({0:F1}px)" -f $dividerCenterToPanelEdgeOffset)
    Send-HenkaBackgroundFramebufferDrag `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -StartFramebufferX $dividerX `
        -StartFramebufferY $dividerY `
        -EndFramebufferX $dividerX `
        -EndFramebufferY $targetDividerY `
        -StdoutPath $StdoutPath `
        -Description $Description
}

function Send-HenkaBackgroundFramebufferScroll {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$FramebufferX,
        [Parameter(Mandatory = $true)][double]$FramebufferY,
        [Parameter(Mandatory = $true)][double]$WheelDelta
    )

    $clientRect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetClientRect($Handle, [ref]$clientRect)) {
        throw "The packaged Sandbox client bounds could not be read for background scrolling."
    }
    $windowWidth = $clientRect.Right - $clientRect.Left
    $windowHeight = $clientRect.Bottom - $clientRect.Top
    $windowPoint = Convert-HenkaFramebufferPointToWindowPoint `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -WindowWidth $windowWidth `
        -WindowHeight $windowHeight `
        -FramebufferX $FramebufferX `
        -FramebufferY $FramebufferY

    $recordCount = [System.IO.File]::ReadAllLines($automationInputPath).Length
    $expectedWheelRecord = [long]$recordCount + 2L
    if ($expectedWheelRecord -gt 512L) {
        throw "Packaged hidden-row scroll record $expectedWheelRecord exceeds the bounded diagnostic window."
    }
    $diagnosticOffset = Get-FileLengthSafe -Path $stdoutPath
    Send-HenkaAutomationScroll `
        -EventPath $automationInputPath `
        -X $windowPoint.X `
        -Y $windowPoint.Y `
        -WheelDelta $WheelDelta
    $consumedPattern = 'HENKA_AUTOMATION_DIAGNOSTIC input record={0} type=wheel button=none release_consumed=[01]' -f $expectedWheelRecord
    if (-not (Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern $consumedPattern `
            -StartingOffset $diagnosticOffset `
            -TimeoutMilliseconds 3000)) {
        throw "The packaged Sandbox did not consume background scroll record $expectedWheelRecord."
    }
}

function Assert-HenkaPackagedHiddenNamedObjectSelectable {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$AlternateName,
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$FramebufferWidth,
        [Parameter(Mandatory = $true)][int]$FramebufferHeight,
        [Parameter(Mandatory = $true)][double]$SceneObjectsX,
        [Parameter(Mandatory = $true)][double]$SceneObjectsY,
        [Parameter(Mandatory = $true)][double]$SceneObjectsWidth,
        [Parameter(Mandatory = $true)][double]$SceneObjectsHeight,
        [Parameter(Mandatory = $true)][double]$DetailsX,
        [Parameter(Mandatory = $true)][double]$DetailsY,
        [Parameter(Mandatory = $true)][double]$DetailsWidth,
        [Parameter(Mandatory = $true)][double]$DetailsHeight,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [switch]$RestorePanelAfterSelection
    )

    $topology = Get-HenkaWorkspaceTopologyReport `
        -StdoutPath $StdoutPath `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -RequireToolsVisible -RequireDivider
    $SceneObjectsX = $topology.SceneObjects.X
    $SceneObjectsY = $topology.SceneObjects.Y
    $SceneObjectsWidth = $topology.SceneObjects.Width
    $SceneObjectsHeight = $topology.SceneObjects.Height
    $originalSceneObjectsHeight = $SceneObjectsHeight

    $escapedName = [Regex]::Escape($Name)
    $rowPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row entity=(?<entity>\d+) name=' +
        $escapedName + ' display=(?<display>[^\r\n]*) lines=(?<lines>\d+) hidden=(?<hidden>[01]) selected=(?<selected>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+)\.'
    $row = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $rowPattern
    if ($null -eq $row) {
        throw "The packaged Scene Objects UI did not expose the '$Name' row."
    }
    $entity = $row.Groups['entity'].Value
    if ($row.Groups['hidden'].Value -ne '0') {
        throw "The isolated packaged startup did not provide '$Name' in its expected visible baseline state."
    }
    if ($AlternateName -eq $Name) {
        throw "The hidden-row selection control requires a distinct alternate object."
    }
    if ($row.Groups['hidden'].Value -ne '1') {
        if ($row.Groups['selected'].Value -ne '1') {
            $rowX = [double]$row.Groups['x'].Value
            $rowY = [double]$row.Groups['y'].Value
            $rowWidth = [double]$row.Groups['width'].Value
            $rowHeight = [double]$row.Groups['height'].Value
            Assert-FramebufferRect `
                -Name "$Name Scene Objects row before hiding" `
                -FramebufferWidth $FramebufferWidth `
                -FramebufferHeight $FramebufferHeight `
                -X $rowX -Y $rowY -Width $rowWidth -Height $rowHeight
            if ($rowX -lt $SceneObjectsX -or $rowY -lt $SceneObjectsY -or
                $rowX + $rowWidth -gt $SceneObjectsX + $SceneObjectsWidth -or
                $rowY + $rowHeight -gt $SceneObjectsY + $SceneObjectsHeight) {
                throw "$Name row hit geometry was outside the visible Scene Objects panel."
            }
            $selectionOffset = Get-FileLengthSafe -Path $StdoutPath
            Send-HenkaBackgroundFramebufferClick `
                -Handle $Handle -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
                -FramebufferX ($rowX + $rowWidth * 0.5) -FramebufferY ($rowY + $rowHeight * 0.5)
            $selectionPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_click entity=' +
                $entity + ' name=' + $escapedName + ' hidden=0 selected_entity=' + $entity + ' selected=1\.'
            if (-not (Wait-FileContainsAfterOffset -Path $StdoutPath -Pattern $selectionPattern `
                    -StartingOffset $selectionOffset -TimeoutMilliseconds 5000)) {
                throw "Clicking the visible '$Name' row did not select its canonical entity."
            }
        }

        $actionsPattern = '^HENKA_AUTOMATION_DIAGNOSTIC object_details_disclosure id=actions entity=' +
            $entity + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28\.0 expanded=(?<expanded>[01])\.'
        $actions = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $actionsPattern
        for ($attempt = 0; $attempt -lt 14 -and $null -eq $actions; ++$attempt) {
            Send-HenkaBackgroundFramebufferScroll `
                -Handle $Handle -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
                -FramebufferX ($DetailsX + [Math]::Max(12.0, $DetailsWidth - 18.0)) `
                -FramebufferY ($DetailsY + [Math]::Max(30.0, $DetailsHeight * 0.55)) `
                -WheelDelta -1.0
            $actions = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $actionsPattern
        }
        if ($null -eq $actions) {
            throw "Object Details > Actions for '$Name' did not become visible after bounded, consumed scrolling."
        }
        if ($actions.Groups['expanded'].Value -ne '1') {
            $actionsX = [double]$actions.Groups['x'].Value
            $actionsY = [double]$actions.Groups['y'].Value
            $actionsWidth = [double]$actions.Groups['width'].Value
            Assert-FramebufferRect `
                -Name "$Name Object Details Actions disclosure" `
                -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
                -X $actionsX -Y $actionsY -Width $actionsWidth -Height 28.0
            $expandOffset = Get-FileLengthSafe -Path $StdoutPath
            Send-HenkaBackgroundFramebufferClick `
                -Handle $Handle -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
                -FramebufferX ($actionsX + $actionsWidth * 0.5) -FramebufferY ($actionsY + 14.0)
            $expandedPattern = '^HENKA_AUTOMATION_DIAGNOSTIC object_details_disclosure id=actions entity=' +
                $entity + ' .* expanded=1\.'
            if (-not (Wait-FileContainsAfterOffset -Path $StdoutPath -Pattern $expandedPattern `
                    -StartingOffset $expandOffset -TimeoutMilliseconds 5000)) {
                throw "Object Details > Actions for '$Name' did not expand after the real UI click."
            }
        }

        $visibilityPattern = '^HENKA_AUTOMATION_DIAGNOSTIC object_visibility_control entity=' +
            $entity + ' name=' + $escapedName + ' label=Hide Object visible=1 x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+)\.'
        $visibility = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $visibilityPattern
        for ($attempt = 0; $attempt -lt 8 -and $null -eq $visibility; ++$attempt) {
            Send-HenkaBackgroundFramebufferScroll `
                -Handle $Handle -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
                -FramebufferX ($DetailsX + [Math]::Max(12.0, $DetailsWidth - 18.0)) `
                -FramebufferY ($DetailsY + [Math]::Max(30.0, $DetailsHeight * 0.55)) `
                -WheelDelta -1.0
            $visibility = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $visibilityPattern
        }
        if ($null -eq $visibility) {
            throw "The visible Hide Object control for '$Name' did not appear after bounded scrolling."
        }
        $visibilityX = [double]$visibility.Groups['x'].Value
        $visibilityY = [double]$visibility.Groups['y'].Value
        $visibilityWidth = [double]$visibility.Groups['width'].Value
        $visibilityHeight = [double]$visibility.Groups['height'].Value
        Assert-FramebufferRect `
            -Name "$Name Hide Object control" `
            -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
            -X $visibilityX -Y $visibilityY -Width $visibilityWidth -Height $visibilityHeight
        $hideOffset = Get-FileLengthSafe -Path $StdoutPath
        Send-HenkaBackgroundFramebufferClick `
            -Handle $Handle -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
            -FramebufferX ($visibilityX + $visibilityWidth * 0.5) `
            -FramebufferY ($visibilityY + $visibilityHeight * 0.5)
        $hiddenPattern = '^HENKA_AUTOMATION_DIAGNOSTIC object_visibility_changed entity=' +
            $entity + ' name=' + $escapedName + ' visible=0\.'
        if (-not (Wait-FileContainsAfterOffset -Path $StdoutPath -Pattern $hiddenPattern `
                -StartingOffset $hideOffset -TimeoutMilliseconds 5000)) {
            throw "The product Hide Object control did not hide '$Name'."
        }
        Write-Output "[pass] Packaged Object Details hid the canonical '$Name' object"
    }

    $row = Wait-LastLogRegexMatch -Path $StdoutPath -Pattern $rowPattern `
        -GroupName 'hidden' -ExpectedValue '1' -TimeoutMilliseconds 5000
    if ($null -eq $row -or $row.Groups['entity'].Value -ne $entity -or
        $row.Groups['hidden'].Value -ne '1') {
        throw "The product Hide Object action did not preserve '$Name' as the same hidden canonical entity."
    }
    $canonicalDisplay = $row.Groups['display'].Value
    if ($canonicalDisplay -notmatch [Regex]::Escape($Name) -or $canonicalDisplay -notmatch 'Hidden') {
        throw "The hidden '$Name' row lost its canonical identity or explicit Hidden marker: '$canonicalDisplay'."
    }

    $alternateEscaped = [Regex]::Escape($AlternateName)
    $alternatePattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row entity=(?<entity>\d+) name=' +
        $alternateEscaped + ' display=(?<display>[^\r\n]*) lines=(?<lines>\d+) hidden=(?<hidden>[01]) selected=(?<selected>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+)\.'
    $alternate = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $alternatePattern
    if ($null -eq $alternate -or $alternate.Groups['selected'].Value -ne '0') {
        throw "A distinct, currently unselected '$AlternateName' row was not observable before the selection-transition control."
    }
    $alternateEntity = $alternate.Groups['entity'].Value
    if ($alternateEntity -eq $entity) {
        throw "The alternate row resolved to the same canonical entity as '$Name'."
    }
    $alternateX = [double]$alternate.Groups['x'].Value
    $alternateY = [double]$alternate.Groups['y'].Value
    $alternateWidth = [double]$alternate.Groups['width'].Value
    $alternateHeight = [double]$alternate.Groups['height'].Value
    if ($alternateX -lt $SceneObjectsX -or $alternateY -lt $SceneObjectsY -or
        $alternateX + $alternateWidth -gt $SceneObjectsX + $SceneObjectsWidth -or
        $alternateY + $alternateHeight -gt $SceneObjectsY + $SceneObjectsHeight) {
        throw "The '$AlternateName' deselection control was outside the fully visible Scene Objects panel."
    }
    $alternateClickOffset = Get-FileLengthSafe -Path $StdoutPath
    Send-HenkaBackgroundFramebufferClickAndWait `
        -Handle $Handle -FramebufferWidth $FramebufferWidth -FramebufferHeight $FramebufferHeight `
        -FramebufferX ($alternateX + $alternateWidth * 0.5) `
        -FramebufferY ($alternateY + $alternateHeight * 0.5) `
        -StdoutPath $StdoutPath `
        -Description "Selecting alternate '$AlternateName' before hidden-row click"
    $alternateClickPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_click entity=' +
        $alternateEntity + ' name=' + $alternateEscaped + ' hidden=[01] selected_entity=' +
        $alternateEntity + ' selected=1\.'
    if (-not (Wait-FileContainsAfterOffset -Path $StdoutPath -Pattern $alternateClickPattern `
            -StartingOffset $alternateClickOffset -TimeoutMilliseconds 5000)) {
        throw "The real Scene Objects click did not move selection away from hidden '$Name' to '$AlternateName'."
    }

    $rowStartY = [double]$row.Groups['y'].Value
    $preCompactRowHeight = [double]$row.Groups['height'].Value
    $minimumOneLineRowHeight = 28.0
    $targetOneLineRowCapacity = $minimumOneLineRowHeight + 2.0
    $panelFooterHeight = 34.0
    $preCompactAvailableRowHeight =
        $SceneObjectsY + $SceneObjectsHeight - $panelFooterHeight - $rowStartY
    if ([int]$row.Groups['lines'].Value -ne 2 -or
        $preCompactRowHeight -le $minimumOneLineRowHeight -or
        $preCompactAvailableRowHeight -lt $preCompactRowHeight) {
        throw "The normal Scene Objects panel did not expose the expected fully visible two-line hidden-row baseline."
    }
    # Derive the splitter target from product-reported row geometry. A fixed
    # panel height can place the list below its footer because authoring
    # controls occupy the upper portion of this panel. Keep a small margin
    # above the existing minimum row height: the prior exact-minimum target
    # rounded to 27.96 px and correctly caused layout rejection, rather than
    # producing a row that could classify the formatter/draw boundary.
    $compactPanelHeight =
        ($rowStartY - $SceneObjectsY) + $panelFooterHeight + $targetOneLineRowCapacity
    $compactAvailableRowHeight =
        $SceneObjectsY + $compactPanelHeight - $panelFooterHeight - $rowStartY
    if ($compactPanelHeight -lt 180.0 -or
        $compactPanelHeight -ge $SceneObjectsHeight -or
        $compactAvailableRowHeight -lt ($minimumOneLineRowHeight + 1.0) -or
        $compactAvailableRowHeight -ge $preCompactRowHeight) {
        throw "The product-reported row position cannot establish a safe one-line Scene Objects capacity."
    }
    Write-Output (("[pass] Derived compact Scene Objects height {0:F1}px from the product row; " +
        "one-line row capacity budget is {1:F1}px (minimum {2:F1}px; baseline row {3:F1}px)") -f `
        $compactPanelHeight, $compactAvailableRowHeight, $minimumOneLineRowHeight, $preCompactRowHeight)
    $compactLayoutOffset = Get-FileLengthSafe -Path $StdoutPath
    $reachabilityScreenshotPath = Join-Path (Split-Path -Parent $StdoutPath) `
        "scene-objects-$($Name.ToLowerInvariant())-divider-reachability-1280x720.bmp"
    Set-HenkaSceneObjectsPanelHeightThroughUi `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -Topology $topology `
        -TargetSceneObjectsBottomY ($SceneObjectsY + $compactPanelHeight) `
        -StdoutPath $StdoutPath `
        -Description "Resizing Scene Objects to the supported one-line row boundary"

    $dividerReleasePattern = '^HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=divider_release '
    if (-not (Wait-FileContainsAfterOffset `
            -Path $StdoutPath `
            -Pattern $dividerReleasePattern `
            -StartingOffset $compactLayoutOffset `
            -TimeoutMilliseconds 5000)) {
        throw "The product did not report the live left-dock geometry after the divider release."
    }
    $resizedTopology = Get-HenkaWorkspaceTopologyReport `
        -StdoutPath $StdoutPath `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -RequireToolsVisible
    $geometryChanged =
        [Math]::Abs($resizedTopology.SceneObjects.X - $topology.SceneObjects.X) -gt 0.5 -or
        [Math]::Abs($resizedTopology.SceneObjects.Y - $topology.SceneObjects.Y) -gt 0.5 -or
        [Math]::Abs($resizedTopology.SceneObjects.Width - $topology.SceneObjects.Width) -gt 0.5 -or
        [Math]::Abs($resizedTopology.SceneObjects.Height - $topology.SceneObjects.Height) -gt 0.5
    if (-not $geometryChanged) {
        throw "The real divider drag events were consumed, but product-reported Scene Objects geometry did not change."
    }
    $SceneObjectsX = $resizedTopology.SceneObjects.X
    $SceneObjectsY = $resizedTopology.SceneObjects.Y
    $SceneObjectsWidth = $resizedTopology.SceneObjects.Width
    $SceneObjectsHeight = $resizedTopology.SceneObjects.Height
    $topology = $resizedTopology
    Write-Output ("[pass] The real product topology divider changed Scene Objects geometry to {0:F1}px high" -f $SceneObjectsHeight)

    $compactLayoutPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_layout result=(?:ready|error) .+\.$'
    if (-not (Wait-FileContainsAfterOffset `
            -Path $StdoutPath `
            -Pattern $compactLayoutPattern `
            -StartingOffset $compactLayoutOffset `
            -TimeoutMilliseconds 5000)) {
        throw "The product did not report the actual Scene Objects row-page calculation after the divider release."
    }
    $compactLayoutReport = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $compactLayoutPattern
    if ($null -eq $compactLayoutReport) {
        throw "The current Scene Objects row-page calculation could not be read from product telemetry."
    }

    # Generic frame heartbeats are deliberately capped for bounded logs. Wait
    # for the row-authority observation emitted by the actual Scene Objects
    # draw path instead of requiring a later periodic frame sample.
    if (-not (Wait-HenkaSceneObjectsRowDrawAfterOffset `
            -Path $StdoutPath `
            -StartingOffset $compactLayoutOffset `
            -TimeoutMilliseconds 5000)) {
        throw "The Sandbox did not execute the Scene Objects row draw path after the compact layout changed."
    }
    $compactRowAuthority = Find-HenkaSceneObjectsRowAuthorityThroughPager `
        -Name $Name `
        -Entity $entity `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -SceneObjects $topology.SceneObjects `
        -StdoutPath $StdoutPath

    # Divider movement can reflow the row list after the initial target was
    # calculated. If the real post-resize row remains two-line, perform one
    # bounded correction from that fresh production row geometry rather than
    # inferring reachability from total dock height or repeating blind drags.
    $correctedPanelHeight = $null
    if ($null -ne $compactRowAuthority) {
        $correctedPanelHeight = Get-HenkaSceneObjectsOneLinePanelHeightFromRow `
            -Row $compactRowAuthority `
            -PanelY $SceneObjectsY `
            -CurrentPanelHeight $SceneObjectsHeight `
            -MinimumRowHeight $minimumOneLineRowHeight `
            -DesiredRowCapacity $targetOneLineRowCapacity `
            -FooterHeight $panelFooterHeight `
            -MinimumPanelHeight 180.0
    }
    if ($null -ne $correctedPanelHeight) {
        $correctedLayoutOffset = Get-FileLengthSafe -Path $StdoutPath
        Set-HenkaSceneObjectsPanelHeightThroughUi `
            -Handle $Handle `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -Topology $topology `
            -TargetSceneObjectsBottomY ($SceneObjectsY + $correctedPanelHeight) `
            -StdoutPath $StdoutPath `
            -Description "Correcting the compact divider target from the fresh product row position"

        if (-not (Wait-FileContainsAfterOffset `
                -Path $StdoutPath `
                -Pattern $dividerReleasePattern `
                -StartingOffset $correctedLayoutOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The product did not report the bounded row-derived divider correction."
        }
        $correctedTopology = Get-HenkaWorkspaceTopologyReport `
            -StdoutPath $StdoutPath `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -RequireToolsVisible
        $correctedGeometryChanged =
            [Math]::Abs($correctedTopology.SceneObjects.X - $topology.SceneObjects.X) -gt 0.5 -or
            [Math]::Abs($correctedTopology.SceneObjects.Y - $topology.SceneObjects.Y) -gt 0.5 -or
            [Math]::Abs($correctedTopology.SceneObjects.Width - $topology.SceneObjects.Width) -gt 0.5 -or
            [Math]::Abs($correctedTopology.SceneObjects.Height - $topology.SceneObjects.Height) -gt 0.5
        if (-not $correctedGeometryChanged) {
            throw "The row-derived divider correction was consumed, but product Scene Objects geometry did not change."
        }

        $SceneObjectsX = $correctedTopology.SceneObjects.X
        $SceneObjectsY = $correctedTopology.SceneObjects.Y
        $SceneObjectsWidth = $correctedTopology.SceneObjects.Width
        $SceneObjectsHeight = $correctedTopology.SceneObjects.Height
        $topology = $correctedTopology
        $resizedTopology = $correctedTopology
        if (-not (Wait-FileContainsAfterOffset `
                -Path $StdoutPath `
                -Pattern $compactLayoutPattern `
                -StartingOffset $correctedLayoutOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The product did not report the Scene Objects layout after the row-derived correction."
        }
        if (-not (Wait-HenkaSceneObjectsRowDrawAfterOffset `
                -Path $StdoutPath `
                -StartingOffset $correctedLayoutOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The Scene Objects row draw path did not run after the row-derived correction."
        }
        $compactRowAuthority = Find-HenkaSceneObjectsRowAuthorityThroughPager `
            -Name $Name `
            -Entity $entity `
            -Handle $Handle `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -SceneObjects $topology.SceneObjects `
            -StdoutPath $StdoutPath
        $correctedRowY = if ($null -ne $compactRowAuthority) {
            [double]$compactRowAuthority.Groups['y'].Value
        }
        else {
            [double]::NaN
        }
        Write-Output ("[pass] One bounded row-derived divider correction reached {0:F1}px; fresh product row y={1}" -f `
            $SceneObjectsHeight, $correctedRowY)
    }

    $compactLayoutReport = Get-LastLogRegexMatch -Path $StdoutPath -Pattern $compactLayoutPattern
    Save-WindowScreenshot `
        -Handle $Handle `
        -Path $reachabilityScreenshotPath `
        -Description "Packaged Scene Objects after real divider and pager interactions"

    $compactRowVisible = $null -ne $compactRowAuthority -and
        [int]$compactRowAuthority.Groups['lines'].Value -eq 1 -and
        $compactRowAuthority.Groups['hidden'].Value -eq '1' -and
        $compactRowAuthority.Groups['selected'].Value -eq '0' -and
        $compactRowAuthority.Groups['display'].Value -ceq ($Name + ' [Hidden]') -and
        $compactRowAuthority.Groups['sameRect'].Value -eq '1'
    if (-not $compactRowVisible) {
        if ($null -ne $compactLayoutReport) {
            Write-Output ("[diagnostic] Actual post-divider Scene Objects layout: {0}" -f $compactLayoutReport.Value)
        }
        if ($null -ne $compactRowAuthority) {
            Write-Output ("[diagnostic] Actual $Name row/draw/hitbox authority: {0}" -f $compactRowAuthority.Value)
        }
        if ($topology.DividerCount -gt 0) {
            $restoreUnreachableOffset = Get-FileLengthSafe -Path $StdoutPath
            Set-HenkaSceneObjectsPanelHeightThroughUi `
                -Handle $Handle `
                -FramebufferWidth $FramebufferWidth `
                -FramebufferHeight $FramebufferHeight `
                -Topology $topology `
                -TargetSceneObjectsBottomY ($SceneObjectsY + $originalSceneObjectsHeight) `
                -StdoutPath $StdoutPath `
                -Description "Restoring the Scene Objects dock after classifying one-line reachability"
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $StdoutPath `
                    -Pattern $dividerReleasePattern `
                    -StartingOffset $restoreUnreachableOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The product did not report Scene Objects geometry after restoring the ordinary dock."
            }
            $restoredTopology = Get-HenkaWorkspaceTopologyReport `
                -StdoutPath $StdoutPath `
                -FramebufferWidth $FramebufferWidth `
                -FramebufferHeight $FramebufferHeight `
                -RequireToolsVisible
            if ([Math]::Abs($restoredTopology.SceneObjects.Height - $originalSceneObjectsHeight) -gt 1.0) {
                throw "The normal Scene Objects height was not restored after the one-line reachability classification."
            }
        }
        $script:hiddenRowOneLineReachable = $false
        Write-Output ("[classified-open] The normal divider interaction reached a {0:F1}px Scene Objects panel, and real pager navigation was attempted where available, but the product did not report '$Name [Hidden]' in a one-line row; no synthetic layout state was introduced." -f $resizedTopology.SceneObjects.Height)
        return
    }

    $row = $compactRowAuthority
    if ($null -eq $row -or $row.Groups['entity'].Value -ne $entity -or
        $row.Groups['hidden'].Value -ne '1' -or $row.Groups['selected'].Value -ne '0' -or
        [int]$row.Groups['lines'].Value -ne 1 -or
        $row.Groups['display'].Value -cne ($Name + ' [Hidden]')) {
        throw "The fresh compact product row report did not prove the exact one-line '$Name [Hidden]' state before clicking."
    }

    $rowX = [double]$row.Groups['x'].Value
    $rowY = [double]$row.Groups['y'].Value
    $rowWidth = [double]$row.Groups['width'].Value
    $rowHeight = [double]$row.Groups['height'].Value
    Assert-FramebufferRect `
        -Name "$Name one-line Scene Objects row" `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -X $rowX -Y $rowY -Width $rowWidth -Height $rowHeight
    if ($rowX -lt $SceneObjectsX -or $rowY -lt $SceneObjectsY -or
        $rowX + $rowWidth -gt $SceneObjectsX + $SceneObjectsWidth -or
        $rowY + $rowHeight -gt $SceneObjectsY + $SceneObjectsHeight) {
        throw "Hidden '$Name' one-line row hit geometry escaped the compact visible Scene Objects panel."
    }

    $selectionOffset = Get-FileLengthSafe -Path $StdoutPath
    Send-HenkaBackgroundFramebufferClickAndWait `
        -Handle $Handle `
        -FramebufferWidth $FramebufferWidth `
        -FramebufferHeight $FramebufferHeight `
        -FramebufferX ($rowX + $rowWidth * 0.5) `
        -FramebufferY ($rowY + $rowHeight * 0.5) `
        -StdoutPath $StdoutPath `
        -Description "Selecting hidden '$Name' through its reported Scene Objects hitbox"
    $selectedPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_click entity=' +
        $entity + ' name=' + $escapedName + ' hidden=1 selected_entity=' + $entity + ' selected=1\.'
    if (-not (Wait-FileContainsAfterOffset -Path $StdoutPath -Pattern $selectedPattern `
            -StartingOffset $selectionOffset -TimeoutMilliseconds 5000)) {
        throw "The real compact-row click did not change authoritative selection to hidden '$Name'."
    }
    $selectedRowPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row entity=' +
        [Regex]::Escape($entity) + ' name=' + $escapedName + ' display=' +
        [Regex]::Escape($Name + ' [Hidden]') + ' lines=1 hidden=1 selected=1 x=[-0-9.]+ y=[-0-9.]+ width=[-0-9.]+ height=[-0-9.]+\.'
    if (-not (Wait-FileContainsAfterOffset -Path $StdoutPath -Pattern $selectedRowPattern `
            -StartingOffset $selectionOffset -TimeoutMilliseconds 5000)) {
        throw "The product Scene Objects row did not retain the same hidden entity, canonical name, and one-line presentation after selection."
    }
    Write-Output "[pass] Packaged Scene Objects changed authoritative selection from '$AlternateName' to hidden '$Name' through the real one-line row hitbox (entity $entity)"

    if ($RestorePanelAfterSelection) {
        $restoreOffset = Get-FileLengthSafe -Path $StdoutPath
        Set-HenkaSceneObjectsPanelHeightThroughUi `
            -Handle $Handle `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -Topology $topology `
            -TargetSceneObjectsBottomY ($SceneObjectsY + $originalSceneObjectsHeight) `
            -StdoutPath $StdoutPath `
            -Description "Restoring the normal Scene Objects panel height after the first row proof"
        if (-not (Wait-FileContainsAfterOffset `
                -Path $StdoutPath `
                -Pattern $dividerReleasePattern `
                -StartingOffset $restoreOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The product did not report Scene Objects geometry after restoring the normal dock height."
        }
        $restoredTopology = Get-HenkaWorkspaceTopologyReport `
            -StdoutPath $StdoutPath `
            -FramebufferWidth $FramebufferWidth `
            -FramebufferHeight $FramebufferHeight `
            -RequireToolsVisible
        if ([Math]::Abs($restoredTopology.SceneObjects.Height - $originalSceneObjectsHeight) -gt 1.0) {
            throw "The normal Scene Objects height was not restored after the hidden-row selection proof."
        }
        $restoredPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row entity=' +
            [Regex]::Escape($entity) + ' name=' + $escapedName + ' display=[^\r\n]* lines=2 hidden=1 selected=1 x=[-0-9.]+ y=[-0-9.]+ width=[-0-9.]+ height=[-0-9.]+\.'
        if (-not (Wait-FileContainsAfterOffset -Path $StdoutPath -Pattern $restoredPattern `
                -StartingOffset $restoreOffset -TimeoutMilliseconds 5000)) {
            throw "The first hidden-row proof did not restore the normal multi-line Scene Objects layout."
        }
    }
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

function Get-HenkaDirectorySnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        return
    }

    $root = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if (($root.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Refusing to snapshot a reparse-point directory: $Path"
    }

    $snapshot = New-Object 'System.Collections.Generic.List[string]'
    $rootPrefix = $root.FullName.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    foreach ($item in @(Get-ChildItem -LiteralPath $root.FullName -Force -Recurse -ErrorAction Stop)) {
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing to traverse a reparse point in the packaged user-data tree: $($item.FullName)"
        }

        $itemFullPath = [IO.Path]::GetFullPath($item.FullName)
        if (-not $itemFullPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to snapshot a path outside the packaged user-data root: $($item.FullName)"
        }
        $relativePath = $itemFullPath.Substring($rootPrefix.Length)
        if ($item.PSIsContainer) {
            $snapshot.Add("D|$relativePath")
        }
        else {
            $hash = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256 -ErrorAction Stop).Hash
            $snapshot.Add("F|$relativePath|$($item.Length)|$hash")
        }
    }

    return @($snapshot.ToArray() | Sort-Object)
}

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$gitCommand = Get-HenkaGitPath
$packageRoot = Get-HenkaPackageRoot -RepositoryRoot $repoRoot -PackageName "HenkaSandbox3D"
$packageUserRoot = Join-Path $packageRoot "user"
$packagedExe = Join-Path $packageRoot "HenkaSandbox3D.exe"
$assetsDir = Join-Path $packageRoot "assets"
$showcaseModelsDir = Join-Path $assetsDir "models"
$helpPath = Join-Path $packageRoot "docs\help\sandbox3d.md"
$readmePath = Join-Path $packageRoot "README.txt"
$packageInfoPath = Join-Path $packageRoot "PACKAGE_INFO.txt"
$logDir = Get-HenkaTestTemporaryRoot -RepositoryRoot $repoRoot
[System.IO.Directory]::CreateDirectory($logDir) | Out-Null
$automationUserDataRoot = Join-Path $logDir ("pkg_" + [guid]::NewGuid().ToString("N"))
$longestNativeAssetTempPath = Join-Path $automationUserDataRoot "authored_assets\NativeAsset_ffffffff\rev1\quad_sphere_4.material.henka-tmp"
if ([IO.Path]::GetFullPath($longestNativeAssetTempPath).Length -ge 260) {
    throw "The isolated packaged user-data root leaves insufficient Windows path capacity for a native material sidecar temporary file."
}
$settingsPath = Join-Path $automationUserDataRoot "sandbox3d.settings"
$stdoutPath = Join-Path $logDir "check_packaged_sandbox3d_stdout.log"
$stderrPath = Join-Path $logDir "check_packaged_sandbox3d_stderr.log"
$startupScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_startup.png"
$productStartupPrimitiveScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_product_startup_add_cube.png"
$productStartupGroundDetailsScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_product_startup_ground_details.png"
$terrainUiBeforeScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_terrain_ui_before_create.png"
$terrainUiAfterScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_terrain_ui_after_create.png"
$qaScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_controls_qa.png"
$shadingMaterialPreviewScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_shading_material_preview_after_action.png"
$shadingRenderedScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_shading_rendered_after_action.png"
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
$physicsCapturePath = Join-Path $logDir "physics-reference-wide.bmp"
$automationInputPath = Join-Path $logDir "check_packaged_sandbox3d_automation.events"
$showcaseAuthoringLaunchArguments = @("--capture-showcase-view", "wide", "solid")
# This face ID came from the previously classified adjacent-face-collapse
# observation. It is optional runtime evidence: the deterministic in-memory
# mesh/operator regressions own the X+ negative control when a packaged source
# revision no longer contains this exact logical ID.
$nativeFaceMoveCollapseTargetId = [uint32]16530

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
$sourceIdentity = Get-PackageInfoValue -Path $packageInfoPath -Name "Source identity"
$buildConfiguration = Get-PackageInfoValue -Path $packageInfoPath -Name "Build configuration"
$buildRef = Get-PackageInfoValue -Path $packageInfoPath -Name "Build ref"
$detachedHead = Get-PackageInfoValue -Path $packageInfoPath -Name "Detached HEAD"
$sourceHash = Get-PackageInfoValue -Path $packageInfoPath -Name "Source executable SHA-256"
$packagedHash = Get-PackageInfoValue -Path $packageInfoPath -Name "Packaged executable SHA-256"
$actualPackagedHash = (Get-FileHash -LiteralPath $packagedExe -Algorithm SHA256).Hash.ToLowerInvariant()
$currentCommitLines = @(& $gitCommand -C $repoRoot rev-parse HEAD 2>$null)
if ($LASTEXITCODE -ne 0 -or $currentCommitLines.Count -ne 1) { throw "Current commit could not be read for package verification." }
$currentCommit = ([string]$currentCommitLines[0]).Trim()
$currentSourceIdentity = Get-HenkaSourceIdentity -RepoRoot $repoRoot
$currentSourceState = $currentSourceIdentity.source_state
if ($packageSchema -ne "4") { throw "Packaged build marker has an unsupported schema." }
if ($sourceCommit -ne $currentCommit) { throw "Packaged source commit does not match current HEAD." }
if ($sourceState -ne $currentSourceState) { throw "Packaged source state does not match the current working tree." }
if ($sourceIdentity -ne $currentSourceIdentity.source_identity) { throw "Packaged source identity does not match the current candidate." }
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

function Invoke-HenkaIsolatedPackageNativeCapture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Label,

        [bool]$RequireIsolatedUserData = $true,

        [ValidateRange(1000, 3600000)]
        [int]$TimeoutMilliseconds = 180000,

        [switch]$Quiet
    )

    $runRoot = Join-Path $logDir ("pkgsmk-" + [guid]::NewGuid().ToString("N"))
    $isolatedUserRoot = Join-Path $runRoot "user"
    $automationInputPath = Join-Path $runRoot "automation.events"
    $persistenceSourcePath = Join-Path $script:repoRoot "engine\src\core\persistence.c"
    $packageUserSnapshotBefore = @(Get-HenkaDirectorySnapshot -Path $packageUserRoot)
    $previousAutomationOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
    $previousAutomationFile = $env:HENKA_AUTOMATION_INPUT_FILE
    $previousAutomationDiagnostics = $env:HENKA_AUTOMATION_DIAGNOSTICS
    $previousAutomationUserDataBasePath = $env:HENKA_AUTOMATION_USER_DATA_BASE_PATH
    $retainRunRoot = $true

    try {
        $logDirectoryItem = Get-Item -LiteralPath $logDir -Force -ErrorAction Stop
        if (($logDirectoryItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing isolated package smoke data under a reparse-point test directory: $logDir"
        }
        if (-not (Test-Path -LiteralPath $persistenceSourcePath -PathType Leaf)) {
            throw "The packaged smoke path budget cannot be verified because the persistence implementation is missing: $persistenceSourcePath"
        }
        $persistenceSuffixDefinition = Select-String `
            -LiteralPath $persistenceSourcePath `
            -Pattern '^\s*#define\s+HENKA_PERSISTENCE_TEMP_SUFFIX\s+"([^"]+)"' |
            Select-Object -First 1
        if ($null -eq $persistenceSuffixDefinition -or $persistenceSuffixDefinition.Matches.Count -ne 1) {
            throw "The packaged smoke path budget could not read HENKA_PERSISTENCE_TEMP_SUFFIX from the production persistence implementation."
        }
        $persistenceTempSuffix = $persistenceSuffixDefinition.Matches[0].Groups[1].Value
        $smokeMaterialSidecarPath = Join-Path $isolatedUserRoot "authored_assets\SmokeAsset\rev1\smoke_box_1.material"
        $smokeMaterialTempPathLength = $smokeMaterialSidecarPath.Length + $persistenceTempSuffix.Length
        if ($smokeMaterialTempPathLength -ge 260) {
            throw "The isolated package smoke material temp path would exceed the legacy Windows MAX_PATH contract ($smokeMaterialTempPathLength characters before its terminator)."
        }
        Write-Host ("[pass] Isolated smoke material temp path budget: {0} characters plus terminator (limit 260)." -f $smokeMaterialTempPathLength)
        if (Test-Path -LiteralPath $runRoot) {
            throw "The unique isolated package smoke data path unexpectedly already exists: $runRoot"
        }

        [System.IO.Directory]::CreateDirectory($isolatedUserRoot) | Out-Null
        [System.IO.File]::WriteAllText($automationInputPath, "")
        $env:HENKA_AUTOMATION_INPUT_OWNED = "1"
        $env:HENKA_AUTOMATION_INPUT_FILE = $automationInputPath
        $env:HENKA_AUTOMATION_DIAGNOSTICS = "1"
        $env:HENKA_AUTOMATION_USER_DATA_BASE_PATH = $isolatedUserRoot

        $capture = Invoke-HenkaNativeCapture `
            -FilePath $FilePath `
            -Arguments $Arguments `
            -WorkingDirectory $WorkingDirectory `
            -Label $Label `
            -TimeoutMilliseconds $TimeoutMilliseconds `
            -Quiet:$Quiet

        if ($RequireIsolatedUserData -and
            $capture.Stdout -notmatch "HENKA_AUTOMATION_DIAGNOSTIC user_data_base_path=isolated") {
            throw "$Label did not confirm its automation-owned isolated user-data root."
        }

        $packageUserSnapshotAfter = @(Get-HenkaDirectorySnapshot -Path $packageUserRoot)
        if (@(Compare-Object -ReferenceObject $packageUserSnapshotBefore -DifferenceObject $packageUserSnapshotAfter).Count -ne 0) {
            throw "$Label changed the packaged user-data tree despite isolated user-data configuration."
        }

        $retainRunRoot = $false
        return $capture
    }
    catch {
        throw "$Label failed with isolated package user data at '$isolatedUserRoot': $($_.Exception.Message)"
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
        if ($null -eq $previousAutomationDiagnostics) {
            Remove-Item Env:HENKA_AUTOMATION_DIAGNOSTICS -ErrorAction SilentlyContinue
        }
        else {
            $env:HENKA_AUTOMATION_DIAGNOSTICS = $previousAutomationDiagnostics
        }
        if ($null -eq $previousAutomationUserDataBasePath) {
            Remove-Item Env:HENKA_AUTOMATION_USER_DATA_BASE_PATH -ErrorAction SilentlyContinue
        }
        else {
            $env:HENKA_AUTOMATION_USER_DATA_BASE_PATH = $previousAutomationUserDataBasePath
        }

        if ($retainRunRoot -and (Test-Path -LiteralPath $runRoot)) {
            Write-Warning "Preserving isolated package smoke data for diagnosis: $runRoot"
        }
        elseif (-not $retainRunRoot -and (Test-Path -LiteralPath $runRoot)) {
            try {
                $normalizedLogDirectory = [IO.Path]::GetFullPath($logDir).TrimEnd([IO.Path]::DirectorySeparatorChar)
                $normalizedRunRoot = [IO.Path]::GetFullPath($runRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
                if (-not $normalizedRunRoot.StartsWith(
                        $normalizedLogDirectory + [IO.Path]::DirectorySeparatorChar,
                        [StringComparison]::OrdinalIgnoreCase)) {
                    throw "Generated package smoke path escaped the test-output directory: $normalizedRunRoot"
                }
                $null = @(Get-HenkaDirectorySnapshot -Path $normalizedRunRoot)
                Remove-Item -LiteralPath $normalizedRunRoot -Recurse -Force -ErrorAction Stop
                if (Test-Path -LiteralPath $normalizedRunRoot) {
                    throw "Generated package smoke data remains after exact-path cleanup: $normalizedRunRoot"
                }
            }
            catch {
                Write-Warning "Could not retire successful isolated package smoke data '$runRoot': $($_.Exception.Message)"
            }
        }
    }
}

if ($NonInteractive) {
    if ($ContractOnly) {
        Write-Step "Completing hosted package contract validation"
        Write-Output "[pass] Packaged sandbox contract validation completed."
        return
    }

    Write-Step "Running deterministic packaged startup smoke test"
    $smoke = Invoke-HenkaIsolatedPackageNativeCapture `
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

    Write-Step "Running packaged Physics QA smoke"
    $physicsSmoke = Invoke-HenkaIsolatedPackageNativeCapture `
        -FilePath $packagedExe `
        -Arguments @("--physics-smoke-test") `
        -WorkingDirectory $packageRoot `
        -Label "Run packaged Physics QA smoke"

    if ($physicsSmoke.Stdout -notmatch "Physics smoke: real scene-linked bodies exercised static, dynamic, and kinematic paths; capsule collider, heightfield contact/raycast, kinematic sphere/capsule/box plane/heightfield contact, triangle-mesh dynamic/kinematic sphere/capsule/box contact/raycast, character-controller Play movement/jump, fixed-step contact/events, trigger state, raycast, and reset passed\.") {
        throw "The packaged Physics smoke test did not report its complete production scenario."
    }
    if ($physicsSmoke.Stderr -notmatch "leaving engine run loop") {
        throw "The packaged Physics smoke test did not leave the engine run loop cleanly."
    }

    Write-Output "[pass] Packaged Physics QA smoke completed."

    Write-Step "Running packaged Physics rendered capture"
    $physicsCapture = Invoke-HenkaIsolatedPackageNativeCapture `
        -FilePath $packagedExe `
        -Arguments @("--capture-physics-view", "wide", "rendered", $logDir) `
        -WorkingDirectory $packageRoot `
        -Label "Run packaged Physics rendered capture"

    if ($physicsCapture.Stdout -notmatch "PHYSICS_CAPTURE_READY mode=rendered view=wide .*body_count=[1-9][0-9]* .*showcase_visible=0 .*debug_colliders=1 debug_contacts=1 .*draw_expected=1") {
        throw "The packaged Physics capture did not report a real-body rendered readiness marker."
    }
    if (-not (Test-Path -LiteralPath $physicsCapturePath -PathType Leaf)) {
        throw "The packaged Physics capture did not create its application-owned framebuffer image."
    }
    if ((Get-Item -LiteralPath $physicsCapturePath).Length -le 0) {
        throw "The packaged Physics capture image is empty."
    }

    Write-Output "[pass] Packaged Physics rendered capture completed."

    Write-Step "Running packaged Audio fixture smoke"
    $audioSmoke = Invoke-HenkaIsolatedPackageNativeCapture -FilePath $packagedExe -Arguments @("--audio-smoke-test") -WorkingDirectory $packageRoot -Label "Run packaged Audio fixture smoke"

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
    Write-Step "Running packaged Prefab public API smoke"
    $prefabSmoke = Invoke-HenkaIsolatedPackageNativeCapture `
        -FilePath $packagedExe `
        -Arguments @("--prefab-smoke-test") `
        -WorkingDirectory $packageRoot `
        -Label "Run packaged Prefab public API smoke" `
        -RequireIsolatedUserData:$false

    if ($prefabSmoke.Stdout -notmatch "Prefab package smoke: public save/load/instantiate/override/duplicate/detach workflow passed\.") {
        throw "The packaged Prefab smoke test did not prove the public Prefab workflow."
    }
    Write-Output "[pass] Packaged Prefab public API smoke completed."

    Write-Step "Running packaged Prefab Game Authoring smoke"
    $prefabAuthoringSmoke = Invoke-HenkaIsolatedPackageNativeCapture `
        -FilePath $packagedExe `
        -Arguments @("--prefab-authoring-smoke-test") `
        -WorkingDirectory $packageRoot `
        -Label "Run packaged Prefab Game Authoring smoke"

    if ($prefabAuthoringSmoke.Stdout -notmatch "Prefab Game Authoring smoke: real source capture, mapped instance refresh, save/load, and Play restart workflow passed\.") {
        throw "The packaged Prefab Game Authoring smoke test did not prove the product-native workflow."
    }
    if ($prefabAuthoringSmoke.Stderr -notmatch "leaving engine run loop") {
        throw "The packaged Prefab Game Authoring smoke test did not leave the engine run loop cleanly."
    }
    Write-Output "[pass] Packaged Prefab Game Authoring smoke completed."

    Write-Step "Running bounded packaged Terrain stream stress"
    $terrainStreamStress = Invoke-HenkaIsolatedPackageNativeCapture `
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
            Name = "material instance"
            Arguments = @("--material-stress")
            Pattern = "Material stress: typed-overrides=all-supported invalid-edit=retained entity-commit=valid refresh=valid reset=valid\."
        }
        @{
            Name = "environment"
            Arguments = @("--environment-stress")
            Pattern = "Environment stress: .*"
        }
    )) {
        Write-Step "Running packaged $($stressCase.Name) stress"
        $stress = Invoke-HenkaIsolatedPackageNativeCapture `
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
        $productStartupPrimitiveScreenshotPath,
        $qaScreenshotPath,
        $shadingMaterialPreviewScreenshotPath,
        $shadingRenderedScreenshotPath,
        $nativeScreenshotPath,
        $nativeAuthoringScreenshotPath,
        $nativeAuthoredScreenshotPath,
        $persistenceStdoutPath,
        $persistenceStderrPath,
        $startupRestoreStdoutPath,
        $startupRestoreStderrPath,
        $physicsCapturePath) `
    -ErrorAction SilentlyContinue

$capturedProcess = $null
$startupRestoreCapture = $null
$startupRestoreProcess = $null
$process = $null
$mainWindowHandle = [System.IntPtr]::Zero
$uiAutomationVerified = $false
$sandboxPanelsVisible = $false
$packagedCheckSucceeded = $false
$packageUserSnapshotBefore = @()
$previousAutomationOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
$previousAutomationFile = $env:HENKA_AUTOMATION_INPUT_FILE
$previousAutomationDiagnostics = $env:HENKA_AUTOMATION_DIAGNOSTICS
$previousAutomationDiagnosticFaceId = $env:HENKA_AUTOMATION_DIAGNOSTIC_FACE_ID
$previousAutomationUserDataBasePath = $env:HENKA_AUTOMATION_USER_DATA_BASE_PATH
try {
    $logDirectoryItem = Get-Item -LiteralPath $logDir -Force -ErrorAction Stop
    if (($logDirectoryItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Refusing to place isolated packaged user data under a reparse-point test directory: $logDir"
    }
    $packageUserSnapshotBefore = @(Get-HenkaDirectorySnapshot -Path $packageUserRoot)
    if (Test-Path -LiteralPath $automationUserDataRoot) {
        throw "The unique isolated package user-data path unexpectedly already exists."
    }
    New-Item -ItemType Directory -Path $automationUserDataRoot -ErrorAction Stop | Out-Null
    New-Item -ItemType File -Path $automationInputPath -Force | Out-Null
    $env:HENKA_AUTOMATION_INPUT_OWNED = "1"
    $env:HENKA_AUTOMATION_INPUT_FILE = $automationInputPath
    $env:HENKA_AUTOMATION_DIAGNOSTICS = "1"
    $env:HENKA_AUTOMATION_DIAGNOSTIC_FACE_ID = [string]$nativeFaceMoveCollapseTargetId
    $env:HENKA_AUTOMATION_USER_DATA_BASE_PATH = $automationUserDataRoot
    Write-Step "Launching the packaged sandbox"
    # The native authoring workflow is an explicit reference-asset path.
    # Ordinary no-argument startup is validated separately as the clean
    # product-native scene and must not be repopulated for this check.
    if ($TerrainStartupOnly -or $ProductStartupPrimitiveOnly) {
        $capturedProcess = Start-HenkaCapturedProcess `
            -FilePath $packagedExe `
            -WorkingDirectory $packageRoot `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath `
            -StartMinimized:$false `
            -StartVisibleWithoutActivation
    }
    else {
        $capturedProcess = Start-HenkaCapturedProcess `
            -FilePath $packagedExe `
            -WorkingDirectory $packageRoot `
            -Arguments $showcaseAuthoringLaunchArguments `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath `
            -StartMinimized:$false `
            -StartVisibleWithoutActivation
    }
    $process = $capturedProcess.Process

    if (-not (Wait-FileContains `
            -Path $stdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC user_data_base_path=isolated' `
            -TimeoutMilliseconds 10000)) {
        throw "The packaged Sandbox did not confirm automation-owned isolated user data before the workflow began."
    }

    for ($index = 0; $index -lt 80 -and $mainWindowHandle -eq [System.IntPtr]::Zero; $index++) {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
        $mainWindowHandle = [NativeMethods]::FindProcessWindow([uint32]$process.Id, "Henka Engine Sandbox 3D")
    }

    if ($mainWindowHandle -eq [System.IntPtr]::Zero) {
        throw "The packaged sandbox window did not become available."
    }

    $startupReadiness = Wait-HenkaPackagedStartupReady `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath `
        -ProcessId $process.Id `
        -HardTimeoutMilliseconds 120000 `
        -NoProgressTimeoutMilliseconds 45000 `
        -PollMilliseconds 150
    $startupReadinessMessage =
        "[pass] Packaged startup readiness reached {0} after {1} ms " +
        "({2} application stages observed)."
    Write-Output ($startupReadinessMessage -f
        $startupReadiness.LastProgressStage,
        $startupReadiness.ElapsedMilliseconds,
        $startupReadiness.ProgressStagesObserved)
    Assert-FileContains -Path $stdoutPath -Pattern "Henka Engine Sandbox 3D" -Description "Startup help heading"
    Assert-FileContains -Path $stdoutPath -Pattern "F4               Show or hide the sandbox panels" -Description "F4 help text"
    Assert-FileContains -Path $stdoutPath -Pattern "F5               Switch Standard and Focus Viewport layouts" -Description "F5 help text"
    Assert-FileContains -Path $stdoutPath -Pattern "Runtime mode: Packaged" -Description "Packaged runtime mode output"
    Assert-FileContains -Path $stdoutPath -Pattern "Startup selection: None" -Description "Packaged startup no-selection output"
    Assert-FileContains -Path $stdoutPath -Pattern "Startup UI:" -Description "Startup UI cue"
    Assert-FileContains -Path $stdoutPath -Pattern "Tools and Physics QA are discoverable immediately" -Description "Startup auto panel cue"
    Assert-FileContains -Path $stdoutPath -Pattern "use the in-window .*utilities" -Description "Startup utility cue"
    Assert-FileContains -Path $stdoutPath -Pattern "recent actions and warnings appear" -Description "Startup status cue"
    if ($TerrainStartupOnly -or $ProductStartupPrimitiveOnly) {
        Assert-FileContains `
            -Path $stdoutPath `
            -Pattern "DEFAULT_SCENE_READY ground=1 ground_editable=1 camera=1 showcase_assets=0 diagnostic_entities=0 scene_content=product_native" `
            -Description "Clean product-native packaged startup"
    }

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
            $preflightToolsMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Scene View Tools control: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)\.'
            if ($null -eq $preflightToolsMatch) {
                throw "The packaged Scene View Tools control geometry was not reported."
            }
            $preflightToolsX = [double]$preflightToolsMatch.Groups[1].Value
            $preflightToolsY = [double]$preflightToolsMatch.Groups[2].Value
            $preflightToolsWidth = [double]$preflightToolsMatch.Groups[3].Value
            $preflightToolsHeight = [double]$preflightToolsMatch.Groups[4].Value
            Assert-FramebufferRect `
                -Name "Scene View Tools control" `
                -FramebufferWidth $preflightFramebufferWidth `
                -FramebufferHeight $preflightFramebufferHeight `
                -X $preflightToolsX `
                -Y $preflightToolsY `
                -Width $preflightToolsWidth `
                -Height $preflightToolsHeight
            Write-Output "[check] Opening the hidden Tools dock through the Scene View header"
            $nativeOpenLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $preflightFramebufferWidth `
                -FramebufferHeight $preflightFramebufferHeight `
                -FramebufferX ($preflightToolsX + $preflightToolsWidth * 0.5) `
                -FramebufferY ($preflightToolsY + $preflightToolsHeight * 0.5)
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern "Tools QA tab:" `
                    -StartingOffset $nativeOpenLogOffset `
                    -TimeoutMilliseconds 4000)) {
                throw "The packaged Tools dock was not available and could not be opened through its logical Scene View header control."
            }
            $toolsTopologyPattern = '^HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=tools_toggle tools_visible=1 '
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $toolsTopologyPattern `
                    -StartingOffset $nativeOpenLogOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The real Scene View Tools click did not produce a fresh product report showing the Controls section and its topology state."
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

        $toolsTopologyPattern = '^HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=[a-z_]+ tools_visible=(?<toolsVisible>[01]) '
        $toolsTopologyMatch = Wait-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern $toolsTopologyPattern `
            -GroupName 'toolsVisible' `
            -ExpectedValue '1' `
            -TimeoutMilliseconds 5000
        if ($null -eq $toolsTopologyMatch) {
            throw "The product did not confirm that Tools/Controls is visible in its current workspace layout."
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

        $workspaceTopology = Get-HenkaWorkspaceTopologyReport `
            -StdoutPath $stdoutPath `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -RequireToolsVisible -RequireDivider
        $leftDockX = $workspaceTopology.Dock.X
        $leftDockY = $workspaceTopology.Dock.Y
        $leftDockWidth = $workspaceTopology.Dock.Width
        $leftDockHeight = $workspaceTopology.Dock.Height
        $controlsX = $workspaceTopology.Controls.X
        $controlsY = $workspaceTopology.Controls.Y
        $controlsWidth = $workspaceTopology.Controls.Width
        $controlsHeight = $workspaceTopology.Controls.Height
        $sceneObjectsX = $workspaceTopology.SceneObjects.X
        $sceneObjectsY = $workspaceTopology.SceneObjects.Y
        $sceneObjectsWidth = $workspaceTopology.SceneObjects.Width
        $sceneObjectsHeight = $workspaceTopology.SceneObjects.Height

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

        if ($ProductStartupPrimitiveOnly) {
            Write-Step "Applying fractional viewport zoom through packaged input and checking authoritative camera state"
            $cameraZoomInputDelta = 0.25
            $cameraZoomPattern = '^HENKA_AUTOMATION_DIAGNOSTIC camera_zoom frame=\d+ wheel_y=(?<wheel>[-+0-9.eE]+) viewport=(?<viewport>[01]) projection=(?<projection>orthographic|perspective|unknown|unavailable) before_distance=(?<beforeDistance>[-+0-9.eE]+) after_distance=(?<afterDistance>[-+0-9.eE]+) before_height=(?<beforeHeight>[-+0-9.eE]+) after_height=(?<afterHeight>[-+0-9.eE]+) applied=(?<applied>[01])$'
            $cameraZoomDiagnosticOffset = Get-FileLengthSafe -Path $stdoutPath
            Send-HenkaBackgroundFramebufferScroll `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($framebufferWidth * 0.5) `
                -FramebufferY ($framebufferHeight * 0.5) `
                -WheelDelta $cameraZoomInputDelta
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $cameraZoomPattern `
                    -StartingOffset $cameraZoomDiagnosticOffset `
                    -TimeoutMilliseconds 3500)) {
                throw "The real packaged viewport wheel event did not produce a product camera-state diagnostic."
            }
            $cameraZoomDiagnostic = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $cameraZoomPattern
            if ($null -eq $cameraZoomDiagnostic -or
                $cameraZoomDiagnostic.Groups["viewport"].Value -ne "1" -or
                $cameraZoomDiagnostic.Groups["applied"].Value -ne "1" -or
                [Math]::Abs([double]$cameraZoomDiagnostic.Groups["wheel"].Value - $cameraZoomInputDelta) -gt 0.00001) {
                throw "The product did not confirm the expected fractional wheel input was applied inside the viewport."
            }
            $cameraZoomProjection = $cameraZoomDiagnostic.Groups["projection"].Value
            if ($cameraZoomProjection -eq "orthographic") {
                $beforeMetric = [double]$cameraZoomDiagnostic.Groups["beforeHeight"].Value
                $afterMetric = [double]$cameraZoomDiagnostic.Groups["afterHeight"].Value
                if (-not ($afterMetric -lt $beforeMetric)) {
                    throw "Fractional viewport zoom did not reduce authoritative orthographic camera height."
                }
            }
            elseif ($cameraZoomProjection -eq "perspective") {
                $beforeMetric = [double]$cameraZoomDiagnostic.Groups["beforeDistance"].Value
                $afterMetric = [double]$cameraZoomDiagnostic.Groups["afterDistance"].Value
                if (-not ($afterMetric -lt $beforeMetric)) {
                    throw "Fractional viewport zoom did not reduce authoritative perspective camera distance."
                }
            }
            else {
                throw "The packaged camera uses an unsupported projection for viewport zoom validation."
            }
            Write-Output ("[pass] Fractional viewport wheel input changed authoritative {0} camera state ({1:F4} -> {2:F4})" -f $cameraZoomProjection, $beforeMetric, $afterMetric)

            Write-Step "Selecting the product-native Ground row to inspect long Object Details text"
            $groundRowPattern = '^Default scene Ground row: x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) selected=(?<selected>[01])\.'
            $groundRowMatch = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $groundRowPattern
            if ($null -eq $groundRowMatch -or $groundRowMatch.Groups["selected"].Value -ne "0") {
                throw "The product-native Ground row was not available in its expected unselected startup state."
            }
            $groundRowX = [double]$groundRowMatch.Groups["x"].Value
            $groundRowY = [double]$groundRowMatch.Groups["y"].Value
            $groundRowWidth = [double]$groundRowMatch.Groups["width"].Value
            $groundRowHeight = [double]$groundRowMatch.Groups["height"].Value
            Assert-FramebufferRect `
                -Name "Product-native Ground Scene Objects row for Object Details" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $groundRowX `
                -Y $groundRowY `
                -Width $groundRowWidth `
                -Height $groundRowHeight
            $groundSelectionLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($groundRowX + $groundRowWidth * 0.5) `
                -FramebufferY ($groundRowY + $groundRowHeight * 0.5)
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern '^Default scene Ground row: .* selected=1\.' `
                    -StartingOffset $groundSelectionLogOffset `
                    -TimeoutMilliseconds 3500)) {
                throw "The normal Scene Objects click did not select the product-native Ground row for its long Details text."
            }
            Write-Output "[pass] Product-native Ground selection reached the authoritative Scene Objects state"
            Set-HenkaAutomationForeground -Handle $mainWindowHandle
            Start-Sleep -Milliseconds 500
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $productStartupGroundDetailsScreenshotPath `
                -Description "Packaged product-native Ground Object Details"
            Write-Output "[pass] Product-native Ground long Object Details visual proof captured"

            Write-Step "Checking product-native Add Cube through the visible Scene Objects UI"
            $addCubeRowX = $sceneObjectsX + 14.0
            $addCubeY = $sceneObjectsY + 60.0
            $addCubeRowWidth = $sceneObjectsWidth - 28.0
            $addCubeHeight = 24.0
            Assert-FramebufferRect `
                -Name "Scene Objects action row containing Add Cube" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $addCubeRowX `
                -Y $addCubeY `
                -Width $addCubeRowWidth `
                -Height $addCubeHeight

            # Ground is selected for the Details capture, so the product renders
            # Add Cube, Clone, and Delete in this row.  The row's center is Clone;
            # click inside its first action cell instead of reusing the unselected
            # full-width Add Cube target.
            $addCubeClickX = $addCubeRowX + 8.0
            $addCubeClickY = $addCubeY + ($addCubeHeight * 0.5)
            $addCubeOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX $addCubeClickX `
                -FramebufferY $addCubeClickY
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern 'DEFAULT_SCENE_ADD_CUBE_READY entity=[0-9]+ document_id=[0-9]+ source=primitive canonical_document=1\.' `
                    -StartingOffset $addCubeOffset `
                    -TimeoutMilliseconds 6000)) {
                throw "The visible default-scene Add Cube control did not publish a canonical primitive Scene Document object."
            }
            if ((Get-Content -LiteralPath $stdoutPath -Raw) -match 'Native asset document: name=NativeAsset action=created parts=0\.') {
                throw "The ordinary default-scene Add Cube path created an asset document instead of using the canonical scene operation."
            }
            Write-Output "[pass] Default-scene Add Cube reached the canonical scene operation and Scene Document binding"
            Set-HenkaAutomationForeground -Handle $mainWindowHandle
            Start-Sleep -Milliseconds 700
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $productStartupPrimitiveScreenshotPath `
                -Description "Packaged product-native Add Cube"
            Write-Output "[pass] Product-native Add Cube visual proof captured"
            Invoke-PackagedProductGameAuthoringWorkflow `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -DetailsX $detailsX `
                -DetailsY $detailsY `
                -DetailsWidth $detailsWidth `
                -DetailsHeight $detailsHeight `
                -StdoutPath $stdoutPath `
                -ScreenshotPath $nativeAuthoringScreenshotPath
            Write-Output "[pass] Packaged Game Authoring workflow used the canonical product-startup Add Cube"
            if ($framebufferWidth -ne 1280 -or $framebufferHeight -ne 720) {
                throw "Hidden Scene Objects selection proof requires the packaged 1280x720 layout; received ${framebufferWidth}x${framebufferHeight}."
            }
            Write-Step "Hiding and selecting multiple named Scene Objects rows at 1280x720"
            $hiddenRowCases = @(
                [PSCustomObject]@{ Name = 'Ground'; AlternateName = 'New Cube'; RestorePanel = $true },
                [PSCustomObject]@{ Name = 'New Cube'; AlternateName = 'Ground'; RestorePanel = $false }
            )
            $script:hiddenRowOneLineReachable = $true
            foreach ($hiddenRowCase in $hiddenRowCases) {
                $hiddenRowArguments = @{
                    Name = $hiddenRowCase.Name
                    AlternateName = $hiddenRowCase.AlternateName
                    Handle = $mainWindowHandle
                    FramebufferWidth = $framebufferWidth
                    FramebufferHeight = $framebufferHeight
                    SceneObjectsX = $sceneObjectsX
                    SceneObjectsY = $sceneObjectsY
                    SceneObjectsWidth = $sceneObjectsWidth
                    SceneObjectsHeight = $sceneObjectsHeight
                    DetailsX = $detailsX
                    DetailsY = $detailsY
                    DetailsWidth = $detailsWidth
                    DetailsHeight = $detailsHeight
                    StdoutPath = $stdoutPath
                }
                if ($hiddenRowCase.RestorePanel) {
                    $hiddenRowArguments.RestorePanelAfterSelection = $true
                }
                Assert-HenkaPackagedHiddenNamedObjectSelectable @hiddenRowArguments
                if (-not $script:hiddenRowOneLineReachable) {
                    break
                }
            }
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path (Join-Path $logDir 'scene-objects-hidden-rows-1280x720.bmp') `
                -Description "Packaged 1280x720 Scene Objects after the bounded hidden-row reachability probe"
            if ($script:hiddenRowOneLineReachable) {
                Write-Output "[pass] Two distinct hidden product-native objects retained readable names and individually selected through their visible one-line rows"
            }
            else {
                Write-Output "[classified-open] The formatter regression remains the one-line fallback proof; packaged evidence covers the narrowest layout reached by the real product divider, and the one-line state is not reachable through this packaged UI path."
            }
            $packagedCheckSucceeded = $true
            return
        }

        Write-Step "Checking packaged Asset Browser Materials tab geometry and selection"
        $utilityAssetsTabMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Utility Assets tab: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)\.'
        if ($null -eq $utilityAssetsTabMatch) {
            throw "The product-owned Utility > Assets tab geometry was not reported."
        }
        $utilityAssetsTabX = [double]$utilityAssetsTabMatch.Groups[1].Value
        $utilityAssetsTabY = [double]$utilityAssetsTabMatch.Groups[2].Value
        $utilityAssetsTabWidth = [double]$utilityAssetsTabMatch.Groups[3].Value
        $utilityAssetsTabHeight = [double]$utilityAssetsTabMatch.Groups[4].Value
        Assert-FramebufferRect `
            -Name "Utility Assets tab" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $utilityAssetsTabX `
            -Y $utilityAssetsTabY `
            -Width $utilityAssetsTabWidth `
            -Height $utilityAssetsTabHeight

        $assetUtilityClickOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($utilityAssetsTabX + $utilityAssetsTabWidth * 0.5) `
            -FramebufferY ($utilityAssetsTabY + $utilityAssetsTabHeight * 0.5)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC utility action_seq=\d+ frame=\d+ before=.* requested=Assets after=Assets changed=[01]' `
                -StartingOffset $assetUtilityClickOffset `
                -TimeoutMilliseconds 4000)) {
            throw "A real Utility > Assets click did not reach the authoritative utility transition path."
        }

        $assetUtilityActionMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC utility action_seq=(\d+) frame=\d+ before=.* requested=Assets after=Assets changed=[01]'
        if ($null -eq $assetUtilityActionMatch) {
            throw "The authoritative Utility > Assets transition sequence was not available for correlating its layout."
        }
        $assetUtilityActionSequence = $assetUtilityActionMatch.Groups[1].Value
        $settledAssetTypeLayoutPattern = "HENKA_AUTOMATION_DIAGNOSTIC asset_type_tabs action_seq=$assetUtilityActionSequence frame=\d+ utility_tabs_visible=0 active_assets=1 status=(?:available|unavailable)"
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern $settledAssetTypeLayoutPattern `
                -StartingOffset $assetUtilityClickOffset `
                -TimeoutMilliseconds 4000)) {
            throw "The Asset Browser did not report current type-tab geometry after its Utility navigation rows settled."
        }
        $assetTypeTabsMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern "HENKA_AUTOMATION_DIAGNOSTIC asset_type_tabs action_seq=$assetUtilityActionSequence frame=\d+ utility_tabs_visible=0 active_assets=1 status=available count=4 materials_x=([-0-9.]+) materials_y=([-0-9.]+) materials_width=([-0-9.]+) materials_height=([-0-9.]+)"
        if ($null -eq $assetTypeTabsMatch) {
            $unavailableMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern "HENKA_AUTOMATION_DIAGNOSTIC asset_type_tabs action_seq=$assetUtilityActionSequence frame=\d+ utility_tabs_visible=0 active_assets=1 status=unavailable result=(-?\d+) count=(\d+)"
            if ($null -ne $unavailableMatch) {
                throw "The production Asset Browser type-tab row is unavailable (result=$($unavailableMatch.Groups[1].Value), count=$($unavailableMatch.Groups[2].Value))."
            }
            throw "The production Materials tab rectangle was not reported."
        }
        $materialsTabX = [double]$assetTypeTabsMatch.Groups[1].Value
        $materialsTabY = [double]$assetTypeTabsMatch.Groups[2].Value
        $materialsTabWidth = [double]$assetTypeTabsMatch.Groups[3].Value
        $materialsTabHeight = [double]$assetTypeTabsMatch.Groups[4].Value
        Assert-FramebufferRect `
            -Name "Asset Browser Materials tab" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $materialsTabX `
            -Y $materialsTabY `
            -Width $materialsTabWidth `
            -Height $materialsTabHeight

        $materialsSelectionOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($materialsTabX + $materialsTabWidth * 0.5) `
            -FramebufferY ($materialsTabY + $materialsTabHeight * 0.5)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC asset_type_tab action=click type=Materials selected=1' `
                -StartingOffset $materialsSelectionOffset `
                -TimeoutMilliseconds 4000)) {
            throw "A real packaged click on the Materials tab did not select the Materials asset view."
        }
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path (Join-Path $logDir 'asset-browser-materials-tab-1280x720.bmp') `
            -Description "Packaged Asset Browser with Materials selected at the actual product tab rectangle"
        Write-Output "[pass] Asset Browser Materials tab selected through its product-owned rectangle and the real packaged UI click"

        Write-Step "Restoring Utility navigation through the visible Asset Browser Tools control"
        $assetBrowserToolsPattern = "HENKA_AUTOMATION_DIAGNOSTIC asset_browser_tools action_seq=$assetUtilityActionSequence frame=\d+ active_assets=1 navigation_visible=0 status=available x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)"
        $assetBrowserToolsMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern $assetBrowserToolsPattern
        if ($null -eq $assetBrowserToolsMatch) {
            throw "The product-owned Asset Browser Tools control geometry was not reported for the active Materials view."
        }
        $assetBrowserToolsX = [double]$assetBrowserToolsMatch.Groups[1].Value
        $assetBrowserToolsY = [double]$assetBrowserToolsMatch.Groups[2].Value
        $assetBrowserToolsWidth = [double]$assetBrowserToolsMatch.Groups[3].Value
        $assetBrowserToolsHeight = [double]$assetBrowserToolsMatch.Groups[4].Value
        Assert-FramebufferRect `
            -Name "Asset Browser Tools control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $assetBrowserToolsX `
            -Y $assetBrowserToolsY `
            -Width $assetBrowserToolsWidth `
            -Height $assetBrowserToolsHeight

        # Capture before dispatch: the product can emit the Tools action and
        # the newly visible Terrain rectangle in the same frame, before this
        # process observes either line.
        $assetBrowserToolsClickOffset = Get-FileLengthSafe -Path $stdoutPath
        $terrainGeometryOffset = $assetBrowserToolsClickOffset
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($assetBrowserToolsX + $assetBrowserToolsWidth * 0.5) `
            -FramebufferY ($assetBrowserToolsY + $assetBrowserToolsHeight * 0.5)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'Asset Browser Tools: action=open utility-navigation\.' `
                -StartingOffset $assetBrowserToolsClickOffset `
                -TimeoutMilliseconds 4000)) {
            throw "The real Asset Browser Tools click did not restore Utility navigation."
        }

        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'Terrain utility tab: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)\.' `
                -StartingOffset $terrainGeometryOffset `
                -TimeoutMilliseconds 4000)) {
            throw "The product did not report current Terrain Utility geometry after Utility navigation was restored."
        }

        Write-Step "Checking packaged Terrain creation through the visible Utility UI"
        $terrainTabMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Terrain utility tab: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)\.'
        if ($null -eq $terrainTabMatch) {
            throw "The packaged Terrain Utility tab geometry was not reported."
        }
        $terrainTabX = [double]$terrainTabMatch.Groups[1].Value
        $terrainTabY = [double]$terrainTabMatch.Groups[2].Value
        $terrainTabWidth = [double]$terrainTabMatch.Groups[3].Value
        $terrainTabHeight = [double]$terrainTabMatch.Groups[4].Value
        Assert-FramebufferRect `
            -Name "Terrain Utility tab" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $terrainTabX `
            -Y $terrainTabY `
            -Width $terrainTabWidth `
            -Height $terrainTabHeight

        $terrainUtilityOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($terrainTabX + $terrainTabWidth * 0.5) `
            -FramebufferY ($terrainTabY + $terrainTabHeight * 0.5)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'Utility active: Terrain\.' `
                -StartingOffset $terrainUtilityOffset `
                -TimeoutMilliseconds 4000)) {
            throw "The visible Utility > Terrain tab did not become active after a real framebuffer click."
        }
        Write-Output "[pass] Utility > Terrain activated through a real packaged UI click"
        Set-HenkaAutomationForeground -Handle $mainWindowHandle
        Start-Sleep -Milliseconds 350
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $terrainUiBeforeScreenshotPath `
            -Description "Packaged Terrain Utility before Create Terrain"

        $terrainCreateMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Terrain Create control: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)\.'
        if ($null -eq $terrainCreateMatch) {
            throw "The visible Create Terrain control geometry was not reported after opening Utility > Terrain."
        }
        $terrainCreateX = [double]$terrainCreateMatch.Groups[1].Value
        $terrainCreateY = [double]$terrainCreateMatch.Groups[2].Value
        $terrainCreateWidth = [double]$terrainCreateMatch.Groups[3].Value
        $terrainCreateHeight = [double]$terrainCreateMatch.Groups[4].Value
        Assert-FramebufferRect `
            -Name "Create Terrain control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $terrainCreateX `
            -Y $terrainCreateY `
            -Width $terrainCreateWidth `
            -Height $terrainCreateHeight

        $terrainCreateOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($terrainCreateX + $terrainCreateWidth * 0.5) `
            -FramebufferY ($terrainCreateY + $terrainCreateHeight * 0.5)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'Terrain authoring content created\.' `
                -StartingOffset $terrainCreateOffset `
                -TimeoutMilliseconds 10000)) {
            throw "The visible Create Terrain button did not create terrain through the normal UI path."
        }
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'Terrain render: 16 bounded chunks resident;' `
                -StartingOffset $terrainCreateOffset `
                -TimeoutMilliseconds 10000)) {
            throw "The visible Create Terrain action did not initialize the production Terrain render path."
        }
        Write-Output "[pass] Create Terrain activated the production streaming, render, and collision path"
        Set-HenkaAutomationForeground -Handle $mainWindowHandle
        Start-Sleep -Milliseconds 700
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $terrainUiAfterScreenshotPath `
            -Description "Packaged Terrain Utility after Create Terrain"

        if ($TerrainStartupOnly) {
            Write-Output "[pass] Packaged product-startup/Terrain gate completed without entering the explicit showcase authoring suite"
            $packagedCheckSucceeded = $true
            return
        }

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
        Start-Sleep -Milliseconds 350
        $sourceRowTelemetryPattern = '^Native authoring source row: name='
        if (-not (Wait-FileContains `
                -Path $stdoutPath `
                -Pattern $sourceRowTelemetryPattern `
                -TimeoutMilliseconds 2000)) {
            throw "The packaged editor did not report imported source-row geometry for the bounded-telemetry check."
        }
        $sourceRowTelemetryCountBeforeIdle = @(
            Select-String -LiteralPath $stdoutPath -Pattern $sourceRowTelemetryPattern
        ).Count
        Start-Sleep -Milliseconds 500
        $sourceRowTelemetryCountAfterIdle = @(
            Select-String -LiteralPath $stdoutPath -Pattern $sourceRowTelemetryPattern
        ).Count
        if ($sourceRowTelemetryCountAfterIdle -ne $sourceRowTelemetryCountBeforeIdle) {
            throw (
                "Imported source-row layout telemetry grew while the selected editor layout was idle " +
                "($sourceRowTelemetryCountBeforeIdle -> $sourceRowTelemetryCountAfterIdle records).")
        }
        Write-Output "[pass] Imported source-row layout telemetry remained stable during a 500 ms idle interval"
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
            -Pattern 'Native authoring (source loaded:|topology bridge: name=.+ source_state=HENKA_NATIVE_EDITABLE_SOURCE\.|startup restore: name=.+ source_state=HENKA_NATIVE_EDITABLE_SOURCE\.|startup restore fallback: name=.+ source_state=HENKA_NATIVE_EDITABLE_SOURCE fallback=IMPORTED_FIXTURE\.)' `
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
        $opticalGeometryStartingOffset = Get-FileLengthSafe -Path $stdoutPath
        $opticalGeometryObserved = $false
        for ($opticalScrollAttempt = 0;
             $opticalScrollAttempt -lt 8 -and
             -not $opticalGeometryObserved;
             ++$opticalScrollAttempt) {
            Scroll-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($detailsX + [Math]::Min(120.0, [Math]::Max(24.0, $detailsWidth - 80.0))) `
                -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                -WheelDelta -1
            $opticalGeometryObserved = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring optical material controls:" `
                -StartingOffset $opticalGeometryStartingOffset `
                -TimeoutMilliseconds 1500
        }
        if (-not $opticalGeometryObserved) {
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
        # The optical controls are below the general material controls. Return
        # to the top of the details flow before using the earlier material
        # geometry record; otherwise the click lands in the scrolled layout.
        Scroll-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($detailsX + [Math]::Min(120.0, [Math]::Max(24.0, $detailsWidth - 80.0))) `
            -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
            -WheelDelta 1
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
                # Automation wheel records use SDL-style notch units. The
                # editor maps one notch to 48 px; Win32's WHEEL_DELTA (120)
                # would skip past this row in one event.
                Scroll-FramebufferPointAndWaitForConsumption `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($detailsX + [Math]::Max(12.0, $detailsWidth - 18.0)) `
                    -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                    -WheelDelta -1 `
                    -TimeoutMilliseconds 3000
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
        if ($framebufferWidth -ne 1280 -or $framebufferHeight -ne 720) {
            throw "The compact modeling toolbar interaction must run at 1280x720; packaged framebuffer is $($framebufferWidth)x$($framebufferHeight)."
        }
        $toolbarFields = Get-HenkaModelingToolbarFields -Path $stdoutPath
        if ($null -eq $toolbarFields -or
            $toolbarFields.authoring -ne '1' -or
            $toolbarFields.compact -ne '1' -or
            $toolbarFields.options_expanded -ne '0' -or
            $toolbarFields.hidden_controls_zero -ne '1') {
            throw "The packaged compact toolbar did not report its collapsed, editable, no-hidden-hit-target state."
        }
        $toolbarBounds = Get-HenkaModelingToolbarRect -Fields $toolbarFields -Name 'bounds'
        $toolbarOptions = Get-HenkaModelingToolbarRect -Fields $toolbarFields -Name 'options'
        Assert-FramebufferRect `
            -Name "Collapsed modeling toolbar" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $toolbarBounds.X -Y $toolbarBounds.Y `
            -Width $toolbarBounds.Width -Height $toolbarBounds.Height
        Assert-FramebufferRect `
            -Name "Visible compact Options disclosure" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $toolbarOptions.X -Y $toolbarOptions.Y `
            -Width $toolbarOptions.Width -Height $toolbarOptions.Height
        foreach ($hiddenControl in @('orientation_rect', 'pivot_rect', 'snap', 'xray')) {
            $hiddenRect = Get-HenkaModelingToolbarRect -Fields $toolbarFields -Name $hiddenControl
            if ($hiddenRect.Width -ne 0.0 -or $hiddenRect.Height -ne 0.0) {
                throw "Collapsed toolbar exposed a hit target for '$hiddenControl'."
            }
        }
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path (Join-Path $logDir 'modeling-toolbar-collapsed-1280x720.png') `
            -Description "Packaged compact modeling toolbar with active state summary"

        $toolbarActionOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($toolbarOptions.X + ($toolbarOptions.Width * 0.5)) `
            -FramebufferY ($toolbarOptions.Y + ($toolbarOptions.Height * 0.5))
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC modeling_toolbar .*options_expanded=1' `
                -StartingOffset $toolbarActionOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The packaged Options disclosure did not open after a real framebuffer click."
        }
        $toolbarFields = Get-HenkaModelingToolbarFields -Path $stdoutPath
        if ($toolbarFields.options_expanded -ne '1' -or $toolbarFields.hidden_controls_zero -ne '0') {
            throw "Expanded compact toolbar state or visible option-control geometry was not reported."
        }
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path (Join-Path $logDir 'modeling-toolbar-expanded-1280x720.png') `
            -Description "Packaged compact modeling toolbar with Options expanded"

        foreach ($control in @(
                [pscustomobject]@{ Name = 'orientation_rect'; Field = 'orientation'; Value = 'Local'; Segment = 1 },
                [pscustomobject]@{ Name = 'pivot_rect'; Field = 'pivot'; Value = 'Individual'; Segment = 2 })) {
            $controlRect = Get-HenkaModelingToolbarRect -Fields $toolbarFields -Name $control.Name
            Assert-FramebufferRect `
                -Name "Expanded modeling toolbar $($control.Field) control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $controlRect.X -Y $controlRect.Y `
                -Width $controlRect.Width -Height $controlRect.Height
            $toolbarActionOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($controlRect.X + ($controlRect.Width * (($control.Segment + 0.5) / 3.0))) `
                -FramebufferY ($controlRect.Y + ($controlRect.Height * 0.5))
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC modeling_toolbar ' `
                    -StartingOffset $toolbarActionOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The packaged $($control.Field) click produced no editor-state report."
            }
            $toolbarFields = Get-HenkaModelingToolbarFields -Path $stdoutPath
            $actualValue = if ($control.Field -eq 'orientation') {
                $toolbarFields.orientation
            }
            else {
                $toolbarFields.pivot
            }
            if ($toolbarFields.authoring -ne '1' -or $actualValue -ne $control.Value) {
                throw "The packaged $($control.Field) click did not preserve the editable selection and set '$($control.Value)' (authoring=$($toolbarFields.authoring), actual=$actualValue)."
            }
        }

        $toolbarInitialSnap = [string]$toolbarFields.snap_enabled
        $toolbarInitialXRay = [string]$toolbarFields.xray_enabled
        $toolbarToggledSnap = if ($toolbarInitialSnap -eq '1') { '0' } else { '1' }
        $toolbarToggledXRay = if ($toolbarInitialXRay -eq '1') { '0' } else { '1' }
        foreach ($control in @(
                [pscustomobject]@{ Name = 'snap'; Field = 'snap_enabled'; Value = $toolbarToggledSnap },
                [pscustomobject]@{ Name = 'xray'; Field = 'xray_enabled'; Value = $toolbarToggledXRay })) {
            $controlRect = Get-HenkaModelingToolbarRect -Fields $toolbarFields -Name $control.Name
            Assert-FramebufferRect `
                -Name "Expanded modeling toolbar $($control.Name) control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $controlRect.X -Y $controlRect.Y `
                -Width $controlRect.Width -Height $controlRect.Height
            $toolbarActionOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($controlRect.X + ($controlRect.Width * 0.5)) `
                -FramebufferY ($controlRect.Y + ($controlRect.Height * 0.5))
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC modeling_toolbar ' `
                    -StartingOffset $toolbarActionOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The packaged $($control.Name) click produced no editor-state report."
            }
            $toolbarFields = Get-HenkaModelingToolbarFields -Path $stdoutPath
            $actualValue = if ($control.Field -eq 'snap_enabled') {
                $toolbarFields.snap_enabled
            }
            else {
                $toolbarFields.xray_enabled
            }
            if ($toolbarFields.authoring -ne '1' -or $actualValue -ne $control.Value) {
                throw "The packaged $($control.Name) click did not preserve the editable selection and toggle to '$($control.Value)' (authoring=$($toolbarFields.authoring), actual=$actualValue)."
            }
        }

        $toolbarOptions = Get-HenkaModelingToolbarRect -Fields $toolbarFields -Name 'options'
        $toolbarActionOffset = Get-FileLengthSafe -Path $stdoutPath
        $cleanToolbarStatePattern = 'HENKA_AUTOMATION_DIAGNOSTIC modeling_toolbar .*options_expanded=0 hidden_controls_zero=1.*orientation=World pivot=Median snap_enabled={0} xray_enabled={1}' -f $toolbarInitialSnap, $toolbarInitialXRay
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($toolbarOptions.X + ($toolbarOptions.Width * 0.5)) `
            -FramebufferY ($toolbarOptions.Y + ($toolbarOptions.Height * 0.5))
        $activeOptionsPattern = 'HENKA_AUTOMATION_DIAGNOSTIC modeling_toolbar .*options_expanded=0 hidden_controls_zero=1.*orientation=Local pivot=Individual snap_enabled={0} xray_enabled={1}' -f $toolbarToggledSnap, $toolbarToggledXRay
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern $activeOptionsPattern `
                -StartingOffset $toolbarActionOffset `
                -TimeoutMilliseconds 5000)) {
            throw "Collapsed Options did not preserve and disclose the active Local, Individual, Snap, and X-Ray states."
        }
        $toolbarFields = Get-HenkaModelingToolbarFields -Path $stdoutPath
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path (Join-Path $logDir 'modeling-toolbar-active-options-collapsed-1280x720.png') `
            -Description "Packaged compact toolbar showing active options while collapsed"

        $toolbarOptions = Get-HenkaModelingToolbarRect -Fields $toolbarFields -Name 'options'
        $toolbarActionOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($toolbarOptions.X + ($toolbarOptions.Width * 0.5)) `
            -FramebufferY ($toolbarOptions.Y + ($toolbarOptions.Height * 0.5))
        $activeOptionsPattern = 'HENKA_AUTOMATION_DIAGNOSTIC modeling_toolbar .*options_expanded=1.*orientation=Local pivot=Individual snap_enabled={0} xray_enabled={1}' -f $toolbarToggledSnap, $toolbarToggledXRay
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern $activeOptionsPattern `
                -StartingOffset $toolbarActionOffset `
                -TimeoutMilliseconds 5000)) {
            throw "Reopening Options did not restore the active orientation, pivot, Snap, and X-Ray values."
        }
        $toolbarFields = Get-HenkaModelingToolbarFields -Path $stdoutPath

        foreach ($control in @(
                [pscustomobject]@{ Name = 'orientation_rect'; Field = 'orientation'; Value = 'World'; Segment = 0 },
                [pscustomobject]@{ Name = 'pivot_rect'; Field = 'pivot'; Value = 'Median'; Segment = 0 })) {
            $controlRect = Get-HenkaModelingToolbarRect -Fields $toolbarFields -Name $control.Name
            $toolbarActionOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($controlRect.X + ($controlRect.Width * (($control.Segment + 0.5) / 3.0))) `
                -FramebufferY ($controlRect.Y + ($controlRect.Height * 0.5))
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern ('HENKA_AUTOMATION_DIAGNOSTIC modeling_toolbar .*{0}={1}' -f $control.Field, $control.Value) `
                    -StartingOffset $toolbarActionOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The packaged $($control.Field) control did not restore its default state."
            }
            $toolbarFields = Get-HenkaModelingToolbarFields -Path $stdoutPath
        }
        foreach ($control in @(
                [pscustomobject]@{ Name = 'snap'; Field = 'snap_enabled'; Value = $toolbarInitialSnap },
                [pscustomobject]@{ Name = 'xray'; Field = 'xray_enabled'; Value = $toolbarInitialXRay })) {
            $controlRect = Get-HenkaModelingToolbarRect -Fields $toolbarFields -Name $control.Name
            $toolbarActionOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($controlRect.X + ($controlRect.Width * 0.5)) `
                -FramebufferY ($controlRect.Y + ($controlRect.Height * 0.5))
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern ('HENKA_AUTOMATION_DIAGNOSTIC modeling_toolbar .*{0}={1}' -f $control.Field, $control.Value) `
                    -StartingOffset $toolbarActionOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The packaged $($control.Name) control did not restore its default state."
            }
            $toolbarFields = Get-HenkaModelingToolbarFields -Path $stdoutPath
        }

        $toolbarOptions = Get-HenkaModelingToolbarRect -Fields $toolbarFields -Name 'options'
        $toolbarActionOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($toolbarOptions.X + ($toolbarOptions.Width * 0.5)) `
            -FramebufferY ($toolbarOptions.Y + ($toolbarOptions.Height * 0.5))
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern $cleanToolbarStatePattern `
                -StartingOffset $toolbarActionOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The compact toolbar did not return to its clean collapsed state."
        }
        $toolbarFields = Get-HenkaModelingToolbarFields -Path $stdoutPath
        $toolbarSelect = Get-HenkaModelingToolbarRect -Fields $toolbarFields -Name 'select'
        Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName 'R'
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC modeling_toolbar .*options_expanded=0.*transform=Rotate' `
                -StartingOffset $toolbarActionOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The Rotate keyboard shortcut did not work while Options was collapsed."
        }
        $toolbarFields = Get-HenkaModelingToolbarFields -Path $stdoutPath
        $toolbarSelect = Get-HenkaModelingToolbarRect -Fields $toolbarFields -Name 'select'
        $toolbarActionOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($toolbarSelect.X + ($toolbarSelect.Width * 0.5)) `
            -FramebufferY ($toolbarSelect.Y + ($toolbarSelect.Height * 0.5))
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC modeling_toolbar .*options_expanded=0.*transform=Select' `
                -StartingOffset $toolbarActionOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The visible Select tool did not restore the core toolbar state after the shortcut check."
        }
        Write-Output "[pass] Packaged 1280x720 modeling toolbar opened, changed each option, preserved active values while collapsed/reopened, and retained the Rotate shortcut"
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
        # Use the real Scene View ray picker and retain its reported face identity.
        # The X+ control below is an intentional negative control for the known
        # adjacent-face collapse; the following Y+ control must safely commit on
        # the same still-selected face.
        $faceFrameExpectedReleaseRecord = [long][IO.File]::ReadAllLines($automationInputPath).Length + 2L
        $faceFrameLogOffset = Get-FileLengthSafe -Path $stdoutPath
        Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F"
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern ('HENKA_AUTOMATION_DIAGNOSTIC input record={0} type=key-up button=none release_consumed=0' -f $faceFrameExpectedReleaseRecord) `
                -StartingOffset $faceFrameLogOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The packaged Sandbox did not consume the object-frame key release before face picking."
        }
        $faceFrameConsumedPattern = 'HENKA_AUTOMATION_DIAGNOSTIC frame seq=[0-9]+ phase=events-polled record_available=0 consumed_seq={0} release_consumed=0 input_faulted=0' -f $faceFrameExpectedReleaseRecord
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern $faceFrameConsumedPattern `
                -StartingOffset $faceFrameLogOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The packaged Sandbox did not complete a frame after consuming the object-frame key release."
        }
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC face_projection ' `
                -StartingOffset $faceFrameLogOffset `
                -TimeoutMilliseconds 3000)) {
            throw "The packaged application did not report the requested face-projection diagnostic."
        }
        $faceProjectionMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern ('HENKA_AUTOMATION_DIAGNOSTIC face_projection id={0} status=projected local_x=[^ ]+ local_y=[^ ]+ local_z=[^ ]+ framebuffer_x=(?<x>-?[0-9]+(?:\.[0-9]+)?) framebuffer_y=(?<y>-?[0-9]+(?:\.[0-9]+)?) depth=(?<depth>-?[0-9]+(?:\.[0-9]+)?) viewport=(?<viewportX>[0-9]+),(?<viewportY>[0-9]+),(?<viewportWidth>[0-9]+),(?<viewportHeight>[0-9]+)' -f `
                $nativeFaceMoveCollapseTargetId)
        $faceProjectionUnavailable = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC face_projection status=unavailable reason=(?<reason>[^ ]+).*requested_face_id=(?<id>[0-9]+)'
        $nativeCollapseNegativeControlAvailable = $null -ne $faceProjectionMatch
        if (-not $nativeCollapseNegativeControlAvailable -and
            ($null -eq $faceProjectionUnavailable -or
             $faceProjectionUnavailable.Groups['reason'].Value -ne 'face-id-not-in-active-mesh' -or
             [uint32]$faceProjectionUnavailable.Groups['id'].Value -ne $nativeFaceMoveCollapseTargetId)) {
            $faceProjectionDetail = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC (?<detail>face_projection [^\r\n]+)'
            $detail = if ($null -ne $faceProjectionDetail) {
                $faceProjectionDetail.Groups['detail'].Value
            } else { 'missing projection result' }
            throw "The packaged face-projection diagnostic was neither the expected projected target nor an absent logical ID: $detail"
        }
        $nativeTargetFaceId = $null
        if ($nativeCollapseNegativeControlAvailable) {
            $invariantCulture = [Globalization.CultureInfo]::InvariantCulture
            $projectionNumberStyle = [Globalization.NumberStyles]::Float
            $targetFaceFramebufferX = [double]::Parse(
                $faceProjectionMatch.Groups['x'].Value, $projectionNumberStyle, $invariantCulture)
            $targetFaceFramebufferY = [double]::Parse(
                $faceProjectionMatch.Groups['y'].Value, $projectionNumberStyle, $invariantCulture)
            $projectedFaceDepth = [double]::Parse(
                $faceProjectionMatch.Groups['depth'].Value, $projectionNumberStyle, $invariantCulture)
            if ([int]$faceProjectionMatch.Groups['viewportX'].Value -ne $componentViewportX -or
                [int]$faceProjectionMatch.Groups['viewportY'].Value -ne $componentViewportY -or
                [int]$faceProjectionMatch.Groups['viewportWidth'].Value -ne $componentViewportWidth -or
                [int]$faceProjectionMatch.Groups['viewportHeight'].Value -ne $componentViewportHeight -or
                $targetFaceFramebufferX -lt $componentViewportX -or
                $targetFaceFramebufferX -ge ($componentViewportX + $componentViewportWidth) -or
                $targetFaceFramebufferY -lt $componentViewportY -or
                $targetFaceFramebufferY -ge ($componentViewportY + $componentViewportHeight) -or
                $projectedFaceDepth -lt 0.0 -or $projectedFaceDepth -gt 1.0) {
                throw "The projected collapse target $nativeFaceMoveCollapseTargetId is outside the active Scene View or behind the camera."
            }
            Write-Output ("[probe] projected Giraffe collapse target face {0} at ({1:N2},{2:N2}) depth={3:N6}; selection will come from the production ray picker" -f `
                $nativeFaceMoveCollapseTargetId, $targetFaceFramebufferX, $targetFaceFramebufferY, $projectedFaceDepth)
            $facePickOffsets = @(
                [pscustomobject]@{ X = 0.0; Y = 0.0 },
                [pscustomobject]@{ X = -2.0; Y = 0.0 },
                [pscustomobject]@{ X = 2.0; Y = 0.0 },
                [pscustomobject]@{ X = 0.0; Y = -2.0 },
                [pscustomobject]@{ X = 0.0; Y = 2.0 },
                [pscustomobject]@{ X = -2.0; Y = -2.0 },
                [pscustomobject]@{ X = 2.0; Y = -2.0 },
                [pscustomobject]@{ X = -2.0; Y = 2.0 },
                [pscustomobject]@{ X = 2.0; Y = 2.0 })
        }
        else {
            Write-Output ("[info] Runtime Giraffe mesh does not contain previously observed collapse face ID {0}; the deterministic mesh and authoring-operator regressions remain the X+ rejection authority. The packaged interaction will use a live picked face for its Y+ positive control." -f `
                $nativeFaceMoveCollapseTargetId)
            $targetFaceFramebufferX = $componentViewportX + ($componentViewportWidth * 0.5)
            $targetFaceFramebufferY = $componentViewportY + ($componentViewportHeight * 0.5)
            $facePickOffsets = @(
                [pscustomobject]@{ X = 0.0; Y = 0.0 },
                [pscustomobject]@{ X = -24.0; Y = 0.0 },
                [pscustomobject]@{ X = 24.0; Y = 0.0 },
                [pscustomobject]@{ X = 0.0; Y = -24.0 },
                [pscustomobject]@{ X = 0.0; Y = 24.0 },
                [pscustomobject]@{ X = -48.0; Y = 0.0 },
                [pscustomobject]@{ X = 48.0; Y = 0.0 },
                [pscustomobject]@{ X = 0.0; Y = -48.0 },
                [pscustomobject]@{ X = 0.0; Y = 48.0 })
        }
        Start-Sleep -Milliseconds 450
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path (Join-Path $logDir 'check_packaged_sandbox3d_face_pick_framed.png') `
            -Description "Packaged Giraffe framed before deterministic face picking"
        $nativeComponentPicked = $false
        $nativeLastPickedFaceId = $null
        $nativeFacePickProbeCount = 0
        $nativeFacePickProbeTotal = $facePickOffsets.Count
        $nativeMoveLogOffset = $null
        # Pick only through the production ray picker. When the diagnostic face
        # exists, require that exact logical identity; otherwise this remains an
        # ordinary packaged positive-control edit on a live Giraffe face.
        foreach ($facePickOffset in $facePickOffsets) {
            if ($nativeComponentPicked) { break }
            ++$nativeFacePickProbeCount
            $probeFramebufferX = $targetFaceFramebufferX + [double]$facePickOffset.X
            $probeFramebufferY = $targetFaceFramebufferY + [double]$facePickOffset.Y
            if ($probeFramebufferX -lt $componentViewportX -or
                $probeFramebufferX -ge ($componentViewportX + $componentViewportWidth) -or
                $probeFramebufferY -lt $componentViewportY -or
                $probeFramebufferY -ge ($componentViewportY + $componentViewportHeight)) {
                throw "The face-pick probe lies outside the current Scene View viewport."
            }
            $componentPickLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX $probeFramebufferX `
                -FramebufferY $probeFramebufferY
            $componentPickObserved = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring component picked:" `
                -StartingOffset $componentPickLogOffset `
                -TimeoutMilliseconds 1200
            $componentPickMatch = $null
            if ($componentPickObserved) {
                $componentPickMatch = Get-LastLogRegexMatch `
                    -Path $stdoutPath `
                    -Pattern 'Native authoring component picked: name=(?<name>.+) visual=(?<visual>.+) mode=(?<mode>face) active=(?<active>[0-9]+) selected=(?<selected>[0-9]+) source_state=(?<sourceState>.+)\.'
            if ($null -ne $componentPickMatch -and
                $componentPickMatch.Groups['name'].Value -eq 'Showcase Giraffe Anatomical Giraffe Study Primitive' -and
                $componentPickMatch.Groups['selected'].Value -eq '1' -and
                [uint32]$componentPickMatch.Groups['active'].Value -gt [uint32]0) {
                $nativeLastPickedFaceId = [uint32]$componentPickMatch.Groups['active'].Value
                if (-not $nativeCollapseNegativeControlAvailable -or
                    $nativeLastPickedFaceId -eq $nativeFaceMoveCollapseTargetId) {
                    $nativeTargetFaceId = $nativeLastPickedFaceId
                    $nativeComponentPicked = $true
                    $nativeMoveLogOffset = $componentPickLogOffset
                }
            }
            }
            Write-Output ("[probe] face-pick {0}/{1} at ({2:N2},{3:N2}) -> {4}" -f `
                $nativeFacePickProbeCount, $nativeFacePickProbeTotal, $probeFramebufferX, $probeFramebufferY, `
                $(if ($componentPickObserved -and $null -ne $componentPickMatch) {
                    "face $($componentPickMatch.Groups['active'].Value)"
                } else {
                    'no new face-hit record'
                }))
        }
        if (-not $nativeComponentPicked) {
            $targetDescription = if ($nativeCollapseNegativeControlAvailable) {
                "collapse target face $nativeFaceMoveCollapseTargetId"
            } else { 'a live Giraffe face' }
            throw "The real Scene View picker did not select $targetDescription after $nativeFacePickProbeCount bounded probes (last picked face: $nativeLastPickedFaceId)."
        }
        if ($nativeCollapseNegativeControlAvailable) {
            Write-Output ("[pass] Production Scene View picking selected the intended Giraffe collapse target face {0}" -f $nativeTargetFaceId)
        }
        else {
            Write-Output ("[pass] Production Scene View picker selected live Giraffe face {0} for the packaged Y+ positive control" -f $nativeTargetFaceId)
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
        Assert-FramebufferRect `
            -Name "Native authoring component-edit control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X ([double]$nativeMoveMatch.Groups[2].Value) `
            -Y ([double]$nativeMoveMatch.Groups[3].Value) `
            -Width 88.0 `
            -Height 24.0
        $nativePickedName = $componentPickMatch.Groups['name'].Value
        $nativeMoveControlName = $nativeMoveMatch.Groups[1].Value
        if ($nativeMoveControlName -ne $nativePickedName) {
            throw "The Move X+ control belongs to '$nativeMoveControlName', not the selected face owner '$nativePickedName'."
        }
        if ($nativeCollapseNegativeControlAvailable) {
            $nativeMoveXClickLogOffset = Get-FileLengthSafe -Path $stdoutPath
            $componentMoveEditCountBeforeX = @(Select-String `
                -LiteralPath $stdoutPath `
                -Pattern '^Native authoring workflow: component move edited ').Count
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ([double]$nativeMoveMatch.Groups[2].Value + 44.0) `
                -FramebufferY ([double]$nativeMoveMatch.Groups[3].Value + 12.0)
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern '^Native authoring component move: name=' `
                    -StartingOffset $nativeMoveXClickLogOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The packaged Move X+ negative control did not report an operation result."
            }
            $nativeMoveXResultMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring component move: name=(?<name>.+) result=(?<result>.+) mode=(?<mode>.+) selected_components=(?<selected>[0-9]+)\.'
            if ($null -eq $nativeMoveXResultMatch) {
                throw "The packaged Move X+ negative-control result could not be parsed."
            }
            if ($nativeMoveXResultMatch.Groups['name'].Value -ne $nativePickedName -or
                $nativeMoveXResultMatch.Groups['result'].Value -ne 'numeric range error' -or
                $nativeMoveXResultMatch.Groups['mode'].Value -ne 'Face' -or
                [int]$nativeMoveXResultMatch.Groups['selected'].Value -ne 1) {
                throw "The packaged Move X+ control returned result '$($nativeMoveXResultMatch.Groups['result'].Value)' for '$($nativeMoveXResultMatch.Groups['name'].Value)' in mode '$($nativeMoveXResultMatch.Groups['mode'].Value)' with $($nativeMoveXResultMatch.Groups['selected'].Value) selected components; expected numeric range rejection for the same single selected face."
            }
            $componentMoveEditCountAfterX = @(Select-String `
                -LiteralPath $stdoutPath `
                -Pattern '^Native authoring workflow: component move edited ').Count
            if ($componentMoveEditCountAfterX -ne $componentMoveEditCountBeforeX) {
                throw "The rejected packaged Move X+ candidate emitted successful component-move publication telemetry."
            }
            Write-Output ("[pass] Packaged Move X+ intentionally rejected the adjacent-face collapse for face owner '{0}' with numeric range error and no edit-publication telemetry." -f $nativePickedName)
        }

        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'Native authoring Move Y\+ control: name=' `
                -StartingOffset $nativeMoveLogOffset `
                -TimeoutMilliseconds 3000)) {
            throw "The packaged application did not report its current Move Y+ control geometry."
        }
        $nativeMoveYControlMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring Move Y\+ control: name=(?<name>.+) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=88.0 height=24.0\.'
        if ($null -eq $nativeMoveYControlMatch) {
            throw "The packaged Move Y+ control geometry could not be parsed."
        }
        $nativeMoveYControlX = [double]$nativeMoveYControlMatch.Groups["x"].Value
        $nativeMoveYControlY = [double]$nativeMoveYControlMatch.Groups["y"].Value
        $nativeMoveYClickLogOffset = Get-FileLengthSafe -Path $stdoutPath
        Assert-FramebufferRect `
            -Name "Native authoring Move Y+ control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMoveYControlX `
            -Y $nativeMoveYControlY `
            -Width 88.0 `
            -Height 24.0
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMoveYControlX + 44.0) `
            -FramebufferY ($nativeMoveYControlY + 12.0)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern '^Native authoring component move: name=' `
                -StartingOffset $nativeMoveYClickLogOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The real Move Y+ control did not report an operation result after its center click."
        }
        $nativeMoveYResultMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring component move: name=(?<name>.+) result=(?<result>.+) mode=(?<mode>.+) selected_components=(?<selected>[0-9]+)\.'
        if ($null -eq $nativeMoveYResultMatch) {
            throw "The Move Y+ operation result could not be parsed."
        }
        if ($nativeMoveYResultMatch.Groups["name"].Value -ne $nativePickedName -or
            $nativeMoveYResultMatch.Groups["result"].Value -ne "success" -or
            $nativeMoveYResultMatch.Groups["mode"].Value -ne "Face" -or
            [int]$nativeMoveYResultMatch.Groups["selected"].Value -ne 1) {
            throw "The real Move Y+ control returned $($nativeMoveYResultMatch.Groups['result'].Value) for '$($nativeMoveYResultMatch.Groups['name'].Value)' in mode $($nativeMoveYResultMatch.Groups['mode'].Value) with $($nativeMoveYResultMatch.Groups['selected'].Value) selected components; expected a successful edit on '$nativePickedName'."
        }
        $nativeMoveObserved = Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern "Native authoring workflow: component move edited" `
            -StartingOffset $nativeMoveYClickLogOffset `
            -TimeoutMilliseconds 10000
        if (-not $nativeMoveObserved) {
            throw "The successful Move Y+ edit did not reach the native authoring workflow publication boundary."
        }
        Write-Output "[pass] Move Y+ edited the same selected face through the packaged authoring workflow"
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
                -WheelDelta 1.0
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
        $connectedSelectionGeometryPattern = '^Native authoring connected selection control:'
        $connectedSelectionGeometryBefore = Get-LogPatternCount `
            -Path $stdoutPath `
            -Pattern $connectedSelectionGeometryPattern
        $faceDeleteGeometryPattern = '^Native authoring face delete control:'
        $faceDeleteGeometryBefore = Get-LogPatternCount `
            -Path $stdoutPath `
            -Pattern $faceDeleteGeometryPattern
        $connectedSelectionGeometryStable = $false
        $faceDeleteGeometryStable = $false
        for ($detailsScrollAttempt = 0;
            $detailsScrollAttempt -lt 12 -and
            (-not $connectedSelectionGeometryStable -or -not $faceDeleteGeometryStable);
            ++$detailsScrollAttempt) {
            Scroll-FramebufferPointAndWaitForConsumption `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($detailsX + [Math]::Max(12.0, $detailsWidth - 18.0)) `
                -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                -WheelDelta -1 `
                -TimeoutMilliseconds 3000

            if (-not $faceDeleteGeometryStable -and
                (Get-LogPatternCount -Path $stdoutPath -Pattern $faceDeleteGeometryPattern) -gt $faceDeleteGeometryBefore) {
                Assert-PackagedGeometryTelemetryStable `
                    -Description "Native Face delete control" `
                    -Pattern $faceDeleteGeometryPattern
                $faceDeleteGeometryStable = $true
            }
            if (-not $connectedSelectionGeometryStable -and
                (Get-LogPatternCount -Path $stdoutPath -Pattern $connectedSelectionGeometryPattern) -gt $connectedSelectionGeometryBefore) {
                Assert-PackagedGeometryTelemetryStable `
                    -Description "Native connected-selection control" `
                    -Pattern $connectedSelectionGeometryPattern
                $connectedSelectionGeometryStable = $true
            }
        }
        if (-not $connectedSelectionGeometryStable) {
            throw "Scrolling Object Details did not expose the native connected-selection control."
        }
        if (-not $faceDeleteGeometryStable) {
            throw "Scrolling Object Details did not expose the native Face delete control."
        }
        # Re-frame after the preceding profile/edge transactions, which may
        # leave the view target elsewhere while preserving the selected object.
        # Wait on the product-consumed key release and following event frame.
        $bevelFaceFrameExpectedReleaseRecord = [long][IO.File]::ReadAllLines($automationInputPath).Length + 2L
        $bevelFaceFrameLogOffset = Get-FileLengthSafe -Path $stdoutPath
        Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F"
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern ('HENKA_AUTOMATION_DIAGNOSTIC input record={0} type=key-up button=none release_consumed=0' -f $bevelFaceFrameExpectedReleaseRecord) `
                -StartingOffset $bevelFaceFrameLogOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The packaged Sandbox did not consume the frame key release before the Bevel face pick."
        }
        $bevelFaceFrameConsumedPattern = 'HENKA_AUTOMATION_DIAGNOSTIC frame seq=[0-9]+ phase=events-polled record_available=0 consumed_seq={0} release_consumed=0 input_faulted=0' -f $bevelFaceFrameExpectedReleaseRecord
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern $bevelFaceFrameConsumedPattern `
                -StartingOffset $bevelFaceFrameLogOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The packaged Sandbox did not complete a frame after the Bevel face-frame key."
        }
        # The framed center can cross the narrow neck or air between body parts.
        # Also probe the visibly broad torso while still requiring a real
        # Giraffe face identity from the production Scene View ray picker.
        $nativeFaceTorsoYOffset = $componentViewportHeight * 0.22
        $nativeFaceTorsoXOffset = $componentViewportWidth * 0.10
        $nativeFacePickOffsets = @(
            [pscustomobject]@{ X = 0.0; Y = 0.0 },
            [pscustomobject]@{ X = -2.0; Y = 0.0 },
            [pscustomobject]@{ X = 2.0; Y = 0.0 },
            [pscustomobject]@{ X = 0.0; Y = -2.0 },
            [pscustomobject]@{ X = 0.0; Y = 2.0 },
            [pscustomobject]@{ X = -2.0; Y = -2.0 },
            [pscustomobject]@{ X = 2.0; Y = -2.0 },
            [pscustomobject]@{ X = -2.0; Y = 2.0 },
            [pscustomobject]@{ X = 2.0; Y = 2.0 },
            [pscustomobject]@{ X = -$nativeFaceTorsoXOffset; Y = $nativeFaceTorsoYOffset },
            [pscustomobject]@{ X = 0.0; Y = $nativeFaceTorsoYOffset },
            [pscustomobject]@{ X = $nativeFaceTorsoXOffset; Y = $nativeFaceTorsoYOffset })
        $nativeFaceTargetX = $componentViewportX + ($componentViewportWidth * 0.5)
        $nativeFaceTargetY = $componentViewportY + ($componentViewportHeight * 0.5)
        $nativeFacePicked = $false
        $nativeBevelPickedFaceId = $null
        $nativeBevelFacePickCount = 0
        foreach ($facePickOffset in $nativeFacePickOffsets) {
            if ($nativeFacePicked) { break }
            ++$nativeBevelFacePickCount
            $nativeFacePickLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeFaceTargetX + [double]$facePickOffset.X) `
                -FramebufferY ($nativeFaceTargetY + [double]$facePickOffset.Y)
            $nativeFacePickObserved = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring component picked:" `
                -StartingOffset $nativeFacePickLogOffset `
                -TimeoutMilliseconds 1200
            if ($nativeFacePickObserved) {
                $nativeBevelFacePickMatch = Get-LastLogRegexMatch `
                    -Path $stdoutPath `
                    -Pattern 'Native authoring component picked: name=(?<name>.+) visual=(?<visual>.+) mode=(?<mode>face) active=(?<active>[0-9]+) selected=(?<selected>[0-9]+) source_state=(?<sourceState>.+)\.'
                if ($null -ne $nativeBevelFacePickMatch -and
                    $nativeBevelFacePickMatch.Groups['name'].Value -eq 'Showcase Giraffe Anatomical Giraffe Study Primitive' -and
                    $nativeBevelFacePickMatch.Groups['selected'].Value -eq '1' -and
                    [uint32]$nativeBevelFacePickMatch.Groups['active'].Value -gt [uint32]0) {
                    $nativeBevelPickedFaceId = [uint32]$nativeBevelFacePickMatch.Groups['active'].Value
                    $nativeFacePicked = $true
                }
            }
        }
        if (-not $nativeFacePicked) {
            throw "The user-facing Face mode did not select a Giraffe viewport face across the framed neck and torso before Bevel after $nativeBevelFacePickCount bounded probes."
        }
        Write-Output ("[pass] Production Scene View selected Giraffe face {0} before Bevel" -f $nativeBevelPickedFaceId)
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
        # Keep a bounded set of visible Giraffe screen regions for the later
        # Bevel and Face-delete fallback probes.  The primary Face selection
        # above is based on the current framed viewport center; these points
        # are only used if a selected imported face cannot accept Bevel.
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
            # outlive the pointer event. Start at the click's pre-recorded
            # offset and require the commit record followed by the refreshed
            # Flip geometry. Do not take another offset after observing the
            # commit: both records can be appended before the poller resumes.
            $nativeBevelObserved = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'Native authoring workflow: bevel operator edited[^\r\n]*\r?\n[\s\S]*?Native authoring priority face flip control: name=.* width=[-0-9.]+ height=24\.0\.' `
                -StartingOffset $bevelLogOffset `
                -TimeoutMilliseconds 15000
        }
        if (-not $nativeBevelObserved) {
            throw "The user-facing native bevel did not commit and re-report the current Face-mode Flip control."
        }
        Write-Output "[pass] User-facing topology selection and bevel changed the native showcase source"
        $nativeFlipMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring priority face flip control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=24.0\.'
        if ($null -eq $nativeFlipMatch) {
            throw "The native Face-mode Flip control geometry could not be parsed."
        }
        $nativeFlipWidth = [double]$nativeFlipMatch.Groups[4].Value
        $nativeFlipOffsetFractions = @(0.25, 0.50, 0.75)
        $nativeFlipObserved = $false
        for ($flipAttempt = 0; $flipAttempt -lt 5 -and -not $nativeFlipObserved; ++$flipAttempt) {
            $latestFlipMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring priority face flip control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=24.0\.'
            if ($null -ne $latestFlipMatch) {
                $nativeFlipX = [double]$latestFlipMatch.Groups[2].Value
                $nativeFlipY = [double]$latestFlipMatch.Groups[3].Value
                $nativeFlipWidth = [double]$latestFlipMatch.Groups[4].Value
            }
            Assert-FramebufferRect `
                -Name "Native authoring Flip control retry" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $nativeFlipX `
                -Y $nativeFlipY `
                -Width $nativeFlipWidth `
                -Height 24.0
            $flipXOffset = $nativeFlipWidth * $nativeFlipOffsetFractions[$flipAttempt % 3]
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
            -Pattern 'Native authoring priority face delete control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=24.0\.'
        if ($null -eq $nativeDeleteMatch) {
            throw "The native Face-mode delete control geometry could not be parsed."
        }
        $nativeDeleteX = [double]$nativeDeleteMatch.Groups[2].Value
        $nativeDeleteY = [double]$nativeDeleteMatch.Groups[3].Value
        $nativeDeleteWidth = [double]$nativeDeleteMatch.Groups[4].Value
        Assert-FramebufferRect `
            -Name "Native authoring Delete Faces control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeDeleteX `
            -Y $nativeDeleteY `
            -Width $nativeDeleteWidth `
            -Height 24.0
        $nativeDeleteObserved = $false
        $nativeDeleteOffsetFractions = @(0.20, 0.50, 0.80, 0.50, 0.50)
        $nativeDeleteOffsetRows = @(6.0, 12.0, 18.0, 6.0, 18.0)
        for ($deleteAttempt = 0;
             $deleteAttempt -lt $nativeDeleteOffsetFractions.Count -and
             -not $nativeDeleteObserved;
             ++$deleteAttempt) {
            $latestDeleteMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'Native authoring priority face delete control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=24.0\.'
            if ($null -ne $latestDeleteMatch) {
                $nativeDeleteX = [double]$latestDeleteMatch.Groups[2].Value
                $nativeDeleteY = [double]$latestDeleteMatch.Groups[3].Value
                $nativeDeleteWidth = [double]$latestDeleteMatch.Groups[4].Value
            }
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeDeleteX + $nativeDeleteWidth * $nativeDeleteOffsetFractions[$deleteAttempt]) `
                -FramebufferY ($nativeDeleteY + $nativeDeleteOffsetRows[$deleteAttempt])
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

        Write-Step "Selecting the Game work context through its visible segmented control"
        $toolsControl = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Scene View Tools control: x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+)\.'
        if ($null -eq $toolsControl) {
            throw "The Scene View header did not report the visible work-context control geometry."
        }
        $toolsX = [double]$toolsControl.Groups["x"].Value
        $toolsY = [double]$toolsControl.Groups["y"].Value
        $toolsWidth = [double]$toolsControl.Groups["width"].Value
        if ([Math]::Abs($toolsWidth - 62.0) -lt 0.1) {
            $contextWidth = 210.0
        }
        elseif ([Math]::Abs($toolsWidth - 58.0) -lt 0.1) {
            $contextWidth = 176.0
        }
        else {
            throw "The Scene View header reported unsupported Tools width $toolsWidth; refusing to guess context geometry."
        }
        $contextX = $toolsX - $contextWidth - 4.0
        $contextSegmentWidth = $contextWidth / 3.0
        $gameContextX = $contextX + $contextSegmentWidth
        $gameContextY = $toolsY
        Assert-FramebufferRect `
            -Name "Game work-context segment" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $gameContextX `
            -Y $gameContextY `
            -Width $contextSegmentWidth `
            -Height 22.0
        $groundRowPattern = '^Default scene Ground row: x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) selected=(?<selected>[01])\.'
        $groundRowMatch = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $groundRowPattern
        if ($null -eq $groundRowMatch) {
            throw "The real Ground row was not reported by the packaged Scene Objects hierarchy."
        }
        $groundRowX = [double]$groundRowMatch.Groups["x"].Value
        $groundRowY = [double]$groundRowMatch.Groups["y"].Value
        $groundRowWidth = [double]$groundRowMatch.Groups["width"].Value
        $groundRowHeight = [double]$groundRowMatch.Groups["height"].Value
        Assert-FramebufferRect `
            -Name "Product-native Ground Scene Objects row" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $groundRowX `
            -Y $groundRowY `
            -Width $groundRowWidth `
            -Height $groundRowHeight
        if ($groundRowMatch.Groups["selected"].Value -ne "1") {
            $groundSelectionLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($groundRowX + $groundRowWidth * 0.5) `
                -FramebufferY ($groundRowY + $groundRowHeight * 0.5)
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern '^Default scene Ground row: .* selected=1\.' `
                    -StartingOffset $groundSelectionLogOffset `
                    -TimeoutMilliseconds 3500)) {
                throw "The normal Scene Objects click did not select the product-native Ground object."
            }
            $groundRowMatch = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $groundRowPattern
        }
        if ($null -eq $groundRowMatch -or $groundRowMatch.Groups["selected"].Value -ne "1") {
            throw "The Game-context acceptance target is not the selected authored Ground object."
        }
        Write-Output "[info] Selected the visible Ground row for Showcase context; Ground is not used as a Game Authoring target"

        $gameContextLogOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($gameContextX + $contextSegmentWidth * 0.5) `
            -FramebufferY ($gameContextY + 11.0)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern '^Game authoring physics disclosure: name=Ground ' `
                -StartingOffset $gameContextLogOffset `
                -TimeoutMilliseconds 3500)) {
            throw "Selecting the visible Game context did not expose a fresh Game Authoring Physics disclosure."
        }
        $gameControlsLogOffset = $gameContextLogOffset
        $gamePhysicsDisclosure = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Game authoring physics disclosure: name=(?<name>Ground) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28.0 expanded=(?<expanded>[01])\.'
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
                    -Pattern 'Game authoring physics disclosure: name=Ground .* expanded=1\.' `
                        -StartingOffset $physicsDisclosureClickOffset `
                        -TimeoutMilliseconds 2500) {
                    $gamePhysicsExpanded = $true
                    $gameControlsLogOffset = $physicsDisclosureClickOffset
                    break
                }
            }
            if (-not $gamePhysicsExpanded) {
                throw "The Game Authoring Physics disclosure did not report expansion after bounded click retries."
            }
        }
        $gamePlayGeometryPattern = '^Game authoring play controls: name=Ground '
        $gameStepGeometryPattern = '^Game authoring step controls: name=Ground '
        $gamePlayGeometryBefore = Get-LogPatternCount `
            -Path $stdoutPath `
            -Pattern $gamePlayGeometryPattern
        $gameStepGeometryBefore = Get-LogPatternCount `
            -Path $stdoutPath `
            -Pattern $gameStepGeometryPattern
        $gamePlayGeometryStable = $gamePlayGeometryBefore -gt 0
        $gameStepGeometryStable = $gameStepGeometryBefore -gt 0
        if ($gamePlayGeometryStable) {
            Assert-PackagedGeometryTelemetryStable `
                -Description "Game Authoring Play control" `
                -Pattern $gamePlayGeometryPattern
        }
        if ($gameStepGeometryStable) {
            Assert-PackagedGeometryTelemetryStable `
                -Description "Game Authoring Step/Stop control" `
                -Pattern $gameStepGeometryPattern
        }
        for ($gameDetailsScrollAttempt = 0;
            $gameDetailsScrollAttempt -lt 12 -and
            (-not $gamePlayGeometryStable -or -not $gameStepGeometryStable);
            ++$gameDetailsScrollAttempt) {
            Scroll-FramebufferPointAndWaitForConsumption `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($detailsX + [Math]::Max(12.0, $detailsWidth - 18.0)) `
                -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                -WheelDelta -1 `
                -TimeoutMilliseconds 3000

            if (-not $gamePlayGeometryStable -and
                (Get-LogPatternCount -Path $stdoutPath -Pattern $gamePlayGeometryPattern) -gt $gamePlayGeometryBefore) {
                Assert-PackagedGeometryTelemetryStable `
                    -Description "Game Authoring Play control" `
                    -Pattern $gamePlayGeometryPattern
                $gamePlayGeometryStable = $true
            }
            if (-not $gameStepGeometryStable -and
                (Get-LogPatternCount -Path $stdoutPath -Pattern $gameStepGeometryPattern) -gt $gameStepGeometryBefore) {
                Assert-PackagedGeometryTelemetryStable `
                    -Description "Game Authoring Step/Stop control" `
                    -Pattern $gameStepGeometryPattern
                $gameStepGeometryStable = $true
            }
        }
        $gameAuthoringControlsAvailable = $gamePlayGeometryStable -and $gameStepGeometryStable
        if (-not $gameAuthoringControlsAvailable) {
            Write-Output "[info] The Showcase Ground helper has no Game Authoring Play/Step binding; the dedicated product-startup Add Cube lane exercises those controls on a canonical Scene Document object."
        }
        if ($gameAuthoringControlsAvailable) {
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $nativeAuthoringScreenshotPath `
            -Description "Packaged Game-context Physics authoring controls screenshot"
        $gamePlayMatch = $null
        $gamePlayMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Game authoring play controls: name=(?<name>Ground) trigger_x=(?<triggerX>[-0-9.]+) play_x=(?<playX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26.0 state=(?<state>[0-9]+)\.'
        if ($null -eq $gamePlayMatch) {
            throw "The Game Authoring Physics disclosure did not expose Play controls."
        }
        $gameStepMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Game authoring step controls: name=(?<name>.+) step_x=(?<stepX>[-0-9.]+) stop_x=(?<stopX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26.0\.'
        Assert-PackagedGeometryTelemetryStable `
            -Description "Game Authoring Physics disclosure" `
            -Pattern '^Game authoring physics disclosure:'
        Assert-PackagedGeometryTelemetryStable `
            -Description "Game Authoring Play control" `
            -Pattern '^Game authoring play controls:'
        Assert-PackagedGeometryTelemetryStable `
            -Description "Game Authoring Step/Stop control" `
            -Pattern '^Game authoring step controls:'
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
        $gameStepMatch = Get-LastLogRegexMatch -Path $stdoutPath -Pattern 'Game authoring step controls: name=(?<name>Ground) step_x=(?<stepX>[-0-9.]+) stop_x=(?<stopX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26.0\.'
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
        }

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
        $shadingNames = @(
            "Wireframe",
            "Solid",
            "Material Preview",
            "Rendered")

        for ($modeIndex = 0;
             $modeIndex -lt $shadingNames.Count;
             ++$modeIndex) {
            $modeControlPattern =
                '^Viewport shading control: mode=' +
                [Regex]::Escape($shadingNames[$modeIndex]) +
                ' x=(?<x>-?[0-9]+(?:\.[0-9]+)?) y=(?<y>-?[0-9]+(?:\.[0-9]+)?) width=(?<width>[0-9]+(?:\.[0-9]+)?) height=(?<height>[0-9]+(?:\.[0-9]+)?)\.'
            $modeControlMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $modeControlPattern
            if ($null -eq $modeControlMatch) {
                throw "The packaged application did not report authoritative geometry for viewport shading mode '$($shadingNames[$modeIndex])'."
            }
            $invariantCulture = [Globalization.CultureInfo]::InvariantCulture
            $numberStyle = [Globalization.NumberStyles]::Float
            $modeX = [double]::Parse($modeControlMatch.Groups['x'].Value, $numberStyle, $invariantCulture)
            $modeY = [double]::Parse($modeControlMatch.Groups['y'].Value, $numberStyle, $invariantCulture)
            $modeWidth = [double]::Parse($modeControlMatch.Groups['width'].Value, $numberStyle, $invariantCulture)
            $modeHeight = [double]::Parse($modeControlMatch.Groups['height'].Value, $numberStyle, $invariantCulture)
            Assert-FramebufferRect `
                -Name "Viewport shading $($shadingNames[$modeIndex]) control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $modeX `
                -Y $modeY `
                -Width $modeWidth `
                -Height $modeHeight
            $modeCenterX = $modeX + ($modeWidth * 0.5)
            $modeCenterY = $modeY + ($modeHeight * 0.5)

            $expectedModePattern =
                "Viewport shading state: mode=" +
                [Regex]::Escape($shadingNames[$modeIndex]) +
                "\."
            $modeObserved = $false
            $modeInputDownObserved = $false
            $modeInputUpObserved = $false
            for ($shadingAttempt = 0;
                 $shadingAttempt -lt 4 -and
                 -not ($modeObserved -and $modeInputDownObserved -and $modeInputUpObserved);
                 ++$shadingAttempt) {
                $modeActionLogOffset = Get-FileLengthSafe -Path $stdoutPath
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX $modeCenterX `
                    -FramebufferY $modeCenterY
                $modeObserved = Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $expectedModePattern `
                    -StartingOffset $modeActionLogOffset `
                    -TimeoutMilliseconds 1200
                $modeInputDownObserved = Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC input record=\d+ type=button-down button=left release_consumed=0' `
                    -StartingOffset $modeActionLogOffset `
                    -TimeoutMilliseconds 1200
                $modeInputUpObserved = Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC input record=\d+ type=button-up button=left release_consumed=1' `
                    -StartingOffset $modeActionLogOffset `
                    -TimeoutMilliseconds 1200
            }
            if (-not ($modeObserved -and $modeInputDownObserved -and $modeInputUpObserved)) {
                throw "Viewport shading mode could not be confirmed: $($shadingNames[$modeIndex])"
            }
            Write-Output "[pass] Packaged Sandbox consumed the complete click and read back authoritative shading state '$($shadingNames[$modeIndex])'."

            if ($shadingNames[$modeIndex] -eq "Material Preview" -or
                $shadingNames[$modeIndex] -eq "Rendered") {
                $renderCompletePattern = 'HENKA_AUTOMATION_DIAGNOSTIC frame seq=\d+ phase=render-complete'
                if (-not (Wait-FileContainsAfterOffset `
                        -Path $stdoutPath `
                        -Pattern $renderCompletePattern `
                        -StartingOffset $modeActionLogOffset `
                        -TimeoutMilliseconds 10000)) {
                    throw "The packaged renderer did not complete a frame after shading changed to '$($shadingNames[$modeIndex])'."
                }
                $nextRenderLogOffset = Get-FileLengthSafe -Path $stdoutPath
                if (-not (Wait-FileContainsAfterOffset `
                        -Path $stdoutPath `
                        -Pattern $renderCompletePattern `
                        -StartingOffset $nextRenderLogOffset `
                        -TimeoutMilliseconds 10000)) {
                    throw "The packaged renderer did not complete a subsequent frame while shading remained '$($shadingNames[$modeIndex])'."
                }

                if ($shadingNames[$modeIndex] -eq "Material Preview") {
                    $shadingScreenshotPath = $shadingMaterialPreviewScreenshotPath
                }
                else {
                    $shadingScreenshotPath = $shadingRenderedScreenshotPath
                }
                Save-WindowScreenshot `
                    -Handle $mainWindowHandle `
                    -Path $shadingScreenshotPath `
                    -Description "Packaged viewport after $($shadingNames[$modeIndex]) shading action"
                Assert-PathExists `
                    -Path $shadingScreenshotPath `
                    -Description "Packaged $($shadingNames[$modeIndex]) post-action viewport screenshot"
                Write-Output "[pass] Captured the packaged viewport after two completed render frames in '$($shadingNames[$modeIndex])' mode."
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
    # The production editor starts with the bounded NativeAsset default. Append
    # a unique suffix instead of queueing dozens of erase events through the
    # one-event-per-frame automation stream on a slow Debug renderer. The
    # click and text event still exercise the real editable asset-name field.
    $genericAssetNameSuffix = "_" + [Guid]::NewGuid().ToString("N").Substring(0, 8)
    $genericAssetName = "NativeAsset" + $genericAssetNameSuffix
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
    Start-Sleep -Milliseconds 600
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $genericAssetNameSuffix
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
        Start-Sleep -Milliseconds 600
        Send-HenkaAutomationText -EventPath $automationInputPath -Text $genericAssetNameSuffix
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
    $genericAssetManifestPath = Join-Path $automationUserDataRoot ("saves\" + $genericAssetName + ".asset")
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
        Stop-HenkaProcessTree -ProcessId $process.Id
        throw "The packaged sandbox did not exit within the expected time; its process tree was terminated."
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

    # Native authoring persistence is keyed to the imported showcase entity.
    # Normal product startup intentionally has no showcase entities, so this
    # relaunch must restore the same source context rather than expect the clean
    # default scene to recreate that entity implicitly.
    Write-Step "Checking persisted native authoring relaunch with the same showcase source"
    $startupRestoreCapture = Start-HenkaCapturedProcess `
        -FilePath $packagedExe `
        -WorkingDirectory $packageRoot `
        -Arguments $showcaseAuthoringLaunchArguments `
        -StdoutPath $startupRestoreStdoutPath `
        -StderrPath $startupRestoreStderrPath
    $startupRestoreProcess = $startupRestoreCapture.Process
    if (-not (Wait-FileContains `
            -Path $startupRestoreStdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC user_data_base_path=isolated' `
            -TimeoutMilliseconds 10000)) {
        throw "The persisted-authoring relaunch did not use the isolated automation user-data authority."
    }
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
        throw "A relaunch with the same showcase source did not restore the saved native source."
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
        throw "A relaunch with the same showcase source did not restore the saved native material sidecar."
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
        Stop-HenkaProcessTree -ProcessId $startupRestoreProcess.Id
        throw "The persisted native authoring relaunch did not close cleanly; its process tree was terminated."
    }
    Close-HenkaCapturedProcess -CapturedProcess $startupRestoreCapture
    $startupRestoreCapture = $null
    $startupRestoreProcess = $null
    Write-Output "[pass] Persisted native source and owned material restored after relaunch with the same showcase source"

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

    $authoringSaveDirectory = Join-Path $automationUserDataRoot "saves"
    $authoringSaveFiles = @()
    if (Test-Path -LiteralPath $authoringSaveDirectory -PathType Container) {
        $authoringSaveFiles = @(Get-ChildItem `
            -LiteralPath $authoringSaveDirectory `
            -File `
            -Filter "sandbox3d_authoring_*.hams" `
            -ErrorAction Stop)
    }
    if ($authoringSaveFiles.Count -eq 0) {
        throw "The packaged authoring workflow did not persist a native HAMS scene under its isolated user-data root."
    }

    $packageUserSnapshotAfter = @(Get-HenkaDirectorySnapshot -Path $packageUserRoot)
    if ([string]::Join("`n", $packageUserSnapshotBefore) -cne
        [string]::Join("`n", $packageUserSnapshotAfter)) {
        throw "The packaged workflow changed pre-existing package user data despite the isolated automation root."
    }
    Write-Output "[pass] Existing package user data remained byte-identical; authoring saves were isolated"
    Write-Output "[pass] Live workspace settings recovery persisted across relaunch"
    Write-Output "[pass] Packaged sandbox checks completed."
    $packagedCheckSucceeded = $true
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
    if ($null -eq $previousAutomationDiagnostics) {
        Remove-Item Env:HENKA_AUTOMATION_DIAGNOSTICS -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_DIAGNOSTICS = $previousAutomationDiagnostics
    }
    if ($null -eq $previousAutomationDiagnosticFaceId) {
        Remove-Item Env:HENKA_AUTOMATION_DIAGNOSTIC_FACE_ID -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_DIAGNOSTIC_FACE_ID = $previousAutomationDiagnosticFaceId
    }
    if ($null -eq $previousAutomationUserDataBasePath) {
        Remove-Item Env:HENKA_AUTOMATION_USER_DATA_BASE_PATH -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_USER_DATA_BASE_PATH = $previousAutomationUserDataBasePath
    }
    if ($null -ne $startupRestoreCapture) {
        Close-HenkaCapturedProcess -CapturedProcess $startupRestoreCapture
    }
    if ($null -ne $capturedProcess) {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
    }
    elseif ($process -ne $null) {
        if (-not $process.HasExited) {
            Stop-HenkaProcessTree -ProcessId $process.Id
        }
        $process.Dispose()
    }

    if (Test-Path -LiteralPath $automationUserDataRoot -PathType Container) {
        if (-not $packagedCheckSucceeded) {
            Write-Warning "Preserving the isolated package user-data tree because this validation did not complete successfully."
        }
        else {
            try {
                $expectedParent = [IO.Path]::GetFullPath($logDir).TrimEnd([IO.Path]::DirectorySeparatorChar)
                $candidatePath = [IO.Path]::GetFullPath($automationUserDataRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
                $candidateItem = Get-Item -LiteralPath $candidatePath -Force -ErrorAction Stop
                if ([IO.Path]::GetDirectoryName($candidatePath).TrimEnd([IO.Path]::DirectorySeparatorChar) -ine $expectedParent -or
                    ($candidateItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                    throw "The generated user-data path no longer matches its exact safe parent or is a reparse point."
                }

                $packageProcesses = @(Get-CimInstance Win32_Process -Filter "Name='HenkaSandbox3D.exe'")
                $activePackageProcesses = @($packageProcesses |
                    Where-Object {
                        -not $_.ExecutablePath -or
                        [IO.Path]::GetFullPath($_.ExecutablePath) -ieq [IO.Path]::GetFullPath($packagedExe)
                    })
                if ($activePackageProcesses.Count -ne 0) {
                    throw "A packaged Sandbox process remains active; preserving its user-data tree."
                }

                $generatedChildren = @(Get-ChildItem -LiteralPath $candidatePath -Force -Recurse -ErrorAction Stop)
                if (@($generatedChildren | Where-Object { ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 }).Count -ne 0) {
                    throw "A reparse point appeared inside the generated user-data tree; preserving it."
                }
                Remove-Item -LiteralPath $candidatePath -Recurse -Force -ErrorAction Stop
                if (Test-Path -LiteralPath $candidatePath) {
                    throw "The generated isolated package user-data tree still exists after cleanup."
                }
                Write-Output "[pass] Retired the exact consumed isolated package user-data tree"
            }
            catch {
                Write-Warning ("Could not safely retire the consumed isolated package user-data tree; preserved it: " + $_.Exception.Message)
            }
        }
    }
}
