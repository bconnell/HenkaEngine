Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$git = Get-HenkaGitPath
$fixtureRoot = Join-Path (Get-HenkaTestTemporaryRoot -RepositoryRoot $repoRoot) ("cmake-fetch-content-fixture-" + [Guid]::NewGuid().ToString("N"))
$dependencyRoot = Join-Path $fixtureRoot "_deps"

function Assert-Condition {
    param(
        [Parameter(Mandatory = $true)]
        [bool]$Condition,

        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Assert-Argument {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,

        [Parameter(Mandatory = $true)]
        [string]$Expected
    )

    Assert-Condition ($Arguments -contains $Expected) "Missing CMake argument: $Expected"
}

try {
    New-Item -ItemType Directory -Path $dependencyRoot -Force | Out-Null
    $markers = @{
        "sdl3-src\CMakeLists.txt" = "project(SDL3)"
        "ktxsoftware-src\CMakeLists.txt" = "project(KTXSoftware)"
        "enet-src\CMakeLists.txt" = "project(ENet)"
        "lua-src\lua.h" = "/* lua */"
        "miniaudio-src\miniaudio.h" = "/* miniaudio */"
        "stb-src\stb_vorbis.c" = "/* stb */"
    }
    foreach ($relativePath in $markers.Keys) {
        $markerPath = Join-Path $dependencyRoot $relativePath
        New-Item -ItemType Directory -Path (Split-Path -Parent $markerPath) -Force | Out-Null
        [System.IO.File]::WriteAllText($markerPath, $markers[$relativePath])
    }

    $allProviders = @("SDL3", "KTXSOFTWARE", "ENET", "LUA", "MINIAUDIO", "STB")
    $allLocal = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot $dependencyRoot `
        -Providers $allProviders
    Assert-Condition ($allLocal.MissingCount -eq 0) "All fixture providers should be available."
    Assert-Condition ([bool]$allLocal.FullyDisconnected) "All-local provider configuration should be disconnected."
    Assert-Condition ($allLocal.Arguments.Count -eq 7) "Expected one source argument per provider plus one mode argument."
    foreach ($provider in $allProviders) {
        $definition = $allLocal.ProviderStates | Where-Object { $_.Name -eq $provider }
        Assert-Condition ($null -ne $definition -and $definition.Available) "Provider state is missing for $provider."
        Assert-Argument -Arguments $allLocal.Arguments -Expected $definition.CMakeArgument
    }
    Assert-Argument -Arguments $allLocal.Arguments -Expected "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    Assert-Condition ((@($allLocal.Arguments | Where-Object { $_ -match '^FETCHCONTENT_SOURCE_DIR_' }).Count) -eq 0) `
        "Provider arguments must retain their -D prefix."
    Assert-Condition ((@($allLocal.Arguments | Where-Object { $_ -match '^-DFETCHCONTENT_SOURCE_DIR_' }).Count) -eq 6) `
        "Expected exactly one source argument per provider."

    Remove-Item -LiteralPath (Join-Path $dependencyRoot "stb-src\stb_vorbis.c") -Force
    $partial = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot $dependencyRoot `
        -Providers $allProviders
    Assert-Condition ($partial.MissingCount -eq 1) "One missing provider should be reported exactly once."
    Assert-Condition (-not [bool]$partial.FullyDisconnected) "Partial provider configuration must allow FetchContent fallback."
    Assert-Argument -Arguments $partial.Arguments -Expected "-DFETCHCONTENT_SOURCE_DIR_STB="
    Assert-Argument -Arguments $partial.Arguments -Expected "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF"

    $network = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot $dependencyRoot `
        -Providers @("ENET", "LUA") `
        -NoLocalProviders
    Assert-Condition ($network.MissingCount -eq 2) "No-local mode should treat every selected provider as unavailable."
    Assert-Condition (-not [bool]$network.FullyDisconnected) "No-local mode must not claim disconnected operation."
    Assert-Condition ($network.Arguments -contains "-DFETCHCONTENT_SOURCE_DIR_ENET=") "No-local ENet fallback is missing."
    Assert-Condition ($network.Arguments -contains "-DFETCHCONTENT_SOURCE_DIR_LUA=") "No-local Lua fallback is missing."

    $emptyRoot = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot "" `
        -Providers @("STB")
    Assert-Condition ($emptyRoot.MissingCount -eq 1) "An empty dependency root should use the network fallback contract."
    Assert-Condition (-not [bool]$emptyRoot.FullyDisconnected) "An empty dependency root must not claim disconnected operation."
    Assert-Condition ($emptyRoot.Arguments -contains "-DFETCHCONTENT_SOURCE_DIR_STB=") "Empty-root STB fallback is missing."

    $pins = Get-HenkaCMakeDependencyPins -RepositoryRoot $repoRoot
    Assert-Condition ($pins.Count -eq 6) "All six third-party sources must expose exact CMake commit pins."
    foreach ($provider in @("SDL3", "KTXSOFTWARE", "ENET", "LUA", "MINIAUDIO", "STB")) {
        Assert-Condition ($pins.ContainsKey($provider) -and $pins[$provider] -match "^[0-9a-f]{40}$") `
            "The pinned CMake revision was not resolved for $provider."
    }

    $pinProbe = Join-Path $fixtureRoot "pinned-source-probe"
    New-Item -ItemType Directory -Path $pinProbe -Force | Out-Null
    & $git -C $pinProbe init --quiet
    if ($LASTEXITCODE -ne 0) { throw "Could not initialize the pinned-source validation fixture." }
    & $git -C $pinProbe config core.autocrlf false
    & $git -C $pinProbe config user.name "Henka test"
    & $git -C $pinProbe config user.email "henka-test@example.invalid"
    [System.IO.File]::WriteAllText((Join-Path $pinProbe "source.txt"), "pinned baseline`n")
    & $git -C $pinProbe add -- source.txt
    & $git -C $pinProbe commit --quiet -m "baseline dependency source"
    if ($LASTEXITCODE -ne 0) { throw "Could not commit the pinned-source validation fixture." }
    $baselineRevision = ([string](& $git -C $pinProbe rev-parse HEAD)).Trim()
    Assert-Condition (Test-HenkaDependencySourceRevision -SourceRoot $pinProbe -ExpectedRevision $baselineRevision) `
        "An unchanged, clean pinned source was rejected."
    [System.IO.File]::AppendAllText((Join-Path $pinProbe "source.txt"), "dirty`n")
    Assert-Condition (-not (Test-HenkaDependencySourceRevision -SourceRoot $pinProbe -ExpectedRevision $baselineRevision)) `
        "A modified dependency source was incorrectly accepted as reusable."
    & $git -C $pinProbe add -- source.txt
    & $git -C $pinProbe commit --quiet -m "changed dependency source"
    if ($LASTEXITCODE -ne 0) { throw "Could not commit the changed-source negative control." }
    Assert-Condition (-not (Test-HenkaDependencySourceRevision -SourceRoot $pinProbe -ExpectedRevision $baselineRevision)) `
        "A changed pinned revision was incorrectly accepted as reusable."

    $selected = Select-HenkaDependencyRootCandidate -Candidates @(
        [pscustomobject]@{ Path = "build-local"; Mode = "build-local"; Priority = 0; AvailableProviders = 0 },
        [pscustomobject]@{ Path = "shared"; Mode = "shared"; Priority = 1; AvailableProviders = 6 }
    )
    Assert-Condition ($selected.Path -eq "shared") `
        "An empty build-local cache incorrectly displaced the complete pinned shared cache."
    $selected = Select-HenkaDependencyRootCandidate -Candidates @(
        [pscustomobject]@{ Path = "build-local"; Mode = "build-local"; Priority = 0; AvailableProviders = 6 },
        [pscustomobject]@{ Path = "shared"; Mode = "shared"; Priority = 1; AvailableProviders = 6 }
    )
    Assert-Condition ($selected.Path -eq "build-local") `
        "An equally complete build-local cache should be reused to preserve its configured build identity."
    $selected = Select-HenkaDependencyRootCandidate -Candidates @(
        [pscustomobject]@{ Path = "build-local"; Mode = "build-local"; Priority = 0; AvailableProviders = 0 },
        [pscustomobject]@{ Path = "shared"; Mode = "shared"; Priority = 1; AvailableProviders = 0 }
    )
    Assert-Condition ($null -eq $selected) `
        "The cache selector must leave CMake FetchContent fallback available when no compatible cache is available."

    $emptyBuildRoot = Join-Path $fixtureRoot "empty-build-root"
    New-Item -ItemType Directory -Path $emptyBuildRoot -Force | Out-Null
    $sharedRoot = Join-Path (Get-HenkaLocalRoot -RepositoryRoot $repoRoot) "dependencies"
    $resolved = Resolve-HenkaBuildDependencyRoot `
        -RepositoryRoot $repoRoot `
        -BuildRoot $emptyBuildRoot
    Assert-Condition ($resolved.Mode -eq "shared" -and
        [System.IO.Path]::GetFullPath($resolved.Root).TrimEnd('\') -ieq [System.IO.Path]::GetFullPath($sharedRoot).TrimEnd('\') -and
        [int]$resolved.AvailableProviders -eq 6) `
        "The real build resolver did not select and report the six-provider shared cache for an empty build-local cache."

    $explicit = Resolve-HenkaBuildDependencyRoot `
        -RepositoryRoot $repoRoot `
        -BuildRoot $emptyBuildRoot `
        -DependencyRoot $sharedRoot
    Assert-Condition ($explicit.Mode -eq "explicit" -and [int]$explicit.AvailableProviders -eq 6) `
        "An explicitly selected exact-pinned cache was not reported with its verified provider count."

    Write-Host "[pass] Dependency cache selection reuses exact-pinned sources, rejects changed inputs, and falls back when no compatible cache is available."
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Get-ChildItem -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue | ForEach-Object {
            $_.Attributes = [System.IO.FileAttributes]::Normal
        }
        (Get-Item -LiteralPath $fixtureRoot -Force).Attributes = [System.IO.FileAttributes]::Normal
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
exit 0
