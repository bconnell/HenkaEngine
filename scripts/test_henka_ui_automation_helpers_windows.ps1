param(
    [Parameter(Mandatory = $true)][string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'henka_script_common.ps1')
. (Join-Path $PSScriptRoot 'henka_ui_automation_helpers.ps1')

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

    $transitionLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_decision action=open choice=cancel scene_retained=1 scene_dirty=1 terrain_dirty_regions=0 active_name=Untitled Scene Unsaved Open Draft active_path=GoldenPathSaveAs.hscene object_count=0 selected_entity=0 terrain_active=0.'
    $transitionDecision = ConvertFrom-HenkaSceneTransitionDecisionLine -Line $transitionLine
    if ($null -eq $transitionDecision -or
        $transitionDecision.Action -ne 'open' -or
        $transitionDecision.Choice -ne 'cancel' -or
        $transitionDecision.SceneRetained -ne '1' -or
        $transitionDecision.SceneDirty -ne '1' -or
        $transitionDecision.TerrainDirtyRegions -ne '0' -or
        $transitionDecision.ActiveName -ne 'Untitled Scene Unsaved Open Draft' -or
        $transitionDecision.ActivePath -ne 'GoldenPathSaveAs.hscene' -or
        $transitionDecision.ObjectCount -ne '0' -or
        $transitionDecision.SelectedEntity -ne '0' -or
        $transitionDecision.TerrainActive -ne '0') {
        throw 'The scene-transition decision parser did not preserve the actual packaged cancellation state.'
    }
    $reorderedTransitionLine = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_decision terrain_dirty_regions=0 active_path=Projects/Scene One.hscene terrain_active=0 selected_entity=2097153 action=open object_count=2 active_name=Open Draft With Spaces scene_retained=1 scene_dirty=1 choice=cancel.'
    $reorderedTransition = ConvertFrom-HenkaSceneTransitionDecisionLine -Line $reorderedTransitionLine
    if ($null -eq $reorderedTransition -or
        $reorderedTransition.Action -ne $transitionDecision.Action -or
        $reorderedTransition.Choice -ne $transitionDecision.Choice -or
        $reorderedTransition.SceneRetained -ne $transitionDecision.SceneRetained -or
        $reorderedTransition.SceneDirty -ne $transitionDecision.SceneDirty -or
        $reorderedTransition.TerrainDirtyRegions -ne $transitionDecision.TerrainDirtyRegions -or
        $reorderedTransition.ActiveName -ne 'Open Draft With Spaces' -or
        $reorderedTransition.ActivePath -ne 'Projects/Scene One.hscene' -or
        $reorderedTransition.ObjectCount -ne '2' -or
        $reorderedTransition.SelectedEntity -ne '2097153' -or
        $reorderedTransition.TerrainActive -ne '0') {
        throw 'Reordered transition telemetry did not parse to the same structured state.'
    }
    $transitionWithUnknownField = 'HENKA_AUTOMATION_DIAGNOSTIC scene_transition_decision action=open diagnostic_frame=812 scene_dirty=1 choice=cancel terrain_dirty_regions=0 active_name=Open Draft With Spaces active_path=Projects/Scene One.hscene object_count=2 selected_entity=2097153 terrain_active=0 scene_retained=1.'
    $transitionWithUnknownField = ConvertFrom-HenkaSceneTransitionDecisionLine -Line $transitionWithUnknownField
    if ($null -eq $transitionWithUnknownField -or
        $transitionWithUnknownField.ActiveName -ne 'Open Draft With Spaces' -or
        $transitionWithUnknownField.ActivePath -ne 'Projects/Scene One.hscene' -or
        $transitionWithUnknownField.SceneDirty -ne '1') {
        throw 'An added diagnostic field contaminated an existing structured scene-state value.'
    }
    if ($null -ne (ConvertFrom-HenkaSceneTransitionDecisionLine -Line ($transitionLine -replace 'scene_dirty=1', 'scene_dirty=1 scene_dirty=0'))) {
        throw 'The scene-transition parser accepted ambiguous duplicate state fields.'
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
