param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$fixture = Join-Path $repoRoot ("build\test_tmp\validation-throughput-" + [Guid]::NewGuid().ToString("N"))
$buildRoot = Join-Path $fixture "build"
$testsRoot = Join-Path $buildRoot "tests"
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
    [System.IO.Directory]::CreateDirectory($testsRoot) | Out-Null
    $ctestFile = Join-Path $testsRoot "CTestTestfile.cmake"
    $ctestText = @(
        'add_test([=[henka_tests]=] "C:/fixture/build/tests/Debug/henka_tests.exe")',
        'add_test([=[henka_audio_tests]=] "C:/fixture/build/tests/Debug/henka_audio_tests.exe")',
        'add_test([=[henka_fullscreen_presentation_mapping_tests]=] "powershell.exe" "-NoProfile")'
    ) -join [Environment]::NewLine
    [System.IO.File]::WriteAllText(
        $ctestFile,
        $ctestText + [Environment]::NewLine,
        [System.Text.UTF8Encoding]::new($false))

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
