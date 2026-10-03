param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$mainPath = Join-Path $RepositoryRoot "examples/sandbox3d/main.c"
$checkerPath = Join-Path $RepositoryRoot "scripts/check_packaged_sandbox3d_windows.ps1"
$mainSource = Get-Content -Raw -LiteralPath $mainPath
$checkerSource = Get-Content -Raw -LiteralPath $checkerPath

function Assert-Sandbox3dAuthoringDiagnosticsContract {
    param([Parameter(Mandatory = $true)][string]$Source)

    $missing = [System.Collections.Generic.List[string]]::new()
    if ($Source -notmatch '(?m)^#define SANDBOX3D_AUTOMATION_GAME_AUTHORING_LAYOUT_LOG_LIMIT [1-9][0-9]*U$') {
        $missing.Add("a positive bounded Game Authoring layout diagnostic limit")
    }

    $helperPattern = '(?s)static bool sandbox3d_automation_layout_report_begin\s*\([^)]*\)\s*\{(?<body>.*?)\n\}'
    $helperMatch = [regex]::Match($Source, $helperPattern)
    if (-not $helperMatch.Success) {
        $missing.Add("the shared bounded layout-report helper")
    }
    else {
        $helperBody = $helperMatch.Groups["body"].Value
        foreach ($required in @(
                'HENKA_AUTOMATION_DIAGNOSTICS',
                'SANDBOX3D_AUTOMATION_GAME_AUTHORING_LAYOUT_LOG_LIMIT',
                'automation_diagnostic_game_authoring_layout_log_lines',
                'cache->entity',
                'cache->bounds.x',
                'cache->bounds.y',
                'cache->bounds.width',
                'cache->bounds.height')) {
            if ($helperBody -notmatch [regex]::Escape($required)) {
                $missing.Add("layout-report helper check '$required'")
            }
        }
    }

    $sites = @(
        @{
            Cache = "game_authoring_physics_disclosure_report"
            Message = "Game authoring physics disclosure:"
        },
        @{
            Cache = "game_authoring_play_controls_report"
            Message = "Game authoring play controls:"
        },
        @{
            Cache = "game_authoring_step_controls_report"
            Message = "Game authoring step controls:"
        }
    )
    foreach ($site in $sites) {
        $pattern = '(?s)sandbox3d_automation_layout_report_begin\s*\(\s*state\s*,\s*&state->' +
            [regex]::Escape($site.Cache) +
            ',.{0,700}?printf\(\s*"' + [regex]::Escape($site.Message)
        if ($Source -notmatch $pattern) {
            $missing.Add("diagnostics-gated report for '$($site.Message)'")
        }
    }

    if ($missing.Count -gt 0) {
        throw "Sandbox3D authoring diagnostics contract failed: $($missing -join '; ')"
    }
}

Assert-Sandbox3dAuthoringDiagnosticsContract -Source $mainSource

if ($checkerSource -notmatch '(?m)^\s*\$env:HENKA_AUTOMATION_DIAGNOSTICS\s*=\s*"1"\s*$' -or
    $checkerSource -notmatch 'Game authoring physics disclosure:' -or
    $checkerSource -notmatch 'Game authoring play controls:' -or
    $checkerSource -notmatch 'Game authoring step controls:') {
    throw "The packaged UI workflow no longer enables and consumes the real authoring-layout diagnostics."
}

$negativeControl = $mainSource.Replace(
    "&state->game_authoring_physics_disclosure_report,",
    "&state->negative_control_report,")
if ($negativeControl -ceq $mainSource) {
    throw "The diagnostics negative control could not locate the Physics disclosure report site."
}
$negativeControlFailed = $false
try {
    Assert-Sandbox3dAuthoringDiagnosticsContract -Source $negativeControl
}
catch {
    $negativeControlFailed = $true
}
if (-not $negativeControlFailed) {
    throw "The diagnostics regression accepted a Physics disclosure site without its bounded reporting guard."
}

Write-Output "Sandbox3D authoring diagnostics contract passed, including its unguarded-site negative control."
