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

function Clear-TextField {
    param([Parameter(Mandatory = $true)][string]$EventPath)

    for ($index = 0; $index -lt 63; ++$index) {
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
}
"@
    }

    Set-HenkaAutomationForeground -Handle $Handle
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
        $graphics.CopyFromScreen(
            $rect.Left,
            $rect.Top,
            0,
            0,
            (New-Object System.Drawing.Size -ArgumentList $width, $height))
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

function Wait-AssetTransition {
    param(
        [Parameter(Mandatory = $true)][string]$LogPath,
        [Parameter(Mandatory = $true)][string]$Action,
        [Parameter(Mandatory = $true)][int]$PartCount
    )

    $pattern = "Native asset document: name=$([Regex]::Escape($assetName)) action=$Action parts=$PartCount\."
    if (-not (Wait-FileContains -Path $LogPath -Pattern $pattern -TimeoutMilliseconds 8000)) {
        throw "The editor did not report the native asset transition: $Action/$PartCount."
    }
}

try {
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "The Sandbox3D executable was not found: $executable"
    }

    New-Item -ItemType Directory -Path $runtimeDirectory -Force | Out-Null
    Copy-Item -LiteralPath $executable -Destination $runtimeExecutable
    Copy-Item `
        -LiteralPath (Join-Path (Split-Path -Parent $executable) "assets") `
        -Destination (Join-Path $runtimeDirectory "assets") `
        -Recurse
    New-Item -ItemType File -Path $automationInputPath -Force | Out-Null

    $env:HENKA_AUTOMATION_INPUT_OWNED = "1"
    $env:HENKA_AUTOMATION_INPUT_FILE = $automationInputPath
    $capturedProcess = Start-HenkaCapturedProcess `
        -FilePath $runtimeExecutable `
        -Arguments @("--primitive-gallery") `
        -WorkingDirectory $runtimeDirectory `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath

    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -TimeoutMilliseconds 30000)) {
        throw "The editor did not report a usable UI."
    }

    $sceneGeometry = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern 'Workspace UI geometry: .*scene_objects=(?<x>[-0-9.]+),(?<y>[-0-9.]+),(?<width>[-0-9.]+),(?<height>[-0-9.]+) '
    $panelX = [double]::Parse($sceneGeometry.Groups["x"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $panelY = [double]::Parse($sceneGeometry.Groups["y"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $panelWidth = [double]::Parse($sceneGeometry.Groups["width"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $actionWidth = [double][Math]::Max(56.0, ($panelWidth - 40.0) / 3.0)
    $nativeActionY = [double]($panelY + 90.0)
    $nameX = [double]($panelX + 14.0 + ($actionWidth * 2.0 + 6.0) * 0.5)
    $nameY = [double]($nativeActionY + 55.0)
    $newAssetX = [double]($panelX + 14.0 + $actionWidth * 0.5)
    $newAssetY = [double]($nativeActionY + 85.0)

    Send-HenkaAutomationClick -EventPath $automationInputPath -X $nameX -Y $nameY
    Clear-TextField -EventPath $automationInputPath
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $assetName
    Send-HenkaAutomationClick -EventPath $automationInputPath -X $newAssetX -Y $newAssetY
    Wait-AssetTransition -LogPath $stdoutPath -Action "created" -PartCount 0

    # Start with one UV sphere. All subsequent changes are normal editor
    # actions against the selected component source, not fixture construction.
    $sphereX = [double]($panelX + 14.0 + ($actionWidth + 6.0) * 2.0 + $actionWidth * 0.5)
    Send-HenkaAutomationClick -EventPath $automationInputPath -X $sphereX -Y ($nativeActionY + 12.0)
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

    $viewport = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern 'Sandbox viewport: origin (?<x>[-0-9.]+),(?<y>[-0-9.]+) size (?<width>[-0-9.]+)x(?<height>[-0-9.]+)\.'
    $viewportX = [double]::Parse($viewport.Groups["x"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $viewportY = [double]::Parse($viewport.Groups["y"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $viewportWidth = [double]::Parse($viewport.Groups["width"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $viewportHeight = [double]::Parse($viewport.Groups["height"].Value, [Globalization.CultureInfo]::InvariantCulture)
    Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F"
    Start-Sleep -Milliseconds 450
    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "after-frame-before-pick.png")
    $initialPickX = [double]($viewportX + $viewportWidth * 0.5)
    $initialPickY = [double]($viewportY + $viewportHeight * 0.5)
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

    Click-LoggedControl `
        -LogPath $stdoutPath `
        -EventPath $automationInputPath `
        -Pattern ("Native authoring face edit tools: name=" + [Regex]::Escape($authoringName) + ' extrude_x=(?<x>[-0-9.]+) inset_x=(?<inset>[-0-9.]+) y=(?<y>[-0-9.]+) width=') `
        -XGroup "x" -YGroup "y"
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring face extrude request:.*result=success" -TimeoutMilliseconds 5000)) {
        throw "The visible face extrude did not commit successfully."
    }
    Start-Sleep -Milliseconds 500
    # Component editing shows the authored source cage by default over the
    # evaluated solid surface. Capture that default presentation first.
    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "after-extrude-before-inset.png")

    # The cyan authored cage above is an editor-only topology overlay. Capture
    # the same committed edit with that overlay disabled so visual evidence
    # proves the scene renderer is showing the evaluated mesh itself rather
    # than only the selection/topology pass.
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X ($viewportX + 328.0) `
        -Y ($viewportY + 26.0)
    Start-Sleep -Milliseconds 300
    Save-ProbeWindowScreenshot `
        -Handle $capturedProcess.Process.MainWindowHandle `
        -Path (Join-Path $runtimeDirectory "after-extrude-topology-overlay-off.png")
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X ($viewportX + 328.0) `
        -Y ($viewportY + 26.0)
    Start-Sleep -Milliseconds 150

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
    $assetActionY = [double]($nativeActionY + 58.0)
    $saveAssetX = [double]($panelX + 14.0 + $actionWidth * 0.5)
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X $saveAssetX `
        -Y ($assetActionY + 12.0)
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native asset document: name=.* action=saved parts=1" -TimeoutMilliseconds 8000)) {
        throw "The visible native asset save did not complete."
    }

    $closeAssetX = [double]($panelX + 14.0 + $actionWidth + 6.0 + $actionWidth * 0.5)
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
    $openX = [double]($panelX + 14.0 + $actionWidth + 6.0 + $actionWidth * 0.5)
    $openY = [double]($nativeActionY + 85.0)
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
    Send-HenkaAutomationClick -EventPath $automationInputPath -X ($viewportX + $viewportWidth * 0.5) -Y ($viewportY + $viewportHeight * 0.5)
    Click-LoggedControl `
        -LogPath $stdoutPath `
        -EventPath $automationInputPath `
        -Pattern 'Native authoring face edit tools: name=(?<name>.+?) extrude_x=(?<x>[-0-9.]+) inset_x=(?<inset>[-0-9.]+) y=(?<y>[-0-9.]+) width=' `
        -XGroup "x" -YGroup "y"
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring face extrude request:.*result=success" -TimeoutMilliseconds 5000)) {
        throw "The post-reload visible edit did not commit successfully."
    }

    $manifest = Join-Path $runtimeDirectory ("user\saves\$assetName.asset")
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        throw "The visible authoring workflow did not leave a persisted manifest."
    }
    $manifestText = [System.IO.File]::ReadAllText($manifest)
    foreach ($requiredLine in @(
        "asset.version=5",
        "asset.name=$assetName",
        "asset.part_count=1",
        "asset.provenance=HENKA_PRODUCT_NATIVE_AUTHORED")) {
        if (-not $manifestText.Contains($requiredLine)) {
            throw "The persisted native asset manifest was missing: $requiredLine"
        }
    }

    Write-Output "[pass] Visible native modeling workflow: new asset, component pick, extrude, inset, material ownership, save, close, reopen, and re-edit completed."
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
