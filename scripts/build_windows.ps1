$ErrorActionPreference = "Stop"

function Get-CMakePath {
    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmakeCommand) {
        return $cmakeCommand.Source
    }

    $fallback = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $fallback) {
        return $fallback
    }

    throw "CMake was not found on PATH or in the expected Visual Studio location."
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$cmake = Get-CMakePath

& $cmake -S $repoRoot -B (Join-Path $repoRoot "build")
& $cmake --build (Join-Path $repoRoot "build") --config Debug
