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

$probeCases = @(
    [pscustomobject]@{
        Name = "agentic-workers"
        Text = -join @(
            [char]97, [char]103, [char]101, [char]110, [char]116, [char]105, [char]99,
            [char]32, [char]119, [char]111, [char]114, [char]107, [char]101, [char]114,
            [char]115
        )
        FailureMessage = "The public hygiene gate accepted private execution-agent terminology."
        PassMessage = "[pass] Public hygiene regression rejected execution-agent terminology."
    }
    [pscustomobject]@{
        Name = "agent-role-language"
        Text = -join @(
            [char]97, [char]103, [char]101, [char]110, [char]116, [char]45, [char]114,
            [char]111, [char]108, [char]101, [char]32, [char]108, [char]97, [char]110,
            [char]103, [char]117, [char]97, [char]103, [char]101
        )
        FailureMessage = "The public hygiene gate accepted private agent-role terminology."
        PassMessage = "[pass] Public hygiene regression rejected agent-role terminology."
    }
    [pscustomobject]@{
        Name = "private-prompts"
        Text = -join @(
            [char]112, [char]114, [char]105, [char]118, [char]97, [char]116, [char]101,
            [char]32, [char]112, [char]114, [char]111, [char]109, [char]112, [char]116,
            [char]115
        )
        FailureMessage = "The public hygiene gate accepted a private prompt reference."
        PassMessage = "[pass] Public hygiene regression rejected private prompt references."
    }
)

foreach ($probe in $probeCases) {
    $probeName = ".public-hygiene-probe-$PID-$($probe.Name).md"
    $probePath = Join-Path $RepositoryRoot $probeName
    try {
        [System.IO.File]::WriteAllText($probePath, $probe.Text)
        $exitCode = Invoke-HenkaExpectedFailure `
            -FilePath "powershell.exe" `
            -Arguments @(
                "-NoProfile",
                "-ExecutionPolicy", "Bypass",
                "-File", $hygieneScript) `
            -WorkingDirectory $RepositoryRoot `
            -Label "Run public repository hygiene regression ($($probe.Name))" `
            -TimeoutMilliseconds 120000
        if ($exitCode -eq 0) {
            throw $probe.FailureMessage
        }
        Write-Host $probe.PassMessage
    } finally {
        if (Test-Path -LiteralPath $probePath -PathType Leaf) {
            Remove-Item -LiteralPath $probePath -Force
        }
    }
}

exit 0
