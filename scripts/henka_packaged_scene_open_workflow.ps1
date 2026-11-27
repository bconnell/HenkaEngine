function Get-GameAuthoringPlayControls {
    param([Parameter(Mandatory = $true)][string]$Path)

    $line = Get-LastLogRegexMatch -Path $Path `
        -Pattern '(?m)^HENKA_AUTOMATION_DIAGNOSTIC game_authoring_play_controls [^\r\n]*$'
    if ($null -eq $line) { return $null }
    $fields = ConvertFrom-HenkaGameAuthoringPlayControlsLine -Line $line.Value
    if ($null -eq $fields) { return $null }
    return ConvertTo-HenkaAutomationGeometryResult `
        -Fields $fields `
        -Line $line.Value `
        -RectangleMappings @('trigger_rect=trigger', 'play_rect=play')
}

function Get-SceneOpenCatalogControls {
    param([Parameter(Mandatory = $true)][string]$Path)

    $number = '[-+]?\d+(?:\.\d+)?'
    $rectangle = {
        param([string]$Prefix)
        return "(?<${Prefix}_x>$number),(?<${Prefix}_y>$number),(?<${Prefix}_width>$number),(?<${Prefix}_height>$number)"
    }
    $pattern = '(?m)^HENKA_AUTOMATION_DIAGNOSTIC scene_path_editor_controls ' +
        '[^\r\n]*?mode=open[^\r\n]*?relative_path_empty=(?<relative_path_empty>[01]) ' +
        'relative_path=(?<relative_path>.*?) catalog_count=(?<count>\d+) ' +
        'catalog_page=(?<page>\d+) catalog_page_count=(?<page_count>\d+) ' +
        'catalog_result=(?<result>-?\d+) catalog_stale=(?<stale>[01]) ' +
        'refresh_rect=' + (& $rectangle 'refresh') + ' row0_rect=' +
        (& $rectangle 'row0') + ' row0_path=(?<row0_path>.*?) ' +
        'row1_rect=' + (& $rectangle 'row1') +
        ' row1_path=(?<row1_path>.*?) row2_rect=' + (& $rectangle 'row2') +
        ' row2_path=(?<row2_path>.*?) previous_rect=' +
        (& $rectangle 'previous') + ' page_rect=' + (& $rectangle 'page') +
        ' next_rect=' + (& $rectangle 'next') + '\.'
    $line = Get-LastLogRegexMatch -Path $Path -Pattern $pattern
    if ($null -eq $line) { return $null }

    $rows = @(
        foreach ($rowIndex in 0..2) {
            $prefix = "row$rowIndex"
            [pscustomobject]@{
                Path = [string]$line.Groups["${prefix}_path"].Value
                X = Convert-HenkaAutomationNumber $line.Groups["${prefix}_x"].Value
                Y = Convert-HenkaAutomationNumber $line.Groups["${prefix}_y"].Value
                Width = Convert-HenkaAutomationNumber $line.Groups["${prefix}_width"].Value
                Height = Convert-HenkaAutomationNumber $line.Groups["${prefix}_height"].Value
            }
        }
    )
    return [pscustomobject]@{
        RelativePath = [string]$line.Groups['relative_path'].Value
        RelativePathEmpty = [int]$line.Groups['relative_path_empty'].Value
        Count = [int]$line.Groups['count'].Value
        Page = [int]$line.Groups['page'].Value
        PageCount = [int]$line.Groups['page_count'].Value
        Result = [int]$line.Groups['result'].Value
        Stale = [int]$line.Groups['stale'].Value
        Rows = $rows
        Previous = [pscustomobject]@{
            X = Convert-HenkaAutomationNumber $line.Groups['previous_x'].Value
            Y = Convert-HenkaAutomationNumber $line.Groups['previous_y'].Value
            Width = Convert-HenkaAutomationNumber $line.Groups['previous_width'].Value
            Height = Convert-HenkaAutomationNumber $line.Groups['previous_height'].Value
        }
        Next = [pscustomobject]@{
            X = Convert-HenkaAutomationNumber $line.Groups['next_x'].Value
            Y = Convert-HenkaAutomationNumber $line.Groups['next_y'].Value
            Width = Convert-HenkaAutomationNumber $line.Groups['next_width'].Value
            Height = Convert-HenkaAutomationNumber $line.Groups['next_height'].Value
        }
        Line = $line.Value
    }
}

function Invoke-SceneOpenCatalogPage {
    param(
        [Parameter(Mandatory = $true)][hashtable]$Context,
        [Parameter(Mandatory = $true)][pscustomobject]$Panel,
        [Parameter(Mandatory = $true)][ValidateSet('previous', 'next')][string]$Direction
    )

    $catalog = Get-SceneOpenCatalogControls -Path $Context.StdoutPath
    if ($null -eq $catalog -or $catalog.Result -ne 0 -or $catalog.Stale -ne 0) {
        throw 'The product did not expose a current, successful Open Scene catalog before page navigation.'
    }
    $pageDelta = if ($Direction -eq 'next') { 1 } else { -1 }
    $expectedPage = $catalog.Page + $pageDelta
    if ($expectedPage -lt 0 -or $expectedPage -ge $catalog.PageCount) {
        throw "Open Scene $Direction navigation was requested outside the current page bounds."
    }
    $control = if ($Direction -eq 'next') { $catalog.Next } else { $catalog.Previous }
    if ($control.Width -le 0.0 -or $control.Height -le 0.0 -or
        $control.X -lt ($Panel.X - 0.5) -or
        $control.Y -lt ($Panel.Y - 0.5) -or
        ($control.X + $control.Width) -gt ($Panel.X + $Panel.Width + 0.5) -or
        ($control.Y + $control.Height) -gt ($Panel.Y + $Panel.Height + 0.5)) {
        throw "The product-reported Open Scene $Direction control was outside visible Object Details."
    }

    $pageOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt `
        -EventPath $Context.AutomationInputPath `
        -X ($control.X + ($control.Width * 0.5)) `
        -Y ($control.Y + ($control.Height * 0.5))
    if (-not (Wait-FileContainsAfterOffset `
            -Path $Context.StdoutPath `
            -Pattern ('catalog_page=' + $expectedPage + '\b') `
            -StartingOffset $pageOffset `
            -TimeoutMilliseconds 10000)) {
        throw "The real Open Scene $Direction control did not advance the catalog page."
    }

    $updated = Get-SceneOpenCatalogControls -Path $Context.StdoutPath
    if ($null -eq $updated -or $updated.Result -ne 0 -or $updated.Stale -ne 0 -or
        $updated.Page -ne $expectedPage -or $updated.PageCount -ne $catalog.PageCount -or
        $updated.RelativePathEmpty -ne $catalog.RelativePathEmpty -or
        $updated.RelativePath -ne $catalog.RelativePath) {
        throw "Open Scene $Direction changed the page without preserving the current catalog/path state."
    }
    return $updated
}

function Invoke-SceneOpenCatalogSelection {
    param(
        [Parameter(Mandatory = $true)][hashtable]$Context,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][pscustomobject]$Panel
    )

    for ($pageAttempt = 0; $pageAttempt -lt 43; $pageAttempt++) {
        $catalog = Get-SceneOpenCatalogControls -Path $Context.StdoutPath
        if ($null -eq $catalog -or $catalog.Count -le 0 -or
            $catalog.Result -ne 0 -or $catalog.Stale -ne 0) {
            throw "The product did not expose a current, successful Open Scene catalog."
        }
        foreach ($row in $catalog.Rows) {
            if ([string]::Equals(
                    $row.Path,
                    $RelativePath,
                    [System.StringComparison]::OrdinalIgnoreCase)) {
                if ($row.Width -le 0.0 -or $row.Height -le 0.0 -or
                    $row.X -lt ($Panel.X - 0.5) -or
                    $row.Y -lt ($Panel.Y - 0.5) -or
                    ($row.X + $row.Width) -gt ($Panel.X + $Panel.Width + 0.5) -or
                    ($row.Y + $row.Height) -gt ($Panel.Y + $Panel.Height + 0.5)) {
                    throw "The selected Open Scene catalog row was outside visible Object Details."
                }
                $selectionOffset = Get-FileLengthSafe -Path $Context.StdoutPath
                Send-HenkaAutomationClickAt `
                    -EventPath $Context.AutomationInputPath `
                    -X ($row.X + ($row.Width * 0.5)) `
                    -Y ($row.Y + ($row.Height * 0.5))
                $selectedPathPattern = 'scene_path_editor_controls .* relative_path_empty=0 relative_path=' +
                    [Regex]::Escape($RelativePath) + ' catalog_count='
                if (-not (Wait-FileContainsAfterOffset `
                        -Path $Context.StdoutPath `
                        -Pattern $selectedPathPattern `
                        -StartingOffset $selectionOffset `
                        -TimeoutMilliseconds 10000)) {
                    throw "A real catalog-row click did not publish the selected canonical scene path."
                }
                $selected = Get-SceneOpenCatalogControls -Path $Context.StdoutPath
                if ($null -eq $selected -or
                    -not [string]::Equals(
                        $selected.RelativePath,
                        $RelativePath,
                        [System.StringComparison]::OrdinalIgnoreCase)) {
                    throw "The Open Scene path state did not match the clicked catalog entry."
                }
                return $selected
            }
        }

        if ($catalog.PageCount -le 1 -or
            $catalog.Page + 1 -ge $catalog.PageCount) {
            break
        }
        $catalog = Invoke-SceneOpenCatalogPage `
            -Context $Context `
            -Panel $Panel `
            -Direction 'next'
    }
    throw "The packaged Open Scene catalog did not contain the required saved scene '$RelativePath'."
}

function Get-GameAuthoringPhysicsDisclosureControls {
    param([Parameter(Mandatory = $true)][string]$Path)

    $line = Get-LastLogRegexMatch -Path $Path `
        -Pattern '(?m)^HENKA_AUTOMATION_DIAGNOSTIC game_authoring_physics_disclosure [^\r\n]*$'
    if ($null -eq $line) { return $null }
    $fields = ConvertFrom-HenkaGameAuthoringPhysicsDisclosureLine -Line $line.Value
    if ($null -eq $fields) { return $null }
    return ConvertTo-HenkaAutomationGeometryResult `
        -Fields $fields `
        -Line $line.Value `
        -RectangleMappings @('rect=disclosure')
}

function Invoke-HenkaPackagedSceneOpenWorkflow {
    [CmdletBinding()]
    param([Parameter(Mandatory = $true)][hashtable]$Context)

    $runtimePrefix = [System.IO.Path]::GetFullPath([string]$Context.RuntimeDirectory).TrimEnd('\') + '\'
    $savedScenePath = [System.IO.Path]::GetFullPath([string]$Context.SavedScenePath)
    if (-not $savedScenePath.StartsWith($runtimePrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $savedScenePath -PathType Leaf)) {
        throw 'The Open Scene pagination fixture source was missing or outside its owned packaged runtime.'
    }
    $savedSceneHash = (Get-FileHash -LiteralPath $savedScenePath -Algorithm SHA256).Hash
    if ($savedSceneHash -ne [string]$Context.SavedSceneHash) {
        throw 'The Open Scene pagination fixture source changed after its saved-scene proof.'
    }
    $sceneDirectory = Split-Path -Parent $savedScenePath
    $fixtureNames = @('OpenCatalogPageFixture01.hscene', 'OpenCatalogPageFixture02.hscene')
    foreach ($fixtureName in $fixtureNames) {
        $fixturePath = Join-Path $sceneDirectory $fixtureName
        if (Test-Path -LiteralPath $fixturePath) {
            throw "The packaged Open Scene pagination fixture path already exists: $fixtureName"
        }
        Copy-Item -LiteralPath $savedScenePath -Destination $fixturePath
        if ((Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256).Hash -ne $savedSceneHash) {
            throw "The Open Scene pagination fixture did not preserve valid scene data: $fixtureName"
        }
    }
    Write-Output '[fixture] Added two byte-verified scene documents inside the owned runtime to exercise catalog pagination.'

    $dirtySceneName = $Context.ExpectedCurrentSceneName + ' Unsaved Open Draft'
    $sceneNameControls = Get-SceneNameControls -Path $Context.StdoutPath
    if ($null -eq $sceneNameControls) {
        throw "The current scene name field was unavailable before the Open Scene dirty-state check."
    }
    Send-HenkaAutomationClickAt `
        -EventPath $Context.AutomationInputPath `
        -X ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_x'].Value) +
            ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_width'].Value) * 0.5)) `
        -Y ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_y'].Value) +
            ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['field_height'].Value) * 0.5))
    Clear-HenkaTextField -EventPath $Context.AutomationInputPath
    $dirtyNameInputOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationText -EventPath $Context.AutomationInputPath -Text $dirtySceneName
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'input record=\d+ type=text' -StartingOffset $dirtyNameInputOffset -TimeoutMilliseconds 10000)) {
        throw "The product did not consume the deliberate dirty scene-name input before the Rename action."
    }
    $sceneNameControls = Get-SceneNameControls -Path $Context.StdoutPath
    if ($null -eq $sceneNameControls) {
        throw "Fresh product-owned Scene Name geometry was unavailable immediately before Rename."
    }
    Send-HenkaAutomationClickAt `
        -EventPath $Context.AutomationInputPath `
        -X ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_x'].Value) +
            ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_width'].Value) * 0.5)) `
        -Y ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_y'].Value) +
            ((Convert-HenkaAutomationNumber $sceneNameControls.Groups['rename_height'].Value) * 0.5))
    $dirtyNamePattern = 'scene_name_rename result=success name=' + [Regex]::Escape($dirtySceneName) + '\.'
    if (-not (Wait-FileContains -Path $Context.StdoutPath -Pattern $dirtyNamePattern -TimeoutMilliseconds 10000)) {
        throw "The real Scene Name control did not publish the deliberately unsaved pre-Open edit."
    }

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
    $emptyOpenFormPattern = New-HenkaScenePathEditorStatePattern `
        -Mode 'open' `
        -RelativePath ''
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($openButton.X + ($openButton.Width * 0.5)) -Y ($openButton.Y + ($openButton.Height * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $emptyOpenFormPattern -StartingOffset $openFormOffset -TimeoutMilliseconds 10000)) {
        throw "The real Open Scene control did not open its project-relative path editor."
    }
    $pathControls = Get-SceneOpenControls -Path $Context.StdoutPath
    if ($null -eq $pathControls -or [int]$pathControls.Groups['relative_path_empty'].Value -ne 1) {
        throw "The Open Scene path field, Open, or Cancel control was not reported."
    }
    $initialCatalog = Get-SceneOpenCatalogControls -Path $Context.StdoutPath
    if ($null -eq $initialCatalog -or $initialCatalog.Result -ne 0 -or
        $initialCatalog.Stale -ne 0 -or $initialCatalog.Page -ne 0 -or
        $initialCatalog.Count -lt 4 -or $initialCatalog.PageCount -lt 2) {
        throw 'The real Open Scene catalog did not expose at least two current, populated pages.'
    }
    $initialPagePaths = @($initialCatalog.Rows | ForEach-Object { $_.Path })
    foreach ($fixtureName in $fixtureNames) {
        if ($initialPagePaths -notcontains $fixtureName) {
            throw "The first product catalog page did not expose its fixture scene: $fixtureName"
        }
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

    $openRelativePath = 'sandbox3d_scene.hscene'
    $formScreenshot = Join-Path $Context.RuntimeDirectory "open-scene-form.png"
    $null = Save-HenkaProductFramebufferScreenshot -Process $Context.Process -StdoutPath $Context.StdoutPath -CaptureDirectory $Context.FrameCaptureDirectory -RequestFile $Context.FrameCaptureRequestPath -Description "Packaged Open Scene catalog" -DestinationPath $formScreenshot
    $catalogSelection = Invoke-SceneOpenCatalogSelection `
        -Context $Context `
        -RelativePath $openRelativePath `
        -Panel $panel
    $catalogScreenshot = Join-Path $Context.RuntimeDirectory "open-scene-catalog-selection.png"
    $null = Save-HenkaProductFramebufferScreenshot -Process $Context.Process -StdoutPath $Context.StdoutPath -CaptureDirectory $Context.FrameCaptureDirectory -RequestFile $Context.FrameCaptureRequestPath -Description "Packaged Open Scene selected catalog entry" -DestinationPath $catalogScreenshot
    $pathControls = Get-SceneOpenControls -Path $Context.StdoutPath
    if ($null -eq $pathControls) {
        throw "Current product-owned Open Scene geometry was unavailable after selecting a catalog row."
    }
    if ($catalogSelection.RelativePathEmpty -ne 0 -or
        $catalogSelection.RelativePath -ne $openRelativePath) {
        throw "The selected catalog entry did not populate the Open Scene path field."
    }
    if ($catalogSelection.Page -ne 1 -or $catalogSelection.PageCount -lt 2) {
        throw 'The target packaged scene was not selected from the product-reachable second catalog page.'
    }
    $returnedToFirstPage = Invoke-SceneOpenCatalogPage `
        -Context $Context `
        -Panel $panel `
        -Direction 'previous'
    if ($returnedToFirstPage.Page -ne 0 -or
        $returnedToFirstPage.RelativePath -ne $openRelativePath) {
        throw 'The real Open Scene Previous control did not return to page one while preserving the selected path.'
    }
    $returnedToSecondPage = Invoke-SceneOpenCatalogPage `
        -Context $Context `
        -Panel $panel `
        -Direction 'next'
    if ($returnedToSecondPage.Page -ne 1 -or
        $returnedToSecondPage.RelativePath -ne $openRelativePath) {
        throw 'The real Open Scene Next control did not restore page two while preserving the selected path.'
    }
    Write-Output '[pass] Packaged Open Scene traversed Previous and Next through live product rectangles on a multi-page catalog.'
    $openRect = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $pathControls.Groups['open_x'].Value
        Y = Convert-HenkaAutomationNumber $pathControls.Groups['open_y'].Value
        Width = Convert-HenkaAutomationNumber $pathControls.Groups['open_width'].Value
        Height = Convert-HenkaAutomationNumber $pathControls.Groups['open_height'].Value
    }
    $pendingPattern = 'open_scene_confirmation_pending relative_path=' + [Regex]::Escape($openRelativePath) + '\.'
    $pendingOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($openRect.X + ($openRect.Width * 0.5)) -Y ($openRect.Y + ($openRect.Height * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $pendingPattern -StartingOffset $pendingOffset -TimeoutMilliseconds 10000) -or
        -not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'scene_transition_dialog available=1 action=open' -StartingOffset $pendingOffset -TimeoutMilliseconds 10000)) {
        throw "The Open button did not request confirmation for the entered scene."
    }
    $confirmation = Get-SceneTransitionDialogControls -Path $Context.StdoutPath -Action 'open'
    if ($null -eq $confirmation) {
        throw "Open Scene confirmation did not expose product-owned Cancel and Open hit regions."
    }
    $cancel = [pscustomobject]@{ X = Convert-HenkaAutomationNumber $confirmation.Groups['cancel_x'].Value; Y = Convert-HenkaAutomationNumber $confirmation.Groups['cancel_y'].Value; Width = Convert-HenkaAutomationNumber $confirmation.Groups['cancel_width'].Value; Height = Convert-HenkaAutomationNumber $confirmation.Groups['cancel_height'].Value }
    $confirm = [pscustomobject]@{ X = Convert-HenkaAutomationNumber $confirmation.Groups['discard_x'].Value; Y = Convert-HenkaAutomationNumber $confirmation.Groups['discard_y'].Value; Width = Convert-HenkaAutomationNumber $confirmation.Groups['discard_width'].Value; Height = Convert-HenkaAutomationNumber $confirmation.Groups['discard_height'].Value }
    $contentWidth = Convert-HenkaAutomationNumber $confirmation.Groups['content_width'].Value
    $contentHeight = Convert-HenkaAutomationNumber $confirmation.Groups['content_height'].Value
    foreach ($rect in @($cancel, $confirm)) {
        if ($rect.Width -le 0.0 -or $rect.Height -le 0.0 -or $rect.X -lt -0.5 -or $rect.Y -lt -0.5 -or ($rect.X + $rect.Width) -gt ($contentWidth + 0.5) -or ($rect.Y + $rect.Height) -gt ($contentHeight + 0.5)) {
            throw "An Open Scene confirmation hit region escaped the visible application content."
        }
    }
    $dialogState = [pscustomobject]@{
        Name = $confirmation.Groups['active_name'].Value
        Path = $confirmation.Groups['active_path'].Value
        ObjectCount = $confirmation.Groups['object_count'].Value
        SelectedEntity = $confirmation.Groups['selected_entity'].Value
        SceneDirty = $confirmation.Groups['scene_dirty'].Value
        TerrainDirtyRegions = $confirmation.Groups['terrain_dirty_regions'].Value
        DirtyQueryResult = $confirmation.Groups['scene_dirty_query_result'].Value
        TerrainDirtyQueryResult = $confirmation.Groups['terrain_dirty_query_result'].Value
    }
    $null = Assert-HenkaStructuredState `
        -Description 'Open Scene dirty-document test precondition' `
        -Actual $dialogState `
        -MismatchClassification 'TEST_PRECONDITION_UNMET' `
        -Expected @{
            Name = $dirtySceneName
            SceneDirty = '1'
            DirtyQueryResult = '0'
            TerrainDirtyQueryResult = '0'
        }
    $cancelOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($cancel.X + ($cancel.Width * 0.5)) -Y ($cancel.Y + ($cancel.Height * 0.5))
    $cancelledDecision = Get-HenkaSceneTransitionDecisionAfterOffset -Path $Context.StdoutPath -Action 'open' -Choice 'cancel' -StartingOffset $cancelOffset -TimeoutMilliseconds 10000
    $null = Assert-HenkaStructuredState `
        -Description 'Cancel Open Scene preserves the exact prior document and selection' `
        -Actual $cancelledDecision `
        -Expected @{
            SceneRetained = '1'
            SceneDirty = $dialogState.SceneDirty
            TerrainDirtyRegions = $dialogState.TerrainDirtyRegions
            ActiveName = $dialogState.Name
            ActivePath = $dialogState.Path
            ObjectCount = $dialogState.ObjectCount
            SelectedEntity = $dialogState.SelectedEntity
        }
    $retryOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    $pathControls = Get-SceneOpenControls -Path $Context.StdoutPath
    if ($null -eq $pathControls) {
        throw "Product-owned Open Scene geometry was unavailable after canceling the confirmation."
    }
    $retryOpenRect = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $pathControls.Groups['open_x'].Value
        Y = Convert-HenkaAutomationNumber $pathControls.Groups['open_y'].Value
        Width = Convert-HenkaAutomationNumber $pathControls.Groups['open_width'].Value
        Height = Convert-HenkaAutomationNumber $pathControls.Groups['open_height'].Value
    }
    $retryPanel = Get-DetailsPanelGeometry -Path $Context.StdoutPath
    if ($null -eq $retryPanel) { throw "Current Object Details bounds were unavailable before retrying Open Scene." }
    $retryPanelRect = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $retryPanel.Groups['x'].Value
        Y = Convert-HenkaAutomationNumber $retryPanel.Groups['y'].Value
        Width = Convert-HenkaAutomationNumber $retryPanel.Groups['width'].Value
        Height = Convert-HenkaAutomationNumber $retryPanel.Groups['height'].Value
    }
    if ($retryOpenRect.Width -le 0.0 -or $retryOpenRect.Height -le 0.0 -or
        $retryOpenRect.X -lt ($retryPanelRect.X - 0.5) -or $retryOpenRect.Y -lt ($retryPanelRect.Y - 0.5) -or
        ($retryOpenRect.X + $retryOpenRect.Width) -gt ($retryPanelRect.X + $retryPanelRect.Width + 0.5) -or
        ($retryOpenRect.Y + $retryOpenRect.Height) -gt ($retryPanelRect.Y + $retryPanelRect.Height + 0.5)) {
        throw "The refreshed Open Scene retry hit region escaped visible Object Details."
    }
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($retryOpenRect.X + ($retryOpenRect.Width * 0.5)) -Y ($retryOpenRect.Y + ($retryOpenRect.Height * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $pendingPattern -StartingOffset $retryOffset -TimeoutMilliseconds 10000) -or
        -not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'scene_transition_dialog available=1 action=open' -StartingOffset $retryOffset -TimeoutMilliseconds 10000)) {
        throw "Canceling confirmation did not retain the entered path for retry."
    }
    $confirmation = Get-SceneTransitionDialogControls -Path $Context.StdoutPath -Action 'open'
    if ($null -eq $confirmation) {
        throw "The retried Open Scene confirmation did not expose its controls."
    }
    $confirmX = Convert-HenkaAutomationNumber $confirmation.Groups['discard_x'].Value
    $confirmY = Convert-HenkaAutomationNumber $confirmation.Groups['discard_y'].Value
    $confirmWidth = Convert-HenkaAutomationNumber $confirmation.Groups['discard_width'].Value
    $confirmHeight = Convert-HenkaAutomationNumber $confirmation.Groups['discard_height'].Value
    $openAttemptOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($confirmX + ($confirmWidth * 0.5)) -Y ($confirmY + ($confirmHeight * 0.5))
    $openedAttempt = Get-HenkaSceneOpenAttemptAfterOffset -Path $Context.StdoutPath -StartingOffset $openAttemptOffset -TimeoutMilliseconds 15000
    $null = Assert-HenkaStructuredState `
        -Description 'Open Scene active-document result after the real confirmation click' `
        -Actual $openedAttempt `
        -Expected @{
            Result = '0'
            RelativePathEmpty = '0'
            ActiveName = $Context.SceneName
            ActiveRelativePath = $openRelativePath
        }
    if ([int]$openedAttempt.ActiveDocumentObjectCount -le 0) {
        throw "[HENKA_VALIDATION_CLASS=PRODUCT_STATE_MISMATCH] Open Scene reported an empty active document after loading the saved scene."
    }
    if ($openedAttempt.SelectionPresent -notin @('0', '1')) {
        throw "[HENKA_VALIDATION_CLASS=UNCLASSIFIED_OBSERVATION_GAP] Open Scene reported an invalid selection-presence value."
    }
    $openedDocument = Get-HenkaSceneOpenedAfterOffset -Path $Context.StdoutPath -StartingOffset $openAttemptOffset -TimeoutMilliseconds 10000
    $null = Assert-HenkaStructuredState `
        -Description 'Open Scene completion diagnostic agrees with the active document' `
        -Actual $openedDocument `
        -Expected @{
            Action = 'scene_opened'
            Name = $openedAttempt.ActiveName
            RelativePath = $openedAttempt.ActiveRelativePath
            RelativePathEmpty = '0'
            DocumentObjectCount = $openedAttempt.ActiveDocumentObjectCount
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
    # Use the CLR type name because the [ulong] accelerator is not available
    # consistently in the Windows PowerShell 5.1 validation host.
    $rowEntity = [System.UInt64]::Parse($row.Groups['entity'].Value)
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
    $cameraTargetEntity = [System.UInt64]::Parse($newSelectedRow.Groups['entity'].Value)
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

    $expectedLiveCount = [int]$openedAttempt.ActiveDocumentObjectCount + 1
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
    $actions = Get-SceneDocumentActionControls -Path $Context.StdoutPath
    $panelMatch = Get-DetailsPanelGeometry -Path $Context.StdoutPath
    if ($null -eq $actions -or $null -eq $panelMatch -or
        $actions.Groups['relative_path_empty'].Value -ne '0') {
        throw "The invalid-path negative control requires a saved active scene."
    }
    $savePanel = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $panelMatch.Groups['x'].Value
        Y = Convert-HenkaAutomationNumber $panelMatch.Groups['y'].Value
        Width = Convert-HenkaAutomationNumber $panelMatch.Groups['width'].Value
        Height = Convert-HenkaAutomationNumber $panelMatch.Groups['height'].Value
    }
    $saveButton = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $actions.Groups['save_x'].Value
        Y = Convert-HenkaAutomationNumber $actions.Groups['save_y'].Value
        Width = Convert-HenkaAutomationNumber $actions.Groups['save_width'].Value
        Height = Convert-HenkaAutomationNumber $actions.Groups['save_height'].Value
    }
    $sceneHashBeforeFixtureSave = (Get-FileHash -LiteralPath $Context.SavedScenePath -Algorithm SHA256).Hash
    $fixtureSaveOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($saveButton.X + ($saveButton.Width * 0.5)) -Y ($saveButton.Y + ($saveButton.Height * 0.5))
    $fixtureSavedPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=saved name=' + [Regex]::Escape($Context.SceneName) + '\.'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $fixtureSavedPattern -StartingOffset $fixtureSaveOffset -TimeoutMilliseconds 15000)) {
        throw "The active edited scene was not saved through the real Save control before the clean-scene invalid-path check."
    }
    $Context.SavedSceneHash = (Get-FileHash -LiteralPath $Context.SavedScenePath -Algorithm SHA256).Hash
    if ($Context.SavedSceneHash -eq $sceneHashBeforeFixtureSave) {
        throw "Saving the product-authored Cube did not update the saved scene fixture."
    }
    if (-not (Test-Path -LiteralPath $Context.ProjectManifestPath -PathType Leaf)) {
        throw "The successful fixture Save did not leave the project startup-scene manifest available."
    }
    $fixtureManifestText = Get-Content -LiteralPath $Context.ProjectManifestPath -Raw
    $fixtureStartupScene = [Regex]::Match($fixtureManifestText, '(?m)^startup_scene=(?<path>[^\r\n]+)$')
    if (-not $fixtureStartupScene.Success -or
        $fixtureStartupScene.Groups['path'].Value -ne $openRelativePath) {
        throw "The fixture Save did not establish the expected startup scene '$openRelativePath' before the invalid Open negative control."
    }
    $Context.ProjectManifestHash = (Get-FileHash -LiteralPath $Context.ProjectManifestPath -Algorithm SHA256).Hash

    $actions = Get-SceneDocumentActionControls -Path $Context.StdoutPath
    $panelMatch = Get-DetailsPanelGeometry -Path $Context.StdoutPath
    if ($null -eq $actions -or $null -eq $panelMatch) {
        throw "Fresh product geometry was unavailable before opening the invalid-path form."
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
        throw "The fresh Open Scene hit region escaped visible Object Details."
    }
    $invalidFormOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($openButton.X + ($openButton.Width * 0.5)) -Y ($openButton.Y + ($openButton.Height * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $emptyOpenFormPattern -StartingOffset $invalidFormOffset -TimeoutMilliseconds 10000)) {
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
    $invalidTextOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationText -EventPath $Context.AutomationInputPath -Text $invalidPath
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'input record=\d+ type=text' -StartingOffset $invalidTextOffset -TimeoutMilliseconds 10000)) {
        throw "The invalid Open Scene path text was not consumed by the product."
    }
    $pathControls = Get-SceneOpenControls -Path $Context.StdoutPath
    if ($null -eq $pathControls) {
        throw "Fresh Open Scene path geometry was unavailable after entering the invalid path."
    }
    $openRect = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $pathControls.Groups['open_x'].Value
        Y = Convert-HenkaAutomationNumber $pathControls.Groups['open_y'].Value
        Width = Convert-HenkaAutomationNumber $pathControls.Groups['open_width'].Value
        Height = Convert-HenkaAutomationNumber $pathControls.Groups['open_height'].Value
    }
    $invalidPendingOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($openRect.X + ($openRect.Width * 0.5)) -Y ($openRect.Y + ($openRect.Height * 0.5))
    $rejected = Get-HenkaSceneOpenAttemptAfterOffset -Path $Context.StdoutPath -StartingOffset $invalidPendingOffset -TimeoutMilliseconds 15000
    if ($null -eq $rejected) {
        throw "The invalid Open request did not publish a complete product-owned scene-open result after the current action boundary."
    }
    if (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'scene_transition_prompt_pending action=open' -StartingOffset $invalidPendingOffset -TimeoutMilliseconds 100) {
        throw "A clean scene incorrectly received an unsaved-change prompt for a rejected Open path."
    }
    if ([int]$rejected.Result -eq 0 -or
        $rejected.RelativePathEmpty -ne '0' -or
        $rejected.ActiveName -ne $Context.SceneName -or
        $rejected.ActiveRelativePath -ne $openRelativePath -or
        [int]$rejected.ActiveDocumentObjectCount -ne $loadedCount -or
        $rejected.SelectionPresent -ne $loadedSelection) {
        throw "Rejected Open changed authoritative active scene state: result=$($rejected.Result) path_empty=$($rejected.RelativePathEmpty) name='$($rejected.ActiveName)' path='$($rejected.ActiveRelativePath)' objects=$($rejected.ActiveDocumentObjectCount) selection=$($rejected.SelectionPresent); expected the pre-operation scene/document/selection state to remain unchanged."
    }
    $invalidTargetPath = [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $Context.SavedScenePath) '..\escape.hscene'))
    if (Test-Path -LiteralPath $invalidTargetPath) {
        throw "Rejected Open created an out-of-project scene file at '$invalidTargetPath'."
    }
    $actualSavedSceneHash = (Get-FileHash -LiteralPath $Context.SavedScenePath -Algorithm SHA256).Hash
    if ($actualSavedSceneHash -ne $Context.SavedSceneHash) {
        throw "Rejected Open modified the saved source scene (expected SHA256 $($Context.SavedSceneHash), actual $actualSavedSceneHash)."
    }
    $actualProjectManifestHash = (Get-FileHash -LiteralPath $Context.ProjectManifestPath -Algorithm SHA256).Hash
    if ($actualProjectManifestHash -ne $Context.ProjectManifestHash) {
        throw "Rejected Open modified the startup manifest (expected SHA256 $($Context.ProjectManifestHash), actual $actualProjectManifestHash)."
    }
    $failureScreenshot = Join-Path $Context.RuntimeDirectory "open-scene-rejected.png"
    $null = Save-HenkaProductFramebufferScreenshot -Process $Context.Process -StdoutPath $Context.StdoutPath -CaptureDirectory $Context.FrameCaptureDirectory -RequestFile $Context.FrameCaptureRequestPath -Description "Packaged rejected Open Scene path" -DestinationPath $failureScreenshot
    $invalidCancelOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    $pathControls = Get-SceneOpenControls -Path $Context.StdoutPath
    if ($null -eq $pathControls) {
        throw "Fresh Open Scene path geometry was unavailable before canceling the rejected path."
    }
    $cancelRect = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $pathControls.Groups['cancel_x'].Value
        Y = Convert-HenkaAutomationNumber $pathControls.Groups['cancel_y'].Value
        Width = Convert-HenkaAutomationNumber $pathControls.Groups['cancel_width'].Value
        Height = Convert-HenkaAutomationNumber $pathControls.Groups['cancel_height'].Value
    }
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($cancelRect.X + ($cancelRect.Width * 0.5)) -Y ($cancelRect.Y + ($cancelRect.Height * 0.5))
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'scene_document_action_controls visible=1 ' -StartingOffset $invalidCancelOffset -TimeoutMilliseconds 10000)) {
        throw "Canceling the rejected Open path did not return to the current scene controls."
    }

    $closeDraftName = 'Close Draft'
    # Scene Name geometry is published only when its bounds change. Closing the
    # path overlay restores the scene action controls, but the unchanged name
    # rectangle is intentionally not emitted again. Read its latest
    # product-owned geometry immediately before the next real interaction.
    $closeNameControls = Get-SceneNameControls -Path $Context.StdoutPath
    if ($null -eq $closeNameControls) {
        throw "The loaded scene name control was unavailable for the close-protection workflow."
    }
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ((Convert-HenkaAutomationNumber $closeNameControls.Groups['field_x'].Value) + ((Convert-HenkaAutomationNumber $closeNameControls.Groups['field_width'].Value) * 0.5)) -Y ((Convert-HenkaAutomationNumber $closeNameControls.Groups['field_y'].Value) + ((Convert-HenkaAutomationNumber $closeNameControls.Groups['field_height'].Value) * 0.5))
    Clear-HenkaTextField -EventPath $Context.AutomationInputPath
    $closeDraftInputOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationText -EventPath $Context.AutomationInputPath -Text $closeDraftName
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern 'input record=\d+ type=text' -StartingOffset $closeDraftInputOffset -TimeoutMilliseconds 10000)) {
        throw "The product did not consume the deliberate close-test name before the Rename action."
    }
    $closeNameControls = Get-SceneNameControls -Path $Context.StdoutPath
    if ($null -eq $closeNameControls) {
        throw "Fresh product-owned Scene Name geometry was unavailable immediately before the close-test Rename."
    }
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ((Convert-HenkaAutomationNumber $closeNameControls.Groups['rename_x'].Value) + ((Convert-HenkaAutomationNumber $closeNameControls.Groups['rename_width'].Value) * 0.5)) -Y ((Convert-HenkaAutomationNumber $closeNameControls.Groups['rename_y'].Value) + ((Convert-HenkaAutomationNumber $closeNameControls.Groups['rename_height'].Value) * 0.5))
    $closeDraftPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_name_rename result=success name=' + [Regex]::Escape($closeDraftName) + '\.'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $closeDraftPattern -StartingOffset $invalidCancelOffset -TimeoutMilliseconds 10000)) {
        throw "The real Scene Name control did not publish the deliberate unsaved close-test edit."
    }
    $savedSceneHashBeforeClose = (Get-FileHash -LiteralPath $Context.SavedScenePath -Algorithm SHA256).Hash

    $closeRequestOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    if (-not $Context.Process.CloseMainWindow()) {
        throw "Windows did not deliver the normal close request to the packaged Henka window."
    }
    $closePendingPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=close_confirmation_pending name=' + [Regex]::Escape($closeDraftName) + '\.'
    $closeDirtyPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_prompt_pending action=close dirty=1 scene_dirty=1 scene_dirty_query_result=0 terrain_dirty_query_result=0 terrain_dirty_regions=0 target_path=' + [Regex]::Escape($openRelativePath) + '\.'
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $closePendingPattern -StartingOffset $closeRequestOffset -TimeoutMilliseconds 10000) -or
        -not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $closeDirtyPattern -StartingOffset $closeRequestOffset -TimeoutMilliseconds 10000)) {
        throw "The normal window-close message did not produce the expected dirty-scene protection state."
    }
    $closeDialog = Get-SceneTransitionDialogControls -Path $Context.StdoutPath -Action 'close'
    if ($null -eq $closeDialog -or $closeDialog.Groups['active_name'].Value -ne $closeDraftName -or
        $closeDialog.Groups['active_path'].Value -ne $openRelativePath -or
        $closeDialog.Groups['scene_dirty'].Value -ne '1' -or
        [int]$closeDialog.Groups['terrain_dirty_regions'].Value -ne 0) {
        throw "The close prompt did not expose the current canonical scene and separate dirty-state authorities."
    }
    $closeContentWidth = [int]$closeDialog.Groups['content_width'].Value
    $closeContentHeight = [int]$closeDialog.Groups['content_height'].Value
    $closeCancel = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $closeDialog.Groups['cancel_x'].Value
        Y = Convert-HenkaAutomationNumber $closeDialog.Groups['cancel_y'].Value
        Width = Convert-HenkaAutomationNumber $closeDialog.Groups['cancel_width'].Value
        Height = Convert-HenkaAutomationNumber $closeDialog.Groups['cancel_height'].Value
    }
    if ($closeCancel.Width -le 0.0 -or $closeCancel.Height -le 0.0 -or
        $closeCancel.X -lt -0.5 -or $closeCancel.Y -lt -0.5 -or
        ($closeCancel.X + $closeCancel.Width) -gt ($closeContentWidth + 0.5) -or
        ($closeCancel.Y + $closeCancel.Height) -gt ($closeContentHeight + 0.5)) {
        throw "The close dialog Cancel target escaped the visible application content."
    }
    $closePromptScreenshot = Join-Path $Context.RuntimeDirectory 'close-unsaved-scene.png'
    $null = Save-HenkaProductFramebufferScreenshot -Process $Context.Process -StdoutPath $Context.StdoutPath -CaptureDirectory $Context.FrameCaptureDirectory -RequestFile $Context.FrameCaptureRequestPath -Description 'Packaged close unsaved-scene prompt' -DestinationPath $closePromptScreenshot
    $closeCancelOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaAutomationClickAt -EventPath $Context.AutomationInputPath -X ($closeCancel.X + ($closeCancel.Width * 0.5)) -Y ($closeCancel.Y + ($closeCancel.Height * 0.5))
    $closeCancelledDecision = Get-HenkaSceneTransitionDecisionAfterOffset -Path $Context.StdoutPath -Action 'close' -Choice 'cancel' -StartingOffset $closeCancelOffset -TimeoutMilliseconds 10000
    if ($null -eq $closeCancelledDecision -or
        $closeCancelledDecision.SceneRetained -ne '1' -or
        $closeCancelledDecision.SceneDirty -ne $closeDialog.Groups['scene_dirty'].Value -or
        $closeCancelledDecision.TerrainDirtyRegions -ne $closeDialog.Groups['terrain_dirty_regions'].Value -or
        $closeCancelledDecision.ActiveName -ne $closeDraftName -or
        $closeCancelledDecision.ActivePath -ne $openRelativePath -or
        $closeCancelledDecision.ObjectCount -ne $closeDialog.Groups['object_count'].Value -or
        $closeCancelledDecision.SelectedEntity -ne $closeDialog.Groups['selected_entity'].Value) {
        throw "Canceling the window-close prompt did not retain the same scene identity and selection."
    }
    $Context.Process.Refresh()
    if ($Context.Process.HasExited) {
        throw "Canceling the window-close prompt still exited the packaged editor."
    }

    if ($loadedCount -lt 1 -or $loadedSelection -ne '1') {
        throw "[FIXTURE/PRECONDITION] Close Save while Play requires the already-proven opened scene with its authored Cube selected."
    }
    $expectedPlayEntity = [string]$closeCancelledDecision.SelectedEntity
    if ($expectedPlayEntity -notmatch '^\d+$' -or $expectedPlayEntity -eq '0') {
        throw "[FIXTURE/PRECONDITION] The retained scene did not report one canonical selected entity for the Play workflow."
    }
    $physicsDisclosurePattern = 'HENKA_AUTOMATION_DIAGNOSTIC game_authoring_physics_disclosure [^\r\n]*\.'
    $playControlsPattern = 'HENKA_AUTOMATION_DIAGNOSTIC game_authoring_play_controls [^\r\n]*\.'
    $playControls = $null
    $detailsForPlay = $null
    $playControlsFresh = $false
    $physicsDisclosureOpened = $false
    for ($navigation = 0; $navigation -lt 24; ++$navigation) {
        $physicsDisclosure = Get-GameAuthoringPhysicsDisclosureControls -Path $Context.StdoutPath
        if ($null -eq $physicsDisclosure -or $null -eq $physicsDisclosure.Fields) {
            throw "[HARNESS/OBSERVATION] The product did not report current Physics disclosure state and geometry."
        }
        if ($physicsDisclosure.Fields.entity_id -ne $expectedPlayEntity) {
            throw "[PRODUCT/STATE] Physics disclosure telemetry belongs to entity $($physicsDisclosure.Fields.entity_id), not retained selection $expectedPlayEntity."
        }
        if ($physicsDisclosure.Fields.expanded -eq '1') {
            $physicsDisclosureOpened = $true
        }
        $detailsForPlay = Get-DetailsPanelGeometry -Path $Context.StdoutPath
        if ($null -eq $detailsForPlay) {
            throw "[HARNESS/OBSERVATION] Object Details geometry was unavailable while opening Physics and locating Play."
        }
        $detailsRect = [pscustomobject]@{
            X = Convert-HenkaAutomationNumber $detailsForPlay.Groups['x'].Value
            Y = Convert-HenkaAutomationNumber $detailsForPlay.Groups['y'].Value
            Width = Convert-HenkaAutomationNumber $detailsForPlay.Groups['width'].Value
            Height = Convert-HenkaAutomationNumber $detailsForPlay.Groups['height'].Value
        }
        $physicsRect = [pscustomobject]@{
            X = Convert-HenkaAutomationNumber $physicsDisclosure.Groups['disclosure_x'].Value
            Y = Convert-HenkaAutomationNumber $physicsDisclosure.Groups['disclosure_y'].Value
            Width = Convert-HenkaAutomationNumber $physicsDisclosure.Groups['disclosure_width'].Value
            Height = Convert-HenkaAutomationNumber $physicsDisclosure.Groups['disclosure_height'].Value
        }
        $physicsRectVisible = $physicsRect.Width -gt 0.0 -and $physicsRect.Height -gt 0.0 -and
            $physicsRect.X -ge ($detailsRect.X - 0.5) -and
            $physicsRect.Y -ge ($detailsRect.Y - 0.5) -and
            ($physicsRect.X + $physicsRect.Width) -le ($detailsRect.X + $detailsRect.Width + 0.5) -and
            ($physicsRect.Y + $physicsRect.Height) -le ($detailsRect.Y + $detailsRect.Height + 0.5)

        if ($physicsDisclosure.Fields.expanded -eq '0' -and $physicsRectVisible) {
            $physicsToggleOffset = Get-FileLengthSafe -Path $Context.StdoutPath
            Send-HenkaObservedAutomationClick `
                -EventPath $Context.AutomationInputPath `
                -StdoutPath $Context.StdoutPath `
                -X ($physicsRect.X + ($physicsRect.Width * 0.5)) `
                -Y ($physicsRect.Y + ($physicsRect.Height * 0.5)) `
                -Purpose 'Game Authoring Physics disclosure' | Out-Null
            if (-not (Wait-FileContainsAfterOffset `
                    -Path $Context.StdoutPath `
                    -Pattern $physicsDisclosurePattern `
                    -StartingOffset $physicsToggleOffset `
                    -TimeoutMilliseconds 5000)) {
                throw "[HARNESS/OBSERVATION] The Physics click was consumed, but the product did not report the disclosure state transition."
            }
            $physicsAfterClick = Get-GameAuthoringPhysicsDisclosureControls -Path $Context.StdoutPath
            if ($null -eq $physicsAfterClick -or
                $physicsAfterClick.Fields.entity_id -ne $expectedPlayEntity -or
                $physicsAfterClick.Fields.expanded -ne '1') {
                throw "[PRODUCT/UI] The real Physics disclosure click did not expand Physics for the retained selected entity."
            }
            $physicsDisclosureOpened = $true
            if (Wait-FileContainsAfterOffset `
                    -Path $Context.StdoutPath `
                    -Pattern $playControlsPattern `
                    -StartingOffset $physicsToggleOffset `
                    -TimeoutMilliseconds 250) {
                $playControlsFresh = $true
            }
            Write-Output "[pass] Expanded Physics for selected entity $expectedPlayEntity through its current product hit rectangle."
            continue
        }

        $playControls = Get-GameAuthoringPlayControls -Path $Context.StdoutPath
        if ($playControlsFresh -and $null -ne $playControls -and $null -ne $playControls.Fields) {
            if ($playControls.Fields.entity_id -ne $expectedPlayEntity -or
                $playControls.Fields.play_state -ne '0') {
                throw "[PRODUCT/STATE] The visible Play controls did not report the retained selected entity in the stopped state."
            }
            $playRect = [pscustomobject]@{
                X = Convert-HenkaAutomationNumber $playControls.Groups['play_x'].Value
                Y = Convert-HenkaAutomationNumber $playControls.Groups['play_y'].Value
                Width = Convert-HenkaAutomationNumber $playControls.Groups['play_width'].Value
                Height = Convert-HenkaAutomationNumber $playControls.Groups['play_height'].Value
            }
            $playRectVisible = $playRect.Width -gt 0.0 -and $playRect.Height -gt 0.0 -and
                $playRect.X -ge ($detailsRect.X - 0.5) -and
                $playRect.Y -ge ($detailsRect.Y - 0.5) -and
                ($playRect.X + $playRect.Width) -le ($detailsRect.X + $detailsRect.Width + 0.5) -and
                ($playRect.Y + $playRect.Height) -le ($detailsRect.Y + $detailsRect.Height + 0.5)
            if ($playRectVisible) {
                break
            }
        }

        $scrollOffset = Get-FileLengthSafe -Path $Context.StdoutPath
        $detailsForPlayScrollX = $detailsRect.X + ($detailsRect.Width * 0.5)
        $detailsForPlayScrollY = $detailsRect.Y + ($detailsRect.Height * 0.5)
        Send-HenkaAutomationScroll `
            -EventPath $Context.AutomationInputPath `
            -X $detailsForPlayScrollX `
            -Y $detailsForPlayScrollY `
            -WheelDelta -1.0
        if (-not (Wait-FileContainsAfterOffset `
                -Path $Context.StdoutPath `
                -Pattern $detailsScrollPattern `
                -StartingOffset $scrollOffset `
                -TimeoutMilliseconds 5000)) {
            throw "[HARNESS/OBSERVATION] The product did not report consumption and result of the bounded Details scroll."
        }
        $scrollReport = Get-LastLogRegexMatch `
            -Path $Context.StdoutPath `
            -Pattern $detailsScrollPattern
        if ($null -eq $scrollReport -or $scrollReport.Groups['handled'].Value -ne '1') {
            throw "[PRODUCT/UI] Object Details did not own the scroll input needed to reach Physics or Play."
        }
        $scrollBefore = Convert-HenkaAutomationNumber $scrollReport.Groups['before'].Value
        $scrollAfter = Convert-HenkaAutomationNumber $scrollReport.Groups['after'].Value
        if ($scrollAfter -le $scrollBefore) {
            throw "[PRODUCT/UI] Object Details reached scroll offset $scrollAfter without progress toward the Physics disclosure or Play controls."
        }
        if (-not (Wait-FileContainsAfterOffset `
                -Path $Context.StdoutPath `
                -Pattern $physicsDisclosurePattern `
                -StartingOffset $scrollOffset `
                -TimeoutMilliseconds 3000)) {
            throw "[HARNESS/OBSERVATION] The Details scroll progressed, but the product did not refresh Physics disclosure geometry."
        }
        if (Wait-FileContainsAfterOffset `
                -Path $Context.StdoutPath `
                -Pattern $playControlsPattern `
                -StartingOffset $scrollOffset `
                -TimeoutMilliseconds 250) {
            $playControlsFresh = $true
        }
    }
    if ($null -eq $playControls -or -not $playControlsFresh -or -not $physicsDisclosureOpened) {
        $physicsState = if ($null -eq $physicsDisclosure) { 'unreported' } else { $physicsDisclosure.Fields.expanded }
        throw "[PRODUCT/UI] Bounded product navigation did not expose fresh Play controls for entity $expectedPlayEntity (Physics expanded=$physicsState)."
    }
    $detailsForPlay = Get-DetailsPanelGeometry -Path $Context.StdoutPath
    if ($null -eq $detailsForPlay) {
        throw "[HARNESS/OBSERVATION] Current Object Details geometry was unavailable immediately before starting Play."
    }
    $playControls = Get-GameAuthoringPlayControls -Path $Context.StdoutPath
    if ($null -eq $playControls -or $null -eq $playControls.Fields) {
        throw "[HARNESS/OBSERVATION] Fresh Play control state and geometry were unavailable immediately before interaction."
    }
    if ($playControls.Fields.entity_id -notmatch '^\d+$' -or
        $playControls.Fields.play_state -ne '0') {
        throw "[PRODUCT/STATE] The selected authored object's Play state changed before the packaged interaction."
    }
    $playRect = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $playControls.Groups['play_x'].Value
        Y = Convert-HenkaAutomationNumber $playControls.Groups['play_y'].Value
        Width = Convert-HenkaAutomationNumber $playControls.Groups['play_width'].Value
        Height = Convert-HenkaAutomationNumber $playControls.Groups['play_height'].Value
    }
    $detailsRect = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $detailsForPlay.Groups['x'].Value
        Y = Convert-HenkaAutomationNumber $detailsForPlay.Groups['y'].Value
        Width = Convert-HenkaAutomationNumber $detailsForPlay.Groups['width'].Value
        Height = Convert-HenkaAutomationNumber $detailsForPlay.Groups['height'].Value
    }
    if ($playRect.Width -le 0.0 -or $playRect.Height -le 0.0 -or
        $playRect.X -lt ($detailsRect.X - 0.5) -or
        $playRect.Y -lt ($detailsRect.Y - 0.5) -or
        ($playRect.X + $playRect.Width) -gt ($detailsRect.X + $detailsRect.Width + 0.5) -or
        ($playRect.Y + $playRect.Height) -gt ($detailsRect.Y + $detailsRect.Height + 0.5)) {
        throw "[PRODUCT/UI] The selected object's Play hit target was not wholly inside visible Object Details."
    }
    $playStartOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaObservedAutomationClick `
        -EventPath $Context.AutomationInputPath `
        -StdoutPath $Context.StdoutPath `
        -X ($playRect.X + ($playRect.Width * 0.5)) `
        -Y ($playRect.Y + ($playRect.Height * 0.5)) `
        -Purpose 'Game Authoring Start Play' | Out-Null
    if (-not (Wait-FileContainsAfterOffset `
            -Path $Context.StdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC game_authoring_play_controls [^\r\n]*\.' `
            -StartingOffset $playStartOffset `
            -TimeoutMilliseconds 10000)) {
        throw "[HARNESS/OBSERVATION] The real Play click was consumed, but no post-action Play-state report was emitted; control activation and product reporting cannot be distinguished."
    }
    $runningPlayControls = Get-GameAuthoringPlayControls -Path $Context.StdoutPath
    if ($null -eq $runningPlayControls -or $null -eq $runningPlayControls.Fields) {
        throw "[HARNESS/OBSERVATION] The post-action Play report arrived but could not be parsed into structured state."
    }
    if ($runningPlayControls.Fields.entity_id -ne $playControls.Fields.entity_id -or
        $runningPlayControls.Fields.play_state -ne '1') {
        throw "[PRODUCT/STATE] The real Play control did not start the same selected authored entity."
    }
    Write-Output "[pass] Started Play for selected entity $($runningPlayControls.Fields.entity_id) through the real Object Details button."

    $saveCloseOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    if (-not $Context.Process.CloseMainWindow()) {
        throw "The second normal window-close request was not delivered."
    }
    if (-not (Wait-FileContainsAfterOffset -Path $Context.StdoutPath -Pattern $closePendingPattern -StartingOffset $saveCloseOffset -TimeoutMilliseconds 10000)) {
        throw "The second window-close request did not restore the unsaved-scene prompt."
    }
    $saveCloseDialog = Get-SceneTransitionDialogControls -Path $Context.StdoutPath -Action 'close'
    if ($null -eq $saveCloseDialog) {
        throw "[HARNESS/OBSERVATION] The close prompt did not expose parseable current Save geometry after cancellation."
    }
    $saveCloseContentWidth = [int]$saveCloseDialog.Groups['content_width'].Value
    $saveCloseContentHeight = [int]$saveCloseDialog.Groups['content_height'].Value
    $closeSave = [pscustomobject]@{
        X = Convert-HenkaAutomationNumber $saveCloseDialog.Groups['save_x'].Value
        Y = Convert-HenkaAutomationNumber $saveCloseDialog.Groups['save_y'].Value
        Width = Convert-HenkaAutomationNumber $saveCloseDialog.Groups['save_width'].Value
        Height = Convert-HenkaAutomationNumber $saveCloseDialog.Groups['save_height'].Value
    }
    if ($closeSave.Width -le 0.0 -or $closeSave.Height -le 0.0 -or
        $closeSave.X -lt -0.5 -or $closeSave.Y -lt -0.5 -or
        ($closeSave.X + $closeSave.Width) -gt ($saveCloseContentWidth + 0.5) -or
        ($closeSave.Y + $closeSave.Height) -gt ($saveCloseContentHeight + 0.5)) {
        throw "The close dialog Save target escaped the visible application content."
    }
    $closeSaveDecisionOffset = Get-FileLengthSafe -Path $Context.StdoutPath
    Send-HenkaObservedAutomationClick `
        -EventPath $Context.AutomationInputPath `
        -StdoutPath $Context.StdoutPath `
        -X ($closeSave.X + ($closeSave.Width * 0.5)) `
        -Y ($closeSave.Y + ($closeSave.Height * 0.5)) `
        -Purpose 'Close Save while Play is active' | Out-Null
    $closePlaySavePattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_close_play_save [^\r\n]*\.'
    if (-not (Wait-FileContainsAfterOffset `
            -Path $Context.StdoutPath `
            -Pattern $closePlaySavePattern `
            -StartingOffset $closeSaveDecisionOffset `
            -TimeoutMilliseconds 10000)) {
        throw "[HARNESS/OBSERVATION] The consumed Close Save click produced no Play-lock report, so the save/stop result cannot be classified."
    }
    $closePlaySaveLine = Get-LastLogRegexMatch -Path $Context.StdoutPath -Pattern $closePlaySavePattern
    if ($null -eq $closePlaySaveLine) {
        throw "[HARNESS/OBSERVATION] The Close Save state report disappeared after its readiness check."
    }
    $closePlaySave = ConvertFrom-HenkaClosePlaySaveLine -Line $closePlaySaveLine.Value
    if ($null -eq $closePlaySave) {
        throw "[HARNESS/OBSERVATION] The emitted Close Save Play-lock report could not be parsed into structured state."
    }
    if ([int]$closePlaySave.result -ne 0 -or
        $closePlaySave.play_locked_before -ne '1' -or
        $closePlaySave.play_locked_after -ne '0') {
        throw "[PRODUCT/STATE] Close Save did not stop Play before committing the save (result=$($closePlaySave.result), before=$($closePlaySave.play_locked_before), after=$($closePlaySave.play_locked_after))."
    }
    $terrainSave = Get-HenkaSceneTransitionTerrainSaveAfterOffset -Path $Context.StdoutPath -StartingOffset $closeSaveDecisionOffset -TimeoutMilliseconds 15000
    if ($null -eq $terrainSave) {
        throw "[HARNESS/OBSERVATION] Close Save emitted no parseable Terrain persistence report."
    }
    if ($terrainSave.Result -ne '0' -or
        $terrainSave.DirtyBefore -ne '0' -or
        $terrainSave.Saved -ne '0' -or
        $terrainSave.DirtyAfter -ne '0') {
        throw "[PRODUCT/STATE] Saving before close did not report the expected clean terrain persistence state."
    }
    $closedResult = Get-HenkaSceneTransitionResultAfterOffset -Path $Context.StdoutPath -Action 'close' -StartingOffset $closeSaveDecisionOffset -TimeoutMilliseconds 15000
    if ($null -eq $closedResult) {
        throw "[HARNESS/OBSERVATION] Close Save emitted no parseable completed scene-close report."
    }
    if ($closedResult.Result -ne '0' -or
        $closedResult.ActiveName -ne $closeDraftName -or
        $closedResult.ActivePath -ne $openRelativePath -or
        [int]$closedResult.ObjectCount -ne $loadedCount) {
        throw "[PRODUCT/STATE] Saving before close did not complete the normal editor-close transition."
    }
    if (-not $Context.Process.WaitForExit(15000) -or $Context.Process.ExitCode -ne 0) {
        throw "The packaged editor did not exit successfully after its saved close transition."
    }
    if ((Get-FileHash -LiteralPath $Context.SavedScenePath -Algorithm SHA256).Hash -eq $savedSceneHashBeforeClose) {
        throw "Close Save did not persist the deliberate Scene Name edit."
    }
    if (-not (Test-Path -LiteralPath $Context.ProjectManifestPath -PathType Leaf) -or
        (Get-Content -LiteralPath $Context.ProjectManifestPath -Raw) -notmatch ('(?m)^startup_scene=' + [Regex]::Escape($openRelativePath) + '$')) {
        throw "Closing after Save changed the project's existing startup-scene authority."
    }
    Write-Output "[pass] Packaged Open Scene and close protection used normal product input: invalid Open preserved the loaded document, Close Cancel retained the dirty scene, and Close Save persisted it before clean exit. Close prompt screenshot: $closePromptScreenshot"
    Write-Output "[pass] Open Scene visual evidence: $formScreenshot, $catalogScreenshot, $openedScreenshot, $focusScreenshot, $failureScreenshot"
}
