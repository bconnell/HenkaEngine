param(
    [string]$BuildDirectory = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$toolchain = Get-HenkaToolchain
$cmake = $toolchain.CMakePath
$ctest = $toolchain.CTestPath
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repoRoot "build\test_tmp\sanitized-runtime"
}
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
$buildStateLock = Enter-HenkaBuildStateLock
try {
$testFixtureDirectory = Join-Path $repoRoot "build\test_tmp"
New-Item -ItemType Directory -Path $testFixtureDirectory -Force | Out-Null

function Get-HenkaAddressSanitizerRuntime {
    $cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
    $compilerPath = $null
    if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
        $compilerLine = Get-Content -LiteralPath $cachePath |
            Where-Object { $_ -match '^CMAKE_C_COMPILER:FILEPATH=(.+)$' } |
            Select-Object -First 1
        if ($null -ne $compilerLine -and $compilerLine -match '^CMAKE_C_COMPILER:FILEPATH=(.+)$') {
            $compilerPath = $Matches[1].Trim()
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($compilerPath)) {
        $compilerRuntime = Join-Path (Split-Path -Parent $compilerPath) "clang_rt.asan_dynamic-x86_64.dll"
        if (Test-Path -LiteralPath $compilerRuntime -PathType Leaf) {
            return Get-Item -LiteralPath $compilerRuntime
        }
    }

    $visualStudioRoot = Join-Path ${env:ProgramFiles} "Microsoft Visual Studio"
    $fallback = Get-ChildItem `
        -Path $visualStudioRoot `
        -Filter "clang_rt.asan_dynamic-x86_64.dll" `
        -File `
        -Recurse `
        -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\VC\\Tools\\MSVC\\[^\\]+\\bin\\Hostx64\\x64\\' } |
        Select-Object -First 1
    if ($null -eq $fallback) {
        throw "MSVC AddressSanitizer runtime clang_rt.asan_dynamic-x86_64.dll was not found beside the configured compiler."
    }
    return $fallback
}

$configureArguments = @(
    "-S", $repoRoot,
    "-B", $BuildDirectory,
    "-DHENKA_ENABLE_SANITIZERS=ON",
    "-DHENKA_BUILD_CLIENT=OFF",
    "-DHENKA_BUILD_EXAMPLES=OFF",
    "-DHENKA_BUILD_DEDICATED_SERVER=OFF",
    "-DHENKA_ENABLE_NETWORK=ON",
    "-DHENKA_ENABLE_KTX2_TRANSCODER=OFF",
    "-DHENKA_ENABLE_LUA=ON",
    "-DHENKA_BUILD_TESTS=ON"
)

$fetchContent = Get-HenkaCMakeFetchContentArguments `
    -DependencyRoot (Join-Path $repoRoot "build\_deps") `
    -Providers @("ENET", "LUA", "MINIAUDIO", "STB")
$configureArguments += @($fetchContent.Arguments)

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments $configureArguments `
    -WorkingDirectory $repoRoot `
    -Label "Configure first-party sanitizer runtime"

$asanRuntime = Get-HenkaAddressSanitizerRuntime
$env:Path = $asanRuntime.DirectoryName + ";" + $env:Path
Write-Host "AddressSanitizer runtime: $($asanRuntime.FullName)"

Invoke-HenkaNative `
    -FilePath $cmake `
    -Arguments @("--build", $BuildDirectory, "--config", "Debug", "--parallel", "2") `
    -WorkingDirectory $repoRoot `
    -Label "Build first-party sanitizer runtime"

Invoke-HenkaNative `
    -FilePath $ctest `
    -Arguments @("--test-dir", $BuildDirectory, "--output-on-failure", "-C", "Debug") `
    -WorkingDirectory $repoRoot `
    -Label "Run first-party sanitizer runtime tests"

Write-Host "[pass] First-party sanitizer runtime gate passed."
} finally {
    Exit-HenkaBuildStateLock -Lock $buildStateLock
}
