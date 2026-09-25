[CmdletBinding()]
param(
    [string]$ExecutablePath = "build\examples\sandbox3d\Debug\henka_sandbox3d.exe",
    [string]$OutputDirectory = "build\test_tmp\visible-native-modeling"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Get-Location).Path
$executable = (Resolve-Path -LiteralPath (Join-Path $repoRoot $ExecutablePath)).Path
$outputRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
$runtimeDirectory = Join-Path $outputRoot ("runtime-" + [Guid]::NewGuid().ToString("N"))
$runtimeExecutable = Join-Path $runtimeDirectory "henka_sandbox3d.exe"
$stdoutPath = Join-Path $runtimeDirectory "stdout.log"
$stderrPath = Join-Path $runtimeDirectory "stderr.log"
$automationInputPath = Join-Path $runtimeDirectory "automation-input.events"
$assetName = "VisibleModel_" + ([Guid]::NewGuid().ToString("N").Substring(0, 12))
$capturedProcess = $null
$previousAutomationOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
$previousAutomationFile = $env:HENKA_AUTOMATION_INPUT_FILE

. (Join-Path $repoRoot "scripts\henka_script_common.ps1")
. (Join-Path $repoRoot "scripts\henka_ui_automation_helpers.ps1")

function Get-LastMatch {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern
    )

    $match = Get-LastLogRegexMatch -Path $Path -Pattern $Pattern
    if ($null -eq $match) {
        throw "The editor did not report the required geometry: $Pattern"
    }
    return $match
}

function Read-SharedLogText {
    param([Parameter(Mandatory = $true)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }
    $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite)
    try {
        $reader = New-Object System.IO.StreamReader($stream)
        try {
            $text = $reader.ReadToEnd()
        }
        finally {
            $reader.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
    return $text
}

function Get-LogMatchCount {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern
    )

    return [Regex]::Matches(
        (Read-SharedLogText -Path $Path),
        $Pattern).Count
}

function Wait-LogMatchCountIncrease {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][int]$InitialCount,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][int]$TimeoutMilliseconds
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    do {
        if ((Get-LogMatchCount -Path $Path -Pattern $Pattern) -gt $InitialCount) {
            return $true
        }
        Start-Sleep -Milliseconds 50
    } while ([DateTime]::UtcNow -lt $deadline)
    return $false
}

function Clear-TextField {
    param([Parameter(Mandatory = $true)][string]$EventPath)

    # NativeAsset is the bounded first-run default. Keep enough erases to
    # replace it without queueing dozens of redundant per-frame events on the
    # slowest supported Debug renderer.
    for ($index = 0; $index -lt 16; ++$index) {
        Send-HenkaAutomationEvent -EventPath $EventPath -EventLine "key Backspace down" -SettleMilliseconds 0
        Send-HenkaAutomationEvent -EventPath $EventPath -EventLine "key Backspace up" -SettleMilliseconds 0
    }
}

function Save-ProbeWindowScreenshot {
    param(
        [Parameter(Mandatory = $true)][System.IntPtr]$Handle,
        [Parameter(Mandatory = $true)][string]$Path
    )

    Add-Type -AssemblyName System.Drawing
    if (-not ("HenkaVisibleProbeNative" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class HenkaVisibleProbeNative
{
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool IsIconic(IntPtr hWnd);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint flags);
}
"@
    }

    if (-not [HenkaVisibleProbeNative]::IsWindowVisible($Handle) -or
        [HenkaVisibleProbeNative]::IsIconic($Handle)) {
        throw "The visible authoring probe window is hidden or minimized; no focus change was attempted."
    }
    $rect = New-Object HenkaVisibleProbeNative+RECT
    if (-not [HenkaVisibleProbeNative]::GetWindowRect($Handle, [ref]$rect)) {
        throw "The visible authoring probe window bounds could not be read."
    }
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0) {
        throw "The visible authoring probe window bounds are invalid."
    }
    $bitmap = New-Object System.Drawing.Bitmap -ArgumentList $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $deviceContext = $graphics.GetHdc()
        try {
            if (-not [HenkaVisibleProbeNative]::PrintWindow($Handle, $deviceContext, 2)) {
                throw "The visible authoring probe window did not render through background-safe PrintWindow; no focus change was attempted."
            }
        }
        finally {
            $graphics.ReleaseHdc($deviceContext) | Out-Null
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Click-LoggedControl {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$XGroup,
        [Parameter(Mandatory = $true)][string]$YGroup,
        [double]$XOffset = 12.0,
        [double]$YOffset = 12.0
    )

    $match = Get-LastMatch -Path $LogPath -Pattern $Pattern
    Send-HenkaAutomationClick `
        -EventPath $EventPath `
        -X ([double]$match.Groups[$XGroup].Value + $XOffset) `
        -Y ([double]$match.Groups[$YGroup].Value + $YOffset)
}

function Invoke-VisibleFaceExtrude {
    param(
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$XGroup,
        [Parameter(Mandatory = $true)][string]$YGroup,
        [Parameter(Mandatory = $true)][string]$FailureMessage
    )

    $previewPattern = 'Native authoring face extrude preview:.*result=success\b'
    $commitPattern = 'Native authoring face extrude request:.*result=success\b'
    $previewCount = Get-LogMatchCount -Path $stdoutPath -Pattern $previewPattern
    Click-LoggedControl `
        -LogPath $stdoutPath `
        -EventPath $automationInputPath `
        -Pattern $Pattern `
        -XGroup $XGroup -YGroup $YGroup
    if (-not (Wait-LogMatchCountIncrease `
            -Path $stdoutPath `
            -InitialCount $previewCount `
            -Pattern $previewPattern `
            -TimeoutMilliseconds 5000)) {
        throw $FailureMessage
    }
    $commitCount = Get-LogMatchCount -Path $stdoutPath -Pattern $commitPattern
    Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "Enter"
    if (-not (Wait-LogMatchCountIncrease `
            -Path $stdoutPath `
            -InitialCount $commitCount `
            -Pattern $commitPattern `
            -TimeoutMilliseconds 5000)) {
        throw $FailureMessage
    }
}

function Wait-AssetTransition {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Action,
        [Parameter(Mandatory = $true)][int]$PartCount
    )

    $pattern = "Native asset document: name=$([Regex]::Escape($assetName)) action=$Action parts=$PartCount\."
    if (-not (Wait-FileContains -Path $LogPath -Pattern $pattern -TimeoutMilliseconds 15000)) {
        throw "The editor did not report the native asset transition: $Action/$PartCount."
    }
}

function Wait-AssetNameInput {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Name
    )

    $pattern = "Native authoring asset name accepted: value=$([Regex]::Escape($Name))\."
    if (-not (Wait-FileContains -Path $LogPath -Pattern $pattern -TimeoutMilliseconds 60000)) {
        throw "The editor did not report the requested native asset name was accepted."
    }
}

try {
    $interactionToolsPath = Join-Path $repoRoot "examples\sandbox3d\interaction_tools.h"
    $sandboxSourcePath = Join-Path $repoRoot "examples\sandbox3d\main.c"
    $interactionToolsText = [System.IO.File]::ReadAllText($interactionToolsPath)
    $sandboxSourceText = [System.IO.File]::ReadAllText($sandboxSourcePath)
    if ($interactionToolsText -notmatch '(?m)^#define SANDBOX3D_AUTHORING_TOPOLOGY_OVERLAY_DEFAULT false\s*$') {
        throw "The topology diagnostic overlay must be disabled by default."
    }
    if ($sandboxSourceText -match 'static bool authoring_topology_overlay_enabled') {
        throw "The topology diagnostic overlay state must be owned by the editor session."
    }
    if ($sandboxSourceText -notmatch 'bool authoring_topology_overlay_enabled;') {
        throw "The editor session is missing topology diagnostic overlay state."
    }
    if ($sandboxSourceText -notmatch '&state->authoring_topology_overlay_enabled') {
        throw "The topology overlay control is not bound to editor session state."
    }
    if ($sandboxSourceText -notmatch '(?s)if\s*\(\s*state->authoring_topology_overlay_enabled\s*&&\s*sandbox3d_build_authoring_cage') {
        throw "The full authored cage must be gated by the explicit topology overlay."
    }
    if ($sandboxSourceText -match 'selection_mode\s*==\s*SANDBOX3D_AUTHORING_SELECTION_VERTEX\s*\|\|\s*point->loose\s*\|\|\s*state->authoring_topology_overlay_enabled') {
        throw "Default Edit mode must not expose every vertex marker."
    }
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "The Sandbox3D executable was not found: $executable"
    }

    New-Item -ItemType Directory -Path $runtimeDirectory -Force | Out-Null
    Copy-Item -LiteralPath $executable -Destination $runtimeExecutable
    Copy-Item `
        -LiteralPath (Join-Path (Split-Path -Parent $executable) "assets") `
        -Destination (Join-Path $runtimeDirectory "assets") `
        -Recurse
    $softwareOpenGLRoot = [string]$env:HENKA_CI_SOFTWARE_OPENGL_ROOT
    if (-not [string]::IsNullOrWhiteSpace($softwareOpenGLRoot)) {
        $softwareOpenGLInstaller = Join-Path $repoRoot "scripts\install_windows_software_opengl.ps1"
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $softwareOpenGLInstaller `
            -SourceDirectory $softwareOpenGLRoot -TargetDirectory $runtimeDirectory
        if ($LASTEXITCODE -ne 0) {
            throw "The CI-only OpenGL runtime could not be installed into the visible modeling runtime."
        }
    }
    New-Item -ItemType File -Path $automationInputPath -Force | Out-Null

    $env:HENKA_AUTOMATION_INPUT_OWNED = "1"
    $env:HENKA_AUTOMATION_INPUT_FILE = $automationInputPath
    # This is the bounded exception for the real visible-authoring gate:
    # PrintWindow and the native UI interaction proof require a visible,
    # non-minimized render surface. Ordinary automated validation keeps
    # the shared default minimized/background-safe policy.
    $capturedProcess = Start-HenkaCapturedProcess `
        -FilePath $runtimeExecutable `
        -Arguments @() `
        -WorkingDirectory $runtimeDirectory `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath `
        -StartMinimized:$false

    # Debug OpenGL startup on slower integrated GPUs can finish engine
    # creation near the existing timeout and still need a bounded first frame
    # to publish the UI geometry report. Keep this bounded, but do not turn a
    # slow renderer startup into a false harness failure.
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -TimeoutMilliseconds 60000)) {
        $startupDiagnostics = ((Read-HenkaSharedText -Path $stdoutPath) + "`n" +
            (Read-HenkaSharedText -Path $stderrPath)).Trim()
        $startupDiagnostics = $startupDiagnostics -replace "\s+", " "
        if ($startupDiagnostics.Length -gt 2048) {
            $startupDiagnostics = $startupDiagnostics.Substring($startupDiagnostics.Length - 2048)
        }
        throw "The editor did not report a usable UI. Startup diagnostics: $startupDiagnostics"
    }

    if (-not (Wait-FileContains `
            -Path $stdoutPath `
            -Pattern 'DEFAULT_SCENE_READY ground=1 ground_editable=1 camera=1 showcase_assets=0 diagnostic_entities=0 scene_content=product_native' `
            -TimeoutMilliseconds 5000)) {
        throw "The generic modeling workflow did not start from the clean product-native scene."
    }
    $startupText = Read-HenkaSharedText -Path $stdoutPath
    if ($startupText -match '(?m)^Native authoring (?:source )?row: name=Showcase (?:Giraffe|Rocket)') {
        throw "The generic modeling evidence was contaminated by a showcase/reference authoring row."
    }

    $sceneGeometry = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern 'Workspace UI geometry: .*scene_objects=(?<x>[-0-9.]+),(?<y>[-0-9.]+),(?<width>[-0-9.]+),(?<height>[-0-9.]+) '
    $panelX = [double]::Parse($sceneGeometry.Groups["x"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $panelY = [double]::Parse($sceneGeometry.Groups["y"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $panelWidth = [double]::Parse($sceneGeometry.Groups["width"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $actionWidth = [double][Math]::Max(56.0, ($panelWidth - 40.0) / 3.0)
    $primitiveActionWidth = [double][Math]::Max(72.0, ($panelWidth - 34.0) / 2.0)
    $nativeActionY = [double]($panelY + 90.0)
    $nameX = [double]($panelX + 14.0 + ($primitiveActionWidth * 2.0 + 6.0) * 0.5)
    $nameY = [double]($nativeActionY + 85.0)
    $newAssetX = [double]($panelX + 14.0 + $primitiveActionWidth * 0.5)
    $newAssetY = [double]($nativeActionY + 115.0)

    Send-HenkaAutomationClick -EventPath $automationInputPath -X $nameX -Y $nameY
    Clear-TextField -EventPath $automationInputPath
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $assetName
    # The runtime consumes one bounded automation event per frame. Wait for the
    # actual product state instead of guessing how long a slow renderer needs.
    Wait-AssetNameInput -LogPath $stdoutPath -Name $assetName
    Send-HenkaAutomationClick -EventPath $automationInputPath -X $newAssetX -Y $newAssetY
    Wait-AssetTransition -LogPath $stdoutPath -Action "created" -PartCount 0

    # Start with one Quad Sphere so the visible workflow proves the generic
    # all-quad primitive path. All subsequent changes are normal editor actions
    # against the selected component source, not fixture construction.
    $quadSphereX = [double]($panelX + 20.0 + $primitiveActionWidth + $primitiveActionWidth * 0.5)
    Send-HenkaAutomationClick -EventPath $automationInputPath -X $quadSphereX -Y ($nativeActionY + 42.0)
    Wait-AssetTransition -LogPath $stdoutPath -Action "part-added" -PartCount 1

    $disclosure = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern 'Native authoring disclosure: name=(?<name>.+?) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28.0 expanded=0\.'
    $authoringName = $disclosure.Groups["name"].Value
    Click-LoggedControl `
        -LogPath $stdoutPath `
        -EventPath $automationInputPath `
        -Pattern ("Native authoring disclosure: name=" + [Regex]::Escape($authoringName) + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28.0 expanded=0\.') `
        -XGroup "x" -YGroup "y" -XOffset 100.0 -YOffset 14.0
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern ("Native authoring disclosure: name=" + [Regex]::Escape($authoringName) + ' .* expanded=1\.') -TimeoutMilliseconds 5000)) {
        throw "The visible native authoring disclosure did not expand for the new asset."
    }
    $faceMode = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern ("Native authoring face controls: name=" + [Regex]::Escape($authoringName) + ' face_x=(?<x>[-0-9.]+) face_y=(?<y>[-0-9.]+) width=88.0 height=24.0\.')
    Click-LoggedControl `
        -LogPath $stdoutPath `
        -EventPath $automationInputPath `
        -Pattern ("Native authoring face controls: name=" + [Regex]::Escape($authoringName) + ' face_x=(?<x>[-0-9.]+) face_y=(?<y>[-0-9.]+) width=88.0 height=24.0\.') `
        -XGroup "x" -YGroup "y"
    if (-not (Wait-FileContains `
            -Path $stdoutPath `
            -Pattern 'Native authoring edit presentation: entity=\d+ mode=2 overlay=0 cage=hidden markers=selected\.' `
            -TimeoutMilliseconds 5000)) {
            throw "The visible Face edit mode did not report clean selected-component presentation with diagnostics disabled."
    }

    $viewport = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern 'Sandbox viewport: origin (?<x>[-0-9.]+),(?<y>[-0-9.]+) size (?<width>[-0-9.]+)x(?<height>[-0-9.]+)\.'
    $viewportX = [double]::Parse($viewport.Groups["x"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $viewportY = [double]::Parse($viewport.Groups["y"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $viewportWidth = [double]::Parse($viewport.Groups["width"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $viewportHeight = [double]::Parse($viewport.Groups["height"].Value, [Globalization.CultureInfo]::InvariantCulture)

    # Modeling evidence must use the product's neutral geometry-reading mode.
    # A fresh sandbox may otherwise restore Rendered mode when no user setting
    # exists, making a valid mesh appear like an overexposed white silhouette.
    # Use the reported Scene View control geometry and verify the app-owned
    # shading transition rather than assuming a coordinate or mode.
    $shadingControls = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern 'Viewport shading controls: x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) button=(?<button>[-0-9.]+) gap=(?<gap>[-0-9.]+)'
    $shadingX = [double]::Parse($shadingControls.Groups["x"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $shadingY = [double]::Parse($shadingControls.Groups["y"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $shadingButtonWidth = [double]::Parse($shadingControls.Groups["button"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $shadingGap = [double]::Parse($shadingControls.Groups["gap"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $solidModeCount = Get-LogMatchCount -Path $stdoutPath -Pattern 'Viewport shading: Solid\.'
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X ($shadingX + ($shadingButtonWidth + $shadingGap) + ($shadingButtonWidth * 0.5)) `
        -Y ($shadingY + 11.0)
    if (-not (Wait-LogMatchCountIncrease `
            -Path $stdoutPath `
            -InitialCount $solidModeCount `
            -Pattern 'Viewport shading: Solid\.' `
            -TimeoutMilliseconds 5000)) {
        throw "The modeling evidence could not establish the product Solid shading mode."
    }

    Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F"
    Start-Sleep -Milliseconds 450
    $initialPickX = [double]($viewportX + $viewportWidth * 0.5)
    $initialPickY = [double]($viewportY + $viewportHeight * 0.5)

    $modeEvidence = @(
        @{ Name = "vertex"; Label = "Vertex"; Code = 0; Pattern = ("Native authoring Vertex selection control: name=" + [Regex]::Escape($authoringName) + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=88.0 height=24.0\.') },
        @{ Name = "edge"; Label = "Edge"; Code = 1; Pattern = ("Native authoring Edge selection control: name=" + [Regex]::Escape($authoringName) + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=88.0 height=24.0\.') },
        @{ Name = "face"; Label = "Face"; Code = 2; Pattern = ("Native authoring face controls: name=" + [Regex]::Escape($authoringName) + ' face_x=(?<x>[-0-9.]+) face_y=(?<y>[-0-9.]+) width=88.0 height=24.0\.') }
    )
    foreach ($mode in $modeEvidence) {
        Click-LoggedControl `
            -LogPath $stdoutPath `
            -EventPath $automationInputPath `
            -Pattern $mode.Pattern `
            -XGroup "x" `
            -YGroup "y" `
            -XOffset 44.0 `
            -YOffset 12.0
        if (-not (Wait-FileContains `
                -Path $stdoutPath `
                -Pattern ("Native authoring topology mode: name=" + [Regex]::Escape($authoringName) + ' mode=' + $mode.Label + ' ') `
                -TimeoutMilliseconds 5000)) {
            throw ("The visible editor did not enter " + $mode.Label + " edit mode.")
        }
        if (-not (Wait-FileContains `
                -Path $stdoutPath `
                -Pattern ("Native authoring edit presentation: entity=\d+ mode=" + $mode.Code + ' overlay=0 cage=hidden markers=selected\.') `
                -TimeoutMilliseconds 5000)) {
                throw ("The visible " + $mode.Label + " edit mode did not report clean selected-component presentation.")
        }
        Start-Sleep -Milliseconds 300
        Save-ProbeWindowScreenshot `
            -Handle $capturedProcess.Process.MainWindowHandle `
            -Path (Join-Path $runtimeDirectory ($mode.Name + "-mode-normal-distance.png"))

        if ($mode.Name -eq "vertex") {
            $pickedCount = Get-LogMatchCount `
                -Path $stdoutPath `
                -Pattern ("Native authoring component picked: name=" + [Regex]::Escape($authoringName) + ' .* mode=vertex .* selected=1')
            Send-HenkaAutomationClick `
                -EventPath $automationInputPath `
                -X $initialPickX `
                -Y $initialPickY
            if (-not (Wait-LogMatchCountIncrease `
                    -Path $stdoutPath `
                    -InitialCount $pickedCount `
                    -Pattern ("Native authoring component picked: name=" + [Regex]::Escape($authoringName) + ' .* mode=vertex .* selected=1') `
                    -TimeoutMilliseconds 5000)) {
                throw "The visible editor did not pick one real source vertex for Smooth Vertices."
            }
            $smoothControls = Get-LastMatch `
                -Path $stdoutPath `
                -Pattern ("Native authoring smooth controls: name=" + [Regex]::Escape($authoringName) + ' smooth_x=(?<x>[-0-9.]+) smooth_y=(?<y>[-0-9.]+) width=90.0 height=24.0\.')
            $smoothPreviewCount = Get-LogMatchCount `
                -Path $stdoutPath `
                -Pattern ("Native authoring smooth preview: name=" + [Regex]::Escape($authoringName) + ' .* result=success')
            Send-HenkaAutomationClick `
                -EventPath $automationInputPath `
                -X ([double]$smoothControls.Groups["x"].Value + 12.0) `
                -Y ([double]$smoothControls.Groups["y"].Value + 12.0)
            if (-not (Wait-LogMatchCountIncrease `
                    -Path $stdoutPath `
                    -InitialCount $smoothPreviewCount `
                    -Pattern ("Native authoring smooth preview: name=" + [Regex]::Escape($authoringName) + ' .* result=success') `
                    -TimeoutMilliseconds 5000)) {
                throw "The visible Smooth Vertices Preview did not publish a successful candidate."
            }
            for ($smoothScrollAttempt = 0; $smoothScrollAttempt -lt 8; ++$smoothScrollAttempt) {
                if ((Get-LogMatchCount `
                        -Path $stdoutPath `
                        -Pattern ("Native authoring smooth transaction: name=" + [Regex]::Escape($authoringName) + ' apply_x=')) -gt 0) {
                    break
                }
                Send-HenkaAutomationScroll `
                    -EventPath $automationInputPath `
                    -X 1040.0 `
                    -Y 350.0 `
                    -WheelDelta -1.0
                Start-Sleep -Milliseconds 250
            }
            $smoothTransaction = Get-LastMatch `
                -Path $stdoutPath `
                -Pattern ("Native authoring smooth transaction: name=" + [Regex]::Escape($authoringName) + ' apply_x=(?<x>[-0-9.]+) cancel_x=(?<cancel>[-0-9.]+) y=(?<y>[-0-9.]+) width=120.0 height=24.0\.')
            $smoothApplyCount = Get-LogMatchCount `
                -Path $stdoutPath `
                -Pattern ("Native authoring smooth apply: name=" + [Regex]::Escape($authoringName) + ' result=success')
            $smoothApplyX = [double]$smoothTransaction.Groups["x"].Value + 40.0
            $smoothApplyY = [double]$smoothTransaction.Groups["y"].Value + 12.0
            Send-HenkaAutomationEvent `
                -EventPath $automationInputPath `
                -EventLine ("move {0} {1}" -f `
                    (Format-HenkaAutomationFloat -Value $smoothApplyX), `
                    (Format-HenkaAutomationFloat -Value $smoothApplyY)) `
                -SettleMilliseconds 300
            Send-HenkaAutomationEvent `
                -EventPath $automationInputPath `
                -EventLine ("button left down {0} {1}" -f `
                    (Format-HenkaAutomationFloat -Value $smoothApplyX), `
                    (Format-HenkaAutomationFloat -Value $smoothApplyY)) `
                -SettleMilliseconds 300
            Send-HenkaAutomationEvent `
                -EventPath $automationInputPath `
                -EventLine ("button left up {0} {1}" -f `
                    (Format-HenkaAutomationFloat -Value $smoothApplyX), `
                    (Format-HenkaAutomationFloat -Value $smoothApplyY)) `
                -SettleMilliseconds 300
            if (-not (Wait-LogMatchCountIncrease `
                    -Path $stdoutPath `
                    -InitialCount $smoothApplyCount `
                    -Pattern ("Native authoring smooth apply: name=" + [Regex]::Escape($authoringName) + ' result=success') `
                    -TimeoutMilliseconds 5000)) {
                throw "The visible Smooth Vertices Apply did not commit successfully."
            }
        }
    }

    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "after-frame-before-pick.png")
    Send-HenkaAutomationEvent `
        -EventPath $automationInputPath `
        -EventLine ("move {0} {1}" -f `
            (Format-HenkaAutomationFloat -Value $initialPickX), `
            (Format-HenkaAutomationFloat -Value $initialPickY)) `
        -SettleMilliseconds 300
    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "face-hover-before-pick.png")
    $boxStartX = [double]($viewportX + $viewportWidth * 0.34)
    $boxStartY = [double]($viewportY + $viewportHeight * 0.22)
    $boxEndX = [double]($viewportX + $viewportWidth * 0.66)
    $boxEndY = [double]($viewportY + $viewportHeight * 0.64)
    Send-HenkaAutomationEvent `
        -EventPath $automationInputPath `
        -EventLine ("move {0} {1}" -f `
            (Format-HenkaAutomationFloat -Value $boxStartX), `
            (Format-HenkaAutomationFloat -Value $boxStartY))
    Send-HenkaAutomationEvent `
        -EventPath $automationInputPath `
        -EventLine ("button left down {0} {1}" -f `
            (Format-HenkaAutomationFloat -Value $boxStartX), `
            (Format-HenkaAutomationFloat -Value $boxStartY))
    Send-HenkaAutomationEvent `
        -EventPath $automationInputPath `
        -EventLine ("move {0} {1}" -f `
            (Format-HenkaAutomationFloat -Value $boxEndX), `
            (Format-HenkaAutomationFloat -Value $boxEndY)) `
        -SettleMilliseconds 300
    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "face-box-selection-drag.png")
    Send-HenkaAutomationEvent `
        -EventPath $automationInputPath `
        -EventLine ("button left up {0} {1}" -f `
            (Format-HenkaAutomationFloat -Value $boxEndX), `
            (Format-HenkaAutomationFloat -Value $boxEndY)) `
        -SettleMilliseconds 300
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring box selection: mode=Face operation=replace xray=off selected=" -TimeoutMilliseconds 5000)) {
        throw "The visible editor did not complete a normal source-face box selection."
    }
    Start-Sleep -Milliseconds 300
    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "after-face-box-selection.png")
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X $initialPickX `
        -Y $initialPickY
    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "after-frame.png")

    $picked = $false
    # The frame screenshot places the new sphere roughly at the viewport
    # center, above the floor occluder. Start inside that proven silhouette;
    # an empty-scene click would intentionally clear the active source before
    # the remaining bounded candidates could be tested.
    for ($xStep = 5; $xStep -le 6 -and -not $picked; $xStep += 1) {
        for ($yStep = 3; $yStep -le 4 -and -not $picked; $yStep += 1) {
            $pickX = [double]($viewportX + $viewportWidth * ([double]$xStep / 10.0))
            $pickY = [double]($viewportY + $viewportHeight * ([double]$yStep / 10.0))
            Send-HenkaAutomationClick -EventPath $automationInputPath -X $pickX -Y $pickY
            $picked = Wait-FileContains `
                -Path $stdoutPath `
                -Pattern ("Native authoring component picked: name=" + [Regex]::Escape($authoringName)) `
                -TimeoutMilliseconds 750
        }
    }
    if (-not $picked) {
        throw "The visible editor could not pick a component on the new native mesh."
    }

    # Exercise the real direct-modeling operator through its visible hotkey and
    # bounded numeric input path. The operation remains generic component move
    # behavior; no fixture-specific geometry is injected by the harness.
    Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "M"
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Modeling operator: move begin entity=.* selected=1\." -TimeoutMilliseconds 5000)) {
        throw "The visible modeling move operator did not begin on the selected component."
    }
    Send-HenkaAutomationText -EventPath $automationInputPath -Text "0.01"
    Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "Enter"
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Modeling operator: numeric move committed entity=.* amount=0\.010\." -TimeoutMilliseconds 5000)) {
        throw "The visible numeric modeling move did not commit through the operator transaction."
    }

    Invoke-VisibleFaceExtrude `
        -Pattern ("Native authoring face edit tools: name=" + [Regex]::Escape($authoringName) + ' extrude_x=(?<x>[-0-9.]+) inset_x=(?<inset>[-0-9.]+) y=(?<y>[-0-9.]+) width=') `
        -XGroup "x" -YGroup "y" `
        -FailureMessage "The visible face extrude did not commit successfully."
    Start-Sleep -Milliseconds 500
    # Component editing keeps the evaluated solid surface and clean object
    # highlight visible by default. Capture that ordinary presentation first.
    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "after-extrude-topology-overlay-default-off.png")

    # The authored cage is an explicit editor-only diagnostic overlay. Capture
    # both enabled and disabled states so the proof distinguishes authored
    # topology from the evaluated solid surface and object highlight.
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X ($viewportX + 328.0) `
        -Y ($viewportY + 26.0)
    Start-Sleep -Milliseconds 300
    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "after-extrude-topology-overlay-on.png")
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X ($viewportX + 328.0) `
        -Y ($viewportY + 26.0)
    Start-Sleep -Milliseconds 150
    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "after-extrude-topology-overlay-off.png")

    Click-LoggedControl `
        -LogPath $stdoutPath `
        -EventPath $automationInputPath `
        -Pattern ("Native authoring face edit tools: name=" + [Regex]::Escape($authoringName) + ' extrude_x=(?<x>[-0-9.]+) inset_x=(?<inset>[-0-9.]+) y=(?<y>[-0-9.]+) width=') `
        -XGroup "inset" -YGroup "y"
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring face inset request:.*result=success" -TimeoutMilliseconds 5000)) {
        throw "The visible face inset did not commit successfully."
    }

    Click-LoggedControl `
        -LogPath $stdoutPath `
        -EventPath $automationInputPath `
        -Pattern ("Native authoring material control: name=" + [Regex]::Escape($authoringName) + ' own_x=(?<x>[-0-9.]+) own_y=(?<y>[-0-9.]+) width=100.0 height=24.0 owned=0\.') `
        -XGroup "x" -YGroup "y"
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring material: editable runtime definition adopted" -TimeoutMilliseconds 5000)) {
        throw "The visible material ownership action did not commit successfully."
    }

    # Exercise the real slot-targeted material picker immediately after Own
    # Material establishes the manager-backed editable instance. The existing
    # save/close/open/re-edit sequence then validates that this material state
    # survives the normal authored-asset persistence boundary.
    $detailsGeometry = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern 'Workspace UI geometry: .*details=(?<x>[-0-9.]+),(?<y>[-0-9.]+),(?<width>[-0-9.]+),(?<height>[-0-9.]+)\.'
    $detailsX = [double]::Parse(
        $detailsGeometry.Groups["x"].Value,
        [Globalization.CultureInfo]::InvariantCulture)
    $detailsY = [double]::Parse(
        $detailsGeometry.Groups["y"].Value,
        [Globalization.CultureInfo]::InvariantCulture)
    $detailsWidth = [double]::Parse(
        $detailsGeometry.Groups["width"].Value,
        [Globalization.CultureInfo]::InvariantCulture)
    $detailsHeight = [double]::Parse(
        $detailsGeometry.Groups["height"].Value,
        [Globalization.CultureInfo]::InvariantCulture)
    $pickerControlPattern =
        'Material texture picker control: entity=\d+ slot=Base Color choose_x=(?<x>[-0-9.]+) choose_y=(?<y>[-0-9.]+) width=70\.0 height=24\.0\.'
    $pickerControlFound = (Get-LogMatchCount `
        -Path $stdoutPath `
        -Pattern $pickerControlPattern) -gt 0
    # Prior modeling/reopen steps legitimately leave Object Details at
    # different scroll offsets. Search to one bounded end, then reverse across
    # the panel rather than assuming the picker is always above or below the
    # current viewport.
    for ($pickerScrollAttempt = 0; $pickerScrollAttempt -lt 48 -and -not $pickerControlFound; ++$pickerScrollAttempt) {
        Send-HenkaAutomationScroll `
            -EventPath $automationInputPath `
            -X ($detailsX + $detailsWidth * 0.5) `
            -Y ($detailsY + $detailsHeight * 0.5) `
            -WheelDelta 1.0
        Start-Sleep -Milliseconds 100
        $pickerControlFound = (Get-LogMatchCount `
            -Path $stdoutPath `
            -Pattern $pickerControlPattern) -gt 0
    }
    for ($pickerScrollAttempt = 0; $pickerScrollAttempt -lt 96 -and -not $pickerControlFound; ++$pickerScrollAttempt) {
        Send-HenkaAutomationScroll `
            -EventPath $automationInputPath `
            -X ($detailsX + $detailsWidth * 0.5) `
            -Y ($detailsY + $detailsHeight * 0.5) `
            -WheelDelta -1.0
        Start-Sleep -Milliseconds 100
        $pickerControlFound = (Get-LogMatchCount `
            -Path $stdoutPath `
            -Pattern $pickerControlPattern) -gt 0
    }
    if (-not $pickerControlFound) {
        throw "The visible editor did not expose the Base Color texture picker control."
    }
    $pickerControl = Get-LastMatch -Path $stdoutPath -Pattern $pickerControlPattern
    $pickerBeginCount = Get-LogMatchCount `
        -Path $stdoutPath `
        -Pattern 'Material texture picker: action=begin entity=\d+ slot=Base Color\.'
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X ([double]$pickerControl.Groups["x"].Value + 35.0) `
        -Y ([double]$pickerControl.Groups["y"].Value + 12.0)
    if (-not (Wait-LogMatchCountIncrease `
            -Path $stdoutPath `
            -InitialCount $pickerBeginCount `
            -Pattern 'Material texture picker: action=begin entity=\d+ slot=Base Color\.' `
            -TimeoutMilliseconds 5000)) {
        throw "The visible Base Color texture picker did not start."
    }

    # Use the checked-in color texture that normal product startup loads.
    # Arbitrary manager rows can be semantic/runtime textures that are valid
    # assets but intentionally unsuitable as a Base Color source. The asset
    # browser is paged, so search its real Next control instead of assuming
    # this candidate is always on the first page.
    $assetRowPattern =
        'Material texture picker asset row: entity=\d+ slot=Base Color path=assets/textures/cube_albedo\.png x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=26\.0\.'
    $assetCandidateFound = (Get-LogMatchCount `
        -Path $stdoutPath `
        -Pattern $assetRowPattern) -gt 0
    if (-not $assetCandidateFound) {
        $utilityGeometry = Get-LastMatch `
            -Path $stdoutPath `
            -Pattern 'Workspace UI geometry: .*utility=(?<x>[-0-9.]+),(?<y>[-0-9.]+),(?<width>[-0-9.]+),(?<height>[-0-9.]+)'
        $utilityX = [double]::Parse(
            $utilityGeometry.Groups["x"].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $utilityY = [double]::Parse(
            $utilityGeometry.Groups["y"].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $utilityHeight = [double]::Parse(
            $utilityGeometry.Groups["height"].Value,
            [Globalization.CultureInfo]::InvariantCulture)
        $pickerNextX = $utilityX + 14.0 + 88.0 + 41.0
        # The product keeps picker navigation one 30 px control stride above
        # the safely inset Apply row. Target the actual navigation-row center,
        # not the old bottom-pinned location that now overlaps Apply.
        $pickerNextY = $utilityY + $utilityHeight - 108.0

        for ($pickerPageAttempt = 0; $pickerPageAttempt -lt 32 -and -not $assetCandidateFound; ++$pickerPageAttempt) {
            Send-HenkaAutomationClick `
                -EventPath $automationInputPath `
                -X $pickerNextX `
                -Y $pickerNextY
            Start-Sleep -Milliseconds 150
            $assetCandidateFound = (Get-LogMatchCount `
                -Path $stdoutPath `
                -Pattern $assetRowPattern) -gt 0
        }
    }
    if (-not $assetCandidateFound) {
        throw "The visible material texture picker did not publish the packaged cube_albedo.png candidate on any bounded asset page."
    }
    $assetRow = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern $assetRowPattern
    $pickerSelectCount = Get-LogMatchCount `
        -Path $stdoutPath `
        -Pattern 'Material texture picker: action=select entity=\d+ slot=Base Color path=assets/textures/cube_albedo\.png\.'
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X ([double]$assetRow.Groups["x"].Value + 18.0) `
        -Y ([double]$assetRow.Groups["y"].Value + 13.0)
    if (-not (Wait-LogMatchCountIncrease `
            -Path $stdoutPath `
            -InitialCount $pickerSelectCount `
            -Pattern 'Material texture picker: action=select entity=\d+ slot=Base Color path=assets/textures/cube_albedo\.png\.' `
            -TimeoutMilliseconds 5000)) {
        throw "The visible material texture picker did not select a manager-owned texture candidate."
    }

    $pickerActionsPattern =
        'Material texture picker actions: entity=\d+ slot=Base Color apply_x=(?<apply>[-0-9.]+) cancel_x=(?<cancel>[-0-9.]+) y=(?<y>[-0-9.]+) apply_width=(?<applyWidth>[-0-9.]+) cancel_width=(?<cancelWidth>[-0-9.]+) height=24\.0\.'
    if (-not (Wait-FileContains `
            -Path $stdoutPath `
            -Pattern $pickerActionsPattern `
            -TimeoutMilliseconds 5000)) {
        throw "The visible material texture picker did not publish Apply/Cancel geometry."
    }
    $pickerActions = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern $pickerActionsPattern
    Start-Sleep -Milliseconds 250
    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "material-texture-picker-selected.png")

    $pickerApplyPattern =
        'Material texture picker: action=apply entity=\d+ slot=Base Color result=(?<result>HENKA_[A-Z0-9_]+)\.'
    $pickerApplyCount = Get-LogMatchCount `
        -Path $stdoutPath `
        -Pattern $pickerApplyPattern
    $pickerApplyX =
        [double]$pickerActions.Groups["apply"].Value +
        [double]$pickerActions.Groups["applyWidth"].Value * 0.5
    $pickerApplyY = [double]$pickerActions.Groups["y"].Value + 12.0
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X $pickerApplyX `
        -Y $pickerApplyY
    if (-not (Wait-LogMatchCountIncrease `
            -Path $stdoutPath `
            -InitialCount $pickerApplyCount `
            -Pattern $pickerApplyPattern `
            -TimeoutMilliseconds 15000)) {
        throw ("The visible material texture picker Apply control produced no product action " +
            "within the bounded wait. click=({0:F1},{1:F1})." -f
            $pickerApplyX,
            $pickerApplyY)
    }
    $pickerApply = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern $pickerApplyPattern
    $pickerApplyResult = $pickerApply.Groups["result"].Value
    if ($pickerApplyResult -ne "HENKA_SUCCESS") {
        throw "The visible material texture picker product action failed with $pickerApplyResult."
    }
    Start-Sleep -Milliseconds 250
    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "material-texture-picker-applied.png")

    $projectControls = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern ("Native authoring project controls: name=" + [Regex]::Escape($authoringName) + ' save_x=(?<saveX>[-0-9.]+) save_y=(?<saveY>[-0-9.]+) reload_x=(?<reloadX>[-0-9.]+) reload_y=(?<reloadY>[-0-9.]+) width=(?<width>[-0-9.]+) height=24.0\.')
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X ([double]$projectControls.Groups["saveX"].Value + 24.0) `
        -Y ([double]$projectControls.Groups["saveY"].Value + 12.0)
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring project request: save=1 reload=0 result=success" -TimeoutMilliseconds 8000)) {
        throw "The visible native project save did not complete."
    }

    # Save the document through its visible asset-level control after the
    # source/project save. This is the persistence boundary used to reopen the
    # complete asset, not the Object Details source reload control.
    $assetActionY = [double]($nativeActionY + 88.0)
    $saveAssetX = [double]($panelX + 14.0 + $primitiveActionWidth * 0.5)
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X $saveAssetX `
        -Y ($assetActionY + 12.0)
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native asset document: name=.* action=saved parts=1" -TimeoutMilliseconds 8000)) {
        throw "The visible native asset save did not complete."
    }

    $closeAssetX = [double]($panelX + 20.0 + $primitiveActionWidth + $primitiveActionWidth * 0.5)
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X $closeAssetX `
        -Y ($assetActionY + 12.0)
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native asset document: name=.* action=closed parts=1" -TimeoutMilliseconds 8000)) {
        throw "The visible native asset close did not complete."
    }

    # Reopen through the visible Open Asset control, then make a second visible
    # component edit. The save/reload boundary is therefore exercised before
    # the final edit, not only at process shutdown.
    $openX = [double]($panelX + 20.0 + $primitiveActionWidth + $primitiveActionWidth * 0.5)
    $openY = [double]($nativeActionY + 115.0)
    Send-HenkaAutomationClick -EventPath $automationInputPath -X $openX -Y $openY
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native asset document: name=.* action=opened parts=1" -TimeoutMilliseconds 8000)) {
        throw "The visible native asset reopen did not complete."
    }
    $reopenedDisclosure = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern ("Native authoring disclosure: name=" + [Regex]::Escape($authoringName) + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28.0 expanded=0\.')
    Click-LoggedControl `
        -LogPath $stdoutPath `
        -EventPath $automationInputPath `
        -Pattern ("Native authoring disclosure: name=" + [Regex]::Escape($authoringName) + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28.0 expanded=0\.') `
        -XGroup "x" -YGroup "y" -XOffset 100.0 -YOffset 14.0
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern ("Native authoring disclosure: name=" + [Regex]::Escape($authoringName) + ' .* expanded=1\.') -TimeoutMilliseconds 5000)) {
        throw "The visible native authoring disclosure did not expand after reopen."
    }
    Click-LoggedControl `
        -LogPath $stdoutPath `
        -EventPath $automationInputPath `
        -Pattern ("Native authoring face controls: name=" + [Regex]::Escape($authoringName) + ' face_x=(?<x>[-0-9.]+) face_y=(?<y>[-0-9.]+) width=88.0 height=24.0\.') `
        -XGroup "x" -YGroup "y"
    Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F"
    Start-Sleep -Milliseconds 450
    $reopenedPickPattern = "Native authoring component picked: name=" + [Regex]::Escape($authoringName) + ' .* mode=face .* selected=1'
    $reopenedPicked = $false
    for ($xStep = 5; $xStep -le 6 -and -not $reopenedPicked; $xStep += 1) {
        for ($yStep = 3; $yStep -le 4 -and -not $reopenedPicked; $yStep += 1) {
            $reopenedPickCount = Get-LogMatchCount -Path $stdoutPath -Pattern $reopenedPickPattern
            $reopenedPickX = [double]($viewportX + $viewportWidth * ([double]$xStep / 10.0))
            $reopenedPickY = [double]($viewportY + $viewportHeight * ([double]$yStep / 10.0))
            Send-HenkaAutomationClick -EventPath $automationInputPath -X $reopenedPickX -Y $reopenedPickY
            $reopenedPicked = Wait-LogMatchCountIncrease `
                -Path $stdoutPath `
                -InitialCount $reopenedPickCount `
                -Pattern $reopenedPickPattern `
                -TimeoutMilliseconds 750
        }
    }
    if (-not $reopenedPicked) {
        throw "The visible reopened asset did not produce a fresh selected face before re-edit."
    }
    Invoke-VisibleFaceExtrude `
        -Pattern 'Native authoring face edit tools: name=(?<name>.+?) extrude_x=(?<x>[-0-9.]+) inset_x=(?<inset>[-0-9.]+) y=(?<y>[-0-9.]+) width=' `
        -XGroup "x" -YGroup "y" `
        -FailureMessage "The post-reload visible edit did not commit successfully."

    $manifest = Join-Path $runtimeDirectory ("user\saves\$assetName.asset")
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        throw "The visible authoring workflow did not leave a persisted manifest."
    }
    $manifestText = [System.IO.File]::ReadAllText($manifest)
    foreach ($requiredLine in @(
        "asset.version=5",
        "asset.name=$assetName",
        "asset.part_count=1",
        "part.0.primitive=5",
        "asset.provenance=HENKA_PRODUCT_NATIVE_AUTHORED")) {
        if (-not $manifestText.Contains($requiredLine)) {
            throw "The persisted native asset manifest was missing: $requiredLine"
        }
    }

    Write-Output "[pass] Product-native generic modeling workflow: clean startup, new asset, component pick, extrude, inset, material ownership, save, close, reopen, re-edit, and slot-targeted texture picker Apply completed."
    Write-Output "[pass] Evidence scope: PRODUCT_NATIVE_GENERIC; showcase/reference fixtures were not loaded."
    Write-Output "[pass] Runtime evidence retained: $runtimeDirectory"
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
}
