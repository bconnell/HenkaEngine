function Invoke-HenkaPackagedSceneOpenWorkflow {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][hashtable]$Context)

    $actions = Get-SceneDocumentActionControls -Path $Context.StdoutPath
    $panelMatch = Get-DetailsPanelGeometry -Path $Context.StdoutPath
    if ($null -eq $actions -or $null -eq $panelMatch) {
        throw "The product did not report Open Scene and Object Details geometry."
    }
    $panel = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $panelMatch.Groups['x'].Value
        Y = Convert-HenkaAutomationNumber $panelMatch.Groups['y'].Value
        Width = Convert-HenkaAutomationNumber $panelMatch.Groups['width'].Value
        Height = Convert-HenkaAutomationNumber $panelMatch.Groups['height'].Value
    }
    $openButton = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $actions.Groups['open_scene_x'].Value
        Y = Convert-HenkaAutomationNumber $actions.Groups['open_scene_y'].Value
        Width = Convert-HenkaAutomationNumber $actions.Groups['open_scene_width'].Value
        Height = Convert-HenkaAutomationNumber $actions.Groups['open_scene_height'].Value
    }
    if ($openButton.Width -le 0.0 -or $openButton.Height -le 0.0 -or
        $openButton.X -lt ($panel.X - 0.5) -or $openButton.Y -lt ($panel.Y - 0.5) -or
        ($openButton.X + $openButton.Width) -gt ($panel.X + $panel.Width + 0.5) -or
        ($openButton.Y + $openButton.Height) -gt ($panel.Y + $panel.Height + 0.5)) {
        throw "Open Scene is not wholly inside the visible Object Details panel."
    }
    $openFormOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($openButton.X + ($openButton.Width * 0.5)) -Y ($openButton.Y + ($openButton.Height * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'scene_open_controls visible=1 .* relative_path_empty=1\.' -StartingOffset $openFormOffset -TimeoutMilliseconds 10000)) {
        throw "The real Open Scene control did not open its project-relative path editor."
    }
    $pathControls = Get-SceneOpenControls -Path $Context.StdoutPath
    if ($null -eq $pathControls -or [int]$pathControls.Groups['relative_path_empty'].Value -ne 1) {
        throw "The Open Scene path field, Open, or Cancel control was not reported."
    }
    $pathRects = @(
        [pscustomobject]@{ X = Convert-HenkaAutomationNumber $pathControls.Groups['path_x'].Value; Y = Convert-HenkaAutomationNumber $pathControls.Groups['path_y'].Value; Width = Convert-HenkaAutomationNumber $pathControls.Groups['path_width'].Value; Height = Convert-HenkaAutomationNumber $pathControls.Groups['path_height'].Value },
        [pscustomobject]@{ X = Convert-HenkaAutomationNumber $pathControls.Groups['open_x'].Value; Y = Convert-HenkaAutomationNumber $pathControls.Groups['open_y'].Value; Width = Convert-HenkaAutomationNumber $pathControls.Groups['open_width'].Value; Height = Convert-HenkaAutomationNumber $pathControls.Groups['open_height'].Value },
        [pscustomobject]@{ X = Convert-HenkaAutomationNumber $pathControls.Groups['cancel_x'].Value; Y = Convert-HenkaAutomationNumber $pathControls.Groups['cancel_y'].Value; Width = Convert-HenkaAutomationNumber $pathControls.Groups['cancel_width'].Value; Height = Convert-HenkaAutomationNumber $pathControls.Groups['cancel_height'].Value })
    foreach ($rect in $pathRects) {
        if ($rect.Width -le 0.0 -or $rect.Height -le 0.0 -or $rect.X -lt ($panel.X - 0.5) -or $rect.Y -lt ($panel.Y - 0.5) -or ($rect.X + $rect.Width) -gt ($panel.X + $panel.Width + 0.5) -or ($rect.Y + $rect.Height) -gt ($panel.Y + $panel.Height + 0.5)) {
            throw "An Open Scene path-editor control escaped visible Object Details."
        }
    }

    $formScreenshot = Join-Path $Context.RuntimeDirectory "open-scene-form.png"
    $null = Save-HenkaProductFramebufferScreenshot -Process $Context.Process -StdoutPath $Context.StdoutPath -CaptureDirectory $Context.FrameCaptureDirectory -RequestFile $Context.FrameCaptureRequestPath -Description "Packaged Open Scene path form" -DestinationPath $formScreenshot
    $openRelativePath = 'sandbox3d_scene.hscene'
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($pathRects[0].X + ($pathRects[0].Width * 0.5)) -Y ($pathRects[0].Y + ($pathRects[0].Height * 0.5))
    Clear-HenkaTextField -EventPath $Context.AutomationInputPath
    $pathTextOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationText -EventPath $Context.AutomationInputPath -Text $openRelativePath
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'input record=\d+ type=text' -StartingOffset $pathTextOffset -TimeoutMilliseconds 10000)) {
        throw "The Open Scene path field did not consume the typed project-relative path."
    }
    $pendingPattern = 'open_scene_confirmation_pending relative_path=' + [Regex]::Escape($openRelativePath) + '\.'
    $pendingOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($pathRects[1].X + ($pathRects[1].Width * 0.5)) -Y ($pathRects[1].Y + ($pathRects[1].Height * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $pendingPattern -StartingOffset $pendingOffset -TimeoutMilliseconds 10000) -or
        -not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'scene_transition_confirmation action=open available=1' -StartingOffset $pendingOffset -TimeoutMilliseconds 10000)) {
        throw "The Open button did not request confirmation for the entered scene."
    }
    $confirmation = Get-SceneTransitionConfirmationControls -Path $Context.StdoutPath -Action 'open'
    if ($null -eq $confirmation) {
        throw "Open Scene confirmation did not expose product-owned Cancel and Open hit regions."
    }
    $cancel = [pscustomobject]@{ X = Convert-HenkaAutomationNumber $confirmation.Groups['cancel_x'].Value; Y = Convert-HenkaAutomationNumber $confirmation.Groups['cancel_y'].Value; Width = Convert-HenkaAutomationNumber $confirmation.Groups['cancel_width'].Value; Height = Convert-HenkaAutomationNumber $confirmation.Groups['cancel_height'].Value }
    $confirm = [pscustomobject]@{ X = Convert-HenkaAutomationNumber $confirmation.Groups['confirm_x'].Value; Y = Convert-HenkaAutomationNumber $confirmation.Groups['confirm_y'].Value; Width = Convert-HenkaAutomationNumber $confirmation.Groups['confirm_width'].Value; Height = Convert-HenkaAutomationNumber $confirmation.Groups['confirm_height'].Value }
    foreach ($rect in @($cancel, $confirm)) {
        if ($rect.Width -le 0.0 -or $rect.Height -le 0.0 -or $rect.X -lt ($panel.X - 0.5) -or $rect.Y -lt ($panel.Y - 0.5) -or ($rect.X + $rect.Width) -gt ($panel.X + $panel.Width + 0.5) -or ($rect.Y + $rect.Height) -gt ($panel.Y + $panel.Height + 0.5)) {
            throw "An Open Scene confirmation hit region escaped visible Object Details."
        }
    }
    $cancelledPattern = 'open_scene_confirmation_cancelled name=' + [Regex]::Escape($Context.ExpectedCurrentSceneName) + ' terrain_active=0\.'
    $cancelOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($cancel.X + ($cancel.Width * 0.5)) -Y ($cancel.Y + ($cancel.Height * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $cancelledPattern -StartingOffset $cancelOffset -TimeoutMilliseconds 10000)) {
        throw "Canceling Open Scene did not retain the current scene."
    }
    $retryOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($pathRects[1].X + ($pathRects[1].Width * 0.5)) -Y ($pathRects[1].Y + ($pathRects[1].Height * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $pendingPattern -StartingOffset $retryOffset -TimeoutMilliseconds 10000) -or
        -not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'scene_transition_confirmation action=open available=1' -StartingOffset $retryOffset -TimeoutMilliseconds 10000)) {
        throw "Canceling confirmation did not retain the entered path for retry."
    }
    $confirmation = Get-SceneTransitionConfirmationControls -Path $Context.StdoutPath -Action 'open'
    if ($null -eq $confirmation) {
        throw "The retried Open Scene confirmation did not expose its controls."
    }
    $confirmX = Convert-HenkaAutomationNumber $confirmation.Groups['confirm_x'].Value
    $confirmY = Convert-HenkaAutomationNumber $confirmation.Groups['confirm_y'].Value
    $confirmWidth = Convert-HenkaAutomationNumber $confirmation.Groups['confirm_width'].Value
    $confirmHeight = Convert-HenkaAutomationNumber $confirmation.Groups['confirm_height'].Value
    $attemptPattern = 'scene_document_persistence action=scene_open_attempt result=(?<result>-?\d+) relative_path_empty=0 active_name=' + [Regex]::Escape($Context.SceneName) + ' active_relative_path=' + [Regex]::Escape($openRelativePath) + ' active_document_object_count=(?<count>\d+) selection_present=(?<selection>[01])\.'
    $openAttemptOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($confirmX + ($confirmWidth * 0.5)) -Y ($confirmY + ($confirmHeight * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $attemptPattern -StartingOffset $openAttemptOffset -TimeoutMilliseconds 15000)) {
        throw "Open Scene did not publish the authoritative active document state."
    }
    $openedAttempt = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $attemptPattern
    if ($null -eq $openedAttempt -or [int]$openedAttempt.Groups['result'].Value -ne 0 -or [int]$openedAttempt.Groups['count'].Value -le 0) {
        throw "Open Scene did not load the previously saved named scene as a nonempty document."
    }
    $openedPattern = 'scene_document_persistence action=scene_opened name=' + [Regex]::Escape($Context.SceneName) + ' relative_path=' + [Regex]::Escape($openRelativePath) + ' relative_path_empty=0 document_object_count=(?<count>\d+)'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $openedPattern -StartingOffset $openAttemptOffset -TimeoutMilliseconds 10000)) {
        throw "Open Scene did not report the canonical loaded scene identity and path."
    }
    $openedDocument = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $openedPattern
    if ($null -eq $openedDocument -or [int]$openedDocument.Groups['count'].Value -ne [int]$openedAttempt.Groups['count'].Value) {
        throw "The loaded document count disagreed with the active document count."
    }
    if ((Get-FileHash -LiteralPath $Context.SavedScenePath -Algorithm SHA256).Hash -ne $Context.SavedSceneHash -or (Get-FileHash -LiteralPath $Context.ProjectManifestPath -Algorithm SHA256).Hash -ne $Context.ProjectManifestHash) {
        throw "Opening the explicit scene changed its source file or project startup manifest."
    }
    $openedScreenshot = Join-Path $Context.RuntimeDirectory "open-scene-loaded.png"
    $null = Save-HenkaProductFramebufferScreenshot -Process $Context.Process -StdoutPath $Context.StdoutPath -CaptureDirectory $Context.FrameCaptureDirectory -RequestFile $Context.FrameCaptureRequestPath -Description "Packaged Open Scene loaded document" -DestinationPath $openedScreenshot

    $loadedCount = [int]$openedAttempt.Groups['count'].Value
    $loadedSelection = $openedAttempt.Groups['selection'].Value
    $actionsPattern = 'scene_document_action_controls visible=1 document_object_count=' + $loadedCount + ' selection_present=' + [Regex]::Escape($loadedSelection) + ' relative_path_empty=0 .* open_scene_rect='
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $actionsPattern -StartingOffset $openAttemptOffset -TimeoutMilliseconds 10000)) {
        throw "Open Scene did not restore the product scene-action controls."
    }
    $actions = Get-SceneDocumentActionControls -Path $Context.StdoutPath
    $panelMatch = Get-DetailsPanelGeometry -Path $Context.StdoutPath
    if ($null -eq $actions -or $null -eq $panelMatch) {
        throw "Open Scene did not restore its product-owned action geometry."
    }
    $panel = [pscustomobject]@{ X = Convert-HenkaAutomationNumber $panelMatch.Groups['x'].Value; Y = Convert-HenkaAutomationNumber $panelMatch.Groups['y'].Value; Width = Convert-HenkaAutomationNumber $panelMatch.Groups['width'].Value; Height = Convert-HenkaAutomationNumber $panelMatch.Groups['height'].Value }
    $openButton = [pscustomobject]@{ X = Convert-HenkaAutomationNumber $actions.Groups['open_scene_x'].Value; Y = Convert-HenkaAutomationNumber $actions.Groups['open_scene_y'].Value; Width = Convert-HenkaAutomationNumber $actions.Groups['open_scene_width'].Value; Height = Convert-HenkaAutomationNumber $actions.Groups['open_scene_height'].Value }
    if ($openButton.Width -le 0.0 -or $openButton.Height -le 0.0 -or $openButton.X -lt ($panel.X - 0.5) -or $openButton.Y -lt ($panel.Y - 0.5) -or ($openButton.X + $openButton.Width) -gt ($panel.X + $panel.Width + 0.5) -or ($openButton.Y + $openButton.Height) -gt ($panel.Y + $panel.Height + 0.5)) {
        throw "The restored Open Scene hit region escaped visible Object Details."
    }
    $invalidFormOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($openButton.X + ($openButton.Width * 0.5)) -Y ($openButton.Y + ($openButton.Height * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'scene_open_controls visible=1 .* relative_path_empty=1\.' -StartingOffset $invalidFormOffset -TimeoutMilliseconds 10000)) {
        throw "Open Scene did not reopen for the invalid-path negative control."
    }
    $pathControls = Get-SceneOpenControls -Path $Context.StdoutPath
    if ($null -eq $pathControls) {
        throw "The invalid-path form did not expose the product path controls."
    }
    $pathRect = [pscustomobject]@{ X = Convert-HenkaAutomationNumber $pathControls.Groups['path_x'].Value; Y = Convert-HenkaAutomationNumber $pathControls.Groups['path_y'].Value; Width = Convert-HenkaAutomationNumber $pathControls.Groups['path_width'].Value; Height = Convert-HenkaAutomationNumber $pathControls.Groups['path_height'].Value }
    $openRect = [pscustomobject]@{ X = Convert-HenkaAutomationNumber $pathControls.Groups['open_x'].Value; Y = Convert-HenkaAutomationNumber $pathControls.Groups['open_y'].Value; Width = Convert-HenkaAutomationNumber $pathControls.Groups['open_width'].Value; Height = Convert-HenkaAutomationNumber $pathControls.Groups['open_height'].Value }
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($pathRect.X + ($pathRect.Width * 0.5)) -Y ($pathRect.Y + ($pathRect.Height * 0.5))
    Clear-HenkaTextField -EventPath $Context.AutomationInputPath
    $invalidPath = '../escape.hscene'
    $invalidPendingPattern = 'open_scene_confirmation_pending relative_path=' + [Regex]::Escape($invalidPath) + '\.'
    $invalidTextOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationText -EventPath $Context.AutomationInputPath -Text $invalidPath
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'input record=\d+ type=text' -StartingOffset $invalidTextOffset -TimeoutMilliseconds 10000)) {
        throw "The invalid Open Scene path text was not consumed by the product."
    }
    $invalidPendingOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($openRect.X + ($openRect.Width * 0.5)) -Y ($openRect.Y + ($openRect.Height * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $invalidPendingPattern -StartingOffset $invalidPendingOffset -TimeoutMilliseconds 10000) -or
        -not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'scene_transition_confirmation action=open available=1' -StartingOffset $invalidPendingOffset -TimeoutMilliseconds 10000)) {
        throw "The path editor did not pass the invalid path unchanged to confirmation."
    }
    $confirmation = Get-SceneTransitionConfirmationControls -Path $Context.StdoutPath -Action 'open'
    if ($null -eq $confirmation) {
        throw "The invalid path request did not expose the real confirmation controls."
    }
    $confirmX = Convert-HenkaAutomationNumber $confirmation.Groups['confirm_x'].Value
    $confirmY = Convert-HenkaAutomationNumber $confirmation.Groups['confirm_y'].Value
    $confirmWidth = Convert-HenkaAutomationNumber $confirmation.Groups['confirm_width'].Value
    $confirmHeight = Convert-HenkaAutomationNumber $confirmation.Groups['confirm_height'].Value
    $rejectedPattern = 'scene_document_persistence action=scene_open_attempt result=(?<result>-?\d+) relative_path_empty=0 active_name=' + [Regex]::Escape($Context.SceneName) + ' active_relative_path=' + [Regex]::Escape($openRelativePath) + ' active_document_object_count=(?<count>\d+) selection_present=(?<selection>[01])\.'
    $rejectedOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($confirmX + ($confirmWidth * 0.5)) -Y ($confirmY + ($confirmHeight * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $rejectedPattern -StartingOffset $rejectedOffset -TimeoutMilliseconds 15000)) {
        throw "Invalid Open Scene did not report the retained authoritative document state."
    }
    $rejected = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $rejectedPattern
    if ($null -eq $rejected -or [int]$rejected.Groups['result'].Value -eq 0 -or [int]$rejected.Groups['count'].Value -ne $loadedCount -or $rejected.Groups['selection'].Value -ne $loadedSelection) {
        throw "The invalid Open Scene candidate partially changed the live document or selection."
    }
    if ((Get-FileHash -LiteralPath $Context.SavedScenePath -Algorithm SHA256).Hash -ne $Context.SavedSceneHash -or (Get-FileHash -LiteralPath $Context.ProjectManifestPath -Algorithm SHA256).Hash -ne $Context.ProjectManifestHash) {
        throw "Rejected Open Scene changed the source file or project startup manifest."
    }
    $failureScreenshot = Join-Path $Context.RuntimeDirectory "open-scene-rejected.png"
    $null = Save-HenkaProductFramebufferScreenshot -Process $Context.Process -StdoutPath $Context.StdoutPath -CaptureDirectory $Context.FrameCaptureDirectory -RequestFile $Context.FrameCaptureRequestPath -Description "Packaged rejected Open Scene path" -DestinationPath $failureScreenshot
    Write-Output "[pass] Packaged Open Scene used the real path editor and confirmation; cancel retained the pending path, the named saved scene loaded with $loadedCount authored objects without changing the startup manifest, and an escaping path was rejected without changing the live document, selection, or files."
    Write-Output "[pass] Open Scene visual evidence: $formScreenshot, $openedScreenshot, $failureScreenshot"
}
