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
        RuleName = "internal development wording 1"
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
        RuleName = "internal development wording 3"
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
        RuleName = "internal development wording 4"
        Text = -join @(
            [char]112, [char]114, [char]105, [char]118, [char]97, [char]116, [char]101,
            [char]32, [char]112, [char]114, [char]111, [char]109, [char]112, [char]116,
            [char]115
        )
        FailureMessage = "The public-content gate accepted internal development-process wording."
        PassMessage = "[pass] Public-content regression rejected internal development-process wording."
    }
)

$probePaths = @()
foreach ($probe in $probeCases) {
    $probeName = ".public-hygiene-probe-$PID-$($probe.Name).md"
    $probePath = Join-Path $RepositoryRoot $probeName
    if (Test-Path -LiteralPath $probePath) {
        throw "The public hygiene regression probe path already exists: $probePath"
    }
    $probePaths += $probePath
}

$pathProbeDirectory = Join-Path $RepositoryRoot "docs\superpowers"
$pathProbePath = Join-Path $pathProbeDirectory ".public-hygiene-path-probe-$PID.md"
if (Test-Path -LiteralPath $pathProbeDirectory) {
    throw "The framework-branded public documentation path already exists; refusing to run the scoped path probe."
}

try {
    foreach ($probeIndex in 0..($probeCases.Count - 1)) {
        $probe = $probeCases[$probeIndex]
        $probePath = $probePaths[$probeIndex]
        [System.IO.File]::WriteAllText($probePath, $probe.Text)
    }

    New-Item -ItemType Directory -Path $pathProbeDirectory | Out-Null
    [System.IO.File]::WriteAllText($pathProbePath, "scoped path regression probe")
    $result = Invoke-HenkaExpectedFailure `
        -FilePath "powershell.exe" `
        -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $hygieneScript) `
        -WorkingDirectory $RepositoryRoot `
        -Label "Run public repository hygiene regression set" `
        -TimeoutMilliseconds 120000 `
        -ReturnOutput
    if ($result.ExitCode -eq 0) {
        throw "The public-content gate accepted one or more internal development or framework-path probes."
    }
    $output = ([string]$result.Stdout) + [Environment]::NewLine + ([string]$result.Stderr)
    foreach ($probeIndex in 0..($probeCases.Count - 1)) {
        $probe = $probeCases[$probeIndex]
        $probeName = [System.IO.Path]::GetFileName($probePaths[$probeIndex])
        if ($output -notmatch [regex]::Escape($probeName) -or
            $output -notmatch [regex]::Escape($probe.RuleName)) {
            throw "The public-content gate did not report the expected $($probe.Name) finding during the combined regression scan."
        }
    }
    $relativePathProbe = $pathProbePath.Substring($RepositoryRoot.Length).TrimStart('\').Replace('\', '/')
    if ($output -notmatch [regex]::Escape($relativePathProbe)) {
        throw "The public-content gate did not report the expected framework-branded public path finding during the combined regression scan."
    }
    Write-Host "[pass] Public-content regression rejected all internal development wording probes in one scan."
    Write-Host "[pass] Public-content regression rejected framework-branded public documentation paths."
} finally {
    foreach ($probePath in $probePaths) {
        if (Test-Path -LiteralPath $probePath -PathType Leaf) {
            Remove-Item -LiteralPath $probePath -Force
        }
    }
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
