param(
    [string]$BuildDirectory = "",
    [string]$Configuration = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$cmake = Get-HenkaCMakePath
$ctest = Get-HenkaCTestPath -CMakePath $cmake
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repoRoot "build"
}
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$expectedWorkingDirectory = [System.IO.Path]::GetFullPath($repoRoot).TrimEnd('\', '/')

if (-not (Test-Path -LiteralPath $BuildDirectory -PathType Container)) {
    throw "CTest build directory does not exist: $BuildDirectory"
}

$ctestOutput = @(& $ctest --test-dir $BuildDirectory -N -V -C $Configuration 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "CTest test listing failed with exit code $LASTEXITCODE."
}

$records = New-Object System.Collections.Generic.List[object]
$workingDirectory = $null
foreach ($line in $ctestOutput) {
    $lineText = [string]$line
    if ($lineText -match '^\s*(?:\d+:\s*)?Working Directory:\s*(?<directory>.+?)\s*$') {
        $workingDirectory = $Matches['directory'].Trim()
        continue
    }

    if ($lineText -match '^\s*(?:\d+:\s*)?Test #\d+:\s*(?<name>henka_[^\s]+)\s*$') {
        $records.Add([pscustomobject]@{
            Name = $Matches['name']
            WorkingDirectory = $workingDirectory
        })
        $workingDirectory = $null
    }
}

if ($records.Count -eq 0) {
    throw "The CTest listing did not contain first-party Henka tests."
}

$failures = @(
    $records | Where-Object {
        if ([string]::IsNullOrWhiteSpace($_.WorkingDirectory)) {
            return $true
        }

        $actualWorkingDirectory = [System.IO.Path]::GetFullPath(
            $_.WorkingDirectory.Trim('"'))
        return -not [System.StringComparer]::OrdinalIgnoreCase.Equals(
            $actualWorkingDirectory.TrimEnd('\', '/'),
            $expectedWorkingDirectory)
    }
)

if ($failures.Count -gt 0) {
    foreach ($failure in $failures) {
        $actual = if ([string]::IsNullOrWhiteSpace($failure.WorkingDirectory)) {
            "<missing>"
        } else {
            $failure.WorkingDirectory
        }
        $message = (
            "CTest working-directory contract failed for {0}: expected '{1}', " +
            "actual '{2}'." -f $failure.Name, $expectedWorkingDirectory, $actual)
        Write-Error $message
    }
    exit 1
}

$passMessage = (
    "[pass] {0} first-party CTest entries use repository-root working directory {1}." -f
    $records.Count, $expectedWorkingDirectory)
Write-Output $passMessage
