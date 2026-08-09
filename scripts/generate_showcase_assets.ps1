param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function New-Part {
    param([int]$Material)
    return [pscustomobject]@{
        Material = $Material
        Vertices = New-Object 'System.Collections.Generic.List[object]'
        Indices = New-Object 'System.Collections.Generic.List[int]'
    }
}

function Add-Vertex {
    param(
        [object]$Part,
        [float[]]$Position,
        [float[]]$Normal,
        [float[]]$Uv,
        [float[]]$Tangent
    )
    $index = $Part.Vertices.Count
    [void]$Part.Vertices.Add([pscustomobject]@{
        Position = $Position
        Normal = $Normal
        Uv = $Uv
        Tangent = $Tangent
    })
    return $index
}

function Add-Triangle {
    param([object]$Part, [int]$A, [int]$B, [int]$C)
    [void]$Part.Indices.Add($A)
    [void]$Part.Indices.Add($B)
    [void]$Part.Indices.Add($C)
}

function Add-Ellipsoid {
    param(
        [object]$Part,
        [float[]]$Center,
        [float[]]$Radius,
        [int]$Rings = 12,
        [int]$Segments = 24
    )
    $row = New-Object 'System.Collections.Generic.List[int]'
    for ($ring = 0; $ring -le $Rings; ++$ring) {
        $theta = [Math]::PI * $ring / $Rings
        $sinTheta = [Math]::Sin($theta)
        $cosTheta = [Math]::Cos($theta)
        for ($segment = 0; $segment -le $Segments; ++$segment) {
            $phi = 2.0 * [Math]::PI * $segment / $Segments
            $sinPhi = [Math]::Sin($phi)
            $cosPhi = [Math]::Cos($phi)
            $unit = @([float]($sinTheta * $cosPhi), [float]$cosTheta, [float]($sinTheta * $sinPhi))
            $normal = @(
                [float]($unit[0] / $Radius[0]),
                [float]($unit[1] / $Radius[1]),
                [float]($unit[2] / $Radius[2]))
            $normalLength = [Math]::Sqrt($normal[0] * $normal[0] + $normal[1] * $normal[1] + $normal[2] * $normal[2])
            $normal = @([float]($normal[0] / $normalLength), [float]($normal[1] / $normalLength), [float]($normal[2] / $normalLength))
            $position = @(
                [float]($Center[0] + $Radius[0] * $unit[0]),
                [float]($Center[1] + $Radius[1] * $unit[1]),
                [float]($Center[2] + $Radius[2] * $unit[2]))
            $tangent = @([float](-$sinPhi), 0.0, [float]$cosPhi, 1.0)
            [void]$row.Add((Add-Vertex $Part $position $normal @([float]($segment / $Segments), [float]($ring / $Rings)) $tangent))
        }
    }
    for ($ring = 0; $ring -lt $Rings; ++$ring) {
        for ($segment = 0; $segment -lt $Segments; ++$segment) {
            $a = $row[$ring * ($Segments + 1) + $segment]
            $b = $row[$ring * ($Segments + 1) + $segment + 1]
            $c = $row[($ring + 1) * ($Segments + 1) + $segment + 1]
            $d = $row[($ring + 1) * ($Segments + 1) + $segment]
            Add-Triangle $Part $a $b $c
            Add-Triangle $Part $a $c $d
        }
    }
}

function Add-Frustum {
    param(
        [object]$Part,
        [float]$BottomY,
        [float]$TopY,
        [float]$BottomRadius,
        [float]$TopRadius,
        [float]$CenterX = 0.0,
        [float]$CenterZ = 0.0,
        [int]$Segments = 24
    )
    $rings = New-Object 'System.Collections.Generic.List[int]'
    foreach ($pair in @(@($BottomY, $BottomRadius), @($TopY, $TopRadius))) {
        for ($segment = 0; $segment -le $Segments; ++$segment) {
            $phi = 2.0 * [Math]::PI * $segment / $Segments
            $sinPhi = [Math]::Sin($phi)
            $cosPhi = [Math]::Cos($phi)
            $slope = ($pair[1] - $BottomRadius) / ($TopY - $BottomY)
            $normal = @([float]$cosPhi, [float](-$slope), [float]$sinPhi)
            $normalLength = [Math]::Sqrt($normal[0] * $normal[0] + $normal[1] * $normal[1] + $normal[2] * $normal[2])
            $normal = @([float]($normal[0] / $normalLength), [float]($normal[1] / $normalLength), [float]($normal[2] / $normalLength))
            $position = @(
                [float]($CenterX + $pair[1] * $cosPhi),
                [float]$pair[0],
                [float]($CenterZ + $pair[1] * $sinPhi))
            $tangent = @([float](-$sinPhi), 0.0, [float]$cosPhi, 1.0)
            [void]$rings.Add((Add-Vertex $Part $position $normal @([float]($segment / $Segments), [float](($pair[0] - $BottomY) / ($TopY - $BottomY))) $tangent))
        }
    }
    for ($segment = 0; $segment -lt $Segments; ++$segment) {
        $a = $rings[$segment]
        $b = $rings[$segment + 1]
        $c = $rings[$Segments + 1 + $segment + 1]
        $d = $rings[$Segments + 1 + $segment]
        Add-Triangle $Part $a $b $c
        Add-Triangle $Part $a $c $d
    }
}

function Add-Box {
    param([object]$Part, [float[]]$Center, [float[]]$HalfExtent)
    $faces = @(
        @(@(0, 0, 1), @(-1, -1, 1), @(1, -1, 1), @(1, 1, 1), @(-1, 1, 1)),
        @(@(0, 0, -1), @(1, -1, -1), @(-1, -1, -1), @(-1, 1, -1), @(1, 1, -1)),
        @(@(1, 0, 0), @(1, -1, 1), @(1, -1, -1), @(1, 1, -1), @(1, 1, 1)),
        @(@(-1, 0, 0), @(-1, -1, -1), @(-1, -1, 1), @(-1, 1, 1), @(-1, 1, -1)),
        @(@(0, 1, 0), @(-1, 1, 1), @(1, 1, 1), @(1, 1, -1), @(-1, 1, -1)),
        @(@(0, -1, 0), @(-1, -1, -1), @(1, -1, -1), @(1, -1, 1), @(-1, -1, 1)))
    foreach ($face in $faces) {
        $normal = @([float]$face[0][0], [float]$face[0][1], [float]$face[0][2])
        $tangentDirection = if ($normal[0] -eq 1.0 -or $normal[0] -eq -1.0) { [float[]]@(0.0, 0.0, 1.0, 1.0) } else { [float[]]@(1.0, 0.0, 0.0, 1.0) }
        $faceIndices = New-Object 'System.Collections.Generic.List[int]'
        for ($corner = 1; $corner -le 4; ++$corner) {
            $sign = $face[$corner]
            $position = @(
                [float]($Center[0] + $HalfExtent[0] * $sign[0]),
                [float]($Center[1] + $HalfExtent[1] * $sign[1]),
                [float]($Center[2] + $HalfExtent[2] * $sign[2]))
            $tangent = [float[]]$tangentDirection
            [void]$faceIndices.Add((Add-Vertex $Part $position $normal @([float](($corner - 1) % 2), [float]([Math]::Floor(($corner - 1) / 2))) $tangent))
        }
        Add-Triangle $Part $faceIndices[0] $faceIndices[1] $faceIndices[2]
        Add-Triangle $Part $faceIndices[0] $faceIndices[2] $faceIndices[3]
    }
}

function New-Material {
    param([string]$Name, [float[]]$Color, [float]$Metallic, [float]$Roughness)
    return [ordered]@{
        name = $Name
        pbrMetallicRoughness = [ordered]@{
            baseColorFactor = $Color
            metallicFactor = $Metallic
            roughnessFactor = $Roughness
            baseColorTexture = [ordered]@{ index = 0 }
        }
    }
}

function Write-Gltf {
    param([string]$Path, [string]$Name, [object[]]$Parts, [object[]]$Materials, [float[]]$Translation)
    $stream = New-Object System.IO.MemoryStream
    $writer = New-Object System.IO.BinaryWriter($stream)
    $views = @()
    $accessors = @()
    $primitives = @()

    foreach ($part in $Parts) {
        while (($stream.Position % 4) -ne 0) { $writer.Write([byte]0) }
        $vertexOffset = [int64]$stream.Position
        foreach ($vertex in $part.Vertices) {
            foreach ($value in $vertex.Position) { $writer.Write([single]$value) }
            foreach ($value in $vertex.Normal) { $writer.Write([single]$value) }
            foreach ($value in $vertex.Uv) { $writer.Write([single]$value) }
            foreach ($value in $vertex.Tangent) { $writer.Write([single]$value) }
        }
        $vertexLength = [int64]$stream.Position - $vertexOffset
        $vertexView = $views.Count
        $views += ,([ordered]@{ buffer = 0; byteOffset = $vertexOffset; byteLength = $vertexLength; byteStride = 48; target = 34962 })
        $positionAccessor = $accessors.Count
        $accessors += ,([ordered]@{ bufferView = $vertexView; byteOffset = 0; componentType = 5126; count = $part.Vertices.Count; type = "VEC3" })
        $normalAccessor = $accessors.Count
        $accessors += ,([ordered]@{ bufferView = $vertexView; byteOffset = 12; componentType = 5126; count = $part.Vertices.Count; type = "VEC3" })
        $uvAccessor = $accessors.Count
        $accessors += ,([ordered]@{ bufferView = $vertexView; byteOffset = 24; componentType = 5126; count = $part.Vertices.Count; type = "VEC2" })
        $tangentAccessor = $accessors.Count
        $accessors += ,([ordered]@{ bufferView = $vertexView; byteOffset = 32; componentType = 5126; count = $part.Vertices.Count; type = "VEC4" })
        while (($stream.Position % 4) -ne 0) { $writer.Write([byte]0) }
        $indexOffset = [int64]$stream.Position
        foreach ($index in $part.Indices) { $writer.Write([uint16]$index) }
        $indexLength = [int64]$stream.Position - $indexOffset
        $indexView = $views.Count
        $views += ,([ordered]@{ buffer = 0; byteOffset = $indexOffset; byteLength = $indexLength; target = 34963 })
        $indexAccessor = $accessors.Count
        $accessors += ,([ordered]@{ bufferView = $indexView; byteOffset = 0; componentType = 5123; count = $part.Indices.Count; type = "SCALAR" })
        $attributes = [ordered]@{
            POSITION = $positionAccessor
            NORMAL = $normalAccessor
            TEXCOORD_0 = $uvAccessor
            TANGENT = $tangentAccessor
        }
        $primitives += ,([ordered]@{ attributes = $attributes; indices = $indexAccessor; material = $part.Material })
    }
    $writer.Flush()
    $bytes = $stream.ToArray()
    [IO.File]::WriteAllBytes([IO.Path]::ChangeExtension($Path, ".bin"), $bytes)
    $json = [ordered]@{
        asset = [ordered]@{ version = "2.0"; generator = "Henka Engine deterministic showcase generator" }
        buffers = @([ordered]@{ uri = ([IO.Path]::GetFileName([IO.Path]::ChangeExtension($Path, ".bin"))); byteLength = $bytes.Length })
        bufferViews = $views
        accessors = $accessors
        images = @([ordered]@{ uri = "cube_albedo.png" })
        textures = @([ordered]@{ source = 0 })
        materials = $Materials
        meshes = @([ordered]@{ name = $Name; primitives = $primitives })
        nodes = @([ordered]@{ name = $Name; mesh = 0; translation = $Translation })
        scenes = @([ordered]@{ name = "$Name Scene"; nodes = @(0) })
        scene = 0
    } | ConvertTo-Json -Depth 30 -Compress
    [IO.File]::WriteAllText($Path, $json, [Text.UTF8Encoding]::new($false))
    $writer.Dispose()
    $stream.Dispose()
}

function New-Giraffe {
    $materials = @(
        (New-Material "Giraffe Tan" @(0.72, 0.44, 0.18, 1.0) 0.0 0.52),
        (New-Material "Giraffe Spots" @(0.16, 0.038, 0.012, 1.0) 0.0 0.64),
        (New-Material "Giraffe Cream" @(0.95, 0.76, 0.48, 1.0) 0.0 0.48),
        (New-Material "Giraffe Eyes" @(0.008, 0.006, 0.004, 1.0) 0.0 0.16),
        (New-Material "Giraffe Smile" @(0.70, 0.035, 0.045, 1.0) 0.0 0.34))
    $tan = New-Part 0
    $spots = New-Part 1
    $cream = New-Part 2
    $eyes = New-Part 3
    $smile = New-Part 4
    Add-Ellipsoid $tan @(0.0, 1.35, 0.0) @(0.92, 1.0, 0.62)
    Add-Frustum $tan 1.55 3.55 0.34 0.29
    Add-Ellipsoid $tan @(0.0, 3.82, 0.0) @(0.72, 0.56, 0.56)
    foreach ($leg in @(@(-0.55, 0.0, -0.36), @(0.55, 0.0, -0.36), @(-0.55, 0.0, 0.36), @(0.55, 0.0, 0.36))) {
        Add-Frustum $tan 0.12 0.92 0.18 0.14 $leg[0] $leg[2] 16
    }
    Add-Ellipsoid $tan @(-0.55, 4.22, 0.0) @(0.32, 0.13, 0.52) 8 16
    Add-Ellipsoid $tan @(0.55, 4.22, 0.0) @(0.32, 0.13, 0.52) 8 16
    Add-Frustum $tan 4.15 4.48 0.095 0.075 -0.24 0.0 12
    Add-Frustum $tan 4.15 4.48 0.095 0.075 0.24 0.0 12
    Add-Ellipsoid $spots @(-0.48, 1.62, 0.61) @(0.24, 0.30, 0.035) 6 12
    Add-Ellipsoid $spots @(0.38, 1.05, 0.61) @(0.28, 0.20, 0.035) 6 12
    Add-Ellipsoid $spots @(0.48, 1.72, -0.61) @(0.22, 0.35, 0.035) 6 12
    Add-Ellipsoid $spots @(-0.22, 2.55, 0.33) @(0.14, 0.25, 0.035) 6 12
    Add-Ellipsoid $spots @(0.20, 3.02, 0.28) @(0.12, 0.22, 0.035) 6 12
    Add-Ellipsoid $spots @(-0.38, 3.92, 0.50) @(0.14, 0.12, 0.035) 6 12
    Add-Ellipsoid $spots @(0.35, 3.70, 0.48) @(0.16, 0.10, 0.035) 6 12
    Add-Ellipsoid $cream @(0.0, 3.62, 0.50) @(0.42, 0.24, 0.20) 8 16
    Add-Ellipsoid $cream @(-0.55, 4.20, 0.0) @(0.34, 0.12, 0.50) 8 16
    Add-Ellipsoid $cream @(0.55, 4.20, 0.0) @(0.34, 0.12, 0.50) 8 16
    Add-Ellipsoid $eyes @(-0.28, 3.98, 0.51) @(0.115, 0.16, 0.065) 8 12
    Add-Ellipsoid $eyes @(0.28, 3.98, 0.51) @(0.115, 0.16, 0.065) 8 12
    Add-Frustum $eyes 4.00 4.18 0.028 0.012 -0.39 0.54 8
    Add-Frustum $eyes 4.00 4.18 0.028 0.012 0.39 0.54 8
    Add-Ellipsoid $smile @(0.14, 3.58, 0.68) @(0.18, 0.055, 0.025) 6 12
    return [pscustomobject]@{ Parts = @($tan, $spots, $cream, $eyes, $smile); Materials = $materials }
}

function New-Rocket {
    $materials = @(
        (New-Material "Rocket Painted Ceramic" @(0.64, 0.70, 0.76, 1.0) 0.18 0.28),
        (New-Material "Rocket Brushed Metal" @(0.48, 0.52, 0.58, 1.0) 0.86 0.20),
        (New-Material "Rocket Heat Shield" @(0.055, 0.065, 0.075, 1.0) 0.62 0.34),
        (New-Material "Rocket Mission Stripe" @(0.84, 0.14, 0.055, 1.0) 0.18 0.30))
    $paint = New-Part 0
    $metal = New-Part 1
    $heat = New-Part 2
    $stripe = New-Part 3
    Add-Frustum $paint 0.35 2.55 0.62 0.56 0.0 0.0 32
    Add-Ellipsoid $paint @(0.0, 2.83, 0.0) @(0.56, 0.62, 0.56) 12 32
    Add-Frustum $paint 2.42 2.58 0.62 0.62 0.0 0.0 32
    Add-Frustum $metal 0.28 0.48 0.64 0.64 0.0 0.0 32
    Add-Frustum $metal 1.20 1.30 0.575 0.575 0.0 0.0 32
    Add-Frustum $metal 2.34 2.43 0.60 0.60 0.0 0.0 32
    foreach ($engine in @(@(-0.28, 0.0), @(0.0, 0.0), @(0.28, 0.0))) {
        Add-Frustum $heat 0.02 0.40 0.16 0.11 $engine[0] $engine[1] 20
        Add-Ellipsoid $heat @($engine[0], 0.02, $engine[1]) @(0.20, 0.08, 0.20) 8 16
    }
    Add-Box $stripe @(0.0, 1.82, 0.0) @(0.575, 0.06, 0.575)
    Add-Box $heat @(-0.78, 0.65, 0.0) @(0.16, 0.42, 0.06)
    Add-Box $heat @(0.78, 0.65, 0.0) @(0.16, 0.42, 0.06)
    Add-Box $heat @(0.0, 0.65, -0.78) @(0.06, 0.42, 0.16)
    Add-Box $heat @(0.0, 0.65, 0.78) @(0.06, 0.42, 0.16)
    return [pscustomobject]@{ Parts = @($paint, $metal, $heat, $stripe); Materials = $materials }
}

if (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
    throw "OutputDirectory must be absolute."
}
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$giraffe = New-Giraffe
Write-Gltf (Join-Path $OutputDirectory "cheeky_giraffe.gltf") "Cheeky Giraffe Mascot" $giraffe.Parts $giraffe.Materials @(-2.15, 0.0, -1.7)
$rocket = New-Rocket
Write-Gltf (Join-Path $OutputDirectory "original_realistic_rocket.gltf") "Original Realistic Rocket" $rocket.Parts $rocket.Materials @(2.15, 0.0, -1.7)
