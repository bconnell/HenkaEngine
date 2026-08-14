param(
    [Parameter(Mandatory = $true)]
    [string]$RepositoryRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$sourcePath = Join-Path ([System.IO.Path]::GetFullPath($RepositoryRoot)) "engine\src\renderer\renderer_opengl.c"
if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
    throw "Renderer source was not found: $sourcePath"
}

$source = Get-Content -LiteralPath $sourcePath -Raw
if ($source -notmatch 'uv\s*=\s*p\s*;') {
    throw "Fullscreen presentation vertex source does not use the full p-domain mapping."
}
if ($source -match 'uv\s*=\s*p\s*\*\s*0\.5\s*;') {
    throw "Fullscreen presentation regressed to the half-domain UV mapping."
}

$pValues = @(
    [pscustomobject]@{ Name = "lower-left";  X = 0.0; Y = 0.0 },
    [pscustomobject]@{ Name = "lower-right"; X = 2.0; Y = 0.0 },
    [pscustomobject]@{ Name = "upper-left";  X = 0.0; Y = 2.0 }
)
$clipValues = foreach ($value in $pValues) {
    [pscustomobject]@{
        Name = $value.Name
        X = $value.X * 2.0 - 1.0
        Y = $value.Y * 2.0 - 1.0
    }
}
if ($clipValues[0].X -ne -1.0 -or $clipValues[0].Y -ne -1.0 -or
    $clipValues[1].X -ne 3.0 -or $clipValues[1].Y -ne -1.0 -or
    $clipValues[2].X -ne -1.0 -or $clipValues[2].Y -ne 3.0) {
    throw "Fullscreen triangle clip-space contract changed unexpectedly."
}

# The visible square corners map to p=(0,0), (1,0), (0,1), and (1,1).
# With uv=p those values remain the complete texture domain. The old
# half-domain mapping exposed only 0..0.5 and failed the same edge/corner contract.
$visibleUv = @(
    [pscustomobject]@{ Name = "bottom-left";  U = 0.0; V = 0.0 },
    [pscustomobject]@{ Name = "bottom-right"; U = 1.0; V = 0.0 },
    [pscustomobject]@{ Name = "top-left";     U = 0.0; V = 1.0 },
    [pscustomobject]@{ Name = "top-right";    U = 1.0; V = 1.0 }
)
foreach ($corner in $visibleUv) {
    if ($corner.U -lt 0.0 -or $corner.U -gt 1.0 -or
        $corner.V -lt 0.0 -or $corner.V -gt 1.0) {
        throw "Visible fullscreen corner fell outside the normalized texture domain: $($corner.Name)"
    }
}

Write-Output "Fullscreen presentation mapping regression passed."
Write-Output "Clip triangle: p=(0,0),(2,0),(0,2) -> clip=(-1,-1),(3,-1),(-1,3)."
Write-Output "Visible texture corners retain the complete 0..1 by 0..1 domain."
