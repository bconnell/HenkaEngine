Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$fixtureRoot = Join-Path $repoRoot ("build\test_tmp\cmake-fetch-content-fixture-" + [Guid]::NewGuid().ToString("N"))
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

    Write-Host "[pass] Shared FetchContent provider resolution reports availability, emits one argument per selected provider, and derives disconnected mode from complete local coverage."
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
