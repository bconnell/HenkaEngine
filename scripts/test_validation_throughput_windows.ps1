param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$fixture = Join-Path $repoRoot ("build\test_tmp\validation-throughput-" + [Guid]::NewGuid().ToString("N"))
$buildRoot = Join-Path $fixture "build"
$testsRoot = Join-Path $buildRoot "tests"
$debugTestsRoot = Join-Path $testsRoot "Debug"
$cachePath = Join-Path $buildRoot "CMakeCache.txt"

function Assert-Condition {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

try {
    [System.IO.Directory]::CreateDirectory($debugTestsRoot) | Out-Null
    $henkaTestExecutable = Join-Path $debugTestsRoot "henka_tests.exe"
    $audioTestExecutable = Join-Path $debugTestsRoot "henka_audio_tests.exe"
    [System.IO.File]::WriteAllBytes($henkaTestExecutable, [byte[]]@())
    [System.IO.File]::WriteAllBytes($audioTestExecutable, [byte[]]@())
    $henkaTestExecutableCMake = $henkaTestExecutable.Replace('\', '/')
    $audioTestExecutableCMake = $audioTestExecutable.Replace('\', '/')
    [System.IO.File]::WriteAllText(
        (Join-Path $buildRoot "CTestTestfile.cmake"),
        'subdirs("tests")' + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
    $ctestFile = Join-Path $testsRoot "CTestTestfile.cmake"
    $ctestText = @(
        ('add_test([=[henka_tests]=] "' + $henkaTestExecutableCMake + '")'),
        ('add_test([=[henka_audio_tests]=] "' + $audioTestExecutableCMake + '")'),
        'add_test([=[henka_fullscreen_presentation_mapping_tests]=] "powershell.exe" "-NoProfile")'
    ) -join [Environment]::NewLine
    [System.IO.File]::WriteAllText(
        $ctestFile,
        $ctestText + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))

    $emptyFilterAccepted = $false
    try {
        Assert-HenkaCTestFilterMatchesRegisteredTests `
            -BuildRoot $buildRoot `
            -TestFilter "" `
            -Configuration Debug
        $emptyFilterAccepted = $true
    }
    catch {
        $emptyFilterAccepted = $false
    }
    Assert-Condition $emptyFilterAccepted `
        "An empty test filter should preserve the full-suite selection without failing parameter binding."

    Assert-HenkaCTestFilterMatchesRegisteredTests `
        -BuildRoot $buildRoot `
        -TestFilter '^henka_tests$' `
        -Configuration Debug
    $unmatchedFilterRejected = $false
    try {
        Assert-HenkaCTestFilterMatchesRegisteredTests `
            -BuildRoot $buildRoot `
            -TestFilter '^henka_missing_tests$' `
            -Configuration Debug
    }
    catch {
        $unmatchedFilterRejected = $_.Exception.Message -match 'matched no registered CTest tests'
    }
    Assert-Condition $unmatchedFilterRejected `
        "A test filter that matches no registered CTest test was not rejected."
    $invalidPlanRejected = $false
    try {
        Resolve-HenkaValidationPlan `
            -BuildRoot $buildRoot `
            -Configuration Debug `
            -TestFilter '(?=henka)henka_tests'
    }
    catch {
        $invalidPlanRejected = $_.Exception.Message -match 'CTest|matched no registered CTest tests'
    }
    Assert-Condition $invalidPlanRejected `
        "Validation-plan resolution accepted a CTest-invalid filter because .NET accepts it."
    $invalidCtestRegexRejected = $false
    try {
        Assert-HenkaCTestFilterMatchesRegisteredTests `
            -BuildRoot $buildRoot `
            -TestFilter '(?=henka)henka_tests' `
            -Configuration Debug
    }
    catch {
        $invalidCtestRegexRejected =
            $_.Exception.Message -match 'CTest|matched no registered CTest tests'
    }
    Assert-Condition $invalidCtestRegexRejected `
        "A .NET-valid but CTest-invalid lookahead expression was accepted as a successful selection."
    $wrapperRejectedUnmatchedFilter = $false
    try {
        & (Join-Path $repoRoot "scripts\test_windows.ps1") `
            -Configuration Debug `
            -TestFilter '^henka_no_such_registered_test$' `
            -SkipBuild
    }
    catch {
        $wrapperRejectedUnmatchedFilter =
            $_.Exception.Message -match 'matched no registered CTest tests'
    }
    Assert-Condition $wrapperRejectedUnmatchedFilter `
        "The Windows test wrapper accepted an unmatched CTest filter as a successful empty run."

    $plan = Resolve-HenkaValidationPlan `
        -BuildRoot $buildRoot `
        -Configuration Debug `
        -TestFilter '^henka_tests$'
    Assert-Condition ($plan.BuildTarget -eq "henka_tests") `
        "An exact executable CTest selection did not resolve to henka_tests."
    Assert-Condition ($plan.Artifact.Path -like "*\build\tests\Debug\henka_tests.exe") `
        "The resolved Debug artifact did not match the selected CTest executable."

    $ambiguousPlan = Resolve-HenkaValidationPlan `
        -BuildRoot $buildRoot `
        -Configuration Debug `
        -TestFilter '^henka_(tests|audio_tests)$'
    Assert-Condition ([string]::IsNullOrWhiteSpace($ambiguousPlan.BuildTarget)) `
        "An ambiguous CTest selection was incorrectly narrowed to one target."

    $scriptPlan = Resolve-HenkaValidationPlan `
        -BuildRoot $buildRoot `
        -Configuration Debug `
        -TestFilter '^henka_fullscreen_presentation_mapping_tests$'
    Assert-Condition ([string]::IsNullOrWhiteSpace($scriptPlan.BuildTarget)) `
        "A PowerShell CTest command was incorrectly treated as a CMake target."

    $fetchContent = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot (Join-Path $repoRoot "build\_deps")
    $configureArguments = @("-DCMAKE_VS_GLOBALS=TrackFileAccess=true") +
        @($fetchContent.Arguments)
    $cacheLines = @("CMAKE_HOME_DIRECTORY:INTERNAL=$repoRoot")
    foreach ($argument in @($configureArguments)) {
        if ($argument -match '^-D(?<name>[^=]+)=(?<value>.*)$') {
            $cacheLines += ("$($Matches.name):PATH=$($Matches.value)")
        }
    }
    [System.IO.File]::WriteAllText(
        $cachePath,
        ($cacheLines -join [Environment]::NewLine) + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
    Assert-Condition (Test-HenkaCMakeConfigurationReady `
            -BuildRoot $buildRoot `
            -RepositoryRoot $repoRoot `
            -ConfigureArguments $configureArguments) `
        "A matching CMake cache was not recognized as reusable."

    $cacheLines[1] = $cacheLines[1] + "-changed"
    [System.IO.File]::WriteAllText(
        $cachePath,
        ($cacheLines -join [Environment]::NewLine) + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))
    Assert-Condition (-not (Test-HenkaCMakeConfigurationReady `
            -BuildRoot $buildRoot `
            -RepositoryRoot $repoRoot `
            -ConfigureArguments $configureArguments)) `
        "A changed dependency source was incorrectly treated as reusable."

    Write-Host "[pass] Validation throughput resolver and CMake-state regression passed."
}
finally {
    if (Test-Path -LiteralPath $fixture -PathType Container) {
        [System.IO.Directory]::Delete($fixture, $true)
    }
}
