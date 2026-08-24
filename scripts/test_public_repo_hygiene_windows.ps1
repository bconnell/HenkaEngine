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

$hygieneScript = Join-Path $RepositoryRoot "scripts\check_public_repo_hygiene.ps1"
$probeName = ".public-hygiene-probe-$PID.md"
$probePath = Join-Path $RepositoryRoot $probeName
$probeText = -join @(
    [char]97, [char]103, [char]101, [char]110, [char]116, [char]105, [char]99,
    [char]32, [char]119, [char]111, [char]114, [char]107, [char]101, [char]114,
    [char]115
)

try {
    [System.IO.File]::WriteAllText($probePath, $probeText)
    $exitCode = Invoke-HenkaExpectedFailure `
        -FilePath "powershell.exe" `
        -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $hygieneScript) `
        -WorkingDirectory $RepositoryRoot `
        -Label "Run public repository hygiene regression" `
        -TimeoutMilliseconds 120000
    if ($exitCode -eq 0) {
        throw "The public hygiene gate accepted private execution-agent terminology."
    }
    Write-Host "[pass] Public hygiene regression rejected the execution-agent probe."
} finally {
    if (Test-Path -LiteralPath $probePath -PathType Leaf) {
        Remove-Item -LiteralPath $probePath -Force
    }
}

exit 0
