function Invoke-HenkaPackagedScriptEditorUnsavedChangeWorkflow {
    param(
        [Parameter(Mandatory = $true)][hashtable]$Context
    )

    $number = '[-+]?(?:\d+(?:\.\d*)?|\.\d+)'
    $Entity = [UInt64]0
    $scriptActionsPattern = '^HENKA_AUTOMATION_DIAGNOSTIC object_details_script_actions visible=1 entity=(?<entity>\d+) lua_x=(?<lua_x>' + $number + ') lua_y=(?<lua_y>' + $number +
        ') lua_width=(?<lua_width>' + $number + ') lua_height=(?<lua_height>' +
        $number + ') hks_x=(?<hks_x>' + $number + ') hks_y=(?<hks_y>' +
        $number + ') hks_width=(?<hks_width>' + $number + ') hks_height=(?<hks_height>' +
        $number + ')\.$'
    $surfacePattern = '^HENKA_AUTOMATION_DIAGNOSTIC script_editor_surface visible=1 entity=(?<entity>\d+) language=(?<language>\d+) dirty=(?<dirty>[01]) asset_path="(?<asset_path>[^"]+)" x=(?<card_x>' +
        $number + ') y=(?<card_y>' + $number + ') width=(?<card_width>' + $number +
        ') height=(?<card_height>' + $number + ') code_x=(?<code_x>' + $number +
        ') code_y=(?<code_y>' + $number + ') code_width=(?<code_width>' + $number +
        ') code_height=(?<code_height>' + $number + ')\.$'
    $scriptActions = $null
    $hksX = 0.0
    $hksY = 0.0
    $hksWidth = 0.0
    $hksHeight = 0.0
    $overviewDisclosurePattern = '^HENKA_AUTOMATION_DIAGNOSTIC object_details_disclosure id=overview entity=(?<entity>\d+) x=(?<x>' + $number + ') y=(?<y>' + $number + ') width=(?<width>' + $number + ') height=(?<height>' + $number + ') expanded=(?<expanded>[01])\.$'
    $detailsScrollPattern = 'HENKA_AUTOMATION_DIAGNOSTIC details-scroll frame=\d+ point=\((?<pointX>[-0-9.]+),(?<pointY>[-0-9.]+)\) content=(?<content>[-0-9.]+) viewport=(?<viewport>[-0-9.]+) delta=(?<delta>[-0-9.]+) offset=(?<before>[-0-9.]+)->(?<after>[-0-9.]+) handled=(?<handled>[01]) authored=[01] document=\d+'

    $overviewDisclosure = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $overviewDisclosurePattern
    if ($null -eq $overviewDisclosure) {
        throw '[HARNESS/GEOMETRY] The current product-owned Object Details Overview disclosure geometry was unavailable.'
    }
    $overviewEntityText = $overviewDisclosure.Groups['entity'].Value
    $Entity = [UInt64]$overviewEntityText
    $overviewX = Convert-HenkaAutomationNumber $overviewDisclosure.Groups['x'].Value
    $overviewY = Convert-HenkaAutomationNumber $overviewDisclosure.Groups['y'].Value
    $overviewWidth = Convert-HenkaAutomationNumber $overviewDisclosure.Groups['width'].Value
    $overviewHeight = Convert-HenkaAutomationNumber $overviewDisclosure.Groups['height'].Value
    $initialPanel = Get-DetailsPanelGeometry -Path $Context.StdoutPath
    if ($null -eq $initialPanel -or $overviewWidth -le 0.0 -or $overviewHeight -le 0.0) {
        throw '[HARNESS/GEOMETRY] Object Details Overview did not expose a usable current disclosure rectangle.'
    }
    $initialPanelX = Convert-HenkaAutomationNumber $initialPanel.Groups['x'].Value
    $initialPanelY = Convert-HenkaAutomationNumber $initialPanel.Groups['y'].Value
    $initialPanelWidth = Convert-HenkaAutomationNumber $initialPanel.Groups['width'].Value
    $initialPanelHeight = Convert-HenkaAutomationNumber $initialPanel.Groups['height'].Value
    if ($overviewX -lt ($initialPanelX - 0.5) -or $overviewY -lt ($initialPanelY - 0.5) -or
        ($overviewX + $overviewWidth) -gt ($initialPanelX + $initialPanelWidth + 0.5) -or
        ($overviewY + $overviewHeight) -gt ($initialPanelY + $initialPanelHeight + 0.5)) {
        throw '[PRODUCT/GEOMETRY] The reported Object Details Overview hit target was outside the visible panel.'
    }
    if ($overviewDisclosure.Groups['expanded'].Value -eq '0') {
        $overviewOffset = Get-FileLengthSafe -Path $Context.StdoutPath
        $null = Send-HenkaObservedAutomationClick `
            -EventPath $Context.AutomationInputPath `
            -StdoutPath $Context.StdoutPath `
            -X ($overviewX + ($overviewWidth * 0.5)) `
            -Y ($overviewY + ($overviewHeight * 0.5)) `
            -Purpose 'Expand Object Details Overview'
        $overviewExpandedPattern = '^HENKA_AUTOMATION_DIAGNOSTIC object_details_disclosure id=overview entity=' +
            [Regex]::Escape($overviewEntityText) + ' x=' + $number + ' y=' + $number + ' width=' + $number + ' height=' + $number + ' expanded=1\.$'
        if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $overviewExpandedPattern -StartingOffset $overviewOffset -TimeoutMilliseconds 10000)) {
            throw '[PRODUCT/REACHABILITY] The real Object Details Overview disclosure click was consumed but Overview did not become expanded.'
        }
        $overviewDisclosure = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $overviewDisclosurePattern
        if ($null -eq $overviewDisclosure -or
            $overviewDisclosure.Groups['entity'].Value -ne $overviewEntityText -or
            $overviewDisclosure.Groups['expanded'].Value -ne '1') {
            throw '[PRODUCT/STATE] Object Details Overview did not report the same selected entity as expanded after the click.'
        }
    }

    for ($attempt = 0; $attempt -lt 12; ++$attempt) {
        $panelMatch = Get-DetailsPanelGeometry -Path $Context.StdoutPath
        if ($null -eq $panelMatch) {
            throw '[HARNESS/GEOMETRY] Object Details geometry was unavailable while locating Add HenkaScript.'
        }
        $panelX = Convert-HenkaAutomationNumber $panelMatch.Groups['x'].Value
        $panelY = Convert-HenkaAutomationNumber $panelMatch.Groups['y'].Value
        $panelWidth = Convert-HenkaAutomationNumber $panelMatch.Groups['width'].Value
        $panelHeight = Convert-HenkaAutomationNumber $panelMatch.Groups['height'].Value
        $scriptActions = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $scriptActionsPattern
        if ($null -ne $scriptActions) {
            if ($scriptActions.Groups['entity'].Value -ne $overviewEntityText) {
                throw '[PRODUCT/STATE] Add HenkaScript geometry belongs to a different selected object than the expanded Overview section.'
            }
            $Entity = [UInt64]$scriptActions.Groups['entity'].Value
            $hksX = Convert-HenkaAutomationNumber $scriptActions.Groups['hks_x'].Value
            $hksY = Convert-HenkaAutomationNumber $scriptActions.Groups['hks_y'].Value
            $hksWidth = Convert-HenkaAutomationNumber $scriptActions.Groups['hks_width'].Value
            $hksHeight = Convert-HenkaAutomationNumber $scriptActions.Groups['hks_height'].Value
            if ($hksWidth -gt 0.0 -and $hksHeight -gt 0.0 -and
                $hksX -ge ($panelX - 0.5) -and $hksY -ge ($panelY - 0.5) -and
                ($hksX + $hksWidth) -le ($panelX + $panelWidth + 0.5) -and
                ($hksY + $hksHeight) -le ($panelY + $panelHeight + 0.5)) {
                break
            }
        }

        $scrollOffset = Get-FileLengthSafe -Path $Context.StdoutPath
        Send-HenkaAutomationScroll `
            -EventPath $Context.AutomationInputPath `
            -X ($panelX + ($panelWidth * 0.5)) `
            -Y ($panelY + ($panelHeight * 0.45)) `
            -WheelDelta -1.0
        if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $detailsScrollPattern -StartingOffset $scrollOffset -TimeoutMilliseconds 5000)) {
            throw '[HARNESS/INPUT] Object Details did not report consumption of the downward scroll used to reach Add HenkaScript.'
        }
        $scroll = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $detailsScrollPattern
        if ($null -eq $scroll -or $scroll.Groups['handled'].Value -ne '1') {
            throw '[PRODUCT/SCROLL] Object Details did not own the scroll input used to reach Add HenkaScript.'
        }
        $offsetBefore = Convert-HenkaAutomationNumber $scroll.Groups['before'].Value
        $offsetAfter = Convert-HenkaAutomationNumber $scroll.Groups['after'].Value
        if ($offsetAfter -le ($offsetBefore + 0.01)) {
            throw '[PRODUCT/REACHABILITY] Consumed downward Object Details scrolling did not advance toward the script controls.'
        }
    }
    if ($null -eq $scriptActions -or
        $hksWidth -le 0.0 -or $hksHeight -le 0.0 -or
        $hksX -lt ($panelX - 0.5) -or $hksY -lt ($panelY - 0.5) -or
        ($hksX + $hksWidth) -gt ($panelX + $panelWidth + 0.5) -or
        ($hksY + $hksHeight) -gt ($panelY + $panelHeight + 0.5)) {
        throw '[PRODUCT/REACHABILITY] The actual Add HenkaScript control did not become visible inside Object Details.'
    }

    $attachOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    $null = Send-HenkaObservedAutomationClick `
        -EventPath $Context.AutomationInputPath `
        -StdoutPath $Context.StdoutPath `
        -X ($hksX + ($hksWidth * 0.5)) `
        -Y ($hksY + ($hksHeight * 0.5)) `
        -Purpose 'Add HenkaScript to the selected scene object'
    $surfaceEntityPattern = $surfacePattern.Replace(
        'entity=(?<entity>\d+)',
        ('entity=' + [Regex]::Escape($overviewEntityText)))
    $surface = $null
    for ($revealAttempt = 0; $revealAttempt -lt 8; ++$revealAttempt) {
        if (Wait-FileContainsAfterOffset `
                -Path $Context.StdoutPath `
                -Pattern $surfaceEntityPattern `
                -StartingOffset $attachOffset `
                -TimeoutMilliseconds 1000) {
            $surface = Get-LastLogRegexMatch `
                -Path $Context.StdoutPath `
                -Pattern $surfacePattern
            if ($null -ne $surface) {
                break
            }
        }
        if ($revealAttempt -ge 7) {
            break
        }

        # Adding a behavior inserts its inline editor into the same scrollable
        # Object Details flow. At a short viewport, the editor can begin below
        # the visible edge even though the Add control was reachable. Continue
        # through the real panel scroll path and wait for its consumed progress.
        $panelMatch = Get-DetailsPanelGeometry -Path $Context.StdoutPath
        if ($null -eq $panelMatch) {
            throw '[HARNESS/GEOMETRY] Fresh Object Details geometry was unavailable while revealing the newly attached Script Editor.'
        }
        $panelX = Convert-HenkaAutomationNumber $panelMatch.Groups['x'].Value
        $panelY = Convert-HenkaAutomationNumber $panelMatch.Groups['y'].Value
        $panelWidth = Convert-HenkaAutomationNumber $panelMatch.Groups['width'].Value
        $panelHeight = Convert-HenkaAutomationNumber $panelMatch.Groups['height'].Value
        if ($panelWidth -le 0.0 -or $panelHeight -le 0.0) {
            throw '[HARNESS/GEOMETRY] Object Details geometry was invalid while revealing the newly attached Script Editor.'
        }

        $scrollObserveOffset = Get-FileLengthSafe -Path $Context.StdoutPath
        Send-HenkaAutomationScroll `
            -EventPath $Context.AutomationInputPath `
            -X ($panelX + ($panelWidth * 0.5)) `
            -Y ($panelY + ($panelHeight * 0.45)) `
            -WheelDelta -1.0
        if (-not (Wait-FileContainsAfterOffset `
                -Path $Context.StdoutPath `
                -Pattern $detailsScrollPattern `
                -StartingOffset $scrollObserveOffset `
                -TimeoutMilliseconds 5000)) {
            throw '[HARNESS/INPUT] Object Details did not acknowledge scroll input while revealing the newly attached Script Editor.'
        }
        $revealScroll = Get-LastLogRegexMatch `
            -Path $Context.StdoutPath `
            -Pattern $detailsScrollPattern
        if ($null -eq $revealScroll -or
            $revealScroll.Groups['handled'].Value -ne '1') {
            throw '[PRODUCT/SCROLL] Object Details did not own the scroll used to reveal the newly attached Script Editor.'
        }
        $revealOffsetBefore = Convert-HenkaAutomationNumber $revealScroll.Groups['before'].Value
        $revealOffsetAfter = Convert-HenkaAutomationNumber $revealScroll.Groups['after'].Value
        if ($revealOffsetAfter -le ($revealOffsetBefore + 0.01)) {
            throw '[PRODUCT/REACHABILITY] Object Details consumed the reveal scroll without advancing toward the newly attached Script Editor.'
        }
    }
    if ($null -eq $surface) {
        throw '[PRODUCT/SCRIPT_EDITOR] The newly attached Script Editor did not become visible after bounded, product-consumed Object Details scrolling.'
    }
    if ($null -eq $surface -or $surface.Groups['dirty'].Value -ne '0' -or
        [UInt64]$surface.Groups['entity'].Value -ne $Entity) {
        throw '[PRODUCT/SCRIPT_EDITOR] The newly attached script did not begin from a clean source model.'
    }
    $assetPath = $surface.Groups['asset_path'].Value
    $runtimeRoot = [IO.Path]::GetFullPath($Context.RuntimeDirectory).TrimEnd('\') + '\'
    $userDataRoot = [IO.Path]::GetFullPath((Split-Path -Parent $Context.ProjectManifestPath))
    $userDataPrefix = $userDataRoot.TrimEnd('\') + '\'
    if (-not $userDataRoot.StartsWith($runtimeRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw '[HARNESS/OWNERSHIP] The package user-data root escaped its owned runtime directory.'
    }
    if ([IO.Path]::IsPathRooted($assetPath) -or
        ($assetPath -split '[\\/]') -contains '..') {
        throw '[PRODUCT/PATH] The Script Editor reported an asset path outside the package project-relative contract.'
    }
    $assetFile = [IO.Path]::GetFullPath((Join-Path $userDataRoot $assetPath))
    if (-not $assetFile.StartsWith($userDataPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $assetFile -PathType Leaf)) {
        throw '[PRODUCT/PERSISTENCE] The attached script asset was not present under the package-owned project data root.'
    }
    $pathToCheck = $assetFile
    while ($pathToCheck.StartsWith($userDataPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        $pathItem = Get-Item -LiteralPath $pathToCheck -Force
        if (($pathItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw '[HARNESS/OWNERSHIP] The generated script asset path is redirected through a reparse point.'
        }
        $parentPath = Split-Path -Parent $pathToCheck
        if ([string]::IsNullOrWhiteSpace($parentPath) -or $parentPath -eq $userDataRoot) {
            break
        }
        $pathToCheck = $parentPath
    }
    $userDataRootItem = Get-Item -LiteralPath $userDataRoot -Force
    if (($userDataRootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw '[HARNESS/OWNERSHIP] The package project-data root is redirected through a reparse point.'
    }
    $originalSource = [IO.File]::ReadAllText($assetFile)

    $saveControls = Get-SceneDocumentActionControls -Path $Context.StdoutPath
    if ($null -eq $saveControls -or
        [double]$saveControls.Groups['save_width'].Value -le 0.0 -or
        [double]$saveControls.Groups['save_height'].Value -le 0.0) {
        throw '[HARNESS/GEOMETRY] Fresh product Save Scene geometry was unavailable after script attachment.'
    }
    $saveOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    $null = Send-HenkaObservedAutomationClick `
        -EventPath $Context.AutomationInputPath `
        -StdoutPath $Context.StdoutPath `
        -X ([double]$saveControls.Groups['save_x'].Value + ([double]$saveControls.Groups['save_width'].Value * 0.5)) `
        -Y ([double]$saveControls.Groups['save_y'].Value + ([double]$saveControls.Groups['save_height'].Value * 0.5)) `
        -Purpose 'Save the scene with the newly attached script before editing its source'
    $savedPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=saved name=' +
        [Regex]::Escape($Context.SceneName) + '\.$'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $savedPattern -StartingOffset $saveOffset -TimeoutMilliseconds 15000)) {
        throw '[PRODUCT/PERSISTENCE] Save Scene did not persist the new script attachment.'
    }
    $Context.SavedSceneHash = (Get-FileHash -LiteralPath $Context.SavedScenePath -Algorithm SHA256).Hash
    $Context.ProjectManifestHash = (Get-FileHash -LiteralPath $Context.ProjectManifestPath -Algorithm SHA256).Hash

    $cardX = Convert-HenkaAutomationNumber $surface.Groups['card_x'].Value
    $cardY = Convert-HenkaAutomationNumber $surface.Groups['card_y'].Value
    $cardWidth = Convert-HenkaAutomationNumber $surface.Groups['card_width'].Value
    $cardHeight = Convert-HenkaAutomationNumber $surface.Groups['card_height'].Value
    $codeX = Convert-HenkaAutomationNumber $surface.Groups['code_x'].Value
    $codeY = Convert-HenkaAutomationNumber $surface.Groups['code_y'].Value
    $codeWidth = Convert-HenkaAutomationNumber $surface.Groups['code_width'].Value
    $codeHeight = Convert-HenkaAutomationNumber $surface.Groups['code_height'].Value
    if ($cardWidth -le 0.0 -or $cardHeight -le 0.0 -or
        $codeWidth -le 42.0 -or $codeHeight -le 0.0 -or
        $codeX -lt ($cardX - 0.5) -or $codeY -lt ($cardY - 0.5) -or
        ($codeX + $codeWidth) -gt ($cardX + $cardWidth + 0.5) -or
        ($codeY + $codeHeight) -gt ($cardY + $cardHeight + 0.5)) {
        throw '[PRODUCT/GEOMETRY] The product-reported code editor region was invalid or escaped its visible Script Editor card.'
    }

    $editOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    $null = Send-HenkaObservedAutomationClick `
        -EventPath $Context.AutomationInputPath `
        -StdoutPath $Context.StdoutPath `
        -X ($codeX + 20.0) `
        -Y ($codeY + 7.0) `
        -Purpose 'Place the caret at the first source character through the real Script Editor hit region'
    $textOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationText -EventPath $Context.AutomationInputPath -Text ' '
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'input record=\d+ type=text' -StartingOffset $textOffset -TimeoutMilliseconds 10000)) {
        throw '[HARNESS/INPUT] The Script Editor did not acknowledge consumption of the source text input.'
    }
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $surfacePattern -StartingOffset $editOffset -TimeoutMilliseconds 10000)) {
        throw '[PRODUCT/SCRIPT_EDITOR] The actual source edit did not produce a current editor state report.'
    }
    $surface = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $surfacePattern
    if ($null -eq $surface -or $surface.Groups['dirty'].Value -ne '1' -or
        [UInt64]$surface.Groups['entity'].Value -ne $Entity -or
        $surface.Groups['asset_path'].Value -ne $assetPath) {
        throw '[PRODUCT/SCRIPT_EDITOR] Typing through the reported code region did not dirty the same canonical script asset.'
    }
    $dirtyCapture = Join-Path $Context.RuntimeDirectory 'script-editor-source-dirty.png'
    $null = Save-HenkaProductFramebufferScreenshot `
        -Process $Context.Process `
        -StdoutPath $Context.StdoutPath `
        -CaptureDirectory $Context.FrameCaptureDirectory `
        -RequestFile $Context.FrameCaptureRequestPath `
        -Description 'Packaged Script Editor with unsaved source edit' `
        -DestinationPath $dirtyCapture

    $reloadAction = Get-SceneDocumentActionControls -Path $Context.StdoutPath
    if ($null -eq $reloadAction -or
        [double]$reloadAction.Groups['reload_width'].Value -le 0.0 -or
        [double]$reloadAction.Groups['reload_height'].Value -le 0.0) {
        throw '[HARNESS/GEOMETRY] Fresh product Reload Scene geometry was unavailable for the script-dirty test.'
    }
    $reloadOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    $null = Send-HenkaObservedAutomationClick `
        -EventPath $Context.AutomationInputPath `
        -StdoutPath $Context.StdoutPath `
        -X ([double]$reloadAction.Groups['reload_x'].Value + ([double]$reloadAction.Groups['reload_width'].Value * 0.5)) `
        -Y ([double]$reloadAction.Groups['reload_y'].Value + ([double]$reloadAction.Groups['reload_height'].Value * 0.5)) `
        -Purpose 'Reload the clean scene while Script Editor source is dirty'
    $promptPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_transition_prompt_pending action=reload dirty=1 scene_dirty=0 script_dirty=1 transform_dirty=0 scene_dirty_query_result=0 '
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $promptPattern -StartingOffset $reloadOffset -TimeoutMilliseconds 10000)) {
        throw '[PRODUCT/UNSAVED_PROTECTION] Reload did not isolate and protect the dirty Script Editor source while the scene document was clean.'
    }
    $dialog = Get-SceneTransitionDialogControls -Path $Context.StdoutPath -Action 'reload'
    if ($null -eq $dialog -or $dialog.Groups['scene_dirty'].Value -ne '0' -or
        $dialog.Groups['script_dirty'].Value -ne '1' -or
        $dialog.Groups['transform_dirty'].Value -ne '0') {
        throw '[PRODUCT/UNSAVED_PROTECTION] The Reload dialog did not report clean scene state plus dirty script state.'
    }
    $promptCapture = Join-Path $Context.RuntimeDirectory 'script-editor-reload-prompt.png'
    $null = Save-HenkaProductFramebufferScreenshot `
        -Process $Context.Process `
        -StdoutPath $Context.StdoutPath `
        -CaptureDirectory $Context.FrameCaptureDirectory `
        -RequestFile $Context.FrameCaptureRequestPath `
        -Description 'Packaged Reload prompt protecting dirty script source' `
        -DestinationPath $promptCapture

    $cancelOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    $null = Send-HenkaObservedAutomationClick `
        -EventPath $Context.AutomationInputPath `
        -StdoutPath $Context.StdoutPath `
        -X ([double]$dialog.Groups['cancel_x'].Value + ([double]$dialog.Groups['cancel_width'].Value * 0.5)) `
        -Y ([double]$dialog.Groups['cancel_y'].Value + ([double]$dialog.Groups['cancel_height'].Value * 0.5)) `
        -Purpose 'Cancel Reload and retain the dirty script source'
    $decisionPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_transition_decision [^\r\n]*$'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $decisionPattern -StartingOffset $cancelOffset -TimeoutMilliseconds 10000)) {
        throw '[PRODUCT/RECOVERY] Cancel did not publish the scene-transition decision for the dirty script.'
    }
    $decisionLine = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $decisionPattern
    $decision = if ($null -eq $decisionLine) { $null } else { ConvertFrom-HenkaSceneTransitionDecisionLine -Line $decisionLine.Value }
    if ($null -eq $decision -or $decision.Action -ne 'reload' -or
        $decision.Choice -ne 'cancel' -or $decision.SceneRetained -ne '1' -or
        $decision.SceneDirty -ne '0' -or $decision.ScriptDirty -ne '1' -or
        $decision.TransformDirty -ne '0') {
        throw '[PRODUCT/RECOVERY] Cancel did not retain the same dirty Script Editor state.'
    }

    $reloadAction = Get-SceneDocumentActionControls -Path $Context.StdoutPath
    if ($null -eq $reloadAction) {
        throw '[HARNESS/GEOMETRY] Scene action geometry was unavailable after canceling Reload.'
    }
    $reloadOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    $null = Send-HenkaObservedAutomationClick `
        -EventPath $Context.AutomationInputPath `
        -StdoutPath $Context.StdoutPath `
        -X ([double]$reloadAction.Groups['reload_x'].Value + ([double]$reloadAction.Groups['reload_width'].Value * 0.5)) `
        -Y ([double]$reloadAction.Groups['reload_y'].Value + ([double]$reloadAction.Groups['reload_height'].Value * 0.5)) `
        -Purpose 'Reopen the Reload prompt to save the dirty script source'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $promptPattern -StartingOffset $reloadOffset -TimeoutMilliseconds 10000)) {
        throw '[PRODUCT/UNSAVED_PROTECTION] The second Reload did not continue to protect the retained dirty script source.'
    }
    $dialog = Get-SceneTransitionDialogControls -Path $Context.StdoutPath -Action 'reload'
    if ($null -eq $dialog -or $dialog.Groups['scene_dirty'].Value -ne '0' -or
        $dialog.Groups['script_dirty'].Value -ne '1' -or
        $dialog.Groups['transform_dirty'].Value -ne '0') {
        throw '[PRODUCT/UNSAVED_PROTECTION] The second Reload prompt disagreed with the retained dirty script state.'
    }

    $saveScriptOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    $null = Send-HenkaObservedAutomationClick `
        -EventPath $Context.AutomationInputPath `
        -StdoutPath $Context.StdoutPath `
        -X ([double]$dialog.Groups['save_x'].Value + ([double]$dialog.Groups['save_width'].Value * 0.5)) `
        -Y ([double]$dialog.Groups['save_y'].Value + ([double]$dialog.Groups['save_height'].Value * 0.5)) `
        -Purpose 'Save the dirty script source and complete Reload'
    $scriptSavePattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_transition_script_save action=reload result=0 dirty_before=1 dirty_after=0 path_available=1\.$'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $scriptSavePattern -StartingOffset $saveScriptOffset -TimeoutMilliseconds 15000)) {
        throw '[PRODUCT/PERSISTENCE] Reload Save did not save the script model successfully before transitioning.'
    }
    $transitionPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_transition_result [^\r\n]*$'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $transitionPattern -StartingOffset $saveScriptOffset -TimeoutMilliseconds 15000)) {
        throw '[PRODUCT/RECOVERY] Saving the script did not complete the requested scene Reload.'
    }
    $transitionLine = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $transitionPattern
    $transitionResult = if ($null -eq $transitionLine) { $null } else { ConvertFrom-HenkaSceneTransitionResultLine -Line $transitionLine.Value }
    if ($null -eq $transitionResult -or $transitionResult.Action -ne 'reload' -or
        $transitionResult.Result -ne '0') {
        throw '[PRODUCT/RECOVERY] Reload did not finish successfully after the script save.'
    }

    $savedSource = [IO.File]::ReadAllText($assetFile)
    if (-not [string]::Equals($savedSource, (' ' + $originalSource), [StringComparison]::Ordinal)) {
        throw '[PRODUCT/PERSISTENCE] The exact leading-space source edit was not preserved in the script asset after Reload Save.'
    }
    $reloadedSurfaceOffset = $saveScriptOffset
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $surfacePattern -StartingOffset $reloadedSurfaceOffset -TimeoutMilliseconds 10000)) {
        throw '[PRODUCT/RELOAD] Reload did not recreate a current Script Editor surface for the attached behavior.'
    }
    $reloadedSurface = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $surfacePattern
    if ($null -eq $reloadedSurface -or $reloadedSurface.Groups['dirty'].Value -ne '0' -or
        [UInt64]$reloadedSurface.Groups['entity'].Value -ne $Entity -or
        $reloadedSurface.Groups['asset_path'].Value -ne $assetPath) {
        throw '[PRODUCT/RELOAD] The reloaded Script Editor did not present the persisted source as the same clean script asset.'
    }
    $Context.SavedSceneHash = (Get-FileHash -LiteralPath $Context.SavedScenePath -Algorithm SHA256).Hash
    $Context.ProjectManifestHash = (Get-FileHash -LiteralPath $Context.ProjectManifestPath -Algorithm SHA256).Hash
    Write-Output "[pass] Packaged Script Editor Add → edit → Reload Cancel retained dirty source → Reload Save persisted and reloaded the same clean asset. Captures: $dirtyCapture, $promptCapture"
}
