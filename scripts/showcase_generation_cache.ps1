Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-ShowcaseCacheSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    $sha256 = [Security.Cryptography.SHA256]::Create()
    $stream = $null
    try {
        $stream = [IO.File]::OpenRead($Path)
        return ([BitConverter]::ToString($sha256.ComputeHash($stream))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        if ($null -ne $stream) {
            $stream.Dispose()
        }
        $sha256.Dispose()
    }
}

function Get-ShowcaseGenerationFingerprint {
    param([Parameter(Mandatory = $true)][string[]]$Inputs)

    $fingerprintBytes = [Text.Encoding]::UTF8.GetBytes(($Inputs -join "`n"))
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha256.ComputeHash($fingerprintBytes))).Replace('-', '').ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
    }
}

function Test-ShowcaseGenerationCache {
    param(
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [Parameter(Mandatory = $true)][string]$OutputDirectory,
        [Parameter(Mandatory = $true)][string]$InputFingerprint,
        [Parameter(Mandatory = $true)][string[]]$OutputNames
    )

    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        return [pscustomobject]@{ CacheHit = $false; Reason = "manifest-missing" }
    }

    try {
        $cachedManifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json -ErrorAction Stop
    }
    catch {
        return [pscustomobject]@{ CacheHit = $false; Reason = "manifest-unreadable" }
    }

    if ([int]$cachedManifest.format_version -ne 1) {
        return [pscustomobject]@{ CacheHit = $false; Reason = "manifest-format-changed" }
    }
    if ([string]$cachedManifest.input_fingerprint -cne $InputFingerprint) {
        return [pscustomobject]@{
            CacheHit = $false
            Reason = "fingerprint-changed"
            CachedFingerprint = [string]$cachedManifest.input_fingerprint
        }
    }

    $cachedOutputs = @($cachedManifest.outputs)
    if ($cachedOutputs.Count -ne $OutputNames.Count) {
        return [pscustomobject]@{ CacheHit = $false; Reason = "output-count-changed" }
    }

    foreach ($outputName in $OutputNames) {
        $matches = @($cachedOutputs | Where-Object { [string]$_.name -ceq $outputName })
        if ($matches.Count -ne 1) {
            return [pscustomobject]@{ CacheHit = $false; Reason = "output-index-changed" }
        }

        $outputPath = Join-Path $OutputDirectory $outputName
        if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf)) {
            return [pscustomobject]@{ CacheHit = $false; Reason = "output-missing" }
        }
        if ([string]$matches[0].sha256 -cne (Get-ShowcaseCacheSha256 -Path $outputPath)) {
            return [pscustomobject]@{ CacheHit = $false; Reason = "output-hash-changed" }
        }
    }

    return [pscustomobject]@{ CacheHit = $true; Reason = "hit" }
}

function Write-ShowcaseGenerationManifest {
    param(
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [Parameter(Mandatory = $true)][object]$Manifest
    )

    $fullManifestPath = [IO.Path]::GetFullPath($ManifestPath)
    $directory = [IO.Path]::GetDirectoryName($fullManifestPath)
    if (-not [IO.Directory]::Exists($directory)) {
        throw "Showcase generation manifest directory does not exist: $directory"
    }

    $operationId = [Guid]::NewGuid().ToString('N')
    $temporaryPath = "$fullManifestPath.$PID.$operationId.tmp"
    $backupPath = "$fullManifestPath.$PID.$operationId.bak"
    $published = $false
    try {
        [IO.File]::WriteAllText(
            $temporaryPath,
            ($Manifest | ConvertTo-Json -Depth 4),
            [Text.UTF8Encoding]::new($false))

        if ([IO.File]::Exists($fullManifestPath)) {
            [IO.File]::Replace($temporaryPath, $fullManifestPath, $backupPath, $true)
        }
        else {
            [IO.File]::Move($temporaryPath, $fullManifestPath)
        }
        $published = $true
    }
    catch {
        if ([IO.File]::Exists($backupPath) -and -not [IO.File]::Exists($fullManifestPath)) {
            throw "Showcase manifest publication failed; the previous manifest is preserved at '$backupPath': $($_.Exception.Message)"
        }
        throw "Showcase manifest publication failed for '$fullManifestPath': $($_.Exception.Message)"
    }
    finally {
        if ([IO.File]::Exists($temporaryPath)) {
            [IO.File]::Delete($temporaryPath)
        }
        if ([IO.File]::Exists($backupPath) -and ($published -or [IO.File]::Exists($fullManifestPath))) {
            [IO.File]::Delete($backupPath)
        }
    }
}
