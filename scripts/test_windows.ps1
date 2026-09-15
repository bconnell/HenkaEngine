param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [string]$DependencyRoot = "",

    [string]$TestFilter = "",

    [string]$BuildTarget = "",

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
$buildRoot = Join-Path $repoRoot "build"
$cmake = Get-HenkaCMakePath
$ctest = Get-HenkaCTestPath -CMakePath $cmake
$provenanceScript = Join-Path $PSScriptRoot "write_build_provenance.ps1"
$resolvedDependencyRoot = $DependencyRoot

if ($SkipBuild) {
    $ctestArguments = @("--test-dir", $buildRoot, "--output-on-failure", "-C", $Configuration)
    if (-not [string]::IsNullOrWhiteSpace($TestFilter)) {
        $ctestArguments += @("-R", $TestFilter)
    }
    Write-Host "Build: skipped; executing tests against the already-proven candidate outputs"
    Write-Host "ctest: $ctest"
    Invoke-HenkaNative `
        -FilePath $ctest `
        -Arguments $ctestArguments `
        -WorkingDirectory $repoRoot `
        -Label "Run Henka Engine tests without rebuild"
    exit 0
}

$dependencyRootWasExplicit = -not [string]::IsNullOrWhiteSpace($resolvedDependencyRoot)
if ($dependencyRootWasExplicit) {
    $resolvedDependencyRoot = [System.IO.Path]::GetFullPath($resolvedDependencyRoot)
    if (-not (Test-Path -LiteralPath $resolvedDependencyRoot -PathType Container)) {
        throw "Henka dependency root was not found: $resolvedDependencyRoot"
    }
} else {
    $defaultDependencyRoot = Join-Path $buildRoot "_deps"
    if (Test-Path -LiteralPath $defaultDependencyRoot -PathType Container) {
        $resolvedDependencyRoot = $defaultDependencyRoot
    } else {
        $resolvedDependencyRoot = ""
        Write-Host "No populated local dependency root was found; FetchContent network fallback remains enabled."
    }
}
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
    try {
        $filterRegex = [System.Text.RegularExpressions.Regex]::new($TestFilter)
        $registeredCount = @(Get-HenkaCTestCommandRecords -BuildRoot $buildRoot |
            Where-Object { $filterRegex.IsMatch([string]$_.Name) }).Count
        if ($registeredCount -eq 0) {
            $configurationReady = $false
            Write-Host "Configure: required to refresh missing CTest metadata for the requested filter."
        }
    }
    catch {
        throw "TestFilter is not a valid regular expression: $TestFilter"
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
            -Label "Configure Henka Engine for tests"
        $configureStopwatch.Stop()
        $configureSeconds = $configureStopwatch.Elapsed.TotalSeconds
        $validationPlan = Resolve-HenkaValidationPlan `
            -BuildRoot $buildRoot `
            -Configuration $Configuration `
            -TestFilter $TestFilter `
            -BuildTarget $BuildTarget
    }

    $buildArguments = @("--build", $buildRoot, "--config", $Configuration)
    if (-not [string]::IsNullOrWhiteSpace($validationPlan.BuildTarget)) {
        $buildArguments += @("--target", $validationPlan.BuildTarget)
    }
    $buildStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Invoke-HenkaNative `
        -FilePath $cmake `
        -Arguments $buildArguments `
        -WorkingDirectory $repoRoot `
        -Label "Build Henka Engine tests"
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
                -Label "Install CI-only OpenGL runtime for $Configuration tests and Sandbox3D"
        }
    }

} finally {
    Exit-HenkaBuildStateLock -Lock $buildStateLock
}

$ctestArguments = @("--test-dir", $buildRoot, "--output-on-failure", "-C", $Configuration)
if (-not [string]::IsNullOrWhiteSpace($TestFilter)) {
    $ctestArguments += @("-R", $TestFilter)
}

$testStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
Invoke-HenkaNative `
    -FilePath $ctest `
    -Arguments $ctestArguments `
    -WorkingDirectory $repoRoot `
    -Label "Run Henka Engine tests"
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
            "-Configuration", $Configuration,
            "-ExecutablePath", $executablePath,
            "-CMakePath", $cmake) `
        -WorkingDirectory $repoRoot `
        -Label "Record build provenance"
} finally {
    Exit-HenkaBuildStateLock -Lock $buildStateLock
}
