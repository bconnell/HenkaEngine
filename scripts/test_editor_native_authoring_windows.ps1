[CmdletBinding()]
param(
    [string]$ExecutablePath = "build\examples\sandbox3d\Debug\henka_sandbox3d.exe",
    [string]$OutputDirectory = "build\test_tmp\editor-native-authoring"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Get-Location).Path
$executable = (Resolve-Path -LiteralPath (Join-Path $repoRoot $ExecutablePath)).Path
$outputRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
$runId = [Guid]::NewGuid().ToString("N")
$runtimeDirectory = Join-Path $outputRoot ("runtime-" + $runId)
$runtimeExecutable = Join-Path $runtimeDirectory "henka_sandbox3d.exe"
$stdoutPath = Join-Path $runtimeDirectory "stdout.log"
$stderrPath = Join-Path $runtimeDirectory "stderr.log"
$automationInputPath = Join-Path $runtimeDirectory "automation-input.events"
$assetName = "EditorProof_" + $runId.Substring(0, 12)
$capturedProcess = $null
$previousAutomationOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
$previousAutomationFile = $env:HENKA_AUTOMATION_INPUT_FILE

. (Join-Path $repoRoot "scripts\henka_script_common.ps1")
. (Join-Path $repoRoot "scripts\henka_ui_automation_helpers.ps1")

function Get-SceneObjectsGeometry {
    param([Parameter(Mandatory = $true)][string]$Path)

    return Get-LastLogRegexMatch `
        -Path $Path `
        -Pattern 'Workspace UI geometry: .*scene_objects=(?<x>[-0-9.]+),(?<y>[-0-9.]+),(?<width>[-0-9.]+),(?<height>[-0-9.]+) '
}

function Send-HenkaAutomationClickAt {
    param(
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][double]$X,
        [Parameter(Mandatory = $true)][double]$Y
    )

    Send-HenkaAutomationClick -EventPath $EventPath -X $X -Y $Y
}

function Clear-HenkaTextField {
    param([Parameter(Mandatory = $true)][string]$EventPath)

    for ($index = 0; $index -lt 63; ++$index) {
        Send-HenkaAutomationEvent `
            -EventPath $EventPath `
            -EventLine "key Backspace down" `
            -SettleMilliseconds 0
        Send-HenkaAutomationEvent `
            -EventPath $EventPath `
            -EventLine "key Backspace up" `
            -SettleMilliseconds 0
    }
    Start-Sleep -Milliseconds 600
}

function Assert-HenkaDocumentTelemetry {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Action,
        [Parameter(Mandatory = $true)][int]$PartCount
    )

    $escapedName = [Regex]::Escape($Name)
    $pattern = "Native asset document: name=$escapedName action=$Action parts=$PartCount\."
    if (-not (Wait-FileContains -Path $Path -Pattern $pattern -TimeoutMilliseconds 5000)) {
        throw "The editor did not report the expected native asset transition: $Action."
    }
}

function Assert-HenkaManifest {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Name
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "The editor did not persist the expected authored asset manifest."
    }
    $manifest = [System.IO.File]::ReadAllText($Path)
    foreach ($requiredLine in @(
        "asset.version=5",
        "asset.name=$Name",
        "asset.part_count=4",
        "asset.revision=1",
        "asset.provenance=HENKA_PRODUCT_NATIVE_AUTHORED")) {
        if (-not $manifest.Contains($requiredLine)) {
            throw "The persisted manifest was missing required editor-owned data: $requiredLine"
        }
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
        -WorkingDirectory $runtimeDirectory `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath

    if (-not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -TimeoutMilliseconds 30000)) {
        throw "The editor did not report a usable UI."
    }
    $geometry = Get-SceneObjectsGeometry -Path $stdoutPath
    if ($null -eq $geometry) {
        throw "The editor did not report the Scene Objects panel geometry."
    }

    $panelX = [double]$geometry.Groups["x"].Value
    $panelY = [double]$geometry.Groups["y"].Value
    $panelWidth = [double]$geometry.Groups["width"].Value
    $panelHeight = [double]$geometry.Groups["height"].Value
    if ($panelWidth -le 0.0 -or $panelHeight -le 0.0) {
        throw "The reported Scene Objects panel geometry was invalid."
    }

    $actionWidth = [Math]::Max(56.0, ($panelWidth - 40.0) / 3.0)
    $nativeActionY = $panelY + 90.0
    $nameFieldX = $panelX + 14.0 + ($actionWidth * 2.0 + 6.0) / 2.0
    $nameFieldY = $nativeActionY + 55.0
    $newAssetX = $panelX + 14.0 + $actionWidth / 2.0
    $newAssetY = $nativeActionY + 85.0
    $openAssetX = $panelX + 14.0 + $actionWidth + 6.0 + $actionWidth / 2.0
    $openAssetY = $newAssetY
    $boxX = $newAssetX
    $boxY = $panelY + 72.0
    $cylinderX = $newAssetX
    $coneX = $openAssetX
    $sphereX = $panelX + 14.0 + ($actionWidth + 6.0) * 2.0 + $actionWidth / 2.0
    $primitiveY = $nativeActionY + 12.0
    $saveAssetX = $newAssetX
    $saveAssetY = $nativeActionY + 70.0
    $closeAssetX = $openAssetX
    $closeAssetY = $saveAssetY

    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $nameFieldX -Y $nameFieldY
    Clear-HenkaTextField -EventPath $automationInputPath
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $assetName
    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $newAssetX -Y $newAssetY
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "created" -PartCount 0

    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $boxX -Y $boxY
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "part-added" -PartCount 1
    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $cylinderX -Y $primitiveY
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "part-added" -PartCount 2
    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $coneX -Y $primitiveY
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "part-added" -PartCount 3
    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $sphereX -Y $primitiveY
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "part-added" -PartCount 4

    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $saveAssetX -Y $saveAssetY
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "saved" -PartCount 4
    Assert-HenkaManifest -Path (Join-Path $runtimeDirectory ("user\saves\" + $assetName + ".asset")) -Name $assetName

    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $closeAssetX -Y $closeAssetY
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "closed" -PartCount 4
    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $openAssetX -Y $openAssetY
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "opened" -PartCount 4

    Write-Output "[pass] Editor-owned native authoring UI: create, add, save, close, and reopen completed."
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
