param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$commonPath = Join-Path $repositoryRoot "scripts\henka_script_common.ps1"
. $commonPath

$cacheModulePath = Join-Path $repositoryRoot "scripts\showcase_generation_cache.ps1"
if (-not (Test-Path -LiteralPath $cacheModulePath -PathType Leaf)) {
    throw "Showcase cache contract helper is missing: $cacheModulePath"
}
. $cacheModulePath

$scratchRelativePath = "temporary\showcase-cache-contract-$([Guid]::NewGuid().ToString('N'))"
$scratchPath = Resolve-HenkaLocalPath -RepositoryRoot $repositoryRoot -Path $scratchRelativePath
$scratchCreated = $false

try {
    $null = New-HenkaLocalDirectory -RepositoryRoot $repositoryRoot -Path $scratchRelativePath
    $scratchCreated = $true

    $outputDirectory = Join-Path $scratchPath "outputs"
    [IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
    $generatorInputPath = Join-Path $scratchPath "generator-input.ps1"
    $outputPath = Join-Path $outputDirectory "fixture.bin"
    $manifestPath = Join-Path $scratchPath "manifest.json"
    $generatorPath = Join-Path $repositoryRoot "scripts\generate_showcase_assets.ps1"
    $pathHelperPath = Join-Path $repositoryRoot "scripts\henka_script_common.ps1"
    $cacheHelperPath = Join-Path $repositoryRoot "scripts\showcase_generation_cache.ps1"
    Add-Type -AssemblyName System.Drawing
    Copy-Item -LiteralPath $generatorPath -Destination $generatorInputPath
    [IO.File]::WriteAllText($outputPath, "stable generated payload", [Text.UTF8Encoding]::new($false))

    $outputNames = @("fixture.bin")
    $fingerprintInputs = @(
        "format=1",
        "generator=$(Get-ShowcaseCacheSha256 -Path $generatorInputPath)",
        "path-helper=$(Get-ShowcaseCacheSha256 -Path $pathHelperPath)",
        "cache-helper=$(Get-ShowcaseCacheSha256 -Path $cacheHelperPath)",
        "powershell=$($PSVersionTable.PSVersion.ToString())",
        "platform=$([Environment]::OSVersion.VersionString)",
        "drawing=$([System.Drawing.Bitmap].Assembly.GetName().Version.ToString())",
        "outputs=$($outputNames -join ',')")
    $fingerprint = Get-ShowcaseGenerationFingerprint -Inputs $fingerprintInputs
    $manifest = [ordered]@{
        format_version = 1
        input_fingerprint = $fingerprint
        outputs = @([pscustomobject]@{
            name = "fixture.bin"
            sha256 = Get-ShowcaseCacheSha256 -Path $outputPath
        })
    }
    Write-ShowcaseGenerationManifest -ManifestPath $manifestPath -Manifest $manifest

    $beforeOutputHash = Get-ShowcaseCacheSha256 -Path $outputPath
    $beforeOutputWriteTicks = (Get-Item -LiteralPath $outputPath).LastWriteTimeUtc.Ticks
    $unchanged = Test-ShowcaseGenerationCache `
        -ManifestPath $manifestPath `
        -OutputDirectory $outputDirectory `
        -InputFingerprint $fingerprint `
        -OutputNames $outputNames
    if (-not $unchanged.CacheHit -or $unchanged.Reason -cne "hit") {
        throw "Unchanged inputs and outputs did not reuse the verified cache: $($unchanged.Reason)"
    }
    Write-Output "[pass] Unchanged source inputs and output hashes reuse the verified cache."

    [IO.File]::AppendAllText($generatorInputPath, "`n# harmless source revision change`n", [Text.UTF8Encoding]::new($false))
    $changedInputs = @(
        "format=1",
        "generator=$(Get-ShowcaseCacheSha256 -Path $generatorInputPath)",
        "path-helper=$(Get-ShowcaseCacheSha256 -Path $pathHelperPath)",
        "cache-helper=$(Get-ShowcaseCacheSha256 -Path $cacheHelperPath)",
        "powershell=$($PSVersionTable.PSVersion.ToString())",
        "platform=$([Environment]::OSVersion.VersionString)",
        "drawing=$([System.Drawing.Bitmap].Assembly.GetName().Version.ToString())",
        "outputs=$($outputNames -join ',')")
    $changedFingerprint = Get-ShowcaseGenerationFingerprint -Inputs $changedInputs
    $changed = Test-ShowcaseGenerationCache `
        -ManifestPath $manifestPath `
        -OutputDirectory $outputDirectory `
        -InputFingerprint $changedFingerprint `
        -OutputNames $outputNames
    if ($changed.CacheHit -or $changed.Reason -cne "fingerprint-changed") {
        throw "A changed generator input did not invalidate the cache precisely: $($changed.Reason)"
    }
    if ((Get-ShowcaseCacheSha256 -Path $outputPath) -cne $beforeOutputHash -or
        (Get-Item -LiteralPath $outputPath).LastWriteTimeUtc.Ticks -ne $beforeOutputWriteTicks) {
        throw "Cache invalidation modified outputs before generation began."
    }
    Write-Output "[pass] A real generator-source hash change invalidates the cache without touching outputs."

    $updatedManifest = [ordered]@{
        format_version = 1
        input_fingerprint = $changedFingerprint
        outputs = @([pscustomobject]@{
            name = "fixture.bin"
            sha256 = Get-ShowcaseCacheSha256 -Path $outputPath
        })
    }
    Write-ShowcaseGenerationManifest -ManifestPath $manifestPath -Manifest $updatedManifest
    $updated = Test-ShowcaseGenerationCache `
        -ManifestPath $manifestPath `
        -OutputDirectory $outputDirectory `
        -InputFingerprint $changedFingerprint `
        -OutputNames $outputNames
    if (-not $updated.CacheHit -or $updated.Reason -cne "hit") {
        throw "The atomically replaced manifest did not establish a cache hit."
    }
    $manifestArtifacts = @(Get-ChildItem -LiteralPath $scratchPath -File -Force |
        Where-Object { $_.Name -match '^manifest\.json\..+\.(tmp|bak)$' })
    if ($manifestArtifacts.Count -ne 0) {
        throw "Successful manifest publication left temporary artifacts: $($manifestArtifacts.Name -join ', ')"
    }
    Write-Output "[pass] Replacing an existing manifest leaves no temporary or backup files."
}
finally {
    if ($scratchCreated -and (Test-Path -LiteralPath $scratchPath)) {
        $scratchItem = Get-Item -LiteralPath $scratchPath -Force
        if (($scratchItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing to clean the Showcase cache test path because it became a reparse point: $scratchPath"
        }
        $null = Resolve-HenkaLocalPath -RepositoryRoot $repositoryRoot -Path $scratchPath
        Remove-Item -LiteralPath $scratchPath -Recurse -Force
    }
}

if (Test-Path -LiteralPath $scratchPath) {
    throw "Showcase cache contract test left its exact temporary root behind: $scratchPath"
}
Write-Output "[pass] The exact test-owned temporary root was retired."
