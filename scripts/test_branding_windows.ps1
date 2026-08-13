param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot,

    [string]$PackageRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = [System.IO.Path]::GetFullPath($RepositoryRoot)
$brandingRoot = Join-Path $root "assets\branding"
$lockupPath = Join-Path $brandingRoot "henka_engine_lockup.png"
$emblemPath = Join-Path $brandingRoot "henka_engine_emblem.png"

function Get-PngMetadata {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 24 -or
        $bytes[0] -ne 137 -or $bytes[1] -ne 80 -or $bytes[2] -ne 78 -or
        $bytes[3] -ne 71 -or $bytes[4] -ne 13 -or $bytes[5] -ne 10 -or
        $bytes[6] -ne 26 -or $bytes[7] -ne 10) {
        throw "Branding asset is not a PNG: $Path"
    }

    $width = [System.Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt32($bytes, 16))
    $height = [System.Net.IPAddress]::NetworkToHostOrder([BitConverter]::ToInt32($bytes, 20))
    if ($width -le 0 -or $height -le 0) {
        throw "Branding asset has invalid dimensions: $Path"
    }

    return @{ Width = $width; Height = $height }
}

foreach ($path in @($lockupPath, $emblemPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required repository branding asset is missing: $path"
    }
    $metadata = Get-PngMetadata -Path $path
    if ($metadata.Width -gt 4096 -or $metadata.Height -gt 4096) {
        throw "Branding asset exceeds the bounded runtime dimensions: $path"
    }
}

Add-Type -AssemblyName System.Drawing
$lockup = [System.Drawing.Bitmap]::FromFile($lockupPath)
$emblem = [System.Drawing.Bitmap]::FromFile($emblemPath)
try {
    if ($lockup.Width -ne $lockup.Height -or $emblem.Width -le 0 -or $emblem.Height -le 0) {
        throw "Branding assets do not have valid presentation dimensions."
    }

    $lockupAspect = [double]$lockup.Width / [double]$lockup.Height
    $emblemAspect = [double]$emblem.Width / [double]$emblem.Height
    if ([double]::IsNaN($lockupAspect) -or [double]::IsInfinity($lockupAspect) -or
        [double]::IsNaN($emblemAspect) -or [double]::IsInfinity($emblemAspect) -or
        $lockupAspect -le 0.0 -or $emblemAspect -le 0.0) {
        throw "Branding aspect-ratio validation failed."
    }
} finally {
    $lockup.Dispose()
    $emblem.Dispose()
}

if (-not [string]::IsNullOrWhiteSpace($PackageRoot)) {
    $package = [System.IO.Path]::GetFullPath($PackageRoot)
    foreach ($relative in @(
        "assets\branding\henka_engine_emblem.png",
        "assets\branding\henka_engine_lockup.png")) {
        if (-not (Test-Path -LiteralPath (Join-Path $package $relative) -PathType Leaf)) {
            throw "Packaged branding asset is missing: $relative"
        }
    }
}

Write-Output "branding assets validated: lockup and emblem"
