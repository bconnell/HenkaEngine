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
$generatorScriptPath = Join-Path $RepositoryRoot "scripts\generate_residency_fixtures_windows.ps1"

foreach ($path in @($sandboxCMakePath, $packageScriptPath, $generatorScriptPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Fixture-scope contract input is missing: $path"
    }
}

$sandboxLines = Get-Content -LiteralPath $sandboxCMakePath
$sandboxText = $sandboxLines -join "`n"
$packageText = Get-Content -LiteralPath $packageScriptPath -Raw
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

Write-Output "[pass] Sandbox3D residency fixtures are excluded from normal builds and generated only by the explicit stress target."
