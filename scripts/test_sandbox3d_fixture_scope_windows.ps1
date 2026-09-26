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
$generatorScriptPath = Join-Path $RepositoryRoot "scripts\generate_residency_fixtures_windows.ps1"
$genericModelingScriptPath = Join-Path $RepositoryRoot "scripts\test_visible_native_modeling_windows.ps1"
$windowsCiWorkflowPath = Join-Path $RepositoryRoot ".github\workflows\windows-ci.yml"

foreach ($path in @($sandboxCMakePath, $packageScriptPath, $packageValidationPath, $generatorScriptPath, $genericModelingScriptPath, $windowsCiWorkflowPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Fixture-scope contract input is missing: $path"
    }
}

$sandboxLines = Get-Content -LiteralPath $sandboxCMakePath
$sandboxText = $sandboxLines -join "`n"
$packageText = Get-Content -LiteralPath $packageScriptPath -Raw
$packageValidationText = Get-Content -LiteralPath $packageValidationPath -Raw
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

Write-Output "[pass] Sandbox3D residency fixtures are excluded from normal builds and generated only by the explicit stress target."
Write-Output "[pass] Visible native modeling explicitly uses the bounded primitive gallery; quote-style parser controls passed."
Write-Output "[pass] Hosted packaged startup retains an independent clean product-native default-scene gate."
