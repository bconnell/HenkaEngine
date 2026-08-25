param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$repoRoot = Split-Path -Parent $PSScriptRoot
$generator = Join-Path $repoRoot "scripts\generate_showcase_assets.ps1"
if (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
    throw "OutputDirectory must be absolute."
}
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null

& $generator -OutputDirectory $OutputDirectory
if ($LASTEXITCODE -ne 0) {
    throw "Showcase generator failed with exit code $LASTEXITCODE."
}
$determinismDirectory = Join-Path $OutputDirectory "determinism_second_run"
try {
    if (Test-Path -LiteralPath $determinismDirectory) {
        Remove-Item -LiteralPath $determinismDirectory -Recurse -Force
    }
    [IO.Directory]::CreateDirectory($determinismDirectory) | Out-Null
    & $generator -OutputDirectory $determinismDirectory
    if ($LASTEXITCODE -ne 0) {
        throw "Showcase generator determinism rerun failed with exit code $LASTEXITCODE."
    }
    foreach ($deterministicFile in @(
            "cheeky_giraffe.gltf",
            "cheeky_giraffe.bin",
            "giraffe_base_color.png",
            "giraffe_detail_normal.png",
            "giraffe_metallic_roughness.png",
            "original_realistic_rocket.gltf",
            "original_realistic_rocket.bin",
            "rocket_base_color.png",
            "rocket_detail_normal.png",
            "rocket_metallic_roughness.png")) {
        $firstHash = (Get-FileHash -LiteralPath (Join-Path $OutputDirectory $deterministicFile) -Algorithm SHA256).Hash
        $secondHash = (Get-FileHash -LiteralPath (Join-Path $determinismDirectory $deterministicFile) -Algorithm SHA256).Hash
        if ($firstHash -ne $secondHash) {
            throw "Showcase generator output is not deterministic for '$deterministicFile'."
        }
    }
}
finally {
    if (Test-Path -LiteralPath $determinismDirectory) {
        Remove-Item -LiteralPath $determinismDirectory -Recurse -Force
    }
}
foreach ($name in @("cheeky_giraffe", "original_realistic_rocket")) {
    $path = Join-Path $OutputDirectory ($name + ".gltf")
    $json = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    if ($null -eq $json.materials -or $json.materials.Count -eq 0 -or
        $null -eq $json.meshes -or $json.meshes.Count -ne 1) {
        throw "Showcase asset $name has no bounded material/mesh contract."
    }
    if ($json.PSObject.Properties.Name -notcontains "images" -or
        $json.PSObject.Properties.Name -notcontains "textures" -or
        (($name -eq "cheeky_giraffe" -and ($json.images.Count -ne 3 -or $json.textures.Count -ne 3)) -or
         ($name -ne "cheeky_giraffe" -and ($json.images.Count -ne 3 -or $json.textures.Count -ne 3)))) {
        throw "Showcase asset $name is missing its bounded material texture dependencies."
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

function Get-ImageChannelAverage {
    param([Parameter(Mandatory = $true)][string]$Path)
    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        $redTotal = [Int64]0
        $greenTotal = [Int64]0
        $blueTotal = [Int64]0
        $pixelCount = $bitmap.Width * $bitmap.Height
        for ($y = 0; $y -lt $bitmap.Height; ++$y) {
            for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                $pixel = $bitmap.GetPixel($x, $y)
                $redTotal += $pixel.R
                $greenTotal += $pixel.G
                $blueTotal += $pixel.B
            }
        }
        return [pscustomobject]@{
            Red = [double]$redTotal / $pixelCount
            Green = [double]$greenTotal / $pixelCount
            Blue = [double]$blueTotal / $pixelCount
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-DarkPixelRowCoverage {
    param([Parameter(Mandatory = $true)][string]$Path)
    $bitmap = [System.Drawing.Bitmap]::new($Path)
    try {
        $darkPixelCount = 0
        $rowMaximum = 0.0
        for ($y = 0; $y -lt $bitmap.Height; ++$y) {
            $rowDarkPixelCount = 0
            for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                $pixel = $bitmap.GetPixel($x, $y)
                if ($pixel.R -lt 130 -and $pixel.G -lt 85) {
                    ++$darkPixelCount
                    ++$rowDarkPixelCount
                }
            }
            $rowMaximum = [Math]::Max($rowMaximum, $rowDarkPixelCount / [double]$bitmap.Width)
        }
        return [pscustomobject]@{
            Total = $darkPixelCount / [double]($bitmap.Width * $bitmap.Height)
            RowMaximum = $rowMaximum
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

$giraffe = Get-Content -LiteralPath (Join-Path $OutputDirectory "cheeky_giraffe.gltf") -Raw | ConvertFrom-Json
$giraffeMaterialNames = @($giraffe.materials | ForEach-Object { $_.name })
$giraffeBinary = [IO.File]::ReadAllBytes((Join-Path $OutputDirectory "cheeky_giraffe.bin"))
foreach ($requiredName in @("Giraffe Eye White", "Giraffe Iris", "Giraffe Eye Detail", "Giraffe Ear Inner", "Giraffe Ossicone Cap", "Giraffe Hoof", "Giraffe Mane", "Giraffe Nose", "Giraffe Joint")) {
    if ($giraffeMaterialNames -notcontains $requiredName) {
        throw "Showcase giraffe is missing authored feature material '$requiredName'."
    }
}
$giraffeNormalBindings = @($giraffe.materials | Where-Object { $_.PSObject.Properties.Name -contains "normalTexture" })
if ($giraffeNormalBindings.Count -lt 5) {
    throw "Showcase giraffe does not exercise enough generated normal-map material bindings."
}
$giraffeTan = @($giraffe.materials | Where-Object { $_.name -eq "Giraffe Tan" })[0]
if ($null -eq $giraffeTan -or
    $giraffeTan.pbrMetallicRoughness.PSObject.Properties.Name -notcontains "baseColorTexture" -or
    [int]$giraffeTan.pbrMetallicRoughness.baseColorTexture.index -ne 2) {
    throw "Showcase giraffe does not bind its deterministic flush spot base-color texture."
}
$giraffeEyeMaterial = @($giraffe.materials | Where-Object { $_.name -eq "Giraffe Eye White" })[0]
if ($null -eq $giraffeEyeMaterial -or
    [double]$giraffeEyeMaterial.pbrMetallicRoughness.roughnessFactor -ge 0.38) {
    throw "Showcase giraffe eye material would incorrectly enter the body-only subsurface and normal-texture evidence path."
}
$giraffeVertexCount = 0
foreach ($primitive in $giraffe.meshes[0].primitives) {
    $giraffeVertexCount += [int]$giraffe.accessors[$primitive.attributes.POSITION].count
}
if ($giraffeVertexCount -lt 16000) {
    throw "Showcase giraffe lost its bounded anatomical topology detail."
}
$giraffeTanPrimitive = @($giraffe.meshes[0].primitives | Where-Object { $_.material -eq 0 })[0]
$giraffeTanVertexCount = [int]$giraffe.accessors[$giraffeTanPrimitive.attributes.POSITION].count
if ($giraffeTanVertexCount -lt 8500) {
    throw "Showcase giraffe limb topology did not retain the curved multi-profile attachment detail."
}
$giraffeTanBounds = Get-PositionBounds -Gltf $giraffe -Binary $giraffeBinary -Primitive $giraffeTanPrimitive
$giraffeTanWidth = $giraffeTanBounds.Maximum[0] - $giraffeTanBounds.Minimum[0]
$giraffeTanHeight = $giraffeTanBounds.Maximum[1] - $giraffeTanBounds.Minimum[1]
$giraffeTanDepth = $giraffeTanBounds.Maximum[2] - $giraffeTanBounds.Minimum[2]
if ($giraffeTanWidth -gt 1.90 -or $giraffeTanHeight -lt 4.35 -or
    $giraffeTanDepth -lt 2.50 -or ($giraffeTanHeight / $giraffeTanDepth) -gt 1.95) {
    throw "Showcase giraffe silhouette lost its long-bodied, forward-necked quadruped proportion contract."
}
$giraffeHideAverage = Get-ImageChannelAverage (Join-Path $OutputDirectory "giraffe_base_color.png")
if ($giraffeHideAverage.Green -ge 205.0 -or $giraffeHideAverage.Blue -ge 150.0) {
    throw "Showcase giraffe hide does not use the restrained ochre palette required for an anatomical study."
}
$giraffeHideCoverage = Get-DarkPixelRowCoverage (Join-Path $OutputDirectory "giraffe_base_color.png")
if ($giraffeHideCoverage.Total -lt 0.15 -or $giraffeHideCoverage.Total -gt 0.45 -or
    $giraffeHideCoverage.RowMaximum -gt 0.50) {
    throw "Showcase giraffe hide contains an implausible dark-patch band or coverage distribution."
}
foreach ($material in $giraffe.materials) {
    if ($material.PSObject.Properties.Name -contains "normalTexture" -and
        [double]$material.normalTexture.scale -gt 0.20) {
        throw "Showcase giraffe detail-normal scale is too strong for restrained surface response."
    }
}
if ($giraffe.meshes[0].primitives.Count -lt 13) {
    throw "Showcase giraffe does not contain enough independently shaded anatomical feature geometry."
}
$earPrimitive = @($giraffe.meshes[0].primitives | Where-Object { $_.material -eq 7 })[0]
$earBounds = Get-PositionBounds -Gltf $giraffe -Binary $giraffeBinary -Primitive $earPrimitive
$earYSpan = $earBounds.Maximum[1] - $earBounds.Minimum[1]
$earZSpan = $earBounds.Maximum[2] - $earBounds.Minimum[2]
if ($earYSpan -lt 0.20 -or $earZSpan -gt 0.10) {
    throw "Showcase giraffe inner ears are not flattened, head-plane features."
}
$smilePrimitive = @($giraffe.meshes[0].primitives | Where-Object { $_.material -eq 6 })[0]
$smileBounds = Get-PositionBounds -Gltf $giraffe -Binary $giraffeBinary -Primitive $smilePrimitive
$smileWidth = $smileBounds.Maximum[0] - $smileBounds.Minimum[0]
$smileHeight = $smileBounds.Maximum[1] - $smileBounds.Minimum[1]
if ($smileWidth -gt 0.36 -or $smileHeight -gt 0.05) {
    throw "Showcase giraffe mouth detail is too expressive or deep for the restrained face contract."
}
$giraffeHoofMaterialIndex = [array]::IndexOf($giraffeMaterialNames, "Giraffe Hoof")
$giraffeHoofPrimitives = @($giraffe.meshes[0].primitives | Where-Object { $_.material -eq $giraffeHoofMaterialIndex })
if ($giraffeHoofMaterialIndex -lt 0 -or $giraffeHoofPrimitives.Count -ne 1) {
    throw "Showcase giraffe is missing independently shaded hoof geometry for all four legs."
}
$giraffeHoofBounds = Get-PositionBounds -Gltf $giraffe -Binary $giraffeBinary -Primitive $giraffeHoofPrimitives[0]
if (($giraffeHoofBounds.Maximum[0] - $giraffeHoofBounds.Minimum[0]) -lt 0.90 -or
    ($giraffeHoofBounds.Maximum[2] - $giraffeHoofBounds.Minimum[2]) -lt 1.25) {
    throw "Showcase giraffe hoof geometry does not establish the separated fore/hind grounded stance."
}
$giraffeManeMaterialIndex = [array]::IndexOf($giraffeMaterialNames, "Giraffe Mane")
$giraffeManePrimitive = @($giraffe.meshes[0].primitives | Where-Object { $_.material -eq $giraffeManeMaterialIndex })[0]
if ($giraffeManeMaterialIndex -lt 0 -or $null -eq $giraffeManePrimitive) {
    throw "Showcase giraffe is missing its separately shaded mane region."
}
$giraffeNoseMaterialIndex = [array]::IndexOf($giraffeMaterialNames, "Giraffe Nose")
$giraffeNosePrimitives = @($giraffe.meshes[0].primitives | Where-Object { $_.material -eq $giraffeNoseMaterialIndex })
if ($giraffeNoseMaterialIndex -lt 0 -or $giraffeNosePrimitives.Count -ne 1) {
    throw "Showcase giraffe is missing paired nostril geometry."
}
$giraffeNoseBounds = Get-PositionBounds -Gltf $giraffe -Binary $giraffeBinary -Primitive $giraffeNosePrimitives[0]
if (($giraffeNoseBounds.Maximum[0] - $giraffeNoseBounds.Minimum[0]) -lt 0.16) {
    throw "Showcase giraffe nostril geometry does not contain a paired face detail."
}
$giraffeEyePrimitive = @($giraffe.meshes[0].primitives | Where-Object { $_.material -eq 3 })[0]
$giraffeEyeBounds = Get-PositionBounds -Gltf $giraffe -Binary $giraffeBinary -Primitive $giraffeEyePrimitive
if (($giraffeEyeBounds.Maximum[0] - $giraffeEyeBounds.Minimum[0]) -gt 0.48) {
    throw "Showcase giraffe eyes remain too wide-set or oversized for the restrained anatomical face."
}
$rocket = Get-Content -LiteralPath (Join-Path $OutputDirectory "original_realistic_rocket.gltf") -Raw | ConvertFrom-Json
$rocketBinary = [IO.File]::ReadAllBytes((Join-Path $OutputDirectory "original_realistic_rocket.bin"))
$rocketMaterialNames = @($rocket.materials | ForEach-Object { $_.name })
if ($rocketMaterialNames -notcontains "Rocket Avionics" -or
    $rocketMaterialNames -notcontains "Rocket Launch Pad") {
    throw "Showcase rocket is missing its stage-separation/avionics or launch-pad material."
}
$rocketNormalBindings = @($rocket.materials | Where-Object { $_.PSObject.Properties.Name -contains "normalTexture" })
if ($rocketNormalBindings.Count -ne $rocket.materials.Count) {
    throw "Showcase rocket does not bind generated normal detail across all material regions."
}
$rocketVertexCount = 0
foreach ($primitive in $rocket.meshes[0].primitives) {
    $rocketVertexCount += [int]$rocket.accessors[$primitive.attributes.POSITION].count
}
if ($rocketVertexCount -lt 7800) {
    throw "Showcase rocket lost its bounded mechanical topology detail."
}
foreach ($material in $rocket.materials) {
    if ([double]$material.normalTexture.scale -gt 0.20) {
        throw "Showcase rocket detail-normal scale is too strong for restrained surface response."
    }
}
if ($rocket.meshes[0].primitives.Count -lt 12) {
    throw "Showcase rocket does not contain enough independently shaded heavy-lift, engine, and ground-support geometry."
}
$rocketCorePrimitive = @($rocket.meshes[0].primitives | Where-Object { $_.material -eq 0 })[0]
$rocketCoreBounds = Get-PositionBounds -Gltf $rocket -Binary $rocketBinary -Primitive $rocketCorePrimitive
$rocketCoreInsulationMaterialIndex = [array]::IndexOf($rocketMaterialNames, "Rocket Core Insulation")
$rocketCoreInsulationPrimitive = @($rocket.meshes[0].primitives | Where-Object { $_.material -eq $rocketCoreInsulationMaterialIndex })[0]
$rocketCoreInsulationBounds = Get-PositionBounds -Gltf $rocket -Binary $rocketBinary -Primitive $rocketCoreInsulationPrimitive
$rocketCoreMinimumY = [Math]::Min($rocketCoreBounds.Minimum[1], $rocketCoreInsulationBounds.Minimum[1])
$rocketCoreMaximumY = [Math]::Max($rocketCoreBounds.Maximum[1], $rocketCoreInsulationBounds.Maximum[1])
$rocketCoreMinimumX = [Math]::Min($rocketCoreBounds.Minimum[0], $rocketCoreInsulationBounds.Minimum[0])
$rocketCoreMaximumX = [Math]::Max($rocketCoreBounds.Maximum[0], $rocketCoreInsulationBounds.Maximum[0])
$rocketCoreHeight = $rocketCoreMaximumY - $rocketCoreMinimumY
if ($rocketCoreHeight -lt 7.5 -or
    ($rocketCoreHeight / ($rocketCoreMaximumX - $rocketCoreMinimumX)) -lt 3.0) {
    throw "Showcase rocket does not retain the tall pointed launch-vehicle proportion required to avoid a character-like silhouette."
}
$rocketPadMaterialIndex = [array]::IndexOf($rocketMaterialNames, "Rocket Launch Pad")
$rocketPadPrimitive = @($rocket.meshes[0].primitives | Where-Object { $_.material -eq $rocketPadMaterialIndex })[0]
$rocketPadBounds = Get-PositionBounds -Gltf $rocket -Binary $rocketBinary -Primitive $rocketPadPrimitive
$rocketPadWidth = $rocketPadBounds.Maximum[0] - $rocketPadBounds.Minimum[0]
$rocketPadDepth = $rocketPadBounds.Maximum[2] - $rocketPadBounds.Minimum[2]
if ($rocketPadWidth -lt 3.5 -or $rocketPadDepth -lt 3.5 -or $rocketPadBounds.Minimum[1] -ge 0.0) {
    throw "Showcase rocket launch pad is missing its bounded ground-contact assembly."
}
$rocketStripePrimitive = @($rocket.meshes[0].primitives | Where-Object { $_.material -eq 3 })[0]
$rocketStripeBounds = Get-PositionBounds -Gltf $rocket -Binary $rocketBinary -Primitive $rocketStripePrimitive
$rocketStripeWidth = $rocketStripeBounds.Maximum[0] - $rocketStripeBounds.Minimum[0]
$rocketStripeHeight = $rocketStripeBounds.Maximum[1] - $rocketStripeBounds.Minimum[1]
if ($rocketStripeWidth -lt 1.20 -or $rocketStripeHeight -lt 0.18) {
    throw "Showcase rocket mission stripe does not visibly wrap the painted core."
}
$rocketPaint = @($rocket.materials | Where-Object { $_.name -eq "Rocket Painted Ceramic" })[0]
if ($null -eq $rocketPaint -or
    $rocketPaint.pbrMetallicRoughness.PSObject.Properties.Name -notcontains "baseColorTexture" -or
    [int]$rocketPaint.pbrMetallicRoughness.baseColorTexture.index -ne 2) {
    throw "Showcase rocket does not bind its deterministic painted-surface base-color texture."
}
$rocketBaseColorBitmap = [System.Drawing.Bitmap]::new((Join-Path $OutputDirectory "rocket_base_color.png"))
try {
    $warmCamouflagePixels = 0
    for ($y = 0; $y -lt $rocketBaseColorBitmap.Height; ++$y) {
        for ($x = 0; $x -lt $rocketBaseColorBitmap.Width; ++$x) {
            $pixel = $rocketBaseColorBitmap.GetPixel($x, $y)
            if ($pixel.R -gt ($pixel.B * 1.20) -and $pixel.G -lt ($pixel.B * 0.80)) {
                ++$warmCamouflagePixels
            }
        }
    }
    if ($warmCamouflagePixels -ne 0) {
        throw "Showcase rocket base-color texture contains giraffe-style warm camouflage patches."
    }
}
finally {
    $rocketBaseColorBitmap.Dispose()
}
foreach ($requiredName in @("Rocket Fastener", "Rocket Thermal Detail", "Rocket Engine Bell")) {
    if ($rocketMaterialNames -notcontains $requiredName) {
        throw "Showcase rocket is missing authored mechanical feature material '$requiredName'."
    }
}
foreach ($requiredName in @("Rocket Panel Detail", "Rocket Core Insulation", "Rocket Booster Coating", "Rocket Service Structure")) {
    if ($rocketMaterialNames -notcontains $requiredName) {
        throw "Showcase rocket is missing required generic heavy-lift material '$requiredName'."
    }
}
if ([double]$rocket.materials[3].pbrMetallicRoughness.baseColorFactor[0] -gt 0.30) {
    throw "Showcase rocket mission marking remains too saturated for the utilitarian launch-vehicle material contract."
}
foreach ($requiredName in @("Rocket Fastener", "Rocket Thermal Detail", "Rocket Engine Bell", "Rocket Panel Detail")) {
    $materialIndex = [array]::IndexOf($rocketMaterialNames, $requiredName)
    if (@($rocket.meshes[0].primitives | Where-Object { $_.material -eq $materialIndex }).Count -lt 1) {
        throw "Showcase rocket is missing geometry for authored mechanical feature material '$requiredName'."
    }
}
$rocketBoosterMaterialIndex = [array]::IndexOf($rocketMaterialNames, "Rocket Booster Coating")
$rocketBoosterPrimitive = @($rocket.meshes[0].primitives | Where-Object { $_.material -eq $rocketBoosterMaterialIndex })[0]
$rocketBoosterBounds = Get-PositionBounds -Gltf $rocket -Binary $rocketBinary -Primitive $rocketBoosterPrimitive
if (($rocketBoosterBounds.Maximum[0] - $rocketBoosterBounds.Minimum[0]) -lt 3.0 -or
    ($rocketBoosterBounds.Maximum[1] - $rocketBoosterBounds.Minimum[1]) -lt 5.5) {
    throw "Showcase rocket booster geometry does not establish the wide twin-booster heavy-lift silhouette."
}
$rocketTowerMaterialIndex = [array]::IndexOf($rocketMaterialNames, "Rocket Service Structure")
$rocketTowerPrimitive = @($rocket.meshes[0].primitives | Where-Object { $_.material -eq $rocketTowerMaterialIndex })[0]
$rocketTowerBounds = Get-PositionBounds -Gltf $rocket -Binary $rocketBinary -Primitive $rocketTowerPrimitive
if (($rocketTowerBounds.Maximum[1] - $rocketTowerBounds.Minimum[1]) -lt 6.0 -or
    $rocketTowerBounds.Minimum[0] -gt -2.5) {
    throw "Showcase rocket is missing the bounded adjacent service-structure scale reference."
}
Write-Host "[pass] Deterministic showcase geometry, material ownership, and generated glTF contracts passed."
