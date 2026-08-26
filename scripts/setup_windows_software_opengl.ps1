param(
    [string]$CacheDirectory = "build\ci\mesa3d-26.1.7",
    [string[]]$TargetDirectory = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$cacheRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $CacheDirectory))
$archiveName = "mesa3d-26.1.7-release-msvc.7z"
$archivePath = Join-Path $cacheRoot $archiveName
$extractRoot = Join-Path $cacheRoot "extracted"
$downloadUri = "https://github.com/pal1000/mesa-dist-win/releases/download/26.1.7/$archiveName"
$expectedSha256 = "c6e90c3117233b66f7816df05026a5fb0f88eaf7829bd07a1724b981487ec0bb"

function Get-SevenZipPath {
    $command = Get-Command 7z.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    foreach ($candidate in @(
        (Join-Path ${env:ProgramFiles} "7-Zip\7z.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "7-Zip\7z.exe"),
        "C:\ProgramData\chocolatey\bin\7z.exe"
    )) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    throw "7-Zip was not found on the hosted Windows runner; cannot extract the pinned Mesa runtime."
}

function Install-SoftwareOpenGLRuntime {
    param([Parameter(Mandatory = $true)][string]$SourceDirectory, [Parameter(Mandatory = $true)][string]$DestinationDirectory)

    $source = [System.IO.Path]::GetFullPath($SourceDirectory)
    $destination = [System.IO.Path]::GetFullPath($DestinationDirectory)
    if (-not (Test-Path -LiteralPath $source -PathType Container)) {
        throw "Mesa runtime directory was not found: $source"
    }
    [System.IO.Directory]::CreateDirectory($destination) | Out-Null
    $dlls = @(Get-ChildItem -LiteralPath $source -Filter "*.dll" -File)
    if ($dlls.Count -eq 0 -or -not (Test-Path -LiteralPath (Join-Path $source "opengl32.dll") -PathType Leaf)) {
        throw "Mesa runtime directory does not contain the required app-local OpenGL DLL set: $source"
    }
    foreach ($dll in $dlls) {
        Copy-Item -LiteralPath $dll.FullName -Destination (Join-Path $destination $dll.Name) -Force
    }
    Write-Host "Installed CI-only Mesa OpenGL runtime into $destination"
}

[System.IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
    Write-Host "Downloading pinned Mesa3D Windows runtime: $downloadUri"
    Invoke-WebRequest -Uri $downloadUri -OutFile $archivePath -UseBasicParsing
}
$actualSha256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSha256 -ne $expectedSha256) {
    throw "Pinned Mesa3D runtime hash mismatch. Expected $expectedSha256, got $actualSha256."
}

$driverDirectories = @()
if (Test-Path -LiteralPath $extractRoot -PathType Container) {
    $driverDirectories = @(Get-ChildItem -LiteralPath $extractRoot -Recurse -Filter "opengl32.dll" -File |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.DirectoryName "libgallium_wgl.dll") } |
        Select-Object -ExpandProperty DirectoryName)
}
if ($driverDirectories.Count -eq 0) {
    $sevenZip = Get-SevenZipPath
    [System.IO.Directory]::CreateDirectory($extractRoot) | Out-Null
    & $sevenZip x $archivePath "-o$extractRoot" -y | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Pinned Mesa3D runtime extraction failed with exit code $LASTEXITCODE."
    }
    $driverDirectories = @(Get-ChildItem -LiteralPath $extractRoot -Recurse -Filter "opengl32.dll" -File |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.DirectoryName "libgallium_wgl.dll") } |
        Select-Object -ExpandProperty DirectoryName)
}
if ($driverDirectories.Count -ne 1) {
    throw "Expected exactly one Mesa desktop OpenGL driver directory, found $($driverDirectories.Count)."
}
$driverDirectory = [System.IO.Path]::GetFullPath($driverDirectories[0])

# This is intentionally an app-local CI dependency. It never changes the host
# OpenGL registration or the shipped Henka package contents.
$env:HENKA_CI_SOFTWARE_OPENGL_ROOT = $driverDirectory
$env:GALLIUM_DRIVER = "llvmpipe"
if (-not [string]::IsNullOrWhiteSpace([string]$env:GITHUB_ENV)) {
    Add-Content -LiteralPath $env:GITHUB_ENV -Value "HENKA_CI_SOFTWARE_OPENGL_ROOT=$driverDirectory"
    Add-Content -LiteralPath $env:GITHUB_ENV -Value "GALLIUM_DRIVER=llvmpipe"
}
if (-not [string]::IsNullOrWhiteSpace([string]$env:GITHUB_PATH)) {
    Add-Content -LiteralPath $env:GITHUB_PATH -Value $driverDirectory
}

foreach ($target in $TargetDirectory) {
    Install-SoftwareOpenGLRuntime -SourceDirectory $driverDirectory -DestinationDirectory $target
}

Write-Host "[pass] Pinned Mesa3D llvmpipe runtime is ready: $driverDirectory"
