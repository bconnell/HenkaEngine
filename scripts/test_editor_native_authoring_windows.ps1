[CmdletBinding()]
param(
    [string]$ExecutablePath = "",
    [string]$OutputDirectory = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")
$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$buildRoot = Get-HenkaBuildRoot -RepositoryRoot $repoRoot
$defaultExecutable = Join-Path $buildRoot "examples\sandbox3d\Debug\henka_sandbox3d.exe"
$executable = if ([string]::IsNullOrWhiteSpace($ExecutablePath)) {
    Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $defaultExecutable
} else {
    Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $ExecutablePath
}
$defaultOutput = Join-Path (Get-HenkaTestTemporaryRoot -RepositoryRoot $repoRoot) "editor-native-authoring"
$outputRoot = if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $defaultOutput
} else {
    Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $OutputDirectory
}
$null = New-HenkaLocalDirectory -RepositoryRoot $repoRoot -Path $outputRoot
$null = Write-HenkaGeneratedRootMarker -RepoRoot $repoRoot -Path $outputRoot `
    -Purpose "editor native authoring test output" -RetentionClass "SCRATCH" -Active $true `
    -CleanupEligible $true -CleanupCondition "the owning authoring validation is terminal and any required output has been classified"
$runId = [Guid]::NewGuid().ToString("N")
$runtimeDirectory = Join-Path $outputRoot ("runtime-" + $runId)
$runtimeExecutable = Join-Path $runtimeDirectory "henka_sandbox3d.exe"
$userDataBasePath = Join-Path $runtimeDirectory "user"
$frameCaptureDirectory = Join-Path $runtimeDirectory "frame-captures"
$frameCaptureRequestPath = Join-Path $runtimeDirectory "frame-capture-request.txt"
$stdoutPath = Join-Path $runtimeDirectory "stdout.log"
$stderrPath = Join-Path $runtimeDirectory "stderr.log"
$automationInputPath = Join-Path $runtimeDirectory "automation-input.events"
$assetName = "EditorProof_" + $runId.Substring(0, 12)
$capturedProcess = $null
$previousAutomationOwned = $env:HENKA_AUTOMATION_INPUT_OWNED
$previousAutomationFile = $env:HENKA_AUTOMATION_INPUT_FILE
$previousAutomationUserDataBasePath = $env:HENKA_AUTOMATION_USER_DATA_BASE_PATH
$previousAutomationDiagnostics = $env:HENKA_AUTOMATION_DIAGNOSTICS
$previousAutomationStartHidden = $env:HENKA_AUTOMATION_START_HIDDEN
$previousToolWindowsStartHidden = $env:HENKA_AUTOMATION_TOOL_WINDOWS_START_HIDDEN
$previousAutomationCaptureOutputDirectory = $env:HENKA_AUTOMATION_CAPTURE_OUTPUT_DIRECTORY
$previousAutomationCaptureRequestFile = $env:HENKA_AUTOMATION_CAPTURE_REQUEST_FILE

. (Join-Path $repoRoot "scripts\henka_ui_automation_helpers.ps1")
. (Join-Path $repoRoot "scripts\henka_packaged_startup_readiness.ps1")
. (Join-Path $repoRoot "scripts\henka_packaged_scene_open_workflow.ps1")

function Get-SceneObjectsGeometry {
    param([Parameter(Mandatory = $true)][string]$Path)

    return Get-LastLogRegexMatch `
        -Path $Path `
        -Pattern 'Workspace UI geometry: .*scene_objects=(?<x>[-0-9.]+),(?<y>[-0-9.]+),(?<width>[-0-9.]+),(?<height>[-0-9.]+) '
}

function Get-SceneNameControls {
    param([Parameter(Mandatory = $true)][string]$Path)

    return Get-LastLogRegexMatch `
        -Path $Path `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC scene_name_controls available=1 label_rect=(?<label_x>[-0-9.]+),(?<label_y>[-0-9.]+),(?<label_width>[-0-9.]+),(?<label_height>[-0-9.]+) field_rect=(?<field_x>[-0-9.]+),(?<field_y>[-0-9.]+),(?<field_width>[-0-9.]+),(?<field_height>[-0-9.]+) rename_rect=(?<rename_x>[-0-9.]+),(?<rename_y>[-0-9.]+),(?<rename_width>[-0-9.]+),(?<rename_height>[-0-9.]+)\.'
}

function Get-DetailsPanelGeometry {
    param([Parameter(Mandatory = $true)][string]$Path)

    return Get-LastLogRegexMatch `
        -Path $Path `
        -Pattern 'Workspace UI geometry: .*details=(?<x>[-0-9.]+),(?<y>[-0-9.]+),(?<width>[-0-9.]+),(?<height>[-0-9.]+)\.'
}

function Get-SceneDocumentActionControls {
    param([Parameter(Mandatory = $true)][string]$Path)

    return Get-LastLogRegexMatch `
        -Path $Path `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_action_controls visible=1 document_object_count=(?<document_object_count>\d+) selection_present=(?<selection_present>[01]) relative_path_empty=(?<relative_path_empty>[01]) save_rect=(?<save_x>[-0-9.]+),(?<save_y>[-0-9.]+),(?<save_width>[-0-9.]+),(?<save_height>[-0-9.]+) save_as_rect=(?<save_as_x>[-0-9.]+),(?<save_as_y>[-0-9.]+),(?<save_as_width>[-0-9.]+),(?<save_as_height>[-0-9.]+) reload_rect=(?<reload_x>[-0-9.]+),(?<reload_y>[-0-9.]+),(?<reload_width>[-0-9.]+),(?<reload_height>[-0-9.]+) new_scene_rect=(?<new_scene_x>[-0-9.]+),(?<new_scene_y>[-0-9.]+),(?<new_scene_width>[-0-9.]+),(?<new_scene_height>[-0-9.]+) open_scene_rect=(?<open_scene_x>[-0-9.]+),(?<open_scene_y>[-0-9.]+),(?<open_scene_width>[-0-9.]+),(?<open_scene_height>[-0-9.]+)\.'
}

function Get-SceneSaveAsControls {
    param([Parameter(Mandatory = $true)][string]$Path)

    return Get-LastLogRegexMatch `
        -Path $Path `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC scene_save_as_controls visible=1 path_rect=(?<path_x>[-0-9.]+),(?<path_y>[-0-9.]+),(?<path_width>[-0-9.]+),(?<path_height>[-0-9.]+) save_rect=(?<save_x>[-0-9.]+),(?<save_y>[-0-9.]+),(?<save_width>[-0-9.]+),(?<save_height>[-0-9.]+) cancel_rect=(?<cancel_x>[-0-9.]+),(?<cancel_y>[-0-9.]+),(?<cancel_width>[-0-9.]+),(?<cancel_height>[-0-9.]+) relative_path_empty=(?<relative_path_empty>[01])\.'
}

function Get-SceneOpenControls {
    param([Parameter(Mandatory = $true)][string]$Path)

    return Get-LastLogRegexMatch -Path $Path -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC scene_open_controls visible=1 path_rect=(?<path_x>[-0-9.]+),(?<path_y>[-0-9.]+),(?<path_width>[-0-9.]+),(?<path_height>[-0-9.]+) open_rect=(?<open_x>[-0-9.]+),(?<open_y>[-0-9.]+),(?<open_width>[-0-9.]+),(?<open_height>[-0-9.]+) cancel_rect=(?<cancel_x>[-0-9.]+),(?<cancel_y>[-0-9.]+),(?<cancel_width>[-0-9.]+),(?<cancel_height>[-0-9.]+) relative_path_empty=(?<relative_path_empty>[01])\.'
}

function Save-HenkaProductFramebufferScreenshot {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$CaptureDirectory,
        [Parameter(Mandatory = $true)][string]$RequestFile,
        [Parameter(Mandatory = $true)][string]$Description,
        [Parameter(Mandatory = $true)][string]$DestinationPath
    )

    $bitmap = New-HenkaAutomationProductFramebufferBitmap `
        -Handle $Process.MainWindowHandle `
        -Process $Process `
        -StdoutPath $StdoutPath `
        -OutputDirectory $CaptureDirectory `
        -RequestFile $RequestFile `
        -Description $Description
    try {
        $geometry = Get-HenkaAutomationWindowGeometry -Handle $Process.MainWindowHandle
        if ($bitmap.Width -ne $geometry.ClientWidth -or
            $bitmap.Height -ne $geometry.ClientHeight) {
            throw "$Description framebuffer was $($bitmap.Width)x$($bitmap.Height), expected client size $($geometry.ClientWidth)x$($geometry.ClientHeight)."
        }
        $content = Get-HenkaAutomationScreenshotContentSummary -Bitmap $bitmap
        if (-not $content.HasVisibleContent) {
            throw ("$Description framebuffer was blank or uniform; sampled={0}, distinct_colors={1}, " +
                "non_dominant_samples={2}, minimum={3}." -f
                $content.SampleCount,
                $content.DistinctColorCount,
                $content.NonDominantSamples,
                $content.MinimumNonDominantSamples)
        }
        $bitmap.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }

    return $DestinationPath
}

function Get-SceneTransitionConfirmationControls {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][ValidateSet("new", "reload", "open")][string]$Action
    )

    $pattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_confirmation action=' +
        [Regex]::Escape($Action) +
        ' available=1 cancel_rect=(?<cancel_x>[-0-9.]+),(?<cancel_y>[-0-9.]+),(?<cancel_width>[-0-9.]+),(?<cancel_height>[-0-9.]+) confirm_rect=(?<confirm_x>[-0-9.]+),(?<confirm_y>[-0-9.]+),(?<confirm_width>[-0-9.]+),(?<confirm_height>[-0-9.]+)\.'
    return Get-LastLogRegexMatch `
        -Path $Path `
        -Pattern $pattern
}

function Get-SceneObjectsControl {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Id,
        [Parameter(Mandatory = $true)][bool]$AssetOpen
    )

    $assetOpenValue = if ($AssetOpen) { '1' } else { '0' }
    $pattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_objects_control id=' +
        [Regex]::Escape($Id) +
        ' available=1 asset_open=' + $assetOpenValue +
        ' rect=(?<x>[-0-9.]+),(?<y>[-0-9.]+),(?<width>[-0-9.]+),(?<height>[-0-9.]+)\.'
    $match = Get-LastLogRegexMatch -Path $Path -Pattern $pattern
    if ($null -eq $match) {
        throw "The product did not report an available Scene Objects control '$Id' in the expected asset state."
    }

    $control = [pscustomobject]@{
        Id = $Id
        X = Convert-HenkaAutomationNumber $match.Groups['x'].Value
        Y = Convert-HenkaAutomationNumber $match.Groups['y'].Value
        Width = Convert-HenkaAutomationNumber $match.Groups['width'].Value
        Height = Convert-HenkaAutomationNumber $match.Groups['height'].Value
    }
    if ($control.Width -le 0.0 -or $control.Height -le 0.0) {
        throw "The product-reported Scene Objects control '$Id' has empty geometry."
    }

    $panel = Get-SceneObjectsGeometry -Path $Path
    if ($null -eq $panel) {
        throw "The product did not report the Scene Objects panel containing '$Id'."
    }
    $panelX = Convert-HenkaAutomationNumber $panel.Groups['x'].Value
    $panelY = Convert-HenkaAutomationNumber $panel.Groups['y'].Value
    $panelWidth = Convert-HenkaAutomationNumber $panel.Groups['width'].Value
    $panelHeight = Convert-HenkaAutomationNumber $panel.Groups['height'].Value
    if ($control.X -lt ($panelX - 0.5) -or
        $control.Y -lt ($panelY - 0.5) -or
        ($control.X + $control.Width) -gt ($panelX + $panelWidth + 0.5) -or
        ($control.Y + $control.Height) -gt ($panelY + $panelHeight + 0.5)) {
        throw "The product-reported Scene Objects control '$Id' escaped its visible panel."
    }

    return $control
}

function Send-HenkaSceneObjectsControlClick {
    param(
        [Parameter(Mandatory = $true)][string]$EventPath,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$Id,
        [Parameter(Mandatory = $true)][bool]$AssetOpen
    )

    $control = Get-SceneObjectsControl -Path $StdoutPath -Id $Id -AssetOpen $AssetOpen
    $x = $control.X + ($control.Width * 0.5)
    $y = $control.Y + ($control.Height * 0.5)
    if ($x -lt $control.X -or $x -gt ($control.X + $control.Width) -or
        $y -lt $control.Y -or $y -gt ($control.Y + $control.Height)) {
        throw "The click target for product control '$Id' is outside its reported hit rectangle."
    }

    Send-HenkaAutomationClickAt -EventPath $EventPath -X $x -Y $y
}

function Convert-HenkaAutomationNumber {
    param([Parameter(Mandatory = $true)][string]$Value)

    return [double]::Parse(
        $Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
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
    New-Item -ItemType Directory -Path $userDataBasePath -Force | Out-Null
    if ((Test-Path -LiteralPath $frameCaptureDirectory) -or
        (Test-Path -LiteralPath $frameCaptureRequestPath)) {
        throw "The unique product framebuffer capture paths unexpectedly already exist."
    }
    New-Item -ItemType Directory -Path $frameCaptureDirectory -ErrorAction Stop | Out-Null
    Copy-Item -LiteralPath $executable -Destination $runtimeExecutable
    Copy-Item `
        -LiteralPath (Join-Path (Split-Path -Parent $executable) "assets") `
        -Destination (Join-Path $runtimeDirectory "assets") `
        -Recurse
    New-Item -ItemType File -Path $automationInputPath -Force | Out-Null

    $env:HENKA_AUTOMATION_INPUT_OWNED = "1"
    $env:HENKA_AUTOMATION_INPUT_FILE = $automationInputPath
    $env:HENKA_AUTOMATION_USER_DATA_BASE_PATH = $userDataBasePath
    $env:HENKA_AUTOMATION_DIAGNOSTICS = "1"
    $env:HENKA_AUTOMATION_START_HIDDEN = "1"
    $env:HENKA_AUTOMATION_TOOL_WINDOWS_START_HIDDEN = "1"
    $env:HENKA_AUTOMATION_CAPTURE_OUTPUT_DIRECTORY = $frameCaptureDirectory
    $env:HENKA_AUTOMATION_CAPTURE_REQUEST_FILE = $frameCaptureRequestPath
    $capturedProcess = Start-HenkaCapturedProcess `
        -FilePath $runtimeExecutable `
        -WorkingDirectory $runtimeDirectory `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath `
        -StartOffscreenWithoutActivation

    $startupReadiness = Wait-HenkaPackagedStartupReady `
        -StdoutPath $stdoutPath `
        -StderrPath $stderrPath `
        -ProcessId $capturedProcess.Process.Id `
        -HardTimeoutMilliseconds 120000 `
        -NoProgressTimeoutMilliseconds 45000
    if (-not $startupReadiness.Ready -or
        -not (Wait-FileContains -Path $stdoutPath -Pattern "Sandbox UI ready:" -TimeoutMilliseconds 10000)) {
        throw "The editor did not report a usable UI after staged startup readiness."
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

    $sceneNameControlPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_name_controls available=1 label_rect='
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $sceneNameControlPattern -TimeoutMilliseconds 15000)) {
        throw "The product did not expose its available Scene Name controls."
    }
    $sceneNameControls = Get-SceneNameControls -Path $stdoutPath
    if ($null -eq $sceneNameControls) {
        throw "The product Scene Name control geometry was missing or malformed."
    }
    $sceneNameControlRects = @(
        [pscustomobject]@{
            X = Convert-HenkaAutomationNumber $sceneNameControls.Groups['label_x'].Value
            Y = Convert-HenkaAutomationNumber $sceneNameControls.Groups['label_y'].Value
            Width = Convert-HenkaAutomationNumber $sceneNameControls.Groups['label_width'].Value
            Height = Convert-HenkaAutomationNumber $sceneNameControls.Groups['label_height'].Value
        },
        [pscustomobject]@{
            X = Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_x'].Value
            Y = Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_y'].Value
            Width = Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_width'].Value
            Height = Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_height'].Value
        },
        [pscustomobject]@{
            X = Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_x'].Value
            Y = Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_y'].Value
            Width = Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_width'].Value
            Height = Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_height'].Value
        })
    foreach ($rect in $sceneNameControlRects) {
        if ($rect.Width -le 0.0 -or $rect.Height -le 0.0 -or
            $rect.X -lt ($panelX - 0.5) -or $rect.Y -lt ($panelY - 0.5) -or
            ($rect.X + $rect.Width) -gt ($panelX + $panelWidth + 0.5) -or
            ($rect.Y + $rect.Height) -gt ($panelY + $panelHeight + 0.5)) {
            throw "A product Scene Name control escaped its visible Scene Objects panel."
        }
    }

    $sceneName = "Scene Rename " + $runId.Substring(0, 10)
    $sceneNameFieldX = $sceneNameControlRects[1].X +
        ($sceneNameControlRects[1].Width * 0.5)
    $sceneNameFieldY = $sceneNameControlRects[1].Y +
        ($sceneNameControlRects[1].Height * 0.5)
    $sceneNameApplyX = $sceneNameControlRects[2].X +
        ($sceneNameControlRects[2].Width * 0.5)
    $sceneNameApplyY = $sceneNameControlRects[2].Y +
        ($sceneNameControlRects[2].Height * 0.5)
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X $sceneNameFieldX `
        -Y $sceneNameFieldY
    Clear-HenkaTextField -EventPath $automationInputPath
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $sceneName
    $typedNamePattern = 'HENKA_AUTOMATION_DIAGNOSTIC input record=\d+ type=text'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $typedNamePattern -TimeoutMilliseconds 15000)) {
        throw "The packaged product did not consume Scene Name text input."
    }
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X $sceneNameApplyX `
        -Y $sceneNameApplyY
    $renamedPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_name_rename result=success name=' +
        [Regex]::Escape($sceneName) + '\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $renamedPattern -TimeoutMilliseconds 10000)) {
        throw "The real Scene Objects Rename control did not publish the requested canonical scene name."
    }

    # Use the row report that is refreshed when either its production rectangle
    # or selection state changes. The richer hierarchy report is emitted only
    # when the whole panel layout changes and can therefore be stale after a
    # local row reflow.
    $groundPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row entity=(?<entity>\d+) name=Ground display=.*? lines=\d+ hidden=(?<hidden>[01]) selected=(?<selected>[01]) x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+)\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $groundPattern -TimeoutMilliseconds 10000)) {
        throw "The packaged Scene Objects hierarchy did not expose the authored Ground row."
    }
    $groundRow = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $groundPattern
    if ($null -eq $groundRow) {
        throw "The product Ground row geometry could not be parsed."
    }
    $groundClickX = (Convert-HenkaAutomationNumber $groundRow.Groups['x'].Value) +
        ((Convert-HenkaAutomationNumber $groundRow.Groups['width'].Value) * 0.5)
    $groundClickY = (Convert-HenkaAutomationNumber $groundRow.Groups['y'].Value) +
        ((Convert-HenkaAutomationNumber $groundRow.Groups['height'].Value) * 0.5)
    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $groundClickX -Y $groundClickY
    $groundEntity = $groundRow.Groups['entity'].Value
    $selectedGroundPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row entity=' +
        [Regex]::Escape($groundEntity) + ' name=Ground .*? selected=1 '
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $selectedGroundPattern -TimeoutMilliseconds 10000)) {
        throw "The production Scene Objects row did not select the canonical Ground entity."
    }
    $selectedGround = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $groundPattern
    if ($null -eq $selectedGround -or $selectedGround.Groups['selected'].Value -ne '1') {
        throw "The latest product-owned Ground row state did not confirm authoritative selection."
    }

    $detailsGeometry = Get-DetailsPanelGeometry -Path $stdoutPath
    if ($null -eq $detailsGeometry) {
        throw "The product did not report Object Details panel geometry."
    }
    $detailsX = Convert-HenkaAutomationNumber $detailsGeometry.Groups['x'].Value
    $detailsY = Convert-HenkaAutomationNumber $detailsGeometry.Groups['y'].Value
    $detailsWidth = Convert-HenkaAutomationNumber $detailsGeometry.Groups['width'].Value
    $detailsHeight = Convert-HenkaAutomationNumber $detailsGeometry.Groups['height'].Value
    if ($detailsWidth -le 0.0 -or $detailsHeight -le 0.0) {
        throw "The reported Object Details panel geometry was invalid."
    }
    $actionsDisclosurePattern = 'HENKA_AUTOMATION_DIAGNOSTIC object_details_disclosure id=actions entity=' +
        [Regex]::Escape($groundEntity) + ' x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+) expanded=(?<expanded>[01])\.'
    $actionsDisclosure = $null
    for ($scroll = 0; $scroll -lt 24 -and $null -eq $actionsDisclosure; ++$scroll) {
        $actionsDisclosure = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $actionsDisclosurePattern
        if ($null -eq $actionsDisclosure) {
            Send-HenkaAutomationScroll `
                -EventPath $automationInputPath `
                -X ($detailsX + ($detailsWidth * 0.5)) `
                -Y ($detailsY + ($detailsHeight * 0.5)) `
                -WheelDelta -1.0
        }
    }
    if ($null -eq $actionsDisclosure) {
        throw "The real Object Details Actions disclosure was not reachable through its panel."
    }
    if ($actionsDisclosure.Groups['expanded'].Value -eq '0') {
        $actionsX = Convert-HenkaAutomationNumber $actionsDisclosure.Groups['x'].Value
        $actionsY = Convert-HenkaAutomationNumber $actionsDisclosure.Groups['y'].Value
        $actionsDisclosureWidth = Convert-HenkaAutomationNumber $actionsDisclosure.Groups['width'].Value
        $actionsDisclosureHeight = Convert-HenkaAutomationNumber $actionsDisclosure.Groups['height'].Value
        Send-HenkaAutomationClickAt `
            -EventPath $automationInputPath `
            -X ($actionsX + ($actionsDisclosureWidth * 0.5)) `
            -Y ($actionsY + ($actionsDisclosureHeight * 0.5))
        $expandedActionsPattern = 'HENKA_AUTOMATION_DIAGNOSTIC object_details_disclosure id=actions entity=' +
            [Regex]::Escape($groundEntity) + ' .* expanded=1\.'
        if (-not (Wait-FileContains -Path $stdoutPath -Pattern $expandedActionsPattern -TimeoutMilliseconds 10000)) {
            throw "The product Object Details Actions disclosure did not open through its real hit target."
        }
    }

    $sceneActionControls = $null
    for ($scroll = 0; $scroll -lt 24 -and $null -eq $sceneActionControls; ++$scroll) {
        $sceneActionControls = Get-SceneDocumentActionControls -Path $stdoutPath
        if ($null -eq $sceneActionControls) {
            Send-HenkaAutomationScroll `
                -EventPath $automationInputPath `
                -X ($detailsX + ($detailsWidth * 0.5)) `
                -Y ($detailsY + ($detailsHeight * 0.5)) `
                -WheelDelta -1.0
        }
    }
    if ($null -eq $sceneActionControls) {
        throw "The product Save Scene and Reload Scene controls were not reachable in Object Details."
    }
    $saveX = Convert-HenkaAutomationNumber $sceneActionControls.Groups['save_x'].Value
    $saveY = Convert-HenkaAutomationNumber $sceneActionControls.Groups['save_y'].Value
    $saveWidth = Convert-HenkaAutomationNumber $sceneActionControls.Groups['save_width'].Value
    $saveHeight = Convert-HenkaAutomationNumber $sceneActionControls.Groups['save_height'].Value
    $saveAsX = Convert-HenkaAutomationNumber $sceneActionControls.Groups['save_as_x'].Value
    $saveAsY = Convert-HenkaAutomationNumber $sceneActionControls.Groups['save_as_y'].Value
    $saveAsWidth = Convert-HenkaAutomationNumber $sceneActionControls.Groups['save_as_width'].Value
    $saveAsHeight = Convert-HenkaAutomationNumber $sceneActionControls.Groups['save_as_height'].Value
    $reloadX = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_x'].Value
    $reloadY = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_y'].Value
    $reloadWidth = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_width'].Value
    $reloadHeight = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_height'].Value
    $newSceneX = Convert-HenkaAutomationNumber $sceneActionControls.Groups['new_scene_x'].Value
    $newSceneY = Convert-HenkaAutomationNumber $sceneActionControls.Groups['new_scene_y'].Value
    $newSceneWidth = Convert-HenkaAutomationNumber $sceneActionControls.Groups['new_scene_width'].Value
    $newSceneHeight = Convert-HenkaAutomationNumber $sceneActionControls.Groups['new_scene_height'].Value
    $openSceneX = Convert-HenkaAutomationNumber $sceneActionControls.Groups['open_scene_x'].Value
    $openSceneY = Convert-HenkaAutomationNumber $sceneActionControls.Groups['open_scene_y'].Value
    $openSceneWidth = Convert-HenkaAutomationNumber $sceneActionControls.Groups['open_scene_width'].Value
    $openSceneHeight = Convert-HenkaAutomationNumber $sceneActionControls.Groups['open_scene_height'].Value
    foreach ($actionRect in @(
            [pscustomobject]@{ X = $saveX; Y = $saveY; Width = $saveWidth; Height = $saveHeight },
            [pscustomobject]@{ X = $saveAsX; Y = $saveAsY; Width = $saveAsWidth; Height = $saveAsHeight },
            [pscustomobject]@{ X = $reloadX; Y = $reloadY; Width = $reloadWidth; Height = $reloadHeight },
            [pscustomobject]@{ X = $newSceneX; Y = $newSceneY; Width = $newSceneWidth; Height = $newSceneHeight },
            [pscustomobject]@{ X = $openSceneX; Y = $openSceneY; Width = $openSceneWidth; Height = $openSceneHeight })) {
        if ($actionRect.Width -le 0.0 -or $actionRect.Height -le 0.0 -or
            $actionRect.X -lt ($detailsX - 0.5) -or $actionRect.Y -lt ($detailsY - 0.5) -or
            ($actionRect.X + $actionRect.Width) -gt ($detailsX + $detailsWidth + 0.5) -or
            ($actionRect.Y + $actionRect.Height) -gt ($detailsY + $detailsHeight + 0.5)) {
            throw "A New/Open/Save/Save As/Reload control escaped the visible Object Details panel."
        }
    }
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($saveX + ($saveWidth * 0.5)) `
        -Y ($saveY + ($saveHeight * 0.5))
    $savedNamePattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=saved name=' +
        [Regex]::Escape($sceneName) + '\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $savedNamePattern -TimeoutMilliseconds 10000)) {
        throw "The real Save Scene control did not persist the renamed Scene Document."
    }
    $savedScenePath = Join-Path $userDataBasePath "sandbox3d_scene.hscene"
    if (-not (Test-Path -LiteralPath $savedScenePath -PathType Leaf)) {
        throw "The packaged Save Scene action reported success without creating its expected scene document."
    }
    $savedSceneHash = (Get-FileHash -LiteralPath $savedScenePath -Algorithm SHA256).Hash

    $transientSceneName = $sceneName + " Draft"
    $sceneNameControls = Get-SceneNameControls -Path $stdoutPath
    if ($null -eq $sceneNameControls) {
        throw "Scene Name geometry was unavailable for the post-save mutation."
    }
    $sceneNameFieldX = (Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_x'].Value) +
        ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_width'].Value) * 0.5)
    $sceneNameFieldY = (Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_y'].Value) +
        ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_height'].Value) * 0.5)
    $sceneNameApplyX = (Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_x'].Value) +
        ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_width'].Value) * 0.5)
    $sceneNameApplyY = (Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_y'].Value) +
        ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_height'].Value) * 0.5)
    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $sceneNameFieldX -Y $sceneNameFieldY
    Clear-HenkaTextField -EventPath $automationInputPath
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $transientSceneName
    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $sceneNameApplyX -Y $sceneNameApplyY
    $transientNamePattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_name_rename result=success name=' +
        [Regex]::Escape($transientSceneName) + '\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $transientNamePattern -TimeoutMilliseconds 10000)) {
        throw "The unsaved Scene Name change did not reach the canonical document before reload."
    }
    $detailsGeometry = Get-DetailsPanelGeometry -Path $stdoutPath
    $sceneActionControls = Get-SceneDocumentActionControls -Path $stdoutPath
    if ($null -eq $detailsGeometry -or $null -eq $sceneActionControls) {
        throw "The product did not provide current Object Details and Reload Scene geometry after editing the name."
    }
    $detailsX = Convert-HenkaAutomationNumber $detailsGeometry.Groups['x'].Value
    $detailsY = Convert-HenkaAutomationNumber $detailsGeometry.Groups['y'].Value
    $detailsWidth = Convert-HenkaAutomationNumber $detailsGeometry.Groups['width'].Value
    $detailsHeight = Convert-HenkaAutomationNumber $detailsGeometry.Groups['height'].Value
    $reloadX = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_x'].Value
    $reloadY = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_y'].Value
    $reloadWidth = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_width'].Value
    $reloadHeight = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_height'].Value
    if ($reloadWidth -le 0.0 -or $reloadHeight -le 0.0 -or
        $reloadX -lt ($detailsX - 0.5) -or $reloadY -lt ($detailsY - 0.5) -or
        ($reloadX + $reloadWidth) -gt ($detailsX + $detailsWidth + 0.5) -or
        ($reloadY + $reloadHeight) -gt ($detailsY + $detailsHeight + 0.5)) {
        throw "The current product Reload Scene target escaped Object Details after the name edit."
    }
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($reloadX + ($reloadWidth * 0.5)) `
        -Y ($reloadY + ($reloadHeight * 0.5))
    $reloadClickPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=reload_button_clicked play_locked=(?<locked>[01])\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $reloadClickPattern -TimeoutMilliseconds 10000)) {
        $capturedProcess.Process.Refresh()
        if ($capturedProcess.Process.HasExited) {
            throw "The packaged process exited with code $($capturedProcess.Process.ExitCode) before the Reload Scene button activated."
        }
        throw "The current product Reload Scene rectangle did not activate the real button control."
    }
    $reloadClick = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $reloadClickPattern
    if ($null -eq $reloadClick -or $reloadClick.Groups['locked'].Value -ne '0') {
        throw "Reload Scene was activated while Game Authoring was play-locked."
    }
    $reloadConfirmationPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=reload_confirmation_pending name=' +
        [Regex]::Escape($transientSceneName) + '\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $reloadConfirmationPattern -TimeoutMilliseconds 5000)) {
        $prematureReloadPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=reloaded name=' +
            [Regex]::Escape($sceneName) + '\.'
        if (Wait-FileContains -Path $stdoutPath -Pattern $prematureReloadPattern -TimeoutMilliseconds 100) {
            throw "The first Reload Scene click discarded the newer unsaved Scene Name without confirmation."
        }
        throw "Reload Scene did not expose a confirmation state after the first click."
    }
    $confirmationControls = Get-SceneTransitionConfirmationControls -Path $stdoutPath -Action "reload"
    if ($null -eq $confirmationControls) {
        throw "The product opened Reload Scene confirmation without exposing its real Cancel and Confirm controls."
    }
    $cancelX = Convert-HenkaAutomationNumber $confirmationControls.Groups['cancel_x'].Value
    $cancelY = Convert-HenkaAutomationNumber $confirmationControls.Groups['cancel_y'].Value
    $cancelWidth = Convert-HenkaAutomationNumber $confirmationControls.Groups['cancel_width'].Value
    $cancelHeight = Convert-HenkaAutomationNumber $confirmationControls.Groups['cancel_height'].Value
    $confirmX = Convert-HenkaAutomationNumber $confirmationControls.Groups['confirm_x'].Value
    $confirmY = Convert-HenkaAutomationNumber $confirmationControls.Groups['confirm_y'].Value
    $confirmWidth = Convert-HenkaAutomationNumber $confirmationControls.Groups['confirm_width'].Value
    $confirmHeight = Convert-HenkaAutomationNumber $confirmationControls.Groups['confirm_height'].Value
    if ($cancelWidth -le 0.0 -or $cancelHeight -le 0.0 -or
        $confirmWidth -le 0.0 -or $confirmHeight -le 0.0 -or
        $cancelX -lt ($detailsX - 0.5) -or $cancelY -lt ($detailsY - 0.5) -or
        ($cancelX + $cancelWidth) -gt ($detailsX + $detailsWidth + 0.5) -or
        ($cancelY + $cancelHeight) -gt ($detailsY + $detailsHeight + 0.5) -or
        $confirmX -lt ($detailsX - 0.5) -or $confirmY -lt ($detailsY - 0.5) -or
        ($confirmX + $confirmWidth) -gt ($detailsX + $detailsWidth + 0.5) -or
        ($confirmY + $confirmHeight) -gt ($detailsY + $detailsHeight + 0.5)) {
        throw "A Reload Scene confirmation control reported invalid or off-panel hit geometry."
    }
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($cancelX + ($cancelWidth * 0.5)) `
        -Y ($cancelY + ($cancelHeight * 0.5))
    $cancelledPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=reload_confirmation_cancelled name=' +
        [Regex]::Escape($transientSceneName) + '(?=\s|$)'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $cancelledPattern -TimeoutMilliseconds 10000)) {
        throw "Cancel did not preserve the unsaved Scene Name in the live document."
    }
    $reloadedBeforeConfirmPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=reloaded name='
    if (Wait-FileContains -Path $stdoutPath -Pattern $reloadedBeforeConfirmPattern -TimeoutMilliseconds 100) {
        throw "Cancelling Reload Scene still replaced the live document."
    }

    $cancelledName = $transientSceneName + " Kept"
    $sceneNameControls = Get-SceneNameControls -Path $stdoutPath
    if ($null -eq $sceneNameControls) {
        throw "Scene Name controls were unavailable after cancelling reload."
    }
    $sceneNameFieldX = (Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_x'].Value) +
        ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_width'].Value) * 0.5)
    $sceneNameFieldY = (Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_y'].Value) +
        ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_height'].Value) * 0.5)
    $sceneNameApplyX = (Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_x'].Value) +
        ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_width'].Value) * 0.5)
    $sceneNameApplyY = (Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_y'].Value) +
        ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_height'].Value) * 0.5)
    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $sceneNameFieldX -Y $sceneNameFieldY
    Clear-HenkaTextField -EventPath $automationInputPath
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $cancelledName
    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $sceneNameApplyX -Y $sceneNameApplyY
    $cancelledNamePattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_name_rename result=success name=' +
        [Regex]::Escape($cancelledName) + '\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $cancelledNamePattern -TimeoutMilliseconds 10000)) {
        throw "The post-cancel scene edit did not reach the canonical document."
    }
    $sceneActionControls = Get-SceneDocumentActionControls -Path $stdoutPath
    if ($null -eq $sceneActionControls) {
        throw "Reload Scene controls were unavailable after cancelling confirmation."
    }
    $reloadX = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_x'].Value
    $reloadY = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_y'].Value
    $reloadWidth = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_width'].Value
    $reloadHeight = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_height'].Value
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($reloadX + ($reloadWidth * 0.5)) `
        -Y ($reloadY + ($reloadHeight * 0.5))
    $secondPendingPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=reload_confirmation_pending name=' +
        [Regex]::Escape($cancelledName) + '\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $secondPendingPattern -TimeoutMilliseconds 10000)) {
        throw "The second Reload Scene click did not request confirmation for the current unsaved name."
    }
    $confirmationControls = Get-SceneTransitionConfirmationControls -Path $stdoutPath -Action "reload"
    if ($null -eq $confirmationControls) {
        throw "The second Reload Scene confirmation did not expose its real controls."
    }
    $confirmX = Convert-HenkaAutomationNumber $confirmationControls.Groups['confirm_x'].Value
    $confirmY = Convert-HenkaAutomationNumber $confirmationControls.Groups['confirm_y'].Value
    $confirmWidth = Convert-HenkaAutomationNumber $confirmationControls.Groups['confirm_width'].Value
    $confirmHeight = Convert-HenkaAutomationNumber $confirmationControls.Groups['confirm_height'].Value
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($confirmX + ($confirmWidth * 0.5)) `
        -Y ($confirmY + ($confirmHeight * 0.5))
    $reloadAttemptPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=reload_attempt result=(?<result>-?\d+)\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $reloadAttemptPattern -TimeoutMilliseconds 10000)) {
        $capturedProcess.Process.Refresh()
        if ($capturedProcess.Process.HasExited) {
            throw "The packaged process exited with code $($capturedProcess.Process.ExitCode) during Scene Document reload."
        }
        throw "The Reload Scene control activated, but the product reload did not return a result."
    }
    $reloadAttempt = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $reloadAttemptPattern
    if ($null -eq $reloadAttempt -or [int]$reloadAttempt.Groups['result'].Value -ne 0) {
        $resultText = if ($null -eq $reloadAttempt) {
            'missing product result'
        } else {
            'Henka result ' + $reloadAttempt.Groups['result'].Value
        }
        throw "The product Reload Scene handler failed: $resultText."
    }
    $reloadedNamePattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=reloaded name=' +
        [Regex]::Escape($sceneName) + '\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $reloadedNamePattern -TimeoutMilliseconds 10000)) {
        throw "Reload Scene did not restore the saved canonical Scene Document name."
    }
    Write-Output "[pass] Packaged Scene Objects Rename, Save Scene, Reload Scene confirmation/cancel, and confirmed reload preserved '$sceneName'."

    $newSceneDraftName = $sceneName + " New Scene Draft"
    $sceneNameControls = Get-SceneNameControls -Path $stdoutPath
    if ($null -eq $sceneNameControls) {
        throw "Scene Name controls were unavailable before the New Scene cancellation check."
    }
    $sceneNameFieldX = (Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_x'].Value) +
        ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_width'].Value) * 0.5)
    $sceneNameFieldY = (Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_y'].Value) +
        ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_height'].Value) * 0.5)
    $sceneNameApplyX = (Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_x'].Value) +
        ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_width'].Value) * 0.5)
    $sceneNameApplyY = (Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_y'].Value) +
        ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_height'].Value) * 0.5)
    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $sceneNameFieldX -Y $sceneNameFieldY
    Clear-HenkaTextField -EventPath $automationInputPath
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $newSceneDraftName
    Send-HenkaAutomationClickAt -EventPath $automationInputPath -X $sceneNameApplyX -Y $sceneNameApplyY
    $newSceneDraftNamePattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_name_rename result=success name=' +
        [Regex]::Escape($newSceneDraftName) + '\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $newSceneDraftNamePattern -TimeoutMilliseconds 10000)) {
        throw "The unsaved Scene Name edit did not reach the canonical document before New Scene."
    }

    $terrainTabMatch = Get-LastLogRegexMatch `
        -Path $stdoutPath `
        -Pattern 'Terrain utility tab: x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+)\.'
    if ($null -eq $terrainTabMatch) {
        throw "The packaged New Scene test could not reach the production Terrain Utility tab."
    }
    $terrainTabX = Convert-HenkaAutomationNumber $terrainTabMatch.Groups['x'].Value
    $terrainTabY = Convert-HenkaAutomationNumber $terrainTabMatch.Groups['y'].Value
    $terrainTabWidth = Convert-HenkaAutomationNumber $terrainTabMatch.Groups['width'].Value
    $terrainTabHeight = Convert-HenkaAutomationNumber $terrainTabMatch.Groups['height'].Value
    $terrainUtilityOffset = Get-FileLengthSafe -Path $stdoutPath
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($terrainTabX + ($terrainTabWidth * 0.5)) `
        -Y ($terrainTabY + ($terrainTabHeight * 0.5))
    if (-not (Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern 'Utility active: Terrain\.' `
            -StartingOffset $terrainUtilityOffset `
            -TimeoutMilliseconds 10000)) {
        throw "The real packaged Terrain Utility tab click did not activate Terrain before New Scene."
    }
    $terrainCreateMatch = Get-LastLogRegexMatch `
        -Path $stdoutPath `
        -Pattern 'Terrain Create control: x=(?<x>[-0-9.]+) y=(?<y>[-0-9.]+) width=(?<width>[-0-9.]+) height=(?<height>[-0-9.]+)\.'
    if ($null -eq $terrainCreateMatch) {
        throw "The production Create Terrain control was unavailable before the New Scene transaction."
    }
    $terrainCreateX = Convert-HenkaAutomationNumber $terrainCreateMatch.Groups['x'].Value
    $terrainCreateY = Convert-HenkaAutomationNumber $terrainCreateMatch.Groups['y'].Value
    $terrainCreateWidth = Convert-HenkaAutomationNumber $terrainCreateMatch.Groups['width'].Value
    $terrainCreateHeight = Convert-HenkaAutomationNumber $terrainCreateMatch.Groups['height'].Value
    $terrainCreateOffset = Get-FileLengthSafe -Path $stdoutPath
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($terrainCreateX + ($terrainCreateWidth * 0.5)) `
        -Y ($terrainCreateY + ($terrainCreateHeight * 0.5))
    if (-not (Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern 'Terrain authoring content created\.' `
            -StartingOffset $terrainCreateOffset `
            -TimeoutMilliseconds 20000) -or
        -not (Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern 'Terrain render: 16 bounded chunks resident;' `
            -StartingOffset $terrainCreateOffset `
            -TimeoutMilliseconds 20000)) {
        throw "The packaged UI could not create active Terrain before the New Scene transaction."
    }
    Write-Output "[pass] Created Terrain through its real Utility action before New Scene."

    $terrainViewportMatch = Get-LastLogRegexMatch `
        -Path $stdoutPath `
        -Pattern 'CAPTURE_READY_STARTUP mode=startup viewport=(?<x>[-0-9.]+),(?<y>[-0-9.]+),(?<width>[-0-9.]+),(?<height>[-0-9.]+) '
    if ($null -eq $terrainViewportMatch) {
        throw "The packaged Terrain edit could not obtain the product-reported scene viewport."
    }
    $terrainViewportX = Convert-HenkaAutomationNumber $terrainViewportMatch.Groups['x'].Value
    $terrainViewportY = Convert-HenkaAutomationNumber $terrainViewportMatch.Groups['y'].Value
    $terrainViewportWidth = Convert-HenkaAutomationNumber $terrainViewportMatch.Groups['width'].Value
    $terrainViewportHeight = Convert-HenkaAutomationNumber $terrainViewportMatch.Groups['height'].Value
    if ($terrainViewportWidth -le 0.0 -or $terrainViewportHeight -le 0.0) {
        throw "The product-reported scene viewport is not large enough for a Terrain edit."
    }
    $terrainCursorPattern = 'HENKA_AUTOMATION_DIAGNOSTIC terrain_cursor frame=(?<frame>\d+) pointer_x=(?<x>[-0-9.]+) pointer_y=(?<y>[-0-9.]+) hit=(?<hit>[01])'
    $terrainProbeFractions = @(
        [pscustomobject]@{ X = 0.75; Y = 0.45 },
        [pscustomobject]@{ X = 0.90; Y = 0.45 },
        [pscustomobject]@{ X = 0.65; Y = 0.60 },
        [pscustomobject]@{ X = 0.85; Y = 0.60 },
        [pscustomobject]@{ X = 0.95; Y = 0.72 },
        [pscustomobject]@{ X = 0.70; Y = 0.78 },
        [pscustomobject]@{ X = 0.50; Y = 0.85 },
        [pscustomobject]@{ X = 0.30; Y = 0.70 },
        [pscustomobject]@{ X = 0.10; Y = 0.60 },
        [pscustomobject]@{ X = 0.50; Y = 0.40 },
        [pscustomobject]@{ X = 0.50; Y = 0.60 },
        [pscustomobject]@{ X = 0.50; Y = 0.80 }
    )
    $terrainHit = $null
    foreach ($probe in $terrainProbeFractions) {
        $probeX = $terrainViewportX + ($terrainViewportWidth * $probe.X)
        $probeY = $terrainViewportY + ($terrainViewportHeight * $probe.Y)
        $probeOffset = Get-FileLengthSafe -Path $stdoutPath
        $moveEvent = 'move {0} {1}' -f `
            (Format-HenkaAutomationFloat -Value $probeX), `
            (Format-HenkaAutomationFloat -Value $probeY)
        Send-HenkaAutomationEvent `
            -EventPath $automationInputPath `
            -EventLine $moveEvent `
            -SettleMilliseconds 180
        if (-not (Wait-FileContainsAfterOffset `
                -Path $stdoutPath `
                -Pattern $terrainCursorPattern `
                -StartingOffset $probeOffset `
                -TimeoutMilliseconds 3000)) {
            throw "The product did not report Terrain ray state after a real viewport move."
        }
        $probeResult = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $terrainCursorPattern
        if ($null -eq $probeResult) {
            throw "The product Terrain ray report could not be parsed after a viewport move."
        }
        $reportedX = Convert-HenkaAutomationNumber $probeResult.Groups['x'].Value
        $reportedY = Convert-HenkaAutomationNumber $probeResult.Groups['y'].Value
        if ([Math]::Abs($reportedX - $probeX) -gt 1.0 -or
            [Math]::Abs($reportedY - $probeY) -gt 1.0) {
            throw "The product Terrain ray report did not correspond to the latest viewport move."
        }
        if ($probeResult.Groups['hit'].Value -eq '1') {
            $terrainHit = $probeResult
            break
        }
    }
    if ($null -eq $terrainHit) {
        throw "The product Terrain ray authority found no resident terrain beneath the bounded viewport probe points."
    }

    $terrainHitX = Convert-HenkaAutomationNumber $terrainHit.Groups['x'].Value
    $terrainHitY = Convert-HenkaAutomationNumber $terrainHit.Groups['y'].Value
    Write-Output ("[pass] Product Terrain ray authority reported resident ground at {0:F1},{1:F1}." -f $terrainHitX, $terrainHitY)
    $terrainEditOffset = Get-FileLengthSafe -Path $stdoutPath
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X $terrainHitX `
        -Y $terrainHitY
    if (-not (Wait-FileContainsAfterOffset `
            -Path $stdoutPath `
            -Pattern 'Terrain raise committed through the shared command API\.' `
            -StartingOffset $terrainEditOffset `
            -TimeoutMilliseconds 10000)) {
        throw "A real Terrain viewport click did not commit a terrain edit before New Scene."
    }
    Write-Output "[pass] The production Terrain brush committed an edit through a real viewport click."

    $sceneActionControls = Get-SceneDocumentActionControls -Path $stdoutPath
    if ($null -eq $sceneActionControls) {
        throw "The product New Scene control was unavailable after the saved scene reloaded."
    }
    $newSceneX = Convert-HenkaAutomationNumber $sceneActionControls.Groups['new_scene_x'].Value
    $newSceneY = Convert-HenkaAutomationNumber $sceneActionControls.Groups['new_scene_y'].Value
    $newSceneWidth = Convert-HenkaAutomationNumber $sceneActionControls.Groups['new_scene_width'].Value
    $newSceneHeight = Convert-HenkaAutomationNumber $sceneActionControls.Groups['new_scene_height'].Value
    if ($newSceneWidth -le 0.0 -or $newSceneHeight -le 0.0 -or
        $newSceneX -lt ($detailsX - 0.5) -or $newSceneY -lt ($detailsY - 0.5) -or
        ($newSceneX + $newSceneWidth) -gt ($detailsX + $detailsWidth + 0.5) -or
        ($newSceneY + $newSceneHeight) -gt ($detailsY + $detailsHeight + 0.5)) {
        throw "The New Scene hit target escaped the visible Object Details panel."
    }

    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($newSceneX + ($newSceneWidth * 0.5)) `
        -Y ($newSceneY + ($newSceneHeight * 0.5))
    $newScenePendingPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=new_scene_confirmation_pending\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $newScenePendingPattern -TimeoutMilliseconds 10000)) {
        throw "The real New Scene control did not request confirmation before replacing the saved scene."
    }
    $newSceneConfirmation = Get-SceneTransitionConfirmationControls -Path $stdoutPath -Action "new"
    if ($null -eq $newSceneConfirmation) {
        throw "New Scene confirmation did not expose its product Cancel/Create Scene controls."
    }
    $cancelX = Convert-HenkaAutomationNumber $newSceneConfirmation.Groups['cancel_x'].Value
    $cancelY = Convert-HenkaAutomationNumber $newSceneConfirmation.Groups['cancel_y'].Value
    $cancelWidth = Convert-HenkaAutomationNumber $newSceneConfirmation.Groups['cancel_width'].Value
    $cancelHeight = Convert-HenkaAutomationNumber $newSceneConfirmation.Groups['cancel_height'].Value
    $confirmX = Convert-HenkaAutomationNumber $newSceneConfirmation.Groups['confirm_x'].Value
    $confirmY = Convert-HenkaAutomationNumber $newSceneConfirmation.Groups['confirm_y'].Value
    $confirmWidth = Convert-HenkaAutomationNumber $newSceneConfirmation.Groups['confirm_width'].Value
    $confirmHeight = Convert-HenkaAutomationNumber $newSceneConfirmation.Groups['confirm_height'].Value
    foreach ($confirmationRect in @(
            [pscustomobject]@{ X = $cancelX; Y = $cancelY; Width = $cancelWidth; Height = $cancelHeight },
            [pscustomobject]@{ X = $confirmX; Y = $confirmY; Width = $confirmWidth; Height = $confirmHeight })) {
        if ($confirmationRect.Width -le 0.0 -or $confirmationRect.Height -le 0.0 -or
            $confirmationRect.X -lt ($detailsX - 0.5) -or $confirmationRect.Y -lt ($detailsY - 0.5) -or
            ($confirmationRect.X + $confirmationRect.Width) -gt ($detailsX + $detailsWidth + 0.5) -or
            ($confirmationRect.Y + $confirmationRect.Height) -gt ($detailsY + $detailsHeight + 0.5)) {
            throw "A New Scene confirmation control escaped the visible Object Details panel."
        }
    }
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($cancelX + ($cancelWidth * 0.5)) `
        -Y ($cancelY + ($cancelHeight * 0.5))
    $newSceneCancelledPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=new_scene_confirmation_cancelled name=' +
        [Regex]::Escape($newSceneDraftName) + ' terrain_active=1\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $newSceneCancelledPattern -TimeoutMilliseconds 10000)) {
        throw "Cancelling New Scene did not preserve the canonical unsaved Scene Name edit."
    }

    $sceneActionControls = Get-SceneDocumentActionControls -Path $stdoutPath
    if ($null -eq $sceneActionControls) {
        throw "The New Scene action row was unavailable after cancelling confirmation."
    }
    $newSceneX = Convert-HenkaAutomationNumber $sceneActionControls.Groups['new_scene_x'].Value
    $newSceneY = Convert-HenkaAutomationNumber $sceneActionControls.Groups['new_scene_y'].Value
    $newSceneWidth = Convert-HenkaAutomationNumber $sceneActionControls.Groups['new_scene_width'].Value
    $newSceneHeight = Convert-HenkaAutomationNumber $sceneActionControls.Groups['new_scene_height'].Value
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($newSceneX + ($newSceneWidth * 0.5)) `
        -Y ($newSceneY + ($newSceneHeight * 0.5))
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $newScenePendingPattern -TimeoutMilliseconds 10000)) {
        throw "A second real New Scene click did not request confirmation."
    }
    $newSceneConfirmation = Get-SceneTransitionConfirmationControls -Path $stdoutPath -Action "new"
    if ($null -eq $newSceneConfirmation) {
        throw "The confirmed New Scene transition did not expose its product controls."
    }
    $confirmX = Convert-HenkaAutomationNumber $newSceneConfirmation.Groups['confirm_x'].Value
    $confirmY = Convert-HenkaAutomationNumber $newSceneConfirmation.Groups['confirm_y'].Value
    $confirmWidth = Convert-HenkaAutomationNumber $newSceneConfirmation.Groups['confirm_width'].Value
    $confirmHeight = Convert-HenkaAutomationNumber $newSceneConfirmation.Groups['confirm_height'].Value
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($confirmX + ($confirmWidth * 0.5)) `
        -Y ($confirmY + ($confirmHeight * 0.5))
    $newSceneCreatedPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=new_scene_created name=Untitled Scene relative_path=[^ ]* relative_path_empty=1 document_object_count=0 scene_entity_count=\d+ camera_present=1 environment_present=1 environment_mode=0 sun_enabled=1 selection_empty=1 terrain_was_active=1 terrain_active=0 audio_system_setter_succeeded=1 audio_asset_manager_setter_succeeded=1\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $newSceneCreatedPattern -TimeoutMilliseconds 15000)) {
        throw "New Scene did not publish an empty unsaved scene with its camera, default environment, and empty selection."
    }
    $newSceneAttemptPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=new_scene_attempt result=(?<result>-?\d+)\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $newSceneAttemptPattern -TimeoutMilliseconds 10000)) {
        throw "The product Create Scene control did not report its transition result."
    }
    $newSceneAttempt = Get-LastLogRegexMatch -Path $stdoutPath -Pattern $newSceneAttemptPattern
    if ($null -eq $newSceneAttempt -or [int]$newSceneAttempt.Groups['result'].Value -ne 0) {
        throw "The product New Scene transaction failed."
    }
    if ((Get-FileHash -LiteralPath $savedScenePath -Algorithm SHA256).Hash -ne $savedSceneHash) {
        throw "New Scene modified the previously saved scene file."
    }

    $emptySceneActionsPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_action_controls visible=1 document_object_count=0 selection_present=0 relative_path_empty=1 '
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $emptySceneActionsPattern -TimeoutMilliseconds 10000)) {
        throw "Scene document actions were unavailable after New Scene cleared the object selection."
    }
    $sceneActionControls = Get-SceneDocumentActionControls -Path $stdoutPath
    if ($null -eq $sceneActionControls) {
        throw "Save/Reload controls were not available for unsaved-scene rejection checks."
    }
    $saveX = Convert-HenkaAutomationNumber $sceneActionControls.Groups['save_x'].Value
    $saveY = Convert-HenkaAutomationNumber $sceneActionControls.Groups['save_y'].Value
    $saveWidth = Convert-HenkaAutomationNumber $sceneActionControls.Groups['save_width'].Value
    $saveHeight = Convert-HenkaAutomationNumber $sceneActionControls.Groups['save_height'].Value
    $reloadX = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_x'].Value
    $reloadY = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_y'].Value
    $reloadWidth = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_width'].Value
    $reloadHeight = Convert-HenkaAutomationNumber $sceneActionControls.Groups['reload_height'].Value
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($saveX + ($saveWidth * 0.5)) `
        -Y ($saveY + ($saveHeight * 0.5))
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern 'action=save_rejected reason=unsaved_scene relative_path_empty=1\.' -TimeoutMilliseconds 10000)) {
        throw "Save Scene did not fail closed for the unsaved scene."
    }
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($reloadX + ($reloadWidth * 0.5)) `
        -Y ($reloadY + ($reloadHeight * 0.5))
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern 'action=reload_rejected reason=no_saved_file relative_path_empty=1\.' -TimeoutMilliseconds 10000)) {
        throw "Reload Scene did not fail closed for the unsaved scene."
    }
    if ((Get-FileHash -LiteralPath $savedScenePath -Algorithm SHA256).Hash -ne $savedSceneHash) {
        throw "Rejected Save/Reload actions changed the prior saved scene file."
    }

    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($saveAsX + ($saveAsWidth * 0.5)) `
        -Y ($saveAsY + ($saveAsHeight * 0.5))
    $saveAsControlsPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_save_as_controls visible=1 .* relative_path_empty=1\.'
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern $saveAsControlsPattern -TimeoutMilliseconds 10000)) {
        throw "Save As did not open its product path editor."
    }
    $saveAsControls = Get-SceneSaveAsControls -Path $stdoutPath
    if ($null -eq $saveAsControls -or
        [int]$saveAsControls.Groups['relative_path_empty'].Value -ne 1) {
        throw "Save As path editor geometry or initial state was unavailable."
    }
    $saveAsOpenScreenshotPath = Join-Path $runtimeDirectory "new-scene-save-as-open.png"
    $null = Save-HenkaProductFramebufferScreenshot `
        -Process $capturedProcess.Process `
        -StdoutPath $stdoutPath `
        -CaptureDirectory $frameCaptureDirectory `
        -RequestFile $frameCaptureRequestPath `
        -Description "Packaged empty-scene Save As form" `
        -DestinationPath $saveAsOpenScreenshotPath
    Write-Output "[pass] Captured the real product framebuffer with Save As open and no selected object. Screenshot: $saveAsOpenScreenshotPath"
    $saveAsRelativePath = 'GoldenPathSaveAs.hscene'
    $saveAsTargetPath = Join-Path $userDataBasePath $saveAsRelativePath
    $projectManifestPath = Join-Path $userDataBasePath 'henka.project'
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ((Convert-HenkaAutomationNumber $saveAsControls.Groups['path_x'].Value) +
            ((Convert-HenkaAutomationNumber $saveAsControls.Groups['path_width'].Value) * 0.5)) `
        -Y ((Convert-HenkaAutomationNumber $saveAsControls.Groups['path_y'].Value) +
            ((Convert-HenkaAutomationNumber $saveAsControls.Groups['path_height'].Value) * 0.5))
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $saveAsRelativePath
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ((Convert-HenkaAutomationNumber $saveAsControls.Groups['save_x'].Value) +
            ((Convert-HenkaAutomationNumber $saveAsControls.Groups['save_width'].Value) * 0.5)) `
        -Y ((Convert-HenkaAutomationNumber $saveAsControls.Groups['save_y'].Value) +
            ((Convert-HenkaAutomationNumber $saveAsControls.Groups['save_height'].Value) * 0.5))
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern 'scene_document_persistence action=saved_as relative_path_empty=0 document_object_count=0\.' -TimeoutMilliseconds 15000)) {
        throw "The real Save As action did not save the empty scene to a project-relative path."
    }
    if (-not (Test-Path -LiteralPath $saveAsTargetPath -PathType Leaf)) {
        throw "The Save As target scene file was not created at the packaged user-data root."
    }
    if (-not (Test-Path -LiteralPath $projectManifestPath -PathType Leaf) -or
        (Get-Content -LiteralPath $projectManifestPath -Raw) -notmatch ('(?m)^startup_scene=' + [Regex]::Escape($saveAsRelativePath) + '$')) {
        throw "Save As did not update the project startup-scene manifest to the selected path."
    }

    if (-not (Wait-FileContains -Path $stdoutPath -Pattern 'scene_document_action_controls visible=1 document_object_count=0 selection_present=0 relative_path_empty=0 ' -TimeoutMilliseconds 10000)) {
        throw "Saved scene actions did not return after Save As completed."
    }
    $savedSceneActions = Get-SceneDocumentActionControls -Path $stdoutPath
    $savedReloadX = Convert-HenkaAutomationNumber $savedSceneActions.Groups['reload_x'].Value
    $savedReloadY = Convert-HenkaAutomationNumber $savedSceneActions.Groups['reload_y'].Value
    $savedReloadWidth = Convert-HenkaAutomationNumber $savedSceneActions.Groups['reload_width'].Value
    $savedReloadHeight = Convert-HenkaAutomationNumber $savedSceneActions.Groups['reload_height'].Value
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ($savedReloadX + ($savedReloadWidth * 0.5)) `
        -Y ($savedReloadY + ($savedReloadHeight * 0.5))
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern 'action=reload_confirmation_pending' -TimeoutMilliseconds 10000)) {
        throw "Reload did not offer confirmation for the newly saved scene."
    }
    $reloadConfirmation = Get-SceneTransitionConfirmationControls `
        -Path $stdoutPath `
        -Action 'reload'
    if ($null -eq $reloadConfirmation) {
        throw "Reload confirmation geometry was unavailable after Save As."
    }
    Send-HenkaAutomationClickAt `
        -EventPath $automationInputPath `
        -X ((Convert-HenkaAutomationNumber $reloadConfirmation.Groups['confirm_x'].Value) +
            ((Convert-HenkaAutomationNumber $reloadConfirmation.Groups['confirm_width'].Value) * 0.5)) `
        -Y ((Convert-HenkaAutomationNumber $reloadConfirmation.Groups['confirm_y'].Value) +
            ((Convert-HenkaAutomationNumber $reloadConfirmation.Groups['confirm_height'].Value) * 0.5))
    if (-not (Wait-FileContains -Path $stdoutPath -Pattern 'scene_document_persistence action=reloaded name=Untitled Scene\.' -TimeoutMilliseconds 15000)) {
        throw "The scene saved through Save As did not reload through the product UI."
    }
    if ((Get-FileHash -LiteralPath $savedScenePath -Algorithm SHA256).Hash -ne $savedSceneHash) {
        throw "Save As or reload modified the independent previously saved scene."
    }
    $newSceneScreenshotPath = Join-Path $runtimeDirectory "new-scene.png"
    $null = Save-HenkaProductFramebufferScreenshot `
        -Process $capturedProcess.Process `
        -StdoutPath $stdoutPath `
        -CaptureDirectory $frameCaptureDirectory `
        -RequestFile $frameCaptureRequestPath `
        -Description "Packaged New Scene editor" `
        -DestinationPath $newSceneScreenshotPath
    Write-Output "[pass] Packaged Save As used the real path field and button, updated the startup manifest, preserved the independent prior scene, and reloaded through the confirmation UI. Screenshot: $newSceneScreenshotPath"

    Send-HenkaSceneObjectsControlClick `
        -EventPath $automationInputPath `
        -StdoutPath $stdoutPath `
        -Id 'asset_name' `
        -AssetOpen $false
    Clear-HenkaTextField -EventPath $automationInputPath
    Send-HenkaAutomationText -EventPath $automationInputPath -Text $assetName
    Send-HenkaSceneObjectsControlClick `
        -EventPath $automationInputPath `
        -StdoutPath $stdoutPath `
        -Id 'new_asset' `
        -AssetOpen $false
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "created" -PartCount 0

    Send-HenkaSceneObjectsControlClick `
        -EventPath $automationInputPath `
        -StdoutPath $stdoutPath `
        -Id 'add_cube' `
        -AssetOpen $true
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "part-added" -PartCount 1
    Send-HenkaSceneObjectsControlClick `
        -EventPath $automationInputPath `
        -StdoutPath $stdoutPath `
        -Id 'cylinder' `
        -AssetOpen $true
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "part-added" -PartCount 2
    Send-HenkaSceneObjectsControlClick `
        -EventPath $automationInputPath `
        -StdoutPath $stdoutPath `
        -Id 'cone' `
        -AssetOpen $true
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "part-added" -PartCount 3
    Send-HenkaSceneObjectsControlClick `
        -EventPath $automationInputPath `
        -StdoutPath $stdoutPath `
        -Id 'uv_sphere' `
        -AssetOpen $true
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "part-added" -PartCount 4

    Send-HenkaSceneObjectsControlClick `
        -EventPath $automationInputPath `
        -StdoutPath $stdoutPath `
        -Id 'save_asset' `
        -AssetOpen $true
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "saved" -PartCount 4
    Assert-HenkaManifest -Path (Join-Path $runtimeDirectory ("user\saves\" + $assetName + ".asset")) -Name $assetName

    Send-HenkaSceneObjectsControlClick `
        -EventPath $automationInputPath `
        -StdoutPath $stdoutPath `
        -Id 'close_asset' `
        -AssetOpen $true
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "closed" -PartCount 4
    Send-HenkaSceneObjectsControlClick `
        -EventPath $automationInputPath `
        -StdoutPath $stdoutPath `
        -Id 'open_asset' `
        -AssetOpen $false
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "opened" -PartCount 4

    Send-HenkaSceneObjectsControlClick -EventPath $automationInputPath -StdoutPath $stdoutPath -Id 'close_asset' -AssetOpen $true
    Assert-HenkaDocumentTelemetry -Path $stdoutPath -Name $assetName -Action "closed" -PartCount 4
    $projectManifestPath = Join-Path $userDataBasePath 'henka.project'
    Invoke-HenkaPackagedSceneOpenWorkflow -Context @{
        Process = $capturedProcess.Process
        AutomationInputPath = $automationInputPath
        StdoutPath = $stdoutPath
        FrameCaptureDirectory = $frameCaptureDirectory
        FrameCaptureRequestPath = $frameCaptureRequestPath
        RuntimeDirectory = $runtimeDirectory
        ExpectedCurrentSceneName = 'Untitled Scene'
        SceneName = $sceneName
        SavedScenePath = $savedScenePath
        SavedSceneHash = $savedSceneHash
        ProjectManifestPath = $projectManifestPath
        ProjectManifestHash = (Get-FileHash -LiteralPath $projectManifestPath -Algorithm SHA256).Hash
    }

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
    if ($null -eq $previousAutomationUserDataBasePath) {
        Remove-Item Env:HENKA_AUTOMATION_USER_DATA_BASE_PATH -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_USER_DATA_BASE_PATH = $previousAutomationUserDataBasePath
    }
    if ($null -eq $previousAutomationDiagnostics) {
        Remove-Item Env:HENKA_AUTOMATION_DIAGNOSTICS -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_DIAGNOSTICS = $previousAutomationDiagnostics
    }
    if ($null -eq $previousAutomationStartHidden) {
        Remove-Item Env:HENKA_AUTOMATION_START_HIDDEN -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_START_HIDDEN = $previousAutomationStartHidden
    }
    if ($null -eq $previousToolWindowsStartHidden) {
        Remove-Item Env:HENKA_AUTOMATION_TOOL_WINDOWS_START_HIDDEN -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_TOOL_WINDOWS_START_HIDDEN = $previousToolWindowsStartHidden
    }
    if ($null -eq $previousAutomationCaptureOutputDirectory) {
        Remove-Item Env:HENKA_AUTOMATION_CAPTURE_OUTPUT_DIRECTORY -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_CAPTURE_OUTPUT_DIRECTORY = $previousAutomationCaptureOutputDirectory
    }
    if ($null -eq $previousAutomationCaptureRequestFile) {
        Remove-Item Env:HENKA_AUTOMATION_CAPTURE_REQUEST_FILE -ErrorAction SilentlyContinue
    }
    else {
        $env:HENKA_AUTOMATION_CAPTURE_REQUEST_FILE = $previousAutomationCaptureRequestFile
    }
    if ($null -ne $capturedProcess) {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
    }
}
