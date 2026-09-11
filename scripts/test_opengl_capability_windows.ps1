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

$seamlessMatches = [regex]::Matches(
    $probeOutput,
    "HENKA_OPENGL_CUBEMAP_SEAMLESS status=(?<status>PASS|FAILED)")
if ($seamlessMatches.Count -eq 0) {
    throw "OpenGL capability probe did not prove that Henka enabled cubemap seamless filtering during renderer initialization."
}
$seamless = $seamlessMatches[$seamlessMatches.Count - 1]
if ($seamless.Groups["status"].Value -ne "PASS") {
    throw "Henka cubemap seamless filtering initialization failed."
}

$capability = $capabilityMatches[$capabilityMatches.Count - 1]
$status = $capability.Groups["status"].Value
$stage = $capability.Groups["stage"].Value
$baselineCapabilityMatches = @(
    $capabilityMatches | Where-Object {
        $_.Groups["status"].Value -eq "CONTEXT_READY" -and
        $_.Groups["stage"].Value -eq "context-capability"
    })
if ($baselineCapabilityMatches.Count -eq 0) {
    throw "OpenGL capability probe did not emit a CONTEXT_READY baseline record."
}
$baselineCapability = $baselineCapabilityMatches[$baselineCapabilityMatches.Count - 1]
$policyPath = Join-Path $repoRoot "engine/src/renderer/opengl_capability_policy.h"
if (-not (Test-Path -LiteralPath $policyPath -PathType Leaf)) {
    throw "OpenGL capability policy header was not found: $policyPath"
}
$policySource = [System.IO.File]::ReadAllText($policyPath)
$requiredLimits = @(
    @{ Field = "limits_frag"; Macro = "HENKA_OPENGL_REQUIRED_FRAGMENT_TEXTURE_IMAGE_UNITS" },
    @{ Field = "limits_combined"; Macro = "HENKA_OPENGL_REQUIRED_COMBINED_TEXTURE_IMAGE_UNITS" },
    @{ Field = "limits_draw_buffers"; Macro = "HENKA_OPENGL_REQUIRED_DRAW_BUFFERS" },
    @{ Field = "limits_color_attachments"; Macro = "HENKA_OPENGL_REQUIRED_COLOR_ATTACHMENTS" },
    @{ Field = "limits_texture_size"; Macro = "HENKA_OPENGL_REQUIRED_TEXTURE_SIZE" },
    @{ Field = "limits_cube_size"; Macro = "HENKA_OPENGL_REQUIRED_CUBE_MAP_TEXTURE_SIZE" }
)
foreach ($limit in $requiredLimits) {
    $requiredMatch = [regex]::Match(
        $policySource,
        "(?m)^\s*#define\s+$([regex]::Escape($limit.Macro))\s+(?<value>\d+)\b")
    if (-not $requiredMatch.Success) {
        throw "OpenGL capability policy did not define $($limit.Macro)."
    }
    $actualMatch = [regex]::Match(
        $baselineCapability.Value,
        "\b$([regex]::Escape($limit.Field))=(?<value>\d+)\b")
    if (-not $actualMatch.Success) {
        throw "OpenGL capability probe did not report $($limit.Field)."
    }
    $requiredValue = [int]$requiredMatch.Groups["value"].Value
    $actualValue = [int]$actualMatch.Groups["value"].Value
    if ($actualValue -lt $requiredValue) {
        throw "OpenGL capability probe reported $($limit.Field)=$actualValue, below the declared minimum $requiredValue."
    }
}
Write-Host "[pass] OpenGL capability probe reported all declared resource limits at or above the supported baseline."
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
