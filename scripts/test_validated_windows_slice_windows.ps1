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

$runner = Join-Path $RepositoryRoot "scripts\invoke_validated_windows_slice.ps1"
$sliceName = "harness-contract-probe-$PID"
$downloads = Join-Path ([Environment]::GetFolderPath("UserProfile")) "Downloads"
$evidencePrefix = "HenkaEngine-$sliceName-FAILED-"
$missingAnchor = "README.md|__validated_slice_probe_missing_literal__|1"
$output = @()
$exitCode = 0

try {
    $output = @(& powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File $runner `
        -CandidateCommitSubject "Harness contract probe" `
        -SliceName $sliceName `
        -SourceAnchor $missingAnchor 2>&1)
    $exitCode = $LASTEXITCODE
    $output | ForEach-Object { Write-Host ([string]$_) }

    if ($exitCode -eq 0) {
        throw "The validated-slice entrypoint accepted a deliberately invalid source anchor."
    }

    $outputText = ($output | ForEach-Object { [string]$_ }) -join "`n"
    if ($outputText -notmatch "\[fail\] Validated Windows slice failed:" -or
        $outputText -notmatch "\[fail\] Validated Windows slice entrypoint failed:") {
        throw "The validated-slice entrypoint did not report deterministic failure markers."
    }

    Write-Host "[pass] Validated Windows slice failure propagation regression passed."
}
finally {
    Get-ChildItem -LiteralPath $downloads -Filter "$evidencePrefix*" -Force -ErrorAction SilentlyContinue |
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
}

exit 0
