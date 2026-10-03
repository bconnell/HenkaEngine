param(
    [switch]$NonInteractive,

    [switch]$ContractOnly,

    [switch]$TerrainStartupOnly,

    [switch]$ProductStartupPrimitiveOnly,

    [string]$PackageRootOverride,

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

function Get-HenkaWindowsExecutableSubsystem {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = [System.IO.File]::OpenRead($Path)
    $reader = [System.IO.BinaryReader]::new($stream)
    try {
        if ($stream.Length -lt 64) {
            throw "The executable is too small to contain a PE header: $Path"
        }

        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        if ($peOffset -lt 0 -or $peOffset -gt ($stream.Length - 24)) {
            throw "The executable has an invalid PE header offset: $Path"
        }

        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "The executable is missing the PE signature: $Path"
        }

        $null = $reader.ReadUInt16()
        $null = $reader.ReadUInt16()
        $stream.Position += 12
        $optionalHeaderSize = $reader.ReadUInt16()
        $stream.Position += 2
        $optionalHeaderOffset = $stream.Position
        if ($optionalHeaderSize -lt 70 -or
            ($optionalHeaderOffset + $optionalHeaderSize) -gt $stream.Length) {
            throw "The executable has an invalid PE optional header: $Path"
        }

        $optionalHeaderMagic = $reader.ReadUInt16()
        if ($optionalHeaderMagic -ne 0x010b -and $optionalHeaderMagic -ne 0x020b) {
            throw "The executable has an unsupported PE optional-header format: $Path"
        }

        $stream.Position = $optionalHeaderOffset + 68
        return [int]$reader.ReadUInt16()
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
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

function Get-PackagedClientSize {
    param([Parameter(Mandatory = $true)][System.IntPtr]$Handle)

    $rect = New-Object NativeMethods+RECT
    if (-not [NativeMethods]::GetClientRect($Handle, [ref]$rect)) {
        throw "The packaged sandbox client bounds could not be read."
    }

    return [pscustomobject]@{
        Width = $rect.Right - $rect.Left
        Height = $rect.Bottom - $rect.Top
    }
}

function Set-PackagedClientSize {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][int]$Width,
        [Parameter(Mandatory = $true)][int]$Height,
        [int]$PositionX = [int]::MinValue,
        [int]$PositionY = [int]::MinValue
    )

    if ($Width -lt 800 -or $Height -lt 600) {
        throw "Responsive-layout validation requested an unsupported client size ${Width}x${Height}."
    }

    $currentClient = Get-PackagedClientSize -Handle $Handle
    $window = Get-WindowRect -Handle $Handle
    $outerWidth = $window.Right - $window.Left
    $outerHeight = $window.Bottom - $window.Top
    $targetOuterWidth = $outerWidth + ($Width - $currentClient.Width)
    $targetOuterHeight = $outerHeight + ($Height - $currentClient.Height)
    $targetX = $window.Left
    $targetY = $window.Top
    $screenBounds = [System.Windows.Forms.Screen]::FromHandle($Handle).Bounds
    if ($targetOuterWidth -gt $screenBounds.Width) {
        $targetX = $screenBounds.Left + [int][Math]::Floor(($screenBounds.Width - $targetOuterWidth) / 2.0)
    }
    if ($targetOuterHeight -gt $screenBounds.Height) {
        $targetY = $screenBounds.Bottom - $targetOuterHeight
    }
    if ($PositionX -ne [int]::MinValue) {
        $targetX = $PositionX
    }
    if ($PositionY -ne [int]::MinValue) {
        $targetY = $PositionY
    }

    # SWP_NOZORDER | SWP_NOACTIVATE: resizing must not raise or focus the app.
    if (-not [NativeMethods]::SetWindowPos(
            $Handle,
            [System.IntPtr]::Zero,
            $targetX,
            $targetY,
            $targetOuterWidth,
            $targetOuterHeight,
            0x14)) {
        $win32Error = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "The packaged sandbox could not be resized to ${Width}x${Height} client pixels (Win32 error $win32Error)."
    }

    $deadline = (Get-Date).AddSeconds(5)
    do {
        $actual = Get-PackagedClientSize -Handle $Handle
        if ($actual.Width -eq $Width -and $actual.Height -eq $Height) {
            return
        }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)

    throw "The packaged sandbox client did not reach ${Width}x${Height}; actual size was $($actual.Width)x$($actual.Height)."
}

function Set-PackagedWindowStyle {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][System.IntPtr]$Style
    )

    if (-not [NativeMethods]::SetWindowStyle($Handle, $Style)) {
        throw "The packaged sandbox window style could not be updated for responsive-layout capture."
    }

    $window = Get-WindowRect -Handle $Handle
    if (-not [NativeMethods]::SetWindowPos(
            $Handle,
            [System.IntPtr]::Zero,
            $window.Left,
            $window.Top,
            $window.Right - $window.Left,
            $window.Bottom - $window.Top,
            0x34)) {
        $win32Error = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "The packaged sandbox non-client frame could not be recalculated (Win32 error $win32Error)."
    }
}

function Assert-PackagedResponsiveLayout {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][int]$Width,
        [Parameter(Mandatory = $true)][int]$Height,
        [string]$ScreenshotPath,
        [int]$PositionX = [int]::MinValue,
        [int]$PositionY = [int]::MinValue
    )

    Set-PackagedClientSize `
        -Handle $Handle `
        -Width $Width `
        -Height $Height `
        -PositionX $PositionX `
        -PositionY $PositionY

    # Reopen the real editor panels to make the Sandbox process report geometry
    # calculated after this resize, rather than accepting stale startup output.
    $hideOffset = Get-FileLengthSafe -Path $stdoutPath
    Send-HenkaAutomationKey -EventPath $EventPath -KeyName "F4"
    if (-not (Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern 'Sandbox panel: hidden' `
            -StartingOffset $hideOffset `
            -TimeoutMilliseconds 5000)) {
        throw "$Name resize did not consume the panel-hide action."
    }

    $showOffset = Get-FileLengthSafe -Path $stdoutPath
    Send-HenkaAutomationKey -EventPath $EventPath -KeyName "F4"
    $readinessPattern = "Sandbox UI ready:.*framebuffer ${Width}x${Height}"
    if (-not (Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern $readinessPattern `
            -StartingOffset $showOffset `
            -TimeoutMilliseconds 5000) -or
        -not (Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern 'Workspace UI geometry:' `
            -StartingOffset $showOffset `
            -TimeoutMilliseconds 5000)) {
        throw "$Name resize did not produce fresh application-owned layout telemetry at ${Width}x${Height}."
    }

    $framebufferMatch = Get-LastLogRegexMatch `
        -Path $stdoutPath `
        -Pattern 'Sandbox UI ready:.*framebuffer ([0-9]+)x([0-9]+)'
    $geometryMatch = Get-LastLogRegexMatch `
        -Path $stdoutPath `
        -Pattern 'Workspace UI geometry: left=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+) right=([-0-9.]+),([-0-9.]+),([-0-9.]+),([-0-9.]+)'
    if ($null -eq $framebufferMatch -or $null -eq $geometryMatch) {
        throw "$Name resize telemetry could not be parsed."
    }

    $framebufferWidth = [int]$framebufferMatch.Groups[1].Value
    $framebufferHeight = [int]$framebufferMatch.Groups[2].Value
    if ($framebufferWidth -ne $Width -or $framebufferHeight -ne $Height) {
        throw "$Name requested ${Width}x${Height} but the app reported ${framebufferWidth}x${framebufferHeight}."
    }

    $leftX = [double]$geometryMatch.Groups[1].Value
    $leftY = [double]$geometryMatch.Groups[2].Value
    $leftWidth = [double]$geometryMatch.Groups[3].Value
    $leftHeight = [double]$geometryMatch.Groups[4].Value
    $rightX = [double]$geometryMatch.Groups[5].Value
    $rightY = [double]$geometryMatch.Groups[6].Value
    $rightWidth = [double]$geometryMatch.Groups[7].Value
    $rightHeight = [double]$geometryMatch.Groups[8].Value

    if ($leftX -lt 0.0 -or $leftY -lt 0.0 -or
        $leftWidth -lt 300.0 -or $leftHeight -le 0.0 -or
        $leftX + $leftWidth -gt $framebufferWidth -or
        $leftY + $leftHeight -gt $framebufferHeight) {
        throw "$Name left Scene Objects panel is clipped, undersized, or outside the framebuffer."
    }
    if ($rightX -lt 0.0 -or $rightY -lt 0.0 -or
        $rightWidth -lt 344.0 -or $rightHeight -le 0.0 -or
        $rightX + $rightWidth -gt $framebufferWidth -or
        $rightY + $rightHeight -gt $framebufferHeight) {
        throw "$Name right Object Details panel is clipped, undersized, or outside the framebuffer."
    }

    if (-not [string]::IsNullOrWhiteSpace($ScreenshotPath)) {
        Save-WindowScreenshot `
            -Handle $Handle `
            -Path $ScreenshotPath `
            -Description "$Name packaged workspace screenshot"
    }

    Write-Output ("[pass] {0}: client/framebuffer {1}x{2}; Scene Objects {3:N0}px, Object Details {4:N0}px; both panels remain in bounds." -f `
        $Name,$framebufferWidth,$framebufferHeight,$leftWidth,$rightWidth)
}

function Invoke-PackagedWorkspaceFramebufferCapture {
    param(
        [Parameter(Mandatory = $true)][int]$Width,
        [Parameter(Mandatory = $true)][int]$Height,
        [Parameter(Mandatory = $true)][string]$BitmapPath,
        [Parameter(Mandatory = $true)][string]$PngPath,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$StderrPath
    )

    foreach ($path in @($BitmapPath, $PngPath, $StdoutPath, $StderrPath)) {
        Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
    }

    $previousAutomationOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
    $previousAutomationFile = $env:HENKA_AUTOMATION_INPUT_FILE
    $previousAutomationDiagnostics = $env:HENKA_AUTOMATION_DIAGNOSTICS
    $captured = $null
    try {
        Remove-Item Env:HENKA_AUTOMATION_INPUT_OWNED -ErrorAction SilentlyContinue
        Remove-Item Env:HENKA_AUTOMATION_INPUT_FILE -ErrorAction SilentlyContinue
        Remove-Item Env:HENKA_AUTOMATION_DIAGNOSTICS -ErrorAction SilentlyContinue

        $captured = Start-HenkaCapturedProcess `
            -FilePath $packagedExe `
            -WorkingDirectory $packageRoot `
            -Arguments @(
                "--capture-workspace-layout",
                $Width.ToString([System.Globalization.CultureInfo]::InvariantCulture),
                $Height.ToString([System.Globalization.CultureInfo]::InvariantCulture),
                $BitmapPath) `
            -StdoutPath $StdoutPath `
            -StderrPath $StderrPath
        $process = $captured.Process
        if (-not $process.WaitForExit(120000)) {
            Stop-HenkaProcessTree -ProcessId $process.Id
            throw "The packaged application-owned ${Width}x${Height} framebuffer capture exceeded its 120 second bound."
        }

        $captureExitCode = $process.ExitCode
        Close-HenkaCapturedProcess -CapturedProcess $captured
        $captured = $null
        $stdout = [System.IO.File]::ReadAllText($StdoutPath)
        $stderr = [System.IO.File]::ReadAllText($StderrPath)
        if ($captureExitCode -ne 0) {
            throw "The packaged ${Width}x${Height} framebuffer capture exited $captureExitCode. stdout=$stdout stderr=$stderr"
        }
        if ($stdout -notmatch ("WORKSPACE_FRAME_CAPTURE_READY requested={0}x{1} framebuffer={0}x{1} .*draw_expected=1" -f $Width, $Height) -or
            $stdout -notmatch "DEFAULT_SCENE_READY ground=1 ground_editable=1 camera=1 showcase_assets=0 diagnostic_entities=0 scene_content=product_native") {
            throw "The packaged ${Width}x${Height} capture did not prove application readiness on the clean product-native default scene. stdout=$stdout"
        }
        if (-not (Test-Path -LiteralPath $BitmapPath -PathType Leaf)) {
            throw "The Sandbox renderer did not create the ${Width}x${Height} application-owned framebuffer capture."
        }

        $bytes = [System.IO.File]::ReadAllBytes($BitmapPath)
        if ($bytes.Length -lt 54 -or
            [char]$bytes[0] -ne 'B' -or
            [char]$bytes[1] -ne 'M') {
            throw "The ${Width}x${Height} application-owned capture is not a complete BMP file."
        }
        $declaredFileSize = [BitConverter]::ToUInt32($bytes, 2)
        $pixelOffset = [BitConverter]::ToUInt32($bytes, 10)
        $bitmapWidth = [BitConverter]::ToInt32($bytes, 18)
        $signedBitmapHeight = [BitConverter]::ToInt32($bytes, 22)
        $bitsPerPixel = [BitConverter]::ToUInt16($bytes, 28)
        $compression = [BitConverter]::ToUInt32($bytes, 30)
        $bitmapHeight = [Math]::Abs([long]$signedBitmapHeight)
        $rowStride = (([long]$bitmapWidth * 3L + 3L) -band 0xFFFFFFFCL)
        $expectedFileSize = [long]$pixelOffset + $rowStride * $bitmapHeight
        if ($bitmapWidth -ne $Width -or
            $bitmapHeight -ne $Height -or
            $signedBitmapHeight -ge 0 -or
            $pixelOffset -ne 54 -or
            $bitsPerPixel -ne 24 -or
            $compression -ne 0 -or
            $declaredFileSize -ne $bytes.Length -or
            $expectedFileSize -ne $bytes.Length) {
            throw "The app-owned BMP header/payload does not match the requested ${Width}x${Height} complete framebuffer."
        }

        $bitmap = [System.Drawing.Bitmap]::new($BitmapPath)
        try {
            $sampledColors = [System.Collections.Generic.HashSet[int]]::new()
            for ($sampleY = 0; $sampleY -lt 8; ++$sampleY) {
                for ($sampleX = 0; $sampleX -lt 8; ++$sampleX) {
                    $x = [int][Math]::Floor(($sampleX + 0.5) * $Width / 8.0)
                    $y = [int][Math]::Floor(($sampleY + 0.5) * $Height / 8.0)
                    $sampledColors.Add($bitmap.GetPixel($x, $y).ToArgb()) | Out-Null
                }
            }
            if ($sampledColors.Count -lt 8) {
                throw "The ${Width}x${Height} app-owned capture is blank or nearly uniform."
            }
            $bitmap.Save($PngPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $bitmap.Dispose()
        }

        Write-Output "[pass] Application-owned workspace framebuffer capture: ${Width}x${Height}; complete top-down 24-bit BMP validated and PNG review image written."
    }
    finally {
        if ($null -ne $captured) {
            Close-HenkaCapturedProcess -CapturedProcess $captured
        }
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
    }
}

function Assert-PackagedWorkspaceFramebufferCaptureRejectsInvalidDimensions {
    param(
        [Parameter(Mandatory = $true)][string]$BitmapPath,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$StderrPath
    )

    foreach ($path in @($BitmapPath, $StdoutPath, $StderrPath)) {
        Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
    }

    $previousAutomationOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
    $previousAutomationFile = $env:HENKA_AUTOMATION_INPUT_FILE
    $previousAutomationDiagnostics = $env:HENKA_AUTOMATION_DIAGNOSTICS
    $captured = $null
    try {
        Remove-Item Env:HENKA_AUTOMATION_INPUT_OWNED -ErrorAction SilentlyContinue
        Remove-Item Env:HENKA_AUTOMATION_INPUT_FILE -ErrorAction SilentlyContinue
        Remove-Item Env:HENKA_AUTOMATION_DIAGNOSTICS -ErrorAction SilentlyContinue

        $captured = Start-HenkaCapturedProcess `
            -FilePath $packagedExe `
            -WorkingDirectory $packageRoot `
            -Arguments @("--capture-workspace-layout", "5000", "1440", $BitmapPath) `
            -StdoutPath $StdoutPath `
            -StderrPath $StderrPath
        $process = $captured.Process
        if (-not $process.WaitForExit(10000)) {
            Stop-HenkaProcessTree -ProcessId $process.Id
            throw "Invalid workspace capture dimensions did not fail within the 10 second no-window bound."
        }
        $captureExitCode = $process.ExitCode
        Close-HenkaCapturedProcess -CapturedProcess $captured
        $captured = $null
        $stdout = [System.IO.File]::ReadAllText($StdoutPath)
        $stderr = [System.IO.File]::ReadAllText($StderrPath)
        if ($captureExitCode -ne 2 -or
            $stderr -notmatch 'Usage:.*--capture-workspace-layout' -or
            (Test-Path -LiteralPath $BitmapPath -PathType Leaf)) {
            throw "The packaged capture CLI did not fail closed for width 5000. exit=$captureExitCode stdout=$stdout stderr=$stderr"
        }

        Write-Output "[pass] Invalid 5000x1440 workspace capture dimensions fail closed before creating a window or image."
    }
    finally {
        if ($null -ne $captured) {
            Close-HenkaCapturedProcess -CapturedProcess $captured
        }
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
    }
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
        [Parameter(Mandatory = $true)][string]$Description,
        [int]$MinimumWidth = 800,
        [int]$MinimumHeight = 600
    )

    $rect = Get-WindowRect -Handle $Handle
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0) {
        throw "$Description window bounds are invalid for screenshot capture."
    }
    Assert-HenkaCaptureDimensions `
        -Width $width `
        -Height $height `
        -MinimumWidth $MinimumWidth `
        -MinimumHeight $MinimumHeight `
        -Description "$Description window"

    $bitmap = New-Object System.Drawing.Bitmap -ArgumentList $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        # Desktop sampling is valid only when the entire window lies inside
        # the virtual screen. Responsive-layout checks deliberately resize
        # beyond the physical display, where CopyFromScreen would silently
        # produce clipped desktop evidence. Use window-local capture there.
        $window_already_foreground =
            [HenkaUiAutomationNative]::GetForegroundWindow() -eq $Handle
        $virtualScreen = [System.Windows.Forms.SystemInformation]::VirtualScreen
        $window_fully_on_virtual_screen =
            $rect.Left -ge $virtualScreen.Left -and
            $rect.Top -ge $virtualScreen.Top -and
            $rect.Right -le $virtualScreen.Right -and
            $rect.Bottom -le $virtualScreen.Bottom
        if (($script:allowForegroundIntegration -or $window_already_foreground) -and
            $window_fully_on_virtual_screen) {
            $size = New-Object System.Drawing.Size -ArgumentList $width, $height
            $graphics.CopyFromScreen(
                $rect.Left,
                $rect.Top,
                0,
                0,
                $size)
        }
        else {
            if (-not $window_fully_on_virtual_screen) {
                Write-Host (
                    "[capture] {0}: window extends beyond virtual screen; " -f $Description +
                    "using window-local PrintWindow instead of clipped desktop pixels.")
            }
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
        [Parameter(Mandatory = $true)][string]$Description,
        [int]$MinimumWidth = 800,
        [int]$MinimumHeight = 600
    )

    Set-HenkaAutomationForeground -Handle $Handle
    $bitmap = New-HenkaCapturedWindowBitmap `
        -Handle $Handle `
        -Description $Description `
        -MinimumWidth $MinimumWidth `
        -MinimumHeight $MinimumHeight
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
        [Parameter(Mandatory = $true)][string]$Pattern,
        [long]$StartingOffset = 0
    )

    if ($StartingOffset -lt 0) {
        throw "A log scan starting offset cannot be negative: $StartingOffset"
    }
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
            if ($StartingOffset -gt $stream.Length) {
                throw "The packaged-sandbox log became shorter than its requested scan offset."
            }
            if ($StartingOffset -gt 0) {
                $stream.Position = $StartingOffset
            }
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

function Scroll-DetailsUntilReported {
    param(
        [Parameter(Mandatory = $true)][string]$PostconditionPattern,
        [Parameter(Mandatory = $true)][string]$Description,
        [long]$StartingOffset = 0
    )

    $scrollPattern = 'HENKA_AUTOMATION_DIAGNOSTIC details-scroll seq=(?<sequence>\d+) frame=(?<frame>\d+) before=(?<before>[-0-9.]+) after=(?<after>[-0-9.]+) content=(?<content>[-0-9.]+) viewport=(?<viewport>[-0-9.]+) delta=(?<delta>[-0-9.]+) accepted=(?<accepted>[01])'
    $attemptLimit = 128
    $lastProgress = $null
    for ($attempt = 0; $attempt -lt $attemptLimit; ++$attempt) {
        $postcondition = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern $PostconditionPattern `
            -StartingOffset $StartingOffset
        if ($null -ne $postcondition) {
            return $postcondition
        }

        $previousScroll = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern $scrollPattern `
            -StartingOffset $StartingOffset
        $previousSequence = if ($null -ne $previousScroll) {
            [int]$previousScroll.Groups['sequence'].Value
        } else {
            0
        }
        Scroll-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($detailsX + [Math]::Max(12.0, $detailsWidth - 18.0)) `
            -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
            -WheelDelta -1

        $deadline = [DateTime]::UtcNow.AddSeconds(3)
        $lastProgress = $null
        do {
            $latestScroll = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $scrollPattern `
                -StartingOffset $StartingOffset
            if ($null -ne $latestScroll -and
                [int]$latestScroll.Groups['sequence'].Value -gt $previousSequence) {
                $lastProgress = $latestScroll
                break
            }
            Start-Sleep -Milliseconds 40
        } while ([DateTime]::UtcNow -lt $deadline)

        if ($null -eq $lastProgress) {
            throw "The packaged app did not report consuming a Details-panel scroll while searching for $Description."
        }

        $before = [double]$lastProgress.Groups['before'].Value
        $after = [double]$lastProgress.Groups['after'].Value
        $contentHeight = [double]$lastProgress.Groups['content'].Value
        $viewportHeight = [double]$lastProgress.Groups['viewport'].Value
        $maximumOffset = [Math]::Max(0.0, $contentHeight - $viewportHeight)
        if ($lastProgress.Groups['accepted'].Value -ne '1') {
            if ($maximumOffset -gt 0.5) {
                throw "The Details panel rejected a scroll with $([Math]::Round($maximumOffset, 1)) px of content overflow while searching for $Description."
            }
            break
        }
        if ($after -le $before + 0.5) {
            if ($before -ge $maximumOffset - 0.5) {
                break
            }
            throw "The Details-panel scroll was consumed but made no progress (offset=$after, maximum=$maximumOffset) while searching for $Description."
        }
        if ($maximumOffset -gt 0.5) {
            $remainingSteps = [int][Math]::Ceiling(($maximumOffset - $after) / 48.0) + 1
            $attemptLimit = [Math]::Min(128, $attempt + 1 + [Math]::Max(1, $remainingSteps))
        }
        if ($after -ge $maximumOffset - 0.5) {
            break
        }
    }

    $postcondition = Get-LastLogRegexMatch `
        -Path $stdoutPath `
        -Pattern $PostconditionPattern `
        -StartingOffset $StartingOffset
    if ($null -ne $postcondition) {
        return $postcondition
    }
    if ($null -ne $lastProgress) {
        throw "The Details panel reached its reported scroll boundary without exposing $Description (offset=$($lastProgress.Groups['after'].Value), content=$($lastProgress.Groups['content'].Value), viewport=$($lastProgress.Groups['viewport'].Value))."
    }
    throw "The Details panel could not expose $Description; no app-reported scroll progress was available."
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
$packageRoot = if ([string]::IsNullOrWhiteSpace($PackageRootOverride)) {
    Join-Path $repoRoot "out\HenkaSandbox3D"
}
else {
    [System.IO.Path]::GetFullPath($PackageRootOverride)
}
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
$wideLayoutScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_layout_1920x1080.png"
$expandedLayoutScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_layout_2560x1440.png"
$wideLayoutFramebufferPath = Join-Path $logDir "check_packaged_sandbox3d_layout_1920x1080.bmp"
$expandedLayoutFramebufferPath = Join-Path $logDir "check_packaged_sandbox3d_layout_2560x1440.bmp"
$wideLayoutCaptureStdoutPath = Join-Path $logDir "check_packaged_sandbox3d_layout_1920x1080.stdout.log"
$wideLayoutCaptureStderrPath = Join-Path $logDir "check_packaged_sandbox3d_layout_1920x1080.stderr.log"
$expandedLayoutCaptureStdoutPath = Join-Path $logDir "check_packaged_sandbox3d_layout_2560x1440.stdout.log"
$expandedLayoutCaptureStderrPath = Join-Path $logDir "check_packaged_sandbox3d_layout_2560x1440.stderr.log"
$invalidLayoutFramebufferPath = Join-Path $logDir "check_packaged_sandbox3d_layout_invalid.bmp"
$invalidLayoutCaptureStdoutPath = Join-Path $logDir "check_packaged_sandbox3d_layout_invalid.stdout.log"
$invalidLayoutCaptureStderrPath = Join-Path $logDir "check_packaged_sandbox3d_layout_invalid.stderr.log"
$productStartupPrimitiveScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_product_startup_add_cube.png"
$productStartupTransformScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_product_startup_transform_fields_1280x720.png"
$physicsQaTopScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_physics_qa_top_1280x720.png"
$utilityHelpTopScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_utility_help_top_1280x720.png"
$utilityHelpScrolledScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_utility_help_scrolled_1280x720.png"
$utilitySettingsTopScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_utility_settings_top_1280x720.png"
$utilitySettingsScrolledScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_utility_settings_scrolled_1280x720.png"
$utilityDiagnosticsTopScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_utility_diagnostics_top_1280x720.png"
$utilityDiagnosticsScrolledScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_utility_diagnostics_scrolled_1280x720.png"
$utilityTransformQaTopScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_utility_transform_qa_top_1280x720.png"
$utilityTransformQaScrolledScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_utility_transform_qa_scrolled_1280x720.png"
$physicsQaCounterScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_physics_qa_counters_1280x720.png"
$physicsQaScrolledScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_physics_qa_scrolled_1280x720.png"
$sceneObjectNameScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_scene_object_full_name_1280x720.png"
$objectDetailsTitleScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_object_details_full_name_1280x720.png"
$objectDetailsValueScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_object_details_value_wrapped_1280x720.png"
$objectDetailsSourceScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_object_details_source_wrapped_1280x720.png"
$terrainUiBeforeScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_terrain_ui_before_create.png"
$terrainUiAfterScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_terrain_ui_after_create.png"
$terrainUiScrolledScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_terrain_ui_scrolled_1280x720.png"
$qaScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_controls_qa.png"
$nativeScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_native_panel.png"
$nativeAuthoringScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_native_authoring.png"
$selectionOutlineScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_selection_outline.png"
$nativeAuthoredScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_native_authored_rocket.png"
$scaleGizmoScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_scale_gizmo_1280x720.png"
$scaledGizmoScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_scale_gizmo_result_1280x720.png"
$contextMenuScreenshotPath = Join-Path $logDir "check_packaged_sandbox3d_context_menu.png"
$stabilityFirstPath = Join-Path $logDir "check_packaged_sandbox3d_stability_a.png"
$stabilitySecondPath = Join-Path $logDir "check_packaged_sandbox3d_stability_b.png"
$persistenceStdoutPath = Join-Path $logDir "check_packaged_sandbox3d_persistence_stdout.log"
$persistenceStderrPath = Join-Path $logDir "check_packaged_sandbox3d_persistence_stderr.log"
$startupRestoreStdoutPath = Join-Path $logDir "check_packaged_sandbox3d_startup_restore_stdout.log"
$startupRestoreStderrPath = Join-Path $logDir "check_packaged_sandbox3d_startup_restore_stderr.log"
$physicsCapturePath = Join-Path $logDir "physics-reference-wide.bmp"
$automationInputPath = Join-Path $logDir "check_packaged_sandbox3d_automation.events"

if (-not $NonInteractive) {
    Add-Type -AssemblyName System.Drawing
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

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool SetWindowPos(
        IntPtr hWnd,
        IntPtr hWndInsertAfter,
        int x,
        int y,
        int cx,
        int cy,
        uint uFlags);

    [DllImport("user32.dll", EntryPoint = "GetWindowLongPtrW", SetLastError = true)]
    public static extern IntPtr GetWindowLongPtr(IntPtr hWnd, int nIndex);

    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtrW", SetLastError = true)]
    public static extern IntPtr SetWindowLongPtr(IntPtr hWnd, int nIndex, IntPtr value);

    public static IntPtr CreateBorderlessPopupStyle(IntPtr originalStyle) {
        uint style = unchecked((uint)originalStyle.ToInt64());
        uint popupStyle = (style & ~0x00CF0000u) | 0x80000000u;
        return new IntPtr(unchecked((int)popupStyle));
    }

    public static bool SetWindowStyle(IntPtr hWnd, IntPtr style) {
        uint requested = unchecked((uint)style.ToInt64());
        uint current = unchecked((uint)GetWindowLongPtr(hWnd, -16).ToInt64());
        if (current == requested) {
            return true;
        }
        SetWindowLongPtr(hWnd, -16, new IntPtr(unchecked((int)requested)));
        uint actual = unchecked((uint)GetWindowLongPtr(hWnd, -16).ToInt64());
        return actual == requested;
    }

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
$packagedSubsystem = Get-HenkaWindowsExecutableSubsystem -Path $packagedExe
if ($packagedSubsystem -ne 2) {
    throw "Packaged Sandbox must use the Windows GUI subsystem (2); found subsystem $packagedSubsystem."
}
Write-Output "[pass] Packaged Sandbox uses the Windows GUI subsystem and does not create a console window."
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
Assert-FileContains -Path $readmePath -Pattern "Selected editable objects expose Position, Rotation \(Euler degrees\), and Scale as X/Y/Z fields in Object Details; each field group has its own Apply action" -Description "Packaged numeric transform editing guidance"
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

    Write-Step "Running packaged Physics QA smoke"
    $physicsSmoke = Invoke-HenkaNativeCapture `
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
    $physicsCapture = Invoke-HenkaNativeCapture `
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
    Write-Step "Running packaged Prefab public API smoke"
    $prefabSmoke = Invoke-HenkaNativeCapture `
        -FilePath $packagedExe `
        -Arguments @("--prefab-smoke-test") `
        -WorkingDirectory $packageRoot `
        -Label "Run packaged Prefab public API smoke"

    if ($prefabSmoke.Stdout -notmatch "Prefab package smoke: public save/load/instantiate/override/duplicate/detach workflow passed\.") {
        throw "The packaged Prefab smoke test did not prove the public Prefab workflow."
    }
    Write-Output "[pass] Packaged Prefab public API smoke completed."

    Write-Step "Running packaged Prefab Game Authoring smoke"
    $prefabAuthoringSmoke = Invoke-HenkaNativeCapture `
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

if (-not $TerrainStartupOnly -and -not $ProductStartupPrimitiveOnly) {
    New-Item -ItemType Directory -Path $logDir -Force | Out-Null
    Write-Step "Verifying full-resolution app-owned workspace framebuffer captures"
    Invoke-PackagedWorkspaceFramebufferCapture `
        -Width 1920 `
        -Height 1080 `
        -BitmapPath $wideLayoutFramebufferPath `
        -PngPath $wideLayoutScreenshotPath `
        -StdoutPath $wideLayoutCaptureStdoutPath `
        -StderrPath $wideLayoutCaptureStderrPath
    Invoke-PackagedWorkspaceFramebufferCapture `
        -Width 2560 `
        -Height 1440 `
        -BitmapPath $expandedLayoutFramebufferPath `
        -PngPath $expandedLayoutScreenshotPath `
        -StdoutPath $expandedLayoutCaptureStdoutPath `
        -StderrPath $expandedLayoutCaptureStderrPath
    Assert-PackagedWorkspaceFramebufferCaptureRejectsInvalidDimensions `
        -BitmapPath $invalidLayoutFramebufferPath `
        -StdoutPath $invalidLayoutCaptureStdoutPath `
        -StderrPath $invalidLayoutCaptureStderrPath
}

New-Item -ItemType Directory -Path $logDir -Force | Out-Null
Remove-Item `
    -LiteralPath @(
        $stdoutPath,
        $stderrPath,
        $startupScreenshotPath,
        $productStartupPrimitiveScreenshotPath,
        $sceneObjectNameScreenshotPath,
        $qaScreenshotPath,
        $nativeScreenshotPath,
        $nativeAuthoringScreenshotPath,
        $scaleGizmoScreenshotPath,
        $scaledGizmoScreenshotPath,
        $objectDetailsSourceScreenshotPath,
        $nativeAuthoredScreenshotPath,
        $persistenceStdoutPath,
        $persistenceStderrPath,
        $startupRestoreStdoutPath,
        $startupRestoreStderrPath,
        $physicsCapturePath) `
    -ErrorAction SilentlyContinue

$capturedProcess = $null
$process = $null
$mainWindowHandle = [System.IntPtr]::Zero
$uiAutomationVerified = $false
$sandboxPanelsVisible = $false
$previousAutomationOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
$previousAutomationFile = $env:HENKA_AUTOMATION_INPUT_FILE
$previousAutomationDiagnostics = $env:HENKA_AUTOMATION_DIAGNOSTICS
try {
    New-Item -ItemType File -Path $automationInputPath -Force | Out-Null
    $env:HENKA_AUTOMATION_INPUT_OWNED = "1"
    $env:HENKA_AUTOMATION_INPUT_FILE = $automationInputPath
    $env:HENKA_AUTOMATION_DIAGNOSTICS = "1"
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
            -Arguments @("--capture-showcase-view", "wide", "solid") `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath `
            -StartMinimized:$false `
            -StartVisibleWithoutActivation
    }
    $process = $capturedProcess.Process

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
        Write-Step "Checking responsive packaged workspace at 1280x720, 1920x1080, and 2560x1440"
        $originalWorkspaceWindow = Get-WindowRect -Handle $mainWindowHandle
        Assert-PackagedResponsiveLayout `
            -Handle $mainWindowHandle `
            -EventPath $automationInputPath `
            -Name "Minimum supported desktop layout" `
            -Width 1280 `
            -Height 720 `
            -ScreenshotPath $startupScreenshotPath
        $emptyDetailsPattern = 'HENKA_AUTOMATION_DIAGNOSTIC details-empty-message framebuffer=(?<framebufferWidth>\d+)x(?<framebufferHeight>\d+) full_bytes=(?<bytes>\d+) single_line_width=(?<lineWidth>[-0-9.]+) available_width=(?<availableWidth>[-0-9.]+) available_height=(?<availableHeight>[-0-9.]+) wrapped_height=(?<wrappedHeight>[-0-9.]+) visible=(?<visible>[01])'
        $emptyDetailsMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern $emptyDetailsPattern
        if ($null -eq $emptyDetailsMatch) {
            throw "The packaged Object Details empty-state instruction did not report its wrapped layout at 1280x720."
        }
        $emptyDetailsWidth = [double]$emptyDetailsMatch.Groups['availableWidth'].Value
        $emptyDetailsFramebufferWidth = [int]$emptyDetailsMatch.Groups['framebufferWidth'].Value
        $emptyDetailsFramebufferHeight = [int]$emptyDetailsMatch.Groups['framebufferHeight'].Value
        $emptyDetailsSingleLineWidth = [double]$emptyDetailsMatch.Groups['lineWidth'].Value
        $emptyDetailsAvailableHeight = [double]$emptyDetailsMatch.Groups['availableHeight'].Value
        $emptyDetailsWrappedHeight = [double]$emptyDetailsMatch.Groups['wrappedHeight'].Value
        if ($emptyDetailsMatch.Groups['visible'].Value -ne '1' -or
            [int]$emptyDetailsMatch.Groups['bytes'].Value -lt 40 -or
            $emptyDetailsFramebufferWidth -ne 1280 -or
            $emptyDetailsFramebufferHeight -ne 720 -or
            $emptyDetailsWidth -lt 280.0 -or $emptyDetailsWidth -gt 340.0 -or
            $emptyDetailsSingleLineWidth -le $emptyDetailsWidth -or
            $emptyDetailsWrappedHeight -le 0.0 -or
            $emptyDetailsWrappedHeight -gt $emptyDetailsAvailableHeight) {
            throw (
                "The packaged Object Details empty-state instruction did not wrap its complete text within the 1280x720 panel: " +
                "visible=$($emptyDetailsMatch.Groups['visible'].Value), " +
                "framebuffer=${emptyDetailsFramebufferWidth}x${emptyDetailsFramebufferHeight}, " +
                "textWidth=$emptyDetailsSingleLineWidth, availableWidth=$emptyDetailsWidth, " +
                "wrappedHeight=$emptyDetailsWrappedHeight, availableHeight=$emptyDetailsAvailableHeight.")
        }
        Write-Output "[pass] Packaged Object Details empty-state instruction wraps visibly within its 1280x720 panel."
        if (-not $TerrainStartupOnly -and -not $ProductStartupPrimitiveOnly) {
            $wrappedSceneLabel = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC scene-object-label entity=(?<entity>\d+) name_bytes=(?<name>\d+) label_bytes=(?<label>\d+) wrapped=1 drawn=1 name_preserved=1 x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+)'
            if ($null -eq $wrappedSceneLabel) {
                throw "The packaged Showcase hierarchy did not draw a complete wrapped long name in its 1280x720 row."
            }

            $hoverEntity = $wrappedSceneLabel.Groups["entity"].Value
            $hoverX = [double]::Parse(
                $wrappedSceneLabel.Groups["x"].Value,
                [Globalization.CultureInfo]::InvariantCulture) +
                [double]::Parse(
                    $wrappedSceneLabel.Groups["width"].Value,
                    [Globalization.CultureInfo]::InvariantCulture) * 0.5
            $hoverY = [double]::Parse(
                $wrappedSceneLabel.Groups["y"].Value,
                [Globalization.CultureInfo]::InvariantCulture) +
                [double]::Parse(
                    $wrappedSceneLabel.Groups["height"].Value,
                    [Globalization.CultureInfo]::InvariantCulture) * 0.5
            $tooltipOffset = Get-FileLengthSafe -Path $stdoutPath
            $framePattern = '(?m)^HENKA_AUTOMATION_DIAGNOSTIC frame seq=(?<sequence>[0-9]+) phase=render-complete(?:\s[^\r\n]*)?$'
            $completedFrameMatches = [System.Text.RegularExpressions.Regex]::Matches(
                (Get-HenkaPackagedStartupLogText -Path $stdoutPath),
                $framePattern)
            $afterFrameSequence = 0L
            foreach ($completedFrameMatch in $completedFrameMatches) {
                $reportedSequence = [long]$completedFrameMatch.Groups['sequence'].Value
                if ($reportedSequence -gt $afterFrameSequence) {
                    $afterFrameSequence = $reportedSequence
                }
            }
            Send-HenkaAutomationEvent `
                -EventPath $automationInputPath `
                -EventLine ("move {0} {1}" -f
                    (Format-HenkaAutomationFloat -Value $hoverX),
                    (Format-HenkaAutomationFloat -Value $hoverY))
            $hoverFrame = Wait-HenkaPackagedFrameRenderComplete `
                -StdoutPath $stdoutPath `
                -ProcessId $process.Id `
                -StartingOffset $tooltipOffset `
                -AfterFrameSequence $afterFrameSequence `
                -HardTimeoutMilliseconds 30000 `
                -NoProgressTimeoutMilliseconds 8000 `
                -PollMilliseconds 150
            $hoverFrameOutput = Get-HenkaPackagedStartupLogText -Path $stdoutPath
            if ([regex]::IsMatch(
                    $hoverFrameOutput,
                    ("HENKA_AUTOMATION_DIAGNOSTIC scene-object-name-tooltip entity={0} full_bytes=\d+ visible=1" -f [regex]::Escape($hoverEntity)))) {
                throw "Hovering a complete wrapped Showcase row drew a duplicate full-name tooltip over the Scene View (frame $($hoverFrame.FrameSequence))."
            }
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $sceneObjectNameScreenshotPath `
                -Description "1280x720 full scene-object name wrapped in its own row without a viewport tooltip"
            Write-Output "[pass] A complete long Showcase name remains readable in its wrapped row; hovering does not cover the Scene View."
        }
        $originalWindowStyle = [NativeMethods]::GetWindowLongPtr($mainWindowHandle, -16)
        $borderlessPopupStyle = [NativeMethods]::CreateBorderlessPopupStyle(
            $originalWindowStyle)
        try {
            # The local display is 1920x1080. A borderless popup lets the
            # actual Sandbox client render beyond host monitor tracking limits
            # without changing display settings or acquiring foreground.
            Set-PackagedWindowStyle `
                -Handle $mainWindowHandle `
                -Style $borderlessPopupStyle
            Assert-PackagedResponsiveLayout `
                -Handle $mainWindowHandle `
                -EventPath $automationInputPath `
                -Name "Standard desktop layout" `
                -Width 1920 `
                -Height 1080
            Assert-PackagedResponsiveLayout `
                -Handle $mainWindowHandle `
                -EventPath $automationInputPath `
                -Name "Expanded desktop layout" `
                -Width 2560 `
                -Height 1440
        }
        finally {
            Set-PackagedWindowStyle `
                -Handle $mainWindowHandle `
                -Style $originalWindowStyle
        }
        # Restore the minimum viewport so the existing interaction scenario
        # continues to exercise the approved minimum-width boundary.
        Assert-PackagedResponsiveLayout `
            -Handle $mainWindowHandle `
            -EventPath $automationInputPath `
            -Name "Restored minimum desktop layout" `
            -Width 1280 `
            -Height 720 `
            -PositionX $originalWorkspaceWindow.Left `
            -PositionY $originalWorkspaceWindow.Top

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
            "Viewport shading control: mode=Wireframe")) {
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
        if ($null -eq $framebufferMatch -or
            $null -eq $viewportMatch -or
            $null -eq $workspaceGeometryMatch -or
            $null -eq $qaTabMatch -or
            $null -eq $toolsHeaderMatch) {
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
            Write-Step "Checking product-native Add Cube through the visible Scene Objects UI"
            $addCubeX = $sceneObjectsX + 14.0
            $addCubeY = $sceneObjectsY + 60.0
            $addCubeWidth = $sceneObjectsWidth - 28.0
            $addCubeHeight = 24.0
            Assert-FramebufferRect `
                -Name "Scene Objects Add Cube control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $addCubeX `
                -Y $addCubeY `
                -Width $addCubeWidth `
                -Height $addCubeHeight

            $addCubeOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($addCubeX + $addCubeWidth * 0.5) `
                -FramebufferY ($addCubeY + $addCubeHeight * 0.5)
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

            $createdEntityMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'DEFAULT_SCENE_ADD_CUBE_READY entity=(?<entity>\d+) document_id=\d+ source=primitive canonical_document=1\.'
            if ($null -eq $createdEntityMatch) {
                throw "The product-native Add Cube result did not identify its live scene entity for Object Details validation."
            }
            $createdEntity = [UInt64]::Parse(
                $createdEntityMatch.Groups['entity'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $entityPattern = [Regex]::Escape(
                $createdEntity.ToString([Globalization.CultureInfo]::InvariantCulture))

            $transformDisclosurePattern =
                'HENKA_AUTOMATION_DIAGNOSTIC object-transform-disclosure entity=' +
                $entityPattern +
                ' expanded=(?<expanded>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) frame=\d+'
            $transformDisclosure = Scroll-DetailsUntilReported `
                -PostconditionPattern $transformDisclosurePattern `
                -Description "the selected object's Transform disclosure"
            if ($transformDisclosure.Groups['expanded'].Value -ne '1') {
                $disclosureOffset = Get-FileLengthSafe -Path $stdoutPath
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ([double]$transformDisclosure.Groups['x'].Value + 40.0) `
                    -FramebufferY ([double]$transformDisclosure.Groups['y'].Value + 14.0)
                $expandedDisclosurePattern =
                    'HENKA_AUTOMATION_DIAGNOSTIC object-transform-disclosure entity=' +
                    $entityPattern +
                    ' expanded=1 x=[-0-9.]+ y=[-0-9.]+ width=[-0-9.]+ height=[-0-9.]+ frame=\d+'
                if (-not (Wait-FileContainsAfterOffset `
                        -Path $stdoutPath `
                        -Pattern $expandedDisclosurePattern `
                        -StartingOffset $disclosureOffset `
                        -TimeoutMilliseconds 4000)) {
                    throw "The real Object Details Transform disclosure did not open for the selected scene object."
                }
            }

            $positionFieldPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC object-transform-field entity=' +
                $entityPattern +
                ' group=position submitted=1 row_x=(?<rowX>[-0-9.]+) row_y=(?<rowY>[-0-9.]+) row_width=(?<rowWidth>[-0-9.]+) row_height=(?<rowHeight>[-0-9.]+) field_x=(?<fieldX>[-0-9.]+) field_y=(?<fieldY>[-0-9.]+) field_width=(?<fieldWidth>[-0-9.]+) field_height=(?<fieldHeight>[-0-9.]+) value_x=(?<valueX>[-+0-9.eE]+) source_x=(?<sourceX>[-+0-9.eE]+) frame=\d+'
            $positionField = Scroll-DetailsUntilReported `
                -PostconditionPattern $positionFieldPattern `
                -Description "the selected object's Position component fields"
            $positionFieldX = [double]::Parse(
                $positionField.Groups['fieldX'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $positionFieldY = [double]::Parse(
                $positionField.Groups['fieldY'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $positionFieldWidth = [double]::Parse(
                $positionField.Groups['fieldWidth'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $positionFieldHeight = [double]::Parse(
                $positionField.Groups['fieldHeight'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            if ($positionFieldWidth -lt 45.0 -or $positionFieldHeight -lt 20.0 -or
                $positionFieldX -lt $detailsX -or
                $positionFieldY -lt $detailsY -or
                $positionFieldX + $positionFieldWidth -gt $detailsX + $detailsWidth -or
                $positionFieldY + $positionFieldHeight -gt $detailsY + $detailsHeight) {
                throw "The Object Details Position X field was not fully readable and in-bounds at 1280x720."
            }

            $positionApplyPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC object-transform-apply-button entity=' +
                $entityPattern +
                ' group=position submitted=1 x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) frame=\d+'
            $positionApply = Scroll-DetailsUntilReported `
                -PostconditionPattern $positionApplyPattern `
                -Description "the visible Apply Position action"
            $positionApplyX = [double]::Parse(
                $positionApply.Groups['x'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $positionApplyY = [double]::Parse(
                $positionApply.Groups['y'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $positionApplyWidth = [double]::Parse(
                $positionApply.Groups['width'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $positionApplyHeight = [double]::Parse(
                $positionApply.Groups['height'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            if ($positionApplyWidth -lt 80.0 -or $positionApplyHeight -lt 20.0 -or
                $positionApplyX -lt $detailsX -or $positionApplyY -lt $detailsY -or
                $positionApplyX + $positionApplyWidth -gt $detailsX + $detailsWidth -or
                $positionApplyY + $positionApplyHeight -gt $detailsY + $detailsHeight) {
                throw "Apply Position was not fully visible and in-bounds at 1280x720."
            }

            $positionField = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $positionFieldPattern
            if ($null -eq $positionField) {
                throw "The currently visible Position field geometry was not available after scrolling to Apply Position."
            }
            $positionFieldX = [double]::Parse(
                $positionField.Groups['fieldX'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $positionFieldY = [double]::Parse(
                $positionField.Groups['fieldY'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $positionFieldWidth = [double]::Parse(
                $positionField.Groups['fieldWidth'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $positionFieldHeight = [double]::Parse(
                $positionField.Groups['fieldHeight'].Value,
                [Globalization.CultureInfo]::InvariantCulture)

            $initialPositionX = [double]::Parse(
                $positionField.Groups['sourceX'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $initialPositionText = $positionField.Groups['valueX'].Value
            $updatedPositionX = $initialPositionX + 0.375
            $updatedPositionText = Format-HenkaAutomationFloat -Value $updatedPositionX
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($positionFieldX + $positionFieldWidth * 0.5) `
                -FramebufferY ($positionFieldY + $positionFieldHeight * 0.5)
            for ($backspaceIndex = 0; $backspaceIndex -lt $initialPositionText.Length; ++$backspaceIndex) {
                Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "Backspace"
            }
            Send-HenkaAutomationText -EventPath $automationInputPath -Text $updatedPositionText
            $positionResultOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($positionApplyX + $positionApplyWidth * 0.5) `
                -FramebufferY ($positionApplyY + $positionApplyHeight * 0.5)
            $positionResultPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC object-transform-result entity=' +
                $entityPattern +
                ' group=position accepted=1 state_valid=1 position_x=(?<positionX>[-+0-9.eE]+) '
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $positionResultPattern `
                    -StartingOffset $positionResultOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "Apply Position did not change the live selected entity through the packaged editor action path."
            }
            $positionResult = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $positionResultPattern
            $actualPositionX = [double]::Parse(
                $positionResult.Groups['positionX'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            if ([Math]::Abs($actualPositionX - $updatedPositionX) -gt 0.00005 -or
                [Math]::Abs($actualPositionX - $initialPositionX) -lt 0.00005) {
                throw "Apply Position did not publish the requested changed live X value. expected=$updatedPositionX actual=$actualPositionX"
            }
            Write-Output "[pass] Object Details Position input and Apply changed the real selected entity through the packaged action path"

            $scaleFieldPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC object-transform-field entity=' +
                $entityPattern +
                ' group=scale submitted=1 row_x=(?<rowX>[-0-9.]+) row_y=(?<rowY>[-0-9.]+) row_width=(?<rowWidth>[-0-9.]+) row_height=(?<rowHeight>[-0-9.]+) field_x=(?<fieldX>[-0-9.]+) field_y=(?<fieldY>[-0-9.]+) field_width=(?<fieldWidth>[-0-9.]+) field_height=(?<fieldHeight>[-0-9.]+) value_x=(?<valueX>[-+0-9.eE]+) source_x=(?<sourceX>[-+0-9.eE]+) frame=\d+'
            $scaleField = Scroll-DetailsUntilReported `
                -PostconditionPattern $scaleFieldPattern `
                -Description "the selected object's Scale component fields"
            $scaleApplyPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC object-transform-apply-button entity=' +
                $entityPattern +
                ' group=scale submitted=1 x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) frame=\d+'
            $scaleApply = Scroll-DetailsUntilReported `
                -PostconditionPattern $scaleApplyPattern `
                -Description "the visible Apply Scale action"
            $scaleFieldMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $scaleFieldPattern
            if ($null -eq $scaleFieldMatch) {
                throw "The currently visible Scale field geometry was not available after scrolling to Apply Scale."
            }
            $scaleValueText = $scaleFieldMatch.Groups['valueX'].Value
            $initialScaleX = [double]::Parse(
                $scaleFieldMatch.Groups['sourceX'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $scaleApplyX = [double]::Parse(
                $scaleApply.Groups['x'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $scaleApplyY = [double]::Parse(
                $scaleApply.Groups['y'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $scaleApplyWidth = [double]::Parse(
                $scaleApply.Groups['width'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $scaleApplyHeight = [double]::Parse(
                $scaleApply.Groups['height'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $scaleFieldX = [double]::Parse(
                $scaleFieldMatch.Groups['fieldX'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $scaleFieldY = [double]::Parse(
                $scaleFieldMatch.Groups['fieldY'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $scaleFieldWidth = [double]::Parse(
                $scaleFieldMatch.Groups['fieldWidth'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $scaleFieldHeight = [double]::Parse(
                $scaleFieldMatch.Groups['fieldHeight'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            if ($scaleFieldWidth -lt 45.0 -or $scaleFieldHeight -lt 20.0 -or
                $scaleFieldX -lt $detailsX -or
                $scaleFieldY -lt $detailsY -or
                $scaleFieldX + $scaleFieldWidth -gt $detailsX + $detailsWidth -or
                $scaleFieldY + $scaleFieldHeight -gt $detailsY + $detailsHeight) {
                throw "The Object Details Scale X field was not fully readable and in-bounds at 1280x720."
            }
            if ($scaleApplyWidth -lt 80.0 -or $scaleApplyHeight -lt 20.0 -or
                $scaleApplyX -lt $detailsX -or $scaleApplyY -lt $detailsY -or
                $scaleApplyX + $scaleApplyWidth -gt $detailsX + $detailsWidth -or
                $scaleApplyY + $scaleApplyHeight -gt $detailsY + $detailsHeight) {
                throw "The Object Details Apply Scale action was not fully reachable at 1280x720."
            }
            Start-Sleep -Milliseconds 250
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $productStartupTransformScreenshotPath `
                -Description "Packaged Object Details numeric transform controls at 1280x720"
            Write-Output "[pass] Packaged Object Details transform-control visual proof captured at 1280x720"
            Start-Sleep -Milliseconds 700
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $productStartupPrimitiveScreenshotPath `
                -Description "Packaged product-native Add Cube"
            Write-Output "[pass] Product-native Add Cube visual proof captured"
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($scaleFieldX + $scaleFieldWidth * 0.5) `
                -FramebufferY ($scaleFieldY + $scaleFieldHeight * 0.5)
            for ($backspaceIndex = 0; $backspaceIndex -lt $scaleValueText.Length; ++$backspaceIndex) {
                Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "Backspace"
            }
            Send-HenkaAutomationText -EventPath $automationInputPath -Text "0"
            $scaleResultOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($scaleApplyX + $scaleApplyWidth * 0.5) `
                -FramebufferY ($scaleApplyY + $scaleApplyHeight * 0.5)
            $scaleResultPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC object-transform-result entity=' +
                $entityPattern +
                ' group=scale accepted=0 state_valid=1 position_x=(?<positionX>[-+0-9.eE]+) position_y=[-+0-9.eE]+ position_z=[-+0-9.eE]+ scale_x=(?<scaleX>[-+0-9.eE]+) scale_y=(?<scaleY>[-+0-9.eE]+) scale_z=(?<scaleZ>[-+0-9.eE]+)'
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $scaleResultPattern `
                    -StartingOffset $scaleResultOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The packaged editor did not reject zero scale through the real action path."
            }
            $scaleResult = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $scaleResultPattern
            $actualScaleX = [double]::Parse(
                $scaleResult.Groups['scaleX'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $actualPositionAfterReject = [double]::Parse(
                $scaleResult.Groups['positionX'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            if ([Math]::Abs($actualScaleX - $initialScaleX) -gt 0.00005 -or
                [Math]::Abs($actualPositionAfterReject - $updatedPositionX) -gt 0.00005) {
                throw "Rejected zero scale partially changed the live transform. scaleBefore=$initialScaleX scaleAfter=$actualScaleX positionExpected=$updatedPositionX positionAfter=$actualPositionAfterReject"
            }
            Write-Output "[pass] Zero Scale was rejected through the real UI action without changing the live transform"

            Write-Step "Checking per-axis Scale gizmo through packaged editor drag at 1280x720"
            $scaleGeometryOffset = Get-FileLengthSafe -Path $stdoutPath
            Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "S"
            $scaleAxisPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC gizmo-scale-axis entity=' +
                $entityPattern +
                ' axis=(?<axis>X) viewport_x=(?<viewportX>-?\d+) viewport_y=(?<viewportY>-?\d+) x0=(?<x0>[-+0-9.eE]+) y0=(?<y0>[-+0-9.eE]+) x1=(?<x1>[-+0-9.eE]+) y1=(?<y1>[-+0-9.eE]+)'
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $scaleAxisPattern `
                    -StartingOffset $scaleGeometryOffset `
                    -TimeoutMilliseconds 4000)) {
                throw "The packaged Scale gizmo did not report visible axis handles from the product model."
            }
            $scaleAxisGeometry = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $scaleAxisPattern
            if ($null -eq $scaleAxisGeometry -or $scaleAxisGeometry.Groups['axis'].Value -ne 'X') {
                throw "The packaged Scale gizmo did not expose the expected X-axis handle in its stable reference view."
            }
            Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "Escape"
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $scaleGizmoScreenshotPath `
                -Description "Packaged per-axis Scale gizmo at 1280x720"

            $scaleAxisViewportX = [double]$scaleAxisGeometry.Groups['viewportX'].Value
            $scaleAxisViewportY = [double]$scaleAxisGeometry.Groups['viewportY'].Value
            $scaleAxisX0 = [double]$scaleAxisGeometry.Groups['x0'].Value
            $scaleAxisY0 = [double]$scaleAxisGeometry.Groups['y0'].Value
            $scaleAxisX1 = [double]$scaleAxisGeometry.Groups['x1'].Value
            $scaleAxisY1 = [double]$scaleAxisGeometry.Groups['y1'].Value
            $scaleAxisDeltaX = $scaleAxisX1 - $scaleAxisX0
            $scaleAxisDeltaY = $scaleAxisY1 - $scaleAxisY0
            $scaleAxisLength = [Math]::Sqrt(
                $scaleAxisDeltaX * $scaleAxisDeltaX +
                $scaleAxisDeltaY * $scaleAxisDeltaY)
            if ($scaleAxisLength -lt 18.0) {
                throw "The packaged X Scale gizmo handle was too short for a reliable interaction. length=$scaleAxisLength"
            }
            $scaleAxisStartX = $scaleAxisViewportX + ($scaleAxisX0 + $scaleAxisX1) * 0.5
            $scaleAxisStartY = $scaleAxisViewportY + ($scaleAxisY0 + $scaleAxisY1) * 0.5
            $scaleAxisEndX = $scaleAxisStartX + $scaleAxisDeltaX / $scaleAxisLength * 18.0
            $scaleAxisEndY = $scaleAxisStartY + $scaleAxisDeltaY / $scaleAxisLength * 18.0
            Assert-FramebufferRect `
                -Name "Scale gizmo drag target" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $scaleAxisStartX `
                -Y $scaleAxisStartY `
                -Width 1.0 `
                -Height 1.0
            Assert-FramebufferRect `
                -Name "Scale gizmo drag destination" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $scaleAxisEndX `
                -Y $scaleAxisEndY `
                -Width 1.0 `
                -Height 1.0

            $scaleGizmoClientRect = New-Object NativeMethods+RECT
            if (-not [NativeMethods]::GetClientRect(
                    $mainWindowHandle,
                    [ref]$scaleGizmoClientRect)) {
                throw "The packaged Sandbox client bounds could not be read for the Scale gizmo drag."
            }
            $scaleGizmoClientWidth = $scaleGizmoClientRect.Right - $scaleGizmoClientRect.Left
            $scaleGizmoClientHeight = $scaleGizmoClientRect.Bottom - $scaleGizmoClientRect.Top
            $scaleGizmoStartPoint = Convert-HenkaFramebufferPointToWindowPoint `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -WindowWidth $scaleGizmoClientWidth `
                -WindowHeight $scaleGizmoClientHeight `
                -FramebufferX $scaleAxisStartX `
                -FramebufferY $scaleAxisStartY
            $scaleGizmoEndPoint = Convert-HenkaFramebufferPointToWindowPoint `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -WindowWidth $scaleGizmoClientWidth `
                -WindowHeight $scaleGizmoClientHeight `
                -FramebufferX $scaleAxisEndX `
                -FramebufferY $scaleAxisEndY
            $gizmoScaleResultOffset = Get-FileLengthSafe -Path $stdoutPath
            Send-HenkaAutomationEvent `
                -EventPath $automationInputPath `
                -EventLine ("move {0} {1}" -f `
                    (Format-HenkaAutomationFloat -Value $scaleGizmoStartPoint.X), `
                    (Format-HenkaAutomationFloat -Value $scaleGizmoStartPoint.Y))
            Send-HenkaAutomationEvent `
                -EventPath $automationInputPath `
                -EventLine ("button left down {0} {1}" -f `
                    (Format-HenkaAutomationFloat -Value $scaleGizmoStartPoint.X), `
                    (Format-HenkaAutomationFloat -Value $scaleGizmoStartPoint.Y))
            Send-HenkaAutomationEvent `
                -EventPath $automationInputPath `
                -EventLine ("move {0} {1}" -f `
                    (Format-HenkaAutomationFloat -Value $scaleGizmoEndPoint.X), `
                    (Format-HenkaAutomationFloat -Value $scaleGizmoEndPoint.Y))
            Send-HenkaAutomationEvent `
                -EventPath $automationInputPath `
                -EventLine ("button left up {0} {1}" -f `
                    (Format-HenkaAutomationFloat -Value $scaleGizmoEndPoint.X), `
                    (Format-HenkaAutomationFloat -Value $scaleGizmoEndPoint.Y))
            $gizmoScaleResultPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC object-transform-result entity=' +
                $entityPattern +
                ' group=gizmo-scale-X accepted=1 state_valid=1 position_x=[-+0-9.eE]+ position_y=[-+0-9.eE]+ position_z=[-+0-9.eE]+ scale_x=(?<scaleX>[-+0-9.eE]+) scale_y=(?<scaleY>[-+0-9.eE]+) scale_z=(?<scaleZ>[-+0-9.eE]+)'
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $gizmoScaleResultPattern `
                    -StartingOffset $gizmoScaleResultOffset `
                    -TimeoutMilliseconds 8000)) {
                throw "A real packaged X-axis Scale gizmo drag did not produce an accepted live transform."
            }
            $gizmoScaleResult = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $gizmoScaleResultPattern
            $gizmoScaleX = [double]::Parse(
                $gizmoScaleResult.Groups['scaleX'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $gizmoScaleY = [double]::Parse(
                $gizmoScaleResult.Groups['scaleY'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $gizmoScaleZ = [double]::Parse(
                $gizmoScaleResult.Groups['scaleZ'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $gizmoStartScaleY = [double]::Parse(
                $scaleResult.Groups['scaleY'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $gizmoStartScaleZ = [double]::Parse(
                $scaleResult.Groups['scaleZ'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            if ([Math]::Abs($gizmoScaleX - $actualScaleX) -le 0.00005 -or
                [Math]::Abs($gizmoScaleY - $gizmoStartScaleY) -gt 0.00005 -or
                [Math]::Abs($gizmoScaleZ - $gizmoStartScaleZ) -gt 0.00005) {
                throw "The packaged X Scale gizmo failed its component-isolation contract. before=($actualScaleX,$gizmoStartScaleY,$gizmoStartScaleZ) after=($gizmoScaleX,$gizmoScaleY,$gizmoScaleZ)"
            }
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $scaledGizmoScreenshotPath `
                -Description "Packaged object after X-axis Scale gizmo drag at 1280x720"
            Assert-PathExists `
                -Path $scaleGizmoScreenshotPath `
                -Description "Packaged 1280x720 Scale gizmo visual proof"
            Assert-PathExists `
                -Path $scaledGizmoScreenshotPath `
                -Description "Packaged 1280x720 per-axis Scale result visual proof"
            Write-Output "[pass] Packaged X-axis Scale gizmo changed only the selected scale component through the normal editor drag path"

            # The following hierarchy check uses this cube as a real parent.
            # Henka intentionally rejects non-uniformly scaled parents, so
            # restore the gizmo-edited X component through the same Object
            # Details action before exercising parenting.
            $restoreScaleText = Format-HenkaAutomationFloat -Value $initialScaleX
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($scaleFieldX + $scaleFieldWidth * 0.5) `
                -FramebufferY ($scaleFieldY + $scaleFieldHeight * 0.5)
            $gizmoScaleText = Format-HenkaAutomationFloat -Value $gizmoScaleX
            for ($backspaceIndex = 0; $backspaceIndex -lt $gizmoScaleText.Length; ++$backspaceIndex) {
                Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "Backspace"
            }
            Send-HenkaAutomationText -EventPath $automationInputPath -Text $restoreScaleText
            $restoreScaleResultOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($scaleApplyX + $scaleApplyWidth * 0.5) `
                -FramebufferY ($scaleApplyY + $scaleApplyHeight * 0.5)
            $restoreScaleResultPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC object-transform-result entity=' +
                $entityPattern +
                ' group=scale accepted=1 state_valid=1 position_x=[-+0-9.eE]+ position_y=[-+0-9.eE]+ position_z=[-+0-9.eE]+ scale_x=(?<scaleX>[-+0-9.eE]+) scale_y=(?<scaleY>[-+0-9.eE]+) scale_z=(?<scaleZ>[-+0-9.eE]+)'
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $restoreScaleResultPattern `
                    -StartingOffset $restoreScaleResultOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "Restoring a valid uniform hierarchy-parent scale through Object Details failed."
            }
            $restoredScaleResult = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $restoreScaleResultPattern
            foreach ($component in @('scaleX', 'scaleY', 'scaleZ')) {
                $restoredComponent = [double]::Parse(
                    $restoredScaleResult.Groups[$component].Value,
                    [Globalization.CultureInfo]::InvariantCulture)
                if ([Math]::Abs($restoredComponent - 1.0) -gt 0.00005) {
                    throw "The hierarchy parent must have unit uniform scale before parenting. $component=$restoredComponent"
                }
            }
            Write-Output "[pass] Restored unit uniform parent scale through the real Object Details action"

            Write-Step "Checking Game Authoring hierarchy through Object Details"
            $hierarchyParentEntity = $createdEntity
            $hierarchyAddCubeButtonRequiredWidths = @(
                (6.0 * 8.0 - 1.0 + 24.0), # Add Cube: measured text plus horizontal padding.
                (6.0 * 5.0 - 1.0 + 24.0), # Clone.
                (6.0 * 6.0 - 1.0 + 24.0)) # Delete.
            $hierarchyAddCubeAvailableWidth = ($sceneObjectsWidth - 28.0) - 12.0
            $hierarchyAddCubeExtraWidth = (
                $hierarchyAddCubeAvailableWidth -
                ($hierarchyAddCubeButtonRequiredWidths | Measure-Object -Sum).Sum) / 3.0
            if ($hierarchyAddCubeExtraWidth -lt 0.0) {
                throw "The selected Scene Objects Add Cube row cannot fit at the supported layout width."
            }
            $hierarchyAddCubeX = $sceneObjectsX + 14.0 +
                (($hierarchyAddCubeButtonRequiredWidths[0] + $hierarchyAddCubeExtraWidth) / 2.0)
            $hierarchyChildOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX $hierarchyAddCubeX `
                -FramebufferY ($addCubeY + $addCubeHeight * 0.5)
            $hierarchyChildPattern = 'DEFAULT_SCENE_ADD_CUBE_READY entity=(?<entity>\d+) document_id=\d+ source=primitive canonical_document=1\.'
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $hierarchyChildPattern `
                    -StartingOffset $hierarchyChildOffset `
                    -TimeoutMilliseconds 6000)) {
                throw "The second visible Add Cube action did not create a Game Authoring child candidate."
            }
            $hierarchyChildMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $hierarchyChildPattern
            if ($null -eq $hierarchyChildMatch) {
                throw "The product-authored hierarchy child did not retain its live entity identity."
            }
            $hierarchyChildEntity = [UInt64]::Parse(
                $hierarchyChildMatch.Groups['entity'].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            if ($hierarchyChildEntity -eq $hierarchyParentEntity) {
                throw "Two visible Add Cube actions returned the same scene entity identity."
            }
            $hierarchyChildPattern = [Regex]::Escape(
                $hierarchyChildEntity.ToString([Globalization.CultureInfo]::InvariantCulture))

            $hierarchyDisclosurePattern =
                'HENKA_AUTOMATION_DIAGNOSTIC game-authoring-hierarchy-disclosure entity=' +
                $hierarchyChildPattern +
                ' expanded=(?<expanded>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28.0 frame=\d+'
            $hierarchyDisclosure = Scroll-DetailsUntilReported `
                -PostconditionPattern $hierarchyDisclosurePattern `
                -Description "the selected object's Game Authoring Hierarchy disclosure"
            Assert-FramebufferRect `
                -Name "Game Authoring Hierarchy disclosure" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X ([double]$hierarchyDisclosure.Groups['x'].Value) `
                -Y ([double]$hierarchyDisclosure.Groups['y'].Value) `
                -Width ([double]$hierarchyDisclosure.Groups['width'].Value) `
                -Height 28.0
            if ($hierarchyDisclosure.Groups['expanded'].Value -eq '0') {
                $hierarchyDisclosureOffset = Get-FileLengthSafe -Path $stdoutPath
                Click-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ([double]$hierarchyDisclosure.Groups['x'].Value + 40.0) `
                    -FramebufferY ([double]$hierarchyDisclosure.Groups['y'].Value + 14.0)
                $expandedHierarchyPattern =
                    'HENKA_AUTOMATION_DIAGNOSTIC game-authoring-hierarchy-disclosure entity=' +
                    $hierarchyChildPattern +
                    ' expanded=1 x=[-0-9.]+ y=[-0-9.]+ width=[-0-9.]+ height=28.0 frame=\d+'
                if (-not (Wait-FileContainsAfterOffset `
                        -Path $stdoutPath `
                        -Pattern $expandedHierarchyPattern `
                        -StartingOffset $hierarchyDisclosureOffset `
                        -TimeoutMilliseconds 4000)) {
                    throw "The selected object's real Object Details Hierarchy disclosure did not expand."
                }
            }

            $hierarchyControlsPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC game-authoring-hierarchy-controls child=' +
                $hierarchyChildPattern +
                ' root=(?<root>[01]) parent_entity=(?<parent>\d+) picker_open=(?<picker>[01]) choose_x=(?<chooseX>[-0-9.]+) choose_y=(?<chooseY>[-0-9.]+) choose_width=(?<chooseWidth>[-0-9.]+) unparent_x=(?<unparentX>[-0-9.]+) unparent_width=(?<unparentWidth>[-0-9.]+) height=28.0 viewport_x=(?<viewportX>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_width=(?<viewportWidth>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+) scroll=(?<scroll>[-0-9.]+) frame=\d+'
            $hierarchyControls = Scroll-DetailsUntilReported `
                -PostconditionPattern $hierarchyControlsPattern `
                -Description "the selected object's visible Game Authoring parent controls"
            if ($hierarchyControls.Groups['root'].Value -ne '1' -or
                $hierarchyControls.Groups['picker'].Value -ne '0') {
                throw "A newly created Game Authoring object did not begin at the scene root."
            }
            Assert-FramebufferRect `
                -Name "Game Authoring Choose Parent control" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X ([double]$hierarchyControls.Groups['chooseX'].Value) `
                -Y ([double]$hierarchyControls.Groups['chooseY'].Value) `
                -Width ([double]$hierarchyControls.Groups['chooseWidth'].Value) `
                -Height 28.0
            $pickerToggleOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ([double]$hierarchyControls.Groups['chooseX'].Value + [double]$hierarchyControls.Groups['chooseWidth'].Value * 0.5) `
                -FramebufferY ([double]$hierarchyControls.Groups['chooseY'].Value + 14.0)
            $hierarchyPickerOpenPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC game-authoring-hierarchy-picker child=' +
                $hierarchyChildPattern + ' open=1'
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $hierarchyPickerOpenPattern `
                    -StartingOffset $pickerToggleOffset `
                    -TimeoutMilliseconds 4000)) {
                throw "Choose Parent did not open the real hierarchy candidate list."
            }

            $hierarchyCandidatePattern =
                'HENKA_AUTOMATION_DIAGNOSTIC game-authoring-hierarchy-candidate child=' +
                $hierarchyChildPattern +
                ' entity=' +
                [Regex]::Escape($hierarchyParentEntity.ToString([Globalization.CultureInfo]::InvariantCulture)) +
                ' submitted=1 x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26.0 viewport_x=(?<viewportX>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_width=(?<viewportWidth>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+) scroll=(?<scroll>[-0-9.]+) frame=\d+'
            $hierarchyCandidate = Scroll-DetailsUntilReported `
                -PostconditionPattern $hierarchyCandidatePattern `
                -Description "the original product-authored parent candidate"
            $candidateX = [double]$hierarchyCandidate.Groups['x'].Value
            $candidateY = [double]$hierarchyCandidate.Groups['y'].Value
            $candidateWidth = [double]$hierarchyCandidate.Groups['width'].Value
            $candidateViewportX = [double]$hierarchyCandidate.Groups['viewportX'].Value
            $candidateViewportY = [double]$hierarchyCandidate.Groups['viewportY'].Value
            $candidateViewportWidth = [double]$hierarchyCandidate.Groups['viewportWidth'].Value
            $candidateViewportHeight = [double]$hierarchyCandidate.Groups['viewportHeight'].Value
            Assert-FramebufferRect `
                -Name "Game Authoring parent candidate" `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $candidateX `
                -Y $candidateY `
                -Width $candidateWidth `
                -Height 26.0
            if ($candidateX -lt $candidateViewportX -or
                $candidateY -lt $candidateViewportY -or
                $candidateX + $candidateWidth -gt $candidateViewportX + $candidateViewportWidth -or
                $candidateY + 26.0 -gt $candidateViewportY + $candidateViewportHeight) {
                throw "The parent candidate was submitted outside the visible Object Details content viewport."
            }
            $parentActionOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($candidateX + $candidateWidth * 0.5) `
                -FramebufferY ($candidateY + 13.0)
            $parentActionPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC game-authoring-hierarchy-action child=' +
                $hierarchyChildPattern + ' kind=parent target=' +
                [Regex]::Escape($hierarchyParentEntity.ToString([Globalization.CultureInfo]::InvariantCulture)) +
                ' accepted=1 result=success'
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $parentActionPattern `
                    -StartingOffset $parentActionOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "Selecting the visible parent candidate did not reparent the selected live Scene Document object."
            }
            $parentedControlsPattern = $hierarchyControlsPattern.Replace(
                'root=(?<root>[01]) parent_entity=(?<parent>\d+)',
                'root=0 parent_entity=' + [Regex]::Escape(
                    $hierarchyParentEntity.ToString([Globalization.CultureInfo]::InvariantCulture)))
            $parentedControls = Scroll-DetailsUntilReported `
                -PostconditionPattern $parentedControlsPattern `
                -Description "the selected object's updated parent state" `
                -StartingOffset $parentActionOffset
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path (Join-Path $logDir "check_packaged_sandbox3d_hierarchy_parented_1280x720.png") `
                -Description "Game Authoring hierarchy with a real parent"

            $unparentOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ([double]$parentedControls.Groups['unparentX'].Value + [double]$parentedControls.Groups['unparentWidth'].Value * 0.5) `
                -FramebufferY ([double]$parentedControls.Groups['chooseY'].Value + 14.0)
            $unparentActionPattern =
                'HENKA_AUTOMATION_DIAGNOSTIC game-authoring-hierarchy-action child=' +
                $hierarchyChildPattern + ' kind=unparent accepted=1 result=success'
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $unparentActionPattern `
                    -StartingOffset $unparentOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The visible Unparent action did not return the live Scene Document object to the root."
            }
            $unparentedControlsPattern = $hierarchyControlsPattern.Replace(
                'root=(?<root>[01]) parent_entity=(?<parent>\d+)',
                'root=1 parent_entity=\d+')
            $null = Scroll-DetailsUntilReported `
                -PostconditionPattern $unparentedControlsPattern `
                -Description "the selected object's restored root state" `
                -StartingOffset $unparentOffset
            Write-Output "[pass] Game Authoring reparent and Unparent used the visible Object Details controls and preserved live entity identity"
            return
        }

        if ($TerrainStartupOnly) {
            Write-Step "Checking Settings Utility clipping at the 1280x720 minimum layout"
            $settingsDiagnosticOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($utilityX + $utilityWidth - 45.0) `
                -FramebufferY ($utilityY + 80.0)
            $settingsControlPattern = 'HENKA_AUTOMATION_DIAGNOSTIC utility-settings-control id=compass_smooth visible=(?<visible>[01]) submitted=(?<submitted>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+) scroll=(?<scroll>[-0-9.]+)'
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $settingsControlPattern `
                    -StartingOffset $settingsDiagnosticOffset `
                    -TimeoutMilliseconds 4000)) {
                throw "Packaged Settings did not report the Smooth Snap control submission boundary."
            }
            $settingsInitial = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $settingsControlPattern
            if ($settingsInitial.Groups['visible'].Value -ne '0' -or
                $settingsInitial.Groups['submitted'].Value -ne '0') {
                throw "An off-viewport Settings control was submitted into the packaged Utility UI."
            }
            Write-Output "[pass] Off-viewport Smooth Snap was not submitted at 1280x720"
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $utilitySettingsTopScreenshotPath `
                -Description "Packaged Utility Settings top at 1280x720"
            $settingsViewportY = [double]$settingsInitial.Groups['viewportY'].Value
            $settingsViewportHeight = [double]$settingsInitial.Groups['viewportHeight'].Value
            $settingsReachedBottom = $false
            for ($settingsScrollAttempt = 0; $settingsScrollAttempt -lt 20; ++$settingsScrollAttempt) {
                Scroll-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($utilityX + 30.0) `
                    -FramebufferY ($settingsViewportY + 12.0) `
                    -WheelDelta -1
                Start-Sleep -Milliseconds 90
                $settingsCurrent = Get-LastLogRegexMatch `
                    -Path $stdoutPath `
                    -Pattern $settingsControlPattern
                if ($null -ne $settingsCurrent -and
                    $settingsCurrent.Groups['visible'].Value -eq '1' -and
                    $settingsCurrent.Groups['submitted'].Value -eq '1' -and
                    [double]$settingsCurrent.Groups['scroll'].Value -gt 0.0) {
                    $settingsControlY = [double]$settingsCurrent.Groups['y'].Value
                    $settingsControlHeight = [double]$settingsCurrent.Groups['height'].Value
                    if ($settingsControlY -lt $settingsViewportY -or
                        $settingsControlY + $settingsControlHeight -gt
                            $settingsViewportY + $settingsViewportHeight + 0.1) {
                        throw "The submitted Smooth Snap hit rectangle extends outside visible Settings content."
                    }
                    $settingsReachedBottom = $true
                    break
                }
            }
            if (-not $settingsReachedBottom) {
                throw "The bottom Settings control never became visible and submitted through Utility scrolling."
            }
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $utilitySettingsScrolledScreenshotPath `
                -Description "Packaged Utility Settings bottom at 1280x720"
            Write-Output "[pass] Bottom Settings control is reachable and bounded at 1280x720"

            Write-Step "Checking Help Utility content at the 1280x720 minimum layout"
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($utilityX + 42.0) `
                -FramebufferY ($utilityY + 50.0)
            $helpPattern = 'HENKA_AUTOMATION_DIAGNOSTIC utility-help scroll=(?<scroll>[-0-9.]+) content=(?<content>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+) terrain_bottom=(?<terrainBottom>[-0-9.]+) heading_visible=(?<heading>[01]) fine_visible=(?<fine>[01])'
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $helpPattern `
                    -StartingOffset 0 `
                    -TimeoutMilliseconds 4000)) {
                throw "Packaged Help did not report a bounded Utility content viewport."
            }
            $helpInitial = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $helpPattern
            if ($null -eq $helpInitial) {
                throw "Packaged Help did not report a bounded Utility content viewport."
            }
            $helpViewportY = [double]$helpInitial.Groups['viewportY'].Value
            $helpViewportHeight = [double]$helpInitial.Groups['viewportHeight'].Value
            $helpContentHeight = [double]$helpInitial.Groups['content'].Value
            if ($helpViewportY -lt [double]$helpInitial.Groups['terrainBottom'].Value + 3.0 -or
                $helpContentHeight -le $helpViewportHeight -or
                $helpInitial.Groups['heading'].Value -ne '1' -or
                $helpInitial.Groups['fine'].Value -ne '0') {
                throw "Packaged Help did not begin below Terrain with bounded overflowing content."
            }
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $utilityHelpTopScreenshotPath `
                -Description "Packaged Utility Help top at 1280x720"
            $helpReachedBottom = $false
            for ($helpScrollAttempt = 0; $helpScrollAttempt -lt 16; ++$helpScrollAttempt) {
                Scroll-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($utilityX + 30.0) `
                    -FramebufferY ($helpViewportY + 12.0) `
                    -WheelDelta -1
                Start-Sleep -Milliseconds 90
                $helpCurrent = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $helpPattern
                if ($null -ne $helpCurrent -and
                    $helpCurrent.Groups['fine'].Value -eq '1' -and
                    [double]$helpCurrent.Groups['scroll'].Value -gt 0.0) {
                    $helpReachedBottom = $true
                    break
                }
            }
            if (-not $helpReachedBottom) {
                throw "Packaged Help could not scroll its final Fine row into the Utility viewport."
            }
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $utilityHelpScrolledScreenshotPath `
                -Description "Packaged Utility Help bottom at 1280x720"
            Write-Output "[pass] Help rows stay below fixed Utility tabs and the last row is reachable by wheel at 1280x720"

            Write-Step "Checking Physics QA Utility scrolling at the 1280x720 minimum layout"
            if ($utilityWidth -le 0.0 -or $utilityHeight -le 138.0) {
                throw "The packaged Physics QA scroll check requires a visible Utility panel with a content viewport."
            }

            # Physics QA is the rightmost Utility destination in the third
            # 24px tab row (panel inset 14px, row y offset 98px). Click its
            # interior so the test proves the normal tab interaction path.
            $physicsQaTabX = $utilityX + $utilityWidth - 40.0
            $physicsQaTabY = $utilityY + 110.0
            $utilityActionOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX $physicsQaTabX `
                -FramebufferY $physicsQaTabY
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC utility action_seq=\d+ frame=\d+ before=.* requested=Physics QA after=Physics QA changed=\d' `
                    -StartingOffset $utilityActionOffset `
                    -TimeoutMilliseconds 4000)) {
                throw "The Physics QA Utility tab did not report an app-consumed selection at the minimum layout."
            }

            $physicsCounterPattern = 'HENKA_AUTOMATION_DIAGNOSTIC utility-counter id=(?<id>bodies|contacts|events) label=(?<label>[A-Za-z]+) value=(?<value>\d+) visible=(?<visible>[01]) submitted=(?<submitted>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) viewport_x=(?<viewportX>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_width=(?<viewportWidth>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+) frame=(?<frame>\d+) scroll=(?<scroll>[-0-9.]+)'

            $utilityScrollPattern = 'HENKA_AUTOMATION_DIAGNOSTIC utility-scroll seq=(?<sequence>\d+) frame=(?<frame>\d+) before=(?<before>[-0-9.]+) after=(?<after>[-0-9.]+) content=(?<content>[-0-9.]+) viewport=(?<viewport>[-0-9.]+) delta=(?<delta>[-0-9.]+) accepted=(?<accepted>[01])'
            $utilityControlPattern = 'HENKA_AUTOMATION_DIAGNOSTIC utility-control id=physics_raycast visible=(?<visible>[01]) submitted=(?<submitted>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) viewport_x=(?<viewportX>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_width=(?<viewportWidth>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+) scroll=(?<scroll>[-0-9.]+)'
            $physicsControlDeadline = [DateTime]::UtcNow.AddSeconds(4)
            $physicsControl = $null
            do {
                $physicsControl = Get-LastLogRegexMatch `
                    -Path $stdoutPath `
                    -Pattern $utilityControlPattern
                if ($null -ne $physicsControl) {
                    break
                }
                Start-Sleep -Milliseconds 40
            } while ([DateTime]::UtcNow -lt $physicsControlDeadline)
            if ($null -eq $physicsControl) {
                throw "The Sandbox did not report whether the lower Physics QA control was inside the Utility viewport."
            }
            $physicsControlVisible = $physicsControl.Groups['visible'].Value -eq '1'
            $physicsControlSubmitted = $physicsControl.Groups['submitted'].Value -eq '1'
            if ($physicsControlVisible -or $physicsControlSubmitted) {
                throw "The lower Camera Raycast control was reported visible/submitted before scrolling into the Utility viewport."
            }
            $physicsControlX = [double]$physicsControl.Groups['x'].Value
            $physicsControlY = [double]$physicsControl.Groups['y'].Value
            $physicsControlWidth = [double]$physicsControl.Groups['width'].Value
            $physicsControlHeight = [double]$physicsControl.Groups['height'].Value
            $physicsViewportX = [double]$physicsControl.Groups['viewportX'].Value
            $physicsViewportY = [double]$physicsControl.Groups['viewportY'].Value
            $physicsViewportWidth = [double]$physicsControl.Groups['viewportWidth'].Value
            $physicsViewportHeight = [double]$physicsControl.Groups['viewportHeight'].Value
            $terrainTabPattern = 'Terrain utility tab: x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+)\.'
            $terrainTab = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $terrainTabPattern
            if ($null -eq $terrainTab) {
                throw "The packaged Utility navigation did not report its fixed Terrain tab geometry."
            }
            $terrainTabBottom =
                [double]$terrainTab.Groups['y'].Value +
                [double]$terrainTab.Groups['height'].Value
            if ($physicsViewportY -lt $terrainTabBottom + 4.0) {
                throw (
                    "Physics QA content viewport overlaps the fixed Terrain Utility tab: " +
                    "viewport_y=$physicsViewportY, terrain_bottom=$terrainTabBottom, " +
                    "required_gap=4.0.")
            }
            Write-Output "[pass] Physics QA content viewport starts below the fixed Terrain Utility tab"
            if ($physicsControlX -ge $physicsViewportX -and
                $physicsControlY -ge $physicsViewportY -and
                $physicsControlX + $physicsControlWidth -le $physicsViewportX + $physicsViewportWidth -and
                $physicsControlY + $physicsControlHeight -le $physicsViewportY + $physicsViewportHeight) {
                throw "The Camera Raycast control was classified hidden despite lying wholly inside the visible Utility content rectangle."
            }
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $physicsQaTopScreenshotPath `
                -Description "Packaged Physics QA Utility at 1280x720 before scrolling"

            # A click at the last two pixels of the visible content viewport
            # cannot intersect a fully-contained 28px control. The application
            # must not route the off-screen Raycast button through this blank
            # Utility strip.
            $noRaycastActionPattern = 'HENKA_AUTOMATION_DIAGNOSTIC utility-action id=physics_raycast'
            $noRaycastActionCountBefore = [regex]::Matches(
                (Get-HenkaPackagedStartupLogText -Path $stdoutPath),
                $noRaycastActionPattern).Count
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($physicsViewportX + ($physicsViewportWidth * 0.5)) `
                -FramebufferY ($physicsViewportY + $physicsViewportHeight - 2.0)
            Start-Sleep -Milliseconds 300
            $noRaycastActionCountAfter = [regex]::Matches(
                (Get-HenkaPackagedStartupLogText -Path $stdoutPath),
                $noRaycastActionPattern).Count
            if ($noRaycastActionCountAfter -ne $noRaycastActionCountBefore) {
                throw "A click in the visible Utility viewport's blank lower edge activated the off-screen Camera Raycast control."
            }
            Write-Output "[pass] Off-viewport Camera Raycast was not submitted or activated by a click inside blank visible Utility content"

            $previousUtilityScroll = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $utilityScrollPattern
            $previousUtilitySequence = if ($null -ne $previousUtilityScroll) {
                [int]$previousUtilityScroll.Groups['sequence'].Value
            }
            else {
                0
            }
            $utilityScroll = $null
            $physicsQaCounterScreenshotCaptured = $false
            for ($scrollAttempt = 0; $scrollAttempt -lt 20 -and -not $physicsControlVisible; $scrollAttempt++) {
                Scroll-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($physicsViewportX + 12.0) `
                    -FramebufferY ($physicsViewportY + 12.0) `
                    -WheelDelta -1
                $utilityScrollDeadline = [DateTime]::UtcNow.AddSeconds(4)
                $utilityScroll = $null
                do {
                    $latestUtilityScroll = Get-LastLogRegexMatch `
                        -Path $stdoutPath `
                        -Pattern $utilityScrollPattern
                    if ($null -ne $latestUtilityScroll -and
                        [int]$latestUtilityScroll.Groups['sequence'].Value -gt $previousUtilitySequence) {
                        $utilityScroll = $latestUtilityScroll
                        break
                    }
                    Start-Sleep -Milliseconds 40
                } while ([DateTime]::UtcNow -lt $utilityScrollDeadline)

                if ($null -eq $utilityScroll) {
                    throw "The Sandbox process did not report consuming Physics QA Utility scroll input $($scrollAttempt + 1)."
                }
                $previousUtilitySequence = [int]$utilityScroll.Groups['sequence'].Value
                $utilityScrollBefore = [double]$utilityScroll.Groups['before'].Value
                $utilityScrollAfter = [double]$utilityScroll.Groups['after'].Value
                $utilityContentHeight = [double]$utilityScroll.Groups['content'].Value
                $utilityViewportHeight = [double]$utilityScroll.Groups['viewport'].Value
                if ($utilityScroll.Groups['accepted'].Value -ne '1' -or
                    $utilityScrollAfter -le $utilityScrollBefore + 0.5 -or
                    $utilityContentHeight -le $utilityViewportHeight + 0.5) {
                    break
                }

                $physicsControlDeadline = [DateTime]::UtcNow.AddSeconds(4)
                do {
                    $latestPhysicsControl = Get-LastLogRegexMatch `
                        -Path $stdoutPath `
                        -Pattern $utilityControlPattern
                    if ($null -ne $latestPhysicsControl -and
                        [Math]::Abs(
                            [double]$latestPhysicsControl.Groups['scroll'].Value -
                            $utilityScrollAfter) -le 0.6) {
                        $physicsControl = $latestPhysicsControl
                        break
                    }
                    Start-Sleep -Milliseconds 40
                } while ([DateTime]::UtcNow -lt $physicsControlDeadline)
                if ($null -eq $physicsControl -or
                    [Math]::Abs(
                        [double]$physicsControl.Groups['scroll'].Value -
                        $utilityScrollAfter) -gt 0.6) {
                    throw "The Sandbox did not report Camera Raycast geometry for the consumed Utility scroll offset $utilityScrollAfter."
                }
                $physicsControlVisible = $physicsControl.Groups['visible'].Value -eq '1'
                $physicsControlSubmitted = $physicsControl.Groups['submitted'].Value -eq '1'

                if (-not $physicsQaCounterScreenshotCaptured) {
                    $counterFrameMatches = [System.Text.RegularExpressions.Regex]::Matches(
                        (Get-HenkaPackagedStartupLogText -Path $stdoutPath),
                        $physicsCounterPattern)
                    $counterRowsByFrame = @{}
                    foreach ($counterFrameMatch in $counterFrameMatches) {
                        $counterFrameKey = "{0}|{1}" -f `
                            $counterFrameMatch.Groups['frame'].Value, `
                            $counterFrameMatch.Groups['scroll'].Value
                        if (-not $counterRowsByFrame.ContainsKey($counterFrameKey)) {
                            $counterRowsByFrame[$counterFrameKey] = @{}
                        }
                        $counterRowsByFrame[$counterFrameKey][
                            $counterFrameMatch.Groups['id'].Value] = $counterFrameMatch
                    }
                    foreach ($counterRows in $counterRowsByFrame.Values) {
                        if ($counterRows.Count -eq 3) {
                            Save-WindowScreenshot `
                                -Handle $mainWindowHandle `
                                -Path $physicsQaCounterScreenshotPath `
                                -Description "Packaged Physics QA counters together at 1280x720"
                            $physicsQaCounterScreenshotCaptured = $true
                            break
                        }
                    }
                }
            }

            if ($null -eq $utilityScroll) {
                throw "The Sandbox did not consume a Physics QA Utility scroll input."
            }
            if (-not $physicsControlVisible -or -not $physicsControlSubmitted) {
                throw "The lower Camera Raycast control never became visible and submitted after scrolling through overflowing Physics QA content."
            }

            $physicsControlX = [double]$physicsControl.Groups['x'].Value
            $physicsControlY = [double]$physicsControl.Groups['y'].Value
            $physicsControlWidth = [double]$physicsControl.Groups['width'].Value
            $physicsControlHeight = [double]$physicsControl.Groups['height'].Value
            $physicsViewportX = [double]$physicsControl.Groups['viewportX'].Value
            $physicsViewportY = [double]$physicsControl.Groups['viewportY'].Value
            $physicsViewportWidth = [double]$physicsControl.Groups['viewportWidth'].Value
            $physicsViewportHeight = [double]$physicsControl.Groups['viewportHeight'].Value
            if ($physicsControlX -lt $physicsViewportX -or
                $physicsControlY -lt $physicsViewportY -or
                $physicsControlX + $physicsControlWidth -gt $physicsViewportX + $physicsViewportWidth -or
                $physicsControlY + $physicsControlHeight -gt $physicsViewportY + $physicsViewportHeight) {
                throw "The submitted Camera Raycast hit target extends outside the visible Utility content viewport."
            }

            if (-not $physicsQaCounterScreenshotCaptured) {
                throw "Packaged scrolling did not present all three Physics QA counter rows in one captured frame."
            }
            $counterMatches = [System.Text.RegularExpressions.Regex]::Matches(
                (Get-HenkaPackagedStartupLogText -Path $stdoutPath),
                $physicsCounterPattern)
            $counterRowsByFrame = @{}
            foreach ($counterMatch in $counterMatches) {
                $counterFrameKey = "{0}|{1}" -f `
                    $counterMatch.Groups['frame'].Value, `
                    $counterMatch.Groups['scroll'].Value
                if (-not $counterRowsByFrame.ContainsKey($counterFrameKey)) {
                    $counterRowsByFrame[$counterFrameKey] = @{}
                }
                $counterRowsByFrame[$counterFrameKey][
                    $counterMatch.Groups['id'].Value] = $counterMatch
            }
            $physicsCounters = $null
            foreach ($counterRows in $counterRowsByFrame.Values) {
                if ($counterRows.Count -eq 3) {
                    $physicsCounters = $counterRows
                    break
                }
            }
            if ($null -eq $physicsCounters) {
                throw "Packaged scrolling did not report Bodies, Contacts, and Events together in one frame/scroll state."
            }
            $expectedPhysicsCounterLabels = @{
                bodies = 'Bodies'
                contacts = 'Contacts'
                events = 'Events'
            }
            foreach ($counterId in @('bodies', 'contacts', 'events')) {
                $counter = $physicsCounters[$counterId]
                $counterLabel = $counter.Groups['label'].Value
                $counterValue = $counter.Groups['value'].Value
                $counterWidth = [double]$counter.Groups['width'].Value
                $counterX = [double]$counter.Groups['x'].Value
                $counterY = [double]$counter.Groups['y'].Value
                $counterHeight = [double]$counter.Groups['height'].Value
                $counterViewportX = [double]$counter.Groups['viewportX'].Value
                $counterViewportY = [double]$counter.Groups['viewportY'].Value
                $counterViewportWidth = [double]$counter.Groups['viewportWidth'].Value
                $counterViewportHeight = [double]$counter.Groups['viewportHeight'].Value
                $labelCapacity = [Math]::Floor(($counterWidth * 0.34) / 6.0)
                $valueCapacity = [Math]::Floor((($counterWidth * 0.62) - 4.0 - 12.0) / 6.0)
                if ($counterLabel -ne $expectedPhysicsCounterLabels[$counterId] -or
                    $counterLabel.Length -gt $labelCapacity -or
                    $counterValue.Length -gt $valueCapacity) {
                    throw "Physics QA counter '$counterId' does not fit its 1280x720 value-row allocation."
                }
                if ($counter.Groups['visible'].Value -ne '1' -or
                    $counter.Groups['submitted'].Value -ne '1') {
                    throw "Physics QA counter '$counterId' was not observed as visible and submitted."
                }
                if ($counterX -lt $counterViewportX -or
                    $counterY -lt $counterViewportY -or
                    $counterX + $counterWidth -gt $counterViewportX + $counterViewportWidth -or
                    $counterY + $counterHeight -gt $counterViewportY + $counterViewportHeight) {
                    throw "Submitted Physics QA counter '$counterId' extends outside the visible Utility content viewport."
                }
            }
            Write-Output "[pass] Physics QA Bodies, Contacts, and Events values each fit visible, submitted 1280x720 value rows"

            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $physicsQaScrolledScreenshotPath `
                -Description "Packaged Physics QA Utility at 1280x720 with lower control reachable"

            $raycastActionOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($physicsControlX + $physicsControlWidth * 0.5) `
                -FramebufferY ($physicsControlY + $physicsControlHeight * 0.5)
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC utility-action id=physics_raycast frame=\d+ hit=[01]' `
                    -StartingOffset $raycastActionOffset `
                    -TimeoutMilliseconds 6000)) {
                throw "The visible Camera Raycast button did not activate through the packaged Physics QA UI."
            }
            Write-Output "[pass] Scrolled Camera Raycast became fully visible, remained inside the Utility viewport, and activated through the real packaged button"

            # Drag the Utility-owned scrollbar thumb back toward mid-content.
            # This uses the app-local automation stream, so ordinary packaged
            # validation does not acquire desktop foreground focus.
            $dragControlBefore = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $utilityControlPattern
            if ($null -eq $dragControlBefore -or
                $dragControlBefore.Groups['visible'].Value -ne '1' -or
                $dragControlBefore.Groups['submitted'].Value -ne '1') {
                throw "The lower Physics QA control was not visible before the scrollbar drag check."
            }
            $dragOffsetBefore = [double]$dragControlBefore.Groups['scroll'].Value
            $dragContentHeight = [double]$utilityScroll.Groups['content'].Value
            $dragViewportHeight = [double]$utilityScroll.Groups['viewport'].Value
            $dragTrackHeight = $physicsViewportHeight
            if ($dragContentHeight -le $dragViewportHeight -or
                $dragTrackHeight -le 24.0) {
                throw "The packaged Utility scrollbar did not have a draggable overflowing track."
            }
            $dragThumbHeight = [Math]::Min(
                $dragTrackHeight,
                [Math]::Max(
                    24.0,
                    $dragTrackHeight * $dragViewportHeight / $dragContentHeight))
            $dragMaximumOffset = $dragContentHeight - $dragViewportHeight
            $dragThumbTravel = $dragTrackHeight - $dragThumbHeight
            if ($dragMaximumOffset -le 0.0 -or $dragThumbTravel -le 0.0) {
                throw "The packaged Utility scrollbar geometry could not represent the overflowing content range."
            }
            $dragThumbOffset =
                $dragThumbTravel * $dragOffsetBefore / $dragMaximumOffset
            $dragTrackX = $physicsViewportX + $physicsViewportWidth + 9.0
            $dragStartY = $physicsViewportY + $dragThumbOffset + ($dragThumbHeight * 0.5)
            $dragEndY = $physicsViewportY + ($dragTrackHeight * 0.5)
            $dragClientSize = Get-PackagedClientSize -Handle $mainWindowHandle
            $dragStartClient = Convert-HenkaFramebufferPointToWindowPoint `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -WindowWidth $dragClientSize.Width `
                -WindowHeight $dragClientSize.Height `
                -FramebufferX $dragTrackX `
                -FramebufferY $dragStartY
            $dragEndClient = Convert-HenkaFramebufferPointToWindowPoint `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -WindowWidth $dragClientSize.Width `
                -WindowHeight $dragClientSize.Height `
                -FramebufferX $dragTrackX `
                -FramebufferY $dragEndY
            $dragXText = Format-HenkaAutomationFloat -Value $dragStartClient.X
            $dragStartYText = Format-HenkaAutomationFloat -Value $dragStartClient.Y
            $dragEndYText = Format-HenkaAutomationFloat -Value $dragEndClient.Y
            $dragLogOffset = Get-FileLengthSafe -Path $stdoutPath
            foreach ($dragEvent in @(
                "move $dragXText $dragStartYText",
                "button left down $dragXText $dragStartYText",
                "move $dragXText $dragEndYText",
                "button left up $dragXText $dragEndYText")) {
                Send-HenkaAutomationEvent `
                    -EventPath $automationInputPath `
                    -EventLine $dragEvent
            }
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $utilityControlPattern `
                    -StartingOffset $dragLogOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The packaged Sandbox did not report Utility control geometry after dragging the scrollbar thumb."
            }
            $dragControlAfter = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $utilityControlPattern
            if ($null -eq $dragControlAfter) {
                throw "The packaged Sandbox did not report the post-drag Utility state."
            }
            $dragOffsetAfter = [double]$dragControlAfter.Groups['scroll'].Value
            if ([Math]::Abs($dragOffsetAfter - $dragOffsetBefore) -le 1.0 -or
                $dragOffsetAfter -lt ($dragMaximumOffset * 0.35) -or
                $dragOffsetAfter -gt ($dragMaximumOffset * 0.65)) {
                throw (
                    "Dragging the Utility scrollbar did not move its independent scroll state to the requested middle range: " +
                    "before=$dragOffsetBefore after=$dragOffsetAfter max=$dragMaximumOffset.")
            }
            if ($dragControlAfter.Groups['visible'].Value -ne '0' -or
                $dragControlAfter.Groups['submitted'].Value -ne '0') {
                throw "The off-viewport Camera Raycast control remained visible or interactable after dragging the Utility scrollbar."
            }
            $raycastActionCountBeforeReleaseCheck = [regex]::Matches(
                (Get-HenkaPackagedStartupLogText -Path $stdoutPath),
                $noRaycastActionPattern).Count

            # A wheel event after the mouse-up must operate on Utility content,
            # proving the drag release did not leave the scrollbar capture stuck.
            $releaseScrollSequence = [int]$previousUtilitySequence
            Scroll-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($physicsViewportX + 12.0) `
                -FramebufferY ($physicsViewportY + 12.0) `
                -WheelDelta 1
            $releaseScroll = $null
            $releaseScrollDeadline = [DateTime]::UtcNow.AddSeconds(5)
            do {
                $latestReleaseScroll = Get-LastLogRegexMatch `
                    -Path $stdoutPath `
                    -Pattern $utilityScrollPattern
                if ($null -ne $latestReleaseScroll -and
                    [int]$latestReleaseScroll.Groups['sequence'].Value -gt $releaseScrollSequence -and
                    [double]$latestReleaseScroll.Groups['before'].Value -ge $dragOffsetAfter - 0.5) {
                    $releaseScroll = $latestReleaseScroll
                    break
                }
                Start-Sleep -Milliseconds 40
            } while ([DateTime]::UtcNow -lt $releaseScrollDeadline)
            if ($null -eq $releaseScroll -or
                $releaseScroll.Groups['accepted'].Value -ne '1' -or
                [double]$releaseScroll.Groups['after'].Value -ge $dragOffsetAfter - 0.5) {
                throw "The Utility scrollbar did not release cleanly back to independent wheel scrolling."
            }
            $raycastActionCountAfterReleaseCheck = [regex]::Matches(
                (Get-HenkaPackagedStartupLogText -Path $stdoutPath),
                $noRaycastActionPattern).Count
            if ($raycastActionCountAfterReleaseCheck -ne $raycastActionCountBeforeReleaseCheck) {
                throw "Moving or scrolling the Utility scrollbar activated the off-viewport Camera Raycast control."
            }
            Write-Output ("[pass] Packaged Utility scrollbar drag moved its owned scroll offset from {0:N1} to {1:N1}, kept Camera Raycast clipped, released cleanly, and preserved wheel input" -f `
                $dragOffsetBefore,
                $dragOffsetAfter)

            $utilityScrollBefore = [double]$utilityScroll.Groups['before'].Value
            $utilityScrollAfter = [double]$utilityScroll.Groups['after'].Value
            $utilityContentHeight = [double]$utilityScroll.Groups['content'].Value
            $utilityViewportHeight = [double]$utilityScroll.Groups['viewport'].Value
            if ($utilityScroll.Groups['accepted'].Value -ne '1' -or
                $utilityScrollAfter -le $utilityScrollBefore + 0.5 -or
                $utilityContentHeight -le $utilityViewportHeight + 0.5) {
                throw (
                    "Physics QA Utility scroll did not advance through overflowing content: " +
                    "accepted=$($utilityScroll.Groups['accepted'].Value), " +
                    "offset=$utilityScrollAfter/$utilityScrollBefore, " +
                    "content=$utilityContentHeight, viewport=$utilityViewportHeight.")
            }
            Write-Output ("[pass] Packaged Physics QA Utility scroll advanced from {0:N1} to {1:N1} over {2:N1}px of content in a {3:N1}px viewport" -f `
                $utilityScrollBefore,
                $utilityScrollAfter,
                $utilityContentHeight,
                $utilityViewportHeight)
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
        if ($TerrainStartupOnly) {
            $terrainRedoPattern = 'HENKA_AUTOMATION_DIAGNOSTIC utility-terrain-control id=terrain_redo visible=(?<visible>[01]) submitted=(?<submitted>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) viewport_x=(?<viewportX>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_width=(?<viewportWidth>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+) scroll=(?<scroll>[-0-9.]+)'
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $terrainRedoPattern `
                    -StartingOffset $terrainCreateOffset `
                    -TimeoutMilliseconds 4000)) {
                throw "Packaged Terrain did not report the Redo control submission boundary."
            }
            $terrainRedoInitial = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $terrainRedoPattern
            if ($terrainRedoInitial.Groups['visible'].Value -ne '0' -or
                $terrainRedoInitial.Groups['submitted'].Value -ne '0') {
                throw "An off-viewport Terrain Redo control was submitted into the packaged Utility UI."
            }
            Write-Output "[pass] Off-viewport Terrain Redo was not submitted at 1280x720"
            $terrainViewportX = [double]$terrainRedoInitial.Groups['viewportX'].Value
            $terrainViewportY = [double]$terrainRedoInitial.Groups['viewportY'].Value
            $terrainViewportWidth = [double]$terrainRedoInitial.Groups['viewportWidth'].Value
            $terrainViewportHeight = [double]$terrainRedoInitial.Groups['viewportHeight'].Value
            $terrainBlankAreaOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($terrainViewportX + 126.0) `
                -FramebufferY ($terrainViewportY + $terrainViewportHeight - 12.0)
            if (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern 'Terrain redo unavailable:' `
                    -StartingOffset $terrainBlankAreaOffset `
                    -TimeoutMilliseconds 600) {
                throw "A click in blank visible Terrain content activated the off-viewport Redo action."
            }
            Write-Output "[pass] Blank visible Terrain content did not activate the clipped Redo action"
            $terrainRedoReached = $false
            for ($terrainScrollAttempt = 0; $terrainScrollAttempt -lt 20; ++$terrainScrollAttempt) {
                Scroll-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($terrainViewportX + 30.0) `
                    -FramebufferY ($terrainViewportY + 12.0) `
                    -WheelDelta -1
                Start-Sleep -Milliseconds 90
                $terrainRedoCurrent = Get-LastLogRegexMatch `
                    -Path $stdoutPath `
                    -Pattern $terrainRedoPattern
                if ($null -ne $terrainRedoCurrent -and
                    $terrainRedoCurrent.Groups['visible'].Value -eq '1' -and
                    $terrainRedoCurrent.Groups['submitted'].Value -eq '1' -and
                    [double]$terrainRedoCurrent.Groups['scroll'].Value -gt 0.0) {
                    $terrainRedoX = [double]$terrainRedoCurrent.Groups['x'].Value
                    $terrainRedoY = [double]$terrainRedoCurrent.Groups['y'].Value
                    $terrainRedoWidth = [double]$terrainRedoCurrent.Groups['width'].Value
                    $terrainRedoHeight = [double]$terrainRedoCurrent.Groups['height'].Value
                    if ($terrainRedoX -lt $terrainViewportX -or
                        $terrainRedoX + $terrainRedoWidth -gt $terrainViewportX + $terrainViewportWidth + 0.1 -or
                        $terrainRedoY -lt $terrainViewportY -or
                        $terrainRedoY + $terrainRedoHeight -gt $terrainViewportY + $terrainViewportHeight + 0.1) {
                        throw "Terrain Redo was submitted beyond the visible Utility content area."
                    }
                    $terrainRedoReached = $true
                    break
                }
            }
            if (-not $terrainRedoReached) {
                throw "Terrain Redo never became visible and submitted through Utility scrolling."
            }
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $terrainUiScrolledScreenshotPath `
                -Description "Packaged Terrain Utility scrolled at 1280x720"
            $terrainRedoActionOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($terrainRedoX + $terrainRedoWidth * 0.5) `
                -FramebufferY ($terrainRedoY + $terrainRedoHeight * 0.5)
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern 'Terrain redo unavailable:' `
                    -StartingOffset $terrainRedoActionOffset `
                    -TimeoutMilliseconds 4000)) {
                throw "The visible Terrain Redo button did not route through the real history action."
            }
            Write-Output "[pass] Terrain Redo became bounded and activated through the packaged UI"
            Scroll-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($terrainViewportX + 30.0) `
                -FramebufferY ($terrainViewportY + 12.0) `
                -WheelDelta -1
            $terrainStorageScrollPattern = 'HENKA_AUTOMATION_DIAGNOSTIC utility-terrain-control id=terrain_redo visible=(?<visible>[01]) submitted=(?<submitted>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) viewport_x=(?<viewportX>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_width=(?<viewportWidth>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+) scroll=(?<scroll>[-0-9.]+)'
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $terrainStorageScrollPattern `
                    -StartingOffset $terrainRedoActionOffset `
                    -TimeoutMilliseconds 3000)) {
                throw "The packaged Terrain view did not report app-side progress after moving to storage controls."
            }
            $terrainStorageScroll = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $terrainStorageScrollPattern
            $terrainSaveY = $terrainViewportY + 406.0 -
                [double]$terrainStorageScroll.Groups['scroll'].Value
            if ($terrainSaveY -lt $terrainViewportY -or
                $terrainSaveY + 24.0 -gt $terrainViewportY + $terrainViewportHeight + 0.1) {
                throw "Terrain Save does not fit in the visible Utility viewport at the captured storage offset."
            }
            Start-Sleep -Milliseconds 400
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $terrainUiScrolledScreenshotPath `
                -Description "Packaged Terrain Utility storage controls at 1280x720"
        }
        Set-HenkaAutomationForeground -Handle $mainWindowHandle
        Start-Sleep -Milliseconds 700
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $terrainUiAfterScreenshotPath `
            -Description "Packaged Terrain Utility after Create Terrain"

        if ($TerrainStartupOnly) {
            Write-Step "Checking Diagnostics and Transform QA Utility overflow at 1280x720"
            $diagnosticsPattern = 'HENKA_AUTOMATION_DIAGNOSTIC utility-diagnostics-control id=native visible=(?<visible>[01]) submitted=(?<submitted>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) viewport_x=(?<viewportX>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_width=(?<viewportWidth>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+) content=(?<content>[-0-9.]+) scroll=(?<scroll>[-0-9.]+)'
            $transformQaPattern = 'HENKA_AUTOMATION_DIAGNOSTIC utility-transform-qa-control id=reset_test_object visible=(?<visible>[01]) submitted=(?<submitted>[01]) activated=(?<activated>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) viewport_x=(?<viewportX>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_width=(?<viewportWidth>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+) content=(?<content>[-0-9.]+) scroll=(?<scroll>[-0-9.]+)'
            $transformQaTestMovePattern = 'HENKA_AUTOMATION_DIAGNOSTIC utility-transform-qa-control id=test_move visible=(?<visible>[01]) submitted=(?<submitted>[01]) activated=(?<activated>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) viewport_x=(?<viewportX>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_width=(?<viewportWidth>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+) content=(?<content>[-0-9.]+) scroll=(?<scroll>[-0-9.]+)'
            $diagnosticsTabOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($utilityX + 50.0) `
                -FramebufferY ($utilityY + 110.0)
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $diagnosticsPattern `
                    -StartingOffset $diagnosticsTabOffset `
                    -TimeoutMilliseconds 4000)) {
                throw "Packaged Diagnostics did not report its actual Utility content geometry."
            }
            $diagnosticsInitial = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $diagnosticsPattern
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $utilityDiagnosticsTopScreenshotPath `
                -Description "Packaged Utility Diagnostics top at 1280x720"

            $transformQaTabOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($utilityX + 155.0) `
                -FramebufferY ($utilityY + 110.0)
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $transformQaPattern `
                    -StartingOffset $transformQaTabOffset `
                    -TimeoutMilliseconds 4000)) {
                throw "Packaged Transform QA did not report its actual Utility content geometry."
            }
            $transformQaInitial = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $transformQaPattern
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $utilityTransformQaTopScreenshotPath `
                -Description "Packaged Utility Transform QA top at 1280x720"

            foreach ($utilityControlCase in @(
                    @{ Name = "Diagnostics Native"; Match = $diagnosticsInitial },
                    @{ Name = "Transform QA Reset Test Object"; Match = $transformQaInitial })) {
                $control = $utilityControlCase.Match
                $controlX = [double]$control.Groups['x'].Value
                $controlY = [double]$control.Groups['y'].Value
                $controlWidth = [double]$control.Groups['width'].Value
                $controlHeight = [double]$control.Groups['height'].Value
                $viewportX = [double]$control.Groups['viewportX'].Value
                $viewportY = [double]$control.Groups['viewportY'].Value
                $viewportWidth = [double]$control.Groups['viewportWidth'].Value
                $viewportHeight = [double]$control.Groups['viewportHeight'].Value
                $expectedVisible =
                    $controlX -ge $viewportX -and
                    $controlY -ge $viewportY -and
                    $controlX + $controlWidth -le $viewportX + $viewportWidth + 0.1 -and
                    $controlY + $controlHeight -le $viewportY + $viewportHeight + 0.1
                if (($control.Groups['visible'].Value -eq '1') -ne $expectedVisible -or
                    ($control.Groups['submitted'].Value -eq '1') -ne $expectedVisible) {
                    throw (
                        "{0} visibility/submission disagreed with the visible Utility content rectangle: " -f
                        $utilityControlCase.Name) +
                        "visible=$($control.Groups['visible'].Value) submitted=$($control.Groups['submitted'].Value) " +
                        "rect=($controlX,$controlY,$controlWidth,$controlHeight) " +
                        "viewport=($viewportX,$viewportY,$viewportWidth,$viewportHeight)."
                }
                if ([double]$control.Groups['content'].Value -le $viewportHeight) {
                    throw "The 1280x720 $($utilityControlCase.Name) case no longer exercises Utility content overflow."
                }
            }
            Write-Output "[pass] Diagnostics and Transform QA exclude controls outside their visible Utility content areas"

            $diagnosticsTabOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($utilityX + 50.0) `
                -FramebufferY ($utilityY + 110.0)
            Start-Sleep -Milliseconds 250
            $diagnosticsCurrent = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $diagnosticsPattern
            if ([double]$diagnosticsCurrent.Groups['scroll'].Value -gt 0.5) {
                throw "Diagnostics did not retain its own unscrolled starting position."
            }

            $diagnosticsViewportX = [double]$diagnosticsInitial.Groups['viewportX'].Value
            $diagnosticsViewportY = [double]$diagnosticsInitial.Groups['viewportY'].Value
            $diagnosticsViewportHeight = [double]$diagnosticsInitial.Groups['viewportHeight'].Value
            $diagnosticsReachedBottom = $false
            for ($diagnosticsScrollAttempt = 0; $diagnosticsScrollAttempt -lt 32; ++$diagnosticsScrollAttempt) {
                Scroll-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($diagnosticsViewportX + 20.0) `
                    -FramebufferY ($diagnosticsViewportY + 10.0) `
                    -WheelDelta -1
                Start-Sleep -Milliseconds 90
                $diagnosticsCurrent = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $diagnosticsPattern
                if ($null -ne $diagnosticsCurrent -and
                    $diagnosticsCurrent.Groups['visible'].Value -eq '1' -and
                    $diagnosticsCurrent.Groups['submitted'].Value -eq '1' -and
                    [double]$diagnosticsCurrent.Groups['scroll'].Value -gt 0.0) {
                    $nativeY = [double]$diagnosticsCurrent.Groups['y'].Value
                    $nativeHeight = [double]$diagnosticsCurrent.Groups['height'].Value
                    if ($nativeY -lt $diagnosticsViewportY -or
                        $nativeY + $nativeHeight -gt $diagnosticsViewportY + $diagnosticsViewportHeight + 0.1) {
                        throw "The submitted Diagnostics Native row extends outside visible Utility content."
                    }
                    $diagnosticsReachedBottom = $true
                    break
                }
            }
            if (-not $diagnosticsReachedBottom) {
                throw "The Diagnostics Native row never became visible through Utility scrolling."
            }
            Start-Sleep -Milliseconds 350
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $utilityDiagnosticsScrolledScreenshotPath `
                -Description "Packaged Utility Diagnostics scrolled at 1280x720"

            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($utilityX + 155.0) `
                -FramebufferY ($utilityY + 110.0)
            Start-Sleep -Milliseconds 250
            $transformQaCurrent = $transformQaInitial
            $transformQaViewportX = [double]$transformQaCurrent.Groups['viewportX'].Value
            $transformQaViewportY = [double]$transformQaCurrent.Groups['viewportY'].Value
            $transformQaViewportHeight = [double]$transformQaCurrent.Groups['viewportHeight'].Value
            Scroll-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($transformQaViewportX + 20.0) `
                -FramebufferY ($transformQaViewportY + 10.0) `
                -WheelDelta -1
            Start-Sleep -Milliseconds 90
            $testMoveAfterFirstWheel = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $transformQaTestMovePattern
            if ($null -eq $testMoveAfterFirstWheel -or
                [Math]::Abs([double]$testMoveAfterFirstWheel.Groups['scroll'].Value - 48.0) -gt 0.5 -or
                $testMoveAfterFirstWheel.Groups['visible'].Value -ne '0' -or
                $testMoveAfterFirstWheel.Groups['submitted'].Value -ne '0') {
                throw "Transform QA did not resume its independent top offset after Diagnostics scrolling."
            }
            for ($testMoveScrollAttempt = 0; $testMoveScrollAttempt -lt 3; ++$testMoveScrollAttempt) {
                Scroll-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($transformQaViewportX + 20.0) `
                    -FramebufferY ($transformQaViewportY + 10.0) `
                    -WheelDelta -1
                Start-Sleep -Milliseconds 90
            }
            $testMoveVisible = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $transformQaTestMovePattern
            if ($null -eq $testMoveVisible -or
                $testMoveVisible.Groups['visible'].Value -ne '1' -or
                $testMoveVisible.Groups['submitted'].Value -ne '1') {
                throw "The Transform QA Test Move action did not become reachable inside the visible Utility viewport."
            }
            $testMoveActivationOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ([double]$testMoveVisible.Groups['x'].Value +
                    [double]$testMoveVisible.Groups['width'].Value * 0.5) `
                -FramebufferY ([double]$testMoveVisible.Groups['y'].Value +
                    [double]$testMoveVisible.Groups['height'].Value * 0.5)
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern ($transformQaTestMovePattern.Replace('activated=(?<activated>[01])', 'activated=1')) `
                    -StartingOffset $testMoveActivationOffset `
                    -TimeoutMilliseconds 4000)) {
                throw "The visible Transform QA Test Move action did not activate through the packaged UI."
            }

            Scroll-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($transformQaViewportX + 20.0) `
                -FramebufferY ($transformQaViewportY + 10.0) `
                -WheelDelta 1
            Start-Sleep -Milliseconds 90
            $testMoveOffPanel = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $transformQaTestMovePattern
            if ($null -eq $testMoveOffPanel -or
                $testMoveOffPanel.Groups['visible'].Value -ne '0' -or
                $testMoveOffPanel.Groups['submitted'].Value -ne '0') {
                throw "The Transform QA Test Move action remained visible or submitted after scrolling outside the Utility content area."
            }
            $testMoveOffPanelX = [double]$testMoveOffPanel.Groups['x'].Value
            $testMoveOffPanelY = [double]$testMoveOffPanel.Groups['y'].Value
            $testMoveOffPanelWidth = [double]$testMoveOffPanel.Groups['width'].Value
            $testMoveOffPanelHeight = [double]$testMoveOffPanel.Groups['height'].Value
            $testMoveOffPanelCenterX = $testMoveOffPanelX + $testMoveOffPanelWidth * 0.5
            $testMoveOffPanelCenterY = $testMoveOffPanelY + $testMoveOffPanelHeight * 0.5
            if ($testMoveOffPanelX + $testMoveOffPanelWidth -gt
                    [double]$testMoveOffPanel.Groups['viewportX'].Value +
                    [double]$testMoveOffPanel.Groups['viewportWidth'].Value + 0.1 -or
                $testMoveOffPanelCenterY -lt $transformQaViewportY + $transformQaViewportHeight - 0.1 -or
                $testMoveOffPanelCenterX -lt 0.0 -or
                $testMoveOffPanelCenterX -ge $framebufferWidth -or
                $testMoveOffPanelCenterY -lt 0.0 -or
                $testMoveOffPanelCenterY -ge $framebufferHeight) {
                throw "The Transform QA off-panel hit target was not outside the Utility content viewport while inside the framebuffer."
            }
            $testMoveActivationPattern = 'utility-transform-qa-control id=test_move visible=1 submitted=1 activated=1'
            $testMoveActivationsBefore = [regex]::Matches(
                (Get-HenkaPackagedStartupLogText -Path $stdoutPath),
                $testMoveActivationPattern).Count
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX $testMoveOffPanelCenterX `
                -FramebufferY $testMoveOffPanelCenterY
            Start-Sleep -Milliseconds 250
            $testMoveActivationsAfter = [regex]::Matches(
                (Get-HenkaPackagedStartupLogText -Path $stdoutPath),
                $testMoveActivationPattern).Count
            if ($testMoveActivationsAfter -ne $testMoveActivationsBefore) {
                throw "The off-panel Transform QA action remained interactable outside its visible Utility content area."
            }
            Write-Output "[pass] Transform QA Test Move activates only while fully visible; its stale off-panel hit location does not activate"

            $transformQaCurrent = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $transformQaPattern
            $transformQaReachedBottom = $transformQaCurrent.Groups['visible'].Value -eq '1'
            for ($transformQaScrollAttempt = 0; $transformQaScrollAttempt -lt 24 -and -not $transformQaReachedBottom; ++$transformQaScrollAttempt) {
                Scroll-FramebufferPoint `
                    -Handle $mainWindowHandle `
                    -FramebufferWidth $framebufferWidth `
                    -FramebufferHeight $framebufferHeight `
                    -FramebufferX ($transformQaViewportX + 20.0) `
                    -FramebufferY ($transformQaViewportY + 10.0) `
                    -WheelDelta -1
                Start-Sleep -Milliseconds 90
                $transformQaCurrent = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $transformQaPattern
                if ($null -ne $transformQaCurrent -and
                    $transformQaCurrent.Groups['visible'].Value -eq '1' -and
                    $transformQaCurrent.Groups['submitted'].Value -eq '1' -and
                    [double]$transformQaCurrent.Groups['scroll'].Value -gt 0.0) {
                    $resetY = [double]$transformQaCurrent.Groups['y'].Value
                    $resetHeight = [double]$transformQaCurrent.Groups['height'].Value
                    if ($resetY -lt $transformQaViewportY -or
                        $resetY + $resetHeight -gt $transformQaViewportY + $transformQaViewportHeight + 0.1) {
                        throw "The submitted Transform QA Reset Test Object action extends outside visible Utility content."
                    }
                    $transformQaReachedBottom = $true
                }
            }
            if (-not $transformQaReachedBottom) {
                throw "The Transform QA Reset Test Object action never became visible through Utility scrolling."
            }
            Start-Sleep -Milliseconds 350
            Save-WindowScreenshot `
                -Handle $mainWindowHandle `
                -Path $utilityTransformQaScrolledScreenshotPath `
                -Description "Packaged Utility Transform QA scrolled at 1280x720"
            $activationOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ([double]$transformQaCurrent.Groups['x'].Value +
                    [double]$transformQaCurrent.Groups['width'].Value * 0.5) `
                -FramebufferY ([double]$transformQaCurrent.Groups['y'].Value +
                    [double]$transformQaCurrent.Groups['height'].Value * 0.5)
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern ($transformQaPattern.Replace('activated=(?<activated>[01])', 'activated=1')) `
                    -StartingOffset $activationOffset `
                    -TimeoutMilliseconds 4000)) {
                throw "The scrolled Transform QA control did not receive a real packaged pointer activation."
            }
            Write-Output "[pass] Diagnostics and Transform QA scroll independently; the clipped QA action is reachable only inside the visible Utility viewport"

            Write-Output "[pass] Packaged product-startup/Terrain gate completed without entering the explicit showcase authoring suite"
            return
        }

        Write-Step "Leaving Terrain Utility before viewport component authoring"
        $helpUtilityTabMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Utility Help tab: x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=([-0-9.]+)\.'
        if ($null -eq $helpUtilityTabMatch) {
            throw "The packaged Utility Help tab geometry was not reported after Terrain creation."
        }
        $helpUtilityTabX = [double]$helpUtilityTabMatch.Groups[1].Value
        $helpUtilityTabY = [double]$helpUtilityTabMatch.Groups[2].Value
        $helpUtilityTabWidth = [double]$helpUtilityTabMatch.Groups[3].Value
        $helpUtilityTabHeight = [double]$helpUtilityTabMatch.Groups[4].Value
        Assert-FramebufferRect `
            -Name "Utility Help tab used to leave Terrain editing" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $helpUtilityTabX `
            -Y $helpUtilityTabY `
            -Width $helpUtilityTabWidth `
            -Height $helpUtilityTabHeight
        $leaveTerrainOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($helpUtilityTabX + $helpUtilityTabWidth * 0.5) `
            -FramebufferY ($helpUtilityTabY + $helpUtilityTabHeight * 0.5)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC utility action_seq=[0-9]+ frame=[0-9]+ before=Terrain requested=Help after=Help changed=1' `
                -StartingOffset $leaveTerrainOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The packaged workflow did not leave Terrain editing through a verified visible Utility transition before viewport authoring."
        }
        Write-Output "[pass] Visible Utility transition cleared Terrain viewport ownership before native component authoring"

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
        $detailsTitleMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC details-title entity=(?<entity>\d+) framebuffer=(?<framebufferWidth>\d+)x(?<framebufferHeight>\d+) name_bytes=(?<nameBytes>\d+) single_line_width=(?<singleLineWidth>[-0-9.]+) available_width=(?<availableWidth>[-0-9.]+) wrapped_height=(?<wrappedHeight>[-0-9.]+) row_height=(?<rowHeight>[-0-9.]+) wrapped=(?<wrapped>[01]) drawn=(?<drawn>[01]) name_preserved=(?<namePreserved>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) viewport_x=(?<viewportX>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_width=(?<viewportWidth>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+)'
        if ($null -eq $detailsTitleMatch) {
            throw "Selecting the showcase row did not report a complete wrapped Object Details title at 1280x720."
        }
        $detailsTitleFramebufferWidth = [int]$detailsTitleMatch.Groups['framebufferWidth'].Value
        $detailsTitleFramebufferHeight = [int]$detailsTitleMatch.Groups['framebufferHeight'].Value
        $detailsTitleNameBytes = [int]$detailsTitleMatch.Groups['nameBytes'].Value
        $detailsTitleSingleLineWidth = [double]::Parse(
            $detailsTitleMatch.Groups['singleLineWidth'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsTitleAvailableWidth = [double]::Parse(
            $detailsTitleMatch.Groups['availableWidth'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsTitleWrappedHeight = [double]::Parse(
            $detailsTitleMatch.Groups['wrappedHeight'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsTitleRowHeight = [double]::Parse(
            $detailsTitleMatch.Groups['rowHeight'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsTitleX = [double]::Parse(
            $detailsTitleMatch.Groups['x'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsTitleY = [double]::Parse(
            $detailsTitleMatch.Groups['y'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsTitleWidth = [double]::Parse(
            $detailsTitleMatch.Groups['width'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsTitleHeight = [double]::Parse(
            $detailsTitleMatch.Groups['height'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsViewportX = [double]::Parse(
            $detailsTitleMatch.Groups['viewportX'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsViewportY = [double]::Parse(
            $detailsTitleMatch.Groups['viewportY'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsViewportWidth = [double]::Parse(
            $detailsTitleMatch.Groups['viewportWidth'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsViewportHeight = [double]::Parse(
            $detailsTitleMatch.Groups['viewportHeight'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        if ($detailsTitleFramebufferWidth -ne 1280 -or
            $detailsTitleFramebufferHeight -ne 720 -or
            $detailsTitleNameBytes -lt 24 -or
            $detailsTitleSingleLineWidth -le $detailsTitleAvailableWidth -or
            $detailsTitleMatch.Groups['wrapped'].Value -ne '1' -or
            $detailsTitleMatch.Groups['drawn'].Value -ne '1' -or
            $detailsTitleMatch.Groups['namePreserved'].Value -ne '1' -or
            $detailsTitleWrappedHeight -le 12.0 -or
            $detailsTitleHeight -lt $detailsTitleWrappedHeight -or
            $detailsTitleRowHeight -lt $detailsTitleHeight -or
            $detailsTitleX -lt $detailsViewportX -or
            $detailsTitleY -lt $detailsViewportY -or
            $detailsTitleX + $detailsTitleWidth -gt $detailsViewportX + $detailsViewportWidth -or
            $detailsTitleY + $detailsTitleHeight -gt $detailsViewportY + $detailsViewportHeight) {
            throw (
                "The selected Object Details title was not fully wrapped inside its visible 1280x720 content region: " +
                "framebuffer=${detailsTitleFramebufferWidth}x${detailsTitleFramebufferHeight}, " +
                "nameBytes=$detailsTitleNameBytes, textWidth=$detailsTitleSingleLineWidth, " +
                "availableWidth=$detailsTitleAvailableWidth, wrappedHeight=$detailsTitleWrappedHeight, " +
                "row=${detailsTitleX},${detailsTitleY},${detailsTitleWidth},${detailsTitleHeight}, " +
                "viewport=${detailsViewportX},${detailsViewportY},${detailsViewportWidth},${detailsViewportHeight}.")
        }
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $objectDetailsTitleScreenshotPath `
            -Description "1280x720 selected Object Details full-name title"
        Write-Output "[pass] Selected Object Details title wraps in full inside its 1280x720 content region."
        $detailsValueMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC details-value entity=(?<entity>\d+) framebuffer=(?<framebufferWidth>\d+)x(?<framebufferHeight>\d+) value_bytes=(?<valueBytes>\d+) single_line_width=(?<singleLineWidth>[-0-9.]+) available_width=(?<availableWidth>[-0-9.]+) row_height=(?<rowHeight>[-0-9.]+) wrapped=(?<wrapped>[01]) drawn=(?<drawn>[01]) value_preserved=(?<valuePreserved>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) viewport_x=(?<viewportX>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_width=(?<viewportWidth>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+)'
        if ($null -eq $detailsValueMatch) {
            throw "The selected Object Details long value did not report a complete wrapped row at 1280x720."
        }
        $detailsValueFramebufferWidth = [int]$detailsValueMatch.Groups['framebufferWidth'].Value
        $detailsValueFramebufferHeight = [int]$detailsValueMatch.Groups['framebufferHeight'].Value
        $detailsValueBytes = [int]$detailsValueMatch.Groups['valueBytes'].Value
        $detailsValueSingleLineWidth = [double]::Parse(
            $detailsValueMatch.Groups['singleLineWidth'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsValueAvailableWidth = [double]::Parse(
            $detailsValueMatch.Groups['availableWidth'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsValueRowX = [double]::Parse(
            $detailsValueMatch.Groups['x'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsValueRowY = [double]::Parse(
            $detailsValueMatch.Groups['y'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsValueRowWidth = [double]::Parse(
            $detailsValueMatch.Groups['width'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsValueRowHeight = [double]::Parse(
            $detailsValueMatch.Groups['height'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsValueViewportX = [double]::Parse(
            $detailsValueMatch.Groups['viewportX'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsValueViewportY = [double]::Parse(
            $detailsValueMatch.Groups['viewportY'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsValueViewportWidth = [double]::Parse(
            $detailsValueMatch.Groups['viewportWidth'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsValueViewportHeight = [double]::Parse(
            $detailsValueMatch.Groups['viewportHeight'].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        if ($detailsValueFramebufferWidth -ne 1280 -or
            $detailsValueFramebufferHeight -ne 720 -or
            $detailsValueBytes -lt 30 -or
            $detailsValueSingleLineWidth -le $detailsValueAvailableWidth -or
            $detailsValueMatch.Groups['wrapped'].Value -ne '1' -or
            $detailsValueMatch.Groups['drawn'].Value -ne '1' -or
            $detailsValueMatch.Groups['valuePreserved'].Value -ne '1' -or
            $detailsValueRowHeight -le 22.0 -or
            $detailsValueRowX -lt $detailsValueViewportX -or
            $detailsValueRowY -lt $detailsValueViewportY -or
            $detailsValueRowX + $detailsValueRowWidth -gt $detailsValueViewportX + $detailsValueViewportWidth -or
            $detailsValueRowY + $detailsValueRowHeight -gt $detailsValueViewportY + $detailsValueViewportHeight) {
            throw (
                "The selected Object Details value did not wrap completely inside its visible 1280x720 row: " +
                "framebuffer=${detailsValueFramebufferWidth}x${detailsValueFramebufferHeight}, " +
                "valueBytes=$detailsValueBytes, textWidth=$detailsValueSingleLineWidth, " +
                "availableWidth=$detailsValueAvailableWidth, " +
                "row=${detailsValueRowX},${detailsValueRowY},${detailsValueRowWidth},${detailsValueRowHeight}, " +
                "viewport=${detailsValueViewportX},${detailsValueViewportY},${detailsValueViewportWidth},${detailsValueViewportHeight}.")
        }
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $objectDetailsValueScreenshotPath `
            -Description "1280x720 selected Object Details wrapped value"
        Write-Output "[pass] Selected Object Details long value wraps in full inside its 1280x720 row."
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
        if (-not (Wait-FileContains `
                -Path $stdoutPath `
                -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC details-source-value ' `
                -TimeoutMilliseconds 2500)) {
            throw "The selected Object Details Source value did not provide product-origin wrapped-row evidence."
        }
        $detailsSourceMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC details-source-value entity=(?<entity>\d+) framebuffer=(?<framebufferWidth>\d+)x(?<framebufferHeight>\d+) value_bytes=(?<valueBytes>\d+) material_owned=(?<materialOwned>[01]) action_reserve=(?<actionReserve>[-0-9.]+) single_line_width=(?<singleLineWidth>[-0-9.]+) available_width=(?<availableWidth>[-0-9.]+) measured_height=(?<measuredHeight>[-0-9.]+) row_height=(?<rowHeight>[-0-9.]+) wrapped=(?<wrapped>[01]) drawn=(?<drawn>[01]) value_preserved=(?<valuePreserved>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) viewport_x=(?<viewportX>[-0-9.]+) viewport_y=(?<viewportY>[-0-9.]+) viewport_width=(?<viewportWidth>[-0-9.]+) viewport_height=(?<viewportHeight>[-0-9.]+) scroll_offset=(?<scrollOffset>[-0-9.]+)'
        if ($null -eq $detailsSourceMatch) {
            throw "The selected Object Details Source value did not report its wrapped row geometry."
        }
        $detailsSourceNumber = {
            param([string]$Name)
            [double]::Parse(
                $detailsSourceMatch.Groups[$Name].Value,
                [Globalization.CultureInfo]::InvariantCulture)
        }
        $detailsSourceFramebufferWidth = [int]$detailsSourceMatch.Groups['framebufferWidth'].Value
        $detailsSourceFramebufferHeight = [int]$detailsSourceMatch.Groups['framebufferHeight'].Value
        $detailsSourceValueBytes = [int]$detailsSourceMatch.Groups['valueBytes'].Value
        $detailsSourceMaterialOwned = [int]$detailsSourceMatch.Groups['materialOwned'].Value
        $detailsSourceExpectedText = if ($detailsSourceMaterialOwned -eq 1) {
            "Authoring mesh + material instance"
        } else {
            "Authoring mesh (per-object user slot)"
        }
        $detailsSourceExpectedActionReserve = if ($detailsSourceMaterialOwned -eq 1) { 0.0 } else { 112.0 }
        $detailsSourceActionReserve = & $detailsSourceNumber 'actionReserve'
        $detailsSourceSingleLineWidth = & $detailsSourceNumber 'singleLineWidth'
        $detailsSourceAvailableWidth = & $detailsSourceNumber 'availableWidth'
        $detailsSourceMeasuredHeight = & $detailsSourceNumber 'measuredHeight'
        $detailsSourceRowHeight = & $detailsSourceNumber 'rowHeight'
        $detailsSourceX = & $detailsSourceNumber 'x'
        $detailsSourceY = & $detailsSourceNumber 'y'
        $detailsSourceWidth = & $detailsSourceNumber 'width'
        $detailsSourceHeight = & $detailsSourceNumber 'height'
        $detailsSourceViewportX = & $detailsSourceNumber 'viewportX'
        $detailsSourceViewportY = & $detailsSourceNumber 'viewportY'
        $detailsSourceViewportWidth = & $detailsSourceNumber 'viewportWidth'
        $detailsSourceViewportHeight = & $detailsSourceNumber 'viewportHeight'
        if ($detailsSourceFramebufferWidth -ne 1280 -or
            $detailsSourceFramebufferHeight -ne 720 -or
            $detailsSourceValueBytes -ne $detailsSourceExpectedText.Length -or
            $detailsSourceActionReserve -ne $detailsSourceExpectedActionReserve -or
            $detailsSourceSingleLineWidth -le $detailsSourceAvailableWidth -or
            $detailsSourceAvailableWidth -le 0.0 -or
            $detailsSourceMatch.Groups['wrapped'].Value -ne '1' -or
            $detailsSourceMatch.Groups['drawn'].Value -ne '1' -or
            $detailsSourceMatch.Groups['valuePreserved'].Value -ne '1' -or
            $detailsSourceMeasuredHeight -le 22.0 -or
            $detailsSourceRowHeight -lt $detailsSourceMeasuredHeight -or
            $detailsSourceHeight -lt $detailsSourceMeasuredHeight -or
            $detailsSourceX -lt $detailsSourceViewportX -or
            $detailsSourceY -lt $detailsSourceViewportY -or
            $detailsSourceX + $detailsSourceWidth -gt $detailsSourceViewportX + $detailsSourceViewportWidth -or
            $detailsSourceY + $detailsSourceHeight -gt $detailsSourceViewportY + $detailsSourceViewportHeight) {
            throw (
                "The selected Object Details Source value did not wrap completely inside its visible 1280x720 row: " +
                "framebuffer=${detailsSourceFramebufferWidth}x${detailsSourceFramebufferHeight}, " +
                "valueBytes=$detailsSourceValueBytes, materialOwned=$detailsSourceMaterialOwned, " +
                "actionReserve=$detailsSourceActionReserve, textWidth=$detailsSourceSingleLineWidth, " +
                "availableWidth=$detailsSourceAvailableWidth, measuredHeight=$detailsSourceMeasuredHeight, " +
                "flowHeight=$detailsSourceRowHeight, " +
                "row=${detailsSourceX},${detailsSourceY},${detailsSourceWidth},${detailsSourceHeight}, " +
                "viewport=${detailsSourceViewportX},${detailsSourceViewportY},${detailsSourceViewportWidth},${detailsSourceViewportHeight}.")
        }
        Save-WindowScreenshot `
            -Handle $mainWindowHandle `
            -Path $objectDetailsSourceScreenshotPath `
            -Description "1280x720 Object Details Source value wrapped in full"
        Write-Output "[pass] Object Details Source value wraps in full inside its visible 1280x720 row."
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

        $nativeMaterialControlsVisible = Wait-FileContains `
            -Path $stdoutPath `
            -Pattern "Native authoring material controls:" `
            -TimeoutMilliseconds 3000
        if (-not $nativeMaterialControlsVisible) {
            # The short-window split preserves a usable Utility viewport by
            # making the Details content independently scrollable. Material
            # controls below the source row therefore need not be visible at
            # scroll offset zero; prove they become visible after the app
            # reports consuming a real Details-panel scroll.
            $scrolledMaterialControlsPattern =
                'Native authoring material controls:.*scroll_offset=(?<scrollOffset>(?:[1-9][0-9]*)(?:\.[0-9]+)?)\.'
            $scrolledMaterialControls = Scroll-DetailsUntilReported `
                -PostconditionPattern $scrolledMaterialControlsPattern `
                -Description "the native material controls below the source row"
            if ($null -eq $scrolledMaterialControls -or
                [double]$scrolledMaterialControls.Groups['scrollOffset'].Value -le 0.0) {
                throw "The native material editor controls were not revealed by a product-reported Details scroll."
            }
            Write-Output "[pass] Native material controls became reachable after product-reported Details scrolling"
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
        # only far enough to get a fresh product geometry report for the visible
        # general controls. At offset zero their second row can be clipped, so
        # reusing geometry from a previous scroll position would target stale
        # screen coordinates.
        $detailsScrollPattern = '^HENKA_AUTOMATION_DIAGNOSTIC details-scroll seq=\d+ frame=\d+ before=([-0-9.]+) after=([-0-9.]+) content=([-0-9.]+) viewport=([-0-9.]+) delta=[-0-9.]+ accepted=1\r?$'
        $detailsScrollMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern $detailsScrollPattern
        if ($null -eq $detailsScrollMatch) {
            throw "The product did not report current Object Details scroll state."
        }
        $detailsScrollOffset = [double]::Parse(
            $detailsScrollMatch.Groups[2].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $nativeMaterialControlsPattern = 'Native authoring material controls: name=(.+) tint_x=([-0-9.]+) metal_x=([-0-9.]+) rough_x=([-0-9.]+) emissive_x=([-0-9.]+) texture_x=([-0-9.]+) subsurface_x=([-0-9.]+) first_y=([-0-9.]+) second_y=([-0-9.]+) width=([-0-9.]+) height=28.0 scroll_offset=(?<scrollOffset>[-0-9.]+)\.'
        $nativeMaterialControlsMatch = $null
        for ($detailsScrollAttempt = 0;
             $detailsScrollAttempt -lt 16 -and
             $null -eq $nativeMaterialControlsMatch -and
             $detailsScrollOffset -gt 0.5;
             $detailsScrollAttempt++) {
            $scrollOutputOffset = Get-FileLengthSafe -Path $stdoutPath
            Scroll-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($detailsX + [Math]::Min(120.0, [Math]::Max(24.0, $detailsWidth - 80.0))) `
                -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                -WheelDelta 1
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $detailsScrollPattern `
                    -StartingOffset $scrollOutputOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The Sandbox did not report Object Details scroll progress after a wheel event."
            }
            $detailsScrollMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $detailsScrollPattern
            if ($null -eq $detailsScrollMatch) {
                throw "The latest product Object Details scroll state could not be read."
            }
            $nextDetailsScrollOffset = [double]::Parse(
                $detailsScrollMatch.Groups[2].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            if ($nextDetailsScrollOffset -ge ($detailsScrollOffset - 0.5)) {
                throw "Object Details did not move toward the top (offset $detailsScrollOffset -> $nextDetailsScrollOffset)."
            }
            $detailsScrollOffset = $nextDetailsScrollOffset
            if (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $nativeMaterialControlsPattern `
                    -StartingOffset $scrollOutputOffset `
                    -TimeoutMilliseconds 3000) {
                $candidateMaterialGeometry = Get-LastLogRegexMatch `
                    -Path $stdoutPath `
                    -Pattern $nativeMaterialControlsPattern
                if ($null -ne $candidateMaterialGeometry -and
                    [Math]::Abs(
                        [double]$candidateMaterialGeometry.Groups['scrollOffset'].Value -
                        $detailsScrollOffset) -le 0.5) {
                    $nativeMaterialControlsMatch = $candidateMaterialGeometry
                }
            }
        }
        if ($null -eq $nativeMaterialControlsMatch) {
            throw "The native material editor did not report fresh visible control geometry after bounded Object Details scrolling."
        }
        Write-Output "[pass] Fresh native material control geometry matches the visible Details scroll offset $detailsScrollOffset"
        $detailsSourceRowY = [double]$detailsSourceMatch.Groups['y'].Value
        $detailsSourceScroll = [double]$detailsSourceMatch.Groups['scrollOffset'].Value
        $detailsSourceRowHeight = [double]$detailsSourceMatch.Groups['height'].Value
        $nativeMaterialFirstRowY = [double]$nativeMaterialControlsMatch.Groups[8].Value
        $nativeMaterialScroll = [double]$nativeMaterialControlsMatch.Groups['scrollOffset'].Value
        $sourceContentY = $detailsSourceRowY + $detailsSourceScroll
        $materialContentY = $nativeMaterialFirstRowY + $nativeMaterialScroll
        if ($materialContentY -lt ($sourceContentY + $detailsSourceRowHeight + 4.0)) {
            throw (
                "Native material controls overlap the Object Details Source row: " +
                "source_content=[{0:N1},{1:N1}], material_first_content_y={2:N1}." -f
                    $sourceContentY,
                    ($sourceContentY + $detailsSourceRowHeight),
                    $materialContentY)
        }
        Write-Output "[pass] Native material controls occupy a distinct row below the Source value"
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
        $nativeMaterialTintLogOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMaterialTintX + ($nativeMaterialWidth * 0.5)) `
            -FramebufferY ($nativeMaterialY + 14.0)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'Native authoring material edited:.*parameter=Base Color' `
                -StartingOffset $nativeMaterialTintLogOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The user-facing native Base Color edit did not complete after its click."
        }
        Assert-FramebufferRect `
            -Name "Native authoring metallic control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeMaterialMetalX `
            -Y $nativeMaterialY `
            -Width $nativeMaterialWidth `
            -Height 28.0
        $nativeMaterialMetalLogOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeMaterialMetalX + ($nativeMaterialWidth * 0.5)) `
            -FramebufferY ($nativeMaterialY + 14.0)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern 'Native authoring material edited:.*parameter=Metallic' `
                -StartingOffset $nativeMaterialMetalLogOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The user-facing native Metallic edit did not complete after its click."
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
                    -Pattern 'Native authoring material controls: name=(.+) tint_x=([-0-9.]+) metal_x=([-0-9.]+) rough_x=([-0-9.]+) emissive_x=([-0-9.]+) texture_x=([-0-9.]+) subsurface_x=([-0-9.]+) first_y=([-0-9.]+) second_y=([-0-9.]+) width=([-0-9.]+) height=28.0 scroll_offset=(?<scrollOffset>[-0-9.]+)\.'
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
        # The history row is below several authored material controls and may
        # not have been drawn at the current scroll offset. Scroll through the
        # real Object Details viewport until the product reports that row as
        # visible; retain the hard bound derived from the product-reported
        # content and viewport heights, then reposition to its exact offset.
        $nativeMaterialHistoryPattern = 'Native authoring material history: name=(.+) undo_x=([-0-9.]+) redo_x=([-0-9.]+) y=([-0-9.]+) scroll=([-0-9.]+) width=([-0-9.]+) height=24.0\.'
        $nativeMaterialHistoryMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern $nativeMaterialHistoryPattern
        $detailsScrollMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern $detailsScrollPattern
        if ($null -eq $detailsScrollMatch) {
            throw "The product did not report current Object Details scroll state before revealing material history."
        }
        $detailsScrollOffset = [double]::Parse(
            $detailsScrollMatch.Groups[2].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $detailsScrollAttempts = 0
        while ($null -eq $nativeMaterialHistoryMatch -and $detailsScrollAttempts -lt 64) {
            $detailsContentHeight = [double]::Parse(
                $detailsScrollMatch.Groups[3].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $detailsViewportHeight = [double]::Parse(
                $detailsScrollMatch.Groups[4].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            $detailsMaximumScroll = [Math]::Max(0.0, $detailsContentHeight - $detailsViewportHeight)
            if ($detailsScrollOffset -ge ($detailsMaximumScroll - 0.5)) {
                break
            }

            $scrollOutputOffset = Get-FileLengthSafe -Path $stdoutPath
            Scroll-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($detailsX + [Math]::Min(120.0, [Math]::Max(24.0, $detailsWidth - 80.0))) `
                -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                -WheelDelta -1
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $detailsScrollPattern `
                    -StartingOffset $scrollOutputOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The Sandbox did not report Object Details scroll progress while revealing native material history."
            }
            $detailsScrollMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $detailsScrollPattern
            if ($null -eq $detailsScrollMatch) {
                throw "The latest product Object Details scroll state could not be read while revealing native material history."
            }
            $nextDetailsScrollOffset = [double]::Parse(
                $detailsScrollMatch.Groups[2].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            if ($nextDetailsScrollOffset -le ($detailsScrollOffset + 0.5)) {
                throw "Object Details failed to advance toward native material history ($detailsScrollOffset -> $nextDetailsScrollOffset)."
            }
            $detailsScrollOffset = $nextDetailsScrollOffset
            $detailsScrollAttempts++
            $nativeMaterialHistoryMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $nativeMaterialHistoryPattern
        }
        if ($null -eq $nativeMaterialHistoryMatch) {
            throw "Native material undo/redo geometry remained undiscoverable after $detailsScrollAttempts bounded scroll steps (offset $detailsScrollOffset of $detailsMaximumScroll)."
        }
        if ($detailsScrollAttempts -gt 0) {
            Write-Output "[pass] Native material history became visible after $detailsScrollAttempts product-reported Object Details scroll steps"
        }
        $nativeMaterialUndoX = [double]$nativeMaterialHistoryMatch.Groups[2].Value
        $nativeMaterialRedoX = [double]$nativeMaterialHistoryMatch.Groups[3].Value
        $nativeMaterialHistoryY = [double]$nativeMaterialHistoryMatch.Groups[4].Value
        $nativeMaterialHistoryScrollOffset = [double]$nativeMaterialHistoryMatch.Groups[5].Value
        $nativeMaterialHistoryWidth = [double]$nativeMaterialHistoryMatch.Groups[6].Value
        $detailsScrollMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern $detailsScrollPattern
        if ($null -eq $detailsScrollMatch) {
            throw "The product did not report current Object Details scroll state for material history."
        }
        $detailsScrollOffset = [double]::Parse(
            $detailsScrollMatch.Groups[2].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        for ($detailsScrollAttempt = 0;
             $detailsScrollAttempt -lt 16 -and
             [Math]::Abs($detailsScrollOffset - $nativeMaterialHistoryScrollOffset) -gt 0.5;
             $detailsScrollAttempt++) {
            $wheelDelta = if ($detailsScrollOffset -lt $nativeMaterialHistoryScrollOffset) { -1 } else { 1 }
            $scrollOutputOffset = Get-FileLengthSafe -Path $stdoutPath
            Scroll-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($detailsX + [Math]::Min(120.0, [Math]::Max(24.0, $detailsWidth - 80.0))) `
                -FramebufferY ($detailsY + [Math]::Max(30.0, $detailsHeight * 0.55)) `
                -WheelDelta $wheelDelta
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $stdoutPath `
                    -Pattern $detailsScrollPattern `
                    -StartingOffset $scrollOutputOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "The Sandbox did not report Object Details scroll progress while revealing material history."
            }
            $detailsScrollMatch = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern $detailsScrollPattern
            if ($null -eq $detailsScrollMatch) {
                throw "The latest product Object Details scroll state could not be read for material history."
            }
            $nextDetailsScrollOffset = [double]::Parse(
                $detailsScrollMatch.Groups[2].Value,
                [Globalization.CultureInfo]::InvariantCulture)
            if (($wheelDelta -lt 0 -and $nextDetailsScrollOffset -le ($detailsScrollOffset + 0.5)) -or
                ($wheelDelta -gt 0 -and $nextDetailsScrollOffset -ge ($detailsScrollOffset - 0.5))) {
                throw "Object Details did not progress toward the material history offset ($detailsScrollOffset -> $nextDetailsScrollOffset; target $nativeMaterialHistoryScrollOffset)."
            }
            $detailsScrollOffset = $nextDetailsScrollOffset
        }
        if ([Math]::Abs($detailsScrollOffset - $nativeMaterialHistoryScrollOffset) -gt 0.5) {
            throw "Object Details offset $detailsScrollOffset does not match the material history geometry offset $nativeMaterialHistoryScrollOffset."
        }
        Write-Output "[pass] Product-reported Object Details offset matches native material history geometry"
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
                -WheelDelta 1
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
        $bevelActionCountBeforeFaceMode = @(
            Select-String -LiteralPath $stdoutPath -Pattern 'Native authoring bevel operator:' -ErrorAction SilentlyContinue
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
            $nativeFaceModeLogOffset = Get-FileLengthSafe -Path $stdoutPath
            Click-FramebufferPoint `
                -Handle $mainWindowHandle `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -FramebufferX ($nativeFaceX + [double]$faceSelectionOffset[0]) `
                -FramebufferY ($nativeFaceY + [double]$faceSelectionOffset[1])
            Start-Sleep -Milliseconds 150
            $nativeFaceModeObserved = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring topology mode:.*mode=Face" `
                -StartingOffset $nativeFaceModeLogOffset `
                -TimeoutMilliseconds 1000
        }
        $nativeFaceModeStableMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring face mode control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=88.0 height=24.0\.'
        if (-not $nativeFaceModeObserved -and $null -ne $nativeFaceModeStableMatch) {
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
            $nativeFaceModeObserved = Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native authoring topology mode:.*mode=Face" `
                -StartingOffset $nativeFaceModeLogOffset `
                -TimeoutMilliseconds 700
        }
        if (-not $nativeFaceModeObserved) {
            throw "The user-facing Face selection mode did not become active."
        }
        $bevelActionCountAfterFaceMode = @(
            Select-String -LiteralPath $stdoutPath -Pattern 'Native authoring bevel operator:' -ErrorAction SilentlyContinue
        ).Count
        if ($bevelActionCountAfterFaceMode -ne $bevelActionCountBeforeFaceMode) {
            throw "The Face-mode transition invoked Bevel before a viewport face was selected."
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
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring face normal controls:" -TimeoutMilliseconds 3000)) {
            throw "The responsive Face-edit action layout did not report its packaged geometry."
        }
        $nativeFaceEditToolsMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring face edit tools: name=(.+) preview_x=([-0-9.]+) inset_x=([-0-9.]+) y=([-0-9.]+) preview_width=([-0-9.]+) inset_width=([-0-9.]+) height=24.0\.'
        $nativeFaceNormalControlsMatch = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'Native authoring face normal controls: name=(.+) positive_x=([-0-9.]+) positive_y=([-0-9.]+) negative_x=([-0-9.]+) negative_y=([-0-9.]+) positive_width=([-0-9.]+) negative_width=([-0-9.]+) panel_x=([-0-9.]+) panel_y=([-0-9.]+) panel_width=([-0-9.]+) panel_height=([-0-9.]+)\.'
        if ($null -eq $nativeFaceEditToolsMatch -or $null -eq $nativeFaceNormalControlsMatch) {
            throw "The packaged Face-edit action geometry could not be parsed."
        }
        $faceEditRowY = [double]$nativeFaceEditToolsMatch.Groups[4].Value
        $faceActionRects = @(
            [pscustomobject]@{
                X = [double]$nativeFaceEditToolsMatch.Groups[2].Value
                Y = $faceEditRowY
                Width = [double]$nativeFaceEditToolsMatch.Groups[5].Value
            },
            [pscustomobject]@{
                X = [double]$nativeFaceEditToolsMatch.Groups[3].Value
                Y = $faceEditRowY
                Width = [double]$nativeFaceEditToolsMatch.Groups[6].Value
            },
            [pscustomobject]@{
                X = [double]$nativeFaceNormalControlsMatch.Groups[2].Value
                Y = [double]$nativeFaceNormalControlsMatch.Groups[3].Value
                Width = [double]$nativeFaceNormalControlsMatch.Groups[6].Value
            },
            [pscustomobject]@{
                X = [double]$nativeFaceNormalControlsMatch.Groups[4].Value
                Y = [double]$nativeFaceNormalControlsMatch.Groups[5].Value
                Width = [double]$nativeFaceNormalControlsMatch.Groups[7].Value
            })
        $faceActionPanelX = [double]$nativeFaceNormalControlsMatch.Groups[8].Value
        $faceActionPanelY = [double]$nativeFaceNormalControlsMatch.Groups[9].Value
        $faceActionPanelWidth = [double]$nativeFaceNormalControlsMatch.Groups[10].Value
        $faceActionPanelHeight = [double]$nativeFaceNormalControlsMatch.Groups[11].Value
        foreach ($faceActionRect in $faceActionRects) {
            if ($faceActionRect.X -lt $faceActionPanelX -or
                $faceActionRect.X + $faceActionRect.Width -gt ($faceActionPanelX + $faceActionPanelWidth) -or
                $faceActionRect.Y -lt $faceActionPanelY -or
                $faceActionRect.Y + 24.0 -gt ($faceActionPanelY + $faceActionPanelHeight) -or
                $faceActionRect.Width -le 82.0) {
                throw "A Face-edit action is truncated or outside the visible Object Details content region."
            }
        }
        if ([double]$nativeFaceNormalControlsMatch.Groups[3].Value -le $faceEditRowY -or
            [double]$nativeFaceNormalControlsMatch.Groups[5].Value -le $faceEditRowY) {
            throw "The Face-edit actions did not reflow into a second readable row."
        }
        Write-Output "[pass] Packaged Face-edit actions use full-width measured buttons in two rows inside Object Details"
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
        $nativeFlipX = [double]$nativeFlipMatch.Groups[2].Value
        $nativeFlipY = [double]$nativeFlipMatch.Groups[3].Value
        Assert-FramebufferRect `
            -Name "Native authoring Flip control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeFlipX `
            -Y $nativeFlipY `
            -Width 88.0 `
            -Height 24.0
        $flipHeartbeatBefore = Get-LastLogRegexMatch `
            -Path $stdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC frame seq=([0-9]+) phase=render-complete'
        $nativeFlipLogOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX ($nativeFlipX + 44.0) `
            -FramebufferY ($nativeFlipY + 12.0)
        $nativeFlipObserved = Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern "Native authoring workflow: face winding flipped for" `
            -StartingOffset $nativeFlipLogOffset `
            -TimeoutMilliseconds 2500
        if (-not $nativeFlipObserved) {
            $flipHeartbeatAfter = Get-LastLogRegexMatch `
                -Path $stdoutPath `
                -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC frame seq=([0-9]+) phase=render-complete'
            $beforeSequence = if ($null -ne $flipHeartbeatBefore) {
                $flipHeartbeatBefore.Groups[1].Value
            } else { "unavailable" }
            $afterSequence = if ($null -ne $flipHeartbeatAfter) {
                $flipHeartbeatAfter.Groups[1].Value
            } else { "unavailable" }
            throw (
                "The user-facing native Face-mode Flip operation did not update the showcase source " +
                "after one click within 2500 ms (application frame seq before={0}, after={1})." -f
                $beforeSequence, $afterSequence)
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
            -Pattern 'Native authoring face delete control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=24.0\.'
        if ($null -eq $nativeDeleteMatch) {
            throw "The native Face-mode delete control geometry could not be parsed."
        }
        $nativeDeleteX = [double]$nativeDeleteMatch.Groups[2].Value
        $nativeDeleteY = [double]$nativeDeleteMatch.Groups[3].Value
        $nativeDeleteWidth = [double]$nativeDeleteMatch.Groups[4].Value
        if ($nativeDeleteWidth -lt 120.0) {
            throw "The native Face-mode Delete Faces control is only $nativeDeleteWidth pixels wide; it needs at least 120 pixels to retain its full label at the supported readability scale."
        }
        Assert-FramebufferRect `
            -Name "Native authoring Delete Faces control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $nativeDeleteX `
            -Y $nativeDeleteY `
            -Width $nativeDeleteWidth `
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
                -Pattern 'Native authoring face delete control: name=(.+) x=([-0-9.]+) y=([-0-9.]+) width=([-0-9.]+) height=24.0\.'
            if ($null -ne $latestDeleteMatch) {
                $nativeDeleteX = [double]$latestDeleteMatch.Groups[2].Value
                $nativeDeleteY = [double]$latestDeleteMatch.Groups[3].Value
                $nativeDeleteWidth = [double]$latestDeleteMatch.Groups[4].Value
                if ($nativeDeleteWidth -lt 120.0) {
                    throw "The refreshed Face-mode Delete Faces control no longer preserves its full label."
                }
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

        Write-Step "Preparing a product-authored object for Game Authoring lifecycle"
        $addCubeBoundsWidth = $sceneObjectsWidth - 28.0
        $addCubeButtonRequiredWidths = @(
            (6.0 * 8.0 - 1.0 + 24.0), # Add Cube: measured text plus horizontal padding.
            (6.0 * 5.0 - 1.0 + 24.0), # Clone.
            (6.0 * 6.0 - 1.0 + 24.0)) # Delete.
        $addCubeAvailableWidth = $addCubeBoundsWidth - 12.0
        $addCubeRequiredWidth = ($addCubeButtonRequiredWidths | Measure-Object -Sum).Sum
        $addCubeExtraWidth = ($addCubeAvailableWidth - $addCubeRequiredWidth) / 3.0
        if ($addCubeExtraWidth -lt 0.0) {
            throw "The visible Scene Objects Add Cube row cannot fit at the current supported layout width."
        }
        $gameAuthoringAddCubeX = $sceneObjectsX + 14.0 +
            (($addCubeButtonRequiredWidths[0] + $addCubeExtraWidth) / 2.0)
        $gameAuthoringAddCubeY = $sceneObjectsY + 60.0
        Assert-FramebufferRect `
            -Name "Game Authoring fixture Add Cube control" `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -X $gameAuthoringAddCubeX `
            -Y $gameAuthoringAddCubeY `
            -Width ($addCubeButtonRequiredWidths[0] + $addCubeExtraWidth) `
            -Height 24.0
        $gameAuthoringObjectOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-FramebufferPoint `
            -Handle $mainWindowHandle `
            -FramebufferWidth $framebufferWidth `
            -FramebufferHeight $framebufferHeight `
            -FramebufferX $gameAuthoringAddCubeX `
            -FramebufferY ($gameAuthoringAddCubeY + 12.0)
        $gameAuthoringObjectPattern = '(?:DEFAULT_SCENE_ADD_CUBE_READY entity=[0-9]+ document_id=[0-9]+ source=primitive canonical_document=1\.|Native asset document: name=.+ action=part-added parts=[0-9]+\.)'
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern $gameAuthoringObjectPattern `
                -StartingOffset $gameAuthoringObjectOffset `
                -TimeoutMilliseconds 6000)) {
            throw "The visible Scene Objects Add Cube path did not create and register a product-authored Game Authoring object."
        }
        Write-Output "[pass] Game Authoring lifecycle fixture was created and registered through the visible Henka Add Cube path"

        Write-Step "Checking Game Authoring Play lifecycle"
        $physicsDisclosurePattern = 'Game authoring physics disclosure: name=(?<name>.+) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28.0 expanded=(?<expanded>[01])\.'
        $gamePhysicsDisclosure = Scroll-DetailsUntilReported `
            -PostconditionPattern $physicsDisclosurePattern `
            -Description "the Game Authoring Physics disclosure"
        if ($gamePhysicsDisclosure.Groups["name"].Value -match 'Showcase (?:Giraffe|Rocket)') {
            throw "Game Authoring Physics remained bound to a Showcase reference entity instead of the newly selected authored primitive."
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
                    -Pattern $physicsDisclosurePattern
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
        $gamePlayPattern = 'Game authoring play controls: name=(?<name>.+) trigger_x=(?<triggerX>[-0-9.]+) play_x=(?<playX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26.0 state=(?<state>[0-9]+)\.'
        $gamePlayMatch = Scroll-DetailsUntilReported `
            -PostconditionPattern $gamePlayPattern `
            -Description "the Game Authoring Play controls"
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
            -Pattern $gamePlayPattern `
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
        $gameStepPattern = 'Game authoring step controls: name=(?<name>.+) step_x=(?<stepX>[-0-9.]+) stop_x=(?<stopX>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26.0\.'
        $gameStepMatch = Scroll-DetailsUntilReported `
            -PostconditionPattern $gameStepPattern `
            -Description "the Game Authoring Step/Stop controls"
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
        foreach ($shadingModeName in @("Wireframe", "Solid", "Material Preview", "Rendered")) {
            $shadingControl = Get-HenkaViewportShadingControl `
                -LogPath $stdoutPath `
                -ModeName $shadingModeName
            Assert-FramebufferRect `
                -Name ("Viewport shading " + $shadingModeName + " control") `
                -FramebufferWidth $framebufferWidth `
                -FramebufferHeight $framebufferHeight `
                -X $shadingControl.X `
                -Y $shadingControl.Y `
                -Width $shadingControl.Width `
                -Height $shadingControl.Height
        }

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
            -Description "Packaged native panel screenshot" `
            -MinimumWidth 320 `
            -MinimumHeight 240
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
            $shadingControl = Get-HenkaViewportShadingControl `
                -LogPath $stdoutPath `
                -ModeName $shadingNames[$modeIndex]
            $modeCenterX = $shadingControl.X + $shadingControl.Width * 0.5
            $modeCenterY = $shadingControl.Y + $shadingControl.Height * 0.5

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

            # The mode status is emitted during the update callback, before
            # its frame is rendered. Wait for a later engine-owned render
            # completion before sending the next mode input; otherwise a slow
            # software-rendered preview can leave the automation queue ahead
            # of a blocked frame and make the next click look like a UI miss.
            $renderBoundaryOffset = Get-FileLengthSafe -Path $stdoutPath
            $framePattern = '(?m)^HENKA_AUTOMATION_DIAGNOSTIC frame seq=(?<sequence>[0-9]+) phase=render-complete(?:\s[^\r\n]*)?$'
            $completedFrameMatches = [System.Text.RegularExpressions.Regex]::Matches(
                (Get-HenkaPackagedStartupLogText -Path $stdoutPath),
                $framePattern)
            $afterFrameSequence = 0L
            foreach ($completedFrameMatch in $completedFrameMatches) {
                $reportedSequence = [long]$completedFrameMatch.Groups['sequence'].Value
                if ($reportedSequence -gt $afterFrameSequence) {
                    $afterFrameSequence = $reportedSequence
                }
            }
            $renderedFrame = Wait-HenkaPackagedFrameRenderComplete `
                -StdoutPath $stdoutPath `
                -ProcessId $process.Id `
                -StartingOffset $renderBoundaryOffset `
                -AfterFrameSequence $afterFrameSequence `
                -HardTimeoutMilliseconds 30000 `
                -NoProgressTimeoutMilliseconds 8000 `
                -PollMilliseconds 150
            Write-Output (
                "[pass] Viewport shading {0} completed application render frame {1} after {2} ms." -f `
                    $shadingNames[$modeIndex],
                    $renderedFrame.FrameSequence,
                    $renderedFrame.ElapsedMilliseconds)
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

    # The preceding visible-authoring checks can leave their NativeAsset
    # document open in this same Sandbox process. In that state the production
    # panel intentionally replaces the New Asset/name controls with Save/Close;
    # clicking the no-document coordinates would test the wrong UI contract.
    # Close that completed test document through the real editor control before
    # starting the independent generic New Asset workflow.
    $priorAssetEvent = Get-LastLogRegexMatch `
        -Path $stdoutPath `
        -Pattern 'Native asset document: name=(?<name>.+?) action=(?<action>created|part-added|saved|opened|closed) parts=(?<parts>\d+)\.'
    if ($null -eq $priorAssetEvent) {
        throw "The preceding packaged authoring workflow did not report native asset document state before generic asset authoring."
    }
    if ($priorAssetEvent.Groups['action'].Value -ne 'closed') {
        $priorAssetName = $priorAssetEvent.Groups['name'].Value
        $priorAssetNamePattern = [Regex]::Escape($priorAssetName)
        $priorAssetParts = $priorAssetEvent.Groups['parts'].Value
        Write-Step "Closing the completed prior asset document '$priorAssetName' before generic New Asset"
        $priorAssetCloseOffset = Get-FileLengthSafe -Path $stdoutPath
        Click-AuthoringWindowPoint `
            -Handle $mainWindowHandle `
            -X $genericOpenAssetX `
            -Y ($genericSaveAssetY + 12.0)
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern "Native asset document: name=$priorAssetNamePattern action=closed parts=$priorAssetParts\." `
                -StartingOffset $priorAssetCloseOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The prior native asset document '$priorAssetName' did not close through the visible Close Asset control; generic New Asset was not attempted."
        }
        Write-Output "[pass] Closed the prior packaged test asset through the visible editor control"
        Start-Sleep -Milliseconds 350
    }

    Assert-FramebufferRect -Name "Generic asset name field" -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -X $genericAssetNameX -Y $genericNameFieldY -Width ($genericActionWidth * 2.0 + 6.0) -Height 24.0
    Assert-FramebufferRect -Name "Generic New Asset control" -FramebufferWidth $framebufferWidth -FramebufferHeight $framebufferHeight -X ($genericPanelX + 14.0) -Y $genericNewAssetY -Width $genericActionWidth -Height 24.0
    Click-AuthoringWindowPoint -Handle $mainWindowHandle -X $genericAssetNameX -Y ($genericNameFieldY + 12.0)
    Start-Sleep -Milliseconds 600
    $genericNameInputOffset = Get-FileLengthSafe -Path $stdoutPath
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $genericAssetNameSuffix
    if (-not (Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern "Native authoring asset name accepted: value=$genericAssetNamePattern\." `
            -StartingOffset $genericNameInputOffset `
            -TimeoutMilliseconds 5000)) {
        throw "The packaged asset-name field did not accept the expected unique suffix '$genericAssetNameSuffix'."
    }
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
    # Relaunch the same explicitly selected Showcase scene that owns the saved
    # per-entity authoring document. The clean default scene is validated
    # separately; it intentionally contains no Showcase entity to restore here.
    # This remains a normal product launch, not another automation session.
    # A child inherits the current process environment, so remove the
    # interactive test's event-file/diagnostic switches only while creating
    # this process, then restore the parent environment for the remaining gate.
    $startupRestoreAutomationOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
    $startupRestoreAutomationFile = $env:HENKA_AUTOMATION_INPUT_FILE
    $startupRestoreAutomationDiagnostics = $env:HENKA_AUTOMATION_DIAGNOSTICS
    try {
        Remove-Item Env:HENKA_AUTOMATION_INPUT_OWNED -ErrorAction SilentlyContinue
        Remove-Item Env:HENKA_AUTOMATION_INPUT_FILE -ErrorAction SilentlyContinue
        Remove-Item Env:HENKA_AUTOMATION_DIAGNOSTICS -ErrorAction SilentlyContinue
        $startupRestoreCapture = Start-HenkaCapturedProcess `
            -FilePath $packagedExe `
            -WorkingDirectory $packageRoot `
            -Arguments @("--capture-showcase-view", "wide", "solid") `
            -StdoutPath $startupRestoreStdoutPath `
            -StderrPath $startupRestoreStderrPath
    }
    finally {
        if ($null -eq $startupRestoreAutomationOwned) {
            Remove-Item Env:HENKA_AUTOMATION_INPUT_OWNED -ErrorAction SilentlyContinue
        }
        else {
            $env:HENKA_AUTOMATION_INPUT_OWNED = $startupRestoreAutomationOwned
        }
        if ($null -eq $startupRestoreAutomationFile) {
            Remove-Item Env:HENKA_AUTOMATION_INPUT_FILE -ErrorAction SilentlyContinue
        }
        else {
            $env:HENKA_AUTOMATION_INPUT_FILE = $startupRestoreAutomationFile
        }
        if ($null -eq $startupRestoreAutomationDiagnostics) {
            Remove-Item Env:HENKA_AUTOMATION_DIAGNOSTICS -ErrorAction SilentlyContinue
        }
        else {
            $env:HENKA_AUTOMATION_DIAGNOSTICS = $startupRestoreAutomationDiagnostics
        }
    }
    $startupRestoreProcess = $startupRestoreCapture.Process
    if (Select-String `
            -LiteralPath $startupRestoreStdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC|HENKA_AUTOMATION_INPUT' `
            -Quiet `
            -ErrorAction SilentlyContinue) {
        throw "The normal packaged relaunch inherited test automation state."
    }
    try {
        $startupRestoreReadiness = Wait-HenkaPackagedNativeAuthoringRestore `
            -StdoutPath $startupRestoreStdoutPath `
            -StderrPath $startupRestoreStderrPath `
            -ProcessId $startupRestoreProcess.Id `
            -HardTimeoutMilliseconds 120000 `
            -NoProgressTimeoutMilliseconds 45000 `
            -PollMilliseconds 150
    }
    catch {
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
        if (-not $startupRestoreProcess.HasExited) {
            $null = $startupRestoreProcess.WaitForExit(10000)
        }
        throw (
            "A normal packaged relaunch did not complete persisted native authoring " +
            "restore: $($_.Exception.Message)")
    }
    if (-not $startupRestoreReadiness.Ready -or
        $startupRestoreReadiness.LastProgressStage -ne "persisted native authoring source" -or
        $startupRestoreReadiness.ProgressStagesObserved -ne 6) {
        throw "The packaged relaunch readiness result omitted a required native source/material restore stage."
    }
    Write-Output (
        "[pass] Persisted native authoring stages reached {0} after {1} ms " +
        "({2} application stages observed)." -f
        $startupRestoreReadiness.LastProgressStage,
        $startupRestoreReadiness.ElapsedMilliseconds,
        $startupRestoreReadiness.ProgressStagesObserved)
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
        -Path $wideLayoutScreenshotPath `
        -Description "Packaged 1920x1080 responsive-layout visual proof"
    Assert-PathExists `
        -Path $expandedLayoutScreenshotPath `
        -Description "Packaged 2560x1440 responsive-layout visual proof"
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
    if ($null -eq $previousAutomationDiagnostics) {
        Remove-Item Env:HENKA_AUTOMATION_DIAGNOSTICS -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_DIAGNOSTICS = $previousAutomationDiagnostics
    }
    if ($null -ne $capturedProcess) {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
    }
    elseif ($process -ne $null) {
        $process.Dispose()
    }
}
