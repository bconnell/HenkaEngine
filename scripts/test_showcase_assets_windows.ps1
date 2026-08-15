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
$giraffe = Get-Content -LiteralPath (Join-Path $OutputDirectory "cheeky_giraffe.gltf") -Raw | ConvertFrom-Json
$giraffeMaterialNames = @($giraffe.materials | ForEach-Object { $_.name })
foreach ($requiredName in @("Giraffe Eye White", "Giraffe Iris", "Giraffe Eye Detail", "Giraffe Ear Inner", "Giraffe Ossicone Cap")) {
    if ($giraffeMaterialNames -notcontains $requiredName) {
        throw "Showcase giraffe is missing authored feature material '$requiredName'."
    }
}
if ($giraffe.meshes[0].primitives.Count -lt 8) {
    throw "Showcase giraffe does not contain enough independently shaded feature geometry."
}
$rocket = Get-Content -LiteralPath (Join-Path $OutputDirectory "original_realistic_rocket.gltf") -Raw | ConvertFrom-Json
$rocketMaterialNames = @($rocket.materials | ForEach-Object { $_.name })
if ($rocketMaterialNames -notcontains "Rocket Avionics") {
    throw "Showcase rocket is missing its stage-separation/avionics material."
}
if ($rocket.meshes[0].primitives.Count -lt 5) {
    throw "Showcase rocket does not contain enough independently shaded stage and engine geometry."
}
Write-Host "[pass] Deterministic showcase geometry, material ownership, and generated glTF contracts passed."
