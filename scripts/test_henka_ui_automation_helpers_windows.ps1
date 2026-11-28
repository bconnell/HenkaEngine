param(
    [Parameter(Mandatory = $true)][string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'henka_script_common.ps1')
. (Join-Path $PSScriptRoot 'henka_ui_automation_helpers.ps1')
. (Join-Path $PSScriptRoot 'henka_packaged_startup_readiness.ps1')
$sceneWorkflowLoadOutput = @(. (Join-Path $PSScriptRoot 'henka_packaged_scene_open_workflow.ps1'))
if ($sceneWorkflowLoadOutput.Count -ne 0 -or
    $null -eq (Get-Command -Name 'Invoke-HenkaPackagedScriptEditorUnsavedChangeWorkflow' -CommandType Function -ErrorAction SilentlyContinue)) {
    throw 'Loading the packaged scene workflow must quietly load its Script Editor workflow dependency.'
}

Add-Type -AssemblyName System.Drawing
$temporaryRoot = Get-HenkaTestTemporaryRoot -RepositoryRoot $RepositoryRoot
$testDirectory = Join-Path $temporaryRoot ('ui-bitmap-detach-' + [Guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($testDirectory) | Out-Null
$imagePath = Join-Path $testDirectory 'capture.bmp'
$telemetryPath = Join-Path $testDirectory 'authoring-rows.log'
$source = $null
$detached = $null
$exclusiveStream = $null
$liveLogWriter = $null

try {
    $source = New-Object System.Drawing.Bitmap -ArgumentList 3, 2
    $source.SetPixel(1, 1, [System.Drawing.Color]::FromArgb(255, 40, 80, 120))
    $source.Save($imagePath, [System.Drawing.Imaging.ImageFormat]::Bmp)
    $source.Dispose()
    $source = $null

    $detached = New-HenkaDetachedBitmapFromFile -Path $imagePath -MaxDimension 16 -MaxPixels 256
    if ($detached.Width -ne 3 -or $detached.Height -ne 2) {
        throw 'The detached capture copy changed its source dimensions.'
    }
    $pixel = $detached.GetPixel(1, 1)
    if ($pixel.R -ne 40 -or $pixel.G -ne 80 -or $pixel.B -ne 120) {
        throw 'The detached capture copy changed a representative source pixel.'
    }

    $blankBitmap = New-Object System.Drawing.Bitmap -ArgumentList 80, 80
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($blankBitmap)
        try {
            $graphics.Clear([System.Drawing.Color]::White)
            $graphics.FillRectangle([System.Drawing.Brushes]::Black, 0, 0, 8, 80)
            $graphics.FillRectangle([System.Drawing.Brushes]::DimGray, 8, 0, 64, 24)
            $graphics.FillRectangle([System.Drawing.Brushes]::SteelBlue, 12, 4, 56, 2)
            $graphics.FillRectangle([System.Drawing.Brushes]::LightGray, 12, 12, 24, 2)
            $graphics.FillRectangle([System.Drawing.Brushes]::Black, 72, 0, 8, 80)
            $graphics.FillRectangle([System.Drawing.Brushes]::Black, 0, 72, 80, 8)
        }
        finally { $graphics.Dispose() }
        $blankSummary = Get-HenkaAutomationScreenshotContentSummary -Bitmap $blankBitmap
        $blankClientSummary = Get-HenkaAutomationScreenshotContentSummary `
            -Bitmap $blankBitmap -RegionX 8 -RegionY 24 -RegionWidth 64 -RegionHeight 48
        if (-not $blankSummary.HasVisibleContent -or
            $blankSummary.DistinctColorCount -lt 3 -or
            $blankClientSummary.HasVisibleContent -or
            $blankClientSummary.DistinctColorCount -ne 1) {
            throw 'A capture containing only non-client frame decoration was accepted as rendered client UI.'
        }

        $graphics = [System.Drawing.Graphics]::FromImage($blankBitmap)
        try {
            $graphics.FillRectangle([System.Drawing.Brushes]::DimGray, 16, 32, 48, 8)
            $graphics.FillRectangle([System.Drawing.Brushes]::Black, 20, 42, 18, 3)
            $graphics.FillRectangle([System.Drawing.Brushes]::SteelBlue, 16, 48, 48, 16)
        }
        finally { $graphics.Dispose() }
        $visibleSummary = Get-HenkaAutomationScreenshotContentSummary `
            -Bitmap $blankBitmap -RegionX 8 -RegionY 24 -RegionWidth 64 -RegionHeight 48
        if (-not $visibleSummary.HasVisibleContent -or $visibleSummary.DistinctColorCount -lt 4) {
            throw 'A representative nonuniform UI capture was rejected as blank.'
        }
    }
    finally {
        $blankBitmap.Dispose()
    }

    $boundedBitmap = New-Object System.Drawing.Bitmap -ArgumentList 257, 255
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($boundedBitmap)
        try { $graphics.Clear([System.Drawing.Color]::White) }
        finally { $graphics.Dispose() }
        $boundedSummary = Get-HenkaAutomationScreenshotContentSummary `
            -Bitmap $boundedBitmap -MaximumSamplesPerAxis 128
        if ($boundedSummary.SampleCount -gt (128 * 128)) {
            throw "Screenshot sampling exceeded its per-axis bound: $($boundedSummary.SampleCount) samples."
        }
    }
    finally {
        $boundedBitmap.Dispose()
    }

    try {
        $exclusiveStream = [System.IO.File]::Open(
            $imagePath,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::ReadWrite,
            [System.IO.FileShare]::None)
    }
    catch {
        throw 'The returned capture bitmap still holds the temporary source image file open.'
    }
    finally {
        if ($null -ne $exclusiveStream) {
            $exclusiveStream.Dispose()
            $exclusiveStream = $null
        }
    }

    [System.IO.File]::WriteAllLines($telemetryPath, @(
        'Native authoring source row: name=Showcase Giraffe Anatomical Giraffe Study Primitive x=12.0 y=20.0 width=180.0 height=28.0.',
        'Native authoring source row: name=Showcase Giraffe Anatomical Giraffe Study Primitive x=14.0 y=24.0 width=184.0 height=28.0.',
        'Native authoring source row: name=Showcase Giraffe [Study].Primitive x=30.5 y=40.5 width=90.0 height=28.0.',
        'Native authoring source row: name=Showcase Giraffe malformed x=no y=1 width=10 height=28.0.'
    ))
    $exactRow = Get-HenkaNativeAuthoringSourceRowGeometry `
        -Path $telemetryPath `
        -Name 'Showcase Giraffe [Study].Primitive'
    if ($null -eq $exactRow -or $exactRow.X -ne 30.5 -or $exactRow.Y -ne 40.5 -or
        $exactRow.Name -ne 'Showcase Giraffe [Study].Primitive') {
        throw 'The exact native authoring row parser did not return escaped-name geometry.'
    }
    $liveLogWriter = [System.IO.File]::Open(
        $telemetryPath,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::Read)
    $liveLogWriter.Position = $liveLogWriter.Length
    $prefixRow = Get-HenkaNativeAuthoringSourceRowGeometry `
        -Path $telemetryPath `
        -NamePrefix 'Showcase Giraffe Anatomical' `
        -Latest
    $liveLogWriter.Dispose()
    $liveLogWriter = $null
    if ($null -eq $prefixRow -or $prefixRow.X -ne 14.0 -or $prefixRow.Y -ne 24.0 -or
        $prefixRow.Width -ne 184.0) {
        throw 'The prefix native authoring row parser did not return the latest complete product geometry.'
    }
    $missingRow = Get-HenkaNativeAuthoringSourceRowGeometry -Path $telemetryPath -Name 'No matching row'
    if ($null -ne $missingRow) {
        throw 'The native authoring row parser fabricated geometry for a missing row.'
    }

    $transitionLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_decision action=open choice=cancel scene_retained=1 scene_dirty=1 script_dirty=1 transform_dirty=1 terrain_dirty_regions=0 active_name=Untitled Scene Unsaved Open Draft active_path=GoldenPathSaveAs.hscene object_count=0 selected_entity=0 terrain_active=0.'
    $transitionDecision = ConvertFrom-HenkaSceneTransitionDecisionLine -Line $transitionLine
    if ($null -eq $transitionDecision -or
        $transitionDecision.Action -ne 'open' -or
        $transitionDecision.Choice -ne 'cancel' -or
        $transitionDecision.SceneRetained -ne '1' -or
        $transitionDecision.SceneDirty -ne '1' -or
        $transitionDecision.ScriptDirty -ne '1' -or
        $transitionDecision.TransformDirty -ne '1' -or
        $transitionDecision.TerrainDirtyRegions -ne '0' -or
        $transitionDecision.ActiveName -ne 'Untitled Scene Unsaved Open Draft' -or
        $transitionDecision.ActivePath -ne 'GoldenPathSaveAs.hscene' -or
        $transitionDecision.ObjectCount -ne '0' -or
        $transitionDecision.SelectedEntity -ne '0' -or
        $transitionDecision.TerrainActive -ne '0') {
        throw 'The scene-transition decision parser did not preserve the actual packaged cancellation state.'
    }
    $reorderedTransitionLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_decision terrain_dirty_regions=0 active_path=Projects/Scene One.hscene terrain_active=0 selected_entity=2097153 action=open object_count=2 active_name=Open Draft With Spaces scene_retained=1 scene_dirty=1 script_dirty=0 transform_dirty=0 choice=cancel.'
    $reorderedTransition = ConvertFrom-HenkaSceneTransitionDecisionLine -Line $reorderedTransitionLine
    if ($null -eq $reorderedTransition -or
        $reorderedTransition.Action -ne $transitionDecision.Action -or
        $reorderedTransition.Choice -ne $transitionDecision.Choice -or
        $reorderedTransition.SceneRetained -ne $transitionDecision.SceneRetained -or
        $reorderedTransition.SceneDirty -ne $transitionDecision.SceneDirty -or
        $reorderedTransition.TransformDirty -ne '0' -or
        $reorderedTransition.TerrainDirtyRegions -ne $transitionDecision.TerrainDirtyRegions -or
        $reorderedTransition.ActiveName -ne 'Open Draft With Spaces' -or
        $reorderedTransition.ActivePath -ne 'Projects/Scene One.hscene' -or
        $reorderedTransition.ObjectCount -ne '2' -or
        $reorderedTransition.SelectedEntity -ne '2097153' -or
        $reorderedTransition.TerrainActive -ne '0') {
        throw 'Reordered transition telemetry did not parse to the same structured state.'
    }
    $terrainOnlyCancelLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_decision action=new choice=cancel scene_retained=1 scene_dirty=0 script_dirty=0 transform_dirty=0 terrain_dirty_regions=1 active_name=Saved Terrain Draft active_path=Projects/Terrain.hscene object_count=2 selected_entity=2097153 terrain_active=1.'
    $terrainOnlyCancel = ConvertFrom-HenkaSceneTransitionDecisionLine -Line $terrainOnlyCancelLine
    if ($null -eq $terrainOnlyCancel -or
        $terrainOnlyCancel.Action -ne 'new' -or
        $terrainOnlyCancel.Choice -ne 'cancel' -or
        $terrainOnlyCancel.SceneRetained -ne '1' -or
        $terrainOnlyCancel.SceneDirty -ne '0' -or
        $terrainOnlyCancel.ScriptDirty -ne '0' -or
        $terrainOnlyCancel.TransformDirty -ne '0' -or
        $terrainOnlyCancel.TerrainDirtyRegions -ne '1' -or
        $terrainOnlyCancel.ActiveName -ne 'Saved Terrain Draft' -or
        $terrainOnlyCancel.ActivePath -ne 'Projects/Terrain.hscene' -or
        $terrainOnlyCancel.ObjectCount -ne '2' -or
        $terrainOnlyCancel.SelectedEntity -ne '2097153' -or
        $terrainOnlyCancel.TerrainActive -ne '1') {
        throw 'Terrain-only New Scene cancellation did not preserve structured scene and terrain state.'
    }
    $transitionWithUnknownField = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_decision action=open diagnostic_frame=812 scene_dirty=1 script_dirty=0 transform_dirty=0 choice=cancel terrain_dirty_regions=0 active_name=Open Draft With Spaces active_path=Projects/Scene One.hscene object_count=2 selected_entity=2097153 terrain_active=0 scene_retained=1.'
    $transitionWithUnknownField = ConvertFrom-HenkaSceneTransitionDecisionLine -Line $transitionWithUnknownField
    if ($null -eq $transitionWithUnknownField -or
        $transitionWithUnknownField.ActiveName -ne 'Open Draft With Spaces' -or
        $transitionWithUnknownField.ActivePath -ne 'Projects/Scene One.hscene' -or
        $transitionWithUnknownField.SceneDirty -ne '1') {
        throw 'An added diagnostic field contaminated an existing structured scene-state value.'
    }
    if ($null -ne (ConvertFrom-HenkaSceneTransitionDecisionLine -Line ($transitionLine -replace 'scene_dirty=1', 'scene_dirty=1 scene_dirty=0')) -or
        $null -ne (ConvertFrom-HenkaSceneTransitionDecisionLine -Line ($transitionLine -replace ' script_dirty=1', '')) -or
        $null -ne (ConvertFrom-HenkaSceneTransitionDecisionLine -Line ($transitionLine -replace ' transform_dirty=1', ''))) {
        throw 'The scene-transition parser accepted ambiguous duplicate state fields.'
    }

    $reorderedTransitionDialogLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_dialog cancel_height=40 action=reload available=1 active_name=Saved Scene active_path=Projects/Scene One.hscene selected_entity=0 object_count=1 content_height=720 content_width=1280 terrain_dirty_regions=2 terrain_dirty_query_result=0 scene_dirty_query_result=0 scene_dirty=0 script_dirty=0 transform_dirty=0 save_x=10 save_y=20 save_width=90 save_height=40 discard_x=110 discard_y=20 discard_width=90 discard_height=40 cancel_x=210 cancel_y=20 cancel_width=90.'
    $reorderedTransitionDialog = ConvertFrom-HenkaSceneTransitionControlsLine `
        -Line $reorderedTransitionDialogLine `
        -EventName 'scene_transition_dialog' `
        -Action 'reload'
    if ($null -eq $reorderedTransitionDialog -or
        $reorderedTransitionDialog.action -ne 'reload' -or
        $reorderedTransitionDialog.active_name -ne 'Saved Scene' -or
        $reorderedTransitionDialog.active_path -ne 'Projects/Scene One.hscene' -or
        $reorderedTransitionDialog.terrain_dirty_regions -ne '2' -or
        $reorderedTransitionDialog.script_dirty -ne '0' -or
        $reorderedTransitionDialog.transform_dirty -ne '0') {
        throw 'Reordered scene-transition control telemetry did not preserve its structured state.'
    }
    $reorderedTransitionDialogMatch = ConvertTo-HenkaAutomationRegexResult `
        -Fields $reorderedTransitionDialog `
        -Line $reorderedTransitionDialogLine
    if ($reorderedTransitionDialogMatch.Groups['cancel_x'].Value -ne '210' -or
        $reorderedTransitionDialogMatch.Groups['active_name'].Value -ne 'Saved Scene') {
        throw 'The structured control parser did not preserve the existing geometry-consumer interface.'
    }
    $packagedAuthoringWorkflow = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'test_editor_native_authoring_windows.ps1') -Raw
    if (-not $packagedAuthoringWorkflow.Contains('ConvertFrom-HenkaSceneTransitionDecisionLine -Line $cancelledDecisionMatch.Value') -or
        -not $packagedAuthoringWorkflow.Contains('ConvertFrom-HenkaSceneTransitionDecisionLine -Line $newSceneCancelledMatch.Value') -or
        $packagedAuthoringWorkflow.Contains('scene_transition_decision action=reload choice=cancel scene_retained=1 scene_dirty=1 terrain_dirty_regions=0') -or
        $packagedAuthoringWorkflow.Contains('scene_transition_decision action=new choice=cancel scene_retained=1 scene_dirty=0 terrain_dirty_regions=')) {
        throw 'The packaged New Scene and Reload cancellation checks must consume structured transition decisions without field-order assumptions.'
    }
    if (-not $packagedAuthoringWorkflow.Contains('$newSceneConfirmation.Groups[''scene_dirty_query_result''].Value') -or
        -not $packagedAuthoringWorkflow.Contains('$newSceneConfirmation.Groups[''terrain_dirty_query_result''].Value') -or
        $packagedAuthoringWorkflow.Contains('$newSceneConfirmation.Groups[''dirty_result''].Value') -or
        $packagedAuthoringWorkflow.Contains('$newSceneConfirmation.Groups[''terrain_dirty_result''].Value')) {
        throw 'The packaged New Scene workflow does not consume the canonical scene-transition query-result fields.'
    }
    $packagedScriptEditorWorkflow = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'henka_packaged_script_editor_workflow.ps1') -Raw
    if (-not $packagedScriptEditorWorkflow.Contains('-WheelDelta -1.0') -or
        -not $packagedScriptEditorWorkflow.Contains('$offsetAfter -le ($offsetBefore + 0.01)') -or
        $packagedScriptEditorWorkflow.Contains('-WheelDelta 1.0')) {
        throw 'The packaged Script Editor workflow must scroll downward toward lower Object Details controls and require forward offset progress.'
    }
    if (-not $packagedScriptEditorWorkflow.Contains('object_details_disclosure id=overview') -or
        -not $packagedScriptEditorWorkflow.Contains('Expand Object Details Overview') -or
        -not $packagedScriptEditorWorkflow.Contains('overviewDisclosure.Groups[''expanded''].Value')) {
        throw 'The packaged Script Editor workflow must expand Overview through its current product-owned disclosure geometry before searching for Add HenkaScript.'
    }
    if (-not $packagedScriptEditorWorkflow.Contains('$surfaceEntityPattern') -or
        -not $packagedScriptEditorWorkflow.Contains('for ($revealAttempt = 0; $revealAttempt -lt 8; ++$revealAttempt)') -or
        -not $packagedScriptEditorWorkflow.Contains('The newly attached Script Editor did not become visible after bounded, product-consumed Object Details scrolling.') -or
        -not $packagedScriptEditorWorkflow.Contains('$revealScroll.Groups[''handled''].Value -ne ''1''') -or
        -not $packagedScriptEditorWorkflow.Contains('$revealOffsetAfter -le ($revealOffsetBefore + 0.01)')) {
        throw 'The packaged Script Editor workflow must reveal newly inserted editor content through bounded Object Details scrolling, require product consumption and forward scroll progress, and match the attached object.'
    }
    $sandboxMainSource = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\examples\sandbox3d\main.c') -Raw
    if (-not $sandboxMainSource.Contains('HENKA_AUTOMATION_DIAGNOSTIC object_details_disclosure id=overview')) {
        throw 'The product does not expose the Overview disclosure rectangle and expansion state needed by the packaged workflow.'
    }
    $packagedSceneOpenWorkflow = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'henka_packaged_scene_open_workflow.ps1') -Raw
    if (-not $packagedSceneOpenWorkflow.Contains("`$confirmation.Groups['scene_dirty_query_result'].Value") -or
        -not $packagedSceneOpenWorkflow.Contains("`$confirmation.Groups['terrain_dirty_query_result'].Value") -or
        $packagedSceneOpenWorkflow.Contains("`$confirmation.Groups['dirty_result'].Value") -or
        $packagedSceneOpenWorkflow.Contains("`$confirmation.Groups['terrain_dirty_result'].Value")) {
        throw 'The packaged Open Scene workflow does not consume the canonical scene-transition query-result fields.'
    }
    if ($null -ne (ConvertFrom-HenkaSceneTransitionControlsLine `
            -Line ($reorderedTransitionDialogLine -replace 'scene_dirty=0', 'scene_dirty=0 scene_dirty=1') `
            -EventName 'scene_transition_dialog' `
            -Action 'reload') -or
        $null -ne (ConvertFrom-HenkaSceneTransitionControlsLine `
            -Line ($reorderedTransitionDialogLine -replace ' script_dirty=0', '') `
            -EventName 'scene_transition_dialog' `
            -Action 'reload') -or
        $null -ne (ConvertFrom-HenkaSceneTransitionControlsLine `
            -Line ($reorderedTransitionDialogLine -replace ' transform_dirty=0', '') `
            -EventName 'scene_transition_dialog' `
            -Action 'reload') -or
        $null -ne (ConvertFrom-HenkaSceneTransitionControlsLine `
            -Line ($reorderedTransitionDialogLine -replace ' cancel_x=210 cancel_y=20 cancel_width=90', '') `
            -EventName 'scene_transition_dialog' `
            -Action 'reload')) {
        throw 'The scene-transition control parser accepted duplicate or incomplete state.'
    }

    $terrainEditLine = 'HENKA_AUTOMATION_DIAGNOSTIC terrain_edit_state dirty_regions=1 content_hash_after=0123456789abcdef world_stats_valid=1 result=0 content_hash_before_valid=1 content_hash_after_valid=1 content_hash_before=fedcba9876543210.'
    $terrainEditState = ConvertFrom-HenkaTerrainEditStateLine -Line $terrainEditLine
    if ($null -eq $terrainEditState -or
        $terrainEditState.result -ne '0' -or
        $terrainEditState.dirty_regions -ne '1' -or
        $terrainEditState.content_hash_before -ne 'fedcba9876543210' -or
        $terrainEditState.content_hash_after -ne '0123456789abcdef') {
        throw 'Reordered terrain edit telemetry did not preserve content and dirty-state fields.'
    }
    $terrainReloadLine = 'HENKA_AUTOMATION_DIAGNOSTIC terrain_scene_reload dirty_regions_after=0 content_hash_after=1111111111111111 rollback_succeeded=0 operation_result=0 action=reload_discard content_hash_before_valid=1 result=0 failure_stage=none dirty_regions_before=1 restored_regions=1 rollback_attempted=0 content_hash_before=2222222222222222 content_hash_after_valid=1.'
    $terrainReloadState = ConvertFrom-HenkaTerrainSceneReloadLine -Line $terrainReloadLine
    if ($null -eq $terrainReloadState -or
        $terrainReloadState.result -ne '0' -or
        $terrainReloadState.dirty_regions_before -ne '1' -or
        $terrainReloadState.restored_regions -ne '1' -or
        $terrainReloadState.dirty_regions_after -ne '0' -or
        $terrainReloadState.content_hash_after -ne '1111111111111111') {
        throw 'Reordered terrain Reload telemetry did not preserve its result and content fields.'
    }
    if ($null -ne (ConvertFrom-HenkaTerrainEditStateLine -Line ($terrainEditLine -replace 'result=0', 'result=0 result=1')) -or
        $null -ne (ConvertFrom-HenkaTerrainSceneReloadLine -Line ($terrainReloadLine -replace 'restored_regions=1', 'restored_regions=1 restored_regions=2'))) {
        throw 'The terrain state parsers accepted ambiguous duplicate fields.'
    }

    $reorderedSaveAsLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_save_as cancel_height=40 action=close available=1 path_height=36 save_height=40 path_x=12 path_y=14 path_width=300 save_x=12 save_y=60 save_width=140 cancel_x=160 cancel_y=60 cancel_width=140.'
    $reorderedSaveAs = ConvertFrom-HenkaSceneTransitionControlsLine `
        -Line $reorderedSaveAsLine `
        -EventName 'scene_transition_save_as' `
        -Action 'close'
    if ($null -eq $reorderedSaveAs -or
        $reorderedSaveAs.path_width -ne '300' -or
        $reorderedSaveAs.cancel_x -ne '160') {
        throw 'Reordered Save As controls did not preserve their actual fields.'
    }

    $reorderedSceneActionsLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_action_controls open_scene_rect=810,20,70,28 visible=1 new_scene_rect=650,20,70,28 reload_rect=570,20,70,28 document_object_count=2 save_as_rect=490,20,70,28 selection_present=1 relative_path_empty=0 save_rect=410,20,70,28.'
    $reorderedSceneActions = ConvertFrom-HenkaSceneDocumentActionControlsLine `
        -Line $reorderedSceneActionsLine
    if ($null -eq $reorderedSceneActions -or
        $reorderedSceneActions.document_object_count -ne '2' -or
        $reorderedSceneActions.selection_present -ne '1') {
        throw 'Reordered Scene Document action controls lost their authoritative fixture state.'
    }
    $sceneActionsGeometry = ConvertTo-HenkaAutomationGeometryResult `
        -Fields $reorderedSceneActions `
        -Line $reorderedSceneActionsLine `
        -RectangleMappings @('save_rect=save', 'open_scene_rect=open_scene')
    if ($null -eq $sceneActionsGeometry -or
        $sceneActionsGeometry.Groups['save_x'].Value -ne '410' -or
        $sceneActionsGeometry.Groups['open_scene_width'].Value -ne '70') {
        throw 'Field-based Scene Document geometry did not map the current product rectangles.'
    }
    if ($null -ne (ConvertFrom-HenkaSceneDocumentActionControlsLine `
            -Line ($reorderedSceneActionsLine -replace 'visible=1', 'visible=1 visible=0')) -or
        $null -ne (ConvertFrom-HenkaSceneDocumentActionControlsLine `
            -Line ($reorderedSceneActionsLine -replace 'open_scene_rect=[^ ]+ ', ''))) {
        throw 'Scene Document controls accepted duplicate or incomplete state.'
    }

    $reorderedPathControlsLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_path_editor_controls relative_path_empty=0 cancel_rect=220,70,120,40 confirm_rect=80,70,120,40 mode=save_as path_rect=80,20,260,36 visible=1.'
    $reorderedPathControls = ConvertFrom-HenkaScenePathEditorControlsLine `
        -Line $reorderedPathControlsLine `
        -Mode 'save_as'
    $pathControlGeometry = ConvertTo-HenkaAutomationGeometryResult `
        -Fields $reorderedPathControls `
        -Line $reorderedPathControlsLine `
        -RectangleMappings @('path_rect=path', 'confirm_rect=save', 'cancel_rect=cancel')
    if ($null -eq $pathControlGeometry -or
        $pathControlGeometry.Groups['save_x'].Value -ne '80' -or
        $pathControlGeometry.Groups['path_height'].Value -ne '36' -or
        $null -ne (ConvertFrom-HenkaScenePathEditorControlsLine `
            -Line $reorderedPathControlsLine -Mode 'open')) {
        throw 'Reordered Save As control geometry or mode authority was not enforced.'
    }

    $catalogPathControlsLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_path_editor_controls visible=1 mode=open path_rect=10,20,200,28 confirm_rect=10,50,80,24 cancel_rect=100,50,80,24 relative_path_empty=0 relative_path=Scenes/Folder With Spaces/First Scene.hscene catalog_count=7 catalog_page=1 catalog_page_count=3 catalog_result=0 catalog_stale=0 refresh_rect=10,80,68,24 row0_rect=10,112,196,24 row0_path=Scenes/Folder With Spaces/First Scene.hscene row1_rect=10,140,196,24 row1_path=Scenes/Second Scene.hscene row2_rect=10,168,196,24 row2_path=<none> previous_rect=10,196,52,24 page_rect=68,196,80,24 next_rect=154,196,52,24.'
    $catalogPathControls = ConvertFrom-HenkaScenePathEditorControlsLine `
        -Line $catalogPathControlsLine `
        -Mode 'open'
    $catalogPathGeometry = ConvertTo-HenkaAutomationGeometryResult `
        -Fields $catalogPathControls `
        -Line $catalogPathControlsLine `
        -RectangleMappings @('path_rect=path', 'confirm_rect=open')
    if ($null -eq $catalogPathGeometry -or
        $catalogPathControls.relative_path_empty -ne '0' -or
        $catalogPathGeometry.Groups['open_x'].Value -ne '10' -or
        $catalogPathGeometry.Groups['path_width'].Value -ne '200') {
        throw 'Open Scene controls did not preserve geometry with the current catalog and spaced-path diagnostics.'
    }
    $catalogRelativePath = $catalogPathGeometry.Groups['relative_path']
    if ($null -eq $catalogRelativePath -or
        $catalogRelativePath.Value -ne 'Scenes/Folder With Spaces/First Scene.hscene') {
        throw 'Open Scene control parsing dropped the selected relative path needed by the packaged workflow.'
    }
    $catalogTelemetryPath = Join-Path $testDirectory 'scene-open-catalog.log'
    Set-Content -LiteralPath $catalogTelemetryPath -Value $catalogPathControlsLine -Encoding UTF8
    $parsedCatalog = Get-SceneOpenCatalogControls -Path $catalogTelemetryPath
    if ($null -eq $parsedCatalog -or
        -not ($parsedCatalog.PSObject.Properties.Name -contains 'Previous') -or
        $parsedCatalog.Previous.X -ne 10.0 -or
        $parsedCatalog.Previous.Y -ne 196.0 -or
        $parsedCatalog.Previous.Width -ne 52.0 -or
        $parsedCatalog.Previous.Height -ne 24.0 -or
        $parsedCatalog.Page -ne 1 -or
        $parsedCatalog.PageCount -ne 3) {
        throw 'The packaged Open Scene catalog parser did not preserve its product-owned Previous page hit rectangle.'
    }
    $catalogOpenPathPattern = New-HenkaScenePathEditorStatePattern `
        -Mode 'open' `
        -RelativePath 'Scenes/Folder With Spaces/First Scene.hscene'
    $emptyCatalogLine = $catalogPathControlsLine -replace `
        'relative_path_empty=0 relative_path=.*? catalog_count=',
        'relative_path_empty=1 relative_path=<empty> catalog_count='
    $emptyOpenPathPattern = New-HenkaScenePathEditorStatePattern `
        -Mode 'open' `
        -RelativePath ''
    $legacyEmptyOpenLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_path_editor_controls visible=1 mode=open path_rect=10,20,200,28 confirm_rect=10,50,80,24 cancel_rect=100,50,80,24 relative_path_empty=1.'
    if (-not [Regex]::IsMatch($catalogPathControlsLine, $catalogOpenPathPattern) -or
        -not [Regex]::IsMatch($emptyCatalogLine, $emptyOpenPathPattern) -or
        [Regex]::IsMatch($legacyEmptyOpenLine, $emptyOpenPathPattern) -or
        [Regex]::IsMatch($catalogPathControlsLine, $emptyOpenPathPattern)) {
        throw 'Scene path state patterns did not distinguish current catalog state from obsolete or conflicting telemetry.'
    }
    $reorderedSceneNameLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_name_controls rename_rect=190,50,90,36 field_rect=20,50,160,36 available=1 label_rect=20,20,120,24.'
    $sceneNameFields = ConvertFrom-HenkaSceneNameControlsLine -Line $reorderedSceneNameLine
    $sceneNameGeometry = ConvertTo-HenkaAutomationGeometryResult `
        -Fields $sceneNameFields `
        -Line $reorderedSceneNameLine `
        -RectangleMappings @('rename_rect=rename')
    if ($null -eq $sceneNameGeometry -or
        $sceneNameGeometry.Groups['rename_x'].Value -ne '190') {
        throw 'Reordered Scene Objects name controls lost their real rename target.'
    }

    $reorderedClickInputLines = @(
        'HENKA_AUTOMATION_DIAGNOSTIC input release_consumed=0 button=none type=move record=41',
        'HENKA_AUTOMATION_DIAGNOSTIC input button=left release_consumed=0 type=button-down record=42',
        'HENKA_AUTOMATION_DIAGNOSTIC input type=button-up record=43 release_consumed=1 button=left')
    $clickConsumption = Get-HenkaAutomationClickConsumptionResult `
        -Lines $reorderedClickInputLines
    if ($null -eq $clickConsumption -or
        -not $clickConsumption.Consumed -or
        $clickConsumption.MoveRecord -ne '41' -or
        $clickConsumption.DownRecord -ne '42' -or
        $clickConsumption.UpRecord -ne '43') {
        throw 'Structured input telemetry did not confirm an ordered, consumed left-click release.'
    }
    $unconsumedRelease = Get-HenkaAutomationClickConsumptionResult -Lines @(
        'HENKA_AUTOMATION_DIAGNOSTIC input record=51 type=move button=none release_consumed=0',
        'HENKA_AUTOMATION_DIAGNOSTIC input record=52 type=button-down button=left release_consumed=0',
        'HENKA_AUTOMATION_DIAGNOSTIC input record=53 type=button-up button=left release_consumed=0')
    $outOfOrderRelease = Get-HenkaAutomationClickConsumptionResult -Lines @(
        'HENKA_AUTOMATION_DIAGNOSTIC input record=61 type=move button=none release_consumed=0',
        'HENKA_AUTOMATION_DIAGNOSTIC input record=63 type=button-down button=left release_consumed=0',
        'HENKA_AUTOMATION_DIAGNOSTIC input record=62 type=button-up button=left release_consumed=1')
    if ($null -eq $unconsumedRelease -or $unconsumedRelease.Consumed -or
        $null -eq $outOfOrderRelease -or $outOfOrderRelease.Consumed) {
        throw 'The click-consumption check accepted an unconsumed or non-monotonic release.'
    }
    $clickLogPath = Join-Path $testDirectory 'click-consumption.log'
    $clickLogLines = @(
        'HENKA_AUTOMATION_DIAGNOSTIC input record=71 type=move button=none release_consumed=0',
        'HENKA_AUTOMATION_DIAGNOSTIC input record=72 type=button-down button=left release_consumed=0',
        'HENKA_AUTOMATION_DIAGNOSTIC input record=73 type=button-up button=left release_consumed=1')
    [System.IO.File]::WriteAllText(
        $clickLogPath,
        (($clickLogLines -join [Environment]::NewLine) + [Environment]::NewLine),
        [System.Text.Encoding]::UTF8)
    $clickLogOffset = (Get-Item -LiteralPath $clickLogPath).Length
    $staleClickResult = Wait-HenkaAutomationClickConsumedAfterOffset `
        -Path $clickLogPath -StartingOffset $clickLogOffset -TimeoutMilliseconds 40
    if ($null -eq $staleClickResult -or $staleClickResult.Consumed) {
        throw 'The click waiter accepted a complete click that predated the current interaction offset.'
    }
    [System.IO.File]::AppendAllText(
        $clickLogPath,
        (($clickLogLines -join [Environment]::NewLine) + [Environment]::NewLine),
        [System.Text.Encoding]::UTF8)
    $freshClickResult = Wait-HenkaAutomationClickConsumedAfterOffset `
        -Path $clickLogPath -StartingOffset $clickLogOffset -TimeoutMilliseconds 1000
    if ($null -eq $freshClickResult -or
        -not $freshClickResult.Consumed -or
        $freshClickResult.UpRecord -ne '73') {
        throw 'The click waiter did not accept the complete consumed click appended after the current interaction offset.'
    }

    $playControlsLine = 'HENKA_AUTOMATION_DIAGNOSTIC game_authoring_play_controls play_rect=206,80,90,26 entity_id=73 available=1 trigger_rect=110,80,90,26 play_state=0.'
    $playControls = ConvertFrom-HenkaGameAuthoringPlayControlsLine -Line $playControlsLine
    $playGeometry = ConvertTo-HenkaAutomationGeometryResult `
        -Fields $playControls `
        -Line $playControlsLine `
        -RectangleMappings @('play_rect=play', 'trigger_rect=trigger')
    if ($null -eq $playGeometry -or
        $playControls.entity_id -ne '73' -or
        $playControls.play_state -ne '0' -or
        $playGeometry.Groups['play_x'].Value -ne '206' -or
        $null -eq $playGeometry.Fields -or
        $playGeometry.Fields.entity_id -ne '73' -or
        $playGeometry.Fields.play_state -ne '0') {
        throw 'The Game Authoring Play parser lost reordered selected-entity state or real button geometry.'
    }
    $closePlaySaveLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_close_play_save play_locked_after=0 result=0 play_locked_before=1.'
    $closePlaySave = ConvertFrom-HenkaClosePlaySaveLine -Line $closePlaySaveLine
    if ($null -eq $closePlaySave -or
        $closePlaySave.result -ne '0' -or
        $closePlaySave.play_locked_before -ne '1' -or
        $closePlaySave.play_locked_after -ne '0' -or
        $null -ne (ConvertFrom-HenkaClosePlaySaveLine `
            -Line ($closePlaySaveLine -replace 'result=0', 'result=0 result=1'))) {
        throw 'The close-save state parser lost lock transition fields or accepted duplicate state.'
    }

    $physicsDisclosureCommand = Get-Command `
        -Name ConvertFrom-HenkaGameAuthoringPhysicsDisclosureLine `
        -CommandType Function `
        -ErrorAction SilentlyContinue
    if ($null -eq $physicsDisclosureCommand) {
        throw 'The Game Authoring Physics disclosure parser is missing.'
    }
    $physicsDisclosureLine = 'HENKA_AUTOMATION_DIAGNOSTIC game_authoring_physics_disclosure expanded=0 rect=846.0,161.9,392.0,28.0 entity_id=2097164 available=1.'
    $physicsDisclosure = ConvertFrom-HenkaGameAuthoringPhysicsDisclosureLine `
        -Line $physicsDisclosureLine
    $physicsDisclosureGeometry = ConvertTo-HenkaAutomationGeometryResult `
        -Fields $physicsDisclosure `
        -Line $physicsDisclosureLine `
        -RectangleMappings @('rect=disclosure')
    if ($null -eq $physicsDisclosureGeometry -or
        $physicsDisclosure.available -ne '1' -or
        $physicsDisclosure.entity_id -ne '2097164' -or
        $physicsDisclosure.expanded -ne '0' -or
        $physicsDisclosureGeometry.Groups['disclosure_x'].Value -ne '846.0' -or
        $physicsDisclosureGeometry.Groups['disclosure_y'].Value -ne '161.9' -or
        $physicsDisclosureGeometry.Fields.entity_id -ne '2097164') {
        throw 'The Physics disclosure parser lost selected-entity state or product-owned hit geometry.'
    }
    $physicsDisclosureReordered = ConvertFrom-HenkaGameAuthoringPhysicsDisclosureLine `
        -Line 'HENKA_AUTOMATION_DIAGNOSTIC game_authoring_physics_disclosure rect=846.0,161.9,392.0,28.0 available=1 expanded=1 entity_id=2097164.'
    if ($null -eq $physicsDisclosureReordered -or
        $physicsDisclosureReordered.entity_id -ne $physicsDisclosure.entity_id -or
        $physicsDisclosureReordered.expanded -ne '1' -or
        $null -ne (ConvertFrom-HenkaGameAuthoringPhysicsDisclosureLine `
            -Line ($physicsDisclosureLine -replace 'expanded=0', 'expanded=2')) -or
        $null -ne (ConvertFrom-HenkaGameAuthoringPhysicsDisclosureLine `
            -Line ($physicsDisclosureLine -replace 'available=1', 'available=1 available=1'))) {
        throw 'The Physics disclosure parser accepted invalid state or rejected reordered authoritative fields.'
    }

    $physicsFreshnessLogPath = Join-Path $testDirectory 'physics-disclosure-freshness.log'
    [System.IO.File]::WriteAllText(
        $physicsFreshnessLogPath,
        $physicsDisclosureLine + [Environment]::NewLine,
        [System.Text.Encoding]::UTF8)
    $physicsFreshnessOffset = Get-FileLengthSafe -Path $physicsFreshnessLogPath
    $physicsFreshnessPattern = '(?m)^HENKA_AUTOMATION_DIAGNOSTIC game_authoring_physics_disclosure [^\r\n]*$'
    if (Wait-FileContainsAfterOffset `
            -Path $physicsFreshnessLogPath `
            -Pattern $physicsFreshnessPattern `
            -StartingOffset $physicsFreshnessOffset `
            -TimeoutMilliseconds 40) {
        throw 'The Physics geometry freshness gate accepted a disclosure line that predated the current interaction.'
    }
    $freshPhysicsDisclosureLine = 'HENKA_AUTOMATION_DIAGNOSTIC game_authoring_physics_disclosure available=1 entity_id=2097164 expanded=0 rect=846.0,302.0,392.0,28.0.'
    [System.IO.File]::AppendAllText(
        $physicsFreshnessLogPath,
        $freshPhysicsDisclosureLine + [Environment]::NewLine,
        [System.Text.Encoding]::UTF8)
    if (-not (Wait-FileContainsAfterOffset `
            -Path $physicsFreshnessLogPath `
            -Pattern $physicsFreshnessPattern `
            -StartingOffset $physicsFreshnessOffset `
            -TimeoutMilliseconds 1000)) {
        throw 'The Physics geometry freshness gate rejected a current disclosure line appended after the interaction boundary.'
    }
    $freshPhysicsMatch = Get-LastLogRegexMatch `
        -Path $physicsFreshnessLogPath `
        -Pattern $physicsFreshnessPattern
    $freshPhysicsFields = if ($null -eq $freshPhysicsMatch) { $null } else {
        ConvertFrom-HenkaGameAuthoringPhysicsDisclosureLine -Line $freshPhysicsMatch.Value
    }
    if ($null -eq $freshPhysicsFields -or
        $freshPhysicsFields.entity_id -ne '2097164' -or
        $freshPhysicsFields.rect -ne '846.0,302.0,392.0,28.0') {
        throw 'The Physics geometry freshness gate did not select the new product row rectangle.'
    }

    $openAttemptLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=scene_open_attempt result=1 relative_path_empty=0 active_name=Open Draft With Spaces active_relative_path=Projects/Scene One.hscene active_document_object_count=2 selection_present=1.'
    $openAttempt = ConvertFrom-HenkaSceneOpenAttemptLine -Line $openAttemptLine
    if ($null -eq $openAttempt -or
        $openAttempt.Result -ne '1' -or
        $openAttempt.RelativePathEmpty -ne '0' -or
        $openAttempt.ActiveName -ne 'Open Draft With Spaces' -or
        $openAttempt.ActiveRelativePath -ne 'Projects/Scene One.hscene' -or
        $openAttempt.ActiveDocumentObjectCount -ne '2' -or
        $openAttempt.SelectionPresent -ne '1') {
        throw 'The scene-open attempt parser did not preserve the product-reported rejected-open state.'
    }
    $reorderedOpenAttemptLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence selection_present=1 active_document_object_count=2 active_relative_path=Projects/Scene One.hscene result=1 active_name=Open Draft With Spaces action=scene_open_attempt relative_path_empty=0.'
    $reorderedOpenAttempt = ConvertFrom-HenkaSceneOpenAttemptLine -Line $reorderedOpenAttemptLine
    if ($null -eq $reorderedOpenAttempt -or
        $reorderedOpenAttempt.Result -ne $openAttempt.Result -or
        $reorderedOpenAttempt.RelativePathEmpty -ne $openAttempt.RelativePathEmpty -or
        $reorderedOpenAttempt.ActiveName -ne $openAttempt.ActiveName -or
        $reorderedOpenAttempt.ActiveRelativePath -ne $openAttempt.ActiveRelativePath -or
        $reorderedOpenAttempt.ActiveDocumentObjectCount -ne $openAttempt.ActiveDocumentObjectCount -or
        $reorderedOpenAttempt.SelectionPresent -ne $openAttempt.SelectionPresent) {
        throw 'Reordered scene-open attempt telemetry did not parse to the same structured state.'
    }
    if ($null -ne (ConvertFrom-HenkaSceneOpenAttemptLine -Line ($openAttemptLine -replace 'result=1', 'result=1 result=0')) -or
        $null -ne (ConvertFrom-HenkaSceneOpenAttemptLine -Line ($openAttemptLine -replace ' selection_present=1\.', '.'))) {
        throw 'The scene-open attempt parser accepted duplicate or incomplete state fields.'
    }

    $openedLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=scene_opened name=Open Draft With Spaces relative_path=Projects/Scene One.hscene relative_path_empty=0 document_object_count=2.'
    $openedState = ConvertFrom-HenkaSceneOpenedLine -Line $openedLine
    $reorderedOpenedState = ConvertFrom-HenkaSceneOpenedLine -Line 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence document_object_count=2 trace_id=91 relative_path=Projects/Scene One.hscene name=Open Draft With Spaces action=scene_opened relative_path_empty=0.'
    if ($null -eq $openedState -or $null -eq $reorderedOpenedState) {
        throw 'The successful scene-open diagnostic did not parse into structured state.'
    }
    $null = Assert-HenkaStructuredState -Description 'Reordered scene-open diagnostic regression' -Actual $reorderedOpenedState -Expected @{
        Action = $openedState.Action
        Name = $openedState.Name
        RelativePath = $openedState.RelativePath
        RelativePathEmpty = $openedState.RelativePathEmpty
        DocumentObjectCount = $openedState.DocumentObjectCount
    }
    if ($null -ne (ConvertFrom-HenkaSceneOpenedLine -Line ($openedLine -replace 'document_object_count=2', 'document_object_count=2 document_object_count=3')) -or
        $null -ne (ConvertFrom-HenkaSceneOpenedLine -Line ($openedLine -replace ' relative_path_empty=0', ''))) {
        throw 'The successful scene-open parser accepted duplicate or incomplete state fields.'
    }

    $productMismatchClassified = $false
    try {
        $null = Assert-HenkaStructuredState -Description 'Product state mismatch regression' -Actual $openedState -Expected @{ Name = 'Different Scene' }
    }
    catch {
        $productMismatchClassified = $_.Exception.Message.Contains('[HENKA_VALIDATION_CLASS=PRODUCT_STATE_MISMATCH]') -and
            $_.Exception.Message.Contains("field 'Name'") -and
            $_.Exception.Message.Contains("product reported 'Open Draft With Spaces'")
    }
    if (-not $productMismatchClassified) {
        throw 'A structured product-state mismatch was not classified with the differing field and values.'
    }

    $preconditionClassified = $false
    try {
        $null = Assert-HenkaStructuredState -Description 'Precondition classification regression' -Actual $openedState -MismatchClassification 'TEST_PRECONDITION_UNMET' -Expected @{ RelativePathEmpty = '1' }
    }
    catch {
        $preconditionClassified = $_.Exception.Message.Contains('[HENKA_VALIDATION_CLASS=TEST_PRECONDITION_UNMET]')
    }
    if (-not $preconditionClassified) {
        throw 'A failed test precondition was not distinguished from a product postcondition failure.'
    }

    $observationGapClassified = $false
    try {
        $null = Assert-HenkaStructuredState -Description 'Observation-gap classification regression' -Actual $openedState -Expected @{ MissingAuthority = '1' }
    }
    catch {
        $observationGapClassified = $_.Exception.Message.Contains('[HENKA_VALIDATION_CLASS=UNCLASSIFIED_OBSERVATION_GAP]')
    }
    if (-not $observationGapClassified) {
        throw 'A missing observed field was incorrectly accepted or attributed to product behavior.'
    }

    $structuredTelemetryPath = Join-Path $testDirectory 'structured-scene-open.log'
    $priorAttemptLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence action=scene_open_attempt result=1 relative_path_empty=0 active_name=Old Draft active_relative_path=Old.hscene active_document_object_count=1 selection_present=0.'
    [System.IO.File]::WriteAllText($structuredTelemetryPath, $priorAttemptLine + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    $actionOffset = (Get-Item -LiteralPath $structuredTelemetryPath).Length
    $staleAttempt = Get-HenkaSceneOpenAttemptAfterOffset -Path $structuredTelemetryPath -StartingOffset $actionOffset -TimeoutMilliseconds 25
    if ($null -ne $staleAttempt) {
        throw 'The scene-open state reader accepted a pre-action diagnostic as post-action evidence.'
    }
    $currentAttemptLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence selection_present=1 trace_id=91 active_document_object_count=2 active_relative_path=Projects/Scene One.hscene result=0 active_name=Current Draft With Spaces action=scene_open_attempt relative_path_empty=0.'
    [System.IO.File]::AppendAllText($structuredTelemetryPath, $currentAttemptLine + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    $currentAttempt = Get-HenkaSceneOpenAttemptAfterOffset -Path $structuredTelemetryPath -StartingOffset $actionOffset -TimeoutMilliseconds 100
    if ($null -eq $currentAttempt -or
        $currentAttempt.Result -ne '0' -or
        $currentAttempt.ActiveName -ne 'Current Draft With Spaces' -or
        $currentAttempt.ActiveRelativePath -ne 'Projects/Scene One.hscene' -or
        $currentAttempt.ActiveDocumentObjectCount -ne '2') {
        throw 'The scene-open state reader did not return the current post-action structured state.'
    }

    [System.IO.File]::AppendAllText($structuredTelemetryPath, $openedLine + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    $openedOffset = (Get-Item -LiteralPath $structuredTelemetryPath).Length
    $staleOpened = Get-HenkaSceneOpenedAfterOffset -Path $structuredTelemetryPath -StartingOffset $openedOffset -TimeoutMilliseconds 25
    if ($null -ne $staleOpened) {
        throw 'The scene-open completion reader accepted a pre-action diagnostic as post-action evidence.'
    }
    $currentOpenedLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_document_persistence document_object_count=3 name=Current Draft relative_path=Projects/Current.hscene action=scene_opened relative_path_empty=0.'
    [System.IO.File]::AppendAllText($structuredTelemetryPath, $currentOpenedLine + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    $currentOpened = Get-HenkaSceneOpenedAfterOffset -Path $structuredTelemetryPath -StartingOffset $openedOffset -TimeoutMilliseconds 100
    $null = Assert-HenkaStructuredState -Description 'Post-action scene-open diagnostic regression' -Actual $currentOpened -Expected @{
        Action = 'scene_opened'
        Name = 'Current Draft'
        RelativePath = 'Projects/Current.hscene'
        RelativePathEmpty = '0'
        DocumentObjectCount = '3'
    }

    $transitionResultLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_result action=close result=0 active_name=Close Draft active_path=sandbox3d_scene.hscene object_count=2 selected_entity=2097164.'
    $transitionResult = ConvertFrom-HenkaSceneTransitionResultLine -Line $transitionResultLine
    $reorderedTransitionResultLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_result selected_entity=2097164 active_path=sandbox3d_scene.hscene action=close object_count=2 result=0 active_name=Close Draft.'
    $reorderedTransitionResult = ConvertFrom-HenkaSceneTransitionResultLine -Line $reorderedTransitionResultLine
    if ($null -eq $transitionResult -or
        $null -eq $reorderedTransitionResult -or
        $transitionResult.Action -ne $reorderedTransitionResult.Action -or
        $transitionResult.Result -ne $reorderedTransitionResult.Result -or
        $transitionResult.ActiveName -ne $reorderedTransitionResult.ActiveName -or
        $transitionResult.ActivePath -ne $reorderedTransitionResult.ActivePath -or
        $transitionResult.ObjectCount -ne $reorderedTransitionResult.ObjectCount -or
        $transitionResult.SelectedEntity -ne $reorderedTransitionResult.SelectedEntity) {
        throw 'Reordered scene-transition result telemetry did not preserve the same structured state.'
    }
    if ($null -ne (ConvertFrom-HenkaSceneTransitionResultLine -Line ($transitionResultLine -replace 'result=0', 'result=0 result=1'))) {
        throw 'The scene-transition result parser accepted an ambiguous duplicate result.'
    }

    $terrainSaveLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_terrain_save result=0 dirty_before=2 saved=2 dirty_after=0.'
    $terrainSave = ConvertFrom-HenkaSceneTransitionTerrainSaveLine -Line $terrainSaveLine
    $reorderedTerrainSave = ConvertFrom-HenkaSceneTransitionTerrainSaveLine -Line 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_terrain_save saved=2 dirty_after=0 dirty_before=2 result=0.'
    if ($null -eq $terrainSave -or
        $null -eq $reorderedTerrainSave -or
        $terrainSave.Result -ne $reorderedTerrainSave.Result -or
        $terrainSave.DirtyBefore -ne $reorderedTerrainSave.DirtyBefore -or
        $terrainSave.Saved -ne $reorderedTerrainSave.Saved -or
        $terrainSave.DirtyAfter -ne $reorderedTerrainSave.DirtyAfter) {
        throw 'Reordered terrain-save telemetry did not preserve its structured result.'
    }
    if ($null -ne (ConvertFrom-HenkaSceneTransitionTerrainSaveLine -Line ($terrainSaveLine -replace 'saved=2', 'saved=2 saved=0')) -or
        $null -ne (ConvertFrom-HenkaSceneTransitionTerrainSaveLine -Line ($terrainSaveLine -replace ' dirty_after=0', ''))) {
        throw 'The terrain-save parser accepted duplicate or incomplete state fields.'
    }

    Write-Output 'Henka UI automation helper tests passed.'
}
finally {
    if ($null -ne $exclusiveStream) { $exclusiveStream.Dispose() }
    if ($null -ne $liveLogWriter) { $liveLogWriter.Dispose() }
    if ($null -ne $detached) { $detached.Dispose() }
    if ($null -ne $source) { $source.Dispose() }
    if (Test-Path -LiteralPath $testDirectory -PathType Container) {
        $rootItem = Get-Item -LiteralPath $testDirectory -Force
        if (($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing to remove a reparse-point test directory: $testDirectory"
        }
        Remove-Item -LiteralPath $testDirectory -Recurse -Force -ErrorAction Stop
        if (Test-Path -LiteralPath $testDirectory) {
            throw "The exact UI capture test directory remains after cleanup: $testDirectory"
        }
    }
}
