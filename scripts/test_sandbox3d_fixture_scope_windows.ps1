param(
    [string]$RepositoryRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
} else {
    $RepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
}

$sandboxCMakePath = Join-Path $RepositoryRoot "examples\sandbox3d\CMakeLists.txt"
$packageScriptPath = Join-Path $RepositoryRoot "scripts\package_sandbox3d_windows.ps1"
$packageValidationPath = Join-Path $RepositoryRoot "scripts\check_packaged_sandbox3d_windows.ps1"
$sandboxSourcePath = Join-Path $RepositoryRoot "examples\sandbox3d\main.c"
$generatorScriptPath = Join-Path $RepositoryRoot "scripts\generate_residency_fixtures_windows.ps1"
$genericModelingScriptPath = Join-Path $RepositoryRoot "scripts\test_visible_native_modeling_windows.ps1"
$windowsCiWorkflowPath = Join-Path $RepositoryRoot ".github\workflows\windows-ci.yml"

foreach ($path in @($sandboxCMakePath, $packageScriptPath, $packageValidationPath, $sandboxSourcePath, $generatorScriptPath, $genericModelingScriptPath, $windowsCiWorkflowPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Fixture-scope contract input is missing: $path"
    }
}

$sandboxLines = Get-Content -LiteralPath $sandboxCMakePath
$sandboxText = $sandboxLines -join "`n"
$packageText = Get-Content -LiteralPath $packageScriptPath -Raw
$packageValidationText = Get-Content -LiteralPath $packageValidationPath -Raw
$sandboxSourceText = Get-Content -LiteralPath $sandboxSourcePath -Raw
$genericModelingText = Get-Content -LiteralPath $genericModelingScriptPath -Raw
$windowsCiWorkflowText = Get-Content -LiteralPath $windowsCiWorkflowPath -Raw

function Test-UsesPrimitiveGalleryArgument {
    param([Parameter(Mandatory = $true)][string]$ScriptText)

    $argumentLists = [System.Text.RegularExpressions.Regex]::Matches(
        $ScriptText,
        '(?is)-Arguments\s+@\((?<arguments>[^)]*)\)')
    foreach ($argumentList in $argumentLists) {
        if ([System.Text.RegularExpressions.Regex]::IsMatch(
                $argumentList.Groups['arguments'].Value,
                '(?:''--primitive-gallery''|"--primitive-gallery")')) {
            return $true
        }
    }
    return $false
}

# Keep the validator independent of PowerShell string-literal quote style and
# prove it rejects a neighboring, non-matching switch as well.
$galleryArgumentCases = @(
    [pscustomobject]@{ Text = "-Arguments @('--primitive-gallery')"; Expected = $true },
    [pscustomobject]@{ Text = '-Arguments @("--primitive-gallery")'; Expected = $true },
    [pscustomobject]@{ Text = "-Arguments @('--other', '--primitive-gallery')"; Expected = $true },
    [pscustomobject]@{ Text = "-Arguments @('--primitive-gallery-extra')"; Expected = $false },
    [pscustomobject]@{ Text = "-Arguments @('--other')"; Expected = $false }
)
foreach ($case in $galleryArgumentCases) {
    if ((Test-UsesPrimitiveGalleryArgument -ScriptText $case.Text) -ne $case.Expected) {
        throw "Primitive-gallery argument parser regression for: $($case.Text)"
    }
}

$insideNormalPostBuild = $false
$normalPostBuildLines = New-Object System.Collections.Generic.List[string]
foreach ($line in $sandboxLines) {
    if ($line -match 'add_custom_command\s*\(\s*TARGET\s+henka_sandbox3d\s+POST_BUILD') {
        $insideNormalPostBuild = $true
        $normalPostBuildLines.Clear()
        $normalPostBuildLines.Add([string]$line)
        continue
    }
    if ($insideNormalPostBuild) {
        $normalPostBuildLines.Add([string]$line)
        if ($line.Trim() -eq ')') {
            $normalPostBuildText = $normalPostBuildLines -join "`n"
            if ($normalPostBuildText -match '(?i)residency' -and
                $normalPostBuildText -notmatch '(?i)remove_directory') {
                throw "Normal Sandbox3D POST_BUILD commands must not generate residency stress fixtures."
            }
            $insideNormalPostBuild = $false
        }
    }
}

if ($insideNormalPostBuild) {
    throw "The normal Sandbox3D POST_BUILD command is not structurally closed."
}
if ($sandboxText -notmatch '(?s)add_custom_target\(\s*henka_sandbox3d_residency_fixtures') {
    throw "The residency fixture generation target is not explicit and opt-in."
}
if ($sandboxText -notmatch '(?s)remove_directory\s+\$<TARGET_FILE_DIR:henka_sandbox3d>/assets/textures/residency') {
    throw "Normal Sandbox3D builds must remove stale opt-in residency output."
}
if ($sandboxText -notmatch 'generate_residency_fixtures_windows\.ps1') {
    throw "The opt-in residency target does not use the bounded fixture generator."
}
if ($packageText -match '"henka_sandbox3d_residency_fixtures"') {
    throw "Packaging must not rebuild the validated executable through the residency custom target."
}
if ($packageText -notmatch 'generate_residency_fixtures_windows\.ps1') {
    throw "Packaging must generate opt-in residency fixtures directly after executable validation."
}
if ($packageText -match '(?s)\$residencyFixtureSource\s+-Destination.*?-Recurse') {
    throw "Packaging must copy only the bounded named residency fixtures, not the whole output directory."
}
if (-not (Test-UsesPrimitiveGalleryArgument -ScriptText $genericModelingText)) {
    throw "The visible native modeling workflow must explicitly identify its bounded primitive-gallery fixture."
}
if ($packageValidationText -notmatch 'DEFAULT_SCENE_READY ground=1 ground_editable=1 camera=1 showcase_assets=0 diagnostic_entities=0 scene_content=product_native') {
    throw "The packaged normal-startup validator must retain the clean product-native default-scene assertion."
}
if ($windowsCiWorkflowText -notmatch '(?m)check_packaged_sandbox3d_windows\.ps1\s+-NonInteractive\s+-ProductStartupPrimitiveOnly') {
    throw "Hosted Windows validation must execute the packaged clean-default startup gate independently of the gallery workflow."
}

function Test-ViewportShadingWaitsForCompletedFrame {
    param([Parameter(Mandatory = $true)][string]$ScriptText)

    $loopStart = $ScriptText.IndexOf('$shadingNames = @(', [System.StringComparison]::Ordinal)
    $loopEnd = $ScriptText.IndexOf('$bevelControlCount = @(', $loopStart, [System.StringComparison]::Ordinal)
    if ($loopStart -lt 0 -or $loopEnd -le $loopStart) {
        return $false
    }

    $loopText = $ScriptText.Substring($loopStart, $loopEnd - $loopStart)
    $modeFailureIndex = $loopText.IndexOf('if (-not $modeObserved)', [System.StringComparison]::Ordinal)
    $renderWaitIndex = $loopText.IndexOf('Wait-HenkaPackagedFrameRenderComplete', [System.StringComparison]::Ordinal)
    return $modeFailureIndex -ge 0 -and
        $renderWaitIndex -gt $modeFailureIndex -and
        $loopText.Contains('$renderBoundaryOffset') -and
        $loopText.Contains('-AfterFrameSequence $afterFrameSequence') -and
        $loopText.Contains('-NoProgressTimeoutMilliseconds 8000')
}

if (-not (Test-ViewportShadingWaitsForCompletedFrame -ScriptText $packageValidationText)) {
    throw "The packaged shading workflow must wait for a post-selection app-rendered frame with a stage-specific no-progress bound."
}
$shadingWithoutRenderBoundary = $packageValidationText -replace 'Wait-HenkaPackagedFrameRenderComplete', 'Wait-FileContainsAfterOffset'
if (Test-ViewportShadingWaitsForCompletedFrame -ScriptText $shadingWithoutRenderBoundary) {
    throw "The viewport shading contract regression accepted mode clicks without an application render-complete boundary."
}
Write-Output "[pass] Viewport shading waits for an application render-complete boundary; its missing-boundary negative control fails."

function Test-ViewportShadingUsesPerControlGeometry {
    param(
        [Parameter(Mandatory = $true)][string]$ApplicationSource,
        [Parameter(Mandatory = $true)][string]$ValidationScript
    )

    $loopStart = $ValidationScript.IndexOf('$shadingNames = @(', [System.StringComparison]::Ordinal)
    $loopEnd = $ValidationScript.IndexOf('$bevelControlCount = @(', $loopStart, [System.StringComparison]::Ordinal)
    if ($loopStart -lt 0 -or $loopEnd -le $loopStart) {
        return $false
    }
    $loopText = $ValidationScript.Substring($loopStart, $loopEnd - $loopStart)
    return $ApplicationSource.Contains('Viewport shading control: mode=%s x=%.1f y=%.1f width=%.1f height=%.1f') -and
        $loopText.Contains('Get-HenkaViewportShadingControl') -and
        $loopText.Contains('$shadingControl.Width * 0.5') -and
        $loopText.Contains('$shadingControl.Height * 0.5') -and
        -not $loopText.Contains('$shadingButtonWidth')
}

if (-not (Test-ViewportShadingUsesPerControlGeometry `
        -ApplicationSource $sandboxSourceText `
        -ValidationScript $packageValidationText)) {
    throw "The packaged shading workflow must click each mode using its application-reported individual control rectangle."
}
$uniformShadingRegression = $packageValidationText.Replace(
    'Get-HenkaViewportShadingControl',
    'Get-LastLogRegexMatch') + '$shadingButtonWidth'
if (Test-ViewportShadingUsesPerControlGeometry `
        -ApplicationSource $sandboxSourceText `
        -ValidationScript $uniformShadingRegression) {
    throw "The per-control shading geometry regression accepted the old uniform-width coordinate assumption."
}
Write-Output "[pass] Viewport shading automation uses exact individual button rectangles; uniform-width negative control fails."

function Test-GameAuthoringLifecycleUsesCreatedProductObject {
    param([Parameter(Mandatory = $true)][string]$ScriptText)

    $fixtureStart = $ScriptText.IndexOf(
        'Write-Step "Preparing a product-authored object for Game Authoring lifecycle"',
        [System.StringComparison]::Ordinal)
    $lifecycleStart = $ScriptText.IndexOf(
        'Write-Step "Checking Game Authoring Play lifecycle"',
        [System.StringComparison]::Ordinal)
    if ($fixtureStart -lt 0 -or $lifecycleStart -le $fixtureStart) {
        return $false
    }

    $fixtureText = $ScriptText.Substring(
        $fixtureStart,
        $lifecycleStart - $fixtureStart)
    return $fixtureText.Contains('Click-FramebufferPoint') -and
        $fixtureText.Contains('Wait-FileContainsAfterOffset') -and
        $fixtureText.Contains('action=part-added parts=[0-9]+') -and
        $fixtureText.Contains('$gameAuthoringAddCubeY = $sceneObjectsY + 60.0') -and
        [regex]::IsMatch(
            $fixtureText,
            '-FramebufferY\s+\(\$gameAuthoringAddCubeY\s*\+\s*12\.0\)')
}

if (-not (Test-GameAuthoringLifecycleUsesCreatedProductObject -ScriptText $packageValidationText)) {
    throw "The packaged Game Authoring lifecycle must create a real object through the visible Henka authoring UI and wait for the production object-registration result before testing Physics/Play."
}
$gameAuthoringLifecycleMarker = 'Write-Step "Checking Game Authoring Play lifecycle"'
$gameAuthoringMarkerIndex = $packageValidationText.IndexOf(
    $gameAuthoringLifecycleMarker,
    [System.StringComparison]::Ordinal)
$brokenGameAuthoringFixture = $packageValidationText.Substring($gameAuthoringMarkerIndex)
if (Test-GameAuthoringLifecycleUsesCreatedProductObject -ScriptText $brokenGameAuthoringFixture) {
    throw "The Game Authoring fixture regression negative control accepted a lifecycle with its object-creation setup removed."
}

function Test-SourceRowTelemetryIsSelectionBound {
    param([Parameter(Mandatory = $true)][string]$SourceText)

    return [regex]::IsMatch(
        $SourceText,
        'if\s*\(\s*state->native_authoring_source_row_reported_entity\s*!=\s*entity\s*&&\s*sandbox3d_get_real_selected_entity\(state\)\s*==\s*entity\s*&&')
}

if (-not (Test-SourceRowTelemetryIsSelectionBound -SourceText $sandboxSourceText)) {
    throw "Automation source-row telemetry must report only the selected source entity so unselected scene rows cannot keep alternating and flooding runtime output."
}
$unboundedSourceRowText = $sandboxSourceText -replace 'sandbox3d_get_real_selected_entity\(state\)\s*==\s*entity\s*&&\s*', ''
if (Test-SourceRowTelemetryIsSelectionBound -SourceText $unboundedSourceRowText) {
    throw "The source-row telemetry regression negative control accepted logging without a selected-entity guard."
}

function Test-GameAuthoringStepControlsUseReportedScroll {
    param([Parameter(Mandatory = $true)][string]$ScriptText)

    return $ScriptText.Contains('$gameStepPattern = ''Game authoring step controls:') -and
        $ScriptText.Contains('$gameStepMatch = Scroll-DetailsUntilReported') -and
        $ScriptText.Contains('-PostconditionPattern $gameStepPattern')
}

if (-not (Test-GameAuthoringStepControlsUseReportedScroll -ScriptText $packageValidationText)) {
    throw "The packaged Game Authoring Step/Stop gate must scroll until the app reports that lower control row instead of reading only the currently visible log state."
}
$withoutGameStepProgressScroll = $packageValidationText.Replace(
    '$gameStepMatch = Scroll-DetailsUntilReported',
    '$gameStepMatch = Get-LastLogRegexMatch')
if (Test-GameAuthoringStepControlsUseReportedScroll -ScriptText $withoutGameStepProgressScroll) {
    throw "The Game Authoring Step/Stop scroll regression negative control accepted a direct log lookup."
}

Write-Output "[pass] Sandbox3D residency fixtures are excluded from normal builds and generated only by the explicit stress target."
Write-Output "[pass] Visible native modeling explicitly uses the bounded primitive gallery; quote-style parser controls passed."
Write-Output "[pass] Hosted packaged startup retains an independent clean product-native default-scene gate."
Write-Output "[pass] Game Authoring Physics/Play validation requires an app-created object; its missing-fixture negative control fails."
Write-Output "[pass] Native authoring source-row telemetry is selection-bound; its unbounded-output negative control fails."
Write-Output "[pass] Game Authoring Step/Stop uses app-reported progress scrolling; its direct-lookup negative control fails."
