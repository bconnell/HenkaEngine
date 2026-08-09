param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$generator = Join-Path $repoRoot "scripts\generate_showcase_assets.ps1"
if (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
    throw "OutputDirectory must be absolute."
}
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $generator -OutputDirectory $OutputDirectory
foreach ($name in @("cheeky_giraffe", "original_realistic_rocket")) {
    $path = Join-Path $OutputDirectory ($name + ".gltf")
    $json = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if ($null -eq $json.materials -or $json.materials.Count -eq 0 -or
        $null -eq $json.meshes -or $json.meshes.Count -ne 1) {
        throw "Showcase asset $name has no bounded material/mesh contract."
    }
    if ($json.PSObject.Properties.Name -contains "images" -or
        $json.PSObject.Properties.Name -contains "textures") {
        throw "Showcase asset $name unexpectedly binds a texture dependency."
    }
    foreach ($primitive in $json.meshes[0].primitives) {
        if ($primitive.material -lt 0 -or $primitive.material -ge $json.materials.Count) {
            throw "Showcase asset $name has an invalid primitive material index."
        }
    }
}
Write-Host "[pass] Deterministic showcase geometry, material ownership, and generated glTF contracts passed."
