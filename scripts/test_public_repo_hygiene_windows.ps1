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
        Name = "internal-development-wording-1"
        Text = -join @(
            [char]97, [char]103, [char]101, [char]110, [char]116, [char]105, [char]99,
            [char]32, [char]119, [char]111, [char]114, [char]107, [char]101, [char]114,
            [char]115
        )
        FailureMessage = "The public-content gate accepted internal development-process wording."
        PassMessage = "[pass] Public-content regression rejected internal development-process wording."
    }
    [pscustomobject]@{
        Name = "internal-development-wording-3"
        Text = -join @(
            [char]97, [char]103, [char]101, [char]110, [char]116, [char]45, [char]114,
            [char]111, [char]108, [char]101, [char]32, [char]108, [char]97, [char]110,
            [char]103, [char]117, [char]97, [char]103, [char]101
        )
        FailureMessage = "The public-content gate accepted internal development-process wording."
        PassMessage = "[pass] Public-content regression rejected internal development-process wording."
    }
    [pscustomobject]@{
        Name = "internal-development-wording-4"
        Text = -join @(
            [char]112, [char]114, [char]105, [char]118, [char]97, [char]116, [char]101,
            [char]32, [char]112, [char]114, [char]111, [char]109, [char]112, [char]116,
            [char]115
        )
        FailureMessage = "The public-content gate accepted internal development-process wording."
        PassMessage = "[pass] Public-content regression rejected internal development-process wording."
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

$pathProbeDirectory = Join-Path $RepositoryRoot "docs\superpowers"
$pathProbePath = Join-Path $pathProbeDirectory ".public-hygiene-path-probe-$PID.md"
if (Test-Path -LiteralPath $pathProbeDirectory) {
    throw "The framework-branded public documentation path already exists; refusing to run the scoped path probe."
}

try {
    New-Item -ItemType Directory -Path $pathProbeDirectory | Out-Null
    [System.IO.File]::WriteAllText($pathProbePath, "scoped path regression probe")
    $exitCode = Invoke-HenkaExpectedFailure `
        -FilePath "powershell.exe" `
        -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $hygieneScript) `
        -WorkingDirectory $RepositoryRoot `
        -Label "Run public repository hygiene regression (framework-public-path)" `
        -TimeoutMilliseconds 120000
    if ($exitCode -eq 0) {
        throw "The public-content gate accepted a framework-branded public documentation path."
    }
    Write-Host "[pass] Public-content regression rejected framework-branded public documentation paths."
} finally {
    if (Test-Path -LiteralPath $pathProbePath -PathType Leaf) {
        Remove-Item -LiteralPath $pathProbePath -Force
    }
    if (Test-Path -LiteralPath $pathProbeDirectory -PathType Container) {
        $remaining = @(Get-ChildItem -LiteralPath $pathProbeDirectory -Force)
        if ($remaining.Count -eq 0) {
            Remove-Item -LiteralPath $pathProbeDirectory -Force
        }
    }
}

exit 0
