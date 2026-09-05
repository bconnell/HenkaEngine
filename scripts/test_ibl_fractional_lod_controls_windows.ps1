param(
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

$sandbox = Get-Content (Join-Path $RepositoryRoot 'examples/sandbox3d/main.c') -Raw
$shader = Get-Content (Join-Path $RepositoryRoot 'assets/shaders/basic_lit.frag') -Raw
$capture = Get-Content (Join-Path $RepositoryRoot 'scripts/capture_visual_evidence_windows.ps1') -Raw
$renderer = Get-Content (Join-Path $RepositoryRoot 'engine/src/renderer/renderer_opengl.c') -Raw
$engineHeader = Get-Content (Join-Path $RepositoryRoot 'engine/include/henka/engine.h') -Raw
$missing = @()

if ($sandbox -notmatch 'SANDBOX3D_REALISM_REFERENCE_KIND_IBL_ORDINARY_MIP') {
    $missing += 'ordinary-path fractional-LOD reference kind'
}
if ($sandbox -notmatch 'strcmp\(value, "ibl_ordinary_mip"\)') {
    $missing += 'ordinary-path fractional-LOD parser'
}
if ($sandbox -notmatch 'ibl_ordinary_mip') {
    $missing += 'ordinary-path fractional-LOD command wiring'
}
if ($sandbox -notmatch 'ordinary_prefilter_lod') {
    $missing += 'ordinary-path fractional-LOD metadata'
}
foreach ($field in @(
    'ibl_ordinary_resource=prefilter_cube',
    'ibl_ordinary_sampler=GL_LINEAR_MIPMAP_LINEAR',
    'ibl_ordinary_lod_formula=surface_roughness\*ibl_prefilter_max_lod',
    'ibl_ordinary_calculated_lod=%\.2f',
    'ibl_ordinary_neighbor_mips=%d,%d',
    'ibl_ordinary_fraction=%\.2f',
    'ibl_ordinary_texture_lod=textureLod',
    'ibl_ordinary_brdf=brdf_lut',
    'ibl_ordinary_probes=0')) {
    if ($sandbox -notmatch $field) {
        $missing += "ordinary-path state metadata: $field"
    }
}
if ($sandbox -notmatch 'ordinary_lower_mip = \(int\)floorf\(ordinary_lod\)' -or
    $sandbox -notmatch 'ordinary_fraction = ordinary_lod - \(float\)ordinary_lower_mip') {
    $missing += 'ordinary-path LOD metadata derived from the requested LOD'
}
if ($shader -notmatch '(?s)float environmentPrefilterLod =.*?if \(iblDiagnosticPrefilterLod >= 0\.0\).*?environmentPrefilterLod = clamp\(\s*iblDiagnosticPrefilterLod') {
    $missing += 'ordinary production path exact fractional-LOD override'
}
if ($sandbox -notmatch '(?s)IBL_ORDINARY_MIP.*?henka_engine_set_ibl_diagnostic_prefilter_lod') {
    $missing += 'ordinary-path override installation'
}
if ($capture -notmatch 'PBR_IBL_FRACTIONAL') {
    $missing += 'fractional-LOD evidence profile'
}
if ($capture -notmatch 'ibl_ordinary_mip') {
    $missing += 'fractional-LOD evidence commands'
}
if ($renderer -match 'ibl_prefilter_max_lod = renderer->ibl_diagnostic_prefilter_lod') {
    $missing += 'diagnostic override mutating resource-derived maximum LOD'
}
if ($engineHeader -notmatch 'resource-derived maximum remains authoritative') {
    $missing += 'ordinary-path LOD authority contract documentation'
}

if ($missing.Count -gt 0) {
    throw "IBL fractional LOD control contract failed: $($missing -join ', ')"
}

Write-Output 'IBL fractional LOD control contract passed.'
