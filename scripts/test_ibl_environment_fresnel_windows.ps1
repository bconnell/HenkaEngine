param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$shaderPath = Join-Path $RepositoryRoot 'assets/shaders/basic_lit.frag'
$shader = Get-Content -LiteralPath $shaderPath -Raw
$missing = @()

# Direct-light Fresnel is evaluated from the directional-light half vector.
# Environment reflection must use its own view-dependent factor so the IBL
# term cannot inherit a localized light-direction artifact.
$environmentBlockMatch = [regex]::Match(
    $shader,
    '(?s)if \(useEnvironment\)\s*\{.*?\n\s*\}\s*\n\s*for \(int lightIndex')
if (-not $environmentBlockMatch.Success) {
    throw 'IBL environment block could not be located for the Fresnel contract.'
}
$environmentBlock = $environmentBlockMatch.Value

if ($environmentBlock -notmatch 'vec3 environmentFresnel\s*=\s*(?:fresnelSchlick\(nDotV,\s*f0\)|viewFresnel)') {
    $missing += 'view-dependent environment Fresnel factor'
}
if ($environmentBlock -notmatch 'environmentSpecular\s*\*\s*\(environmentFresnel\s*\*\s*brdf\.x\s*\+\s*brdf\.y\)') {
    $missing += 'environment Fresnel used by IBL specular response'
}
if ($environmentBlock -notmatch 'sampleEnvironment\(transmissionDirection\).*?\(1\.0\s*-\s*environmentFresnel\)') {
    $missing += 'environment Fresnel used by IBL transmission response'
}
if ($environmentBlock -match 'environmentSpecular\s*\*\s*\((?<![A-Za-z])fresnel\s*\*\s*brdf\.x') {
    $missing += 'direct-light Fresnel reused by IBL specular response'
}

if ($shader -notmatch 'vec3 viewFresnel\s*=\s*fresnelSchlick\(nDotV,\s*f0\)') {
    $missing += 'view-dependent fallback Fresnel factor'
}
if ($shader -notmatch 'safeAmbient\s*\*\s*\(\(1\.0\s*-\s*surfaceTransmission\).*?viewFresnel\s*\*\s*0\.5') {
    $missing += 'view-dependent Fresnel used by ambient fallback'
}
if ($shader -match 'safeAmbient\s*\*\s*\(\(1\.0\s*-\s*surfaceTransmission\).*?(?<![A-Za-z])fresnel\s*\*\s*0\.5') {
    $missing += 'direct-light Fresnel reused by ambient fallback'
}

if ($missing.Count -gt 0) {
    throw "IBL environment Fresnel contract failed: $($missing -join ', ')"
}

Write-Output 'IBL environment Fresnel contract passed.'
