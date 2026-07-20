param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "henka_script_common.ps1")

$repoRoot = Get-HenkaRepoRoot -ScriptDirectory $PSScriptRoot
$executable = Join-Path $repoRoot "build\examples\sandbox3d\$Configuration\henka_sandbox3d.exe"

if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "The $Configuration sandbox executable was not found. Run .\scripts\build_windows.ps1 -Configuration $Configuration first."
}

Invoke-HenkaNative `
    -FilePath $executable `
    -WorkingDirectory (Split-Path -Parent $executable) `
    -Label "Run Henka Engine Sandbox 3D $Configuration"
