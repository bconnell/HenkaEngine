[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$installer = Join-Path $PSScriptRoot "install_windows_software_opengl.ps1"
$setup = Join-Path $PSScriptRoot "setup_windows_software_opengl.ps1"
$fixtureName = "software-opengl-path-resolution-" + [Guid]::NewGuid().ToString("N")
$fixture = Join-Path $repoRoot ("build\test_tmp\" + $fixtureName)
$sourceRelative = "build\test_tmp\$fixtureName\source"
$targetRelative = "build\test_tmp\$fixtureName\target"
$foreignDirectory = Join-Path $fixture "foreign-current-directory"
$source = Join-Path $repoRoot $sourceRelative
$target = Join-Path $repoRoot $targetRelative

$previousLocation = (Get-Location).Path
$previousCurrentDirectory = [Environment]::CurrentDirectory

try {
    [System.IO.Directory]::CreateDirectory($source) | Out-Null
    [System.IO.Directory]::CreateDirectory($foreignDirectory) | Out-Null

    [System.IO.File]::WriteAllBytes(
        (Join-Path $source "opengl32.dll"),
        [byte[]](0x48, 0x58, 0x01))
    [System.IO.File]::WriteAllBytes(
        (Join-Path $source "libgallium_wgl.dll"),
        [byte[]](0x48, 0x58, 0x02))

    Set-Location -LiteralPath $foreignDirectory
    [Environment]::CurrentDirectory = $foreignDirectory

    $resolvedTarget = Resolve-HenkaRepositoryPath -RepoRoot $repoRoot -Path $targetRelative
    if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals(
            [System.IO.Path]::GetFullPath($resolvedTarget),
            [System.IO.Path]::GetFullPath($target))) {
        throw "Repository-relative path resolution followed the process working directory."
    }

    $global:LASTEXITCODE = 0
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer `
        -SourceDirectory $sourceRelative `
        -TargetDirectory $targetRelative

    if ($LASTEXITCODE -ne 0) {
        throw "App-local OpenGL installer failed with exit code $LASTEXITCODE."
    }

    if (-not (Test-Path -LiteralPath (Join-Path $target "opengl32.dll") -PathType Leaf)) {
        throw "The installer did not place the OpenGL runtime under the repository-root target."
    }

    $wrongTarget = [System.IO.Path]::GetFullPath(
        (Join-Path $foreignDirectory $targetRelative))
    if (Test-Path -LiteralPath $wrongTarget) {
        throw "The installer wrote a repository-relative target under the process working directory: $wrongTarget"
    }

    $setupText = [System.IO.File]::ReadAllText($setup)
    $expectedSetupBinding = 'Resolve-HenkaRepositoryPath -RepoRoot $repoRoot -Path $target'
    if (-not $setupText.Contains($expectedSetupBinding)) {
        throw "The Mesa setup path does not use the canonical repository-relative resolver."
    }

    $global:LASTEXITCODE = 0
    Write-Host "[pass] Repository-relative OpenGL validation paths remain anchored to the Henka checkout even when the process working directory is elsewhere."
}
finally {
    [Environment]::CurrentDirectory = $previousCurrentDirectory
    Set-Location -LiteralPath $previousLocation
    if (Test-Path -LiteralPath $fixture) {
        Remove-Item -LiteralPath $fixture -Recurse -Force
    }
}