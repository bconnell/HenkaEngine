param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$BuildDirectory = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repoRoot "build"
}
$probe = Join-Path $BuildDirectory "tests\$Configuration\henka_opengl_capability_probe.exe"
if (-not (Test-Path -LiteralPath $probe -PathType Leaf)) {
    throw "OpenGL capability probe was not built: $probe"
}

Write-Host ""
Write-Host "==> Run Henka OpenGL capability probe"
Write-Host "    $probe"
$previousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try {
    $probeOutput = (& $probe 2>&1 | ForEach-Object { [string]$_ }) -join "`n"
    $probeExitCode = $LASTEXITCODE
}
finally {
    $ErrorActionPreference = $previousErrorActionPreference
}

Write-Host $probeOutput
$capabilityMatches = [regex]::Matches(
    $probeOutput,
    "HENKA_OPENGL_CAPABILITY status=(?<status>[A-Z_]+) stage=(?<stage>[a-z-]+)(?:[^\r\n]*)")
if ($capabilityMatches.Count -eq 0) {
    throw "OpenGL capability probe did not emit a structured HENKA_OPENGL_CAPABILITY record."
}

$capability = $capabilityMatches[$capabilityMatches.Count - 1]
$status = $capability.Groups["status"].Value
$stage = $capability.Groups["stage"].Value
switch ($status) {
    "PASS" {
        if ($probeExitCode -ne 0) {
            throw "OpenGL capability probe reported PASS but exited with code $probeExitCode."
        }
        Write-Host "[pass] OpenGL baseline and required Henka entry points are available."
    }
    "INFRASTRUCTURE_BLOCKED" {
        throw "OpenGL capability is infrastructure-blocked at stage '$stage'; the runner did not provide Henka's declared OpenGL baseline."
    }
    "PRODUCT_FAILURE" {
        throw "OpenGL capability is present but Henka failed at stage '$stage'."
    }
    default {
        throw "OpenGL capability probe emitted unknown status '$status'."
    }
}
