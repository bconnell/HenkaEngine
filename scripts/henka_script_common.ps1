Set-StrictMode -Version Latest
if ($null -eq (Get-Variable -Name HenkaCommonScriptDirectory -Scope Script -ErrorAction SilentlyContinue)) {
    $script:HenkaCommonScriptDirectory = $PSScriptRoot
}

function Get-HenkaRepoRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptDirectory
    )

    return (Resolve-Path (Join-Path $ScriptDirectory "..")).Path
}

function Get-HenkaCanonicalRepositoryRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot
    )

    $repository = [System.IO.Path]::GetFullPath($RepositoryRoot).TrimEnd("\", "/")
    if (-not (Test-Path -LiteralPath $repository -PathType Container)) {
        throw "Henka repository root does not exist: $repository"
    }
    $git = Get-HenkaGitPath
    $commonDirectoryText = [string](& $git -C $repository rev-parse --git-common-dir 2>$null)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($commonDirectoryText)) {
        throw "Could not resolve the Henka repository's shared Git directory: $repository"
    }
    $commonDirectory = $commonDirectoryText.Trim()
    if (-not [System.IO.Path]::IsPathRooted($commonDirectory)) {
        $commonDirectory = Join-Path $repository $commonDirectory
    }
    $commonDirectory = [System.IO.Path]::GetFullPath($commonDirectory).TrimEnd("\", "/")
    if (-not [string]::Equals((Split-Path -Leaf $commonDirectory), ".git", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Henka storage placement requires a non-bare repository with a .git common directory: $commonDirectory"
    }
    return [System.IO.Path]::GetFullPath((Split-Path -Parent $commonDirectory)).TrimEnd("\", "/")
}

function Get-HenkaLocalRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot
    )

    $repository = [System.IO.Path]::GetFullPath($RepositoryRoot).TrimEnd("\", "/")
    if (-not (Test-Path -LiteralPath $repository -PathType Container)) {
        throw "Henka repository root does not exist: $repository"
    }

    $helperRepository = Get-HenkaRepoRoot -ScriptDirectory $script:HenkaCommonScriptDirectory
    $canonicalRepository = Get-HenkaCanonicalRepositoryRoot -RepositoryRoot $helperRepository
    $projectRoot = Split-Path -Parent $canonicalRepository
    if ([string]::IsNullOrWhiteSpace($projectRoot)) {
        throw "Could not determine the Henka project-owned storage parent from $canonicalRepository"
    }
    $localRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "_local")).TrimEnd("\", "/")

    $requestedCanonicalRepository = $null
    try {
        $requestedCanonicalRepository = Get-HenkaCanonicalRepositoryRoot -RepositoryRoot $repository
    }
    catch {
        $requestedCanonicalRepository = $null
    }

    $repositoryIsInsideLocalRoot = [string]::Equals(
        $repository,
        $localRoot,
        [System.StringComparison]::OrdinalIgnoreCase) -or
        $repository.StartsWith(
            $localRoot + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase)
    $repositorySharesHenkaGitAuthority = $null -ne $requestedCanonicalRepository -and
        [string]::Equals(
            [System.IO.Path]::GetFullPath($requestedCanonicalRepository).TrimEnd("\", "/"),
            [System.IO.Path]::GetFullPath($canonicalRepository).TrimEnd("\", "/"),
            [System.StringComparison]::OrdinalIgnoreCase)

    if (-not $repositoryIsInsideLocalRoot -and -not $repositorySharesHenkaGitAuthority) {
        throw "Repository does not share Henka's canonical Git authority: $repository"
    }

    return $localRoot
}

function Resolve-HenkaLocalPath {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "Henka generated path cannot be empty."
    }

    $localRoot = [System.IO.Path]::GetFullPath((Get-HenkaLocalRoot -RepositoryRoot $RepositoryRoot)).TrimEnd("\", "/")
    $fullPath = if ([System.IO.Path]::IsPathRooted($Path)) {
        [System.IO.Path]::GetFullPath($Path)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $localRoot $Path))
    }
    $fullPath = $fullPath.TrimEnd("\", "/")
    $insideRoot = [string]::Equals($fullPath, $localRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
        $fullPath.StartsWith($localRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)
    if (-not $insideRoot) {
        throw "Generated Henka path is outside the canonical project-owned _local root: $fullPath"
    }

    $relative = $fullPath.Substring($localRoot.Length).TrimStart("\", "/")
    $current = $localRoot
    $rootItem = Get-Item -LiteralPath $current -Force -ErrorAction SilentlyContinue
    if ($null -ne $rootItem) {
        if (-not $rootItem.PSIsContainer -or
            (($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0)) {
            throw "Canonical Henka _local root is not a normal directory: $current"
        }
    }
    foreach ($part in @($relative -split "[\\/]" | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })) {
        $current = Join-Path $current $part
        $item = Get-Item -LiteralPath $current -Force -ErrorAction SilentlyContinue
        if ($null -eq $item) {
            break
        }
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Generated Henka path crosses a reparse point: $current"
        }
        if (-not $item.PSIsContainer -and $current -ne $fullPath) {
            throw "Generated Henka path crosses a file instead of a directory: $current"
        }
    }
    return $fullPath
}

function Resolve-HenkaDependencyRoot {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$DependencyRoot
    )

    $resolvedRoot = Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot -Path $DependencyRoot
    if (-not (Test-Path -LiteralPath $resolvedRoot -PathType Container)) {
        throw "Henka dependency root was not found beneath canonical _local: $resolvedRoot"
    }
    return $resolvedRoot
}

function Get-HenkaCheckoutStorageKey {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [switch]$CompactBuildKey
    )

    $repository = [System.IO.Path]::GetFullPath($RepositoryRoot).TrimEnd("\", "/")
    $leaf = (Split-Path -Leaf $repository) -replace "[^A-Za-z0-9._-]", "-"
    if ([string]::IsNullOrWhiteSpace($leaf)) { $leaf = "checkout" }
    $hashAlgorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hashBytes = $hashAlgorithm.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($repository.ToLowerInvariant()))
        $hashText = ([BitConverter]::ToString($hashBytes)).Replace("-", "").ToLowerInvariant()
    }
    finally { $hashAlgorithm.Dispose() }
    if ($CompactBuildKey) {
        $leaf = $leaf.Substring(0, [Math]::Min(4, $leaf.Length))
        $hashText = $hashText.Substring(0, 16)
    }
    else {
        $hashText = $hashText.Substring(0, 12)
    }
    return "$leaf-$hashText"
}

function Get-HenkaBuildRoot {
    param([Parameter(Mandatory = $true)][string]$RepositoryRoot)

    $repository = [System.IO.Path]::GetFullPath($RepositoryRoot).TrimEnd("\", "/")
    $localRoot = Get-HenkaLocalRoot -RepositoryRoot $repository
    $buildKey = Get-HenkaCheckoutStorageKey -RepositoryRoot $repository -CompactBuildKey
    return Resolve-HenkaLocalPath -RepositoryRoot $repository `
        -Path (Join-Path (Join-Path $localRoot "builds") $buildKey)
}

function Get-HenkaPackageRoot {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [string]$PackageName = "HenkaSandbox3D"
    )

    if ([string]::IsNullOrWhiteSpace($PackageName) -or $PackageName -match "[\\/:*?`"<>|]" -or $PackageName -in @(".", "..")) {
        throw "Henka package name is not a valid single path segment: $PackageName"
    }
    $localRoot = Get-HenkaLocalRoot -RepositoryRoot $RepositoryRoot
    return Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot `
        -Path (Join-Path (Join-Path (Join-Path $localRoot "packages") (Get-HenkaCheckoutStorageKey -RepositoryRoot $RepositoryRoot)) $PackageName)
}

function Get-HenkaWorktreeRoot {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][ValidatePattern("^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")][string]$Name
    )

    if ($Name -in @(".", "..")) {
        throw "Henka worktree name must be a single safe path segment: $Name"
    }
    $localRoot = Get-HenkaLocalRoot -RepositoryRoot $RepositoryRoot
    return Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot -Path (Join-Path (Join-Path $localRoot "worktrees") $Name)
}

function Get-HenkaExactCandidateRoot {
    param([Parameter(Mandatory = $true)][string]$RepositoryRoot)
    $localRoot = Get-HenkaLocalRoot -RepositoryRoot $RepositoryRoot
    return Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot -Path (Join-Path $localRoot "exact-candidates")
}

function Get-HenkaEvidenceRoot {
    param([Parameter(Mandatory = $true)][string]$RepositoryRoot)
    $localRoot = Get-HenkaLocalRoot -RepositoryRoot $RepositoryRoot
    return Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot `
        -Path (Join-Path (Join-Path $localRoot "evidence") (Get-HenkaCheckoutStorageKey -RepositoryRoot $RepositoryRoot))
}

function Get-HenkaTestTemporaryRoot {
    param([Parameter(Mandatory = $true)][string]$RepositoryRoot)
    return Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot -Path (Join-Path (Get-HenkaBuildRoot -RepositoryRoot $RepositoryRoot) "test_tmp")
}

function New-HenkaTemporaryDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][ValidatePattern("^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")][string]$Purpose
    )

    $localRoot = Get-HenkaLocalRoot -RepositoryRoot $RepositoryRoot
    $path = Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot `
        -Path (Join-Path (Join-Path $localRoot "temporary") ($Purpose + "-" + [Guid]::NewGuid().ToString("N")))
    $null = New-HenkaLocalDirectory -RepositoryRoot $RepositoryRoot -Path $path
    $null = Write-HenkaGeneratedRootMarker -RepoRoot $RepositoryRoot -Path $path `
        -Purpose $Purpose -RetentionClass "SCRATCH" -Active $true -CleanupEligible $true `
        -CleanupCondition "the owning command completed or was interrupted and active consumers were checked"
    return $path
}

function New-HenkaLocalDirectory {
    param(
        [Parameter(Mandatory = $true)][string]$RepositoryRoot,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $fullPath = Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot -Path $Path
    [System.IO.Directory]::CreateDirectory($fullPath) | Out-Null
    $fullPath = Resolve-HenkaLocalPath -RepositoryRoot $RepositoryRoot -Path $fullPath
    return $fullPath
}

function Resolve-HenkaRepositoryPath {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "Henka repository path cannot be empty."
    }

    $root = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd('\', '/')
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    $resolved = [System.IO.Path]::GetFullPath((Join-Path $root $Path))
    $rootPrefix = $root + [System.IO.Path]::DirectorySeparatorChar
    $insideRoot = $resolved.Equals($root, [System.StringComparison]::OrdinalIgnoreCase) -or
        $resolved.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)
    if (-not $insideRoot) {
        throw "Relative Henka repository path escapes the repository root: $Path"
    }

    return $resolved
}

function Write-HenkaGeneratedRootMarker {
    param(
        [Parameter(Mandatory = $true)] [string]$RepoRoot,
        [Parameter(Mandatory = $true)] [string]$Path,
        [Parameter(Mandatory = $true)] [string]$Purpose,
        [Parameter(Mandatory = $true)] [ValidateSet("SCRATCH", "ACTIVE_CANDIDATE", "PUBLISHED_BOUNDARY_EVIDENCE", "NEGATIVE_CONTROL", "CACHE", "PROOF_CONSUMED", "SUPERSEDED", "REBUILDABLE")]
        [string]$RetentionClass,
        [Parameter(Mandatory = $true)] [bool]$Active,
        [Parameter(Mandatory = $true)] [bool]$CleanupEligible,
        [Parameter(Mandatory = $true)] [string]$CleanupCondition,
        [string]$Configuration = "n/a"
    )

    $repo = (Resolve-Path -LiteralPath $RepoRoot).Path.TrimEnd("\")
    $fullPath = Resolve-HenkaLocalPath -RepositoryRoot $repo -Path $Path
    [System.IO.Directory]::CreateDirectory($fullPath) | Out-Null
    $fullPath = Resolve-HenkaLocalPath -RepositoryRoot $repo -Path $fullPath
    $item = Get-Item -LiteralPath $fullPath -Force
    if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Generated root marker path is a reparse point: $fullPath"
    }
    $git = Get-HenkaGitPath
    $sourceSha = ([string](& $git -C $repo rev-parse HEAD 2>$null)).Trim()
    if ($LASTEXITCODE -ne 0 -or $sourceSha -notmatch "^[0-9a-fA-F]{40}$") {
        throw "Could not resolve the source commit for generated root marker: $fullPath"
    }
    $marker = [ordered]@{
        schema_version = 1
        owner = "Henka repository validation"
        purpose = $Purpose
        source_sha = $sourceSha
        configuration = $Configuration
        created_utc = [DateTime]::UtcNow.ToString("o")
        retention_class = $RetentionClass
        cleanup_owner = "Henka repository validation"
        cleanup_condition = $CleanupCondition
        active = $Active
        cleanup_eligible = $CleanupEligible
    }
    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText(
        (Join-Path $fullPath ".henka-generated.json"),
        (($marker | ConvertTo-Json -Depth 4) + [Environment]::NewLine),
        $encoding)
    return (Join-Path $fullPath ".henka-generated.json")
}

function Get-HenkaToolVersionLine {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [ValidateSet("cmake", "ctest")]
        [string]$ToolName
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$ToolName executable does not exist: $Path"
    }
    $output = @(& $Path --version 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "$ToolName did not report a valid version: $Path"
    }
    $prefix = "$ToolName version "
    $versionLine = @($output | ForEach-Object { [string]$_ } |
        Where-Object { $_ -like "$prefix*" } | Select-Object -First 1)
    if ($versionLine.Count -ne 1 -or [string]::IsNullOrWhiteSpace($versionLine[0])) {
        throw "$ToolName reported an unrecognized version: $Path"
    }
    return $versionLine[0].Trim()
}

function Get-HenkaToolchain {
    param(
        [string]$CMakePath = ""
    )

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($CMakePath)) {
        $candidates += [System.IO.Path]::GetFullPath($CMakePath)
    }
    else {
        $cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
        if ($null -eq $cmakeCommand) {
            $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
        }
        if ($null -ne $cmakeCommand -and
            -not [string]::IsNullOrWhiteSpace([string]$cmakeCommand.Source)) {
            $candidates += [System.IO.Path]::GetFullPath([string]$cmakeCommand.Source)
        }

        $vswhereCandidates = @(
            (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"),
            (Join-Path $env:ProgramFiles "Microsoft Visual Studio\Installer\vswhere.exe")
        )
        foreach ($vswhere in $vswhereCandidates) {
            if ([string]::IsNullOrWhiteSpace($vswhere) -or
                -not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
                continue
            }
            $arguments = @(
                "-latest",
                "-products", "*",
                "-requires", "Microsoft.VisualStudio.Component.VC.CMake.Project",
                "-property", "installationPath"
            )
            $installationPaths = @(& $vswhere @arguments)
            if ($LASTEXITCODE -ne 0) {
                continue
            }
            foreach ($installationPath in $installationPaths) {
                if (-not [string]::IsNullOrWhiteSpace([string]$installationPath)) {
                    $candidates += Join-Path ([string]$installationPath).Trim() `
                        "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
                }
            }
        }

        $candidates += @(
            "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
            "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
            "C:\Program Files\CMake\bin\cmake.exe"
        )
    }

    $failures = @()
    foreach ($candidate in @($candidates | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_) } |
            ForEach-Object { [System.IO.Path]::GetFullPath([string]$_) } | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            $failures += "$candidate (missing)"
            continue
        }
        $ctest = Join-Path (Split-Path -Parent $candidate) "ctest.exe"
        if (-not (Test-Path -LiteralPath $ctest -PathType Leaf)) {
            $failures += "$candidate (paired ctest.exe missing)"
            continue
        }
        try {
            $cmakeVersion = Get-HenkaToolVersionLine -Path $candidate -ToolName "cmake"
            $ctestVersion = Get-HenkaToolVersionLine -Path $ctest -ToolName "ctest"
            $cmakeValue = $cmakeVersion.Substring("cmake version ".Length)
            $ctestValue = $ctestVersion.Substring("ctest version ".Length)
            if ($cmakeValue -ne $ctestValue) {
                throw "CMake/CTest version mismatch ($cmakeValue versus $ctestValue)."
            }
            return [pscustomobject]@{
                CMakePath = $candidate
                CTestPath = $ctest
                CMakeVersion = $cmakeVersion
                CTestVersion = $ctestVersion
            }
        }
        catch {
            $failures += "$candidate ($($_.Exception.Message))"
        }
    }

    $details = if ($failures.Count -eq 0) { "no supported candidates were found" } else { $failures -join "; " }
    throw "Henka could not resolve a compatible CMake/CTest pair: $details"
}

function Get-HenkaCMakePath {
    return (Get-HenkaToolchain).CMakePath
}

function Get-HenkaCTestPath {
    param(
        [string]$CMakePath = ""
    )

    if ([string]::IsNullOrWhiteSpace($CMakePath)) {
        return (Get-HenkaToolchain).CTestPath
    }
    return (Get-HenkaToolchain -CMakePath $CMakePath).CTestPath
}

function Get-HenkaBuildArtifact {
    param(
        [Parameter(Mandatory = $true)] [string]$BuildRoot,
        [Parameter(Mandatory = $true)] [ValidateSet("Debug", "Release")][string]$Configuration,
        [string]$BuildTarget = ""
    )

    $target = [string]$BuildTarget
    if ([string]::IsNullOrWhiteSpace($target) -or $target -eq "henka_sandbox3d") {
        return [pscustomobject]@{
            Path = Join-Path $BuildRoot "examples\sandbox3d\$Configuration\henka_sandbox3d.exe"
            Kind = "executable"
            Target = if ([string]::IsNullOrWhiteSpace($target)) { "default" } else { $target }
        }
    }
    if ($target -eq "henka" -or $target -eq "henka_runtime") {
        return [pscustomobject]@{
            Path = Join-Path $BuildRoot "engine\$Configuration\$target.lib"
            Kind = "static-library"
            Target = $target
        }
    }
    if ($target.EndsWith("_tests", [System.StringComparison]::OrdinalIgnoreCase)) {
        return [pscustomobject]@{
            Path = Join-Path $BuildRoot "tests\$Configuration\$target.exe"
            Kind = "executable"
            Target = $target
        }
    }
    throw "No target-aware provenance artifact mapping exists for CMake target '$target'. Supply a supported target or extend the explicit mapping."
}

function Enter-HenkaBuildStateLock {
    param(
        [ValidateRange(0, 3600)]
        [int]$TimeoutSeconds = 900
    )

    $mutexName = "Local\HenkaEngineSharedGeneratedBuildState"
    $mutex = New-Object System.Threading.Mutex($false, $mutexName)
    $acquired = $false
    try {
        try {
            $acquired = $mutex.WaitOne([TimeSpan]::FromSeconds($TimeoutSeconds))
        } catch [System.Threading.AbandonedMutexException] {
            $acquired = $true
            Write-Warning "Recovered an abandoned shared generated build-state lock."
        }
        if (-not $acquired) {
            throw "Could not acquire the shared generated build-state lock within $TimeoutSeconds seconds. Another Henka build/configure operation may be using the shared generated tree."
        }
        return [pscustomobject]@{
            Mutex = $mutex
            Name = $mutexName
        }
    } catch {
        $mutex.Dispose()
        throw
    }
}

function Exit-HenkaBuildStateLock {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Lock
    )

    try {
        $Lock.Mutex.ReleaseMutex()
    } finally {
        $Lock.Mutex.Dispose()
    }
}

function Get-HenkaCMakeFetchContentArguments {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$DependencyRoot,

        [ValidateSet("SDL3", "KTXSOFTWARE", "ENET", "LUA", "MINIAUDIO", "STB")]
        [string[]]$Providers = @("SDL3", "KTXSOFTWARE", "ENET", "LUA", "MINIAUDIO", "STB"),

        [switch]$NoLocalProviders
    )

    $definitions = @{
        SDL3 = [pscustomobject]@{
            CacheName = "SDL3"
            RelativePath = "sdl3-src"
            Marker = "CMakeLists.txt"
            Label = "SDL3"
        }
        KTXSOFTWARE = [pscustomobject]@{
            CacheName = "KTXSOFTWARE"
            RelativePath = "ktxsoftware-src"
            Marker = "CMakeLists.txt"
            Label = "KTX-Software"
        }
        ENET = [pscustomobject]@{
            CacheName = "ENET"
            RelativePath = "enet-src"
            Marker = "CMakeLists.txt"
            Label = "ENet"
        }
        LUA = [pscustomobject]@{
            CacheName = "LUA"
            RelativePath = "lua-src"
            Marker = "lua.h"
            Label = "Lua"
        }
        MINIAUDIO = [pscustomobject]@{
            CacheName = "MINIAUDIO"
            RelativePath = "miniaudio-src"
            Marker = "miniaudio.h"
            Label = "miniaudio"
        }
        STB = [pscustomobject]@{
            CacheName = "STB"
            RelativePath = "stb-src"
            Marker = "stb_vorbis.c"
            Label = "stb"
        }
    }

    $arguments = @()
    $states = @()
    $missingCount = 0

    foreach ($provider in @($Providers)) {
        if (-not $definitions.ContainsKey($provider)) {
            throw "Unknown Henka FetchContent provider: $provider"
        }

        $definition = $definitions[$provider]
        $sourceRoot = if ([string]::IsNullOrWhiteSpace($DependencyRoot)) {
            $definition.RelativePath
        } else {
            Join-Path $DependencyRoot $definition.RelativePath
        }
        $markerPath = Join-Path $sourceRoot $definition.Marker
        $available = (-not $NoLocalProviders) -and
            (Test-Path -LiteralPath $markerPath -PathType Leaf)
        $sourceValue = if ($available) { [System.IO.Path]::GetFullPath($sourceRoot) } else { "" }
        $arguments += "-DFETCHCONTENT_SOURCE_DIR_$($definition.CacheName)=$sourceValue"
        if (-not $available) {
            $missingCount++
        }
        $states += [pscustomobject]@{
            Name = $provider
            Label = $definition.Label
            SourceRoot = $sourceValue
            Available = [bool]$available
            CMakeArgument = [string]$arguments[$arguments.Count - 1]
        }
    }

    $fullyDisconnected = ($missingCount -eq 0)
    $modeArgument = if ($fullyDisconnected) {
        "-DFETCHCONTENT_FULLY_DISCONNECTED=ON"
    } else {
        "-DFETCHCONTENT_FULLY_DISCONNECTED=OFF"
    }
    $arguments += $modeArgument

    return [pscustomobject]@{
        Arguments = [string[]]$arguments
        ProviderStates = [object[]]$states
        MissingCount = $missingCount
        FullyDisconnected = $fullyDisconnected
    }
}

function Convert-HenkaCMakeValueForComparison {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Value
    )

    return $Value.Trim().Trim('"').Replace("\", "/")
}

function Get-HenkaCMakeCacheEntry {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CacheText,

        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $pattern = "(?m)^" + [System.Text.RegularExpressions.Regex]::Escape($Name) +
        ":[^=]*=(?<value>.*)$"
    $match = [System.Text.RegularExpressions.Regex]::Match($CacheText, $pattern)
    return [pscustomobject]@{
        Found = $match.Success
        Value = if ($match.Success) { $match.Groups["value"].Value.Trim() } else { "" }
    }
}

function Test-HenkaCMakeConfigurationReady {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildRoot,

        [Parameter(Mandatory = $true)]
        [string]$RepositoryRoot,

        [Parameter(Mandatory = $true)]
        [string[]]$ConfigureArguments
    )

    $cachePath = Join-Path $BuildRoot "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) {
        return $false
    }

    $cacheText = [System.IO.File]::ReadAllText($cachePath)
    $repositoryValue = Convert-HenkaCMakeValueForComparison `
        -Value ([System.IO.Path]::GetFullPath($RepositoryRoot))
    $homeEntry = Get-HenkaCMakeCacheEntry `
        -CacheText $cacheText `
        -Name "CMAKE_HOME_DIRECTORY"
    if (-not $homeEntry.Found -or
        (Convert-HenkaCMakeValueForComparison -Value $homeEntry.Value) -ne $repositoryValue) {
        return $false
    }

    foreach ($argument in @($ConfigureArguments)) {
        if ([string]$argument -notmatch '^-D(?<name>[^=]+)=(?<value>.*)$') {
            continue
        }
        $entry = Get-HenkaCMakeCacheEntry `
            -CacheText $cacheText `
            -Name $Matches.name
        if (-not $entry.Found -or
            (Convert-HenkaCMakeValueForComparison -Value $entry.Value) -ne
                (Convert-HenkaCMakeValueForComparison -Value $Matches.value)) {
            return $false
        }
    }
    return $true
}

function ConvertFrom-HenkaCTestJsonListing {
    param(
        [Parameter(Mandatory = $true)]
        [string]$JsonText,

        [string]$BuildRoot = "CTest"
    )

    try {
        $listing = ConvertFrom-Json -InputObject $JsonText -ErrorAction Stop
    }
    catch {
        throw "CTest returned an invalid JSON test listing for '$BuildRoot': $($_.Exception.Message)"
    }
    if ([string]$listing.kind -ne "ctestInfo") {
        throw "CTest returned an unexpected JSON listing kind for '$BuildRoot'."
    }

    $tests = @($listing.tests | Where-Object { $null -ne $_ })
    foreach ($test in $tests) {
        $nameProperty = $test.PSObject.Properties["name"]
        if ($null -eq $nameProperty -or
            [string]::IsNullOrWhiteSpace([string]$nameProperty.Value)) {
            $recordSummary = ConvertTo-Json -InputObject $test -Compress -Depth 8
            throw "CTest returned a test record without a test name for '$BuildRoot': $recordSummary"
        }

        # CTest's json-v1 schema makes command optional. Preserve a name-only
        # record so filter validation remains authoritative; target planning
        # will conservatively fall back to the aggregate build when the
        # executable identity is unavailable.
        $command = ""
        $commandProperty = $test.PSObject.Properties["command"]
        if ($null -ne $commandProperty) {
            $commandParts = @($commandProperty.Value | Where-Object { $null -ne $_ })
            if ($commandParts.Count -eq 0 -or
                [string]::IsNullOrWhiteSpace([string]$commandParts[0])) {
                throw "CTest returned an empty test command for '$($nameProperty.Value)'."
            }
            $command = [string]$commandParts[0]
        }

        [pscustomobject]@{
            Name = [string]$nameProperty.Value
            Command = $command
        }
    }
}

function Get-HenkaCTestCommandRecords {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildRoot,

        [ValidateSet("Debug", "Release")]
        [string]$Configuration = "Debug",

        [string]$TestFilter = ""
    )

    $testManifest = Join-Path $BuildRoot "CTestTestfile.cmake"
    if (-not (Test-Path -LiteralPath $BuildRoot -PathType Container) -or
        -not (Test-Path -LiteralPath $testManifest -PathType Leaf)) {
        return @()
    }

    $cmake = Get-HenkaCMakePath
    $ctest = Get-HenkaCTestPath -CMakePath $cmake
    $arguments = @(
        "--test-dir", [System.IO.Path]::GetFullPath($BuildRoot),
        "-C", $Configuration,
        "--show-only=json-v1")
    if (-not [string]::IsNullOrWhiteSpace($TestFilter)) {
        $arguments += @("-R", $TestFilter)
    }

    $repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
    $capture = Invoke-HenkaNativeCapture `
        -FilePath $ctest `
        -Arguments $arguments `
        -WorkingDirectory $repoRoot `
        -Label "List CTest tests for validation planning" `
        -TimeoutMilliseconds 120000 `
        -Quiet
    $combinedOutput = [string]$capture.Stdout + "`n" + [string]$capture.Stderr
    if (-not [string]::IsNullOrWhiteSpace($TestFilter) -and
        $combinedOutput -match '(?im)RegularExpression::compile\(\)|Error in compile|regular expression.{0,80}compile') {
        $diagnostic = ([string]$capture.Stderr).Trim()
        if ([string]::IsNullOrWhiteSpace($diagnostic)) {
            $diagnostic = ([string]$capture.Stdout).Trim()
        }
        if ($diagnostic.Length -gt 512) {
            $diagnostic = $diagnostic.Substring(0, 512)
        }
        throw "CTest rejected test filter '$TestFilter'. CTest diagnostics: $diagnostic"
    }

    $jsonStart = ([string]$capture.Stdout).IndexOf('{')
    $jsonEnd = ([string]$capture.Stdout).LastIndexOf('}')
    if ($jsonStart -lt 0 -or $jsonEnd -lt $jsonStart) {
        throw "CTest did not return a JSON test listing for '$BuildRoot'."
    }
    $jsonText = ([string]$capture.Stdout).Substring(
        $jsonStart, $jsonEnd - $jsonStart + 1)
    return @(ConvertFrom-HenkaCTestJsonListing `
        -JsonText $jsonText `
        -BuildRoot $BuildRoot)
}

function Assert-HenkaCTestFilterMatchesRegisteredTests {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildRoot,

        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$TestFilter,

        [ValidateSet("Debug", "Release")]
        [string]$Configuration = "Debug"
    )

    if ([string]::IsNullOrWhiteSpace($TestFilter)) {
        return
    }

    $records = @(Get-HenkaCTestCommandRecords `
        -BuildRoot $BuildRoot `
        -Configuration $Configuration `
        -TestFilter $TestFilter)
    if ($records.Count -eq 0) {
        throw "TestFilter '$TestFilter' matched no registered CTest tests in configuration '$Configuration'."
    }
}

function Get-HenkaCTestCommandResolution {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Command,

        [Parameter(Mandatory = $true)]
        [ValidateSet("Debug", "Release")]
        [string]$Configuration
    )

    if ([string]::IsNullOrWhiteSpace($Command)) {
        return "aggregate-or-unresolved"
    }
    if ([System.IO.Path]::GetExtension($Command) -ne ".exe" -or
        $Command -notmatch "[\\/]$([System.Text.RegularExpressions.Regex]::Escape($Configuration))[\\/]") {
        return "non-executable-test"
    }
    return "registered-executable"
}

function Resolve-HenkaValidationPlan {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildRoot,

        [Parameter(Mandatory = $true)]
        [ValidateSet("Debug", "Release")]
        [string]$Configuration,

        [string]$TestFilter = "",

        [string]$BuildTarget = ""
    )

    $matchingTests = @()
    if (-not [string]::IsNullOrWhiteSpace($TestFilter)) {
        $matchingTests = @(Get-HenkaCTestCommandRecords `
            -BuildRoot $BuildRoot `
            -Configuration $Configuration `
            -TestFilter $TestFilter)
    }

    $explicitTarget = $BuildTarget.Trim()
    if (-not [string]::IsNullOrWhiteSpace($explicitTarget)) {
        return [pscustomobject]@{
            TestFilter = $TestFilter
            BuildTarget = $explicitTarget
            Artifact = Get-HenkaBuildArtifact `
                -BuildRoot $BuildRoot `
                -Configuration $Configuration `
                -BuildTarget $explicitTarget
            Resolution = "explicit-target"
        }
    }

    $defaultArtifact = Get-HenkaBuildArtifact `
        -BuildRoot $BuildRoot `
        -Configuration $Configuration
    if ([string]::IsNullOrWhiteSpace($TestFilter)) {
        return [pscustomobject]@{
            TestFilter = $TestFilter
            BuildTarget = ""
            Artifact = $defaultArtifact
            Resolution = "default-build"
        }
    }

    if ($matchingTests.Count -ne 1) {
        return [pscustomobject]@{
            TestFilter = $TestFilter
            BuildTarget = ""
            Artifact = $defaultArtifact
            Resolution = "aggregate-or-unresolved"
        }
    }

    $command = [string]$matchingTests[0].Command
    $commandResolution = Get-HenkaCTestCommandResolution `
        -Command $command `
        -Configuration $Configuration
    if ($commandResolution -ne "registered-executable") {
        return [pscustomobject]@{
            TestFilter = $TestFilter
            BuildTarget = ""
            Artifact = $defaultArtifact
            Resolution = $commandResolution
        }
    }

    $target = [System.IO.Path]::GetFileNameWithoutExtension($command)
    try {
        $artifact = Get-HenkaBuildArtifact `
            -BuildRoot $BuildRoot `
            -Configuration $Configuration `
            -BuildTarget $target
    }
    catch {
        return [pscustomobject]@{
            TestFilter = $TestFilter
            BuildTarget = ""
            Artifact = $defaultArtifact
            Resolution = "aggregate-or-unresolved"
        }
    }

    if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals(
            [System.IO.Path]::GetFileName($artifact.Path),
            [System.IO.Path]::GetFileName([string]$matchingTests[0].Command))) {
        return [pscustomobject]@{
            TestFilter = $TestFilter
            BuildTarget = ""
            Artifact = $defaultArtifact
            Resolution = "aggregate-or-unresolved"
        }
    }

    return [pscustomobject]@{
        TestFilter = $TestFilter
        BuildTarget = $target
        Artifact = $artifact
        Resolution = "registered-executable"
    }
}

function Get-HenkaGitPath {
    $gitCommand = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($null -eq $gitCommand) {
        $gitCommand = Get-Command git -ErrorAction SilentlyContinue
    }
    if ($null -eq $gitCommand) {
        throw "Git was not found on PATH."
    }
    return $gitCommand.Source
}

function Get-HenkaSourceIdentity {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $repoPath = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd([char[]]@("\", "/"))
    $git = Get-HenkaGitPath
    $commitLines = @(& $git -C $repoPath rev-parse HEAD 2>$null)
    $commitExitCode = $LASTEXITCODE
    $statusLines = @(& $git -C $repoPath status --porcelain=v1 --untracked-files=all 2>$null)
    $statusExitCode = $LASTEXITCODE
    # Keep Git path enumeration line-oriented here. Native NUL-delimited
    # output is decoded differently by Windows PowerShell and PowerShell 7,
    # which made the same clean tree receive different identities depending
    # on which supported host invoked this shared helper.
    $trackedPaths = @(& $git -C $repoPath ls-files --cached 2>$null)
    $trackedExitCode = $LASTEXITCODE
    $untrackedPaths = @(& $git -C $repoPath ls-files --others --exclude-standard 2>$null)
    $untrackedExitCode = $LASTEXITCODE

    if ($commitExitCode -ne 0 -or
        $statusExitCode -ne 0 -or
        $trackedExitCode -ne 0 -or
        $untrackedExitCode -ne 0 -or
        $commitLines.Count -ne 1 -or
        [string]::IsNullOrWhiteSpace([string]$commitLines[0])) {
        throw "Git source identity query failed for $repoPath."
    }

    $relativePathList = New-Object 'System.Collections.Generic.List[string]'
    $relativePathSet = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::Ordinal)
    foreach ($path in @($trackedPaths + $untrackedPaths)) {
        if ([string]::IsNullOrWhiteSpace([string]$path)) {
            continue
        }
        $normalizedPath = ([string]$path).Replace("\", "/")
        if ($relativePathSet.Add($normalizedPath)) {
            [void]$relativePathList.Add($normalizedPath)
        }
    }
    $relativePathList.Sort([System.StringComparer]::Ordinal)
    $relativePaths = @($relativePathList)
    $manifestBuilder = New-Object System.Text.StringBuilder
    [void]$manifestBuilder.Append("commit`0")
    [void]$manifestBuilder.Append(([string]$commitLines[0]).Trim())
    [void]$manifestBuilder.Append("`0")

    foreach ($relativePath in $relativePaths) {
        $filePath = Join-Path $repoPath ($relativePath.Replace("/", [System.IO.Path]::DirectorySeparatorChar))
        if (-not (Test-Path -LiteralPath $filePath -PathType Leaf)) {
            throw "Source identity input disappeared during hashing: $filePath"
        }
        $fileHash = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash.ToLowerInvariant()
        [void]$manifestBuilder.Append($relativePath)
        [void]$manifestBuilder.Append("`0")
        [void]$manifestBuilder.Append($fileHash)
        [void]$manifestBuilder.Append("`0")
    }

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $digest = $sha256.ComputeHash(
            [System.Text.Encoding]::UTF8.GetBytes($manifestBuilder.ToString()))
    }
    finally {
        $sha256.Dispose()
    }

    return [pscustomobject]@{
        commit_sha = ([string]$commitLines[0]).Trim()
        source_state = if ($statusLines.Count -eq 0) { "clean" } else { "working-tree" }
        source_identity = (-join ($digest | ForEach-Object { $_.ToString("x2") }))
    }
}

function Write-HenkaUtf8NoBom {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Text
    )

    $encoding = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $encoding)
}

function ConvertTo-HenkaNativeArgument {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyString()]
        [string]$Value
    )

    if ($Value.Length -gt 0 -and $Value -notmatch '[\s"]') {
        return $Value
    }

    $builder = New-Object System.Text.StringBuilder
    [void]$builder.Append('"')
    $backslashCount = 0

    foreach ($character in $Value.ToCharArray()) {
        if ($character -eq '\') {
            $backslashCount++
            continue
        }

        if ($character -eq '"') {
            if ($backslashCount -gt 0) {
                [void]$builder.Append((('\' * ($backslashCount * 2)) -join ''))
                $backslashCount = 0
            }
            [void]$builder.Append('\"')
            continue
        }

        if ($backslashCount -gt 0) {
            [void]$builder.Append((('\' * $backslashCount) -join ''))
            $backslashCount = 0
        }
        [void]$builder.Append($character)
    }

    if ($backslashCount -gt 0) {
        [void]$builder.Append((('\' * ($backslashCount * 2)) -join ''))
    }
    [void]$builder.Append('"')
    return $builder.ToString()
}

function ConvertTo-HenkaNativeArgumentString {
    param([string[]]$Arguments = @())

    $values = @($Arguments | Where-Object { $null -ne $_ })
    if ($values.Count -ne @($Arguments).Count) {
        throw "A null native-process argument was provided."
    }

    return (@($values | ForEach-Object {
        ConvertTo-HenkaNativeArgument -Value ([string]$_)
    }) -join " ")
}

function Initialize-HenkaCapturedProcessType {
    if ($null -ne ("HenkaCapturedProcess" -as [type])) {
        return
    }

    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

public sealed class HenkaCapturedProcess : IDisposable
{
    private readonly object stdoutLock = new object();
    private readonly object stderrLock = new object();
    private readonly StreamWriter stdoutWriter;
    private readonly StreamWriter stderrWriter;
    private bool disposed;

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    private static extern IntPtr GetForegroundWindow();

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetForegroundWindow(IntPtr hWnd);

    private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(
        IntPtr hWnd,
        out uint processId);

    private const int SW_SHOWMINNOACTIVE = 7;
    private const int SW_SHOWNOACTIVATE = 4;

    public Process Process { get; private set; }

    private HenkaCapturedProcess(
        Process process,
        StreamWriter stdoutWriter,
        StreamWriter stderrWriter)
    {
        Process = process;
        this.stdoutWriter = stdoutWriter;
        this.stderrWriter = stderrWriter;
        Process.OutputDataReceived += OnOutputDataReceived;
        Process.ErrorDataReceived += OnErrorDataReceived;
    }

    public static HenkaCapturedProcess Start(
        string filePath,
        string arguments,
        string workingDirectory,
        string stdoutPath,
        string stderrPath,
        bool createNoWindow,
        bool startMinimized,
        bool startVisibleWithoutActivation,
        string windowsPowerShellModulePath)
    {
        ProcessStartInfo startInfo = new ProcessStartInfo();
        startInfo.FileName = filePath;
        startInfo.Arguments = arguments ?? String.Empty;
        startInfo.WorkingDirectory = workingDirectory;
        startInfo.UseShellExecute = false;
        startInfo.CreateNoWindow = createNoWindow;
        if (!String.IsNullOrWhiteSpace(windowsPowerShellModulePath))
        {
            startInfo.EnvironmentVariables["PSModulePath"] = windowsPowerShellModulePath;
        }
        if (!createNoWindow && (startMinimized || startVisibleWithoutActivation))
        {
            // Validation may need a native window for PrintWindow or an
            // event-driven UI path, but it must not take ownership of the
            // user's typing focus merely by being launched.
            // Native GUI frameworks can ignore a minimized startup hint while
            // creating their first window. Hidden startup prevents a transient
            // foreground activation; StartMinimize then exposes it minimized
            // without activation once the real window exists.
            startInfo.WindowStyle = ProcessWindowStyle.Hidden;
        }
        startInfo.RedirectStandardOutput = true;
        startInfo.RedirectStandardError = true;

        UTF8Encoding encoding = new UTF8Encoding(false);
        StreamWriter stdoutWriter = new StreamWriter(stdoutPath, false, encoding);
        StreamWriter stderrWriter = new StreamWriter(stderrPath, false, encoding);
        stdoutWriter.AutoFlush = true;
        stderrWriter.AutoFlush = true;

        Process process = new Process();
        process.StartInfo = startInfo;
        HenkaCapturedProcess capture = new HenkaCapturedProcess(
            process,
            stdoutWriter,
            stderrWriter);

        try
        {
            if (!process.Start())
            {
                throw new InvalidOperationException("The process did not start.");
            }
            if (!createNoWindow && startMinimized)
            {
                StartMinimize(process, GetForegroundWindow());
            }
            else if (!createNoWindow && startVisibleWithoutActivation)
            {
                StartShowWithoutActivation(process);
            }
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();
            return capture;
        }
        catch
        {
            capture.Dispose();
            throw;
        }
    }

    public static void StartMinimize(Process process)
    {
        StartMinimize(process, GetForegroundWindow());
    }

    public static IntPtr CurrentForegroundWindow()
    {
        return GetForegroundWindow();
    }

    private static void MinimizeOwnedWindows(
        int processId,
        IntPtr previousForeground)
    {
        EnumWindows(delegate(IntPtr hWnd, IntPtr lParam)
        {
            uint ownerProcessId;
            GetWindowThreadProcessId(hWnd, out ownerProcessId);
            if (ownerProcessId == (uint)processId)
            {
                ShowWindow(hWnd, SW_SHOWMINNOACTIVE);
                if (previousForeground != IntPtr.Zero &&
                    GetForegroundWindow() == hWnd)
                {
                    // Restore the user's existing foreground owner; never
                    // activate the newly launched validation window here.
                    SetForegroundWindow(previousForeground);
                }
            }
            return true;
        }, IntPtr.Zero);
    }

    private static void ShowOwnedWindowsWithoutActivation(
        int processId,
        HashSet<IntPtr> handledWindows)
    {
        EnumWindows(delegate(IntPtr hWnd, IntPtr lParam)
        {
            uint ownerProcessId;
            GetWindowThreadProcessId(hWnd, out ownerProcessId);
            if (ownerProcessId == (uint)processId && handledWindows.Add(hWnd))
            {
                ShowWindow(hWnd, SW_SHOWNOACTIVATE);
            }
            return true;
        }, IntPtr.Zero);
    }

    public static void StartMinimize(Process process, IntPtr previousForeground)
    {
        // Apply one synchronous pass before returning the launch helper to its
        // caller. This closes the gap where a native window is created between
        // Process.Start and the first thread-pool callback.
        try
        {
            if (!process.HasExited)
            {
                process.Refresh();
                MinimizeOwnedWindows(process.Id, previousForeground);
            }
        }
        catch
        {
            // The bounded monitor below remains the recovery path for a
            // process whose native window is not ready yet.
        }
        ThreadPool.QueueUserWorkItem(delegate(object state)
        {
            Process target = (Process)state;
            // Keep enforcing the non-activating state through the bounded
            // startup window. A GUI framework can create its handle first and
            // activate it a few scheduler ticks later, after a one-shot
            // minimization would already have returned.
            for (int attempt = 0; attempt < 2000; ++attempt)
            {
                try
                {
                    if (target.HasExited)
                    {
                        return;
                    }
                    target.Refresh();
                    MinimizeOwnedWindows(target.Id, previousForeground);
                }
                catch
                {
                    return;
                }
                Thread.Sleep(10);
            }
        }, process);
    }

    public static void StartShowWithoutActivation(
        Process process)
    {
        ThreadPool.QueueUserWorkItem(delegate(object state)
        {
            Process target = (Process)state;
            HashSet<IntPtr> handledWindows = new HashSet<IntPtr>();
            for (int attempt = 0; attempt < 2000; ++attempt)
            {
                try
                {
                    if (target.HasExited)
                    {
                        return;
                    }
                    target.Refresh();
                    ShowOwnedWindowsWithoutActivation(
                        target.Id,
                        handledWindows);
                }
                catch
                {
                    return;
                }
                Thread.Sleep(10);
            }
        }, process);
    }

    private void OnOutputDataReceived(object sender, DataReceivedEventArgs eventArgs)
    {
        if (eventArgs.Data == null)
        {
            return;
        }
        lock (stdoutLock)
        {
            stdoutWriter.WriteLine(eventArgs.Data);
        }
    }

    private void OnErrorDataReceived(object sender, DataReceivedEventArgs eventArgs)
    {
        if (eventArgs.Data == null)
        {
            return;
        }
        lock (stderrLock)
        {
            stderrWriter.WriteLine(eventArgs.Data);
        }
    }

    public bool WaitForExit(int timeoutMilliseconds)
    {
        bool exited = Process.WaitForExit(timeoutMilliseconds);
        if (exited)
        {
            Process.WaitForExit();
            lock (stdoutLock) { stdoutWriter.Flush(); }
            lock (stderrLock) { stderrWriter.Flush(); }
        }
        return exited;
    }

    public void Dispose()
    {
        if (disposed)
        {
            return;
        }
        disposed = true;

        try
        {
            if (Process != null && !Process.HasExited)
            {
                Process.Kill();
                Process.WaitForExit(5000);
            }
            if (Process != null && Process.HasExited)
            {
                // Drain redirected async output only after a bounded exit was
                // observed. Disposal must never become an unbounded wait.
                Process.WaitForExit();
            }
        }
        catch
        {
        }

        if (Process != null)
        {
            Process.OutputDataReceived -= OnOutputDataReceived;
            Process.ErrorDataReceived -= OnErrorDataReceived;
        }

        lock (stdoutLock) { stdoutWriter.Dispose(); }
        lock (stderrLock) { stderrWriter.Dispose(); }

        if (Process != null)
        {
            Process.Dispose();
        }
    }
}
'@
}

function Get-HenkaWindowsPowerShellModulePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath
    )

    if ([System.IO.Path]::GetFileName($FilePath) -ine "powershell.exe") {
        return ""
    }

    $resolvedExecutable = $FilePath
    if (-not [System.IO.Path]::IsPathRooted($resolvedExecutable)) {
        $application = Get-Command -Name $FilePath -CommandType Application -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($null -eq $application) {
            return ""
        }
        $resolvedExecutable = $application.Source
    }

    if (-not (Test-Path -LiteralPath $resolvedExecutable -PathType Leaf)) {
        return ""
    }

    $nativeModulesPath = Join-Path (Split-Path -Parent $resolvedExecutable) "Modules"
    if (-not (Test-Path -LiteralPath $nativeModulesPath -PathType Container)) {
        return ""
    }
    $nativeModulesPath = [System.IO.Path]::GetFullPath($nativeModulesPath)

    $orderedPaths = New-Object 'System.Collections.Generic.List[string]'
    [void]$orderedPaths.Add($nativeModulesPath)
    $pathSeparator = [System.IO.Path]::PathSeparator
    $existingModulePath = [System.Environment]::GetEnvironmentVariable("PSModulePath", "Process")
    if (-not [string]::IsNullOrWhiteSpace($existingModulePath)) {
        foreach ($entry in $existingModulePath.Split($pathSeparator)) {
            $trimmedEntry = $entry.Trim()
            if ([string]::IsNullOrWhiteSpace($trimmedEntry)) {
                continue
            }

            $entryPath = $trimmedEntry
            try {
                $entryPath = [System.IO.Path]::GetFullPath($trimmedEntry)
            }
            catch {
                # Preserve an unusual inherited entry as-is; only the native
                # Windows PowerShell module path needs explicit precedence.
            }

            if ([string]::Equals(
                $entryPath.TrimEnd([char[]]@("\", "/")),
                $nativeModulesPath.TrimEnd([char[]]@("\", "/")),
                [System.StringComparison]::OrdinalIgnoreCase)) {
                continue
            }
            [void]$orderedPaths.Add($trimmedEntry)
        }
    }

    return ($orderedPaths -join [string]$pathSeparator)
}

function Start-HenkaProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [switch]$CreateNoWindow,

        # Native validation windows start minimized by default. A capturable
        # visible surface can instead opt into a non-activating show state.
        [bool]$StartMinimized = $true,

        [switch]$StartVisibleWithoutActivation
    )

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = ConvertTo-HenkaNativeArgumentString -Arguments $Arguments
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = [bool]$CreateNoWindow
    $windowsPowerShellModulePath = Get-HenkaWindowsPowerShellModulePath -FilePath $FilePath
    if (-not [string]::IsNullOrWhiteSpace($windowsPowerShellModulePath)) {
        $startInfo.EnvironmentVariables["PSModulePath"] = $windowsPowerShellModulePath
    }
    if (-not $CreateNoWindow -and
        ($StartMinimized -or $StartVisibleWithoutActivation)) {
        # See the captured-process path: hidden creation avoids a transient
        # foreground activation, then the shared callback exposes the window
        # minimized without activation when its native handle exists.
        $startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
    }

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $CreateNoWindow -and
        ($StartMinimized -or $StartVisibleWithoutActivation)) {
        Initialize-HenkaCapturedProcessType
    }
    $previousForeground = if (-not $CreateNoWindow -and
        ($StartMinimized -or $StartVisibleWithoutActivation)) {
        [HenkaCapturedProcess]::CurrentForegroundWindow()
    } else {
        [IntPtr]::Zero
    }
    if (-not $process.Start()) {
        $process.Dispose()
        throw "The process did not start: $FilePath"
    }
    if (-not $CreateNoWindow -and $StartMinimized) {
        [HenkaCapturedProcess]::StartMinimize($process, $previousForeground)
    }
    elseif (-not $CreateNoWindow -and $StartVisibleWithoutActivation) {
        [HenkaCapturedProcess]::StartShowWithoutActivation($process)
    }
    return $process
}

function Start-HenkaCapturedProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$StdoutPath,

        [Parameter(Mandatory = $true)]
        [string]$StderrPath,

        [switch]$CreateNoWindow,

        # Native validation windows start minimized by default. A capturable
        # visible surface can instead opt into a non-activating show state.
        [bool]$StartMinimized = $true,

        [switch]$StartVisibleWithoutActivation
    )

    Initialize-HenkaCapturedProcessType
    $stdoutDirectory = Split-Path -Parent $StdoutPath
    $stderrDirectory = Split-Path -Parent $StderrPath
    if (-not [string]::IsNullOrWhiteSpace($stdoutDirectory)) {
        [System.IO.Directory]::CreateDirectory($stdoutDirectory) | Out-Null
    }
    if (-not [string]::IsNullOrWhiteSpace($stderrDirectory)) {
        [System.IO.Directory]::CreateDirectory($stderrDirectory) | Out-Null
    }
    $windowsPowerShellModulePath = Get-HenkaWindowsPowerShellModulePath -FilePath $FilePath

    return [HenkaCapturedProcess]::Start(
        $FilePath,
        (ConvertTo-HenkaNativeArgumentString -Arguments $Arguments),
        $WorkingDirectory,
        $StdoutPath,
        $StderrPath,
        [bool]$CreateNoWindow,
        $StartMinimized,
        [bool]$StartVisibleWithoutActivation,
        $windowsPowerShellModulePath)
}

function Close-HenkaCapturedProcess {
    param($CapturedProcess)

    if ($null -eq $CapturedProcess) {
        return
    }

    try {
        if ($null -ne $CapturedProcess.Process -and
            -not $CapturedProcess.Process.HasExited) {
            Stop-HenkaProcessTree -ProcessId $CapturedProcess.Process.Id
            [void]$CapturedProcess.WaitForExit(5000)
        }
    }
    catch {
        # Disposal below remains the last bounded cleanup attempt.
    }
    $CapturedProcess.Dispose()
}

function Stop-HenkaProcessTree {
    param(
        [Parameter(Mandatory = $true)]
        [int]$ProcessId
    )

    if ($ProcessId -le 0) {
        return
    }

    $taskkill = Join-Path $env:SystemRoot "System32\taskkill.exe"
    if (Test-Path -LiteralPath $taskkill -PathType Leaf) {
        $previousErrorActionPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = "SilentlyContinue"
            & $taskkill /PID $ProcessId /T /F 2>$null | Out-Null
        }
        finally {
            $ErrorActionPreference = $previousErrorActionPreference
        }
    }
}

function Invoke-HenkaNative {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Label,

        [int]$TimeoutMilliseconds = -1
    )

    Write-Host ""
    Write-Host "==> $Label"
    Write-Host "    $FilePath $($Arguments -join ' ')"

    $previousErrorActionPreference = $ErrorActionPreference
    $exitCode = -1

    Push-Location $WorkingDirectory
    try {
        if ($TimeoutMilliseconds -gt 0) {
            # Use the captured path for bounded commands so their diagnostic
            # output remains visible in CI. The prior plain Process path hid
            # CTest's failing-test details and reduced a useful failure to only
            # its exit code.
            $null = Invoke-HenkaNativeCapture `
                -FilePath $FilePath `
                -Arguments $Arguments `
                -WorkingDirectory $WorkingDirectory `
                -Label $Label `
                -TimeoutMilliseconds $TimeoutMilliseconds
            $exitCode = 0
        }
        else {
            $ErrorActionPreference = "Continue"
            & $FilePath @Arguments 2>&1 |
                ForEach-Object {
                    Write-Host ([string]$_)
                }
            $exitCode = $LASTEXITCODE
        }
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
        Pop-Location
    }

    if ($exitCode -ne 0) {
        throw "$Label failed with exit code $exitCode."
    }
}

function Read-HenkaSharedText {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }

    $stream = $null
    $reader = $null
    try {
        $share = [System.IO.FileShare]::ReadWrite -bor [System.IO.FileShare]::Delete
        $stream = [System.IO.File]::Open(
            $Path,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read,
            $share)
        $reader = New-Object System.IO.StreamReader(
            $stream,
            [System.Text.Encoding]::UTF8,
            $true)
        return $reader.ReadToEnd()
    }
    finally {
        if ($null -ne $reader) {
            $reader.Dispose()
        }
        elseif ($null -ne $stream) {
            $stream.Dispose()
        }
    }
}

function Invoke-HenkaNativeCapture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Label,

        [ValidateRange(1000, 3600000)]
        [int]$TimeoutMilliseconds = 180000,

        [switch]$Quiet
    )

    $repoRoot = Get-HenkaRepoRoot -ScriptDirectory $script:HenkaCommonScriptDirectory
    $captureRoot = New-HenkaTemporaryDirectory -RepositoryRoot $repoRoot -Purpose "native-process-capture"
    $stdoutPath = Join-Path $captureRoot "stdout.log"
    $stderrPath = Join-Path $captureRoot "stderr.log"
    $capturedProcess = $null

    try {
        if (-not $Quiet) {
            Write-Host ""
            Write-Host "==> $Label"
            Write-Host "    $FilePath $($Arguments -join ' ')"
        }

        $capturedProcess = Start-HenkaCapturedProcess `
            -FilePath $FilePath `
            -Arguments $Arguments `
            -WorkingDirectory $WorkingDirectory `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath `
            -CreateNoWindow

        if (-not $capturedProcess.WaitForExit($TimeoutMilliseconds)) {
            Stop-HenkaProcessTree -ProcessId $capturedProcess.Process.Id
            # Wait for redirected async output callbacks to drain before
            # snapshotting timeout diagnostics. Without this bounded flush, a
            # ready marker emitted immediately before termination can be lost.
            [void]$capturedProcess.WaitForExit(5000)
            $timeoutOutput = (Read-HenkaSharedText -Path $stdoutPath) + "`n" +
                (Read-HenkaSharedText -Path $stderrPath)
            $timeoutDiagnostic = (($timeoutOutput -replace "\\s+", " ").Trim())
            if ($timeoutDiagnostic.Length -gt 512) {
                $timeoutDiagnostic = $timeoutDiagnostic.Substring(0, 512)
            }
            throw "$Label exceeded timeout ${TimeoutMilliseconds}ms and its process tree was terminated. Diagnostics: $timeoutDiagnostic"
        }

        $exitCode = $capturedProcess.Process.ExitCode
        $stdout = Read-HenkaSharedText -Path $stdoutPath
        $stderr = Read-HenkaSharedText -Path $stderrPath

        if (-not $Quiet -or $exitCode -ne 0) {
            if (-not [string]::IsNullOrWhiteSpace($stdout)) {
                Write-Host $stdout.TrimEnd()
            }
            if (-not [string]::IsNullOrWhiteSpace($stderr)) {
                Write-Host $stderr.TrimEnd()
            }
        }

        if ($exitCode -ne 0) {
            throw "$Label failed with exit code $exitCode."
        }

        return [pscustomobject]@{
            ExitCode = $exitCode
            Stdout = $stdout
            Stderr = $stderr
        }
    }
    finally {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
        Remove-Item -LiteralPath $captureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

function Invoke-HenkaExpectedFailure {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory = $true)]
        [string]$Label,

        [int]$TimeoutMilliseconds = 120000,

        [switch]$ReturnOutput
    )

    if ($TimeoutMilliseconds -le 0) {
        throw "Expected-failure process timeout must be positive."
    }

    $repoRoot = Get-HenkaRepoRoot -ScriptDirectory $script:HenkaCommonScriptDirectory
    $captureRoot = New-HenkaTemporaryDirectory -RepositoryRoot $repoRoot -Purpose "expected-failure-capture"
    $stdoutPath = Join-Path $captureRoot "stdout.log"
    $stderrPath = Join-Path $captureRoot "stderr.log"
    $capturedProcess = $null

    try {
        Write-Host ""
        Write-Host "==> $Label"
        Write-Host "    $FilePath $($Arguments -join ' ')"

        $capturedProcess = Start-HenkaCapturedProcess `
            -FilePath $FilePath `
            -Arguments $Arguments `
            -WorkingDirectory $WorkingDirectory `
            -StdoutPath $stdoutPath `
            -StderrPath $stderrPath `
            -CreateNoWindow

        if (-not $capturedProcess.WaitForExit($TimeoutMilliseconds)) {
            Stop-HenkaProcessTree -ProcessId $capturedProcess.Process.Id
            # Wait for redirected async output callbacks to drain before
            # snapshotting timeout diagnostics. Without this bounded flush, a
            # ready marker emitted immediately before termination can be lost.
            [void]$capturedProcess.WaitForExit(5000)
            $timeoutOutput = (Read-HenkaSharedText -Path $stdoutPath) + "`n" +
                (Read-HenkaSharedText -Path $stderrPath)
            $timeoutDiagnostic = (($timeoutOutput -replace "\\s+", " ").Trim())
            if ($timeoutDiagnostic.Length -gt 512) {
                $timeoutDiagnostic = $timeoutDiagnostic.Substring(0, 512)
            }
            throw "$Label exceeded timeout ${TimeoutMilliseconds}ms and its process tree was terminated. Diagnostics: $timeoutDiagnostic"
        }

        $stdout = Read-HenkaSharedText -Path $stdoutPath
        $stderr = Read-HenkaSharedText -Path $stderrPath
        if (-not [string]::IsNullOrWhiteSpace($stdout)) {
            Write-Host $stdout.TrimEnd()
        }
        if (-not [string]::IsNullOrWhiteSpace($stderr)) {
            Write-Host $stderr.TrimEnd()
        }

        $result = [pscustomobject]@{
            ExitCode = [int]$capturedProcess.Process.ExitCode
            Stdout = $stdout
            Stderr = $stderr
        }
        if ($ReturnOutput) {
            return $result
        }
        return $result.ExitCode
    }
    finally {
        Close-HenkaCapturedProcess -CapturedProcess $capturedProcess
        Remove-Item -LiteralPath $captureRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
