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

function Add-AnatomicalLoft {
    param(
        [object]$Part,
        [float[]]$Y,
        [float[]]$RadiusX,
        [float[]]$RadiusZ,
        [float]$CenterX = 0.0,
        [float[]]$CenterZ = @(),
        [int]$Segments = 32
    )
    if ($Y.Count -lt 2 -or
        $Y.Count -ne $RadiusX.Count -or
        $Y.Count -ne $RadiusZ.Count -or
        ($CenterZ.Count -ne 0 -and $CenterZ.Count -ne $Y.Count) -or
        $Segments -lt 3) {
        throw "Showcase anatomical loft requires matching bounded profiles."
    }
    for ($profileIndex = 0; $profileIndex -lt $Y.Count; ++$profileIndex) {
        if ($RadiusX[$profileIndex] -le 0.0 -or $RadiusZ[$profileIndex] -le 0.0) {
            throw "Showcase anatomical loft profile has an invalid radius."
        }
        if ($profileIndex -gt 0 -and $Y[$profileIndex] -le $Y[$profileIndex - 1]) {
            throw "Showcase anatomical loft profile is not strictly increasing."
        }
    }
    $rings = New-Object 'System.Collections.Generic.List[int]'
    $firstY = [float]$Y[0]
    $lastY = [float]$Y[$Y.Count - 1]
    for ($profileIndex = 0; $profileIndex -lt $Y.Count; ++$profileIndex) {
        if ($profileIndex -eq 0) {
            $previous = 0
            $next = 1
        }
        elseif ($profileIndex -eq ($Y.Count - 1)) {
            $previous = $profileIndex - 1
            $next = $profileIndex
        }
        else {
            $previous = $profileIndex - 1
            $next = $profileIndex + 1
        }
        $slopeX = ($RadiusX[$next] - $RadiusX[$previous]) /
            [Math]::Max(0.000001, ($Y[$next] - $Y[$previous]))
        $slopeZ = ($RadiusZ[$next] - $RadiusZ[$previous]) /
            [Math]::Max(0.000001, ($Y[$next] - $Y[$previous]))
        $centerSlopeZ = if ($CenterZ.Count -eq 0) { 0.0 } else {
            ($CenterZ[$next] - $CenterZ[$previous]) /
                [Math]::Max(0.000001, ($Y[$next] - $Y[$previous]))
        }
        $zCenter = if ($CenterZ.Count -eq 0) { 0.0 } else { [float]$CenterZ[$profileIndex] }
        for ($segment = 0; $segment -le $Segments; ++$segment) {
            $phi = 2.0 * [Math]::PI * $segment / $Segments
            $sinPhi = [Math]::Sin($phi)
            $cosPhi = [Math]::Cos($phi)
            if ([Math]::Abs($sinPhi) -lt 0.000001) { $sinPhi = 0.0 }
            if ([Math]::Abs($cosPhi) -lt 0.000001) { $cosPhi = 0.0 }
            $normal = Normalize-Vector @(
                [float]($cosPhi / $RadiusX[$profileIndex]),
                [float](-($slopeX * $cosPhi * $cosPhi / $RadiusX[$profileIndex]) -
                    (($centerSlopeZ + $slopeZ * $sinPhi) * $sinPhi / $RadiusZ[$profileIndex])),
                [float]($sinPhi / $RadiusZ[$profileIndex]))
            $position = @(
                [float]($CenterX + $RadiusX[$profileIndex] * $cosPhi),
                [float]$Y[$profileIndex],
                [float]($zCenter + $RadiusZ[$profileIndex] * $sinPhi))
            $tangent = Normalize-Vector @(
                [float](-$RadiusX[$profileIndex] * $sinPhi),
                0.0,
                [float]($RadiusZ[$profileIndex] * $cosPhi))
            [void]$rings.Add((Add-Vertex $Part $position $normal @(
                    [float]($segment / $Segments),
                    [float](($Y[$profileIndex] - $firstY) / ($lastY - $firstY))) @(
                    [float]$tangent[0], [float]$tangent[1], [float]$tangent[2], 1.0)))
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

function Add-HorizontalLoft {
    param(
        [object]$Part,
        [float[]]$Z,
        [float[]]$RadiusX,
        [float[]]$RadiusY,
        [float]$CenterX = 0.0,
        [float[]]$CenterY = @(),
        [int]$Segments = 32
    )
    if ($Z.Count -lt 2 -or
        $Z.Count -ne $RadiusX.Count -or
        $Z.Count -ne $RadiusY.Count -or
        ($CenterY.Count -ne 0 -and $CenterY.Count -ne $Z.Count) -or
        $Segments -lt 3) {
        throw "Showcase horizontal loft requires matching bounded profiles."
    }
    for ($profileIndex = 0; $profileIndex -lt $Z.Count; ++$profileIndex) {
        if ($RadiusX[$profileIndex] -le 0.0 -or $RadiusY[$profileIndex] -le 0.0) {
            throw "Showcase horizontal loft profile has an invalid radius."
        }
        if ($profileIndex -gt 0 -and $Z[$profileIndex] -le $Z[$profileIndex - 1]) {
            throw "Showcase horizontal loft profile is not strictly increasing."
        }
    }
    $centerYProfiles = if ($CenterY.Count -eq 0) {
        @($Z | ForEach-Object { [float]0.0 })
    }
    else {
        @($CenterY)
    }
    $rings = New-Object 'System.Collections.Generic.List[int]'
    $firstZ = [float]$Z[0]
    $lastZ = [float]$Z[$Z.Count - 1]
    for ($profileIndex = 0; $profileIndex -lt $Z.Count; ++$profileIndex) {
        if ($profileIndex -eq 0) {
            $previous = 0
            $next = 1
        }
        elseif ($profileIndex -eq ($Z.Count - 1)) {
            $previous = $profileIndex - 1
            $next = $profileIndex
        }
        else {
            $previous = $profileIndex - 1
            $next = $profileIndex + 1
        }
        $slopeX = ($RadiusX[$next] - $RadiusX[$previous]) /
            [Math]::Max(0.000001, ($Z[$next] - $Z[$previous]))
        $slopeY = ($RadiusY[$next] - $RadiusY[$previous]) /
            [Math]::Max(0.000001, ($Z[$next] - $Z[$previous]))
        $centerSlopeY = if ($CenterY.Count -eq 0) { 0.0 } else {
            ($centerYProfiles[$next] - $centerYProfiles[$previous]) /
                [Math]::Max(0.000001, ($Z[$next] - $Z[$previous]))
        }
        $yCenter = [float]$centerYProfiles[$profileIndex]
        for ($segment = 0; $segment -le $Segments; ++$segment) {
            $phi = 2.0 * [Math]::PI * $segment / $Segments
            $sinPhi = [Math]::Sin($phi)
            $cosPhi = [Math]::Cos($phi)
            if ([Math]::Abs($sinPhi) -lt 0.000001) { $sinPhi = 0.0 }
            if ([Math]::Abs($cosPhi) -lt 0.000001) { $cosPhi = 0.0 }
            $normal = Normalize-Vector @(
                [float]($cosPhi / $RadiusX[$profileIndex]),
                [float]($sinPhi / $RadiusY[$profileIndex]),
                [float](-($slopeX * $cosPhi * $cosPhi / $RadiusX[$profileIndex]) -
                    (($centerSlopeY + $slopeY * $sinPhi) * $sinPhi / $RadiusY[$profileIndex])))
            $position = @(
                [float]($CenterX + $RadiusX[$profileIndex] * $cosPhi),
                [float]($yCenter + $RadiusY[$profileIndex] * $sinPhi),
                [float]$Z[$profileIndex])
            $tangent = Normalize-Vector @(
                [float](-$RadiusX[$profileIndex] * $sinPhi),
                [float]($RadiusY[$profileIndex] * $cosPhi),
                0.0)
            [void]$rings.Add((Add-Vertex $Part $position $normal @(
                [float]($segment / $Segments),
                [float](($Z[$profileIndex] - $firstZ) / ($lastZ - $firstZ))) @(
                    [float]$tangent[0], [float]$tangent[1], [float]$tangent[2], 1.0)))
        }
    }
    for ($profileIndex = 0; $profileIndex -lt ($Z.Count - 1); ++$profileIndex) {
        $ringOffset = $profileIndex * ($Segments + 1)
        $nextRingOffset = ($profileIndex + 1) * ($Segments + 1)
        for ($segment = 0; $segment -lt $Segments; ++$segment) {
            $a = $rings[$ringOffset + $segment]
            $b = $rings[$ringOffset + $segment + 1]
            $c = $rings[$nextRingOffset + $segment + 1]
            $d = $rings[$nextRingOffset + $segment]
            Add-Triangle $Part $a $b $c
            Add-Triangle $Part $a $c $d
        }
    }
    $rearCenter = Add-Vertex $Part @([float]$CenterX, [float]$centerYProfiles[0], [float]$firstZ) @(0.0, 0.0, -1.0) @(0.5, 0.0) @(1.0, 0.0, 0.0, 1.0)
    $frontCenter = Add-Vertex $Part @([float]$CenterX, [float]$centerYProfiles[$centerYProfiles.Count - 1], [float]$lastZ) @(0.0, 0.0, 1.0) @(0.5, 1.0) @(1.0, 0.0, 0.0, 1.0)
    for ($segment = 0; $segment -lt $Segments; ++$segment) {
        $rearA = $rings[$segment]
        $rearB = $rings[$segment + 1]
        $frontOffset = ($Z.Count - 1) * ($Segments + 1)
        $frontA = $rings[$frontOffset + $segment]
        $frontB = $rings[$frontOffset + $segment + 1]
        Add-Triangle $Part $rearCenter $rearB $rearA
        Add-Triangle $Part $frontCenter $frontA $frontB
    }
}

function Add-CurvedLimb {
    param(
        [object]$Part,
        [float[]]$Y,
        [float[]]$CenterX,
        [float[]]$CenterZ,
        [float[]]$RadiusX,
        [float[]]$RadiusZ,
        [int]$Segments = 32
    )
    if ($Y.Count -lt 2 -or
        $Y.Count -ne $CenterX.Count -or
        $Y.Count -ne $CenterZ.Count -or
        $Y.Count -ne $RadiusX.Count -or
        $Y.Count -ne $RadiusZ.Count -or
        $Segments -lt 8) {
        throw "Showcase curved limb requires matching bounded profiles."
    }
    for ($profileIndex = 1; $profileIndex -lt $Y.Count; ++$profileIndex) {
        if ($Y[$profileIndex] -le $Y[$profileIndex - 1] -or
            $RadiusX[$profileIndex] -le 0.0 -or
            $RadiusZ[$profileIndex] -le 0.0) {
            throw "Showcase curved limb profile is not strictly increasing and positive."
        }
    }
    if ($RadiusX[0] -le 0.0 -or $RadiusZ[0] -le 0.0) {
        throw "Showcase curved limb profile has an invalid base radius."
    }
    $rings = New-Object 'System.Collections.Generic.List[int]'
    for ($profileIndex = 0; $profileIndex -lt $Y.Count; ++$profileIndex) {
        $previous = [Math]::Max(0, $profileIndex - 1)
        $next = [Math]::Min($Y.Count - 1, $profileIndex + 1)
        $pathSlopeX = ($CenterX[$next] - $CenterX[$previous]) / ($Y[$next] - $Y[$previous])
        $pathSlopeZ = ($CenterZ[$next] - $CenterZ[$previous]) / ($Y[$next] - $Y[$previous])
        for ($segment = 0; $segment -le $Segments; ++$segment) {
            $phi = 2.0 * [Math]::PI * $segment / $Segments
            $cosPhi = [Math]::Cos($phi)
            $sinPhi = [Math]::Sin($phi)
            $tangentPhi = @(
                [float](-$RadiusX[$profileIndex] * $sinPhi),
                0.0,
                [float]($RadiusZ[$profileIndex] * $cosPhi))
            $pathTangent = @([float]$pathSlopeX, 1.0, [float]$pathSlopeZ)
            $normal = Normalize-Vector (Cross-Vector $pathTangent $tangentPhi)
            $position = @(
                [float]($CenterX[$profileIndex] + $RadiusX[$profileIndex] * $cosPhi),
                [float]$Y[$profileIndex],
                [float]($CenterZ[$profileIndex] + $RadiusZ[$profileIndex] * $sinPhi))
            $tangent = Normalize-Vector $tangentPhi
            [void]$rings.Add((Add-Vertex $Part $position $normal @(
                    [float]($segment / $Segments),
                    [float]($profileIndex / ($Y.Count - 1))) @(
                    [float]$tangent[0],
                    [float]$tangent[1],
                    [float]$tangent[2],
                    1.0)))
        }
    }
    for ($profileIndex = 0; $profileIndex -lt ($Y.Count - 1); ++$profileIndex) {
        for ($segment = 0; $segment -lt $Segments; ++$segment) {
            $a = $rings[$profileIndex * ($Segments + 1) + $segment]
            $b = $rings[$profileIndex * ($Segments + 1) + $segment + 1]
            $c = $rings[($profileIndex + 1) * ($Segments + 1) + $segment + 1]
            $d = $rings[($profileIndex + 1) * ($Segments + 1) + $segment]
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

function Scale-Part-Uniform {
    param([object]$Part, [float]$Scale)
    if ($Scale -le 0.0 -or [double]::IsNaN($Scale) -or [double]::IsInfinity($Scale)) {
        throw "Showcase uniform scale must be finite and positive."
    }
    foreach ($vertex in $Part.Vertices) {
        $vertex.Position = @(
            [float]($vertex.Position[0] * $Scale),
            [float]($vertex.Position[1] * $Scale),
            [float]($vertex.Position[2] * $Scale))
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
        [int]$BaseColorTextureIndex = -1,
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
    if ($BaseColorTextureIndex -ge 0) {
        $material.pbrMetallicRoughness.Add("baseColorTexture", [ordered]@{
            index = $BaseColorTextureIndex
        })
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

function Get-ShowcaseLatticeValue {
    param(
        [int]$X,
        [int]$Y,
        [int]$CellsX,
        [int]$CellsY,
        [int]$Seed
    )
    if ($CellsX -lt 1 -or $CellsY -lt 1) {
        throw "Showcase texture noise requires positive lattice dimensions."
    }
    $wrappedX = $X % $CellsX
    $wrappedY = $Y % $CellsY
    if ($wrappedX -lt 0) { $wrappedX += $CellsX }
    if ($wrappedY -lt 0) { $wrappedY += $CellsY }
    $hash = [Math]::Sin(
        ($wrappedX * 127.1) +
        ($wrappedY * 311.7) +
        ($Seed * 74.3)) * 43758.5453
    return $hash - [Math]::Floor($hash)
}

function Get-ShowcaseTileableNoise {
    param(
        [double]$U,
        [double]$V,
        [int]$CellsX,
        [int]$CellsY,
        [int]$Seed
    )
    if ($U -lt 0.0 -or $U -gt 1.0 -or $V -lt 0.0 -or $V -gt 1.0) {
        throw "Showcase texture noise coordinates must be normalized."
    }
    $x = $U * $CellsX
    $y = $V * $CellsY
    $x0 = [int][Math]::Floor($x)
    $y0 = [int][Math]::Floor($y)
    $tx = $x - $x0
    $ty = $y - $y0
    $fadeX = $tx * $tx * (3.0 - (2.0 * $tx))
    $fadeY = $ty * $ty * (3.0 - (2.0 * $ty))
    $v00 = Get-ShowcaseLatticeValue $x0 $y0 $CellsX $CellsY $Seed
    $v10 = Get-ShowcaseLatticeValue ($x0 + 1) $y0 $CellsX $CellsY $Seed
    $v01 = Get-ShowcaseLatticeValue $x0 ($y0 + 1) $CellsX $CellsY $Seed
    $v11 = Get-ShowcaseLatticeValue ($x0 + 1) ($y0 + 1) $CellsX $CellsY $Seed
    $xTop = $v00 + (($v10 - $v00) * $fadeX)
    $xBottom = $v01 + (($v11 - $v01) * $fadeX)
    return $xTop + (($xBottom - $xTop) * $fadeY)
}

function Get-ShowcaseDetailNoise {
    param(
        [double]$U,
        [double]$V,
        [int]$Seed
    )
    $coarse = Get-ShowcaseTileableNoise $U $V 7 5 $Seed
    $fine = Get-ShowcaseTileableNoise $U $V 19 17 ($Seed + 17)
    return (0.62 * $coarse) + (0.38 * $fine)
}

function Write-ShowcaseTexture {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][ValidateSet("base_color", "normal", "metallic_roughness")][string]$Kind,
        [Parameter(Mandatory = $true)][ValidateSet("giraffe", "rocket")][string]$Subject
    )
    Add-Type -AssemblyName System.Drawing
    $size = if ($Kind -eq "base_color") { 256 } else { 64 }
    $bitmap = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $giraffeSeeds = @(
        [pscustomobject]@{ U = 0.08; V = 0.11; Ru = 0.075; Rv = 0.055; Angle = 0.30 },
        [pscustomobject]@{ U = 0.26; V = 0.07; Ru = 0.100; Rv = 0.070; Angle = -0.25 },
        [pscustomobject]@{ U = 0.48; V = 0.13; Ru = 0.085; Rv = 0.065; Angle = 0.15 },
        [pscustomobject]@{ U = 0.73; V = 0.08; Ru = 0.120; Rv = 0.060; Angle = -0.35 },
        [pscustomobject]@{ U = 0.91; V = 0.16; Ru = 0.070; Rv = 0.090; Angle = 0.45 },
        [pscustomobject]@{ U = 0.15; V = 0.28; Ru = 0.110; Rv = 0.075; Angle = -0.50 },
        [pscustomobject]@{ U = 0.39; V = 0.25; Ru = 0.075; Rv = 0.110; Angle = 0.10 },
        [pscustomobject]@{ U = 0.63; V = 0.31; Ru = 0.120; Rv = 0.080; Angle = 0.60 },
        [pscustomobject]@{ U = 0.84; V = 0.27; Ru = 0.090; Rv = 0.060; Angle = -0.20 },
        [pscustomobject]@{ U = 0.05; V = 0.45; Ru = 0.100; Rv = 0.080; Angle = 0.70 },
        [pscustomobject]@{ U = 0.25; V = 0.42; Ru = 0.080; Rv = 0.060; Angle = -0.15 },
        [pscustomobject]@{ U = 0.52; V = 0.48; Ru = 0.130; Rv = 0.090; Angle = -0.40 },
        [pscustomobject]@{ U = 0.78; V = 0.43; Ru = 0.080; Rv = 0.110; Angle = 0.25 },
        [pscustomobject]@{ U = 0.96; V = 0.52; Ru = 0.110; Rv = 0.070; Angle = -0.55 },
        [pscustomobject]@{ U = 0.13; V = 0.65; Ru = 0.090; Rv = 0.060; Angle = 0.25 },
        [pscustomobject]@{ U = 0.35; V = 0.60; Ru = 0.120; Rv = 0.080; Angle = -0.70 },
        [pscustomobject]@{ U = 0.61; V = 0.68; Ru = 0.090; Rv = 0.110; Angle = 0.40 },
        [pscustomobject]@{ U = 0.86; V = 0.63; Ru = 0.130; Rv = 0.070; Angle = -0.10 },
        [pscustomobject]@{ U = 0.05; V = 0.82; Ru = 0.080; Rv = 0.100; Angle = -0.30 },
        [pscustomobject]@{ U = 0.24; V = 0.87; Ru = 0.120; Rv = 0.065; Angle = 0.55 },
        [pscustomobject]@{ U = 0.49; V = 0.81; Ru = 0.085; Rv = 0.100; Angle = -0.20 },
        [pscustomobject]@{ U = 0.72; V = 0.91; Ru = 0.110; Rv = 0.075; Angle = 0.35 },
        [pscustomobject]@{ U = 0.94; V = 0.84; Ru = 0.080; Rv = 0.110; Angle = -0.65 },
        [pscustomobject]@{ U = 0.62; V = 0.04; Ru = 0.080; Rv = 0.055; Angle = 0.50 },
        # Secondary off-grid cells break up large contiguous patches without
        # introducing a repeated stamp cadence on the neck or ribcage.
        [pscustomobject]@{ U = 0.31; V = 0.18; Ru = 0.055; Rv = 0.045; Angle = 0.80 },
        [pscustomobject]@{ U = 0.68; V = 0.20; Ru = 0.060; Rv = 0.050; Angle = -0.35 },
        [pscustomobject]@{ U = 0.02; V = 0.34; Ru = 0.050; Rv = 0.040; Angle = -0.70 },
        [pscustomobject]@{ U = 0.52; V = 0.36; Ru = 0.058; Rv = 0.045; Angle = 0.25 },
        [pscustomobject]@{ U = 0.91; V = 0.39; Ru = 0.052; Rv = 0.060; Angle = -0.50 },
        [pscustomobject]@{ U = 0.18; V = 0.55; Ru = 0.060; Rv = 0.045; Angle = 0.40 },
        [pscustomobject]@{ U = 0.70; V = 0.57; Ru = 0.050; Rv = 0.058; Angle = -0.15 },
        [pscustomobject]@{ U = 0.42; V = 0.73; Ru = 0.055; Rv = 0.042; Angle = 0.65 },
        [pscustomobject]@{ U = 0.98; V = 0.74; Ru = 0.048; Rv = 0.055; Angle = -0.25 },
        [pscustomobject]@{ U = 0.06; V = 0.93; Ru = 0.052; Rv = 0.045; Angle = 0.15 },
        [pscustomobject]@{ U = 0.57; V = 0.96; Ru = 0.060; Rv = 0.040; Angle = -0.60 },
        [pscustomobject]@{ U = 0.84; V = 0.94; Ru = 0.050; Rv = 0.052; Angle = 0.35 })
    try {
        for ($y = 0; $y -lt $size; ++$y) {
            for ($x = 0; $x -lt $size; ++$x) {
                $u = ($x + 0.5) / $size
                $v = ($y + 0.5) / $size
                if ($Kind -eq "base_color") {
                    if ($Subject -eq "giraffe") {
                        # Hand-authored deterministic seed patches avoid the
                        # rows, columns, and repeated stamps that made the
                        # earlier hide field look procedural at close range.
                        $nearestDistance = [double]::PositiveInfinity
                        $nearestSeed = 0.0
                        foreach ($seed in $giraffeSeeds) {
                            $seedU = [float]$seed.U
                            $seedV = [float]$seed.V
                            $seedRu = [float]$seed.Ru
                            $seedRv = [float]$seed.Rv
                            $seedAngle = [float]$seed.Angle
                            $deltaU = $u - $seedU
                            $deltaV = $v - $seedV
                            $cosAngle = [Math]::Cos($seedAngle)
                            $sinAngle = [Math]::Sin($seedAngle)
                            $localU = ($deltaU * $cosAngle) + ($deltaV * $sinAngle)
                            $localV = (-$deltaU * $sinAngle) + ($deltaV * $cosAngle)
                            $normalizedU = $localU / $seedRu
                            $normalizedV = $localV / $seedRv
                            $shapeDistance =
                                [Math]::Pow([Math]::Abs($normalizedU), 2.60) +
                                [Math]::Pow([Math]::Abs($normalizedV), 2.60)
                            $edgeWarp =
                                (0.13 * [Math]::Sin(($localU * 37.0) + ($localV * 19.0) + ($seedU * 17.0))) +
                                (0.07 * [Math]::Cos(($localU * 71.0) - ($localV * 43.0) + ($seedV * 23.0)))
                            $normalizedDistance = $shapeDistance + $edgeWarp
                            if ($normalizedDistance -lt $nearestDistance) {
                                $nearestDistance = $normalizedDistance
                                $nearestSeed = 0.5 + (0.5 * [Math]::Sin(($seedU * 127.1) + ($seedV * 311.7)))
                            }
                        }
                        $spot = $nearestDistance -lt 0.30
                        $surfaceGrain = 0.5 + (0.5 * [Math]::Sin(($u * 97.0) + ($v * 61.0) + ($nearestSeed * 11.0)))
                        if ($spot) {
                            $red = 67 + [int][Math]::Round(27.0 * $nearestSeed + 6.0 * $surfaceGrain)
                            $green = 30 + [int][Math]::Round(15.0 * $nearestSeed + 4.0 * $surfaceGrain)
                            $blue = 8 + [int][Math]::Round(8.0 * $nearestSeed + 3.0 * $surfaceGrain)
                        }
                        else {
                            $red = 181 + [int][Math]::Round(28.0 * $nearestSeed + 7.0 * $surfaceGrain)
                            $green = 128 + [int][Math]::Round(23.0 * $nearestSeed + 6.0 * $surfaceGrain)
                            $blue = 54 + [int][Math]::Round(16.0 * $nearestSeed + 5.0 * $surfaceGrain)
                        }
                    }
                    else {
                        # Rocket paint uses a cool ceramic panel variation, not
                        # the giraffe spot field. Keep the texture close to
                        # neutral so the material factor remains authoritative
                        # while subtle panel-scale variation catches lighting.
                        $panelVariation = 0.5 + 0.5 * [Math]::Sin(($u * 6.0 * [Math]::PI) + ($v * 0.8))
                        $surfaceVariation = 0.5 + 0.5 * [Math]::Sin(($x + 2) * 0.19 + ($y + 5) * 0.07)
                        $red = 184 + [int][Math]::Round(16.0 * $panelVariation + 7.0 * $surfaceVariation)
                        $green = 191 + [int][Math]::Round(14.0 * $panelVariation + 6.0 * $surfaceVariation)
                        $blue = 196 + [int][Math]::Round(12.0 * $panelVariation + 5.0 * $surfaceVariation)
                    }
                }
                elseif ($Kind -eq "normal") {
                    $seed = if ($Subject -eq "giraffe") { 19 } else { 47 }
                    $nxField = Get-ShowcaseDetailNoise $u $v $seed
                    $nyField = Get-ShowcaseDetailNoise $v $u ($seed + 13)
                    $nx = 0.10 * (($nxField * 2.0) - 1.0)
                    $ny = 0.08 * (($nyField * 2.0) - 1.0)
                    $nz = [Math]::Sqrt([Math]::Max(0.0, 1.0 - ($nx * $nx) - ($ny * $ny)))
                    $red = [int][Math]::Round(128.0 + (127.0 * $nx))
                    $green = [int][Math]::Round(128.0 + (127.0 * $ny))
                    $blue = [int][Math]::Round(255.0 * $nz)
                }
                else {
                    $variation = Get-ShowcaseDetailNoise $u $v $(if ($Subject -eq "giraffe") { 73 } else { 101 })
                    $microVariation = Get-ShowcaseTileableNoise $u $v 43 37 $(if ($Subject -eq "giraffe") { 89 } else { 127 })
                    $variation = (0.78 * $variation) + (0.22 * $microVariation)
                    $roughness = if ($Subject -eq "giraffe") {
                        118.0 + (64.0 * $variation)
                    }
                    else {
                        88.0 + (86.0 * $variation)
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
        [Parameter(Mandatory = $true)][ValidateSet("giraffe", "rocket")][string]$Subject,
        [switch]$IncludeBaseColor
    )
    $normalName = "${Subject}_detail_normal.png"
    $metallicRoughnessName = "${Subject}_metallic_roughness.png"
    Write-ShowcaseTexture (Join-Path $OutputDirectory $normalName) "normal" $Subject
    Write-ShowcaseTexture (Join-Path $OutputDirectory $metallicRoughnessName) "metallic_roughness" $Subject
    $definitions = @(
        ([ordered]@{ uri = $normalName }),
        ([ordered]@{ uri = $metallicRoughnessName }))
    if ($IncludeBaseColor) {
        $baseColorName = "${Subject}_base_color.png"
        Write-ShowcaseTexture (Join-Path $OutputDirectory $baseColorName) "base_color" $Subject
        $definitions += ,([ordered]@{ uri = $baseColorName })
    }
    return $definitions
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
        (New-Material "Giraffe Tan" @(1.0, 1.0, 1.0, 1.0) 0.0 0.82 0.0 0.62 @(0.028, 0.009, 0.002) 0.66 -BaseColorTextureIndex 2 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Spots" @(0.075, 0.025, 0.006, 1.0) 0.0 0.84 -NormalTextureIndex 0 -NormalTextureScale 0.10 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Cream" @(0.54, 0.33, 0.14, 1.0) 0.0 0.76 0.0 0.58 @(0.045, 0.018, 0.005) 0.68 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Eye White" @(0.018, 0.006, 0.002, 1.0) 0.0 0.24 0.08 0.24),
        (New-Material "Giraffe Iris" @(0.005, 0.001, 0.0005, 1.0) 0.0 0.18 0.04 0.12),
        (New-Material "Giraffe Eye Detail" @(0.004, 0.002, 0.001, 1.0) 0.0 0.10 0.70 0.06),
        (New-Material "Giraffe Smile" @(0.11, 0.015, 0.008, 1.0) 0.0 0.34 0.05 0.22),
        (New-Material "Giraffe Ear Inner" @(0.30, 0.065, 0.025, 1.0) 0.0 0.44 0.03 0.18 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Ossicone Cap" @(0.09, 0.020, 0.004, 1.0) 0.0 0.52 0.02 0.24 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Hoof" @(0.055, 0.012, 0.004, 1.0) 0.0 0.66 0.01 0.26 -NormalTextureIndex 0 -NormalTextureScale 0.10 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Mane" @(0.075, 0.014, 0.003, 1.0) 0.0 0.74 0.01 0.30 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Nose" @(0.055, 0.006, 0.002, 1.0) 0.0 0.58 0.01 0.22 -NormalTextureIndex 0 -NormalTextureScale 0.10 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Joint" @(0.20, 0.085, 0.022, 1.0) 0.0 0.64 0.01 0.24 -NormalTextureIndex 0 -NormalTextureScale 0.10 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Shoulder" @(1.0, 1.0, 1.0, 1.0) 0.0 0.80 0.0 0.58 -BaseColorTextureIndex 2 -NormalTextureIndex 0 -NormalTextureScale 0.10 -MetallicRoughnessTextureIndex 1),
        (New-Material "Giraffe Tail Tuft" @(0.060, 0.012, 0.003, 1.0) 0.0 0.78 0.01 0.28 -NormalTextureIndex 0 -NormalTextureScale 0.10 -MetallicRoughnessTextureIndex 1))
    $tan = New-Part 0
    $spots = New-Part 1
    $cream = New-Part 2
    $eyes = New-Part 3
    $iris = New-Part 4
    $details = New-Part 5
    $smile = New-Part 6
    $earInner = New-Part 7
    $ossicone = New-Part 8
    $hoof = New-Part 9
    $mane = New-Part 10
    $nose = New-Part 11
    $joint = New-Part 12
    $shoulder = New-Part 13
    $tailTuft = New-Part 14
    # Keep the mascot identity restrained, but use continuous profiles and
    # believable curvature so the silhouette does not read as primitive
    # assembly when inspected from the authored front and three-quarter views.
    # The body is a horizontal ribcage with a distinct forward neck. Earlier
    # versions made a single upright pear-shaped loft, which read as a toy
    # rather than a four-legged animal from three-quarter views.
    Add-HorizontalLoft $tan `
        @(-1.23, -1.18, -1.08, -0.92, -0.72, -0.48, -0.20, 0.08, 0.34, 0.56, 0.74, 0.88, 0.96, 1.02, 1.06, 1.08) `
        @(0.20, 0.34, 0.44, 0.50, 0.53, 0.54, 0.55, 0.55, 0.53, 0.49, 0.44, 0.38, 0.30, 0.24, 0.18, 0.14) `
        @(0.22, 0.34, 0.45, 0.52, 0.57, 0.59, 0.60, 0.58, 0.54, 0.49, 0.43, 0.36, 0.30, 0.24, 0.18, 0.12) `
        0.0 @(1.60, 1.60, 1.62, 1.65, 1.68, 1.70, 1.71, 1.72, 1.70, 1.68, 1.65, 1.63, 1.60, 1.57, 1.54, 1.52) 192
    Add-AnatomicalLoft $tan `
        @(1.86, 2.12, 2.46, 2.82, 3.18, 3.50, 3.74) `
        @(0.34, 0.30, 0.265, 0.235, 0.215, 0.205, 0.22) `
        @(0.36, 0.32, 0.28, 0.245, 0.22, 0.205, 0.24) `
        0.0 @(0.60, 0.67, 0.74, 0.80, 0.86, 0.92, 0.98) 112
    # Shoulder and haunch transitions are integrated lofts, not floating
    # spheres. They add the broad muscle planes that keep the torso from
    # reading as a single inflated mascot body in close views.
    Add-AnatomicalLoft $shoulder `
        @(0.82, 1.02, 1.22, 1.42, 1.58, 1.68) `
        @(0.20, 0.28, 0.34, 0.32, 0.24, 0.12) `
        @(0.22, 0.31, 0.38, 0.34, 0.24, 0.12) `
        0.0 @(0.45, 0.52, 0.58, 0.58, 0.55, 0.50) 64
    Add-AnatomicalLoft $shoulder `
        @(0.72, 0.94, 1.18, 1.40, 1.56, 1.66) `
        @(0.22, 0.30, 0.37, 0.36, 0.28, 0.14) `
        @(0.26, 0.36, 0.43, 0.40, 0.30, 0.14) `
        0.0 @(-0.68, -0.72, -0.74, -0.72, -0.68, -0.62) 64
    # Keep the head subordinate to the long neck and torso. The earlier
    # mascot-scale head overwhelmed the silhouette even after the torso loft
    # was made continuous.
    Add-HorizontalLoft $tan `
        @(0.72, 0.82, 0.96, 1.12, 1.28, 1.42, 1.54) `
        @(0.18, 0.27, 0.34, 0.37, 0.34, 0.27, 0.12) `
        @(0.15, 0.22, 0.28, 0.30, 0.26, 0.20, 0.10) `
        0.0 @(3.90, 3.91, 3.92, 3.91, 3.86, 3.80, 3.75) 320
    foreach ($leg in @(
            [pscustomobject]@{ X = -0.46; Z = -0.82 },
            [pscustomobject]@{ X = 0.46; Z = -0.82 },
            [pscustomobject]@{ X = -0.46; Z = 0.72 },
            [pscustomobject]@{ X = 0.46; Z = 0.72 })) {
        $legXBase = [float]$leg.X
        $legZBase = [float]$leg.Z
        $legY = @(0.08, 0.30, 0.72, 1.08, 1.40, 1.62)
        $legX = @(
            $legXBase,
            ($legXBase * [float]1.02),
            ($legXBase * [float]1.00),
            ($legXBase * [float]0.94),
            ($legXBase * [float]0.78),
            ($legXBase * [float]0.58))
        $isHindLeg = $legZBase -lt 0.0
        $legZ = if ($isHindLeg) {
            @(
                $legZBase,
                ($legZBase * [float]1.06),
                ($legZBase * [float]1.10),
                ($legZBase * [float]0.96),
                ($legZBase * [float]0.76),
                ($legZBase * [float]0.68))
        }
        else {
            @(
                $legZBase,
                ($legZBase * [float]1.01),
                ($legZBase * [float]0.98),
                ($legZBase * [float]0.92),
                ($legZBase * [float]0.80),
                ($legZBase * [float]0.68))
        }
        Add-CurvedLimb $tan $legY $legX $legZ @(0.12, 0.13, 0.125, 0.13, 0.20, 0.28) @(0.13, 0.14, 0.13, 0.14, 0.23, 0.32) 48
        Add-Ellipsoid $hoof @($legXBase, 0.08, $legZBase + 0.015) @(0.13, 0.055, 0.17) 16 28
        Add-Ellipsoid $joint @($legXBase, 0.88, $legZBase + 0.012) @(0.125, 0.085, 0.11) 12 22
    }
    # Model the ears as compact, flattened lobes in the head plane. The
    # previous deep capsules read like small missiles when viewed from the
    # front, especially with the inner-ear patch sitting on their top edge.
    Add-Ellipsoid $tan @(-0.39, 4.16, 0.94) @(0.17, 0.12, 0.070) 16 32
    Add-Ellipsoid $tan @(0.39, 4.16, 0.94) @(0.17, 0.12, 0.070) 16 32
    Add-Frustum $tan 4.18 4.49 0.062 0.050 -0.16 0.96 16
    Add-Frustum $tan 4.18 4.49 0.062 0.050 0.16 0.96 16
    Add-Ellipsoid $earInner @(-0.39, 4.16, 1.010) @(0.080, 0.105, 0.018) 10 20
    Add-Ellipsoid $earInner @(0.39, 4.16, 1.010) @(0.080, 0.105, 0.018) 10 20
    # Ossicones use a visible tan stalk and a short cap angled outward from
    # the head. The previous isolated ellipsoids read as floating pegs and
    # gave the mascot an unintended, lopsided silhouette in front views.
    Add-Frustum $tan 4.30 4.56 0.050 0.040 -0.16 0.96 16
    Add-Frustum $tan 4.30 4.56 0.050 0.040 0.16 0.96 16
    Add-OrientedCone $ossicone @(-0.16, 4.52, 0.96) @(-0.22, 4.76, 0.95) 0.065 16
    Add-OrientedCone $ossicone @(0.16, 4.52, 0.96) @(0.22, 4.76, 0.95) 0.065 16
    # A short mane row gives the neck a readable rear contour without making
    # the mascot realistic in the photographic sense.
    foreach ($maneY in @(2.08, 2.30, 2.52, 2.74, 2.96, 3.18, 3.40, 3.60)) {
        Add-Ellipsoid $mane @(0.0, $maneY, 0.50 + (($maneY - 2.08) * 0.17)) @(0.075, 0.13, 0.040) 10 20
    }
    # A short articulated tail restores a key rear-body relationship without
    # introducing a disconnected decorative peg.
    Add-OrientedCone $tan @(0.0, 1.56, -1.16) @(0.08, 1.84, -1.42) 0.060 16
    Add-OrientedCone $spots @(0.08, 1.84, -1.42) @(0.10, 2.02, -1.52) 0.075 16
    Add-OrientedCone $tailTuft @(0.10, 2.02, -1.52) @(0.11, 2.28, -1.59) 0.115 20
    # The face is built as an elongated muzzle with small, recessed dark eyes
    # and a neutral lip crease. The model intentionally avoids oversized
    # highlights or a smiling mouth so its expression remains anatomical.
    Add-HorizontalLoft $cream `
        @(1.34, 1.44, 1.58, 1.72, 1.86, 1.94) `
        @(0.20, 0.23, 0.22, 0.17, 0.13, 0.08) `
        @(0.12, 0.14, 0.13, 0.10, 0.075, 0.045) `
        0.0 @(3.68, 3.67, 3.65, 3.62, 3.60, 3.59) 64
    Add-Ellipsoid $eyes @(-0.18, 4.02, 1.39) @(0.045, 0.050, 0.018) 16 28
    Add-Ellipsoid $eyes @(0.18, 4.02, 1.39) @(0.045, 0.050, 0.018) 16 28
    Add-Ellipsoid $iris @(-0.18, 4.02, 1.414) @(0.023, 0.031, 0.008) 12 24
    Add-Ellipsoid $iris @(0.18, 4.02, 1.414) @(0.023, 0.031, 0.008) 12 24
    Add-Ellipsoid $details @(-0.18, 4.055, 1.407) @(0.018, 0.006, 0.006) 8 18
    Add-Ellipsoid $details @(0.18, 4.055, 1.407) @(0.018, 0.006, 0.006) 8 18
    Add-Ellipsoid $details @(-0.075, 3.69, 1.895) @(0.030, 0.018, 0.010) 8 14
    Add-Ellipsoid $details @(0.075, 3.69, 1.895) @(0.030, 0.018, 0.010) 8 14
    Add-Ellipsoid $smile @(0.0, 3.53, 1.855) @(0.065, 0.008, 0.006) 8 16
    Add-Ellipsoid $nose @(-0.075, 3.69, 1.915) @(0.024, 0.018, 0.012) 10 20
    Add-Ellipsoid $nose @(0.075, 3.69, 1.915) @(0.024, 0.018, 0.012) 10 20
    return [pscustomobject]@{ Parts = @($tan, $spots, $cream, $eyes, $iris, $details, $smile, $earInner, $ossicone, $hoof, $mane, $nose, $joint, $shoulder, $tailTuft); Materials = $materials }
}

function New-Rocket {
    $materials = @(
        (New-Material "Rocket Painted Ceramic" @(0.96, 0.96, 0.94, 1.0) 0.12 0.36 0.24 0.18 -BaseColorTextureIndex 2 -NormalTextureIndex 0 -NormalTextureScale 0.14 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Brushed Metal" @(0.20, 0.22, 0.24, 1.0) 0.86 0.24 0.08 0.20 -NormalTextureIndex 0 -NormalTextureScale 0.14 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Heat Shield" @(0.035, 0.042, 0.050, 1.0) 0.62 0.38 0.0 0.20 @(0.0, 0.0, 0.0) 0.50 @(0.18, 0.025, 0.005) 0.40 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Mission Stripe" @(0.22, 0.12, 0.045, 1.0) 0.18 0.38 0.08 0.22 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Avionics" @(0.018, 0.024, 0.034, 1.0) 0.72 0.28 0.10 0.18 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Launch Pad" @(0.075, 0.090, 0.105, 1.0) 0.32 0.68 0.10 0.16 -NormalTextureIndex 0 -NormalTextureScale 0.10 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Fastener" @(0.20, 0.23, 0.28, 1.0) 0.78 0.22 0.12 0.18 -NormalTextureIndex 0 -NormalTextureScale 0.10 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Thermal Detail" @(0.025, 0.032, 0.040, 1.0) 0.38 0.58 0.02 0.22 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Engine Bell" @(0.16, 0.19, 0.23, 1.0) 0.76 0.30 0.04 0.22 -NormalTextureIndex 0 -NormalTextureScale 0.14 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Panel Detail" @(0.075, 0.090, 0.105, 1.0) 0.56 0.34 0.03 0.20 -NormalTextureIndex 0 -NormalTextureScale 0.10 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Core Insulation" @(0.52, 0.16, 0.035, 1.0) 0.06 0.58 0.02 0.26 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Booster Coating" @(0.66, 0.67, 0.64, 1.0) 0.10 0.44 0.10 0.20 -NormalTextureIndex 0 -NormalTextureScale 0.12 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Service Structure" @(0.090, 0.105, 0.120, 1.0) 0.64 0.30 0.02 0.24 -NormalTextureIndex 0 -NormalTextureScale 0.10 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Avionics Hardware" @(0.035, 0.045, 0.055, 1.0) 0.82 0.25 0.02 0.20 -NormalTextureIndex 0 -NormalTextureScale 0.10 -MetallicRoughnessTextureIndex 1),
        (New-Material "Rocket Interstage Insulation" @(0.16, 0.18, 0.20, 1.0) 0.34 0.48 0.02 0.22 -NormalTextureIndex 0 -NormalTextureScale 0.10 -MetallicRoughnessTextureIndex 1))
    $paint = New-Part 0
    $metal = New-Part 1
    $heat = New-Part 2
    $stripe = New-Part 3
    $avionics = New-Part 4
    $pad = New-Part 5
    $fastener = New-Part 6
    $thermal = New-Part 7
    $engineBell = New-Part 8
    $panelDetail = New-Part 9
    $coreInsulation = New-Part 10
    $booster = New-Part 11
    $serviceStructure = New-Part 12
    $hardware = New-Part 13
    $interstageInsulation = New-Part 14
    # Staged core, interstage, and ogive-like fairing sections provide a more
    # believable modern launch-vehicle silhouette while remaining bounded.
    # The extra shoulder profiles keep the tanks from reading as one perfect
    # cylinder and give the skirt and fairing continuous, useful curvature.
    Add-ProfiledFrustum $coreInsulation @(0.38, 0.52, 0.70, 1.55, 2.42, 2.78, 3.10) @(0.40, 0.51, 0.55, 0.55, 0.54, 0.52, 0.48) 0.0 0.0 88
    Add-ProfiledFrustum $paint @(3.10, 3.28, 3.44, 3.58, 3.72, 3.88, 4.06, 4.24, 4.42, 4.58, 4.70, 4.78) @(0.48, 0.505, 0.515, 0.512, 0.495, 0.46, 0.41, 0.35, 0.28, 0.20, 0.10, 0.025) 0.0 0.0 96
    Add-Frustum $metal 0.28 0.55 0.58 0.58 0.0 0.0 40
    Add-Frustum $metal 0.55 0.64 0.58 0.54 0.0 0.0 40
    Add-Frustum $metal 1.58 1.66 0.545 0.545 0.0 0.0 40
    Add-Frustum $metal 3.15 3.25 0.54 0.54 0.0 0.0 40
    Add-Frustum $metal 3.48 3.56 0.51 0.51 0.0 0.0 40
    Add-Frustum $metal 3.74 3.82 0.49 0.49 0.0 0.0 40
    Add-Frustum $metal 4.04 4.12 0.38 0.38 0.0 0.0 40
    Add-Frustum $avionics 1.50 1.58 0.56 0.56 0.0 0.0 40
    Add-Frustum $avionics 3.40 3.48 0.51 0.51 0.0 0.0 40
    # A bounded seven-engine cluster gives the lower stage a recognizable
    # modern launch-vehicle layout instead of three red cylinders. Each
    # engine keeps a metallic throat ring, a dark heat bell, and a smaller
    # avionics insert so the cluster remains legible under the studio HDR.
    foreach ($engine in @(
            @(-0.28, 0.0), @(0.0, 0.0), @(0.28, 0.0),
            @(0.0, -0.28), @(0.0, 0.28), @(-0.20, -0.20), @(0.20, 0.20))) {
        Add-Frustum $metal 0.02 0.16 0.21 0.18 $engine[0] $engine[1] 20
        Add-Frustum $engineBell 0.14 0.40 0.16 0.105 $engine[0] $engine[1] 24
        Add-Frustum $heat 0.40 0.46 0.105 0.075 $engine[0] $engine[1] 20
        Add-Frustum $avionics 0.05 0.12 0.12 0.075 $engine[0] $engine[1] 16
        Add-Ellipsoid $heat @($engine[0], 0.02, $engine[1]) @(0.17, 0.06, 0.17) 8 16
    }
    Add-Frustum $stripe 2.35 2.54 0.535 0.535 0.0 0.0 40
    Add-ProfiledFrustum $thermal @(1.05, 1.08, 1.16, 1.19) @(0.54, 0.555, 0.555, 0.54) 0.0 0.0 56
    Add-ProfiledFrustum $thermal @(1.89, 1.92, 1.99, 2.02) @(0.535, 0.55, 0.55, 0.535) 0.0 0.0 56
    Add-ProfiledFrustum $thermal @(2.92, 2.95, 3.04, 3.07) @(0.52, 0.535, 0.535, 0.52) 0.0 0.0 56
    foreach ($ring in @(@(1.58, 0.49), @(3.48, 0.44))) {
        for ($fastenerIndex = 0; $fastenerIndex -lt 8; ++$fastenerIndex) {
            $angle = 2.0 * [Math]::PI * $fastenerIndex / 8.0
            Add-Ellipsoid $fastener @(
                [float]($ring[1] * [Math]::Cos($angle)),
                [float]$ring[0],
                [float]($ring[1] * [Math]::Sin($angle))) @(0.035, 0.052, 0.025) 10 20
        }
    }
    # Low-profile tank stringers and access-panel rails break up the painted
    # core with construction detail without turning it into a repeated debug
    # primitive grid. They sit just proud of the four visible radial sides.
    foreach ($stringer in @(
            @(-0.52, 0.0), @(-0.26, -0.455), @(0.26, -0.455), @(0.52, 0.0),
            @(0.26, 0.455), @(-0.26, 0.455))) {
        Add-Box $panelDetail @($stringer[0], 1.86, $stringer[1]) @(0.014, 0.58, 0.014)
    }
    foreach ($railY in @(1.12, 1.38, 2.10, 2.36, 2.76)) {
        Add-Box $panelDetail @(0.0, $railY, 0.548) @(0.27, 0.012, 0.014)
        Add-Box $panelDetail @(0.0, $railY, -0.548) @(0.27, 0.012, 0.014)
    }
    Add-Box $panelDetail @(0.0, 2.20, 0.555) @(0.035, 0.82, 0.016)
    Add-Box $panelDetail @(-0.34, 2.20, 0.435) @(0.028, 0.72, 0.015)
    Add-Box $panelDetail @(0.34, 2.20, 0.435) @(0.028, 0.72, 0.015)
    # Equipment bays and cable trays give the painted core functional surface
    # detail at inspection distance. These are bounded authored panels and
    # fasteners, not a decorative primitive grid or a named-vehicle shortcut.
    foreach ($module in @(
            @(-0.31, 1.78), @(0.31, 1.78),
            @(-0.31, 2.18), @(0.31, 2.18),
            @(-0.31, 2.58), @(0.31, 2.58))) {
        Add-Box $hardware @([float]$module[0], [float]$module[1], 0.574) @(0.070, 0.105, 0.020)
        Add-Box $hardware @([float]$module[0], [float]($module[1] + 0.115), 0.598) @(0.040, 0.012, 0.012)
        Add-Ellipsoid $hardware @([float]($module[0] - 0.050), [float]($module[1] - 0.075), 0.602) @(0.014, 0.018, 0.010) 8 16
        Add-Ellipsoid $hardware @([float]($module[0] + 0.050), [float]($module[1] - 0.075), 0.602) @(0.014, 0.018, 0.010) 8 16
    }
    Add-Box $hardware @(-0.43, 2.20, 0.574) @(0.028, 0.62, 0.018)
    Add-Box $hardware @(0.43, 2.20, 0.574) @(0.028, 0.62, 0.018)
    Add-Frustum $hardware 1.68 2.64 0.050 0.050 -0.565 0.0 16
    Add-Frustum $hardware 1.68 2.64 0.050 0.050 0.565 0.0 16
    # Thin insulation collars make the interstage and booster transitions
    # read as layered flight hardware instead of uninterrupted cylinders.
    Add-ProfiledFrustum $interstageInsulation @(2.52, 2.60, 2.68, 2.76) @(0.555, 0.565, 0.565, 0.555) 0.0 0.0 48
    foreach ($boosterX in @(-0.92, 0.92)) {
        Add-ProfiledFrustum $booster @(0.34, 0.48, 0.62, 2.40, 2.65, 2.84, 3.00, 3.24, 3.40, 3.50) @(0.22, 0.29, 0.30, 0.30, 0.285, 0.285, 0.27, 0.22, 0.14, 0.025) $boosterX 0.0 64
        Add-Frustum $metal 0.42 0.56 0.315 0.315 $boosterX 0.0 32
        Add-Frustum $metal 1.34 1.41 0.302 0.302 $boosterX 0.0 32
        Add-Frustum $metal 2.06 2.13 0.292 0.292 $boosterX 0.0 32
        Add-ProfiledFrustum $thermal @(2.49, 2.52, 2.61, 2.64) @(0.286, 0.295, 0.295, 0.286) $boosterX 0.0 40
        Add-Frustum $avionics 3.00 3.07 0.275 0.275 $boosterX 0.0 32
        Add-ProfiledFrustum $interstageInsulation @(2.46, 2.54, 2.62) @(0.296, 0.304, 0.296) $boosterX 0.0 40
        Add-Ellipsoid $heat @($boosterX, 0.30, 0.0) @(0.24, 0.06, 0.24) 10 20
        # Short radial attachment brackets make the boosters read as mounted
        # stages instead of two cylinders floating beside the core.
        Add-Box $serviceStructure @([float]($boosterX * 0.72), 1.18, 0.0) @(0.18, 0.035, 0.055)
        Add-Box $serviceStructure @([float]($boosterX * 0.72), 2.32, 0.0) @(0.18, 0.035, 0.055)
    }
    # The service structure is a generic bounded lattice-and-arm reference:
    # it gives the vehicle real launch-scale context without copying any
    # specific agency tower, branding, or mission hardware.
    foreach ($towerZ in @(-0.34, 0.34)) {
        Add-Box $serviceStructure @(-1.55, 2.10, $towerZ) @(0.065, 2.10, 0.065)
    }
    foreach ($towerY in @(0.38, 1.18, 2.04, 2.90, 3.78)) {
        Add-Box $serviceStructure @(-1.55, $towerY, 0.0) @(0.075, 0.045, 0.44)
    }
    Add-Box $serviceStructure @(-0.92, 3.14, 0.0) @(0.70, 0.060, 0.16)
    Add-Box $serviceStructure @(-0.92, 2.48, 0.0) @(0.48, 0.050, 0.12)
    Add-RadialFin $metal 1.0 0.0
    Add-RadialFin $metal -1.0 0.0
    Add-RadialFin $metal 0.0 1.0
    Add-RadialFin $metal 0.0 -1.0
    # Keep the rocket clearly larger than the giraffe in the generated asset,
    # so the relationship survives the ordinary glTF import path.
    # The bounded pad adds real launch-site context: a slab, raised flame
    # trench plate, and four hold-down towers without a full launch complex.
    Add-Box $pad @(0.0, -0.18, 0.0) @(1.30, 0.10, 1.30)
    Add-Box $pad @(0.0, -0.045, 0.0) @(0.78, 0.035, 0.78)
    foreach ($tower in @(
            @(-0.92, -0.92), @(0.92, -0.92),
            @(-0.92, 0.92), @(0.92, 0.92))) {
        Add-Box $pad @($tower[0], 0.27, $tower[1]) @(0.08, 0.32, 0.08)
    }
    $rocketScale = 1.80
    foreach ($part in @($paint, $metal, $heat, $stripe, $avionics, $pad, $fastener, $thermal, $engineBell, $panelDetail, $coreInsulation, $booster, $serviceStructure, $hardware, $interstageInsulation)) {
        Scale-Part-Uniform $part $rocketScale
    }
    return [pscustomobject]@{ Parts = @($paint, $metal, $heat, $stripe, $avionics, $pad, $fastener, $thermal, $engineBell, $panelDetail, $coreInsulation, $booster, $serviceStructure, $hardware, $interstageInsulation); Materials = $materials }
}

if (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
    throw "OutputDirectory must be absolute."
}
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$giraffe = New-Giraffe
$giraffeTextures = New-ShowcaseTextureDefinitions -OutputDirectory $OutputDirectory -Subject "giraffe" -IncludeBaseColor
Write-Gltf (Join-Path $OutputDirectory "cheeky_giraffe.gltf") "Anatomical Giraffe Study" $giraffe.Parts $giraffe.Materials @(-2.15, 0.0, -1.7) $giraffeTextures
$rocket = New-Rocket
$rocketTextures = New-ShowcaseTextureDefinitions $OutputDirectory "rocket" -IncludeBaseColor
Write-Gltf (Join-Path $OutputDirectory "original_realistic_rocket.gltf") "Original Realistic Rocket" $rocket.Parts $rocket.Materials @(2.15, 0.0, -1.7) $rocketTextures
