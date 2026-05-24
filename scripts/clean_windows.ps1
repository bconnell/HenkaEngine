$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$buildPath = Join-Path $repoRoot "build"
$outPath = Join-Path $repoRoot "out"

if (Test-Path $buildPath) {
    Remove-Item -LiteralPath $buildPath -Recurse -Force
}

if (Test-Path $outPath) {
    Write-Host "Removing the generated out folder, including any package-local user data."
    Remove-Item -LiteralPath $outPath -Recurse -Force
}
