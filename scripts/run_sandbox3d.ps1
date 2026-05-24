$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$debugExe = Join-Path $repoRoot "build\examples\sandbox3d\Debug\henka_sandbox3d.exe"
$releaseExe = Join-Path $repoRoot "build\examples\sandbox3d\Release\henka_sandbox3d.exe"

if (Test-Path $debugExe) {
    & $debugExe
    exit $LASTEXITCODE
}

if (Test-Path $releaseExe) {
    & $releaseExe
    exit $LASTEXITCODE
}

throw "Sandbox executable not found. Build the project first."
