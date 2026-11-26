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

    $rowPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_authority entity=(?<entity>\d+) name=(?:Ground|New Cube) .*? row_x=(?<x>-?(?:\d+(?:\.\d*)?|\.\d+)) row_y=(?<y>-?(?:\d+(?:\.\d*)?|\.\d+)) row_width=(?<width>-?(?:\d+(?:\.\d*)?|\.\d+)) row_height=(?<height>-?(?:\d+(?:\.\d*)?|\.\d+)) .*? clicked=[01]\.$'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $rowPattern -StartingOffset $openAttemptOffset -TimeoutMilliseconds 15000)) {
        throw "The opened scene did not report a real Scene Objects row for selection proof."
    }
    $row = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $rowPattern
    if ($null -eq $row) {
        throw "The opened scene's product-owned Scene Objects row geometry could not be read."
    }
    $rowEntity = [ulong]::Parse($row.Groups['entity'].Value)
    $rowX = Convert-HenkaAutomationNumber $row.Groups['x'].Value
    $rowY = Convert-HenkaAutomationNumber $row.Groups['y'].Value
    $rowWidth = Convert-HenkaAutomationNumber $row.Groups['width'].Value
    $rowHeight = Convert-HenkaAutomationNumber $row.Groups['height'].Value
    if ($rowWidth -le 0.0 -or $rowHeight -le 0.0) {
        throw "The product-reported Scene Objects row has no usable hit area."
    }
    $selectionOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($rowX + ($rowWidth * 0.5)) -Y ($rowY + ($rowHeight * 0.5))
    $selectionPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_click entity=' +
        $rowEntity + ' name=.* hidden=[01] selected_entity=' + $rowEntity + ' selected=1\.'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $selectionPattern -StartingOffset $selectionOffset -TimeoutMilliseconds 10000)) {
        throw "A real Scene Objects row click did not select its canonical opened-scene entity."
    }

    # Ground is the scene's environmental surface. Exercise the post-open
    # action context with a product-authored finite mesh, then select it through
    # its actual Scene Objects row before invoking Focus Camera.
    $addCubePattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_control id=add_cube available=1 asset_open=0 rect=(?<x>-?(?:\d+(?:\.\d*)?|\.\d+)),(?<y>-?(?:\d+(?:\.\d*)?|\.\d+)),(?<width>-?(?:\d+(?:\.\d*)?|\.\d+)),(?<height>-?(?:\d+(?:\.\d*)?|\.\d+))\.'
    if (-not (Wait-FileContainsAfterOffset `
            -Path $Context.StdoutPath `
            -Pattern $addCubePattern `
            -StartingOffset $openAttemptOffset `
            -TimeoutMilliseconds 10000)) {
        throw "The opened scene did not expose the real Add Cube control."
    }
    $addCube = Get-LastLogRegexMatch `
        -Path $Context.StdoutPath `
        -Pattern $addCubePattern
    $sceneObjectsMatch = Get-SceneObjectsGeometry -Path $Context.StdoutPath
    if ($null -eq $addCube -or $null -eq $sceneObjectsMatch) {
        throw "The product-owned Add Cube control or Scene Objects panel geometry was unavailable."
    }
    $addCubeX = Convert-HenkaAutomationNumber $addCube.Groups['x'].Value
    $addCubeY = Convert-HenkaAutomationNumber $addCube.Groups['y'].Value
    $addCubeWidth = Convert-HenkaAutomationNumber $addCube.Groups['width'].Value
    $addCubeHeight = Convert-HenkaAutomationNumber $addCube.Groups['height'].Value
    $sceneObjectsX = Convert-HenkaAutomationNumber $sceneObjectsMatch.Groups['x'].Value
    $sceneObjectsY = Convert-HenkaAutomationNumber $sceneObjectsMatch.Groups['y'].Value
    $sceneObjectsWidth = Convert-HenkaAutomationNumber $sceneObjectsMatch.Groups['width'].Value
    $sceneObjectsHeight = Convert-HenkaAutomationNumber $sceneObjectsMatch.Groups['height'].Value
    if ($addCubeWidth -le 0.0 -or $addCubeHeight -le 0.0 -or
        $addCubeX -lt ($sceneObjectsX - 0.5) -or $addCubeY -lt ($sceneObjectsY - 0.5) -or
        ($addCubeX + $addCubeWidth) -gt ($sceneObjectsX + $sceneObjectsWidth + 0.5) -or
        ($addCubeY + $addCubeHeight) -gt ($sceneObjectsY + $sceneObjectsHeight + 0.5)) {
        throw "The product-reported Add Cube hit area escaped the visible Scene Objects panel."
    }
    $addCubeOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt `
        -EventPath $Context.AutomationInputPath `
        -X ($addCubeX + ($addCubeWidth * 0.5)) `
        -Y ($addCubeY + ($addCubeHeight * 0.5))
    $newSelectedRowPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row entity=(?<entity>\d+) name=(?<name>.+?) display=.*? lines=\d+ hidden=0 selected=1 x=(?<x>-?(?:\d+(?:\.\d*)?|\.\d+)) y=(?<y>-?(?:\d+(?:\.\d*)?|\.\d+)) width=(?<width>-?(?:\d+(?:\.\d*)?|\.\d+)) height=(?<height>-?(?:\d+(?:\.\d*)?|\.\d+))\.$'
    if (-not (Wait-FileContainsAfterOffset `
            -Path $Context.StdoutPath `
            -Pattern $newSelectedRowPattern `
            -StartingOffset $addCubeOffset `
            -TimeoutMilliseconds 10000)) {
        throw "The real Add Cube action did not publish a selected canonical Scene Objects row."
    }
    $newSelectedRow = Get-LastLogRegexMatch `
        -Path $Context.StdoutPath `
        -Pattern $newSelectedRowPattern
    if ($null -eq $newSelectedRow) {
        throw "The product-created cube's selected Scene Objects row could not be read."
    }
    $cameraTargetEntity = [ulong]::Parse($newSelectedRow.Groups['entity'].Value)
    $cameraTargetName = $newSelectedRow.Groups['name'].Value
    $cameraTargetRowX = Convert-HenkaAutomationNumber $newSelectedRow.Groups['x'].Value
    $cameraTargetRowY = Convert-HenkaAutomationNumber $newSelectedRow.Groups['y'].Value
    $cameraTargetRowWidth = Convert-HenkaAutomationNumber $newSelectedRow.Groups['width'].Value
    $cameraTargetRowHeight = Convert-HenkaAutomationNumber $newSelectedRow.Groups['height'].Value
    if ($cameraTargetEntity -eq $rowEntity -or $cameraTargetName -eq 'Ground' -or
        $cameraTargetRowWidth -le 0.0 -or $cameraTargetRowHeight -le 0.0) {
        throw "Add Cube did not create a distinct selectable mesh row for camera-focus proof."
    }
    $cubeSelectionOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt `
        -EventPath $Context.AutomationInputPath `
        -X ($cameraTargetRowX + ($cameraTargetRowWidth * 0.5)) `
        -Y ($cameraTargetRowY + ($cameraTargetRowHeight * 0.5))
    $cubeSelectionPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_click entity=' +
        $cameraTargetEntity + ' name=' + [Regex]::Escape($cameraTargetName) +
        ' hidden=0 selected_entity=' + $cameraTargetEntity + ' selected=1\.'
    if (-not (Wait-FileContainsAfterOffset `
            -Path $Context.StdoutPath `
            -Pattern $cubeSelectionPattern `
            -StartingOffset $cubeSelectionOffset `
            -TimeoutMilliseconds 10000)) {
        throw "A real Scene Objects row click did not select the newly created cube entity."
    }
    $rowEntity = $cameraTargetEntity
    $selectionOffset = $cubeSelectionOffset

    $actionsDisclosurePattern = '^HENKA_AUTOMATION_DIAGNOSTIC object_details_disclosure id=actions entity=' +
        $rowEntity + ' x=(?<x>-?(?:\d+(?:\.\d*)?|\.\d+)) y=(?<y>-?(?:\d+(?:\.\d*)?|\.\d+)) width=(?<width>-?(?:\d+(?:\.\d*)?|\.\d+)) height=(?<height>-?(?:\d+(?:\.\d*)?|\.\d+)) expanded=(?<expanded>[01])\.'
    $detailsScrollPattern = 'HENKA_AUTOMATION_DIAGNOSTIC details-scroll frame=\d+ point=\((?<pointX>[-0-9.]+),(?<pointY>[-0-9.]+)\) content=(?<content>[-0-9.]+) viewport=(?<viewport>[-0-9.]+) delta=(?<delta>[-0-9.]+) offset=(?<before>[-0-9.]+)->(?<after>[-0-9.]+) handled=(?<handled>[01]) authored=(?<authored>[01]) document=\d+'
    $actionsDisclosure = $null
    for ($actionsScrollAttempt = 0;
         $actionsScrollAttempt -lt 12 -and $null -eq $actionsDisclosure;
         ++$actionsScrollAttempt) {
        if (Wait-FileContainsAfterOffset `
                -Path $Context.StdoutPath `
                -Pattern $actionsDisclosurePattern `
                -StartingOffset $selectionOffset `
                -TimeoutMilliseconds 300) {
            $actionsDisclosure = Get-LastLogRegexMatch `
                -Path $Context.StdoutPath `
                -Pattern $actionsDisclosurePattern
            if ($null -ne $actionsDisclosure) {
                break
            }
        }

        $detailsMatch = Get-DetailsPanelGeometry -Path $Context.StdoutPath
        if ($null -eq $detailsMatch) {
            throw "The selected object's visible Object Details viewport was unavailable for normal scroll navigation."
        }
        $detailsX = Convert-HenkaAutomationNumber $detailsMatch.Groups['x'].Value
        $detailsY = Convert-HenkaAutomationNumber $detailsMatch.Groups['y'].Value
        $detailsWidth = Convert-HenkaAutomationNumber $detailsMatch.Groups['width'].Value
        $detailsHeight = Convert-HenkaAutomationNumber $detailsMatch.Groups['height'].Value
        if ($detailsWidth -le 0.0 -or $detailsHeight -le 0.0) {
            throw "The selected object's Object Details viewport has no usable scroll area."
        }

        $scrollOffset = Get-FileLengthSafe -Path $Context.StdoutPath
        Send-HenkaAutomationScroll `
            -EventPath $Context.AutomationInputPath `
            -X ($detailsX + ($detailsWidth * 0.5)) `
            -Y ($detailsY + ($detailsHeight * 0.55)) `
            -WheelDelta -1.0
        if (-not (Wait-FileContainsAfterOffset `
                -Path $Context.StdoutPath `
                -Pattern $detailsScrollPattern `
                -StartingOffset $scrollOffset `
                -TimeoutMilliseconds 5000)) {
            throw "The product did not report consumption of the Object Details scroll used to reach Actions."
        }
        $scroll = Get-LastLogRegexMatch `
            -Path $Context.StdoutPath `
            -Pattern $detailsScrollPattern
        if ($null -eq $scroll -or $scroll.Groups['handled'].Value -ne '1') {
            throw "The visible Object Details region did not own the scroll input needed to reach Actions."
        }
        $offsetBefore = Convert-HenkaAutomationNumber $scroll.Groups['before'].Value
        $offsetAfter = Convert-HenkaAutomationNumber $scroll.Groups['after'].Value
        if ([Math]::Abs($offsetAfter - $offsetBefore) -lt 0.01) {
            throw "Object Details consumed the scroll input but its content offset did not progress toward the Actions controls."
        }
    }
    if ($null -eq $actionsDisclosure) {
        throw "Bounded normal Object Details scrolling did not expose the selected object's Actions disclosure."
    }
    $focusControlStartOffset = $selectionOffset
    if ($actionsDisclosure.Groups['expanded'].Value -ne '1') {
        $actionsX = Convert-HenkaAutomationNumber $actionsDisclosure.Groups['x'].Value
        $actionsY = Convert-HenkaAutomationNumber $actionsDisclosure.Groups['y'].Value
        $actionsWidth = Convert-HenkaAutomationNumber $actionsDisclosure.Groups['width'].Value
        $actionsHeight = Convert-HenkaAutomationNumber $actionsDisclosure.Groups['height'].Value
        if ($actionsWidth -le 0.0 -or $actionsHeight -le 0.0) {
            throw "The product-reported Actions disclosure has no usable hit area."
        }
        $actionsDisclosureOffset = Get-FileLengthSafe -Path $Context.StdoutPath
        Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($actionsX + ($actionsWidth * 0.5)) -Y ($actionsY + ($actionsHeight * 0.5))
        $expandedActionsPattern = '^HENKA_AUTOMATION_DIAGNOSTIC object_details_disclosure id=actions entity=' +
            $rowEntity + ' x=.* expanded=1\.'
        if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $expandedActionsPattern -StartingOffset $actionsDisclosureOffset -TimeoutMilliseconds 10000)) {
            throw "Clicking the real Actions disclosure did not expand the selected object's action controls."
        }
        $focusControlStartOffset = $actionsDisclosureOffset
    }

    $focusControlPattern = '^HENKA_AUTOMATION_DIAGNOSTIC object_details_focus_camera_control entity=' +
        $rowEntity + ' label=Focus_Camera x=(?<x>-?(?:\d+(?:\.\d*)?|\.\d+)) y=(?<y>-?(?:\d+(?:\.\d*)?|\.\d+)) width=(?<width>-?(?:\d+(?:\.\d*)?|\.\d+)) height=(?<height>-?(?:\d+(?:\.\d*)?|\.\d+))\.'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $focusControlPattern -StartingOffset $focusControlStartOffset -TimeoutMilliseconds 10000)) {
        throw "Selecting the opened-scene entity did not expose the real Focus Camera control."
    }
    $focusControl = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $focusControlPattern
    $detailsMatch = Get-DetailsPanelGeometry -Path $Context.StdoutPath
    if ($null -eq $focusControl -or $null -eq $detailsMatch) {
        throw "Focus Camera geometry or its visible Object Details panel was unavailable."
    }
    $focusX = Convert-HenkaAutomationNumber $focusControl.Groups['x'].Value
    $focusY = Convert-HenkaAutomationNumber $focusControl.Groups['y'].Value
    $focusWidth = Convert-HenkaAutomationNumber $focusControl.Groups['width'].Value
    $focusHeight = Convert-HenkaAutomationNumber $focusControl.Groups['height'].Value
    $detailsX = Convert-HenkaAutomationNumber $detailsMatch.Groups['x'].Value
    $detailsY = Convert-HenkaAutomationNumber $detailsMatch.Groups['y'].Value
    $detailsWidth = Convert-HenkaAutomationNumber $detailsMatch.Groups['width'].Value
    $detailsHeight = Convert-HenkaAutomationNumber $detailsMatch.Groups['height'].Value
    if ($focusWidth -le 0.0 -or $focusHeight -le 0.0 -or
        $focusX -lt ($detailsX - 0.5) -or $focusY -lt ($detailsY - 0.5) -or
        ($focusX + $focusWidth) -gt ($detailsX + $detailsWidth + 0.5) -or
        ($focusY + $focusHeight) -gt ($detailsY + $detailsHeight + 0.5)) {
        throw "The product-reported Focus Camera control escaped visible Object Details."
    }
    $focusActionOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($focusX + ($focusWidth * 0.5)) -Y ($focusY + ($focusHeight * 0.5))
    $focusActionPattern = '^HENKA_AUTOMATION_DIAGNOSTIC camera_focus_action entity=' +
        $rowEntity + ' result=(?<result>[01]) camera_changed=(?<changed>[01]) position_x=.*\.'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $focusActionPattern -StartingOffset $focusActionOffset -TimeoutMilliseconds 10000)) {
        throw "The actual Focus Camera click did not report the product action and resulting camera state."
    }
    $focusAction = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $focusActionPattern
    if ($null -eq $focusAction -or $focusAction.Groups['result'].Value -ne '1' -or
        $focusAction.Groups['changed'].Value -ne '1') {
        throw "Focus Camera did not change the authoritative camera for the selected opened-scene entity."
    }
    $focusScreenshot = Join-Path $Context.RuntimeDirectory "open-scene-camera-focused.png"
    $null = Save-HenkaProductFramebufferScreenshot -Process $Context.Process -StdoutPath $Context.StdoutPath -CaptureDirectory $Context.FrameCaptureDirectory -RequestFile $Context.FrameCaptureRequestPath -Description "Packaged Open Scene camera focused on selected entity" -DestinationPath $focusScreenshot

    $expectedLiveCount = [int]$openedAttempt.Groups['count'].Value + 1
    $actionsPattern = 'scene_document_action_controls visible=1 document_object_count=' + $expectedLiveCount + ' selection_present=1 relative_path_empty=0 .* open_scene_rect='
    if (-not (Wait-FileContainsAfterOffset `
            -Path $Context.StdoutPath `
            -Pattern $actionsPattern `
            -StartingOffset $addCubeOffset `
            -TimeoutMilliseconds 10000)) {
        throw "The product did not report the opened scene plus one newly authored cube and its active selection."
    }
    $liveActions = Get-SceneDocumentActionControls -Path $Context.StdoutPath
    if ($null -eq $liveActions -or
        [int]$liveActions.Groups['document_object_count'].Value -ne $expectedLiveCount -or
        $liveActions.Groups['selection_present'].Value -ne '1') {
        throw "The product scene-action controls did not expose the current post-cube document state."
    }
    $loadedCount = $expectedLiveCount
    $loadedSelection = '1'
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
    Write-Output "[pass] Packaged Open Scene used the real path editor and confirmation; the named scene loaded with $loadedCount authored objects, a real Scene Objects row selected entity $rowEntity, and its real Focus Camera control changed the product camera. An escaping path was rejected without changing the live document, selection, or files."
    Write-Output "[pass] Open Scene visual evidence: $formScreenshot, $openedScreenshot, $focusScreenshot, $failureScreenshot"
}
