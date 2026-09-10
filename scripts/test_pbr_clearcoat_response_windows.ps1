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
Assert-Contract ($shader -match '(?s)float localMicrofacetResponse\s*=\s*min\(\s*localDistribution\s*\*\s*localVisibility\s*,\s*8\.0\s*\).*?vec3 localSpecular\s*=\s*localMicrofacetResponse\s*\*\s*localFresnel') `
    'the local-light GGX response must have the same finite per-pixel bound as the main direct-light path.'
Assert-Contract ($shader -match '(?s)float moonMicrofacetResponse\s*=\s*min\(\s*moonDistribution\s*\*\s*moonVisibility\s*,\s*8\.0\s*\).*?vec3 moonSpecular\s*=\s*moonMicrofacetResponse\s*\*\s*moonFresnel') `
    'the secondary directional-light GGX response must have the same finite per-pixel bound as the main direct-light path.'
Assert-Contract ($shader -match '(?s)float sheenMicrofacetResponse\s*=\s*min\(\s*sheenDistribution\s*\*\s*sheenVisibility\s*,\s*32\.0\s*\).*?color \+= sheenFresnel \* sheenMicrofacetResponse') `
    'the direct sheen GGX response must have a finite per-pixel bound.'
Assert-Contract ($shader -match '(?s)float clearcoatMicrofacet\s*=\s*min\(\s*clearcoatDistribution\s*\*\s*clearcoatVisibility\s*,\s*32\.0\s*\)') `
    'the direct clearcoat microfacet response must have a finite per-pixel bound.'
Assert-Contract ($shader -match '(?s)clearcoatMicrofacet.*?surfaceClearcoat\s*\*\s*0\.25') `
    'the bounded clearcoat response must retain a conservative coat-energy factor.'
Assert-Contract ($shader -notmatch 'color \+= clearcoatFresnel \* clearcoatDistribution \* clearcoatVisibility') `
    'the direct clearcoat path must not reintroduce an unbounded distribution-times-visibility sum.'

Write-Output 'PBR clearcoat response source contract test passed.'
