Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$git = Get-HenkaGitPath
$fixtureRoot = Join-Path (Get-HenkaTestTemporaryRoot -RepositoryRoot $repoRoot) ("cmake-fetch-content-fixture-" + [Guid]::NewGuid().ToString("N"))
$sharedRoot = Join-Path $fixtureRoot "shared"
$emptyBuildRoot = Join-Path $fixtureRoot "empty-build-deps"
$fullBuildRoot = Join-Path $fixtureRoot "full-build-deps"
$partialBuildRoot = Join-Path $fixtureRoot "partial-build-deps"
$resolverSharedRoot = Join-Path $fixtureRoot "resolver-shared"
$pinSourceRoot = Join-Path $fixtureRoot "pin-source"

function Assert-Condition {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Assert-Argument {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$Expected
    )

    Assert-Condition ($Arguments -contains $Expected) "Missing CMake argument: $Expected"
}

function Invoke-TestGit {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)

    $output = & $git @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Fixture Git command failed ($LASTEXITCODE): git $($Arguments -join ' ')`n$($output -join "`n")"
    }
    return [string]($output -join "`n").Trim()
}

function New-PinnedFixtureRepository {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$CacheDirectory,
        [Parameter(Mandatory = $true)][string]$Marker
    )

    $remoteRoot = Join-Path $fixtureRoot ("upstream-" + $Name.ToLowerInvariant())
    $markerPath = Join-Path $remoteRoot $Marker
    New-Item -ItemType Directory -Path $remoteRoot -Force | Out-Null
    [System.IO.File]::WriteAllText($markerPath, "fixture source`n")
    [System.IO.File]::WriteAllText((Join-Path $remoteRoot ".gitignore"), "ignored-cache-probe.txt`n")
    $null = Invoke-TestGit -Arguments @("-C", $remoteRoot, "init", "--quiet")
    $null = Invoke-TestGit -Arguments @("-C", $remoteRoot, "config", "core.autocrlf", "false")
    $null = Invoke-TestGit -Arguments @("-C", $remoteRoot, "config", "user.name", "Henka test")
    $null = Invoke-TestGit -Arguments @("-C", $remoteRoot, "config", "user.email", "henka-test@example.invalid")
    $null = Invoke-TestGit -Arguments @("-C", $remoteRoot, "add", "--", ".")
    $null = Invoke-TestGit -Arguments @("-C", $remoteRoot, "commit", "--quiet", "-m", "initial fixture source")
    $pin = Invoke-TestGit -Arguments @("-C", $remoteRoot, "rev-parse", "HEAD")

    $sharedSource = Join-Path $sharedRoot $CacheDirectory
    New-Item -ItemType Directory -Path $sharedRoot -Force | Out-Null
    $null = Invoke-TestGit -Arguments @("clone", "--quiet", "--local", $remoteRoot, $sharedSource)

    return [pscustomobject]@{
        Name = $Name
        CacheDirectory = $CacheDirectory
        Marker = $Marker
        RemoteRoot = $remoteRoot
        SharedSource = $sharedSource
        InitialPin = $pin
    }
}

function Write-PinSourceCMakeLists {
    param(
        [Parameter(Mandatory = $true)][object[]]$Definitions,
        [Parameter(Mandatory = $true)][hashtable]$Pins
    )

    $lines = @()
    foreach ($definition in $Definitions) {
        $repository = $definition.RemoteRoot.Replace("\", "/")
        $lines += "FetchContent_Declare("
        $lines += "    $($definition.Name)"
        $lines += "    GIT_REPOSITORY `"$repository`""
        $lines += "    GIT_TAG $($Pins[$definition.Name])"
        $lines += ")"
    }
    [System.IO.File]::WriteAllText((Join-Path $pinSourceRoot "CMakeLists.txt"), ($lines -join [Environment]::NewLine) + [Environment]::NewLine)
}

function Copy-FixtureProviders {
    param(
        [Parameter(Mandatory = $true)][string]$DestinationRoot,
        [Parameter(Mandatory = $true)][object[]]$Definitions,
        [string[]]$Names = @()
    )

    New-Item -ItemType Directory -Path $DestinationRoot -Force | Out-Null
    foreach ($definition in $Definitions) {
        if ($Names.Count -gt 0 -and $Names -notcontains $definition.Name) { continue }
        $destination = Join-Path $DestinationRoot $definition.CacheDirectory
        $null = Invoke-TestGit -Arguments @("clone", "--quiet", "--local", $definition.RemoteRoot, $destination)
    }
}

try {
    New-Item -ItemType Directory -Path $pinSourceRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $emptyBuildRoot -Force | Out-Null

    $definitions = @(
        [pscustomobject]@{ Name = "SDL3"; CacheDirectory = "sdl3-src"; Marker = "CMakeLists.txt" },
        [pscustomobject]@{ Name = "KTXSoftware"; CacheDirectory = "ktxsoftware-src"; Marker = "CMakeLists.txt" },
        [pscustomobject]@{ Name = "ENet"; CacheDirectory = "enet-src"; Marker = "CMakeLists.txt" },
        [pscustomobject]@{ Name = "Lua"; CacheDirectory = "lua-src"; Marker = "lua.h" },
        [pscustomobject]@{ Name = "miniaudio"; CacheDirectory = "miniaudio-src"; Marker = "miniaudio.h" },
        [pscustomobject]@{ Name = "stb"; CacheDirectory = "stb-src"; Marker = "stb_vorbis.c" }
    )

    $fixtureProviders = @()
    foreach ($definition in $definitions) {
        $fixtureProviders += New-PinnedFixtureRepository `
            -Name $definition.Name `
            -CacheDirectory $definition.CacheDirectory `
            -Marker $definition.Marker
    }

    $pins = @{}
    foreach ($provider in $fixtureProviders) { $pins[$provider.Name] = $provider.InitialPin }
    Write-PinSourceCMakeLists -Definitions $fixtureProviders -Pins $pins
    $allProviders = @("SDL3", "KTXSOFTWARE", "ENET", "LUA", "MINIAUDIO", "STB")

    $sharedReuse = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot $emptyBuildRoot `
        -SharedDependencyRoot $sharedRoot `
        -PinSourceRoot $pinSourceRoot `
        -Providers $allProviders
    Assert-Condition ($sharedReuse.MissingCount -eq 0) ("An empty build-local root should reuse all six verified shared sources (missed {0})." -f $sharedReuse.MissingCount)
    Assert-Condition ([bool]$sharedReuse.FullyDisconnected) "Complete verified shared-source coverage should disable network fallback."
    Assert-Condition ($sharedReuse.Arguments.Count -eq 7) "Expected one source argument per provider plus one mode argument."
    foreach ($provider in $allProviders) {
        $state = $sharedReuse.ProviderStates | Where-Object { $_.Name -eq $provider }
        Assert-Condition ($null -ne $state -and $state.Available) "Provider state is missing for $provider."
        Assert-Condition ($state.SourceScope -eq "shared-cache") "$provider should be selected from the shared source cache."
        Assert-Condition ($state.PinnedCommit -eq $pins[$state.CMakeName]) "$provider did not report the pinned source commit."
        Assert-Argument -Arguments $sharedReuse.Arguments -Expected $state.CMakeArgument
    }
    Assert-Argument -Arguments $sharedReuse.Arguments -Expected "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    Assert-Condition ((@($sharedReuse.Arguments | Where-Object { $_ -match '^-DFETCHCONTENT_SOURCE_DIR_' }).Count) -eq 6) `
        "Expected exactly one source argument per provider."

    Copy-FixtureProviders -DestinationRoot $fullBuildRoot -Definitions $fixtureProviders
    $localPriority = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot $fullBuildRoot `
        -SharedDependencyRoot $sharedRoot `
        -PinSourceRoot $pinSourceRoot `
        -Providers $allProviders
    Assert-Condition ($localPriority.MissingCount -eq 0) "Valid build-local providers should remain usable."
    foreach ($state in $localPriority.ProviderStates) {
        Assert-Condition ($state.SourceScope -eq "build-local") "$($state.Name) should prefer its valid build-local source."
    }

    $stb = $fixtureProviders | Where-Object { $_.Name -eq "stb" }
    $stbSharedSource = Join-Path $sharedRoot $stb.CacheDirectory
    $wrongOrigin = $stb.RemoteRoot + "-unexpected-origin"
    $null = Invoke-TestGit -Arguments @("-C", $stbSharedSource, "remote", "set-url", "origin", $wrongOrigin)
    $wrongOriginResult = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot $emptyBuildRoot `
        -SharedDependencyRoot $sharedRoot `
        -PinSourceRoot $pinSourceRoot `
        -Providers $allProviders
    $wrongOriginState = $wrongOriginResult.ProviderStates | Where-Object { $_.Name -eq "STB" }
    Assert-Condition ($wrongOriginResult.MissingCount -eq 1 -and
        $wrongOriginState.RejectionReason -eq "source-origin-does-not-match-pin-source") `
        "A source at the pinned commit with a different origin must not be reused."
    $null = Invoke-TestGit -Arguments @("-C", $stbSharedSource, "remote", "set-url", "origin", $stb.RemoteRoot)

    [System.IO.File]::WriteAllText((Join-Path $stbSharedSource "ignored-cache-probe.txt"), "preserve ignored data`n")
    $ignoredInput = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot $emptyBuildRoot `
        -SharedDependencyRoot $sharedRoot `
        -PinSourceRoot $pinSourceRoot `
        -Providers $allProviders
    $ignoredState = $ignoredInput.ProviderStates | Where-Object { $_.Name -eq "STB" }
    Assert-Condition ($ignoredInput.MissingCount -eq 1 -and
        $ignoredState.RejectionReason -eq "source-working-tree-is-not-clean") `
        "An ignored file inside a shared source must prevent cache reuse."
    Remove-Item -LiteralPath (Join-Path $stbSharedSource "ignored-cache-probe.txt") -Force

    Copy-FixtureProviders -DestinationRoot $partialBuildRoot -Definitions $fixtureProviders -Names @("SDL3", "KTXSoftware", "ENet", "Lua", "miniaudio")
    $localRoot = Get-HenkaLocalRoot -RepositoryRoot $repoRoot
    $invalidUnusedSharedRoot = [System.IO.Path]::GetFullPath(
        (Join-Path $localRoot "..\invalid-unused-fetchcontent-cache"))
    $invalidSharedRootRejected = $false
    try {
        $null = Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $invalidUnusedSharedRoot
    } catch {
        $invalidSharedRootRejected = $_.Exception.Message -match "outside the canonical project-owned _local root"
    }
    Assert-Condition $invalidSharedRootRejected `
        "The no-shared-cache regression requires a path that the local-root authority rejects."
    $explicitOnly = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot $partialBuildRoot `
        -SharedDependencyRoot $invalidUnusedSharedRoot `
        -PinSourceRoot $pinSourceRoot `
        -NoSharedDependencyCache `
        -Providers @("SDL3", "KTXSOFTWARE", "ENET", "LUA", "MINIAUDIO", "STB")
    Assert-Condition ($explicitOnly.MissingCount -eq 1) "An explicit dependency root must not silently use the shared cache."
    Assert-Condition (-not [bool]$explicitOnly.FullyDisconnected) "An incomplete explicit dependency root must retain network fallback."
    Assert-Argument -Arguments $explicitOnly.Arguments -Expected "-DFETCHCONTENT_SOURCE_DIR_STB="
    Assert-Argument -Arguments $explicitOnly.Arguments -Expected "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF"

    $network = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot $fullBuildRoot `
        -SharedDependencyRoot $invalidUnusedSharedRoot `
        -PinSourceRoot $pinSourceRoot `
        -NoLocalProviders `
        -Providers @("ENET", "LUA")
    Assert-Condition ($network.MissingCount -eq 2) "No-local mode must disable build-local and shared sources."
    Assert-Condition (-not [bool]$network.FullyDisconnected) "No-local mode must allow normal FetchContent fallback."
    Assert-Argument -Arguments $network.Arguments -Expected "-DFETCHCONTENT_SOURCE_DIR_ENET="
    Assert-Argument -Arguments $network.Arguments -Expected "-DFETCHCONTENT_SOURCE_DIR_LUA="

    [System.IO.File]::AppendAllText((Join-Path $stb.RemoteRoot $stb.Marker), "/* changed pinned input */`n")
    $null = Invoke-TestGit -Arguments @("-C", $stb.RemoteRoot, "add", "--", $stb.Marker)
    $null = Invoke-TestGit -Arguments @("-C", $stb.RemoteRoot, "commit", "--quiet", "-m", "changed pinned source")
    $changedPin = Invoke-TestGit -Arguments @("-C", $stb.RemoteRoot, "rev-parse", "HEAD")
    Assert-Condition ($changedPin -ne $stb.InitialPin) "The invalidation control must use a genuinely changed source commit."
    $pins.stb = $changedPin
    Write-PinSourceCMakeLists -Definitions $fixtureProviders -Pins $pins

    $repositoryPins = Get-HenkaCMakeDependencyPins -RepositoryRoot $repoRoot
    Assert-Condition ($repositoryPins.Count -eq 6) "All six third-party sources must expose exact CMake commit pins."
    foreach ($provider in @("SDL3", "KTXSOFTWARE", "ENET", "LUA", "MINIAUDIO", "STB")) {
        Assert-Condition ($repositoryPins.ContainsKey($provider) -and $repositoryPins[$provider] -match "^[0-9a-f]{40}$") `
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

    $resolverEmptyBuildRoot = Join-Path $fixtureRoot "empty-build-root"
    New-Item -ItemType Directory -Path $resolverEmptyBuildRoot -Force | Out-Null
    # Earlier negative controls intentionally mutate the primary shared fixture.
    # Give the resolver integration checks an independent, clean six-provider cache.
    Copy-FixtureProviders -DestinationRoot $resolverSharedRoot -Definitions $fixtureProviders
    $repositorySharedRoot = Get-HenkaCMakeSharedDependencyRoot -RepositoryRoot $repoRoot
    $expectedSharedRoot = Resolve-HenkaLocalPath `
        -RepositoryRoot $repoRoot `
        -Path (Join-Path (Join-Path (Get-HenkaLocalRoot -RepositoryRoot $repoRoot) "dependencies") "fetchcontent")
    Assert-Condition ([System.IO.Path]::GetFullPath($repositorySharedRoot).Equals(
        [System.IO.Path]::GetFullPath($expectedSharedRoot),
        [System.StringComparison]::OrdinalIgnoreCase)) `
        "The default shared source cache must remain under Henka's canonical _local dependency root."

    $resolved = Resolve-HenkaBuildDependencyRoot `
        -RepositoryRoot $repoRoot `
        -BuildRoot $resolverEmptyBuildRoot `
        -SharedDependencyRoot $resolverSharedRoot `
        -PinSourceRoot $pinSourceRoot
    $sharedResolution = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot "" `
        -SharedDependencyRoot $resolverSharedRoot `
        -PinnedRevisions (Get-HenkaCMakeDependencyPins -RepositoryRoot $pinSourceRoot) `
        -PinSourceRoot $pinSourceRoot
    $sharedProviderSummary = @($sharedResolution.ProviderStates | ForEach-Object {
        "{0}={1}({2})" -f $_.Name,$_.Available,$_.RejectionReason
    }) -join "; "
    Assert-Condition ($resolved.Mode -eq "shared" -and
        [string]::IsNullOrWhiteSpace($resolved.Root) -and
        [int]$resolved.AvailableProviders -eq 6) `
        ("The real build resolver did not select and report the six-provider shared cache for an empty build-local cache. " +
            "Actual mode='$($resolved.Mode)', root='$($resolved.Root)', provider count=$($resolved.AvailableProviders); " +
            "shared providers: $sharedProviderSummary")

    $explicit = Resolve-HenkaBuildDependencyRoot `
        -RepositoryRoot $repoRoot `
        -BuildRoot $resolverEmptyBuildRoot `
        -DependencyRoot $resolverSharedRoot `
        -PinSourceRoot $pinSourceRoot
    Assert-Condition ($explicit.Mode -eq "explicit" -and [int]$explicit.AvailableProviders -eq 6) `
        "An explicitly selected exact-pinned fixture cache was not reported with its verified provider count."

    $outsidePinSourceRejected = $false
    try {
        $outsideLocalRoot = Split-Path -Parent (Get-HenkaLocalRoot -RepositoryRoot $repoRoot)
        $null = Resolve-HenkaBuildDependencyRoot `
            -RepositoryRoot $repoRoot `
            -BuildRoot $resolverEmptyBuildRoot `
            -PinSourceRoot $outsideLocalRoot
    } catch {
        $outsidePinSourceRejected = $true
    }
    Assert-Condition $outsidePinSourceRejected `
        "A test pin-source override outside canonical _local must be rejected."

    $actualDefaultResolution = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot "" `
        -PinnedRevisions (Get-HenkaCMakeDependencyPins -RepositoryRoot $repoRoot)
    $actualDefaultCount = @($actualDefaultResolution.ProviderStates | Where-Object { $_.Available }).Count
    $actualDefaultMode = if ($actualDefaultCount -gt 0) { "shared" } else { "network-fallback" }
    $actualDefault = Resolve-HenkaBuildDependencyRoot `
        -RepositoryRoot $repoRoot `
        -BuildRoot $resolverEmptyBuildRoot
    Assert-Condition ($actualDefault.Mode -eq $actualDefaultMode -and
        [int]$actualDefault.AvailableProviders -eq $actualDefaultCount) `
        "The default resolver must report the actual verified machine cache or truthful FetchContent fallback."

    Write-Host "[pass] Dependency cache selection reuses exact-pinned sources, rejects changed inputs, and falls back when no compatible cache is available."
    $changedInput = Get-HenkaCMakeFetchContentArguments `
        -DependencyRoot $emptyBuildRoot `
        -SharedDependencyRoot $sharedRoot `
        -PinSourceRoot $pinSourceRoot `
        -Providers $allProviders
    Assert-Condition ($changedInput.MissingCount -eq 1) "A changed source pin should invalidate only its provider."
    Assert-Condition (-not [bool]$changedInput.FullyDisconnected) "One changed provider must retain network fallback."
    foreach ($state in $changedInput.ProviderStates) {
        if ($state.Name -eq "STB") {
            Assert-Condition (-not $state.Available -and $state.SourceScope -eq "none") "The stale STB source must not be reused."
            Assert-Argument -Arguments $changedInput.Arguments -Expected "-DFETCHCONTENT_SOURCE_DIR_STB="
        } else {
            Assert-Condition ($state.Available -and $state.SourceScope -eq "shared-cache") "$($state.Name) should remain reusable after the STB pin changes."
        }
    }
    Assert-Argument -Arguments $changedInput.Arguments -Expected "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF"

    Write-Host "[pass] Empty build roots reuse six clean pinned shared sources; local precedence, explicit-root isolation, no-local mode, and changed-pin invalidation are executable regressions."
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
