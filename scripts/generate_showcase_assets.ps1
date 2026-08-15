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

function Normalize-Vector {
    param([float[]]$Vector)
    $length = [Math]::Sqrt(
        $Vector[0] * $Vector[0] +
        $Vector[1] * $Vector[1] +
        $Vector[2] * $Vector[2])
    if ($length -le 0.000001 -or [double]::IsNaN($length) -or [double]::IsInfinity($length)) {
        throw "Showcase generator received a zero or non-finite vector."
    }
    return @(
        [float]($Vector[0] / $length),
        [float]($Vector[1] / $length),
        [float]($Vector[2] / $length))
}

function Dot-Vector {
    param([float[]]$Left, [float[]]$Right)
    return [float](
        $Left[0] * $Right[0] +
        $Left[1] * $Right[1] +
        $Left[2] * $Right[2])
}

function Cross-Vector {
    param([float[]]$Left, [float[]]$Right)
    return @(
        [float]($Left[1] * $Right[2] - $Left[2] * $Right[1]),
        [float]($Left[2] * $Right[0] - $Left[0] * $Right[2]),
        [float]($Left[0] * $Right[1] - $Left[1] * $Right[0]))
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
            # The analytic ellipsoid tangent is the phi derivative of the
            # non-uniformly scaled surface.  The old unit-sphere tangent was
            # not orthogonal after scaling, which made normal maps and
            # specular highlights split across the showcase.
            $analyticTangent = @(
                [float](-$Radius[0] * $sinTheta * $sinPhi),
                0.0,
                [float]($Radius[2] * $sinTheta * $cosPhi))
            if ([Math]::Sqrt(
                    $analyticTangent[0] * $analyticTangent[0] +
                    $analyticTangent[1] * $analyticTangent[1] +
                    $analyticTangent[2] * $analyticTangent[2]) -le 0.000001) {
                $tangent = @([float](-$sinPhi), 0.0, [float]$cosPhi)
            }
            else {
                $tangent = Normalize-Vector $analyticTangent
            }
            $tangent = @([float]$tangent[0], [float]$tangent[1], [float]$tangent[2], 1.0)
            [void]$row.Add((Add-Vertex $Part $position $normal @([float]($segment / $Segments), [float]($ring / $Rings)) $tangent))
        }
    }
    for ($ring = 0; $ring -lt $Rings; ++$ring) {
        for ($segment = 0; $segment -lt $Segments; ++$segment) {
            $a = $row[$ring * ($Segments + 1) + $segment]
            $b = $row[$ring * ($Segments + 1) + $segment + 1]
            $c = $row[($ring + 1) * ($Segments + 1) + $segment + 1]
            $d = $row[($ring + 1) * ($Segments + 1) + $segment]
            # The duplicated longitude samples at each pole are retained for
            # simple bounded vertex layout, but their zero-area pole triangles
            # are intentionally omitted.
            if ($ring -gt 0) {
                Add-Triangle $Part $a $b $c
            }
            if ($ring -lt ($Rings - 1)) {
                Add-Triangle $Part $a $c $d
            }
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
    if ($TopY -le $BottomY -or $BottomRadius -le 0.0 -or $TopRadius -le 0.0) {
        throw "Showcase frustum dimensions are invalid."
    }
    $slope = ($TopRadius - $BottomRadius) / ($TopY - $BottomY)
    $rings = New-Object 'System.Collections.Generic.List[int]'
    foreach ($pair in @(@($BottomY, $BottomRadius), @($TopY, $TopRadius))) {
        for ($segment = 0; $segment -le $Segments; ++$segment) {
            $phi = 2.0 * [Math]::PI * $segment / $Segments
            $sinPhi = [Math]::Sin($phi)
            $cosPhi = [Math]::Cos($phi)
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
        # Increasing phi and increasing height otherwise wind the side inward.
        Add-Triangle $Part $a $c $b
        Add-Triangle $Part $a $d $c
    }
}

function Add-ProfiledFrustum {
    param(
        [object]$Part,
        [float[]]$Y,
        [float[]]$Radius,
        [float]$CenterX = 0.0,
        [float]$CenterZ = 0.0,
        [int]$Segments = 32
    )
    if ($Y.Count -lt 2 -or $Y.Count -ne $Radius.Count -or $Segments -lt 3) {
        throw "Showcase profiled frustum requires matching bounded profiles."
    }
    for ($profileIndex = 1; $profileIndex -lt $Y.Count; ++$profileIndex) {
        if ($Y[$profileIndex] -le $Y[$profileIndex - 1] -or $Radius[$profileIndex] -le 0.0) {
            throw "Showcase profiled frustum profile is not strictly increasing and positive."
        }
    }
    if ($Radius[0] -le 0.0) {
        throw "Showcase profiled frustum profile has an invalid base radius."
    }
    $rings = New-Object 'System.Collections.Generic.List[int]'
    $firstY = [float]$Y[0]
    $lastY = [float]$Y[$Y.Count - 1]
    for ($profileIndex = 0; $profileIndex -lt $Y.Count; ++$profileIndex) {
        if ($profileIndex -eq 0) {
            $slope = ($Radius[1] - $Radius[0]) / ($Y[1] - $Y[0])
        }
        elseif ($profileIndex -eq ($Y.Count - 1)) {
            $previous = $profileIndex - 1
            $slope = ($Radius[$profileIndex] - $Radius[$previous]) / ($Y[$profileIndex] - $Y[$previous])
        }
        else {
            $previous = $profileIndex - 1
            $next = $profileIndex + 1
            $slope = ($Radius[$next] - $Radius[$previous]) / ($Y[$next] - $Y[$previous])
        }
        for ($segment = 0; $segment -le $Segments; ++$segment) {
            $phi = 2.0 * [Math]::PI * $segment / $Segments
            $sinPhi = [Math]::Sin($phi)
            $cosPhi = [Math]::Cos($phi)
            $normal = Normalize-Vector @([float]$cosPhi, [float](-$slope), [float]$sinPhi)
            $position = @(
                [float]($CenterX + $Radius[$profileIndex] * $cosPhi),
                [float]$Y[$profileIndex],
                [float]($CenterZ + $Radius[$profileIndex] * $sinPhi))
            $tangent = @([float](-$sinPhi), 0.0, [float]$cosPhi, 1.0)
            [void]$rings.Add((Add-Vertex $Part $position $normal @([float]($segment / $Segments), [float](($Y[$profileIndex] - $firstY) / ($lastY - $firstY))) $tangent))
        }
    }
    for ($profileIndex = 0; $profileIndex -lt ($Y.Count - 1); ++$profileIndex) {
        $ringOffset = $profileIndex * ($Segments + 1)
        $nextRingOffset = ($profileIndex + 1) * ($Segments + 1)
        for ($segment = 0; $segment -lt $Segments; ++$segment) {
            $a = $rings[$ringOffset + $segment]
            $b = $rings[$ringOffset + $segment + 1]
            $c = $rings[$nextRingOffset + $segment + 1]
            $d = $rings[$nextRingOffset + $segment]
            Add-Triangle $Part $a $c $b
            Add-Triangle $Part $a $d $c
        }
    }
}

function Add-OrientedCone {
    param(
        [object]$Part,
        [float[]]$BaseCenter,
        [float[]]$TipCenter,
        [float]$BaseRadius,
        [int]$Segments = 16
    )
    if ($BaseRadius -le 0.0 -or $Segments -lt 3) {
        throw "Showcase cone dimensions are invalid."
    }
    $axisVector = @(
        [float]($TipCenter[0] - $BaseCenter[0]),
        [float]($TipCenter[1] - $BaseCenter[1]),
        [float]($TipCenter[2] - $BaseCenter[2]))
    $axisLength = [Math]::Sqrt(
        $axisVector[0] * $axisVector[0] +
        $axisVector[1] * $axisVector[1] +
        $axisVector[2] * $axisVector[2])
    if ($axisLength -le 0.000001 -or [double]::IsNaN($axisLength) -or [double]::IsInfinity($axisLength)) {
        throw "Showcase cone endpoints are coincident or non-finite."
    }
    $axis = Normalize-Vector $axisVector
    $reference = if ([Math]::Abs($axis[1]) -lt 0.9) { @(0.0, 1.0, 0.0) } else { @(0.0, 0.0, 1.0) }
    $basis = Normalize-Vector (Cross-Vector $axis $reference)
    $bitangent = Normalize-Vector (Cross-Vector $basis $axis)
    $slope = [float]($BaseRadius / $axisLength)
    $ring = New-Object 'System.Collections.Generic.List[int]'
    for ($segment = 0; $segment -lt $Segments; ++$segment) {
        $angle = 2.0 * [Math]::PI * $segment / $Segments
        $cosine = [Math]::Cos($angle)
        $sine = [Math]::Sin($angle)
        $radial = @(
            [float]($basis[0] * $cosine + $bitangent[0] * $sine),
            [float]($basis[1] * $cosine + $bitangent[1] * $sine),
            [float]($basis[2] * $cosine + $bitangent[2] * $sine))
        $normal = Normalize-Vector @(
            [float]($radial[0] + $axis[0] * $slope),
            [float]($radial[1] + $axis[1] * $slope),
            [float]($radial[2] + $axis[2] * $slope))
        $tangent = @(
            [float](-$basis[0] * $sine + $bitangent[0] * $cosine),
            [float](-$basis[1] * $sine + $bitangent[1] * $cosine),
            [float](-$basis[2] * $sine + $bitangent[2] * $cosine),
            1.0)
        $position = @(
            [float]($BaseCenter[0] + $radial[0] * $BaseRadius),
            [float]($BaseCenter[1] + $radial[1] * $BaseRadius),
            [float]($BaseCenter[2] + $radial[2] * $BaseRadius))
        [void]$ring.Add((Add-Vertex $Part $position $normal @([float]($segment / $Segments), 0.0) $tangent))
    }
    $tipIndex = Add-Vertex $Part $TipCenter $axis @(0.5, 1.0) @([float]$basis[0], [float]$basis[1], [float]$basis[2], 1.0)
    for ($segment = 0; $segment -lt $Segments; ++$segment) {
        $next = ($segment + 1) % $Segments
        Add-Triangle $Part $ring[$segment] $tipIndex $ring[$next]
    }
}

function Add-EllipsoidSurfaceSpot {
    param(
        [object]$Part,
        [float[]]$Center,
        [float[]]$Radius,
        [float[]]$UnitSurfaceDirection,
        [float]$MajorRadius,
        [float]$MinorRadius,
        [float]$RotationRadians = 0.0
    )
    $unit = Normalize-Vector $UnitSurfaceDirection
    $point = @(
        [float]($Center[0] + $Radius[0] * $unit[0]),
        [float]($Center[1] + $Radius[1] * $unit[1]),
        [float]($Center[2] + $Radius[2] * $unit[2]))
    $normal = Normalize-Vector @(
        [float]($unit[0] / $Radius[0]),
        [float]($unit[1] / $Radius[1]),
        [float]($unit[2] / $Radius[2]))
    Add-SurfaceSpot $Part $point $normal $MajorRadius $MinorRadius $RotationRadians
}

function Add-SurfaceSpot {
    param(
        [object]$Part,
        [float[]]$Center,
        [float[]]$Normal,
        [float]$MajorRadius,
        [float]$MinorRadius,
        [float]$RotationRadians = 0.0,
        [int]$Segments = 12
    )
    $normal = Normalize-Vector $Normal
    $reference = if ([Math]::Abs($normal[1]) -lt 0.9) { @(0.0, 1.0, 0.0) } else { @(1.0, 0.0, 0.0) }
    $tangent = Normalize-Vector (Cross-Vector $reference $normal)
    $bitangent = Normalize-Vector (Cross-Vector $normal $tangent)
    $rotatedTangent = @(
        [float]($tangent[0] * [Math]::Cos($RotationRadians) + $bitangent[0] * [Math]::Sin($RotationRadians)),
        [float]($tangent[1] * [Math]::Cos($RotationRadians) + $bitangent[1] * [Math]::Sin($RotationRadians)),
        [float]($tangent[2] * [Math]::Cos($RotationRadians) + $bitangent[2] * [Math]::Sin($RotationRadians)))
    $rotatedBitangent = @(
        [float](-$tangent[0] * [Math]::Sin($RotationRadians) + $bitangent[0] * [Math]::Cos($RotationRadians)),
        [float](-$tangent[1] * [Math]::Sin($RotationRadians) + $bitangent[1] * [Math]::Cos($RotationRadians)),
        [float](-$tangent[2] * [Math]::Sin($RotationRadians) + $bitangent[2] * [Math]::Cos($RotationRadians)))
    $epsilon = 0.006
    $centerPosition = @(
        [float]($Center[0] + $normal[0] * $epsilon),
        [float]($Center[1] + $normal[1] * $epsilon),
        [float]($Center[2] + $normal[2] * $epsilon))
    $centerIndex = Add-Vertex $Part $centerPosition $normal @(0.5, 0.5) @([float]$rotatedTangent[0], [float]$rotatedTangent[1], [float]$rotatedTangent[2], 1.0)
    $ring = New-Object 'System.Collections.Generic.List[int]'
    for ($segment = 0; $segment -lt $Segments; ++$segment) {
        $angle = 2.0 * [Math]::PI * $segment / $Segments
        $cosine = [Math]::Cos($angle)
        $sine = [Math]::Sin($angle)
        $position = @(
            [float]($centerPosition[0] + $rotatedTangent[0] * $MajorRadius * $cosine + $rotatedBitangent[0] * $MinorRadius * $sine),
            [float]($centerPosition[1] + $rotatedTangent[1] * $MajorRadius * $cosine + $rotatedBitangent[1] * $MinorRadius * $sine),
            [float]($centerPosition[2] + $rotatedTangent[2] * $MajorRadius * $cosine + $rotatedBitangent[2] * $MinorRadius * $sine))
        [void]$ring.Add((Add-Vertex $Part $position $normal @([float](0.5 + 0.5 * $cosine), [float](0.5 + 0.5 * $sine)) @([float]$rotatedTangent[0], [float]$rotatedTangent[1], [float]$rotatedTangent[2], 1.0)))
    }
    for ($segment = 0; $segment -lt $Segments; ++$segment) {
        $next = ($segment + 1) % $Segments
        Add-Triangle $Part $centerIndex $ring[$segment] $ring[$next]
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

function Add-Quad {
    param([object]$Part, [float[][]]$Positions)
    if ($Positions.Count -ne 4) {
        throw "Showcase quad requires four positions."
    }
    $edge1 = @(
        [float]($Positions[1][0] - $Positions[0][0]),
        [float]($Positions[1][1] - $Positions[0][1]),
        [float]($Positions[1][2] - $Positions[0][2]))
    $edge2 = @(
        [float]($Positions[2][0] - $Positions[0][0]),
        [float]($Positions[2][1] - $Positions[0][1]),
        [float]($Positions[2][2] - $Positions[0][2]))
    $normal = Normalize-Vector (Cross-Vector $edge1 $edge2)
    $tangent = Normalize-Vector $edge1
    $tangent4 = @([float]$tangent[0], [float]$tangent[1], [float]$tangent[2], 1.0)
    $indices = New-Object 'System.Collections.Generic.List[int]'
    foreach ($position in $Positions) {
        [void]$indices.Add((Add-Vertex $Part $position $normal @(0.0, 0.0) $tangent4))
    }
    Add-Triangle $Part $indices[0] $indices[1] $indices[2]
    Add-Triangle $Part $indices[0] $indices[2] $indices[3]
}

function Add-RadialFin {
    param(
        [object]$Part,
        [float]$DirectionX,
        [float]$DirectionZ,
        [float]$Thickness = 0.07
    )
    $radial = Normalize-Vector @($DirectionX, 0.0, $DirectionZ)
    $tangent = @([float](-$radial[2]), 0.0, [float]$radial[0])
    $profile = @(
        @([float]0.52, [float]0.35),
        @([float]0.96, [float]0.48),
        @([float]0.88, [float]1.18),
        @([float]0.55, [float]0.96))
    $front = @()
    $back = @()
    foreach ($point in $profile) {
        $front += ,@(
            [float]($radial[0] * $point[0] + $tangent[0] * $Thickness),
            [float]$point[1],
            [float]($radial[2] * $point[0] + $tangent[2] * $Thickness))
        $back += ,@(
            [float]($radial[0] * $point[0] - $tangent[0] * $Thickness),
            [float]$point[1],
            [float]($radial[2] * $point[0] - $tangent[2] * $Thickness))
    }
    Add-Quad $Part @($front[0], $front[3], $front[2], $front[1])
    Add-Quad $Part @($back[0], $back[1], $back[2], $back[3])
    Add-Quad $Part @($front[1], $back[1], $back[2], $front[2])
    Add-Quad $Part @($front[2], $back[2], $back[3], $front[3])
    Add-Quad $Part @($front[3], $back[3], $back[0], $front[0])
    Add-Quad $Part @($front[0], $back[0], $back[1], $front[1])
}

function New-Material {
    param(
        [string]$Name,
        [float[]]$Color,
        [float]$Metallic,
        [float]$Roughness,
        [float]$Clearcoat = 0.0,
        [float]$ClearcoatRoughness = 0.20,
        [float[]]$SheenColor = @(0.0, 0.0, 0.0),
        [float]$SheenRoughness = 0.50,
        [float[]]$EmissiveColor = @(0.0, 0.0, 0.0),
        [float]$EmissiveStrength = 1.0,
        [int]$NormalTextureIndex = -1,
        [float]$NormalTextureScale = 0.35,
        [int]$MetallicRoughnessTextureIndex = -1
    )
    $material = [ordered]@{
        name = $Name
        pbrMetallicRoughness = [ordered]@{
            baseColorFactor = $Color
            metallicFactor = $Metallic
            roughnessFactor = $Roughness
        }
    }
    if ($MetallicRoughnessTextureIndex -ge 0) {
        $material.pbrMetallicRoughness.Add("metallicRoughnessTexture", [ordered]@{
            index = $MetallicRoughnessTextureIndex
        })
    }
    if ($NormalTextureIndex -ge 0) {
        $material.Add("normalTexture", [ordered]@{
            index = $NormalTextureIndex
            scale = $NormalTextureScale
        })
    }
    $extensions = [ordered]@{}
    if ($Clearcoat -gt 0.0) {
        $extensions.Add("KHR_materials_clearcoat", [ordered]@{
            clearcoatFactor = $Clearcoat
            clearcoatRoughnessFactor = $ClearcoatRoughness
        })
    }
    if (([Math]::Abs($SheenColor[0]) + [Math]::Abs($SheenColor[1]) + [Math]::Abs($SheenColor[2])) -gt 0.0001) {
        $extensions.Add("KHR_materials_sheen", [ordered]@{
            sheenColorFactor = $SheenColor
            sheenRoughnessFactor = $SheenRoughness
        })
    }
    if (([Math]::Abs($EmissiveColor[0]) + [Math]::Abs($EmissiveColor[1]) + [Math]::Abs($EmissiveColor[2])) -gt 0.0001) {
        $material.Add("emissiveFactor", $EmissiveColor)
        $extensions.Add("KHR_materials_emissive_strength", [ordered]@{
            emissiveStrength = $EmissiveStrength
        })
    }
    if ($extensions.Count -gt 0) {
        $material.Add("extensions", $extensions)
    }
    return $material
}

function Write-ShowcaseTexture {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][ValidateSet("normal", "metallic_roughness")][string]$Kind,
        [Parameter(Mandatory = $true)][ValidateSet("giraffe", "rocket")][string]$Subject
    )
    Add-Type -AssemblyName System.Drawing
    $bitmap = [System.Drawing.Bitmap]::new(64, 64, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        for ($y = 0; $y -lt 64; ++$y) {
            for ($x = 0; $x -lt 64; ++$x) {
                $u = ($x + 0.5) / 64.0
                $v = ($y + 0.5) / 64.0
                if ($Kind -eq "normal") {
                    $frequency = if ($Subject -eq "giraffe") { 0.34 } else { 0.52 }
                    $nx = 0.12 * [Math]::Sin(($x + 3) * $frequency) * [Math]::Cos(($y + 7) * ($frequency * 0.73))
                    $ny = 0.10 * [Math]::Cos(($y + 5) * ($frequency * 0.91)) * [Math]::Sin(($x + 11) * ($frequency * 0.61))
                    $nz = [Math]::Sqrt([Math]::Max(0.0, 1.0 - ($nx * $nx) - ($ny * $ny)))
                    $red = [int][Math]::Round(128.0 + (127.0 * $nx))
                    $green = [int][Math]::Round(128.0 + (127.0 * $ny))
                    $blue = [int][Math]::Round(255.0 * $nz)
                }
                else {
                    $variation = 0.5 + (0.5 * [Math]::Sin(($x + 2) * 0.24 + ($y + 5) * 0.17))
                    $roughness = if ($Subject -eq "giraffe") {
                        120.0 + (58.0 * $variation)
                    }
                    else {
                        82.0 + (78.0 * $variation)
                    }
                    $red = 255
                    $green = [int][Math]::Round($roughness)
                    $blue = if ($Subject -eq "giraffe") { 0 } else { 190 }
                }
                $bitmap.SetPixel(
                    $x,
                    $y,
                    [System.Drawing.Color]::FromArgb(
                        255,
                        [Math]::Max(0, [Math]::Min(255, $red)),
                        [Math]::Max(0, [Math]::Min(255, $green)),
                        [Math]::Max(0, [Math]::Min(255, $blue))))
            }
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

function New-ShowcaseTextureDefinitions {
    param(
        [Parameter(Mandatory = $true)][string]$OutputDirectory,
        [Parameter(Mandatory = $true)][ValidateSet("giraffe", "rocket")][string]$Subject
    )
    $normalName = "${Subject}_detail_normal.png"
    $metallicRoughnessName = "${Subject}_metallic_roughness.png"
    Write-ShowcaseTexture (Join-Path $OutputDirectory $normalName) "normal" $Subject
    Write-ShowcaseTexture (Join-Path $OutputDirectory $metallicRoughnessName) "metallic_roughness" $Subject
    return @(
        ([ordered]@{ uri = $normalName }),
        ([ordered]@{ uri = $metallicRoughnessName }))
}

function Test-ShowcasePart {
    param([object]$Part, [int]$MaterialCount)
    if ($Part.Vertices.Count -lt 3 -or $Part.Indices.Count -lt 3 -or ($Part.Indices.Count % 3) -ne 0) {
        throw "Showcase part has an invalid triangle count."
    }
    if ($Part.Material -lt 0 -or $Part.Material -ge $MaterialCount) {
        throw "Showcase part references an invalid material."
    }
    foreach ($vertex in $Part.Vertices) {
        $values = @($vertex.Position + $vertex.Normal + $vertex.Uv + $vertex.Tangent)
        foreach ($value in $values) {
            if ([double]::IsNaN([double]$value) -or [double]::IsInfinity([double]$value)) {
                throw "Showcase part contains a non-finite vertex value."
            }
        }
        $normalLength = [Math]::Sqrt((Dot-Vector $vertex.Normal $vertex.Normal))
        $tangentLength = [Math]::Sqrt(
            $vertex.Tangent[0] * $vertex.Tangent[0] +
            $vertex.Tangent[1] * $vertex.Tangent[1] +
            $vertex.Tangent[2] * $vertex.Tangent[2])
        if ($normalLength -lt 0.99 -or $normalLength -gt 1.01 -or
            $tangentLength -lt 0.99 -or $tangentLength -gt 1.01 -or
            [Math]::Abs((Dot-Vector $vertex.Normal @($vertex.Tangent[0], $vertex.Tangent[1], $vertex.Tangent[2]))) -gt 0.02 -or
            [Math]::Abs($vertex.Tangent[3]) -lt 0.5) {
            throw "Showcase part contains an invalid normal/tangent frame."
        }
    }
    for ($index = 0; $index -lt $Part.Indices.Count; $index += 3) {
        $a = $Part.Indices[$index]
        $b = $Part.Indices[$index + 1]
        $c = $Part.Indices[$index + 2]
        if ($a -lt 0 -or $b -lt 0 -or $c -lt 0 -or
            $a -ge $Part.Vertices.Count -or $b -ge $Part.Vertices.Count -or $c -ge $Part.Vertices.Count) {
            throw "Showcase part contains an out-of-range index."
        }
        $edge1 = @(
            [float]($Part.Vertices[$b].Position[0] - $Part.Vertices[$a].Position[0]),
            [float]($Part.Vertices[$b].Position[1] - $Part.Vertices[$a].Position[1]),
            [float]($Part.Vertices[$b].Position[2] - $Part.Vertices[$a].Position[2]))
        $edge2 = @(
            [float]($Part.Vertices[$c].Position[0] - $Part.Vertices[$a].Position[0]),
            [float]($Part.Vertices[$c].Position[1] - $Part.Vertices[$a].Position[1]),
            [float]($Part.Vertices[$c].Position[2] - $Part.Vertices[$a].Position[2]))
        $faceNormal = Cross-Vector $edge1 $edge2
        $area = [Math]::Sqrt((Dot-Vector $faceNormal $faceNormal))
        $averageNormal = Normalize-Vector @(
            [float]($Part.Vertices[$a].Normal[0] + $Part.Vertices[$b].Normal[0] + $Part.Vertices[$c].Normal[0]),
            [float]($Part.Vertices[$a].Normal[1] + $Part.Vertices[$b].Normal[1] + $Part.Vertices[$c].Normal[1]),
            [float]($Part.Vertices[$a].Normal[2] + $Part.Vertices[$b].Normal[2] + $Part.Vertices[$c].Normal[2]))
        if ($area -lt 0.000001 -or (Dot-Vector $faceNormal $averageNormal) -le 0.0) {
            throw "Showcase part material $($Part.Material) contains a degenerate or inward-wound triangle at index $index."
        }
    }
}

function Write-Gltf {
    param(
        [string]$Path,
        [string]$Name,
        [object[]]$Parts,
        [object[]]$Materials,
        [float[]]$Translation,
        [object[]]$TextureDefinitions = @())
    if ($Parts.Count -eq 0 -or $Materials.Count -eq 0) {
        throw "Showcase asset must contain geometry and materials."
    }
    foreach ($part in $Parts) {
        Test-ShowcasePart $part $Materials.Count
    }
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
        extensionsUsed = @("KHR_materials_clearcoat", "KHR_materials_sheen", "KHR_materials_emissive_strength")
        buffers = @([ordered]@{ uri = ([IO.Path]::GetFileName([IO.Path]::ChangeExtension($Path, ".bin"))); byteLength = $bytes.Length })
        bufferViews = $views
        accessors = $accessors
        materials = $Materials
        meshes = @([ordered]@{ name = $Name; primitives = $primitives })
        nodes = @([ordered]@{ name = $Name; mesh = 0; translation = $Translation })
        scenes = @([ordered]@{ name = "$Name Scene"; nodes = @(0) })
        scene = 0
    }
    if ($TextureDefinitions.Count -gt 0) {
        $json.Add("images", @($TextureDefinitions))
        $textures = @()
        for ($textureIndex = 0; $textureIndex -lt $TextureDefinitions.Count; ++$textureIndex) {
            $textures += ,([ordered]@{ source = $textureIndex })
        }
        $json.Add("textures", $textures)
    }
    $jsonText = $json | ConvertTo-Json -Depth 30 -Compress
    [IO.File]::WriteAllText($Path, $jsonText, [Text.UTF8Encoding]::new($false))
    $writer.Dispose()
    $stream.Dispose()
}

function New-Giraffe {
    $materials = @(
        (New-Material "Giraffe Tan" @(0.46, 0.25, 0.08, 1.0) 0.0 0.56 0.08 0.36 @(0.07, 0.025, 0.01) 0.45 -NormalTextureIndex 0 -NormalTextureScale 0.14 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Spots" @(0.09, 0.018, 0.006, 1.0) 0.0 0.68 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Cream" @(0.72, 0.55, 0.32, 1.0) 0.0 0.50 0.06 0.28 @(0.11, 0.06, 0.025) 0.48 -NormalTextureIndex 0 -NormalTextureScale 0.14 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Eye White" @(0.76, 0.72, 0.62, 1.0) 0.0 0.18 0.54 0.08),
        (New-Material "Giraffe Iris" @(0.18, 0.055, 0.014, 1.0) 0.0 0.24 0.64 0.06),
        (New-Material "Giraffe Eye Detail" @(0.004, 0.002, 0.001, 1.0) 0.0 0.10 0.70 0.06),
        (New-Material "Giraffe Smile" @(0.11, 0.015, 0.008, 1.0) 0.0 0.34 0.05 0.22),
        (New-Material "Giraffe Ear Inner" @(0.30, 0.065, 0.025, 1.0) 0.0 0.44 0.03 0.18 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Ossicone Cap" @(0.12, 0.025, 0.006, 1.0) 0.0 0.48 0.02 0.24 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1))
    $tan = New-Part 0
    $spots = New-Part 1
    $cream = New-Part 2
    $eyes = New-Part 3
    $iris = New-Part 4
    $details = New-Part 5
    $smile = New-Part 6
    $earInner = New-Part 7
    $ossicone = New-Part 8
    # Keep the mascot identity restrained, but use continuous profiles and
    # believable curvature so the silhouette does not read as primitive
    # assembly when inspected from the authored front and three-quarter views.
    Add-Ellipsoid $tan @(0.0, 1.42, 0.0) @(0.98, 0.96, 0.64) 28 56
    Add-ProfiledFrustum $tan @(1.55, 1.86, 2.25, 2.72, 3.18, 3.55) @(0.41, 0.39, 0.36, 0.33, 0.30, 0.29) 0.0 0.0 48
    Add-Ellipsoid $tan @(0.0, 3.82, 0.0) @(0.68, 0.54, 0.52) 26 48
    foreach ($leg in @(@(-0.55, 0.0, -0.36), @(0.55, 0.0, -0.36), @(-0.55, 0.0, 0.36), @(0.55, 0.0, 0.36))) {
        Add-ProfiledFrustum $tan @(0.08, 0.28, 0.62, 0.98, 1.10) @(0.16, 0.18, 0.17, 0.145, 0.13) $leg[0] $leg[2] 28
    }
    # Model the ears as compact, flattened lobes in the head plane. The
    # previous deep capsules read like small missiles when viewed from the
    # front, especially with the inner-ear patch sitting on their top edge.
    Add-Ellipsoid $tan @(-0.62, 4.22, 0.0) @(0.34, 0.23, 0.14) 12 24
    Add-Ellipsoid $tan @(0.62, 4.22, 0.0) @(0.34, 0.23, 0.14) 12 24
    Add-Frustum $tan 4.15 4.48 0.095 0.075 -0.24 0.0 16
    Add-Frustum $tan 4.15 4.48 0.095 0.075 0.24 0.0 16
    Add-Ellipsoid $earInner @(-0.62, 4.22, 0.15) @(0.18, 0.12, 0.025) 10 20
    Add-Ellipsoid $earInner @(0.62, 4.22, 0.15) @(0.18, 0.12, 0.025) 10 20
    # Ossicones use a visible tan stalk and a short cap angled outward from
    # the head. The previous isolated ellipsoids read as floating pegs and
    # gave the mascot an unintended, lopsided silhouette in front views.
    Add-Frustum $tan 4.28 4.52 0.065 0.052 -0.24 0.0 16
    Add-Frustum $tan 4.28 4.52 0.065 0.052 0.24 0.0 16
    Add-OrientedCone $ossicone @(-0.24, 4.48, 0.0) @(-0.32, 4.72, -0.01) 0.10 16
    Add-OrientedCone $ossicone @(0.24, 4.48, 0.0) @(0.32, 4.72, -0.01) 0.10 16
    # A short mane row gives the neck a readable rear contour without making
    # the mascot realistic in the photographic sense.
    foreach ($maneY in @(1.95, 2.18, 2.41, 2.64, 2.87, 3.10, 3.33)) {
        Add-Ellipsoid $spots @(0.0, $maneY, -0.31) @(0.075, 0.13, 0.035) 8 16
    }
    # Spots are low-profile tangent patches, not intersecting ellipsoids. The
    # small outward bias is a deterministic decal-style separation that keeps
    # the pattern readable without introducing bumps or coplanar z-fighting.
    Add-EllipsoidSurfaceSpot $spots @(0.0, 1.35, 0.0) @(0.92, 1.0, 0.62) @(-0.48, 0.20, 0.86) 0.23 0.17 -0.20
    Add-EllipsoidSurfaceSpot $spots @(0.0, 1.35, 0.0) @(0.92, 1.0, 0.62) @(0.38, -0.18, 0.91) 0.25 0.14 0.35
    Add-EllipsoidSurfaceSpot $spots @(0.0, 1.35, 0.0) @(0.92, 1.0, 0.62) @(-0.18, -0.52, 0.83) 0.16 0.22 0.20
    Add-EllipsoidSurfaceSpot $spots @(0.0, 1.35, 0.0) @(0.92, 1.0, 0.62) @(0.54, 0.50, 0.67) 0.16 0.12 -0.45
    Add-EllipsoidSurfaceSpot $spots @(0.0, 1.35, 0.0) @(0.92, 1.0, 0.62) @(-0.70, -0.35, 0.62) 0.15 0.12 0.55
    Add-SurfaceSpot $spots @(0.0, 2.22, 0.30) @(0.0, 0.0, 1.0) 0.13 0.22 -0.15
    Add-SurfaceSpot $spots @(0.0, 2.78, 0.30) @(0.0, 0.0, 1.0) 0.12 0.17 0.25
    Add-EllipsoidSurfaceSpot $spots @(0.0, 3.82, 0.0) @(0.72, 0.56, 0.56) @(-0.32, 0.18, 0.92) 0.12 0.10 0.35
    Add-EllipsoidSurfaceSpot $spots @(0.0, 3.82, 0.0) @(0.72, 0.56, 0.56) @(0.34, -0.22, 0.90) 0.13 0.09 -0.30
    Add-Ellipsoid $cream @(0.0, 3.63, 0.50) @(0.40, 0.22, 0.19) 18 32
    Add-Ellipsoid $cream @(0.0, 3.48, 0.53) @(0.29, 0.12, 0.14) 14 28
    Add-Ellipsoid $eyes @(-0.27, 3.98, 0.50) @(0.095, 0.13, 0.060) 16 28
    Add-Ellipsoid $eyes @(0.27, 3.98, 0.50) @(0.095, 0.13, 0.060) 16 28
    Add-Ellipsoid $iris @(-0.27, 3.98, 0.562) @(0.052, 0.074, 0.020) 12 24
    Add-Ellipsoid $iris @(0.27, 3.98, 0.562) @(0.052, 0.074, 0.020) 12 24
    Add-Ellipsoid $details @(-0.27, 3.98, 0.581) @(0.020, 0.038, 0.009) 10 18
    Add-Ellipsoid $details @(0.27, 3.98, 0.581) @(0.020, 0.038, 0.009) 10 18
    Add-Ellipsoid $eyes @(-0.285, 4.015, 0.590) @(0.010, 0.015, 0.005) 8 14
    Add-Ellipsoid $eyes @(0.255, 4.015, 0.590) @(0.010, 0.015, 0.005) 8 14
    Add-Ellipsoid $details @(-0.14, 3.66, 0.695) @(0.070, 0.038, 0.014) 8 14
    Add-Ellipsoid $details @(0.14, 3.66, 0.695) @(0.070, 0.038, 0.014) 8 14
    Add-Ellipsoid $smile @(0.0, 3.55, 0.695) @(0.24, 0.035, 0.014) 8 16
    Add-Ellipsoid $smile @(0.14, 3.58, 0.685) @(0.18, 0.045, 0.016) 8 16
    return [pscustomobject]@{ Parts = @($tan, $spots, $cream, $eyes, $iris, $details, $smile, $earInner, $ossicone); Materials = $materials }
}

function New-Rocket {
    $materials = @(
        (New-Material "Rocket Painted Ceramic" @(0.52, 0.58, 0.66, 1.0) 0.18 0.32 0.32 0.16 -NormalTextureIndex 0 -NormalTextureScale 0.16 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Brushed Metal" @(0.36, 0.40, 0.47, 1.0) 0.86 0.24 0.08 0.20 -NormalTextureIndex 0 -NormalTextureScale 0.14 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Heat Shield" @(0.035, 0.042, 0.050, 1.0) 0.62 0.38 0.0 0.20 @(0.0, 0.0, 0.0) 0.50 @(0.18, 0.025, 0.005) 0.40 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Mission Stripe" @(0.68, 0.085, 0.028, 1.0) 0.18 0.34 0.12 0.20 -NormalTextureIndex 0 -NormalTextureScale 0.14 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Avionics" @(0.018, 0.024, 0.034, 1.0) 0.72 0.28 0.10 0.18 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1))
    $paint = New-Part 0
    $metal = New-Part 1
    $heat = New-Part 2
    $stripe = New-Part 3
    $avionics = New-Part 4
    # Staged core, interstage, and ogive-like fairing sections provide a more
    # believable modern launch-vehicle silhouette while remaining bounded.
    Add-ProfiledFrustum $paint @(0.35, 1.15, 2.25, 2.50, 2.62, 2.76, 2.92, 3.12) @(0.62, 0.61, 0.58, 0.56, 0.54, 0.44, 0.27, 0.075) 0.0 0.0 56
    Add-Ellipsoid $paint @(0.0, 3.17, 0.0) @(0.08, 0.11, 0.08) 16 32
    Add-Frustum $metal 0.28 0.48 0.64 0.64 0.0 0.0 32
    Add-Frustum $metal 0.48 0.56 0.64 0.59 0.0 0.0 32
    Add-Frustum $metal 1.24 1.30 0.575 0.575 0.0 0.0 32
    Add-Frustum $metal 2.28 2.38 0.60 0.60 0.0 0.0 32
    Add-Frustum $metal 2.54 2.62 0.56 0.56 0.0 0.0 32
    Add-Frustum $metal 2.76 2.84 0.44 0.44 0.0 0.0 32
    Add-Frustum $avionics 1.18 1.24 0.58 0.58 0.0 0.0 32
    Add-Frustum $avionics 2.48 2.54 0.56 0.56 0.0 0.0 32
    # A bounded seven-engine cluster gives the lower stage a recognizable
    # modern launch-vehicle layout instead of three red cylinders. Each
    # engine keeps a metallic throat ring, a dark heat bell, and a smaller
    # avionics insert so the cluster remains legible under the studio HDR.
    foreach ($engine in @(
            @(-0.28, 0.0), @(0.0, 0.0), @(0.28, 0.0),
            @(0.0, -0.28), @(0.0, 0.28), @(-0.20, -0.20), @(0.20, 0.20))) {
        Add-Frustum $metal 0.02 0.16 0.21 0.18 $engine[0] $engine[1] 20
        Add-Frustum $heat 0.14 0.40 0.16 0.105 $engine[0] $engine[1] 20
        Add-Frustum $heat 0.40 0.46 0.105 0.075 $engine[0] $engine[1] 20
        Add-Frustum $avionics 0.05 0.12 0.12 0.075 $engine[0] $engine[1] 16
        Add-Ellipsoid $heat @($engine[0], 0.02, $engine[1]) @(0.17, 0.06, 0.17) 8 16
    }
    Add-Frustum $stripe 1.78 1.91 0.59 0.59 0.0 0.0 32
    Add-RadialFin $metal 1.0 0.0
    Add-RadialFin $metal -1.0 0.0
    Add-RadialFin $metal 0.0 1.0
    Add-RadialFin $metal 0.0 -1.0
    return [pscustomobject]@{ Parts = @($paint, $metal, $heat, $stripe, $avionics); Materials = $materials }
}

if (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
    throw "OutputDirectory must be absolute."
}
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$giraffe = New-Giraffe
$giraffeTextures = New-ShowcaseTextureDefinitions $OutputDirectory "giraffe"
Write-Gltf (Join-Path $OutputDirectory "cheeky_giraffe.gltf") "Cheeky Giraffe Mascot" $giraffe.Parts $giraffe.Materials @(-2.15, 0.0, -1.7) $giraffeTextures
$rocket = New-Rocket
$rocketTextures = New-ShowcaseTextureDefinitions $OutputDirectory "rocket"
Write-Gltf (Join-Path $OutputDirectory "original_realistic_rocket.gltf") "Original Realistic Rocket" $rocket.Parts $rocket.Materials @(2.15, 0.0, -1.7) $rocketTextures
