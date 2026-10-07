[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$installer = Join-Path $PSScriptRoot "install_windows_software_opengl.ps1"
$setup = Join-Path $PSScriptRoot "setup_windows_software_opengl.ps1"
$fixture = New-HenkaTemporaryDirectory -RepositoryRoot $repoRoot -Purpose "software-opengl-path-resolution"
$localRoot = Get-HenkaLocalRoot -RepositoryRoot $repoRoot
$fixtureRelative = $fixture.Substring($localRoot.Length).TrimStart("\", "/")
$sourceRelative = Join-Path $fixtureRelative "source"
$targetRelative = Join-Path $fixtureRelative "target"
$foreignDirectory = Join-Path $fixture "foreign-current-directory"
$source = Join-Path $fixture "source"
$target = Join-Path $fixture "target"

$previousLocation = (Get-Location).Path
$previousCurrentDirectory = [Environment]::CurrentDirectory

try {
    $null = New-HenkaLocalDirectory -RepositoryRoot $repoRoot -Path $source
    $null = New-HenkaLocalDirectory -RepositoryRoot $repoRoot -Path $foreignDirectory

    [System.IO.File]::WriteAllBytes(
        (Join-Path $source "opengl32.dll"),
        [byte[]](0x48, 0x58, 0x01))
    [System.IO.File]::WriteAllBytes(
        (Join-Path $source "libgallium_wgl.dll"),
        [byte[]](0x48, 0x58, 0x02))

    Set-Location -LiteralPath $foreignDirectory
    [Environment]::CurrentDirectory = $foreignDirectory

    $resolvedTarget = Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $targetRelative
    if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals(
            [System.IO.Path]::GetFullPath($resolvedTarget),
            [System.IO.Path]::GetFullPath($target))) {
        throw "Canonical local-root path resolution followed the process working directory."
    }

    $global:LASTEXITCODE = 0
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer `
        -SourceDirectory $sourceRelative `
        -TargetDirectory $targetRelative

    if ($LASTEXITCODE -ne 0) {
        throw "App-local OpenGL installer failed with exit code $LASTEXITCODE."
    }

    if (-not (Test-Path -LiteralPath (Join-Path $target "opengl32.dll") -PathType Leaf)) {
        throw "The installer did not place the OpenGL runtime under the canonical local-root target."
    }

    $wrongTarget = [System.IO.Path]::GetFullPath(
        (Join-Path $foreignDirectory $targetRelative))
    if (Test-Path -LiteralPath $wrongTarget) {
        throw "The installer wrote a repository-relative target under the process working directory: $wrongTarget"
    }

    $setupText = [System.IO.File]::ReadAllText($setup)
    $expectedSetupBinding = 'Resolve-HenkaLocalPath -RepositoryRoot $repoRoot -Path $target'
    if (-not $setupText.Contains($expectedSetupBinding)) {
        throw "The Mesa setup path does not use the canonical Henka local-root resolver."
    }

    $global:LASTEXITCODE = 0
    Write-Host "[pass] OpenGL validation paths remain anchored to canonical _local even when the process working directory is elsewhere."
}
finally {
    [Environment]::CurrentDirectory = $previousCurrentDirectory
    Set-Location -LiteralPath $previousLocation
    if (Test-Path -LiteralPath $fixture) {
        Remove-Item -LiteralPath $fixture -Recurse -Force
    }
}
