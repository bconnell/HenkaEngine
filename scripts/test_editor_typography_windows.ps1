param(
    [string]$PackageRoot = "",
    [string]$OutputDirectory = "",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")
. (Join-Path $PSScriptRoot "henka_ui_automation_helpers.ps1")
. (Join-Path $PSScriptRoot "henka_typography_package_policy.ps1")
. (Join-Path $PSScriptRoot "henka_typography_capture_policy.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$buildRoot = Get-HenkaBuildRoot -RepositoryRoot $repoRoot
if ([string]::IsNullOrWhiteSpace($PackageRoot)) {
    $PackageRoot = Get-HenkaPackageRoot -RepositoryRoot $repoRoot -PackageName "HenkaSandbox3D"
}
$PackageRoot = [System.IO.Path]::GetFullPath($PackageRoot)
$executable = Join-Path $PackageRoot "HenkaSandbox3D.exe"
$packageInfoPath = Join-Path $PackageRoot "PACKAGE_INFO.txt"
$buildInfoPath = Join-Path $buildRoot "henka-build-info.json"
$fontHashes = @{
    atkinson = "7FB917C89019896D0B52EE84B7CBB3304C18CB90B19A62F5E32712BD23E97669"
    noto = "478C558EA716033CD60C03438F628DFA75694DCF6B5F6D505A2F05FD2B4F3823"
}
$fontRelativePaths = @{
    atkinson = "assets\ui\fonts\AtkinsonHyperlegible-Regular.ttf"
    noto = "assets\ui\fonts\NotoSans-Regular.ttf"
}

foreach ($requiredPath in @($executable, $packageInfoPath, $buildInfoPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "The packaged typography candidate is incomplete: $requiredPath"
    }
}
$packageInfo = [System.IO.File]::ReadAllText($packageInfoPath)
$comparisonModeMatch = [regex]::Match(
    $packageInfo,
    '(?im)^Typography comparison candidates:\s*(included|excluded)\s*$')
if (-not $comparisonModeMatch.Success -or $comparisonModeMatch.Groups[1].Value -ne "included") {
    throw "Typography comparison requires an explicitly marked comparison package. Repackage with -IncludeTypographyComparisonFonts."
}
$selectedFontMetadata = Get-HenkaTypographySelectedFontMetadata
if ($packageInfo -notmatch ("(?im)^Typography font SHA-256:\s*{0}\s*$" -f [regex]::Escape($selectedFontMetadata.Sha256)) -or
    $packageInfo -notmatch ("(?im)^Typography license SHA-256:\s*{0}\s*$" -f [regex]::Escape($selectedFontMetadata.LicenseSha256))) {
    throw "The comparison package marker does not identify the selected font and license bytes."
}
Assert-HenkaTypographyPackageAssetSet -PackageRoot $PackageRoot -IncludeComparisonFonts
$packageHashMatch = [regex]::Match(
    $packageInfo,
    '(?im)^Packaged executable SHA-256:\s*([0-9a-f]{64})\s*$')
if (-not $packageHashMatch.Success) {
    throw "PACKAGE_INFO does not contain the packaged executable SHA-256."
}
$executableHash = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToUpperInvariant()
if ($executableHash -ne $packageHashMatch.Groups[1].Value.ToUpperInvariant()) {
    throw "The package executable hash differs from PACKAGE_INFO."
}
$buildInfo = Get-Content -LiteralPath $buildInfoPath -Raw | ConvertFrom-Json
$expectedArtifact = "examples/sandbox3d/$Configuration/henka_sandbox3d.exe"
if ($buildInfo.artifact_relative_path -ne $expectedArtifact -or
    $buildInfo.artifact_sha256.ToUpperInvariant() -ne $executableHash) {
    throw "The build provenance does not identify the exact packaged Sandbox executable. Rebuild Sandbox3D before typography capture."
}
$buildInfoHash = (Get-FileHash -LiteralPath $buildInfoPath -Algorithm SHA256).Hash.ToUpperInvariant()
$packageInfoHash = (Get-FileHash -LiteralPath $packageInfoPath -Algorithm SHA256).Hash.ToUpperInvariant()
$comparisonScriptHash = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToUpperInvariant()
$automationHelperPath = Join-Path $PSScriptRoot "henka_ui_automation_helpers.ps1"
$automationHelperHash = (Get-FileHash -LiteralPath $automationHelperPath -Algorithm SHA256).Hash.ToUpperInvariant()
$localRoot = [System.IO.Path]::GetFullPath((Get-HenkaLocalRoot -RepositoryRoot $repoRoot)).TrimEnd('\') + '\'
if (-not $PackageRoot.StartsWith($localRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "The packaged typography candidate is outside the canonical Henka local root: $PackageRoot"
}
$packageRootRelativeToLocal = $PackageRoot.Substring($localRoot.Length)
foreach ($font in @("atkinson", "noto")) {
    $fontPath = Join-Path $PackageRoot $fontRelativePaths[$font]
    if (-not (Test-Path -LiteralPath $fontPath -PathType Leaf)) {
        throw "The packaged $font font asset is missing: $fontPath"
    }
    $fontHash = (Get-FileHash -LiteralPath $fontPath -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($fontHash -ne $fontHashes[$font]) {
        throw "The packaged $font font asset does not match its pinned source hash."
    }
}

$sourceIdentity = [string]$buildInfo.source_identity
if ([string]::IsNullOrWhiteSpace($sourceIdentity)) {
    throw "The exact Sandbox build has no source identity."
}
$validationHarnessIdentity = Get-HenkaSourceIdentity -RepoRoot $repoRoot
if ($validationHarnessIdentity.source_state -ne "clean") {
    throw "The typography comparison harness must run from a clean committed candidate."
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path (Get-HenkaEvidenceRoot -RepositoryRoot $repoRoot) `
        ("editor-typography-comparison-" + $sourceIdentity.Substring(0, 12))
}
$OutputDirectory = Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $OutputDirectory
if (Test-Path -LiteralPath $OutputDirectory) {
    throw "Refusing to overwrite existing typography comparison evidence: $OutputDirectory"
}
[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$scratchRoot = New-HenkaTemporaryDirectory -RepositoryRoot $repoRoot -Purpose "editor-typography-comparison"
$userDataRoot = Join-Path $scratchRoot "user-data"
[System.IO.Directory]::CreateDirectory($userDataRoot) | Out-Null
$frameCaptureDirectory = Join-Path $scratchRoot "renderer-frames"
[System.IO.Directory]::CreateDirectory($frameCaptureDirectory) | Out-Null
[System.IO.Directory]::CreateDirectory((Join-Path $scratchRoot "window-captures")) | Out-Null
Add-Type -AssemblyName System.Drawing
$captures = New-Object System.Collections.Generic.List[object]
$settingsScrollProofs = New-Object System.Collections.Generic.List[object]
$detailsCompactProofs = New-Object System.Collections.Generic.List[object]
$detachedContextProof = $null
$completed = $false

function Get-LogText {
    param([Parameter(Mandatory = $true)][string]$Path)
    return Read-HenkaSharedText -Path $Path
}

function Wait-LogLine {
    param(
        [Parameter(Mandatory = $true)]$CapturedProcess,
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [int]$TimeoutMilliseconds = 20000
    )
    $deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    do {
        if ($CapturedProcess.Process.HasExited) {
            throw "Packaged Sandbox exited before '$Pattern'. Captured stdout: $Path"
        }
        $lines = (Get-LogText -Path $Path) -split "`r?`n"
        for ($index = $lines.Length - 1; $index -ge 0; --$index) {
            if ($lines[$index] -match $Pattern) {
                return $lines[$index]
            }
        }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)
    throw "Timed out waiting for packaged typography state '$Pattern'. Log: $Path"
}

function Find-LastLogLine {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern
    )
    $lines = (Get-LogText -Path $Path) -split "`r?`n"
    for ($index = $lines.Length - 1; $index -ge 0; --$index) {
        if ($lines[$index] -match $Pattern) {
            return $lines[$index]
        }
    }
    return $null
}

function Wait-TypographyStableReportedGeometry {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][string]$GeometryPattern,
        [Parameter(Mandatory = $true)][string]$Description,
        [ValidateRange(1000, 30000)][int]$TimeoutMilliseconds = 10000
    )

    $framePattern = 'HENKA_AUTOMATION_DIAGNOSTIC frame seq=\d+ phase=render-complete'
    $deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    $lastFrame = -1L
    $lastGeometryLine = $null
    $stableRenderAdvances = 0

    do {
        if ($Session.Captured.Process.HasExited) {
            throw "Packaged Sandbox exited while waiting for stable $Description geometry. Log: $($Session.StdoutPath)"
        }

        $frameLine = Find-LastLogLine -Path $Session.StdoutPath -Pattern $framePattern
        $geometryLine = Find-LastLogLine -Path $Session.StdoutPath -Pattern $GeometryPattern
        $frameMatch = [regex]::Match($frameLine, 'frame seq=(?<sequence>\d+) phase=render-complete')
        if ($frameMatch.Success -and -not [string]::IsNullOrWhiteSpace($geometryLine)) {
            $frameSequence = [int64]::Parse(
                $frameMatch.Groups['sequence'].Value,
                [System.Globalization.CultureInfo]::InvariantCulture)
            if ($frameSequence -gt $lastFrame) {
                if ($geometryLine -eq $lastGeometryLine) {
                    $stableRenderAdvances++
                }
                else {
                    $lastGeometryLine = $geometryLine
                    $stableRenderAdvances = 0
                }
                $lastFrame = $frameSequence
                if ($stableRenderAdvances -ge 1) {
                    return $geometryLine
                }
            }
        }
        Start-Sleep -Milliseconds 50
    } while ((Get-Date) -lt $deadline)

    throw ("The product did not report stable $Description geometry across render progress. " +
        "last_frame=$lastFrame last_geometry=$lastGeometryLine log=$($Session.StdoutPath)")
}

function Wait-TypographyStableToolsControl {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [ValidateRange(1000, 30000)][int]$TimeoutMilliseconds = 10000
    )
    return Wait-TypographyStableReportedGeometry `
        -Session $Session `
        -GeometryPattern 'Scene View Tools control: x=-?[0-9.]+ y=-?[0-9.]+ width=[0-9.]+ height=[0-9.]+' `
        -Description 'Scene View Tools control' `
        -TimeoutMilliseconds $TimeoutMilliseconds
}

function Wait-TypographyStableUtilityContextHeader {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][ValidateSet(100, 125, 150, 200)][int]$Scale,
        [ValidateRange(1000, 30000)][int]$TimeoutMilliseconds = 10000
    )
    $pattern = 'HENKA_AUTOMATION_DIAGNOSTIC workspace_context_header id=utility x=-?[0-9.]+ y=-?[0-9.]+ width=[0-9.]+ height=[0-9.]+ visible=1 ui_scale_percent=' + $Scale + ' frame=\d+'
    return Wait-TypographyStableReportedGeometry `
        -Session $Session `
        -GeometryPattern $pattern `
        -Description 'Utility workspace context header' `
        -TimeoutMilliseconds $TimeoutMilliseconds
}

function Get-TypographyLastRenderCompleteSequence {
    param([Parameter(Mandatory = $true)]$Session)

    $line = Find-LastLogLine `
        -Path $Session.StdoutPath `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC frame seq=\d+ phase=render-complete'
    $match = [regex]::Match($line, 'frame seq=(?<sequence>\d+) phase=render-complete')
    if (-not $match.Success) {
        throw 'The packaged application has not reported a completed render frame.'
    }
    return [int64]::Parse(
        $match.Groups['sequence'].Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
}

function Wait-TypographyRenderCompleteAfterSequence {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][int64]$PreviousSequence,
        [ValidateRange(1000, 30000)][int]$TimeoutMilliseconds = 10000
    )

    $pattern = 'HENKA_AUTOMATION_DIAGNOSTIC frame seq=\d+ phase=render-complete'
    $deadline = (Get-Date).AddMilliseconds($TimeoutMilliseconds)
    do {
        if ($Session.Captured.Process.HasExited) {
            throw 'The packaged Sandbox exited before the detached Utility render completed.'
        }
        $line = Find-LastLogLine -Path $Session.StdoutPath -Pattern $pattern
        $match = [regex]::Match($line, 'frame seq=(?<sequence>\d+) phase=render-complete')
        if ($match.Success) {
            $sequence = [int64]::Parse(
                $match.Groups['sequence'].Value,
                [System.Globalization.CultureInfo]::InvariantCulture)
            if ($sequence -gt $PreviousSequence) {
                return [pscustomobject]@{ Sequence = $sequence; Line = $line }
            }
        }
        Start-Sleep -Milliseconds 50
    } while ((Get-Date) -lt $deadline)

    throw "The detached Utility did not complete a packaged render after attachment; previous frame sequence=$PreviousSequence."
}

function Get-ReportedRect {
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Prefix
    )
    $match = [regex]::Match(
        $Line,
        [regex]::Escape($Prefix) + ' x=(?<x>-?\d+(?:\.\d+)?) y=(?<y>-?\d+(?:\.\d+)?) width=(?<w>\d+(?:\.\d+)?) height=(?<h>\d+(?:\.\d+)?)')
    if (-not $match.Success) {
        throw "The product did not report a valid $Prefix rectangle: $Line"
    }
    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    return [pscustomobject]@{
        X = [double]::Parse($match.Groups['x'].Value, $culture)
        Y = [double]::Parse($match.Groups['y'].Value, $culture)
        Width = [double]::Parse($match.Groups['w'].Value, $culture)
        Height = [double]::Parse($match.Groups['h'].Value, $culture)
    }
}

function Get-ReportedScrollState {
    param([Parameter(Mandatory = $true)][string]$Line)
    $match = [regex]::Match(
        $Line,
        'controls_scroll_viewport page=camera_status .* offset=(?<offset>-?\d+(?:\.\d+)?) content_height=(?<content>\d+(?:\.\d+)?)')
    if (-not $match.Success) {
        throw "The product did not report a valid Camera/Status scroll state: $Line"
    }
    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    return [pscustomobject]@{
        Rect = Get-ReportedRect -Line $Line -Prefix 'controls_scroll_viewport page=camera_status'
        Offset = [double]::Parse($match.Groups['offset'].Value, $culture)
        ContentHeight = [double]::Parse($match.Groups['content'].Value, $culture)
    }
}

function Get-ReportedUtilitySettingsLayout {
    param([Parameter(Mandatory = $true)][string]$Line)

    $number = '-?\d+(?:\.\d+)?'
    $pattern = "HENKA_AUTOMATION_DIAGNOSTIC utility_settings_layout frame=(?<frame>\d+) viewport_x=(?<vx>$number) viewport_y=(?<vy>$number) viewport_width=(?<vw>$number) viewport_height=(?<vh>$number) content_height=(?<content>$number) scroll_offset=(?<offset>$number) smooth_snap_x=(?<sx>$number) smooth_snap_y=(?<sy>$number) smooth_snap_width=(?<sw>$number) smooth_snap_height=(?<sh>$number) smooth_snap_visible=(?<visible>[01]) smooth_snap_enabled=(?<enabled>[01])"
    $match = [regex]::Match($Line, $pattern)
    if (-not $match.Success) {
        throw "The product did not report a complete Utility Settings layout: $Line"
    }
    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    return [pscustomobject]@{
        Frame = [UInt64]::Parse($match.Groups['frame'].Value, $culture)
        Viewport = [pscustomobject]@{
            X = [double]::Parse($match.Groups['vx'].Value, $culture)
            Y = [double]::Parse($match.Groups['vy'].Value, $culture)
            Width = [double]::Parse($match.Groups['vw'].Value, $culture)
            Height = [double]::Parse($match.Groups['vh'].Value, $culture)
        }
        ContentHeight = [double]::Parse($match.Groups['content'].Value, $culture)
        ScrollOffset = [double]::Parse($match.Groups['offset'].Value, $culture)
        SmoothRect = [pscustomobject]@{
            X = [double]::Parse($match.Groups['sx'].Value, $culture)
            Y = [double]::Parse($match.Groups['sy'].Value, $culture)
            Width = [double]::Parse($match.Groups['sw'].Value, $culture)
            Height = [double]::Parse($match.Groups['sh'].Value, $culture)
        }
        SmoothVisible = $match.Groups['visible'].Value -eq '1'
        SmoothEnabled = $match.Groups['enabled'].Value -eq '1'
    }
}

function Get-ReportedUtilitySettingsScroll {
    param([Parameter(Mandatory = $true)][string]$Line)

    $number = '-?\d+(?:\.\d+)?'
    $pattern = "HENKA_AUTOMATION_DIAGNOSTIC utility_settings_scroll frame=(?<frame>\d+) viewport_x=$number viewport_y=$number viewport_width=$number viewport_height=$number content_height=$number delta=(?<delta>$number) offset=(?<before>$number)->(?<after>$number) handled=(?<handled>[01])"
    $match = [regex]::Match($Line, $pattern)
    if (-not $match.Success) {
        throw "The product did not report a complete Utility Settings scroll event: $Line"
    }
    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    return [pscustomobject]@{
        Frame = [UInt64]::Parse($match.Groups['frame'].Value, $culture)
        Delta = [double]::Parse($match.Groups['delta'].Value, $culture)
        OffsetBefore = [double]::Parse($match.Groups['before'].Value, $culture)
        OffsetAfter = [double]::Parse($match.Groups['after'].Value, $culture)
        Handled = $match.Groups['handled'].Value -eq '1'
    }
}

function Wait-TypographyUtilitySettingsLayout {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][UInt64]$AfterFrame
    )

    $deadline = (Get-Date).AddSeconds(6)
    do {
        if ($Session.Captured.Process.HasExited) {
            throw "Packaged Sandbox exited while observing Utility Settings layout. Captured stdout: $($Session.StdoutPath)"
        }
        $line = Find-LastLogLine `
            -Path $Session.StdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC utility_settings_layout frame=\d+'
        if (-not [string]::IsNullOrWhiteSpace($line)) {
            $layout = Get-ReportedUtilitySettingsLayout -Line $line
            if ($layout.Frame -gt $AfterFrame) {
                return $layout
            }
        }
        Start-Sleep -Milliseconds 50
    } while ((Get-Date) -lt $deadline)
    throw "Timed out waiting for a new product-owned Utility Settings layout after frame $AfterFrame. Log: $($Session.StdoutPath)"
}

function Wait-TypographyUtilitySettingsScroll {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][UInt64]$AfterFrame
    )

    $deadline = (Get-Date).AddSeconds(6)
    do {
        if ($Session.Captured.Process.HasExited) {
            throw "Packaged Sandbox exited while observing Utility Settings scrolling. Captured stdout: $($Session.StdoutPath)"
        }
        $line = Find-LastLogLine `
            -Path $Session.StdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC utility_settings_scroll frame=\d+'
        if (-not [string]::IsNullOrWhiteSpace($line)) {
            $scroll = Get-ReportedUtilitySettingsScroll -Line $line
            if ($scroll.Frame -gt $AfterFrame) {
                return $scroll
            }
        }
        Start-Sleep -Milliseconds 50
    } while ((Get-Date) -lt $deadline)
    throw "Timed out waiting for a new product-owned Utility Settings scroll event after frame $AfterFrame. Log: $($Session.StdoutPath)"
}

function Get-ReportedWorkspaceTopology {
    param([Parameter(Mandatory = $true)][string]$Line)

    $number = '-?\d+(?:\.\d+)?'
    $pattern = "HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=(?<cause>\S+) tools_visible=(?<tools>[01]) divider_count=(?<count>\d+) left_dock=(?<lx>$number),(?<ly>$number),(?<lw>$number),(?<lh>$number) scene_objects=(?<sx>$number),(?<sy>$number),(?<sw>$number),(?<sh>$number) controls=(?<cx>$number),(?<cy>$number),(?<cw>$number),(?<ch>$number) divider0=(?<dx>$number),(?<dy>$number),(?<dw>$number),(?<dh>$number)\."
    $match = [regex]::Match($Line, $pattern)
    if (-not $match.Success) {
        throw "The product did not report a complete workspace topology: $Line"
    }
    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    $rect = {
        param([string]$Prefix)
        return [pscustomobject]@{
            X = [double]::Parse($match.Groups[($Prefix + 'x')].Value, $culture)
            Y = [double]::Parse($match.Groups[($Prefix + 'y')].Value, $culture)
            Width = [double]::Parse($match.Groups[($Prefix + 'w')].Value, $culture)
            Height = [double]::Parse($match.Groups[($Prefix + 'h')].Value, $culture)
        }
    }
    $dock = & $rect 'l'
    $sceneObjects = & $rect 's'
    $controls = & $rect 'c'
    $divider = & $rect 'd'
    $toolsVisible = $match.Groups['tools'].Value -eq '1'
    $dividerCount = [int]::Parse($match.Groups['count'].Value, $culture)
    if ($dock.Width -le 0.0 -or $dock.Height -le 0.0) {
        throw "The reported left dock is empty: $Line"
    }
    if ($toolsVisible) {
        if ($dividerCount -ne 1 -or
            $controls.Width -le 0.0 -or $controls.Height -le 0.0 -or
            $divider.Width -le 0.0 -or $divider.Height -le 0.0 -or
            $divider.X -lt $dock.X -or $divider.Y -lt $dock.Y -or
            ($divider.X + $divider.Width) -gt ($dock.X + $dock.Width + 0.01) -or
            ($divider.Y + $divider.Height) -gt ($dock.Y + $dock.Height + 0.01)) {
            throw "The reported visible Controls dock or topology divider is invalid: $Line"
        }
    }
    else {
        $emptyControls = [Math]::Abs($controls.X) -le 0.01 -and
            [Math]::Abs($controls.Y) -le 0.01 -and
            [Math]::Abs($controls.Width) -le 0.01 -and
            [Math]::Abs($controls.Height) -le 0.01
        $emptyDivider = [Math]::Abs($divider.X) -le 0.01 -and
            [Math]::Abs($divider.Y) -le 0.01 -and
            [Math]::Abs($divider.Width) -le 0.01 -and
            [Math]::Abs($divider.Height) -le 0.01
        $sceneObjectsMatchDock = [Math]::Abs($sceneObjects.X - $dock.X) -le 0.01 -and
            [Math]::Abs($sceneObjects.Y - $dock.Y) -le 0.01 -and
            [Math]::Abs($sceneObjects.Width - $dock.Width) -le 0.01 -and
            [Math]::Abs($sceneObjects.Height - $dock.Height) -le 0.01
        if ($dividerCount -ne 0 -or -not $emptyControls -or -not $emptyDivider -or -not $sceneObjectsMatchDock) {
            throw "The reported collapsed workspace topology is inconsistent: $Line"
        }
    }
    return [pscustomobject]@{
        Cause = $match.Groups['cause'].Value
        ToolsVisible = $toolsVisible
        DividerCount = $dividerCount
        LeftDock = $dock
        SceneObjects = $sceneObjects
        Controls = $controls
        Divider = $divider
    }
}

function Get-TypographyRuntimeScales {
    param([Parameter(Mandatory = $true)]$Session)

    $runtimeScaleLine = Find-LastLogLine `
        -Path $Session.StdoutPath `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC editor_typography stage=(loaded|scale_applied) .* ui_scale_percent=\d+ os_display_scale=\d+/\d+'
    $match = [regex]::Match(
        [string]$runtimeScaleLine,
        'ui_scale_percent=(?<ui>\d+) os_display_scale=(?<numerator>\d+)/(?<denominator>\d+)')
    if (-not $match.Success) {
        throw 'The product did not report its active editor and display scales for pointer targeting.'
    }
    return [pscustomobject]@{
        EditorScalePercent = [int]::Parse(
            $match.Groups['ui'].Value,
            [System.Globalization.CultureInfo]::InvariantCulture)
        DisplayScaleNumerator = [uint32]::Parse(
            $match.Groups['numerator'].Value,
            [System.Globalization.CultureInfo]::InvariantCulture)
        DisplayScaleDenominator = [uint32]::Parse(
            $match.Groups['denominator'].Value,
            [System.Globalization.CultureInfo]::InvariantCulture)
    }
}

function Convert-TypographySessionLogicalPoint {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][double]$LogicalX,
        [Parameter(Mandatory = $true)][double]$LogicalY
    )

    $scales = Get-TypographyRuntimeScales -Session $Session
    $point = Convert-TypographyLogicalPointToFramebufferPoint `
        -LogicalX $LogicalX -LogicalY $LogicalY `
        -EditorScalePercent $scales.EditorScalePercent `
        -DisplayScaleNumerator $scales.DisplayScaleNumerator `
        -DisplayScaleDenominator $scales.DisplayScaleDenominator
    return [pscustomobject]@{
        X = $point.X
        Y = $point.Y
        EditorScalePercent = $scales.EditorScalePercent
        DisplayScaleNumerator = $scales.DisplayScaleNumerator
        DisplayScaleDenominator = $scales.DisplayScaleDenominator
    }
}

function Send-TypographyLogicalClick {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][double]$LogicalX,
        [Parameter(Mandatory = $true)][double]$LogicalY,
        [ValidateSet("left", "right", "middle")][string]$Button = "left"
    )

    $framebufferPoint = Convert-TypographySessionLogicalPoint `
        -Session $Session -LogicalX $LogicalX -LogicalY $LogicalY
    Send-HenkaAutomationClick `
        -EventPath $Session.EventPath `
        -X $framebufferPoint.X `
        -Y $framebufferPoint.Y `
        -Button $Button
}

function Send-TypographyLogicalScroll {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][double]$LogicalX,
        [Parameter(Mandatory = $true)][double]$LogicalY,
        [Parameter(Mandatory = $true)][double]$WheelDelta
    )

    $point = Convert-TypographySessionLogicalPoint `
        -Session $Session -LogicalX $LogicalX -LogicalY $LogicalY
    Send-HenkaAutomationScroll `
        -EventPath $Session.EventPath `
        -X $point.X `
        -Y $point.Y `
        -WheelDelta $WheelDelta
}

function Send-TypographyLogicalDrag {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][double]$StartLogicalX,
        [Parameter(Mandatory = $true)][double]$StartLogicalY,
        [Parameter(Mandatory = $true)][double]$EndLogicalX,
        [Parameter(Mandatory = $true)][double]$EndLogicalY
    )

    $start = Convert-TypographySessionLogicalPoint `
        -Session $Session -LogicalX $StartLogicalX -LogicalY $StartLogicalY
    $end = Convert-TypographySessionLogicalPoint `
        -Session $Session -LogicalX $EndLogicalX -LogicalY $EndLogicalY
    $startX = Format-HenkaAutomationFloat -Value $start.X
    $startY = Format-HenkaAutomationFloat -Value $start.Y
    $endX = Format-HenkaAutomationFloat -Value $end.X
    $endY = Format-HenkaAutomationFloat -Value $end.Y
    Send-HenkaAutomationEvent -EventPath $Session.EventPath -EventLine "move $startX $startY"
    Send-HenkaAutomationEvent -EventPath $Session.EventPath -EventLine "button left down $startX $startY"
    Send-HenkaAutomationEvent -EventPath $Session.EventPath -EventLine "move $endX $endY"
    Send-HenkaAutomationEvent -EventPath $Session.EventPath -EventLine "button left up $endX $endY"
}

function Convert-TypographyLogicalPointToFramebufferPoint {
    param(
        [Parameter(Mandatory = $true)][double]$LogicalX,
        [Parameter(Mandatory = $true)][double]$LogicalY,
        [Parameter(Mandatory = $true)][int]$EditorScalePercent,
        [Parameter(Mandatory = $true)][uint32]$DisplayScaleNumerator,
        [Parameter(Mandatory = $true)][uint32]$DisplayScaleDenominator
    )

    if ([double]::IsNaN($LogicalX) -or [double]::IsInfinity($LogicalX) -or
        [double]::IsNaN($LogicalY) -or [double]::IsInfinity($LogicalY) -or
        $LogicalX -lt 0.0 -or $LogicalY -lt 0.0 -or
        $EditorScalePercent -notin @(100, 125, 150, 200) -or
        $DisplayScaleNumerator -eq 0 -or $DisplayScaleDenominator -eq 0) {
        throw 'The product-reported typography pointer transform was invalid.'
    }
    $displayScale = [double]$DisplayScaleNumerator / [double]$DisplayScaleDenominator
    $compositeScale = ($EditorScalePercent / 100.0) * $displayScale
    if ([double]::IsNaN($compositeScale) -or [double]::IsInfinity($compositeScale) -or
        $displayScale -gt 4.0 -or $compositeScale -le 0.0 -or $compositeScale -gt 8.0) {
        throw 'The product-reported typography pointer transform exceeded its supported scale bounds.'
    }
    $framebufferX = $LogicalX * $compositeScale
    $framebufferY = $LogicalY * $compositeScale
    if ([double]::IsNaN($framebufferX) -or [double]::IsInfinity($framebufferX) -or
        [double]::IsNaN($framebufferY) -or [double]::IsInfinity($framebufferY) -or
        $framebufferX -gt 65536.0 -or $framebufferY -gt 65536.0) {
        throw 'The transformed typography pointer exceeded the automation coordinate bounds.'
    }
    return [pscustomobject]@{ X = $framebufferX; Y = $framebufferY }
}

foreach ($case in @(
    @{ Ui = 100; Num = 1; Den = 1; X = 50.0; Y = 40.0; ExpectedX = 50.0; ExpectedY = 40.0 },
    @{ Ui = 125; Num = 1; Den = 1; X = 50.0; Y = 40.0; ExpectedX = 62.5; ExpectedY = 50.0 },
    @{ Ui = 150; Num = 3; Den = 2; X = 50.0; Y = 40.0; ExpectedX = 112.5; ExpectedY = 90.0 },
    @{ Ui = 200; Num = 2; Den = 1; X = 50.0; Y = 40.0; ExpectedX = 200.0; ExpectedY = 160.0 }
)) {
    $transformed = Convert-TypographyLogicalPointToFramebufferPoint `
        -LogicalX $case.X -LogicalY $case.Y `
        -EditorScalePercent $case.Ui `
        -DisplayScaleNumerator $case.Num -DisplayScaleDenominator $case.Den
    if ([Math]::Abs($transformed.X - $case.ExpectedX) -gt 0.001 -or
        [Math]::Abs($transformed.Y - $case.ExpectedY) -gt 0.001) {
        throw "Typography pointer transform regression at UI $($case.Ui)% and OS scale $($case.Num)/$($case.Den)."
    }
}

$toolsGeometryProbe = Get-ReportedRect `
    -Line 'Scene View Tools control: x=112.5 y=20.25 width=62.0 height=22.0.' `
    -Prefix 'Scene View Tools control:'
if ($toolsGeometryProbe.X -ne 112.5 -or
    $toolsGeometryProbe.Y -ne 20.25 -or
    $toolsGeometryProbe.Width -ne 62.0 -or
    $toolsGeometryProbe.Height -ne 22.0) {
    throw "The product geometry parser failed its bounded decimal/trailing-punctuation regression."
}

$pointerProbeLog = Join-Path $scratchRoot "pointer-scale-probe.stdout.log"
$pointerProbeEvents = Join-Path $scratchRoot "pointer-scale-probe.events"
[System.IO.File]::WriteAllText(
    $pointerProbeLog,
    "HENKA_AUTOMATION_DIAGNOSTIC editor_typography stage=loaded font=atkinson ui_scale_percent=200 os_display_scale=3/2`n")
[System.IO.File]::WriteAllText($pointerProbeEvents, "")
$pointerProbeSession = [pscustomobject]@{
    StdoutPath = $pointerProbeLog
    EventPath = $pointerProbeEvents
}
Send-TypographyLogicalClick `
    -Session $pointerProbeSession `
    -LogicalX 25.0 `
    -LogicalY 40.0
$pointerProbeText = [System.IO.File]::ReadAllText($pointerProbeEvents)
if ($pointerProbeText -notmatch '(?m)^move 75 120\r?\nbutton left down 75 120\r?\nbutton left up 75 120\r?\n?$') {
    throw "The scaled logical click did not use the product's Editor UI Scale and display-scale transform: $pointerProbeText"
}
$topologyProbe = Get-ReportedWorkspaceTopology -Line 'HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=tools_toggle tools_visible=1 divider_count=1 left_dock=12.0,12.0,239.6,516.0 scene_objects=12.0,12.0,239.6,335.0 controls=12.0,348.0,239.6,180.0 divider0=12.0,342.5,239.6,10.0.'
if (-not $topologyProbe.ToolsVisible -or $topologyProbe.DividerCount -ne 1 -or
    [Math]::Abs($topologyProbe.Controls.Height - 180.0) -gt 0.01 -or
    $topologyProbe.Divider.Y -ne 342.5) {
    throw 'The workspace-topology parser did not preserve the product-reported divider and dock geometry.'
}
$collapsedTopologyProbe = Get-ReportedWorkspaceTopology -Line 'HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=tools_toggle tools_visible=0 divider_count=0 left_dock=16.0,16.0,256.0,688.0 scene_objects=16.0,16.0,256.0,688.0 controls=0.0,0.0,0.0,0.0 divider0=0.0,0.0,0.0,0.0.'
if ($collapsedTopologyProbe.ToolsVisible -or $collapsedTopologyProbe.DividerCount -ne 0 -or
    $collapsedTopologyProbe.SceneObjects.Width -ne $collapsedTopologyProbe.LeftDock.Width -or
    $collapsedTopologyProbe.SceneObjects.Height -ne $collapsedTopologyProbe.LeftDock.Height -or
    $collapsedTopologyProbe.Controls.Width -ne 0.0 -or $collapsedTopologyProbe.Divider.Height -ne 0.0) {
    throw 'The workspace-topology parser did not preserve the product-reported collapsed Tools layout.'
}
$inconsistentCollapsedTopologyRejected = $false
try {
    Get-ReportedWorkspaceTopology -Line 'HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=tools_toggle tools_visible=0 divider_count=1 left_dock=16.0,16.0,256.0,688.0 scene_objects=16.0,16.0,256.0,688.0 controls=0.0,0.0,0.0,0.0 divider0=0.0,0.0,0.0,0.0.' | Out-Null
}
catch {
    $inconsistentCollapsedTopologyRejected = $true
}
if (-not $inconsistentCollapsedTopologyRejected) {
    throw 'The workspace-topology parser accepted a collapsed Tools state with a contradictory divider count.'
}

function Get-TypographyCaptureScalePlan {
    param(
        [Parameter(Mandatory = $true)][int]$CurrentScalePercent,
        [Parameter(Mandatory = $true)][int]$TargetScalePercent
    )
    $allowedScales = @(100, 125, 150, 200)
    if ($allowedScales -notcontains $CurrentScalePercent -or
        $allowedScales -notcontains $TargetScalePercent) {
        throw "Unsupported Editor UI Scale transition: $CurrentScalePercent% to $TargetScalePercent%."
    }
    if ($CurrentScalePercent -ne $TargetScalePercent) {
        return $TargetScalePercent
    }
    $refreshScale = if ($TargetScalePercent -eq 100) { 125 } else { 100 }
    return @($refreshScale, $TargetScalePercent)
}

$sameScaleRefreshPlan = @(
    Get-TypographyCaptureScalePlan -CurrentScalePercent 100 -TargetScalePercent 100)
if ($sameScaleRefreshPlan.Count -ne 2 -or
    $sameScaleRefreshPlan[0] -ne 125 -or
    $sameScaleRefreshPlan[1] -ne 100) {
    throw 'A same-scale capture must traverse another real scale and return to the requested scale to refresh its product framebuffer.'
}
$changedScalePlan = @(
    Get-TypographyCaptureScalePlan -CurrentScalePercent 125 -TargetScalePercent 100)
if ($changedScalePlan.Count -ne 1 -or $changedScalePlan[0] -ne 100) {
    throw 'A normal scale change must use one direct product UI transition.'
}
$sameHighScaleRefreshPlan = @(
    Get-TypographyCaptureScalePlan -CurrentScalePercent 200 -TargetScalePercent 200)
if ($sameHighScaleRefreshPlan.Count -ne 2 -or
    $sameHighScaleRefreshPlan[0] -ne 100 -or
    $sameHighScaleRefreshPlan[1] -ne 200) {
    throw 'A same-scale capture at 200 percent must use the bounded 100 percent refresh step.'
}

function Start-TypographyCandidateProcess {
    param(
        [Parameter(Mandatory = $true)][string]$Font,
        [Parameter(Mandatory = $true)][string]$Label,
        [Parameter(Mandatory = $true)][string]$CaptureLabel,
        [Parameter(Mandatory = $true)][string]$UserDataRoot
    )
    $eventPath = Join-Path $scratchRoot ("{0}.events" -f $Label)
    $stdoutPath = Join-Path $scratchRoot ("{0}.stdout.log" -f $Label)
    $stderrPath = Join-Path $scratchRoot ("{0}.stderr.log" -f $Label)
    [System.IO.File]::WriteAllText($eventPath, "", (New-Object System.Text.UTF8Encoding($false)))

    $environmentNames = @(
        "HENKA_AUTOMATION_INPUT_OWNED",
        "HENKA_AUTOMATION_DIAGNOSTICS",
        "HENKA_AUTOMATION_START_HIDDEN",
        "HENKA_AUTOMATION_TOOL_WINDOWS_START_HIDDEN",
        "HENKA_AUTOMATION_INPUT_FILE",
        "HENKA_AUTOMATION_USER_DATA_BASE_PATH",
        "HENKA_AUTOMATION_EDITOR_FONT",
        "HENKA_AUTOMATION_FRAME_CAPTURE_DIRECTORY",
        "HENKA_AUTOMATION_FRAME_CAPTURE_LABEL")
    $oldEnvironment = @{}
    foreach ($name in $environmentNames) {
        $oldEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
    }
    $captured = $null
    try {
        $env:HENKA_AUTOMATION_INPUT_OWNED = "1"
        $env:HENKA_AUTOMATION_DIAGNOSTICS = "1"
        $env:HENKA_AUTOMATION_START_HIDDEN = "1"
        $env:HENKA_AUTOMATION_TOOL_WINDOWS_START_HIDDEN = "1"
        $env:HENKA_AUTOMATION_INPUT_FILE = $eventPath
        $env:HENKA_AUTOMATION_USER_DATA_BASE_PATH = $UserDataRoot
        $env:HENKA_AUTOMATION_EDITOR_FONT = $Font
        $env:HENKA_AUTOMATION_FRAME_CAPTURE_DIRECTORY = $frameCaptureDirectory
        $env:HENKA_AUTOMATION_FRAME_CAPTURE_LABEL = $CaptureLabel
        $captured = Start-HenkaCapturedProcess `
            -FilePath $executable `
            -WorkingDirectory $PackageRoot `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath `
            -StartMinimized:$false `
            -StartHidden
    }
    finally {
        foreach ($name in $environmentNames) {
            [Environment]::SetEnvironmentVariable($name, $oldEnvironment[$name], "Process")
        }
    }
    if ($null -eq $captured) {
        throw "Packaged Sandbox process creation returned no process handle."
    }
    $window = [IntPtr]::Zero
    try {
        [void](Wait-LogLine `
            -CapturedProcess $captured `
            -Path $stderrPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC window_focus_policy focusable=0 input_focused=0' `
            -TimeoutMilliseconds 30000)
        [void](Wait-LogLine `
            -CapturedProcess $captured `
            -Path $stdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC window_start_hidden=1' `
            -TimeoutMilliseconds 30000)
        $windowDeadline = (Get-Date).AddSeconds(30)
        do {
            $window = Find-HenkaAutomationWindow `
                -ProcessId $captured.Process.Id `
                -TitleContains "Henka Engine Sandbox 3D" `
                -IncludeHidden
            if ($window -ne [IntPtr]::Zero) { break }
            if ($captured.Process.HasExited) { throw "Packaged Sandbox exited before its hidden main window was created." }
            Start-Sleep -Milliseconds 100
        } while ((Get-Date) -lt $windowDeadline)
        if ($window -eq [IntPtr]::Zero) { throw "Packaged Sandbox hidden main window was not found." }
        $windowVisible = [HenkaUiAutomationNative]::IsWindowVisible($window)
        $windowIconic = [HenkaUiAutomationNative]::IsIconic($window)
        $foregroundWindow = [HenkaUiAutomationNative]::GetForegroundWindow()
        if ($windowVisible -or $windowIconic -or $foregroundWindow -eq $window) {
            throw ("The typography candidate must remain hidden and non-foreground " +
                "(visible=$windowVisible iconic=$windowIconic foreground=$($foregroundWindow.ToInt64()) target=$($window.ToInt64())).")
        }
    }
    catch {
        Close-HenkaCapturedProcess $captured
        throw
    }
    return [pscustomobject]@{
        Captured = $captured
        EventPath = $eventPath
        StdoutPath = $stdoutPath
        StderrPath = $stderrPath
        CaptureDirectory = $frameCaptureDirectory
        CaptureLabel = $CaptureLabel
        Window = $window
        WindowPresentation = "hidden"
    }
}

function Ensure-TypographySettingsVisible {
    param([Parameter(Mandatory = $true)]$Session)
    $topologyLine = Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=startup tools_visible=[01] divider_count=\d+'
    if ($topologyLine -match 'tools_visible=0') {
        $toolsLine = Wait-TypographyStableToolsControl -Session $Session
        $toolsRect = Get-ReportedRect -Line $toolsLine -Prefix 'Scene View Tools control:'
        Send-TypographyLogicalClick `
            -Session $Session `
            -LogicalX ($toolsRect.X + $toolsRect.Width / 2.0) `
            -LogicalY ($toolsRect.Y + $toolsRect.Height / 2.0)
        $topologyLine = Wait-LogLine `
            -CapturedProcess $Session.Captured `
            -Path $Session.StdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=tools_toggle tools_visible=1 divider_count=\d+'
    }
    $topology = Get-ReportedWorkspaceTopology -Line $topologyLine
    if (-not $topology.ToolsVisible) {
        throw "The product did not expose the Controls dock after the real Tools interaction: $topologyLine"
    }
    if ($topology.Controls.Height -le 188.0) {
        if ($topology.DividerCount -ne 1) {
            throw "The Camera/Status page needs more than 188 logical pixels, but the visible Controls dock has no real topology divider: $topologyLine"
        }
        $targetControlsHeight = 240.0
        $increase = $targetControlsHeight - $topology.Controls.Height
        $startX = $topology.Divider.X + ($topology.Divider.Width * 0.5)
        $startY = $topology.Divider.Y + ($topology.Divider.Height * 0.5)
        $endY = $startY - $increase
        if ($increase -le 0.0 -or $endY -lt ($topology.LeftDock.Y + 100.0)) {
            throw "The real Controls topology divider cannot be moved safely to reach its supported scroll viewport: $topologyLine"
        }
        Send-TypographyLogicalDrag `
            -Session $Session `
            -StartLogicalX $startX `
            -StartLogicalY $startY `
            -EndLogicalX $startX `
            -EndLogicalY $endY
        $updatedTopologyLine = Wait-LogLine `
            -CapturedProcess $Session.Captured `
            -Path $Session.StdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=divider_release tools_visible=1 divider_count=\d+'
        $updatedTopology = Get-ReportedWorkspaceTopology -Line $updatedTopologyLine
        if ($updatedTopology.Controls.Height -le 188.0 -or
            $updatedTopology.Controls.Height -le $topology.Controls.Height + 0.5) {
            throw "The real product divider interaction did not reach a valid Controls height. Before=$($topology.Controls.Height), after=$($updatedTopology.Controls.Height), topology=$updatedTopologyLine"
        }
        $topology = $updatedTopology
    }
    $cameraStatusTabLine = Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC controls_page_rect id=camera_status x=-?[0-9.]+ y=-?[0-9.]+ width=[0-9.]+ height=[0-9.]+'
    $cameraStatusTabRect = Get-ReportedRect `
        -Line $cameraStatusTabLine `
        -Prefix 'HENKA_AUTOMATION_DIAGNOSTIC controls_page_rect id=camera_status'
    Send-TypographyLogicalClick `
        -Session $Session `
        -LogicalX ($cameraStatusTabRect.X + $cameraStatusTabRect.Width / 2.0) `
        -LogicalY ($cameraStatusTabRect.Y + $cameraStatusTabRect.Height / 2.0)
    [void](Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC controls_page id=camera_status selected=1')
    $scrollPattern = 'HENKA_AUTOMATION_DIAGNOSTIC controls_scroll_viewport page=camera_status .*'
    $scrollLine = Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern $scrollPattern
    $scrollState = Get-ReportedScrollState -Line $scrollLine
    for ($attempt = 0; $attempt -lt 100; ++$attempt) {
        $tabLine = Find-LastLogLine `
            -Path $Session.StdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC utility_tab id=settings .* active=[01]'
        if (-not [string]::IsNullOrWhiteSpace($tabLine)) {
            $rect = Get-ReportedRect -Line $tabLine -Prefix 'utility_tab id=settings'
            $insideViewport =
                $rect.X -ge $scrollState.Rect.X -and
                $rect.Y -ge $scrollState.Rect.Y -and
                ($rect.X + $rect.Width) -le ($scrollState.Rect.X + $scrollState.Rect.Width) -and
                ($rect.Y + $rect.Height) -le ($scrollState.Rect.Y + $scrollState.Rect.Height)
            if ($insideViewport) {
                if ($tabLine -match 'active=1$') {
                    break
                }
                Send-TypographyLogicalClick `
                    -Session $Session `
                    -LogicalX ($rect.X + $rect.Width / 2.0) `
                    -LogicalY ($rect.Y + $rect.Height / 2.0)
                [void](Wait-LogLine `
                    -CapturedProcess $Session.Captured `
                    -Path $Session.StdoutPath `
                    -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC utility_tab id=settings .* active=1')
                break
            }
        }

        $maximumOffset = [Math]::Max(
            0.0,
            $scrollState.ContentHeight - $scrollState.Rect.Height)
        if ($scrollState.Offset -ge ($maximumOffset - 0.5)) {
            throw "Camera/Status reached its product-reported scroll limit before Settings became visible. offset=$($scrollState.Offset) max=$maximumOffset tab=$tabLine"
        }

        $previousOffset = $scrollState.Offset
        Send-TypographyLogicalScroll `
            -Session $Session `
            -LogicalX ($scrollState.Rect.X + $scrollState.Rect.Width / 2.0) `
            -LogicalY ($scrollState.Rect.Y + $scrollState.Rect.Height / 2.0) `
            -WheelDelta -1.0

        $deadline = (Get-Date).AddSeconds(4)
        $advanced = $false
        do {
            if ($Session.Captured.Process.HasExited) {
                throw "Packaged Sandbox exited while scrolling Camera/Status to Settings. Captured stdout: $($Session.StdoutPath)"
            }
            $updatedLine = Find-LastLogLine `
                -Path $Session.StdoutPath `
                -Pattern $scrollPattern
            if (-not [string]::IsNullOrWhiteSpace($updatedLine)) {
                $updatedState = Get-ReportedScrollState -Line $updatedLine
                if ($updatedState.Offset -gt ($previousOffset + 0.5)) {
                    $scrollState = $updatedState
                    $advanced = $true
                    break
                }
            }
            Start-Sleep -Milliseconds 50
        } while ((Get-Date) -lt $deadline)
        if (-not $advanced) {
            throw "The real Camera/Status wheel interaction did not advance product-reported scroll state. offset=$previousOffset log=$($Session.StdoutPath)"
        }
    }
    $finalTabLine = Find-LastLogLine `
        -Path $Session.StdoutPath `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC utility_tab id=settings .* active=1'
    if ([string]::IsNullOrWhiteSpace($finalTabLine)) {
        throw "The Settings tab did not become active through the packaged UI. Log: $($Session.StdoutPath)"
    }

    # The persisted Settings page may reopen at the last saved scroll offset.
    # Navigate back to the scale control through the product-reported viewport;
    # do not assume it is initially visible after a restart or resize.
    $settingsLayoutLine = Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC utility_settings_layout frame=\d+'
    $settingsLayout = Get-ReportedUtilitySettingsLayout -Line $settingsLayoutLine
    if ($settingsLayout.Viewport.Width -le 0.0 -or
        $settingsLayout.Viewport.Height -le 0.0) {
        throw "The active Settings page reported no usable scroll viewport: $settingsLayoutLine"
    }
    for ($attempt = 0; $attempt -lt 64 -and $settingsLayout.ScrollOffset -gt 0.5; ++$attempt) {
        $before = $settingsLayout
        Send-TypographyLogicalScroll `
            -Session $Session `
            -LogicalX ($before.Viewport.X + $before.Viewport.Width / 2.0) `
            -LogicalY ($before.Viewport.Y + $before.Viewport.Height / 2.0) `
            -WheelDelta 1.0
        $scroll = Wait-TypographyUtilitySettingsScroll `
            -Session $Session `
            -AfterFrame $before.Frame
        if (-not $scroll.Handled -or
            [Math]::Abs($scroll.OffsetBefore - $before.ScrollOffset) -gt 0.6 -or
            $scroll.OffsetAfter -ge ($before.ScrollOffset - 0.5)) {
            throw "The real Settings wheel event did not return toward the scale control. Before=$($before.ScrollOffset), event=$($scroll.OffsetBefore)->$($scroll.OffsetAfter), handled=$($scroll.Handled)."
        }
        $settingsLayout = Wait-TypographyUtilitySettingsLayout `
            -Session $Session `
            -AfterFrame $before.Frame
        if ([Math]::Abs($settingsLayout.ScrollOffset - $scroll.OffsetAfter) -gt 0.6) {
            throw "Settings layout telemetry disagrees with the consumed scroll event. Layout=$($settingsLayout.ScrollOffset), event=$($scroll.OffsetBefore)->$($scroll.OffsetAfter)."
        }
    }
    if ($settingsLayout.ScrollOffset -gt 0.5) {
        throw "The persisted Settings page did not return to its top within the bounded real-wheel sequence. Offset=$($settingsLayout.ScrollOffset)."
    }
    [void](Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC editor_scale_control .* selected_percent=100|HENKA_AUTOMATION_DIAGNOSTIC editor_scale_control .* selected_percent=125|HENKA_AUTOMATION_DIAGNOSTIC editor_scale_control .* selected_percent=150|HENKA_AUTOMATION_DIAGNOSTIC editor_scale_control .* selected_percent=200')
}

function Test-TypographySettingsScrollControl {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][string]$Font
    )

    $layoutLine = Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC utility_settings_layout frame=\d+'
    $layout = Get-ReportedUtilitySettingsLayout -Line $layoutLine
    if ($layout.Viewport.Width -le 0.0 -or
        $layout.Viewport.Height -le 0.0 -or
        $layout.ContentHeight -le $layout.Viewport.Height) {
        throw "The 1280x720 Settings page did not expose a scrollable product-owned viewport at the selected scale: $layoutLine"
    }

    $initialScrollOffset = $layout.ScrollOffset
    $scrollEvents = 0
    for ($attempt = 0; $attempt -lt 32; ++$attempt) {
        if ($attempt -gt 0 -and $layout.SmoothVisible) {
            break
        }

        $before = $layout
        Send-TypographyLogicalScroll `
            -Session $Session `
            -LogicalX ($before.Viewport.X + $before.Viewport.Width / 2.0) `
            -LogicalY ($before.Viewport.Y + $before.Viewport.Height / 2.0) `
            -WheelDelta -1.0
        $scroll = Wait-TypographyUtilitySettingsScroll `
            -Session $Session `
            -AfterFrame $before.Frame
        if (-not $scroll.Handled -or
            [Math]::Abs($scroll.OffsetBefore - $before.ScrollOffset) -gt 0.6 -or
            $scroll.OffsetAfter -le ($before.ScrollOffset + 0.5)) {
            throw "The real Utility Settings wheel event did not advance its own scroll state. Before=$($before.ScrollOffset), event=$($scroll.OffsetBefore)->$($scroll.OffsetAfter), handled=$($scroll.Handled)."
        }
        $layout = Wait-TypographyUtilitySettingsLayout `
            -Session $Session `
            -AfterFrame $before.Frame
        if ([Math]::Abs($layout.ScrollOffset - $scroll.OffsetAfter) -gt 0.6) {
            throw "Utility Settings layout telemetry disagrees with the consumed scroll event. Layout=$($layout.ScrollOffset), event=$($scroll.OffsetAfter)."
        }
        ++$scrollEvents
    }

    if ($scrollEvents -eq 0 -or -not $layout.SmoothVisible) {
        throw "Smooth Snap did not become visible through bounded real Settings scrolling after $scrollEvents wheel events. Last state: $($Session.StdoutPath)"
    }
    if ($layout.SmoothRect.Width -le 0.0 -or
        $layout.SmoothRect.Height -le 0.0 -or
        $layout.SmoothRect.X -lt $layout.Viewport.X -or
        $layout.SmoothRect.Y -lt $layout.Viewport.Y -or
        ($layout.SmoothRect.X + $layout.SmoothRect.Width) -gt ($layout.Viewport.X + $layout.Viewport.Width + 0.1) -or
        ($layout.SmoothRect.Y + $layout.SmoothRect.Height) -gt ($layout.Viewport.Y + $layout.Viewport.Height + 0.1)) {
        throw "The product-reported Smooth Snap control is not fully inside its visible Settings viewport: $($Session.StdoutPath)"
    }

    $originalSmoothState = $layout.SmoothEnabled
    $beforeClickFrame = $layout.Frame
    Send-TypographyLogicalClick `
        -Session $Session `
        -LogicalX ($layout.SmoothRect.X + $layout.SmoothRect.Width / 2.0) `
        -LogicalY ($layout.SmoothRect.Y + $layout.SmoothRect.Height / 2.0)
    $changed = Wait-TypographyUtilitySettingsLayout `
        -Session $Session `
        -AfterFrame $beforeClickFrame
    if ($changed.SmoothEnabled -eq $originalSmoothState -or
        -not $changed.SmoothVisible -or
        [Math]::Abs($changed.SmoothRect.X - $layout.SmoothRect.X) -gt 0.1 -or
        [Math]::Abs($changed.SmoothRect.Y - $layout.SmoothRect.Y) -gt 0.1) {
        throw "Clicking the real Smooth Snap hit target did not change its authoritative setting while it remained visible. Log: $($Session.StdoutPath)"
    }

    $restoreFrame = $changed.Frame
    Send-TypographyLogicalClick `
        -Session $Session `
        -LogicalX ($changed.SmoothRect.X + $changed.SmoothRect.Width / 2.0) `
        -LogicalY ($changed.SmoothRect.Y + $changed.SmoothRect.Height / 2.0)
    $restored = Wait-TypographyUtilitySettingsLayout `
        -Session $Session `
        -AfterFrame $restoreFrame
    if ($restored.SmoothEnabled -ne $originalSmoothState -or
        -not $restored.SmoothVisible) {
        throw "The real Smooth Snap control did not restore its original authoritative state. Log: $($Session.StdoutPath)"
    }

    while ($restored.ScrollOffset -gt 0.5) {
        $before = $restored
        Send-TypographyLogicalScroll `
            -Session $Session `
            -LogicalX ($before.Viewport.X + $before.Viewport.Width / 2.0) `
            -LogicalY ($before.Viewport.Y + $before.Viewport.Height / 2.0) `
            -WheelDelta 1.0
        $scroll = Wait-TypographyUtilitySettingsScroll `
            -Session $Session `
            -AfterFrame $before.Frame
        if (-not $scroll.Handled -or
            $scroll.OffsetAfter -ge ($before.ScrollOffset - 0.5)) {
            throw "The real Utility Settings wheel event did not return toward the top. Before=$($before.ScrollOffset), event=$($scroll.OffsetBefore)->$($scroll.OffsetAfter), handled=$($scroll.Handled)."
        }
        $restored = Wait-TypographyUtilitySettingsLayout `
            -Session $Session `
            -AfterFrame $before.Frame
        if ([Math]::Abs($restored.ScrollOffset - $scroll.OffsetAfter) -gt 0.6) {
            throw "Utility Settings layout telemetry disagrees while returning to the top. Layout=$($restored.ScrollOffset), event=$($scroll.OffsetAfter)."
        }
    }
    if ($restored.ScrollOffset -gt 0.5 -or
        $restored.SmoothEnabled -ne $originalSmoothState) {
        throw "Utility Settings did not return to its initial scroll and Smooth Snap state. Log: $($Session.StdoutPath)"
    }
    return [pscustomobject]@{
        font = $Font
        client_resolution = '1280x720'
        editor_ui_scale_percent = 200
        initial_scroll_offset = $initialScrollOffset
        wheel_events_to_reveal_control = $scrollEvents
        product_reported_control_visible = $true
        real_click_changed_authoritative_setting = $true
        second_real_click_restored_authoritative_setting = $true
        final_scroll_offset = $restored.ScrollOffset
    }
}

function Test-TypographyObjectDetailsCompactLayout {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][string]$Font
    )

    $scalePattern = 'HENKA_AUTOMATION_DIAGNOSTIC editor_typography stage=scale_applied .* ui_scale_percent=200(?:\s|$)'
    $footerPattern = 'Native authoring project controls: name=Ground save_x=(?<saveX>-?[0-9.]+) save_y=(?<saveY>-?[0-9.]+) reload_x=(?<reloadX>-?[0-9.]+) reload_y=(?<reloadY>-?[0-9.]+) width=(?<width>-?[0-9.]+) height=24\.0\.'
    $deadline = (Get-Date).AddSeconds(8)
    $footerLine = $null

    do {
        if ($Session.Captured.Process.HasExited) {
            throw "Packaged Sandbox exited while checking compact Object Details controls. Log: $($Session.StdoutPath)"
        }
        $lines = (Get-LogText -Path $Session.StdoutPath) -split "`r?`n"
        $scaleIndex = -1
        for ($index = $lines.Length - 1; $index -ge 0; --$index) {
            if ($lines[$index] -match $scalePattern) {
                $scaleIndex = $index
                break
            }
        }
        if ($scaleIndex -ge 0) {
            for ($index = $lines.Length - 1; $index -gt $scaleIndex; --$index) {
                if ($lines[$index] -match $footerPattern) {
                    $footerLine = $lines[$index]
                    break
                }
            }
        }
        if ($null -eq $footerLine) {
            Start-Sleep -Milliseconds 100
        }
    } while ($null -eq $footerLine -and (Get-Date) -lt $deadline)

    if ($null -eq $footerLine) {
        throw "At 200% UI scale and 1280x720, the packaged Object Details panel did not publish reachable Save Project and Reload Project button geometry for $Font. Earlier 100% geometry is not accepted as proof. Log: $($Session.StdoutPath)"
    }

    $footerMatch = [regex]::Match($footerLine, $footerPattern)
    if (-not $footerMatch.Success) {
        throw "The compact Object Details footer geometry was malformed: $footerLine"
    }
    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    $saveX = [double]::Parse($footerMatch.Groups['saveX'].Value, $culture)
    $saveY = [double]::Parse($footerMatch.Groups['saveY'].Value, $culture)
    $reloadX = [double]::Parse($footerMatch.Groups['reloadX'].Value, $culture)
    $reloadY = [double]::Parse($footerMatch.Groups['reloadY'].Value, $culture)
    $buttonWidth = [double]::Parse($footerMatch.Groups['width'].Value, $culture)
    if ($buttonWidth -lt 96.0 -or $buttonWidth -gt 140.0 -or
        $reloadY -lt ($saveY + 20.0) -or
        [Math]::Abs($reloadX - $saveX) -gt 0.5) {
        throw "The compact Object Details footer must preserve full-size stacked targets at 200% and 1280x720: $footerLine"
    }

    Send-TypographyLogicalClick `
        -Session $Session `
        -LogicalX ($saveX + $buttonWidth / 2.0) `
        -LogicalY ($saveY + 12.0)
    [void](Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern 'Native authoring project request: save=1 reload=0 result=success\.' `
        -TimeoutMilliseconds 12000)

    return [pscustomobject]@{
        font = $Font
        client_resolution = '1280x720'
        editor_ui_scale_percent = 200
        save_reload_stacked = $true
        save_button_width = $buttonWidth
        save_geometry = @{ x = $saveX; y = $saveY }
        reload_geometry = @{ x = $reloadX; y = $reloadY }
        real_save_click_succeeded = $true
    }
}

function Select-TypographyGroundThroughUi {
    param([Parameter(Mandatory = $true)]$Session)

    $rowPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_authority entity=(?<entity>\d+) name=Ground display=.*? label_lines=(?<lines>\d+) hidden=(?<hidden>[01]) selected=(?<selected>[01]) .*? row_x=(?<x>-?(?:\d+(?:\.\d*)?|\.\d+)) row_y=(?<y>-?(?:\d+(?:\.\d*)?|\.\d+)) row_width=(?<width>-?(?:\d+(?:\.\d*)?|\.\d+)) row_height=(?<height>-?(?:\d+(?:\.\d*)?|\.\d+)) visible_line_capacity=(?<capacity>\d+) .*? hitbox_uses_same_row_rect=1 clicked=[01]\.'
    $rowLine = Find-LastLogLine -Path $Session.StdoutPath -Pattern $rowPattern
    if ([string]::IsNullOrWhiteSpace($rowLine)) {
        throw "The product did not report the visible Ground row geometry needed for real selection. Log: $($Session.StdoutPath)"
    }
    $rowMatch = [regex]::Match($rowLine, $rowPattern)
    if (-not $rowMatch.Success -or $rowMatch.Groups['hidden'].Value -ne '0' -or
        [int]::Parse($rowMatch.Groups['capacity'].Value) -lt 1) {
        throw "Ground is not a visible, selectable product row: $rowLine"
    }
    $numberCulture = [System.Globalization.CultureInfo]::InvariantCulture
    $rowX = [double]::Parse($rowMatch.Groups['x'].Value, $numberCulture)
    $rowY = [double]::Parse($rowMatch.Groups['y'].Value, $numberCulture)
    $rowWidth = [double]::Parse($rowMatch.Groups['width'].Value, $numberCulture)
    $rowHeight = [double]::Parse($rowMatch.Groups['height'].Value, $numberCulture)
    if ($rowWidth -le 0.0 -or $rowHeight -le 0.0) {
        throw "The product-reported Ground row has no clickable area: $rowLine"
    }
    $entityId = [UInt64]::Parse(
        $rowMatch.Groups['entity'].Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
    Send-TypographyLogicalClick `
        -Session $Session `
        -LogicalX ($rowX + $rowWidth / 2.0) `
        -LogicalY ($rowY + $rowHeight / 2.0)
    $clickPattern = "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_row_click entity=$entityId name=Ground hidden=0 selected_entity=$entityId selected=1\."
    [void](Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern $clickPattern `
        -TimeoutMilliseconds 12000)
    return [pscustomobject]@{
        EntityId = $entityId
        Name = 'Ground'
        Hidden = $false
        Method = 'scene_objects_row_click'
    }
}

function Get-ReportedSceneObjectsActionState {
    param([Parameter(Mandatory = $true)]$Session)

    $number = '-?\d+(?:\.\d+)?'
    $pattern = "HENKA_AUTOMATION_DIAGNOSTIC scene_objects_layout result=ready .* actions_viewport=(?<x>$number),(?<y>$number),(?<width>$number),(?<height>$number) actions_content_height=(?<content>$number) actions_scroll_offset=(?<offset>$number) .*"
    $line = Find-LastLogLine -Path $Session.StdoutPath -Pattern $pattern
    if ([string]::IsNullOrWhiteSpace($line)) {
        throw "The product did not report its canonical Scene Objects action viewport and scroll state. Log: $($Session.StdoutPath)"
    }
    $match = [regex]::Match($line, $pattern)
    if (-not $match.Success) {
        throw "The Scene Objects action viewport report was malformed: $line"
    }
    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    $state = [pscustomobject]@{
        Line = $line
        Viewport = [pscustomobject]@{
            X = [double]::Parse($match.Groups['x'].Value, $culture)
            Y = [double]::Parse($match.Groups['y'].Value, $culture)
            Width = [double]::Parse($match.Groups['width'].Value, $culture)
            Height = [double]::Parse($match.Groups['height'].Value, $culture)
        }
        ContentHeight = [double]::Parse($match.Groups['content'].Value, $culture)
        Offset = [double]::Parse($match.Groups['offset'].Value, $culture)
    }
    if ($state.Viewport.Width -le 0.0 -or $state.Viewport.Height -le 0.0 -or
        $state.ContentHeight -lt 0.0 -or $state.Offset -lt 0.0 -or
        $state.Offset -gt [Math]::Max(0.0, $state.ContentHeight - $state.Viewport.Height) + 0.01) {
        throw "The reported Scene Objects action scroll state is outside its product viewport: $line"
    }
    return $state
}

function Test-TypographySceneObjectsActionScroll {
    param([Parameter(Mandatory = $true)]$Session)

    $state = Get-ReportedSceneObjectsActionState -Session $Session
    if ($state.ContentHeight -le ($state.Viewport.Height + 1.0)) {
        throw "The 200% 1280x720 product action list does not exceed its viewport; the scroll regression would not exercise overflow: $($state.Line)"
    }
    $x = $state.Viewport.X + ($state.Viewport.Width / 2.0)
    $y = $state.Viewport.Y + ($state.Viewport.Height / 2.0)
    Send-TypographyLogicalScroll `
        -Session $Session `
        -LogicalX $x `
        -LogicalY $y `
        -WheelDelta -1.0
    $scrollLine = Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC scene_objects_actions_scroll .*' `
        -TimeoutMilliseconds 12000
    $scrollMatch = [regex]::Match(
        $scrollLine,
        'offset=(?<before>-?\d+(?:\.\d+)?)->(?<after>-?\d+(?:\.\d+)?) handled=(?<handled>[01])\.')
    if (-not $scrollMatch.Success -or $scrollMatch.Groups['handled'].Value -ne '1') {
        throw "The actual Scene Objects wheel input was not applied to the action scroll owner: $scrollLine"
    }
    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    $after = [double]::Parse($scrollMatch.Groups['after'].Value, $culture)
    if ($after -le $state.Offset + 0.01) {
        throw "The action scroll offset did not advance after the real packaged wheel input: $scrollLine"
    }

    $expectedOffsetText = [regex]::Escape($after.ToString('F1', $culture))
    $returnPattern = 'HENKA_AUTOMATION_DIAGNOSTIC scene_objects_actions_scroll .* offset=' +
        $expectedOffsetText + '->0\.0 handled=1\.'
    Send-TypographyLogicalScroll `
        -Session $Session `
        -LogicalX $x `
        -LogicalY $y `
        -WheelDelta 1.0
    $returnLine = Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern $returnPattern `
        -TimeoutMilliseconds 12000
    $returnMatch = [regex]::Match(
        $returnLine,
        'offset=(?<before>-?\d+(?:\.\d+)?)->(?<after>-?\d+(?:\.\d+)?) handled=(?<handled>[01])\.')
    if (-not $returnMatch.Success -or $returnMatch.Groups['handled'].Value -ne '1' -or
        [Math]::Abs([double]::Parse($returnMatch.Groups['after'].Value, $culture)) -gt 0.01) {
        throw "The action viewport did not return to its top after the reverse real wheel input: $returnLine"
    }
}

function Hide-TypographyToolsThroughUi {
    param([Parameter(Mandatory = $true)]$Session)

    $topologyPattern = 'HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=\S+ tools_visible=[01] divider_count=\d+ .*'
    $topologyLine = Find-LastLogLine -Path $Session.StdoutPath -Pattern $topologyPattern
    if ([string]::IsNullOrWhiteSpace($topologyLine)) {
        throw "The product did not report the workspace topology before the comparison layout was normalized."
    }
    $topology = Get-ReportedWorkspaceTopology -Line $topologyLine
    if ($topology.ToolsVisible) {
        $toolsLine = Wait-TypographyStableToolsControl -Session $Session
        $toolsRect = Get-ReportedRect -Line $toolsLine -Prefix 'Scene View Tools control:'
        Send-TypographyLogicalClick `
            -Session $Session `
            -LogicalX ($toolsRect.X + $toolsRect.Width / 2.0) `
            -LogicalY ($toolsRect.Y + $toolsRect.Height / 2.0)
        $topologyLine = Wait-LogLine `
            -CapturedProcess $Session.Captured `
            -Path $Session.StdoutPath `
            -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=tools_toggle tools_visible=0 divider_count=\d+ .*'
        $topology = Get-ReportedWorkspaceTopology -Line $topologyLine
    }
    if ($topology.ToolsVisible) {
        throw "The real Tools interaction did not collapse the Tools dock: $topologyLine"
    }
}

function Set-TypographyScaleThroughUi {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][int]$TargetScale,
        [switch]$RefreshCapture
    )
    $pattern = "HENKA_AUTOMATION_DIAGNOSTIC editor_typography stage=scale_applied .* ui_scale_percent=$TargetScale(?:\s|$)"
    $currentLine = Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern "HENKA_AUTOMATION_DIAGNOSTIC editor_scale_control .* selected_percent=\d+"
    $selectedScaleMatch = [regex]::Match($currentLine, 'selected_percent=(?<scale>\d+)')
    if (-not $selectedScaleMatch.Success) {
        throw 'The product did not report the active Editor UI Scale.'
    }
    $currentScalePercent = [int]::Parse(
        $selectedScaleMatch.Groups['scale'].Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
    if ($currentScalePercent -eq $TargetScale -and -not $RefreshCapture) {
        return
    }
    $selectionPlan = @(
        Get-TypographyCaptureScalePlan `
            -CurrentScalePercent $currentScalePercent `
            -TargetScalePercent $TargetScale)
    if ($currentScalePercent -eq $TargetScale) {
        if ($selectionPlan.Count -ne 2) {
            throw 'A requested fresh capture at the current scale did not produce a bounded refresh transition.'
        }
        [void](Set-TypographyScaleThroughUi `
            -Session $Session `
            -TargetScale $selectionPlan[0])
        $currentLine = Wait-LogLine `
            -CapturedProcess $Session.Captured `
            -Path $Session.StdoutPath `
            -Pattern "HENKA_AUTOMATION_DIAGNOSTIC editor_scale_control .* selected_percent=$($selectionPlan[0])(?:\s|$)"
    }
    elseif ($selectionPlan.Count -ne 1 -or $selectionPlan[0] -ne $TargetScale) {
        throw 'The requested Editor UI Scale did not resolve to one direct product transition.'
    }
    $rect = Get-ReportedRect -Line $currentLine -Prefix 'HENKA_AUTOMATION_DIAGNOSTIC editor_scale_control'
    $choiceMatch = [regex]::Match(
        $currentLine,
        'choices_x=(?<x>-?\d+(?:\.\d+)?) choices_y=(?<y>-?\d+(?:\.\d+)?) choices_width=(?<width>\d+(?:\.\d+)?) choices_height=(?<height>\d+(?:\.\d+)?)')
    if (-not $choiceMatch.Success) {
        throw 'The product did not report the canonical segmented scale-choice hit rectangle.'
    }
    $choiceCulture = [System.Globalization.CultureInfo]::InvariantCulture
    $choicesX = [double]::Parse($choiceMatch.Groups['x'].Value, $choiceCulture)
    $choicesY = [double]::Parse($choiceMatch.Groups['y'].Value, $choiceCulture)
    $choicesWidth = [double]::Parse($choiceMatch.Groups['width'].Value, $choiceCulture)
    $choicesHeight = [double]::Parse($choiceMatch.Groups['height'].Value, $choiceCulture)
    if ($choicesWidth -le 0.0 -or $choicesHeight -lt 24.0 -or
        $choicesX -lt $rect.X -or $choicesY -lt $rect.Y -or
        ($choicesX + $choicesWidth) -gt ($rect.X + $rect.Width + 0.01) -or
        ($choicesY + $choicesHeight) -gt ($rect.Y + $rect.Height + 0.01)) {
        throw "The product-reported Settings scale control is too small to interact with."
    }
    $targets = @(100, 125, 150, 200)
    $targetIndex = [Array]::IndexOf($targets, $TargetScale)
    if ($targetIndex -lt 0) {
        throw "Unsupported Editor UI Scale request: $TargetScale"
    }
    $segmentWidth = $choicesWidth / $targets.Length
    $clickX = $choicesX + (($targetIndex + 0.5) * $segmentWidth)
    $clickY = $choicesY + ($choicesHeight / 2.0)
    $selectedScaleMatch = [regex]::Match($currentLine, 'selected_percent=(?<scale>\d+)')
    if (-not $selectedScaleMatch.Success) {
        throw 'The product did not report the active UI and display scales needed to target its logical control rectangle.'
    }
    $currentScalePercent = [int]::Parse(
        $selectedScaleMatch.Groups['scale'].Value,
        [System.Globalization.CultureInfo]::InvariantCulture)
    $logicalPoint = Convert-TypographySessionLogicalPoint `
        -Session $Session -LogicalX $clickX -LogicalY $clickY
    if ($currentScalePercent -ne $logicalPoint.EditorScalePercent) {
        throw "The scale control selection ($currentScalePercent%) disagrees with active runtime scale ($($logicalPoint.EditorScalePercent)%)."
    }
    $frameCapturePath = Join-Path $Session.CaptureDirectory ("{0}-ui-scale-{1}.bmp" -f $Session.CaptureLabel, $TargetScale)
    $ownedScratchPrefix = [System.IO.Path]::GetFullPath($scratchRoot).TrimEnd('\') + '\'
    if (-not ([System.IO.Path]::GetFullPath($frameCapturePath)).StartsWith(
            $ownedScratchPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to retire a prior frame capture outside this run's owned scratch root."
    }
    if (Test-Path -LiteralPath $frameCapturePath -PathType Leaf) {
        Remove-Item -LiteralPath $frameCapturePath -Force
    }
    Send-TypographyLogicalClick `
        -Session $Session `
        -LogicalX $clickX `
        -LogicalY $clickY
    [void](Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern "HENKA_AUTOMATION_DIAGNOSTIC editor_typography stage=scale_requested current_scale_percent=\d+ requested_scale_percent=$TargetScale(?:\s|$)" `
        -TimeoutMilliseconds 12000)
    [void](Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern $pattern `
        -TimeoutMilliseconds 12000)
    [void](Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern "HENKA_AUTOMATION_DIAGNOSTIC editor_scale_control .* selected_percent=$TargetScale(?:\s|$)" `
        -TimeoutMilliseconds 12000)
}

function Wait-TypographyFrameCapture {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][int]$Scale
    )
    $path = Join-Path $Session.CaptureDirectory ("{0}-ui-scale-{1}.bmp" -f $Session.CaptureLabel, $Scale)
    $deadline = (Get-Date).AddSeconds(15)
    $previousLength = -1L
    $stableReads = 0
    do {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $item = Get-Item -LiteralPath $path -ErrorAction Stop
            if ($item.Length -gt 54 -and $item.Length -eq $previousLength) {
                $stableReads++
                if ($stableReads -ge 2) {
                    return $item
                }
            }
            else {
                $stableReads = 0
                $previousLength = $item.Length
            }
        }
        if ($Session.Captured.Process.HasExited) {
            throw "The packaged Sandbox exited before its product-owned framebuffer capture was written: $path"
        }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)
    throw "The product-owned framebuffer capture for $Scale percent did not become complete: $path"
}

function Save-TypographyCapture {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][string]$Font,
        [Parameter(Mandatory = $true)][int]$Scale,
        [Parameter(Mandatory = $true)][int]$Width,
        [Parameter(Mandatory = $true)][int]$Height
    )
    $fontLine = Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern "HENKA_AUTOMATION_DIAGNOSTIC editor_typography stage=(loaded|scale_applied) font=$Font .* ui_scale_percent=$Scale(?:\s|$)"
    $runtimeScales = Get-TypographyRuntimeScales -Session $Session
    if ($runtimeScales.EditorScalePercent -ne $Scale -or
        $runtimeScales.DisplayScaleNumerator -eq 0 -or
        $runtimeScales.DisplayScaleDenominator -eq 0) {
        throw "The product-reported editor or OS display scale is invalid for the $Scale percent capture."
    }
    $scaleLine = Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern "HENKA_AUTOMATION_DIAGNOSTIC editor_scale_control .* selected_percent=$Scale(?:\s|$)"
    if ($fontLine -notmatch "font_sha256=$($fontHashes[$Font])(?:\s|$)") {
        throw "The packaged runtime did not prove the expected loaded font bytes for $Font."
    }
    $topologyLine = Find-LastLogLine `
        -Path $Session.StdoutPath `
        -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC workspace_topology cause=\S+ tools_visible=[01] divider_count=\d+ .*'
    if ([string]::IsNullOrWhiteSpace($topologyLine)) {
        throw "The packaged product did not report workspace visibility for the $Scale percent capture."
    }
    $topology = Get-ReportedWorkspaceTopology -Line $topologyLine
    if ($topology.ToolsVisible) {
        throw "The Tools dock was not in the normalized collapsed state for the $Scale percent capture: $topologyLine"
    }
    [void](Wait-TypographySettingsNavigationTab -Session $Session -Scale $Scale)
    $selectionProof = $Session.SelectedEntityProof
    if ($null -eq $selectionProof -or
        $selectionProof.Name -ne 'Ground' -or
        $selectionProof.Hidden -ne $false -or
        $selectionProof.Method -ne 'scene_objects_row_click') {
        throw "The typography sample did not establish the canonical visible Ground selection through the real Scene Objects row."
    }
    $geometry = Get-HenkaAutomationWindowGeometry -Handle $Session.Window
    if ($geometry.ClientWidth -ne $Width -or $geometry.ClientHeight -ne $Height) {
        throw "The capture client area is $($geometry.ClientWidth)x$($geometry.ClientHeight), expected ${Width}x${Height}."
    }
    $path = Join-Path $OutputDirectory ("{0}-{1}-{2}x{3}.png" -f $Font, $Scale, $Width, $Height)
    $frameCapture = Wait-TypographyFrameCapture -Session $Session -Scale $Scale
    $frameImage = [System.Drawing.Image]::FromFile($frameCapture.FullName)
    try {
        if ($frameImage.Width -ne $Width -or $frameImage.Height -ne $Height) {
            throw "The product framebuffer capture is $($frameImage.Width)x$($frameImage.Height), expected ${Width}x${Height}."
        }
        $frameImage.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        $frameWidth = $frameImage.Width
        $frameHeight = $frameImage.Height
    }
    finally {
        $frameImage.Dispose()
    }
    $windowCapturePath = Join-Path $scratchRoot (Join-Path "window-captures" ("{0}-{1}-{2}x{3}.png" -f $Font, $Scale, $Width, $Height))
    $windowCapture = Save-HenkaAutomationWindowScreenshot `
        -Handle $Session.Window `
        -Path $windowCapturePath `
        -Description "$Font window surface at $Scale percent, ${Width}x${Height}" `
        -AllowHidden
    $fontRect = Get-ReportedRect -Line $scaleLine -Prefix 'HENKA_AUTOMATION_DIAGNOSTIC editor_scale_control'
    $captures.Add([pscustomobject]@{
        font = $Font
        font_identity = ($fontLine -replace '^.*font_identity=([^ ]+(?: [^ ]+)*) font_sha256=.*$', '$1')
        font_sha256 = $fontHashes[$Font]
        ui_scale_percent = $Scale
        os_display_scale_numerator = $runtimeScales.DisplayScaleNumerator
        os_display_scale_denominator = $runtimeScales.DisplayScaleDenominator
        client_width = $Width
        client_height = $Height
        framebuffer_capture_width = $frameWidth
        framebuffer_capture_height = $frameHeight
        window_capture_width = $windowCapture.Width
        window_capture_height = $windowCapture.Height
        window_capture_sha256 = (Get-FileHash -LiteralPath $windowCapturePath -Algorithm SHA256).Hash.ToUpperInvariant()
        window_presentation = $Session.WindowPresentation
        settings_scale_control = $fontRect
        screenshot = (Split-Path -Leaf $path)
        screenshot_sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToUpperInvariant()
        workspace_tools_visible = $topology.ToolsVisible
        settings_tab_active = $true
        selected_entity_name = $selectionProof.Name
        selected_entity_id = $selectionProof.EntityId
        selected_entity_hidden = $selectionProof.Hidden
        selection_method = $selectionProof.Method
    })
}

function Stop-TypographyCandidateProcess {
    param($Session)
    if ($null -ne $Session -and $null -ne $Session.Captured) {
        Close-HenkaCapturedProcess -CapturedProcess $Session.Captured
    }
}

function Wait-TypographySettingsNavigationTab {
    param(
        [Parameter(Mandatory = $true)]$Session,
        [Parameter(Mandatory = $true)][ValidateSet(100, 125, 150, 200)][int]$Scale
    )

    $pattern = "HENKA_AUTOMATION_DIAGNOSTIC utility_navigation_tab id=settings x=-?[0-9.]+ y=-?[0-9.]+ width=[0-9.]+ height=[0-9.]+ visible=1 active=1 ui_scale_percent=$Scale frame=\d+"
    return Wait-LogLine `
        -CapturedProcess $Session.Captured `
        -Path $Session.StdoutPath `
        -Pattern $pattern `
        -TimeoutMilliseconds 10000
}

function Test-TypographyDetachedUtilityContext {
    param(
        [Parameter(Mandatory = $true)][string]$Font,
        [Parameter(Mandatory = $true)][string]$ExpectedDisplayScale,
        [Parameter(Mandatory = $true)][string]$UserDataRoot
    )

    $label = "$Font-detached-utility"
    [System.IO.Directory]::CreateDirectory($UserDataRoot) | Out-Null
    $session = Start-TypographyCandidateProcess `
        -Font $Font `
        -Label $label `
        -CaptureLabel $label `
        -UserDataRoot $UserDataRoot
    try {
        [void](Wait-LogLine `
            -CapturedProcess $session.Captured `
            -Path $session.StdoutPath `
            -Pattern "HENKA_AUTOMATION_DIAGNOSTIC editor_typography stage=loaded font=$Font .* font_sha256=$($fontHashes[$Font]) ui_scale_percent=100" `
            -TimeoutMilliseconds 60000)
        [void](Set-HenkaAutomationWindowClientSize `
            -Handle $session.Window `
            -Width 1280 `
            -Height 720)
        Ensure-TypographySettingsVisible -Session $session
        Set-TypographyScaleThroughUi -Session $session -TargetScale 200

        $headerLine = Wait-TypographyStableUtilityContextHeader `
            -Session $session `
            -Scale 200 `
            -TimeoutMilliseconds 20000
        $headerRect = Get-ReportedRect -Line $headerLine -Prefix 'workspace_context_header id=utility'
        if ($headerRect.Width -le 0.0 -or $headerRect.Height -le 0.0) {
            throw "The product-reported Utility title-strip rectangle was not a valid context-menu hit target: $headerLine"
        }
        $headerTopLeft = Convert-TypographySessionLogicalPoint `
            -Session $session `
            -LogicalX $headerRect.X `
            -LogicalY $headerRect.Y
        $headerBottomRight = Convert-TypographySessionLogicalPoint `
            -Session $session `
            -LogicalX ($headerRect.X + $headerRect.Width) `
            -LogicalY ($headerRect.Y + $headerRect.Height)
        $headerWindowGeometry = Get-HenkaAutomationWindowGeometry -Handle $session.Window
        $headerInsideViewport =
            $headerTopLeft.X -ge 0.0 -and
            $headerTopLeft.Y -ge 0.0 -and
            $headerBottomRight.X -le $headerWindowGeometry.ClientWidth -and
            $headerBottomRight.Y -le $headerWindowGeometry.ClientHeight
        if (-not $headerInsideViewport) {
            throw ("The current-scale Utility title-strip rectangle is outside the packaged window " +
                "({0},{1})-({2},{3}) within {4}x{5}: {6}" -f
                $headerTopLeft.X,
                $headerTopLeft.Y,
                $headerBottomRight.X,
                $headerBottomRight.Y,
                $headerWindowGeometry.ClientWidth,
                $headerWindowGeometry.ClientHeight,
                $headerLine)
        }
        Send-TypographyLogicalClick `
            -Session $session `
            -LogicalX ($headerRect.X + $headerRect.Width / 2.0) `
            -LogicalY ($headerRect.Y + $headerRect.Height / 2.0) `
            -Button right
        [void](Wait-LogLine `
            -CapturedProcess $session.Captured `
            -Path $session.StdoutPath `
            -Pattern 'Workspace context menu: section=Utility horizontal=available vertical=available\.')
        for ($index = 0; $index -lt 6; ++$index) {
            Send-HenkaAutomationKey -EventPath $session.EventPath -KeyName down
        }
        Send-HenkaAutomationKey -EventPath $session.EventPath -KeyName enter

        $displayScaleMatch = [regex]::Match(
            $ExpectedDisplayScale,
            '^(?<numerator>\d+)/(?<denominator>\d+)$')
        if (-not $displayScaleMatch.Success) {
            throw "The expected detached-context display scale was invalid: $ExpectedDisplayScale"
        }
        $contextPattern = "HENKA_AUTOMATION_DIAGNOSTIC editor_typography_context stage=attached window_id=\d+ font=$Font font_sha256=$($fontHashes[$Font]) ui_scale_percent=200 os_display_scale=$ExpectedDisplayScale(?:\s|$)"
        $contextLine = Wait-LogLine `
            -CapturedProcess $session.Captured `
            -Path $session.StdoutPath `
            -Pattern $contextPattern `
            -TimeoutMilliseconds 20000

        $detachedWindow = [IntPtr]::Zero
        $windowDeadline = (Get-Date).AddSeconds(15)
        do {
            $detachedWindow = Find-HenkaAutomationWindow `
                -ProcessId $session.Captured.Process.Id `
                -TitleContains "Henka - Utility" `
                -IncludeHidden
            if ($detachedWindow -ne [IntPtr]::Zero) { break }
            if ($session.Captured.Process.HasExited) {
                throw "Packaged Sandbox exited before its detached Utility window appeared."
            }
            Start-Sleep -Milliseconds 100
        } while ((Get-Date) -lt $windowDeadline)
        if ($detachedWindow -eq [IntPtr]::Zero) {
            throw "The normal Utility detach action did not create its native tool window."
        }

        $foregroundBeforeDetach = [HenkaUiAutomationNative]::GetForegroundWindow()
        if ([HenkaUiAutomationNative]::IsWindowVisible($detachedWindow) -or
            $foregroundBeforeDetach -eq $detachedWindow -or
            $foregroundBeforeDetach -eq $session.Window) {
            throw "The detached Utility left its hidden, non-foreground automation state before render verification."
        }
        $windowGeometry = Get-HenkaAutomationWindowGeometry -Handle $detachedWindow
        if ($windowGeometry.ClientWidth -lt 320 -or $windowGeometry.ClientHeight -lt 240) {
            throw "The detached Utility window did not provide a usable rendering surface."
        }
        $renderSequenceAtContextReady = Get-TypographyLastRenderCompleteSequence -Session $session
        $detachedRender = Wait-TypographyRenderCompleteAfterSequence `
            -Session $session `
            -PreviousSequence $renderSequenceAtContextReady
        $foregroundAfterDetach = [HenkaUiAutomationNative]::GetForegroundWindow()
        if ([HenkaUiAutomationNative]::IsWindowVisible($detachedWindow) -or
            $foregroundAfterDetach -eq $detachedWindow -or
            $foregroundAfterDetach -eq $session.Window) {
            throw "Detaching and rendering Utility changed the hidden-window or foreground state."
        }

        return [pscustomobject]@{
            window_title = "Henka - Utility"
            font = $Font
            font_sha256 = $fontHashes[$Font]
            ui_scale_percent = 200
            os_display_scale = $ExpectedDisplayScale
            context_attachment_diagnostic = $contextLine.Trim()
            tool_window_hidden_confirmed = $true
            render_complete_after_context_attach = $true
            render_complete_sequence = $detachedRender.Sequence
            render_complete_line = $detachedRender.Line.Trim()
            foreground_before_detach = $foregroundBeforeDetach.ToInt64()
            foreground_after_detach = $foregroundAfterDetach.ToInt64()
            client_width = $windowGeometry.ClientWidth
            client_height = $windowGeometry.ClientHeight
            screenshot_status = 'not_captured'
            screenshot_reason = 'PrintWindow exposed the native window frame but not its OpenGL client framebuffer; detached visual output is not claimed.'
        }
    }
    finally {
        Stop-TypographyCandidateProcess -Session $session
    }
}

function Remove-TypographyScratch {
    param([Parameter(Mandatory = $true)][string]$Path)

    $root = Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $Path
    $temporaryRoot = Join-Path (Get-HenkaLocalRoot -RepositoryRoot $repoRoot) "temporary"
    $temporaryRoot = [System.IO.Path]::GetFullPath($temporaryRoot).TrimEnd('\')
    if (-not $root.StartsWith(
            $temporaryRoot + '\',
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to retire typography scratch outside the canonical temporary root: $root"
    }
    $markerPath = Join-Path $root ".henka-generated.json"
    if (-not (Test-Path -LiteralPath $markerPath -PathType Leaf)) {
        throw "Refusing to retire typography scratch without its generated-root marker: $root"
    }
    $marker = Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json
    if ($marker.owner -ne "Henka repository validation" -or
        $marker.purpose -ne "editor-typography-comparison" -or
        $marker.active -ne $true -or
        $marker.cleanup_eligible -ne $true) {
        throw "Refusing to retire typography scratch with unexpected lifecycle ownership: $root"
    }
    [void](Write-HenkaGeneratedRootMarker `
        -RepoRoot $repoRoot `
        -Path $root `
        -Purpose "editor-typography-comparison" `
        -RetentionClass "PROOF_CONSUMED" `
        -Active $false `
        -CleanupEligible $true `
        -CleanupCondition "comparison manifest and screenshots were written; all owned Sandbox processes were closed")
    foreach ($item in @(Get-Item -LiteralPath $root -Force) + @(Get-ChildItem -LiteralPath $root -Force -Recurse -ErrorAction Stop)) {
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing to remove typography scratch containing a reparse point: $($item.FullName)"
        }
    }
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction Stop
    if (Test-Path -LiteralPath $root) {
        throw "Typography scratch retirement did not remove its exact generated path: $root"
    }
}

try {
    foreach ($font in @("atkinson", "noto")) {
        Write-Output "==> Packaged editor typography: $font"
        $fontUserDataRoot = Join-Path $userDataRoot $font
        [System.IO.Directory]::CreateDirectory($fontUserDataRoot) | Out-Null
        $session = Start-TypographyCandidateProcess `
            -Font $font `
            -Label "$font-1280" `
            -CaptureLabel "$font-1280" `
            -UserDataRoot $fontUserDataRoot
        try {
            $startupPattern = "HENKA_AUTOMATION_DIAGNOSTIC editor_typography stage=loaded font=$font .* font_sha256=$($fontHashes[$font]) ui_scale_percent=100"
            [void](Wait-LogLine -CapturedProcess $session.Captured -Path $session.StdoutPath -Pattern $startupPattern -TimeoutMilliseconds 60000)
            [void](Wait-LogLine -CapturedProcess $session.Captured -Path $session.StdoutPath -Pattern 'HENKA_AUTOMATION_DIAGNOSTIC user_data_base_path=isolated')
            $size = Get-HenkaAutomationWindowGeometry -Handle $session.Window
            if ($size.ClientWidth -ne 1280 -or $size.ClientHeight -ne 720) {
                $size = Set-HenkaAutomationWindowClientSize -Handle $session.Window -Width 1280 -Height 720
            }
            Ensure-TypographySettingsVisible -Session $session
            $selectionProof = Select-TypographyGroundThroughUi -Session $session
            Add-Member -InputObject $session -NotePropertyName SelectedEntityProof -NotePropertyValue $selectionProof -Force
            Hide-TypographyToolsThroughUi -Session $session
            foreach ($scale in @(100, 125, 150, 200)) {
                Set-TypographyScaleThroughUi `
                    -Session $session `
                    -TargetScale $scale `
                    -RefreshCapture
                if ($scale -eq 200) {
                    $detailsCompactProofs.Add((Test-TypographyObjectDetailsCompactLayout `
                        -Session $session `
                        -Font $font))
                    $settingsScrollProofs.Add((Test-TypographySettingsScrollControl `
                        -Session $session `
                        -Font $font))
                    Test-TypographySceneObjectsActionScroll -Session $session
                }
                $selectionProof = Select-TypographyGroundThroughUi -Session $session
                Add-Member -InputObject $session -NotePropertyName SelectedEntityProof -NotePropertyValue $selectionProof -Force
                Save-TypographyCapture -Session $session -Font $font -Scale $scale -Width 1280 -Height 720
            }
            $persistedScale = 200
        }
        finally {
            Stop-TypographyCandidateProcess -Session $session
        }

        $session = Start-TypographyCandidateProcess `
            -Font $font `
            -Label "$font-persisted" `
            -CaptureLabel "$font-1920" `
            -UserDataRoot $fontUserDataRoot
        try {
            [void](Wait-LogLine `
                -CapturedProcess $session.Captured `
                -Path $session.StdoutPath `
                -Pattern "HENKA_AUTOMATION_DIAGNOSTIC editor_typography stage=loaded font=$font .* font_sha256=$($fontHashes[$font]) ui_scale_percent=$persistedScale(?:\s|$)" `
                -TimeoutMilliseconds 60000)
            [void](Set-HenkaAutomationWindowClientSize `
                -Handle $session.Window `
                -Width 1920 `
                -Height 1080 `
                -BorderlessViewport)
            $session.WindowPresentation = "hidden-borderless-desktop"
            Ensure-TypographySettingsVisible -Session $session
            Set-TypographyScaleThroughUi -Session $session -TargetScale 100
            $selectionProof = Select-TypographyGroundThroughUi -Session $session
            Add-Member -InputObject $session -NotePropertyName SelectedEntityProof -NotePropertyValue $selectionProof -Force
            Hide-TypographyToolsThroughUi -Session $session
            foreach ($scale in @(100, 125, 150, 200)) {
                Set-TypographyScaleThroughUi `
                    -Session $session `
                    -TargetScale $scale `
                    -RefreshCapture
                $selectionProof = Select-TypographyGroundThroughUi -Session $session
                Add-Member -InputObject $session -NotePropertyName SelectedEntityProof -NotePropertyValue $selectionProof -Force
                Save-TypographyCapture -Session $session -Font $font -Scale $scale -Width 1920 -Height 1080
            }
        }
        finally {
            Stop-TypographyCandidateProcess -Session $session
        }
    }

    $atkinsonBaselineCapture = @($captures | Where-Object {
        $_.font -eq 'atkinson' -and $_.ui_scale_percent -eq 100
    } | Select-Object -First 1)
    if ($atkinsonBaselineCapture.Count -ne 1 -or
        $atkinsonBaselineCapture[0].os_display_scale_numerator -le 0 -or
        $atkinsonBaselineCapture[0].os_display_scale_denominator -le 0) {
        throw 'The completed Atkinson baseline capture did not provide a valid product-reported OS display scale.'
    }
    $detachedUserDataRoot = Join-Path $userDataRoot 'atkinson-detached'
    $detachedContextProof = Test-TypographyDetachedUtilityContext `
        -Font atkinson `
        -ExpectedDisplayScale ("{0}/{1}" -f $atkinsonBaselineCapture[0].os_display_scale_numerator, $atkinsonBaselineCapture[0].os_display_scale_denominator) `
        -UserDataRoot $detachedUserDataRoot

    foreach ($font in @("atkinson", "noto")) {
        foreach ($resolution in @(@{Width=1280;Height=720}, @{Width=1920;Height=1080})) {
            $small = @($captures | Where-Object {
                $_.font -eq $font -and $_.ui_scale_percent -eq 100 -and
                $_.client_width -eq $resolution.Width -and $_.client_height -eq $resolution.Height
            }) | Select-Object -First 1
            $large = @($captures | Where-Object {
                $_.font -eq $font -and $_.ui_scale_percent -eq 200 -and
                $_.client_width -eq $resolution.Width -and $_.client_height -eq $resolution.Height
            }) | Select-Object -First 1
            if ($null -eq $small -or $null -eq $large -or
                $small.screenshot_sha256 -eq $large.screenshot_sha256) {
                throw "Product framebuffer captures did not reflect the 100-to-200 percent editor scale change for $font at $($resolution.Width)x$($resolution.Height)."
            }
        }
    }

foreach ($scale in @(100, 125, 150, 200)) {
    foreach ($resolution in @(@{Width=1280;Height=720}, @{Width=1920;Height=1080})) {
        $atkinson = @($captures | Where-Object {
            $_.font -eq 'atkinson' -and $_.ui_scale_percent -eq $scale -and
            $_.client_width -eq $resolution.Width -and $_.client_height -eq $resolution.Height
        }) | Select-Object -First 1
        $noto = @($captures | Where-Object {
            $_.font -eq 'noto' -and $_.ui_scale_percent -eq $scale -and
            $_.client_width -eq $resolution.Width -and $_.client_height -eq $resolution.Height
        }) | Select-Object -First 1
        if ($null -eq $atkinson -or $null -eq $noto -or
            $atkinson.workspace_tools_visible -or
            $noto.workspace_tools_visible -or
            -not $atkinson.settings_tab_active -or
            -not $noto.settings_tab_active -or
            $atkinson.selected_entity_name -ne $noto.selected_entity_name -or
            $atkinson.selected_entity_id -ne $noto.selected_entity_id -or
            $atkinson.selected_entity_hidden -ne $noto.selected_entity_hidden -or
            $atkinson.selection_method -ne $noto.selection_method) {
            throw "Atkinson and Noto captures do not share the same verified editor state at $scale percent, $($resolution.Width)x$($resolution.Height)."
        }
    }
}

    $scalePairViolation = Get-HenkaTypographyCaptureScalePairViolation -Captures $captures.ToArray()
    if ($null -ne $scalePairViolation) {
        throw "Typography capture scale-pair policy failed: $scalePairViolation"
    }

    $finalHarnessIdentity = Get-HenkaSourceIdentity -RepoRoot $repoRoot
    if ($finalHarnessIdentity.commit_sha -ne $validationHarnessIdentity.commit_sha -or
        $finalHarnessIdentity.source_state -ne $validationHarnessIdentity.source_state -or
        $finalHarnessIdentity.source_identity -ne $validationHarnessIdentity.source_identity) {
        throw "The typography comparison source changed while packaged evidence was being captured."
    }

    $manifest = [pscustomobject]@{
        schema_version = 5
        created_utc = [DateTime]::UtcNow.ToString("o")
        product_executable_provenance = @{
            build_commit_sha = [string]$buildInfo.commit_sha
            build_source_identity = $sourceIdentity
            build_source_state = [string]$buildInfo.source_state
            configuration = $Configuration
            executable_sha256 = $executableHash
            build_manifest_sha256 = $buildInfoHash
            package_info_sha256 = $packageInfoHash
        }
        validation_harness_provenance = @{
            commit_sha = [string]$validationHarnessIdentity.commit_sha
            source_state = [string]$validationHarnessIdentity.source_state
            source_identity = [string]$validationHarnessIdentity.source_identity
            comparison_script_sha256 = $comparisonScriptHash
            shared_automation_helper_sha256 = $automationHelperHash
        }
        package_root_relative_to_local = $packageRootRelativeToLocal
        proof = "real packaged startup font verification; real Settings tab and scale-control input; persisted 200 percent restart; at 1280x720 and 200 percent, real Utility Settings wheel input reveals the Smooth Snap control inside the reported viewport, and two real clicks change then restore the authoritative setting; real product-reported Object Details footer geometry and Save Project click at 200 percent; visual evidence from Henka's completed main-window framebuffer at 1280x720 and borderless desktop 1920x1080; a real Utility detach through its section context menu, product-reported per-context font/atlas scale identity, and a subsequent completed engine render frame while the detached automation window remains hidden and non-foreground. Detached OpenGL pixels are not represented by a Win32 PrintWindow screenshot."
        ui_scales = @(100, 125, 150, 200)
        client_resolutions = @("1280x720", "1920x1080")
        settings_scroll_control_tests = $settingsScrollProofs.ToArray()
        object_details_compact_layout_tests = $detailsCompactProofs.ToArray()
        fonts = @{
            atkinson = $fontHashes.atkinson
            noto = $fontHashes.noto
        }
        detached_context = $detachedContextProof
        captures = $captures.ToArray()
    }
    $manifestPath = Join-Path $OutputDirectory "comparison-manifest.json"
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    $completed = $true
    Write-Output "Packaged font/scale comparison passed: $OutputDirectory"
    Write-Output "Captures: $($captures.Count); persisted Editor UI Scale was verified after restart for both candidates."
}
finally {
    if ($completed) {
        Remove-TypographyScratch -Path $scratchRoot
    }
    else {
        Write-Warning "Failed typography comparison scratch was preserved for diagnosis: $scratchRoot"
        Write-Warning "Partial screenshots, isolated settings, and captured process logs remain under $scratchRoot."
    }
}
