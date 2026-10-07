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
    $BuildDirectory = Get-HenkaBuildRoot -RepositoryRoot $repoRoot
}
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$expectedWorkingDirectory = [System.IO.Path]::GetFullPath($BuildDirectory).TrimEnd('\', '/')
$localRoot = [System.IO.Path]::GetFullPath((Get-HenkaLocalRoot -RepositoryRoot $repoRoot)).TrimEnd('\', '/')
$null = Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $expectedWorkingDirectory

if (-not (Test-Path -LiteralPath $BuildDirectory -PathType Container)) {
    throw "CTest build directory does not exist: $BuildDirectory"
}

$ctestJsonText = (@(
    & $ctest --test-dir $BuildDirectory --show-only=json-v1 -C $Configuration 2>&1
) -join [Environment]::NewLine)
if ($LASTEXITCODE -ne 0) {
    throw "CTest JSON test listing failed with exit code $LASTEXITCODE."
}
try {
    $ctestJson = $ctestJsonText | ConvertFrom-Json
}
catch {
    throw "CTest JSON test listing was not valid JSON: $($_.Exception.Message)"
}

$jsonTests = @($ctestJson.tests | Where-Object { ([string]$_.name) -like "henka_*" })
if ($jsonTests.Count -eq 0) {
    throw "The CTest JSON listing did not contain first-party Henka tests."
}

$workingDirectoryFailures = New-Object System.Collections.Generic.List[string]
$timeoutFailures = New-Object System.Collections.Generic.List[string]
foreach ($test in $jsonTests) {
    $workingDirectoryProperty = @($test.properties |
        Where-Object { ([string]$_.name) -eq "WORKING_DIRECTORY" } |
        Select-Object -First 1)
    if ($workingDirectoryProperty.Count -ne 1 -or
        [string]::IsNullOrWhiteSpace([string]$workingDirectoryProperty[0].value)) {
        $workingDirectoryFailures.Add("$($test.name): <missing>")
    }
    else {
        try {
            $actualWorkingDirectory = [System.IO.Path]::GetFullPath(
                ([string]$workingDirectoryProperty[0].value).Trim('"'))
            if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals(
                    $actualWorkingDirectory.TrimEnd('\', '/'),
                    $expectedWorkingDirectory)) {
                $workingDirectoryFailures.Add(
                    "$($test.name): $actualWorkingDirectory")
            }
        }
        catch {
            $workingDirectoryFailures.Add(
                "$($test.name): $($workingDirectoryProperty[0].value)")
        }
    }

    $timeoutProperty = @($test.properties |
        Where-Object { ([string]$_.name) -eq "TIMEOUT" } |
        Select-Object -First 1)
    if ($timeoutProperty.Count -ne 1) {
        $timeoutFailures.Add("$($test.name): <missing>")
        continue
    }
    try {
        $timeoutSeconds = [double]$timeoutProperty[0].value
    }
    catch {
        $timeoutFailures.Add("$($test.name): $($timeoutProperty[0].value)")
        continue
    }
    if ([Math]::Abs($timeoutSeconds - 300.0) -gt 0.001) {
        $timeoutFailures.Add("$($test.name): $timeoutSeconds")
    }
}

if ($workingDirectoryFailures.Count -gt 0) {
                Write-Error ("CTest working-directory contract failed. Expected binary directory '$expectedWorkingDirectory' under canonical local root '$localRoot': " +
        ($workingDirectoryFailures -join "; "))
    exit 1
}
if ($timeoutFailures.Count -gt 0) {
    Write-Error ("CTest timeout contract failed. Expected 300 seconds for every first-party test: " +
        ($timeoutFailures -join "; "))
    exit 1
}

Write-Output ("[pass] {0} first-party CTest entries use canonical-local binary working directory {1}." -f
    $jsonTests.Count, $expectedWorkingDirectory)
Write-Output ("[pass] {0} first-party CTest entries carry the 300-second per-test timeout contract." -f
    $jsonTests.Count)
