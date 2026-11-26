param(
    [string]$RepositoryRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")
. (Join-Path $PSScriptRoot "henka_ui_automation_helpers.ps1")

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
} else {
    $RepositoryRoot = (Resolve-Path -LiteralPath $RepositoryRoot).Path
}

$checkerPath = Join-Path $RepositoryRoot "scripts\check_packaged_sandbox3d_windows.ps1"
if (-not (Test-Path -LiteralPath $checkerPath -PathType Leaf)) {
    throw "Packaged Sandbox checker was not found: $checkerPath"
}
$sceneObjectsHelperPath = Join-Path $RepositoryRoot "scripts\henka_packaged_scene_objects.ps1"
if (-not (Test-Path -LiteralPath $sceneObjectsHelperPath -PathType Leaf)) {
    throw "Packaged Scene Objects validation helper was not found: $sceneObjectsHelperPath"
}

$tokens = $null
$parseErrors = $null
$checkerAst = [System.Management.Automation.Language.Parser]::ParseFile(
    $checkerPath,
    [ref]$tokens,
    [ref]$parseErrors)
if ($parseErrors.Count -ne 0) {
    throw "Packaged Sandbox checker has PowerShell parse errors: $($parseErrors[0].Message)"
}
$automationHelpersPath = Join-Path $RepositoryRoot "scripts\henka_ui_automation_helpers.ps1"
if (-not (Test-Path -LiteralPath $automationHelpersPath -PathType Leaf)) {
    throw "The shared UI automation helper file was not found: $automationHelpersPath"
}
$automationTokens = $null
$automationParseErrors = $null
$automationHelpersAst = [System.Management.Automation.Language.Parser]::ParseFile(
    $automationHelpersPath,
    [ref]$automationTokens,
    [ref]$automationParseErrors)
if ($automationParseErrors.Count -ne 0) {
    throw "The shared UI automation helper file has PowerShell parse errors: $($automationParseErrors[0].Message)"
}
$sceneTokens = $null
$sceneParseErrors = $null
$sceneObjectsAst = [System.Management.Automation.Language.Parser]::ParseFile(
    $sceneObjectsHelperPath,
    [ref]$sceneTokens,
    [ref]$sceneParseErrors)
if ($sceneParseErrors.Count -ne 0) {
    throw "Packaged Scene Objects helper has PowerShell parse errors: $($sceneParseErrors[0].Message)"
}

$waitFunction = $automationHelpersAst.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq "Wait-FileContainsAfterOffset"
}, $true)
if ($null -eq $waitFunction) {
    throw "The shared UI automation helpers do not define their log-wait helper."
}
$logMatchFunction = $checkerAst.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq "Get-LastLogRegexMatch"
}, $true)
if ($null -eq $logMatchFunction) {
    throw "The packaged checker does not define its last-log match helper."
}
foreach ($sharedHelperName in @(
        "Wait-FileContainsAfterOffset",
        "Get-FileLengthSafe",
        "Normalize-HenkaLogLineEndings")) {
    $duplicateCheckerFunction = $checkerAst.Find({
        param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
            $node.Name -eq $sharedHelperName
    }, $true)
    if ($null -ne $duplicateCheckerFunction) {
        throw "The packaged checker duplicates shared helper '$sharedHelperName' instead of using the canonical helper."
    }
}
$requiredSceneFunctions = @(
    "Wait-HenkaSceneObjectsRowDrawAfterOffset",
    "Get-HenkaSceneObjectsPagerReport",
    "Get-HenkaSceneObjectsControlReport",
    "Get-HenkaSceneObjectsControlClickPoint",
    "Move-HenkaSceneObjectsPagerThroughUi",
    "Test-HenkaSceneObjectsRowAuthorityCurrent",
    "Get-HenkaSceneObjectsOneLinePanelHeightFromRow",
    "Find-HenkaSceneObjectsRowAuthorityThroughPager")
foreach ($functionName in $requiredSceneFunctions) {
    $sceneFunction = $sceneObjectsAst.Find({
        param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
            $node.Name -eq $functionName
    }, $true)
    if ($null -eq $sceneFunction) {
        throw "The packaged Scene Objects helper is missing $functionName."
    }
}
$checkerSource = [System.IO.File]::ReadAllText($checkerPath)
$genericAuthoringStart = $checkerSource.IndexOf(
    'Write-Step "Checking generic engine-native asset authoring"',
    [StringComparison]::Ordinal)
if ($genericAuthoringStart -lt 0) {
    throw "The packaged checker generic-authoring boundary could not be identified for geometry-authority review."
}
$genericAuthoringEnd = $checkerSource.IndexOf(
    'Write-Step "Checking clean close-window shutdown"',
    $genericAuthoringStart,
    [StringComparison]::Ordinal)
if ($genericAuthoringEnd -le $genericAuthoringStart) {
    throw "The packaged checker generic-authoring boundary could not be identified for geometry-authority review."
}
$genericAuthoringSource = $checkerSource.Substring(
    $genericAuthoringStart,
    $genericAuthoringEnd - $genericAuthoringStart)
foreach ($controlId in @(
        'asset_name', 'new_asset', 'add_cube', 'save_asset', 'close_asset', 'open_asset')) {
    if ($genericAuthoringSource -notmatch "Get-GenericSceneObjectsControlTarget -Id '$controlId'") {
        throw "The packaged checker does not resolve $controlId from product-owned control geometry."
    }
}
if (-not $genericAuthoringSource.Contains("`$genericPrimitiveIds = @('cylinder', 'cone', 'quad_sphere')") -or
    -not $genericAuthoringSource.Contains('Get-GenericSceneObjectsControlTarget -Id $primitiveId')) {
    throw "The packaged checker primitive loop does not resolve Cylinder, Cone, and Quad Sphere through product-owned geometry."
}
if ($genericAuthoringSource -match 'genericActionWidth|genericPrimitiveActionWidth|genericPrimitiveX|genericOpenAssetX') {
    throw "The packaged generic-authoring path still duplicates measured production control geometry."
}
$fileLengthFunction = $automationHelpersAst.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq "Get-FileLengthSafe"
}, $true)
$framebufferRectFunction = $checkerAst.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq "Assert-FramebufferRect"
}, $true)
if ($null -eq $fileLengthFunction -or $null -eq $framebufferRectFunction) {
    throw "The packaged checker is missing the path-length or framebuffer-geometry authority required by pager tests."
}
$normalizerFunction = $automationHelpersAst.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq "Normalize-HenkaLogLineEndings"
}, $true)
if ($null -eq $normalizerFunction) {
    throw "The shared UI automation helpers do not define the canonical log line normalizer."
}
. ([ScriptBlock]::Create($normalizerFunction.Extent.Text))
. ([ScriptBlock]::Create($waitFunction.Extent.Text))
. ([ScriptBlock]::Create($logMatchFunction.Extent.Text))
. ([ScriptBlock]::Create($fileLengthFunction.Extent.Text))
. ([ScriptBlock]::Create($framebufferRectFunction.Extent.Text))
. $sceneObjectsHelperPath

$temporaryRoot = New-HenkaTemporaryDirectory `
    -RepositoryRoot $RepositoryRoot `
    -Purpose "packaged-log-crlf-regression"
$logPath = Join-Path $temporaryRoot "windows-crlf.log"

try {
    $encoding = [System.Text.UTF8Encoding]::new($false)
    $prefix = "earlier product output`r`n"
    [System.IO.File]::WriteAllText($logPath, $prefix, $encoding)
    $startingOffset = (Get-Item -LiteralPath $logPath).Length
    $record = "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_layout result=ready panel_height=291.0.`r`n"
    [System.IO.File]::AppendAllText($logPath, $record, $encoding)

    $recordPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_layout result=ready panel_height=291\.0\.$'
    if (-not (Wait-FileContainsAfterOffset `
            -Path $logPath `
            -Pattern $recordPattern `
            -StartingOffset $startingOffset `
            -TimeoutMilliseconds 1000)) {
        throw "The packaged checker missed a new CRLF-terminated product record after the captured byte offset."
    }
    Write-Output "[pass] Packaged checker observes CRLF product records after an exact byte offset"

    $absentPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_layout result=error panel_height=291\.0\.$'
    if (Wait-FileContainsAfterOffset `
            -Path $logPath `
            -Pattern $absentPattern `
            -StartingOffset $startingOffset `
            -TimeoutMilliseconds 250) {
        throw "An absent product record incorrectly satisfied the packaged checker wait."
    }
    Write-Output "[pass] Packaged checker rejects an absent CRLF product record"

    $lastMatch = Get-LastLogRegexMatch -Path $logPath -Pattern $recordPattern
    if ($null -eq $lastMatch -or $lastMatch.Value -ne $record.TrimEnd("`r", "`n")) {
        throw "The packaged checker did not extract a CRLF-terminated product record."
    }
    Write-Output "[pass] Packaged checker extracts CRLF product records with anchored patterns"

    $missingMatch = Get-LastLogRegexMatch -Path $logPath -Pattern $absentPattern
    if ($null -ne $missingMatch) {
        throw "An absent CRLF product record incorrectly produced a last-log match."
    }
    Write-Output "[pass] Packaged checker returns no match for absent CRLF product records"

    $boundedFramePrefix = (1..256 | ForEach-Object {
        "HENKA_AUTOMATION_DIAGNOSTIC frame seq=$_ phase=render-complete`r`n"
    }) -join ""
    $saturatedLogPath = Join-Path $temporaryRoot "bounded-frame-sampling.log"
    $layoutRecord = "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_layout result=ready panel_height=291.0.`r`n"
    [System.IO.File]::WriteAllText($saturatedLogPath, $boundedFramePrefix + $layoutRecord, $encoding)
    $postLayoutOffset = (Get-Item -LiteralPath $saturatedLogPath).Length
    $drawRecord = "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_authority entity=2097153 name=Ground display=Ground [Hidden] label_lines=1.`r`n"
    [System.IO.File]::AppendAllText($saturatedLogPath, $drawRecord, $encoding)
    if (-not (Wait-HenkaSceneObjectsRowDrawAfterOffset `
            -Path $saturatedLogPath `
            -StartingOffset $postLayoutOffset `
            -TimeoutMilliseconds 1000)) {
        throw "A product-owned row draw after layout was missed when the bounded generic frame-report budget was exhausted."
    }
    Write-Output "[pass] Stage-specific Scene Objects draw evidence remains observable after the 256-frame diagnostic budget"

    $noDrawLogPath = Join-Path $temporaryRoot "bounded-frame-no-draw.log"
    [System.IO.File]::WriteAllText($noDrawLogPath, $boundedFramePrefix + $layoutRecord, $encoding)
    $noDrawOffset = (Get-Item -LiteralPath $noDrawLogPath).Length
    if (Wait-HenkaSceneObjectsRowDrawAfterOffset `
            -Path $noDrawLogPath `
            -StartingOffset $noDrawOffset `
            -TimeoutMilliseconds 250) {
        throw "A saturated generic frame log without a Scene Objects row draw incorrectly satisfied the stage-specific wait."
    }
    Write-Output "[pass] Saturated generic frame telemetry cannot substitute for a missing Scene Objects row draw"

    $pagerLogPath = Join-Path $temporaryRoot "scene-objects-pager.log"
    $pagerRecord = "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_pager page=0 page_count=2 can_previous=0 can_next=1 previous_rect=30.0,305.0,74.0,24.0 next_rect=184.0,305.0,74.0,24.0.`r`n"
    [System.IO.File]::WriteAllText($pagerLogPath, $pagerRecord, $encoding)
    $pager = Get-HenkaSceneObjectsPagerReport -Path $pagerLogPath
    if ($null -eq $pager -or $pager.Page -ne 0 -or $pager.PageCount -ne 2 -or
        $pager.CanPrevious -or -not $pager.CanNext -or
        $pager.NextRect.X -ne 184.0 -or $pager.NextRect.Y -ne 305.0 -or
        $pager.NextRect.Width -ne 74.0 -or $pager.NextRect.Height -ne 24.0) {
        throw "The packaged checker did not parse the real Next-page rectangle and page state from product telemetry."
    }
    Write-Output "[pass] Product pager telemetry resolves the page state and real Next-button hit rectangle"

    $malformedPagerPath = Join-Path $temporaryRoot "scene-objects-pager-malformed.log"
    [System.IO.File]::WriteAllText(
        $malformedPagerPath,
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_pager page=0 page_count=2 can_previous=0 can_next=1 next_rect=184.0,305.0,74.0,24.0.`r`n",
        $encoding)
    if ($null -ne (Get-HenkaSceneObjectsPagerReport -Path $malformedPagerPath)) {
        throw "A pager record without the product-owned previous-button geometry was accepted as complete."
    }
    Write-Output "[pass] Incomplete pager telemetry cannot authorize a synthetic page click"

    $controlReportPath = Join-Path $temporaryRoot "scene-objects-control-geometry.log"
    $controlRecords = @(
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_control id=cone available=1 asset_open=1 rect=123.0,81.0,62.0,24.0.",
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_control id=save_asset available=0 asset_open=0 rect=0.0,0.0,0.0,0.0."
    ) -join "`r`n"
    [System.IO.File]::WriteAllText($controlReportPath, $controlRecords + "`r`n", $encoding)
    $coneControl = Get-HenkaSceneObjectsControlReport -Path $controlReportPath -Id "cone"
    if ($null -eq $coneControl -or -not $coneControl.Available -or
        $coneControl.Rect.X -ne 123.0 -or $coneControl.Rect.Y -ne 81.0 -or
        $coneControl.Rect.Width -ne 62.0 -or $coneControl.Rect.Height -ne 24.0 -or
        -not $coneControl.AssetOpen -or
        [Math]::Abs(($coneControl.Rect.X + $coneControl.Rect.Width / 2.0) - 154.0) -gt 0.001) {
        throw "The packaged checker did not preserve and center the product-owned unequal-width Cone rectangle."
    }
    $coneTarget = Get-HenkaSceneObjectsControlClickPoint `
        -Path $controlReportPath -Id "cone" -ExpectedAssetOpen $true `
        -SceneObjects ([PSCustomObject]@{ X = 100.0; Y = 50.0; Width = 200.0; Height = 100.0 }) `
        -FramebufferWidth 1280 -FramebufferHeight 720
    if ($coneTarget.X -ne 154.0 -or $coneTarget.Y -ne 93.0 -or
        $coneTarget.X -eq 202.5) {
        throw "The packaged target resolver did not use the actual product Cone hit rectangle instead of the equal-cell guess."
    }
    $unavailableControl = Get-HenkaSceneObjectsControlReport -Path $controlReportPath -Id "save_asset"
    if ($null -eq $unavailableControl -or $unavailableControl.Available -or
        $unavailableControl.Rect.Width -ne 0.0 -or $unavailableControl.Rect.Height -ne 0.0) {
        throw "An unavailable Scene Objects control incorrectly produced an actionable hit rectangle."
    }
    $unavailableRejected = $false
    try {
        $null = Get-HenkaSceneObjectsControlClickPoint `
            -Path $controlReportPath -Id "save_asset" -ExpectedAssetOpen $true `
            -SceneObjects ([PSCustomObject]@{ X = 100.0; Y = 50.0; Width = 200.0; Height = 100.0 }) `
            -FramebufferWidth 1280 -FramebufferHeight 720
    }
    catch {
        $unavailableRejected = $true
    }
    if (-not $unavailableRejected) {
        throw "The packaged click-target resolver authorized an unavailable button."
    }
    Write-Output "[pass] Unequal text-measured control geometry is parsed from product telemetry; unavailable controls cannot authorize clicks"

    $nativeControlRecords = @(
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_control id=asset_name available=1 asset_open=0 rect=30.0,219.0,216.0,24.0.",
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_control id=new_asset available=1 asset_open=0 rect=30.0,249.0,84.0,24.0."
    ) -join "`r`n"
    [System.IO.File]::WriteAllText($controlReportPath, $nativeControlRecords + "`r`n", $encoding)
    $scenePanel = [PSCustomObject]@{ X = 16.0; Y = 16.0; Width = 256.0; Height = 688.0 }
    $assetNameTarget = Get-HenkaSceneObjectsControlClickPoint `
        -Path $controlReportPath -Id "asset_name" -ExpectedAssetOpen $false `
        -SceneObjects $scenePanel -FramebufferWidth 1280 -FramebufferHeight 720
    $newAssetTarget = Get-HenkaSceneObjectsControlClickPoint `
        -Path $controlReportPath -Id "new_asset" -ExpectedAssetOpen $false `
        -SceneObjects $scenePanel -FramebufferWidth 1280 -FramebufferHeight 720
    $staleNameClickIntersectsField =
        144.0 -ge $assetNameTarget.Rect.X -and
        144.0 -lt ($assetNameTarget.Rect.X + $assetNameTarget.Rect.Width) -and
        191.0 -ge $assetNameTarget.Rect.Y -and
        191.0 -lt ($assetNameTarget.Rect.Y + $assetNameTarget.Rect.Height)
    $staleNewAssetClickIntersectsButton =
        71.0 -ge $newAssetTarget.Rect.X -and
        71.0 -lt ($newAssetTarget.Rect.X + $newAssetTarget.Rect.Width) -and
        221.0 -ge $newAssetTarget.Rect.Y -and
        221.0 -lt ($newAssetTarget.Rect.Y + $newAssetTarget.Rect.Height)
    if ($assetNameTarget.X -ne 138.0 -or $assetNameTarget.Y -ne 231.0 -or
        $newAssetTarget.X -ne 72.0 -or $newAssetTarget.Y -ne 261.0 -or
        $staleNameClickIntersectsField -or $staleNewAssetClickIntersectsButton) {
        throw "Native authoring targets did not use the product-reported name-field and New Asset rectangles."
    }
    Write-Output "[pass] Native asset name and New Asset clicks resolve from their product-reported rectangles, not stale panel-relative guesses"

    $controlTransitionLogPath = Join-Path $temporaryRoot "scene-objects-control-transition.log"
    $createdTransition = "Native asset document: name=FreshAsset action=created parts=0."
    $staleQuadSphereControl = "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_control id=quad_sphere available=1 asset_open=0 rect=36.0,279.0,72.0,24.0."
    [System.IO.File]::WriteAllText(
        $controlTransitionLogPath,
        $staleQuadSphereControl + "`r`n" + $createdTransition + "`r`n",
        $encoding)
    $createdTransitionMatch = Get-LastLogRegexMatch `
        -Path $controlTransitionLogPath `
        -Pattern ([Regex]::Escape($createdTransition))
    if ($null -eq $createdTransitionMatch) {
        throw "The synthetic asset-created transition could not be located in the focused state-wait regression."
    }
    $staleControlFailure = $null
    try {
        $null = Get-HenkaSceneObjectsControlClickPoint `
            -Path $controlTransitionLogPath -Id "quad_sphere" -ExpectedAssetOpen $true `
            -AfterLogIndex $createdTransitionMatch.Index `
            -StateWaitTimeoutMilliseconds 100 -StatePollIntervalMilliseconds 10 `
            -SceneObjects $scenePanel -FramebufferWidth 1280 -FramebufferHeight 720
    }
    catch {
        $staleControlFailure = $_.Exception.Message
    }
    if ($staleControlFailure -notmatch 'fresh.*after log index') {
        throw "A pre-creation Quad Sphere report did not fail the bounded post-transition state wait with a stale-authority diagnostic."
    }
    $freshQuadSphereControl = "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_control id=quad_sphere available=1 asset_open=1 rect=36.0,279.0,72.0,24.0."
    [System.IO.File]::AppendAllText(
        $controlTransitionLogPath,
        $freshQuadSphereControl + "`r`n",
        $encoding)
    $freshQuadSphereTarget = Get-HenkaSceneObjectsControlClickPoint `
        -Path $controlTransitionLogPath -Id "quad_sphere" -ExpectedAssetOpen $true `
        -AfterLogIndex $createdTransitionMatch.Index `
        -StateWaitTimeoutMilliseconds 100 -StatePollIntervalMilliseconds 10 `
        -SceneObjects $scenePanel -FramebufferWidth 1280 -FramebufferHeight 720
    if (-not $freshQuadSphereTarget.Report.Available -or
        -not $freshQuadSphereTarget.Report.AssetOpen -or
        $freshQuadSphereTarget.Report.LogIndex -le $createdTransitionMatch.Index -or
        $freshQuadSphereTarget.X -ne 72.0 -or $freshQuadSphereTarget.Y -ne 291.0) {
        throw "A fresh post-creation Quad Sphere report did not authorize its product-reported click center."
    }
    Write-Output "[pass] Pre-creation control telemetry cannot authorize a post-creation click; a fresh product report after the transition can"

    $malformedControlPath = Join-Path $temporaryRoot "scene-objects-control-geometry-malformed.log"
    [System.IO.File]::WriteAllText(
        $malformedControlPath,
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_control id=cone available=1 asset_open=1 rect=0.0,0.0,0.0,0.0.`r`n",
        $encoding)
    if ($null -ne (Get-HenkaSceneObjectsControlReport -Path $malformedControlPath -Id "cone")) {
        throw "Invalid available-control geometry was accepted as a real Scene Objects hit target."
    }
    Write-Output "[pass] Invalid product control geometry cannot authorize a packaged click"
    Write-Output "[pass] Packaged authoring consumes product rectangles for every interaction and rejects equal-cell geometry guesses"

    function Send-HenkaBackgroundFramebufferClickAndWait {
        param(
            [System.IntPtr]$Handle,
            [int]$FramebufferWidth,
            [int]$FramebufferHeight,
            [double]$FramebufferX,
            [double]$FramebufferY,
            [string]$StdoutPath,
            [string]$Description
        )
        $script:observedPagerClick = [PSCustomObject]@{ X = $FramebufferX; Y = $FramebufferY }
        $pageChange = @(
            "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_layout result=ready panel_x=0.0 panel_y=0.0 panel_width=320.0 panel_height=400.0 row_start_y=20.0 footer_y=366.0 available_row_height=346.0 line_height=20.0 requested_page=1 page=1 page_count=2 first_visible=1 visible_count=1 hierarchy_rows=2 ground_index_valid=1 ground_index=0 ground_measured_lines=2.",
            "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_authority entity=2097153 name=New Cube display=New Cube [Hidden] label_lines=1 hidden=1 selected=0 page=1 page_count=2 row_index=1 first_visible=1 visible_count=1 available_row_height=30.0 line_height=20.0 natural_height=40.0 row_x=10.0 row_y=20.0 row_width=100.0 row_height=30.0 visible_line_capacity=1 formatter=hidden-one-line text_x=18.0 text_y=25.0 hitbox_uses_same_row_rect=1 clicked=0.",
            "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_pager page=1 page_count=2 can_previous=1 can_next=0 previous_rect=30.0,305.0,74.0,24.0 next_rect=184.0,305.0,74.0,24.0."
        ) -join "`r`n"
        [System.IO.File]::AppendAllText($StdoutPath, $pageChange + "`r`n", [System.Text.UTF8Encoding]::new($false))
    }

    $rowAuthorityLogPath = Join-Path $temporaryRoot "scene-objects-row-authority.log"
    $rowAuthorityLog = @(
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_layout result=ready panel_x=0.0 panel_y=0.0 panel_width=320.0 panel_height=400.0 row_start_y=20.0 footer_y=366.0 available_row_height=346.0 line_height=20.0 requested_page=0 page=0 page_count=2 first_visible=0 visible_count=1 hierarchy_rows=2 ground_index_valid=1 ground_index=0 ground_measured_lines=2.",
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_authority entity=2097153 name=New Cube display=New Cube [Hidden] label_lines=1 hidden=1 selected=0 page=0 page_count=2 row_index=0 first_visible=0 visible_count=1 available_row_height=30.0 line_height=20.0 natural_height=40.0 row_x=10.0 row_y=20.0 row_width=100.0 row_height=30.0 visible_line_capacity=1 formatter=hidden-one-line text_x=18.0 text_y=25.0 hitbox_uses_same_row_rect=1 clicked=0.",
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_pager page=0 page_count=2 can_previous=0 can_next=1 previous_rect=30.0,305.0,74.0,24.0 next_rect=184.0,305.0,74.0,24.0."
    ) -join "`r`n"
    [System.IO.File]::WriteAllText($rowAuthorityLogPath, $rowAuthorityLog + "`r`n", $encoding)
    $visibleRow = Find-HenkaSceneObjectsRowAuthorityThroughPager `
        -Name "New Cube" `
        -Entity "2097153" `
        -Handle ([System.IntPtr]::Zero) `
        -FramebufferWidth 1280 `
        -FramebufferHeight 720 `
        -SceneObjects ([PSCustomObject]@{ X = 0.0; Y = 0.0; Width = 320.0; Height = 400.0 }) `
        -StdoutPath $rowAuthorityLogPath
    if ($null -eq $visibleRow -or $visibleRow.Groups['entity'].Value -ne '2097153' -or
        $visibleRow.Groups['display'].Value -cne 'New Cube [Hidden]' -or
        $visibleRow.Groups['lines'].Value -ne '1' -or
        $visibleRow.Groups['sameRect'].Value -ne '1' -or
        $visibleRow.Groups['x'].Value -ne '10.0') {
        throw "The checker failed to correlate product row geometry with its layout when the pager record follows the row draw."
    }
    Write-Output "[pass] Visible row identity, hidden state, line count, and hitbox geometry are correlated to the current product page despite draw-order telemetry"

    $staleRowLogPath = Join-Path $temporaryRoot "scene-objects-row-authority-stale.log"
    $staleRowLog = @(
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_authority entity=2097153 name=New Cube display=New Cube [Hidden] label_lines=1 hidden=1 selected=0 page=0 page_count=2 row_index=0 first_visible=0 visible_count=1 available_row_height=30.0 line_height=20.0 natural_height=40.0 row_x=10.0 row_y=20.0 row_width=100.0 row_height=30.0 visible_line_capacity=1 formatter=hidden-one-line text_x=18.0 text_y=25.0 hitbox_uses_same_row_rect=1 clicked=0.",
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_layout result=ready panel_x=0.0 panel_y=0.0 panel_width=320.0 panel_height=400.0 row_start_y=20.0 footer_y=366.0 available_row_height=346.0 line_height=20.0 requested_page=0 page=0 page_count=2 first_visible=0 visible_count=1 hierarchy_rows=2 ground_index_valid=1 ground_index=0 ground_measured_lines=2.",
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_pager page=0 page_count=2 can_previous=0 can_next=1 previous_rect=30.0,305.0,74.0,24.0 next_rect=184.0,305.0,74.0,24.0."
    ) -join "`r`n"
    [System.IO.File]::WriteAllText($staleRowLogPath, $staleRowLog + "`r`n", $encoding)
    $staleRowPattern = '^HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_authority entity=2097153 name=New Cube display=(?<display>[^\r\n]*) label_lines=(?<lines>\d+) hidden=(?<hidden>[01]) selected=(?<selected>[01]) page=(?<page>\d+) page_count=(?<pageCount>\d+) .+\.$'
    $staleRow = Get-LastLogRegexMatch -Path $staleRowLogPath -Pattern $staleRowPattern
    $stalePager = Get-HenkaSceneObjectsPagerReport -Path $staleRowLogPath
    if ($null -eq $staleRow -or $null -eq $stalePager -or
        (Test-HenkaSceneObjectsRowAuthorityCurrent -Path $staleRowLogPath -Row $staleRow -Pager $stalePager)) {
        throw "A prior row-draw record incorrectly satisfied a later Scene Objects layout generation."
    }
    Write-Output "[pass] A row report emitted before the latest layout is rejected as stale authority"

    $postResizeRowPath = Join-Path $temporaryRoot "scene-objects-post-resize-row.log"
    $postResizeRowRecord = "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_authority entity=2097153 name=New Cube display=New Cube|- Hidden label_lines=2 hidden=1 selected=0 page=1 page_count=2 row_index=1 first_visible=1 visible_count=1 available_row_height=61.96 line_height=10.00 natural_height=32.00 row_x=30.0 row_y=243.0 row_width=228.0 row_height=32.0 visible_line_capacity=2 formatter=bounded-text text_x=38.0 text_y=249.0 hitbox_uses_same_row_rect=1 clicked=0."
    $postResizeRows = @(
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_layout result=ready panel_x=16.0 panel_y=16.0 panel_width=256.0 panel_height=323.0 row_start_y=243.0 footer_y=305.0 available_row_height=61.96 line_height=10.00 requested_page=1 page=1 page_count=2 first_visible=1 visible_count=1 hierarchy_rows=2 ground_index_valid=1 ground_index=0 ground_measured_lines=2.",
        $postResizeRowRecord,
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_pager page=1 page_count=2 can_previous=1 can_next=0 previous_rect=30.0,305.0,74.0,24.0 next_rect=184.0,305.0,74.0,24.0."
    ) -join "`r`n"
    [System.IO.File]::WriteAllText($postResizeRowPath, $postResizeRows + "`r`n", $encoding)
    $postResizeRow = Find-HenkaSceneObjectsRowAuthorityThroughPager `
        -Name "New Cube" `
        -Entity "2097153" `
        -Handle ([System.IntPtr]::Zero) `
        -FramebufferWidth 1280 `
        -FramebufferHeight 720 `
        -SceneObjects ([PSCustomObject]@{ X = 16.0; Y = 16.0; Width = 256.0; Height = 323.0 }) `
        -StdoutPath $postResizeRowPath
    if ($null -eq $postResizeRow) {
        throw "The row-derived compact-height regression fixture did not match the production telemetry format."
    }
    $recalculatedPanelHeight = Get-HenkaSceneObjectsOneLinePanelHeightFromRow `
        -Row $postResizeRow `
        -PanelY 16.0 `
        -CurrentPanelHeight 323.0 `
        -MinimumRowHeight 28.0 `
        -DesiredRowCapacity 30.0 `
        -FooterHeight 34.0 `
        -MinimumPanelHeight 180.0
    if ($null -eq $recalculatedPanelHeight -or [Math]::Abs($recalculatedPanelHeight - 291.0) -gt 0.01) {
        throw "The next divider target was not recalculated from the actual post-resize row y=243 and desired 30px row capacity."
    }
    Write-Output "[pass] Compact panel correction derives 291px from the post-resize row rather than stale pre-resize position"

    $visibleTwoLinePath = Join-Path $temporaryRoot "scene-objects-post-resize-visible-row.log"
    $visibleTwoLineRecords = $postResizeRows -replace 'hidden=1 selected=0', 'hidden=0 selected=0'
    [System.IO.File]::WriteAllText($visibleTwoLinePath, $visibleTwoLineRecords + "`r`n", $encoding)
    $visibleTwoLineRow = Find-HenkaSceneObjectsRowAuthorityThroughPager `
        -Name "New Cube" `
        -Entity "2097153" `
        -Handle ([System.IntPtr]::Zero) `
        -FramebufferWidth 1280 `
        -FramebufferHeight 720 `
        -SceneObjects ([PSCustomObject]@{ X = 16.0; Y = 16.0; Width = 256.0; Height = 323.0 }) `
        -StdoutPath $visibleTwoLinePath
    if ($null -ne (Get-HenkaSceneObjectsOneLinePanelHeightFromRow `
            -Row $visibleTwoLineRow -PanelY 16.0 -CurrentPanelHeight 323.0 `
            -MinimumRowHeight 28.0 -DesiredRowCapacity 30.0 -FooterHeight 34.0 `
            -MinimumPanelHeight 180.0)) {
        throw "A visible two-line row incorrectly authorized the hidden-row compact correction."
    }
    $alreadyCompactHeight = Get-HenkaSceneObjectsOneLinePanelHeightFromRow `
        -Row $postResizeRow -PanelY 16.0 -CurrentPanelHeight 291.0 `
        -MinimumRowHeight 28.0 -DesiredRowCapacity 30.0 -FooterHeight 34.0 `
        -MinimumPanelHeight 180.0
    if ($null -ne $alreadyCompactHeight) {
        throw "The bounded divider correction proposed a repeated target when the panel was already at that height."
    }
    Write-Output "[pass] Row-derived correction is limited to hidden two-line rows and cannot repeat an already-reached target"

    $transitionLogPath = Join-Path $temporaryRoot "scene-objects-pager-transition.log"
    [System.IO.File]::WriteAllText(
        $transitionLogPath,
        "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_pager page=0 page_count=2 can_previous=0 can_next=1 previous_rect=30.0,305.0,74.0,24.0 next_rect=184.0,305.0,74.0,24.0.`r`n",
        $encoding)
    $startingPager = Get-HenkaSceneObjectsPagerReport -Path $transitionLogPath
    $script:observedPagerClick = $null
    $transitionPager = Move-HenkaSceneObjectsPagerThroughUi `
        -Pager $startingPager `
        -Direction 1 `
        -Handle ([System.IntPtr]::Zero) `
        -FramebufferWidth 1280 `
        -FramebufferHeight 720 `
        -SceneObjects ([PSCustomObject]@{ X = 0.0; Y = 0.0; Width = 320.0; Height = 400.0 }) `
        -StdoutPath $transitionLogPath
    if ($transitionPager -isnot [PSCustomObject] -or $transitionPager.Page -ne 1 -or
        $transitionPager.PageCount -ne 2 -or $null -eq $script:observedPagerClick -or
        $script:observedPagerClick.X -ne 221.0 -or $script:observedPagerClick.Y -ne 317.0) {
        throw "The pager transition leaked progress output into its return value or failed to click the product-reported control center."
    }
    Write-Output "[pass] Pager transition returns only the new page authority and clicks the reported control center"
} finally {
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}

exit 0
