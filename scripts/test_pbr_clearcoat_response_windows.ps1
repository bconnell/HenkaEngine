$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$shader = Get-Content -Raw (Join-Path $repoRoot 'assets/shaders/basic_lit.frag')

function Assert-Contract([bool]$condition, [string]$message) {
    if (-not $condition) {
        throw "PBR clearcoat response contract failed: $message"
    }
}

Assert-Contract ($shader -match '(?s)float microfacetResponse\s*=\s*min\(\s*distribution\s*\*\s*visibility\s*,\s*8\.0\s*\).*?vec3 specular\s*=\s*microfacetResponse\s*\*\s*fresnel') `
    'the direct GGX response must have a finite per-pixel bound.'
Assert-Contract ($shader -match '(?s)float clearcoatMicrofacet\s*=\s*min\(\s*clearcoatDistribution\s*\*\s*clearcoatVisibility\s*,\s*32\.0\s*\)') `
    'the direct clearcoat microfacet response must have a finite per-pixel bound.'
Assert-Contract ($shader -match '(?s)clearcoatMicrofacet.*?surfaceClearcoat\s*\*\s*0\.25') `
    'the bounded clearcoat response must retain a conservative coat-energy factor.'
Assert-Contract ($shader -notmatch 'color \+= clearcoatFresnel \* clearcoatDistribution \* clearcoatVisibility') `
    'the direct clearcoat path must not reintroduce an unbounded distribution-times-visibility sum.'

Write-Output 'PBR clearcoat response source contract test passed.'
