$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$debugExe = Join-Path $repoRoot "build\examples\sandbox3d\Debug\henka_sandbox3d.exe"
$releaseExe = Join-Path $repoRoot "build\examples\sandbox3d\Release\henka_sandbox3d.exe"

if (Test-Path $debugExe) {
    Push-Location (Split-Path $debugExe)
    try {
        & ".\henka_sandbox3d.exe"
        exit $LASTEXITCODE
    }
    finally {
        Pop-Location
    }
}

if (Test-Path $releaseExe) {
    Push-Location (Split-Path $releaseExe)
    try {
        & ".\henka_sandbox3d.exe"
        exit $LASTEXITCODE
    }
    finally {
        Pop-Location
    }
}

throw "Sandbox executable not found. Build the project first."
