[CmdletBinding()]
param(
    [ValidateSet("Rocket", "Giraffe")]
    [string]$Asset = "Rocket",
    [string]$ExecutablePath = "build\examples\sandbox3d\Debug\henka_sandbox3d.exe",
    [string]$OutputDirectory = "build\test_tmp\showcase-authoring",
    [string]$PublishDirectory = "",
    [switch]$Publish
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
$assetName = "Cheeky" + $Asset
$capturedProcess = $null
$previousAutomationOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
$previousAutomationFile = $env:HENKA_AUTOMATION_INPUT_FILE

. (Join-Path $repoRoot "scripts\henka_script_common.ps1")
. (Join-Path $repoRoot "scripts\henka_ui_automation_helpers.ps1")

function Get-LastMatch {
    param(
        [string]$Path = $stdoutPath,
        [Parameter(Mandatory = $true)][string]$Pattern
    )

    $match = Get-LastLogRegexMatch -Path $Path -Pattern $Pattern
    if ($null -eq $match) {
        throw "The editor did not report the required control: $Pattern"
    }
    return $match
}

function Wait-AssetEvent {
    param(
        [Parameter(Mandatory = $true)][string]$Action,
        [Parameter(Mandatory = $true)][int]$PartCount
    )

    $pattern = "Native asset document: name=$([Regex]::Escape($assetName)) action=$Action parts=$PartCount\."
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $pattern -TimeoutMilliseconds 10000)) {
        throw "The editor did not report asset transition $Action/$PartCount."
    }
}

function Click-LoggedControl {
    param(
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$XGroup,
        [Parameter(Mandatory = $true)][string]$YGroup,
        [double]$XOffset = 12.0,
        [double]$YOffset = 12.0
    )

    $match = Get-LastMatch -Path $stdoutPath -Pattern $Pattern
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X ([double]$match.Groups[$XGroup].Value + $XOffset) `
        -Y ([double]$match.Groups[$YGroup].Value + $YOffset)
}

function Set-ViewportShadingMode {
    param(
        [Parameter(Mandatory = $true)][int]$ModeIndex,
        [Parameter(Mandatory = $true)][string]$ModeLabel
    )

    if ($ModeIndex -lt 0 -or $ModeIndex -gt 3) {
        throw "The requested viewport shading mode index was outside the native four-mode range."
    }

    $match = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern 'Viewport shading controls: x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) button=(?<button>[-0-9.]+) gap=(?<gap>[-0-9.]+)'
    $startX = [double]::Parse($match.Groups["x"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $startY = [double]::Parse($match.Groups["y"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $buttonWidth = [double]::Parse($match.Groups["button"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $gap = [double]::Parse($match.Groups["gap"].Value, [Globalization.CultureInfo]::InvariantCulture)
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X ($startX + (($buttonWidth + $gap) * $ModeIndex) + ($buttonWidth * 0.5)) `
        -Y ($startY + 11.0)
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern ("Viewport shading: " + [Regex]::Escape($ModeLabel) + '\.') -TimeoutMilliseconds 5000)) {
        throw "The visible viewport shading control did not enter $ModeLabel mode."
    }
}

function Clear-TextField {
    for ($index = 0; $index -lt 63; ++$index) {
        Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine "key Backspace down" -SettleMilliseconds 0
        Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine "key Backspace up" -SettleMilliseconds 0
    }
}

function Save-WindowScreenshot {
    param([Parameter(Mandatory = $true)][string]$Path)

    Add-Type -AssemblyName System.Drawing
    if (-not ("HenkaShowcaseAuthoringNative" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class HenkaShowcaseAuthoringNative
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

    $handle = $capturedProcess.Process.MainWindowHandle
    Set-HenkaAutomationForeground -Handle $handle
    $rect = New-Object HenkaShowcaseAuthoringNative+RECT
    if (-not [HenkaShowcaseAuthoringNative]::GetWindowRect($handle, [ref]$rect)) {
        throw "The showcase authoring window bounds could not be read."
    }
    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    if ($width -le 0 -or $height -le 0) {
        throw "The showcase authoring window bounds were invalid."
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

try {
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
    Clear-TextField
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $assetName
    Send-HenkaAutomationClick -EventPath $automationInputPath -X $newAssetX -Y $newAssetY
    Wait-AssetEvent -Action "created" -PartCount 0

    $primitiveX = if ($Asset -eq "Rocket") {
        $panelX + 14.0 + $actionWidth * 0.5
    }
    else {
        $panelX + 14.0 + ($actionWidth + 6.0) * 2.0 + $actionWidth * 0.5
    }
    Send-HenkaAutomationClick -EventPath $automationInputPath -X $primitiveX -Y ($nativeActionY + 12.0)
    Wait-AssetEvent -Action "part-added" -PartCount 1

    $disclosure = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern 'Native authoring disclosure: name=(?<name>.+?) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28.0 expanded=0\.'
    $partName = $disclosure.Groups["name"].Value
    Click-LoggedControl `
        -Pattern ("Native authoring disclosure: name=" + [Regex]::Escape($partName) + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=') `
        -XGroup "x" -YGroup "y" -XOffset 100.0 -YOffset 14.0
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern ("Native authoring disclosure: name=" + [Regex]::Escape($partName) + ' .* expanded=1\.') -TimeoutMilliseconds 5000)) {
        throw "The native authoring disclosure did not expand."
    }

    Click-LoggedControl `
        -Pattern ("Native authoring face controls: name=" + [Regex]::Escape($partName) + ' face_x=(?<x>[-0-9.]+) face_y=(?<y>[-0-9.]+)') `
        -XGroup "x" -YGroup "y"
    $viewport = Get-LastMatch `
        -Path $stdoutPath `
        -Pattern 'Sandbox viewport: origin (?<x>[-0-9.]+),(?<y>[-0-9.]+) size (?<width>[-0-9.]+)x(?<height>[-0-9.]+)\.'
    $viewportX = [double]::Parse($viewport.Groups["x"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $viewportY = [double]::Parse($viewport.Groups["y"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $viewportWidth = [double]::Parse($viewport.Groups["width"].Value, [Globalization.CultureInfo]::InvariantCulture)
    $viewportHeight = [double]::Parse($viewport.Groups["height"].Value, [Globalization.CultureInfo]::InvariantCulture)
    Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F"
    Start-Sleep -Milliseconds 500
    if ($Asset -eq "Rocket") {
        # The editor compass overlays the upper cap in the default front view.
        # Orbit through the documented Alt+Left viewport interaction so the
        # cap can be selected by the same visible modeling workflow a user
        # would use; no mesh data is injected by this harness.
        $orbitX = [double]($viewportX + $viewportWidth * 0.50)
        $orbitY = [double]($viewportY + $viewportHeight * 0.52)
        $orbitYTarget = [double]($orbitY + 100.0)
        Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine "key LeftAlt down"
        Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine ("move {0} {1}" -f (Format-HenkaAutomationFloat $orbitX), (Format-HenkaAutomationFloat $orbitY))
        Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine ("button left down {0} {1}" -f (Format-HenkaAutomationFloat $orbitX), (Format-HenkaAutomationFloat $orbitY))
        Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine ("move {0} {1}" -f (Format-HenkaAutomationFloat $orbitX), (Format-HenkaAutomationFloat $orbitYTarget))
        Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine ("button left up {0} {1}" -f (Format-HenkaAutomationFloat $orbitX), (Format-HenkaAutomationFloat $orbitYTarget))
        Send-HenkaAutomationEvent -EventPath $automationInputPath -EventLine "key LeftAlt up"
        Start-Sleep -Milliseconds 500
    }
    Save-WindowScreenshot -Path (Join-Path $runtimeDirectory "01-framed-before-modeling.png")

    # The cylinder profile uses the selected top cap to grow a single
    # continuous source mesh. The giraffe profile uses the same visible face
    # operations on an editable UV sphere. No source geometry is written by
    # this harness.
    $pickX = [double]($viewportX + $viewportWidth * 0.50)
    $pickY = [double]($viewportY + $viewportHeight * 0.30)
    if ($Asset -eq "Rocket") {
        Click-LoggedControl `
            -Pattern ("Native authoring extreme face controls: name=" + [Regex]::Escape($partName) + ' maximum_y_x=(?<x>[-0-9.]+) minimum_y_x=(?<minimum>[-0-9.]+) y=(?<y>[-0-9.]+)') `
            -XGroup "x" -YGroup "y" -YOffset 44.0
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern ("Native authoring extreme face selection: name=" + [Regex]::Escape($partName) + ' result=success direction=maximum_y') -TimeoutMilliseconds 5000)) {
            throw "The visible workflow could not select the top profile face."
        }
    }
    else {
        Send-HenkaAutomationClick -EventPath $automationInputPath -X $pickX -Y $pickY
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern ("Native authoring component picked: name=" + [Regex]::Escape($partName)) -TimeoutMilliseconds 5000)) {
            throw "The visible workflow could not select the initial modeling face."
        }

        # Shape a bounded upper band through the same generic selection and
        # component-move controls a user can use for an authored organic
        # profile.  This deliberately exercises geometry selection rather
        # than encoding giraffe-specific topology or coordinates.
        Click-LoggedControl `
            -Pattern ("Native authoring extreme face band controls: name=" + [Regex]::Escape($partName) + ' maximum_y_x=(?<x>[-0-9.]+) minimum_y_x=(?<minimum>[-0-9.]+) y=(?<y>[-0-9.]+)') `
            -XGroup "x" -YGroup "y" -YOffset 44.0
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern ("Native authoring extreme face band selection: name=" + [Regex]::Escape($partName) + ' result=success direction=maximum_y') -TimeoutMilliseconds 5000)) {
            throw "The visible workflow could not select the upper face band."
        }
        for ($bandStep = 0; $bandStep -lt 3; ++$bandStep) {
            Click-LoggedControl `
                -Pattern ("Native authoring move control: name=" + [Regex]::Escape($partName) + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=88.0 height=24.0') `
                -XGroup "x" -YGroup "y" -XOffset 108.0
            if (-not (Wait-FileContains -Path $stdoutPath -Pattern ("Native authoring component move: name=" + [Regex]::Escape($partName) + ' result=success') -TimeoutMilliseconds 5000)) {
                throw "The visible upper-band move failed at step $bandStep."
            }
        }
    }

    $primaryStepCount = if ($Asset -eq "Rocket") { 5 } else { 3 }
    for ($step = 0; $step -lt $primaryStepCount; ++$step) {
        Click-LoggedControl `
            -Pattern ("Native authoring face edit tools: name=" + [Regex]::Escape($partName) + ' extrude_x=(?<x>[-0-9.]+) inset_x=(?<inset>[-0-9.]+) y=(?<y>[-0-9.]+) width=') `
            -XGroup "inset" -YGroup "y"
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring face inset request:.*result=success" -TimeoutMilliseconds 5000)) {
            throw "Visible face inset failed at step $step."
        }
        Start-Sleep -Milliseconds 250
        Click-LoggedControl `
            -Pattern ("Native authoring face edit tools: name=" + [Regex]::Escape($partName) + ' extrude_x=(?<x>[-0-9.]+) inset_x=(?<inset>[-0-9.]+) y=(?<y>[-0-9.]+) width=') `
            -XGroup "x" -YGroup "y"
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring face extrude request:.*result=success" -TimeoutMilliseconds 5000)) {
            throw "Visible face extrude failed at step $step."
        }
        Start-Sleep -Milliseconds 250
    }

    if ($Asset -eq "Rocket") {
        Click-LoggedControl `
            -Pattern ("Native authoring bevel control: name=" + [Regex]::Escape($partName) + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=88.0 height=24.0') `
            -XGroup "x" -YGroup "y"
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring workflow: face bevel edited" -TimeoutMilliseconds 5000)) {
            throw "Visible top profile bevel failed."
        }
        Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F"
        Start-Sleep -Milliseconds 400

        Click-LoggedControl `
            -Pattern ("Native authoring extreme face controls: name=" + [Regex]::Escape($partName) + ' maximum_y_x=(?<maximum>[-0-9.]+) minimum_y_x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+)') `
            -XGroup "x" -YGroup "y" -YOffset 44.0
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern ("Native authoring extreme face selection: name=" + [Regex]::Escape($partName) + ' result=success direction=minimum_y') -TimeoutMilliseconds 5000)) {
            throw "The visible workflow could not select the bottom profile face."
        }
        for ($step = 0; $step -lt 2; ++$step) {
            Click-LoggedControl `
                -Pattern ("Native authoring face edit tools: name=" + [Regex]::Escape($partName) + ' extrude_x=(?<x>[-0-9.]+) inset_x=(?<inset>[-0-9.]+) y=(?<y>[-0-9.]+) width=') `
                -XGroup "inset" -YGroup "y"
            if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring face inset request:.*result=success" -TimeoutMilliseconds 5000)) {
                throw "Visible bottom inset failed at step $step."
            }
            Start-Sleep -Milliseconds 250
            Click-LoggedControl `
                -Pattern ("Native authoring face edit tools: name=" + [Regex]::Escape($partName) + ' extrude_x=(?<x>[-0-9.]+) inset_x=(?<inset>[-0-9.]+) y=(?<y>[-0-9.]+) width=') `
                -XGroup "x" -YGroup "y"
            if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring face extrude request:.*result=success" -TimeoutMilliseconds 5000)) {
                throw "Visible bottom extrude failed at step $step."
            }
            Start-Sleep -Milliseconds 250
        }
        Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F"
        Start-Sleep -Milliseconds 400
        Save-WindowScreenshot -Path (Join-Path $runtimeDirectory "rocket-after-profile.png")
        $finY = [double]($viewportY + $viewportHeight * 0.48)
        foreach ($finXFactor in @(0.41, 0.59)) {
            Send-HenkaAutomationClick `
                -EventPath $automationInputPath `
                -X ([double]($viewportX + $viewportWidth * $finXFactor)) `
                -Y $finY
            if (-not (Wait-FileContains -Path $stdoutPath -Pattern ("Native authoring component picked: name=" + [Regex]::Escape($partName)) -TimeoutMilliseconds 5000)) {
                throw "The visible workflow could not select a rocket fin face."
            }
            Click-LoggedControl `
                -Pattern ("Native authoring face edit tools: name=" + [Regex]::Escape($partName) + ' extrude_x=(?<x>[-0-9.]+) inset_x=(?<inset>[-0-9.]+) y=(?<y>[-0-9.]+) width=') `
                -XGroup "x" -YGroup "y"
            if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring face extrude request:.*result=success" -TimeoutMilliseconds 5000)) {
                throw "Visible rocket fin extrusion failed."
            }
            Start-Sleep -Milliseconds 300
        }
        Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F"
        Start-Sleep -Milliseconds 500
    }

    Click-LoggedControl `
        -Pattern ("Native authoring material control: name=" + [Regex]::Escape($partName) + ' own_x=(?<x>[-0-9.]+) own_y=(?<y>[-0-9.]+) width=100.0 height=24.0 owned=0\.') `
        -XGroup "x" -YGroup "y"
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring material: editable runtime definition adopted" -TimeoutMilliseconds 5000)) {
        throw "Visible native material ownership failed."
    }
    Set-ViewportShadingMode -ModeIndex 3 -ModeLabel "Rendered"
    Start-Sleep -Milliseconds 800
    Save-WindowScreenshot -Path (Join-Path $runtimeDirectory "04-rendered-authored.png")
    Click-LoggedControl `
        -Pattern ("Native authoring project controls: name=" + [Regex]::Escape($partName) + ' save_x=(?<saveX>[-0-9.]+) save_y=(?<saveY>[-0-9.]+) reload_x=') `
        -XGroup "saveX" -YGroup "saveY" -XOffset 24.0
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring project request: save=1 reload=0 result=success" -TimeoutMilliseconds 8000)) {
        throw "Visible native project save failed."
    }
    Save-WindowScreenshot -Path (Join-Path $runtimeDirectory "02-authored-before-save.png")

    $assetActionY = [double]($nativeActionY + 58.0)
    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X ([double]($panelX + 14.0 + $actionWidth * 0.5)) `
        -Y ($assetActionY + 12.0)
    Wait-AssetEvent -Action "saved" -PartCount 1

    Send-HenkaAutomationClick `
        -EventPath $automationInputPath `
        -X ([double]($panelX + 14.0 + $actionWidth + 6.0 + $actionWidth * 0.5)) `
        -Y ($assetActionY + 12.0)
    Wait-AssetEvent -Action "closed" -PartCount 1
    Start-Sleep -Milliseconds 800
    Save-WindowScreenshot -Path (Join-Path $runtimeDirectory "05-rendered-after-close.png")

    $openX = [double]($panelX + 14.0 + $actionWidth + 6.0 + $actionWidth * 0.5)
    $openY = [double]($nativeActionY + 85.0)
    Send-HenkaAutomationClick -EventPath $automationInputPath -X $openX -Y $openY
    Wait-AssetEvent -Action "opened" -PartCount 1
    $reopenedDisclosure = Get-LastMatch `
        -Pattern ("Native authoring disclosure: name=" + [Regex]::Escape($partName) + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=28.0 expanded=0\.')
    Click-LoggedControl `
        -Pattern ("Native authoring disclosure: name=" + [Regex]::Escape($partName) + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=') `
        -XGroup "x" -YGroup "y" -XOffset 100.0 -YOffset 14.0
    Click-LoggedControl `
        -Pattern ("Native authoring face controls: name=" + [Regex]::Escape($partName) + ' face_x=(?<x>[-0-9.]+) face_y=(?<y>[-0-9.]+)') `
        -XGroup "x" -YGroup "y"
    Send-HenkaAutomationKey -EventPath $automationInputPath -KeyName "F"
    Start-Sleep -Milliseconds 400
    Set-ViewportShadingMode -ModeIndex 3 -ModeLabel "Rendered"
    Start-Sleep -Milliseconds 800
    Send-HenkaAutomationClick -EventPath $automationInputPath -X $pickX -Y $pickY
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern ("Native authoring component picked: name=" + [Regex]::Escape($partName)) -TimeoutMilliseconds 5000)) {
        throw "The reopened asset could not be component-selected."
    }
    Click-LoggedControl `
        -Pattern ("Native authoring face edit tools: name=" + [Regex]::Escape($partName) + ' extrude_x=(?<x>[-0-9.]+) inset_x=(?<inset>[-0-9.]+) y=(?<y>[-0-9.]+) width=') `
        -XGroup "x" -YGroup "y"
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Native authoring face extrude request:.*result=success" -TimeoutMilliseconds 5000)) {
        throw "The reopened asset could not be edited."
    }
    Save-WindowScreenshot -Path (Join-Path $runtimeDirectory "03-reopened-after-edit.png")

    $manifest = Join-Path $runtimeDirectory ("user\saves\$assetName.asset")
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        throw "The authored showcase manifest was not persisted."
    }
    $manifestText = [System.IO.File]::ReadAllText($manifest)
    foreach ($requiredLine in @(
        "asset.version=5",
        "asset.name=$assetName",
        "asset.part_count=1",
        "asset.provenance=HENKA_PRODUCT_NATIVE_AUTHORED")) {
        if (-not $manifestText.Contains($requiredLine)) {
            throw "The authored showcase manifest was missing: $requiredLine"
        }
    }

    if ($Publish) {
        if ([string]::IsNullOrWhiteSpace($PublishDirectory)) {
            throw "-Publish requires -PublishDirectory."
        }
        $publishRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $PublishDirectory))
        New-Item -ItemType Directory -Path $publishRoot -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $runtimeDirectory "user\saves") -Destination $publishRoot -Recurse -Force
        Copy-Item -LiteralPath (Join-Path $runtimeDirectory "user\authored_assets") -Destination $publishRoot -Recurse -Force
        Write-Output "[pass] Published only the editor-produced asset files to $publishRoot"
    }

    Write-Output "[pass] Visible $Asset showcase workflow: single mesh primitive, component modeling, material ownership, save, close, reopen, re-edit, and visual captures completed."
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
