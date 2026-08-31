param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$sourceDirectory = Join-Path $repoRoot "assets\textures"
$textureCount = 65

if (-not [System.IO.Path]::IsPathRooted($OutputDirectory)) {
    throw "OutputDirectory must be absolute."
}

$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null

for ($index = 0; $index -lt $textureCount; $index++) {
    $sourceName = if (($index % 2) -eq 0) {
        "cube_albedo.png"
    } else {
        "ground_checker.png"
    }
    $sourcePath = Join-Path $sourceDirectory $sourceName
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Residency fixture source is missing: $sourcePath"
    }

    $destinationPath = Join-Path $OutputDirectory ("residency_{0}.png" -f $index)
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath -Force
}

$generatedFiles = @(Get-ChildItem -LiteralPath $OutputDirectory -Filter "residency_*.png" -File)
if ($generatedFiles.Count -ne $textureCount) {
    throw "Expected exactly $textureCount residency fixtures, found $($generatedFiles.Count)."
}

Write-Output "[pass] Generated $textureCount opt-in residency fixtures at $OutputDirectory."
