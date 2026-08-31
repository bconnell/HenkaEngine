param(
    [string]$RepositoryRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $RepositoryRoot = (Resolve-Path $RepositoryRoot).Path
}

$probeRoot = Join-Path $RepositoryRoot ("build\test_tmp\documentation-truth-{0}" -f $PID)
$copyTargets = @(
    "docs",
    "scripts\henka_script_common.ps1",
    "scripts\check_documentation_truth.ps1",
    "README.md",
    "SUPPORT.md",
    "CONTRIBUTING.md",
    "LICENSE",
    "examples\sandbox3d\main.c",
    "templates\external_game_minimal\src\main.c",
    "engine\src\mesh\authoring_mesh.c"
)

function Invoke-TruthCheck {
    $checker = Join-Path $probeRoot "scripts\check_documentation_truth.ps1"
    return Invoke-HenkaExpectedFailure `
        -FilePath "powershell.exe" `
        -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $checker) `
        -WorkingDirectory $probeRoot `
        -Label "Run documentation truth check" `
        -TimeoutMilliseconds 120000
}

try {
    New-Item -ItemType Directory -Path $probeRoot -Force | Out-Null
    foreach ($relativePath in $copyTargets) {
        $source = Join-Path $RepositoryRoot $relativePath
        $destination = Join-Path $probeRoot $relativePath
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination -Recurse -Force
    }

    $git = (Get-Command git.exe -ErrorAction Stop).Source
    & $git -C $probeRoot init --quiet
    & $git -C $probeRoot config user.email "truth-test@example.invalid"
    & $git -C $probeRoot config user.name "Documentation Truth Test"
    & $git -c core.autocrlf=false -C $probeRoot add --all

    $authoringMeshPath = Join-Path $probeRoot "docs\authoring-mesh.md"
    $readmePath = Join-Path $probeRoot "README.md"

    if ((Invoke-TruthCheck) -ne 0) {
        throw "Documentation truth baseline unexpectedly failed."
    }

    $readmeSource = [System.IO.File]::ReadAllText($readmePath)
    $statusProbe = [regex]::Replace(
        $readmeSource,
        '(?m)^\|\s*Renderer\s*\|\s*Available \(Unhardened\)\s*\|',
        '| Renderer | Available |',
        1)
    if ($statusProbe -eq $readmeSource) {
        throw "Documentation truth regression could not locate the Renderer status row."
    }
    [System.IO.File]::WriteAllText($readmePath, $statusProbe)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a README status escalation unsupported by the status contract."
    }
    Write-Host "[pass] Documentation truth regression rejected an unsupported capability status escalation."
    [System.IO.File]::WriteAllText($readmePath, $readmeSource)

    $modelLoadingPath = Join-Path $probeRoot "docs\model-loading.md"
    $modelLoadingSource = [System.IO.File]::ReadAllText($modelLoadingPath)
    [System.IO.File]::AppendAllText(
        $modelLoadingPath,
        [Environment]::NewLine + "[Broken anchor regression](#deliberately-missing-heading)" + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a deliberately broken Markdown heading anchor."
    }
    Write-Host "[pass] Documentation truth regression rejected a broken Markdown heading anchor."
    [System.IO.File]::WriteAllText($modelLoadingPath, $modelLoadingSource)
    if ((Invoke-TruthCheck) -ne 0) {
        throw "Documentation truth remained failed after the broken Markdown heading anchor probe was removed."
    }
    Write-Host "[pass] Documentation truth returned green after the broken Markdown heading anchor probe was removed."

    $authoringMeshSourcePath = Join-Path $probeRoot "engine\src\mesh\authoring_mesh.c"
    $authoringMeshSource = [System.IO.File]::ReadAllText($authoringMeshSourcePath)
    $writerVersionMatch = [regex]::Match(
        $authoringMeshSource,
        '(?m)^\s*#define\s+HENKA_AUTHORING_MESH_FILE_VERSION\s+(\d+)U\b')
    if (-not $writerVersionMatch.Success) {
        throw "Documentation truth regression could not determine the copied writer version."
    }
    $writerVersion = [int]$writerVersionMatch.Groups[1].Value
    $staleVersion = if ($writerVersion -gt 1) { $writerVersion - 1 } else { $writerVersion + 1 }
    [System.IO.File]::AppendAllText(
        $authoringMeshPath,
        [Environment]::NewLine + "The current writer emits HAMS v$staleVersion." + [Environment]::NewLine)
    if ((Invoke-TruthCheck) -eq 0) {
        throw "Documentation truth accepted a stale current-writer version claim."
    }
    Write-Host "[pass] Documentation truth regression rejected a stale current-writer claim."
} finally {
    if (Test-Path -LiteralPath $probeRoot) {
        Remove-Item -LiteralPath $probeRoot -Recurse -Force
    }
}

exit 0
