param(
    [string]$RepositoryRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

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
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $checker 2>&1
    $exitCode = $LASTEXITCODE
    $output | ForEach-Object { Write-Host $_ }
    return [int]$exitCode
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

    if ((Invoke-TruthCheck) -ne 0) {
        throw "Documentation truth baseline unexpectedly failed."
    }

    [System.IO.File]::AppendAllText(
        $authoringMeshPath,
        [Environment]::NewLine + "The current writer emits HAMS v3." + [Environment]::NewLine)
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
