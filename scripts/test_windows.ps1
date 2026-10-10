param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$DependencyRoot = "",

    [string]$TestFilter = "",

    [string]$BuildTarget = "",

    [string]$BuildRoot = "",

    [ValidateRange(30, 3600)]
    [int]$PerTestTimeoutSeconds = 300,

    [ValidateRange(60, 3600)]
    [int]$CommandTimeoutSeconds = 1200,

    [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$totalStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
$configureSeconds = 0.0
$buildSeconds = 0.0
$testSeconds = 0.0
$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$buildRoot = if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    Get-HenkaBuildRoot -RepositoryRoot $repoRoot
} else {
    Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $BuildRoot
}
$resolvedDependencyRoot = $DependencyRoot
$dependencyRootWasExplicit = -not [string]::IsNullOrWhiteSpace($resolvedDependencyRoot)
if ($dependencyRootWasExplicit) {
    $resolvedDependencyRoot = Resolve-HenkaDependencyRoot `
        -RepositoryRoot $repoRoot `
        -DependencyRoot $resolvedDependencyRoot
}
$cmake = Get-HenkaCMakePath
$ctest = Get-HenkaCTestPath -CMakePath $cmake
$provenanceScript = Join-Path $PSScriptRoot "write_build_provenance.ps1"
$commandTimeoutMilliseconds = $CommandTimeoutSeconds * 1000

if ($SkipBuild) {
    Assert-HenkaCTestFilterMatchesRegisteredTests `
        -BuildRoot $buildRoot `
        -TestFilter $TestFilter `
        -Configuration $Configuration
    $ctestArguments = @(
    "--test-dir", $buildRoot,
    "--output-on-failure",
    "--timeout", [string]$PerTestTimeoutSeconds,
    "-C", $Configuration)
    if (-not [string]::IsNullOrWhiteSpace($TestFilter)) {
        $ctestArguments += @("-R", $TestFilter)
    }
    Write-Host "Build: skipped; executing tests against the already-proven candidate outputs"
    Write-Host "ctest: $ctest"
    Invoke-HenkaNative `
        -FilePath $ctest `
        -Arguments $ctestArguments `
        -WorkingDirectory $repoRoot `
        -Label "Run Henka Engine tests without rebuild" `
        -TimeoutMilliseconds $commandTimeoutMilliseconds
    exit 0
}

if (-not $dependencyRootWasExplicit) {
    $defaultDependencyRoot = Join-Path $buildRoot "_deps"
    if (Test-Path -LiteralPath $defaultDependencyRoot -PathType Container) {
        $resolvedDependencyRoot = Resolve-HenkaDependencyRoot `
            -RepositoryRoot $repoRoot `
            -DependencyRoot $defaultDependencyRoot
    } else {
        $resolvedDependencyRoot = ""
        Write-Host "No populated local dependency root was found; FetchContent network fallback remains enabled."
    }
}

# Reject invalid dependency storage before creating or marking the build root.
$null = New-HenkaLocalDirectory -RepositoryRoot $repoRoot -Path $buildRoot
$null = Write-HenkaGeneratedRootMarker `
    -RepoRoot $repoRoot `
    -Path $buildRoot `
    -Purpose "CTest build and runtime output" `
    -RetentionClass "REBUILDABLE" `
    -Active $false `
    -CleanupEligible $true `
    -CleanupCondition "source inputs are preserved and no validation process is using this build root" `
    -Configuration "multi-configuration"

$fetchContent = Get-HenkaCMakeFetchContentArguments `
    -DependencyRoot $resolvedDependencyRoot `
    -Providers @("SDL3", "KTXSOFTWARE", "ENET", "LUA", "MINIAUDIO", "STB")
$configureArguments = @(
    "-S", $repoRoot,
    "-B", $buildRoot,
    "-DCMAKE_VS_GLOBALS=TrackFileAccess=true") + @($fetchContent.Arguments)
$validationPlan = Resolve-HenkaValidationPlan `
    -BuildRoot $buildRoot `
    -Configuration $Configuration `
    -TestFilter $TestFilter `
    -BuildTarget $BuildTarget
$configurationReady = Test-HenkaCMakeConfigurationReady `
    -BuildRoot $buildRoot `
    -RepositoryRoot $repoRoot `
    -ConfigureArguments $configureArguments
if ($configurationReady -and
    -not [string]::IsNullOrWhiteSpace($TestFilter) -and
    $validationPlan.Resolution -eq "aggregate-or-unresolved") {
    $registeredCount = @(Get-HenkaCTestCommandRecords `
        -BuildRoot $buildRoot `
        -Configuration $Configuration `
        -TestFilter $TestFilter).Count
    if ($registeredCount -eq 0) {
        $configurationReady = $false
        Write-Host "Configure: required to refresh missing CTest metadata for the requested filter."
    }
}
foreach ($provider in $fetchContent.ProviderStates) {
    if ($provider.Available) {
        Write-Host "$($provider.Label) provider: repository-local populated source"
    } else {
        Write-Host "$($provider.Label) provider: FetchContent network fallback"
    }
}
if ($fetchContent.FullyDisconnected) {
    Write-Host "FetchContent mode: fully disconnected because all repository-local providers are present"
} else {
    Write-Host "FetchContent mode: normal network-capable fallback for missing providers"
}

Write-Host "cmake: $cmake"
Write-Host "ctest: $ctest"
Write-Host "repo: $repoRoot"
$dependencyDescription = if ([string]::IsNullOrWhiteSpace($resolvedDependencyRoot)) {
    "<none; FetchContent fallback>"
} else {
    $resolvedDependencyRoot
}
Write-Host "dependency root: $dependencyDescription"
Write-Host "validation plan: $($validationPlan.Resolution)"
if (-not [string]::IsNullOrWhiteSpace($validationPlan.BuildTarget)) {
    Write-Host "validation target: $($validationPlan.BuildTarget)"
}
Write-Host "validation artifact: $($validationPlan.Artifact.Path)"

$buildStateLock = Enter-HenkaBuildStateLock
try {
    if ($configurationReady) {
        Write-Host "Configure: skipped; matching CMake cache is reusable."
    } else {
        $configureStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        Invoke-HenkaNative `
            -FilePath $cmake `
            -Arguments $configureArguments `
            -WorkingDirectory $repoRoot `
            -Label "Configure Henka Engine for tests" `
            -TimeoutMilliseconds $commandTimeoutMilliseconds
        $configureStopwatch.Stop()
        $configureSeconds = $configureStopwatch.Elapsed.TotalSeconds
        $validationPlan = Resolve-HenkaValidationPlan `
            -BuildRoot $buildRoot `
            -Configuration $Configuration `
            -TestFilter $TestFilter `
            -BuildTarget $BuildTarget
    }

    Assert-HenkaCTestFilterMatchesRegisteredTests `
        -BuildRoot $buildRoot `
        -TestFilter $TestFilter `
        -Configuration $Configuration

    $buildArguments = @("--build", $buildRoot, "--config", $Configuration)
    if (-not [string]::IsNullOrWhiteSpace($validationPlan.BuildTarget)) {
        $buildArguments += @("--target", $validationPlan.BuildTarget)
    }
    $buildStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Invoke-HenkaNative `
        -FilePath $cmake `
        -Arguments $buildArguments `
        -WorkingDirectory $repoRoot `
        -Label "Build Henka Engine tests" `
        -TimeoutMilliseconds $commandTimeoutMilliseconds
    $buildStopwatch.Stop()
    $buildSeconds = $buildStopwatch.Elapsed.TotalSeconds

    $softwareOpenGLRoot = [string]$env:HENKA_CI_SOFTWARE_OPENGL_ROOT
    if (-not [string]::IsNullOrWhiteSpace($softwareOpenGLRoot)) {
        $softwareOpenGLInstaller = Join-Path $PSScriptRoot "install_windows_software_opengl.ps1"
        $softwareOpenGLTargets = @(
            (Join-Path $buildRoot "tests\$Configuration")
        )
        $sandboxDirectory = Join-Path $buildRoot "examples\sandbox3d\$Configuration"
        if (Test-Path -LiteralPath $sandboxDirectory -PathType Container) {
            $softwareOpenGLTargets += $sandboxDirectory
        }
        foreach ($targetDirectory in $softwareOpenGLTargets) {
            Invoke-HenkaNative `
                -FilePath "powershell.exe" `
                -Arguments @(
                    "-NoProfile",
                    "-ExecutionPolicy", "Bypass",
                    "-File", $softwareOpenGLInstaller,
                    "-SourceDirectory", $softwareOpenGLRoot,
                    "-TargetDirectory", $targetDirectory) `
                -WorkingDirectory $repoRoot `
                -Label "Install CI-only OpenGL runtime for $Configuration tests and Sandbox3D" `
                -TimeoutMilliseconds $commandTimeoutMilliseconds
        }
    }

} finally {
    Exit-HenkaBuildStateLock -Lock $buildStateLock
}

$ctestArguments = @(
    "--test-dir", $buildRoot,
    "--output-on-failure",
    "--timeout", [string]$PerTestTimeoutSeconds,
    "-C", $Configuration)
Assert-HenkaCTestFilterMatchesRegisteredTests `
    -BuildRoot $buildRoot `
    -TestFilter $TestFilter `
    -Configuration $Configuration
if (-not [string]::IsNullOrWhiteSpace($TestFilter)) {
    $ctestArguments += @("-R", $TestFilter)
}

$testStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
Invoke-HenkaNative `
    -FilePath $ctest `
    -Arguments $ctestArguments `
    -WorkingDirectory $repoRoot `
    -Label "Run Henka Engine tests" `
    -TimeoutMilliseconds $commandTimeoutMilliseconds
$testStopwatch.Stop()
$testSeconds = $testStopwatch.Elapsed.TotalSeconds

$totalStopwatch.Stop()
Write-Host ("VALIDATION_TIMING_SECONDS configure={0:N3} build={1:N3} test={2:N3} total={3:N3}" -f `
    $configureSeconds, $buildSeconds, $testSeconds, $totalStopwatch.Elapsed.TotalSeconds)

$buildStateLock = Enter-HenkaBuildStateLock
try {
    $executablePath = $validationPlan.Artifact.Path
    Invoke-HenkaNative `
        -FilePath "powershell.exe" `
        -Arguments @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", $provenanceScript,
            "-RepoRoot", $repoRoot,
            "-BuildRoot", $buildRoot,
            "-Configuration", $Configuration,
            "-ExecutablePath", $executablePath,
            "-CMakePath", $cmake) `
        -WorkingDirectory $repoRoot `
        -Label "Record build provenance" `
        -TimeoutMilliseconds $commandTimeoutMilliseconds
} finally {
    Exit-HenkaBuildStateLock -Lock $buildStateLock
}
