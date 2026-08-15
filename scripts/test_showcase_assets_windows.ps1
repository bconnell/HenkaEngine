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
    if ($json.PSObject.Properties.Name -notcontains "images" -or
        $json.PSObject.Properties.Name -notcontains "textures" -or
        $json.images.Count -ne 2 -or $json.textures.Count -ne 2) {
        throw "Showcase asset $name is missing its bounded normal and metallic/roughness texture dependencies."
    }
    foreach ($image in $json.images) {
        if ([IO.Path]::IsPathRooted([string]$image.uri) -or
            [string]::IsNullOrWhiteSpace([string]$image.uri)) {
            throw "Showcase asset $name contains an invalid texture URI."
        }
        $imagePath = Join-Path $OutputDirectory ([string]$image.uri)
        if (-not (Test-Path -LiteralPath $imagePath -PathType Leaf)) {
            throw "Showcase asset $name is missing generated texture $($image.uri)."
        }
    }
    foreach ($primitive in $json.meshes[0].primitives) {
        if ($primitive.material -lt 0 -or $primitive.material -ge $json.materials.Count) {
            throw "Showcase asset $name has an invalid primitive material index."
        }
    }
}

function Get-PositionBounds {
    param(
        [Parameter(Mandatory = $true)][object]$Gltf,
        [Parameter(Mandatory = $true)][byte[]]$Binary,
        [Parameter(Mandatory = $true)][object]$Primitive
    )
    $accessor = $Gltf.accessors[$Primitive.attributes.POSITION]
    $bufferView = $Gltf.bufferViews[$accessor.bufferView]
    $stride = if ($bufferView.PSObject.Properties.Name -contains "byteStride") {
        [int]$bufferView.byteStride
    }
    else {
        12
    }
    $baseOffset = [int]$bufferView.byteOffset + [int]$accessor.byteOffset
    $minimum = @([double]::PositiveInfinity, [double]::PositiveInfinity, [double]::PositiveInfinity)
    $maximum = @([double]::NegativeInfinity, [double]::NegativeInfinity, [double]::NegativeInfinity)
    for ($index = 0; $index -lt [int]$accessor.count; ++$index) {
        $offset = $baseOffset + ($index * $stride)
        $position = @(
            [double][BitConverter]::ToSingle($Binary, $offset),
            [double][BitConverter]::ToSingle($Binary, $offset + 4),
            [double][BitConverter]::ToSingle($Binary, $offset + 8))
        for ($axis = 0; $axis -lt 3; ++$axis) {
            $minimum[$axis] = [Math]::Min($minimum[$axis], $position[$axis])
            $maximum[$axis] = [Math]::Max($maximum[$axis], $position[$axis])
        }
    }
    return [pscustomobject]@{ Minimum = $minimum; Maximum = $maximum }
}

$giraffe = Get-Content -LiteralPath (Join-Path $OutputDirectory "cheeky_giraffe.gltf") -Raw | ConvertFrom-Json
$giraffeMaterialNames = @($giraffe.materials | ForEach-Object { $_.name })
foreach ($requiredName in @("Giraffe Eye White", "Giraffe Iris", "Giraffe Eye Detail", "Giraffe Ear Inner", "Giraffe Ossicone Cap")) {
    if ($giraffeMaterialNames -notcontains $requiredName) {
        throw "Showcase giraffe is missing authored feature material '$requiredName'."
    }
}
$giraffeNormalBindings = @($giraffe.materials | Where-Object { $_.PSObject.Properties.Name -contains "normalTexture" })
if ($giraffeNormalBindings.Count -lt 5) {
    throw "Showcase giraffe does not exercise enough generated normal-map material bindings."
}
$giraffeVertexCount = 0
foreach ($primitive in $giraffe.meshes[0].primitives) {
    $giraffeVertexCount += [int]$giraffe.accessors[$primitive.attributes.POSITION].count
}
if ($giraffeVertexCount -lt 9000) {
    throw "Showcase giraffe lost its bounded curved-form topology density."
}
foreach ($material in $giraffe.materials) {
    if ($material.PSObject.Properties.Name -contains "normalTexture" -and
        [double]$material.normalTexture.scale -gt 0.20) {
        throw "Showcase giraffe detail-normal scale is too strong for restrained surface response."
    }
}
if ($giraffe.meshes[0].primitives.Count -lt 8) {
    throw "Showcase giraffe does not contain enough independently shaded feature geometry."
}
$giraffeBinary = [IO.File]::ReadAllBytes((Join-Path $OutputDirectory "cheeky_giraffe.bin"))
$earPrimitive = @($giraffe.meshes[0].primitives | Where-Object { $_.material -eq 7 })[0]
$earBounds = Get-PositionBounds -Gltf $giraffe -Binary $giraffeBinary -Primitive $earPrimitive
$earYSpan = $earBounds.Maximum[1] - $earBounds.Minimum[1]
$earZSpan = $earBounds.Maximum[2] - $earBounds.Minimum[2]
if ($earYSpan -lt 0.20 -or $earZSpan -gt 0.10) {
    throw "Showcase giraffe inner ears are not flattened, head-plane features."
}
$rocket = Get-Content -LiteralPath (Join-Path $OutputDirectory "original_realistic_rocket.gltf") -Raw | ConvertFrom-Json
$rocketMaterialNames = @($rocket.materials | ForEach-Object { $_.name })
if ($rocketMaterialNames -notcontains "Rocket Avionics") {
    throw "Showcase rocket is missing its stage-separation/avionics material."
}
$rocketNormalBindings = @($rocket.materials | Where-Object { $_.PSObject.Properties.Name -contains "normalTexture" })
if ($rocketNormalBindings.Count -ne $rocket.materials.Count) {
    throw "Showcase rocket does not bind generated normal detail across all material regions."
}
$rocketVertexCount = 0
foreach ($primitive in $rocket.meshes[0].primitives) {
    $rocketVertexCount += [int]$rocket.accessors[$primitive.attributes.POSITION].count
}
if ($rocketVertexCount -lt 3500) {
    throw "Showcase rocket lost its continuous tapered-form topology density."
}
foreach ($material in $rocket.materials) {
    if ([double]$material.normalTexture.scale -gt 0.20) {
        throw "Showcase rocket detail-normal scale is too strong for restrained surface response."
    }
}
if ($rocket.meshes[0].primitives.Count -lt 5) {
    throw "Showcase rocket does not contain enough independently shaded stage and engine geometry."
}
Write-Host "[pass] Deterministic showcase geometry, material ownership, and generated glTF contracts passed."
