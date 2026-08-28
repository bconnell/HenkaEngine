# Rendering Realism

Henka's realism work is a layered rasterized OpenGL rendering stack with physically based material inputs, HDR presentation, image-based lighting, local reflection probes, shadow maps, screen-space effects, and bounded temporal reconstruction.

> **Status:** Active renderer-realism foundation. Each effect keeps an explicit scope and validation boundary. Current evidence covers the supported OpenGL path and its deterministic reference fixtures.

## Contents

- [Current realism stack](#current-realism-stack)
- [Reference-scene overview](#reference-scene-overview)
- [PBR material reference](#pbr-material-reference)
- [Normal-map reference](#normal-map-reference)
- [Color-space reference](#color-space-reference)
- [Energy-response reference](#energy-response-reference)
- [IBL reference](#ibl-reference)
- [Scene-probe reference](#scene-probe-reference)
- [Lighting and shadow reference](#lighting-and-shadow-reference)
- [Subsurface reference](#subsurface-reference)
- [SSGI reference and motion evidence](#ssgi-reference-and-motion-evidence)
- [SSGI performance evidence](#ssgi-performance-evidence)
- [Screen-space indirect diffuse lighting](#screen-space-indirect-diffuse-lighting)
- [Scene-probe diffuse transfer](#scene-probe-diffuse-transfer)
- [Exposure and HDR-range reference](#exposure-and-hdr-range-reference)
- [Direction](#direction)

## Current realism stack

### PBR material inputs

The Rendered path currently supports glTF-oriented material inputs for:

- base color;
- metallic/roughness;
- normal mapping;
- occlusion;
- emissive response;
- specular controls;
- IOR;
- transmission;
- volume attenuation;
- clearcoat;
- sheen;
- alpha modes;
- double-sided rendering;
- runtime-authored subsurface amount and tint.

### Transmission and volume

Transmission uses the authored IOR for a bounded environment-refraction direction and authored volume attenuation.

Manager-owned linear texture data supports:

- `KHR_materials_transmission` scalar textures;
- `KHR_materials_volume` thickness textures.

These textures modulate the corresponding renderer response. Screen-space refraction, layered volumes, and production glass remain unfinished.

### Subsurface response

Subsurface-tinted materials use a bounded view-aware three-lobe direct-light diffusion-profile approximation.

The current response includes:

- authored subsurface amount and tint;
- authored thickness;
- imported linear thickness texture support;
- profile widening from thickness data;
- a bounded back-facing environment contribution;
- reserved diffuse energy so the subsurface term does not stack on a full diffuse response.

The shared material-instance/editor path can assign, clear, restore, inspect, and transactionally refresh the imported thickness texture while preserving the existing material authority.

Current SSS does not provide true multi-scatter diffusion, skin/wax profiles, screen-space SSS, or ray-traced SSS.

### Image-based lighting

HDR environment lighting derives the following resources transactionally:

- 32-sample cosine-weighted irradiance;
- 128-sample GGX-prefiltered specular environment data across bounded mip levels;
- a 128-sample split-sum BRDF lookup texture.

IBL roughness lookup is capped at the supported 32x32 prefilter level. High-roughness subjects therefore avoid the under-resolved 4x4 and 2x2 prefilter levels.

This is the current rasterized environment-lighting foundation. Path tracing and full-scene global illumination are not implemented.

### Local reflection probes

Local reflection probes use a bounded seven-level cubemap chain. Captured mip 0 is filtered through the existing bounded GGX prefilter program.

The chain contains:

- 64x64;
- 32x32;
- 16x16;
- 8x8;
- 4x4;
- 2x2;
- 1x1 faces.

Local-probe roughness lookup is capped at the supported 16x16 prefilter level. The 8x8, 4x4, 2x2, and 1x1 levels are not used for high-roughness lookup.

The current probe system is a bounded local approximation. Production probe grids remain future work.

### Shadows

Current shadow foundations include:

- directional shadows;
- cascaded directional shadows;
- spot shadows;
- point shadows.

### Ambient occlusion

The current renderer provides depth-derived ambient occlusion.

### Screen-space reflections

The bounded SSR path uses the material pass's per-pixel roughness output and provides:

- signed depth-crossing hit refinement;
- surface reconstruction from valid central depth neighbors;
- back-facing-hit rejection;
- edge-invalid-hit rejection;
- quadratic roughness attenuation;
- edge-distance weighting;
- bounded Schlick Fresnel weighting;
- smooth-material screen-space hits;
- filtered environment/probe fallback for rougher materials;
- a bounded roughness-aware resolve;
- environment/probe fallback when required HDR resources are unavailable.

Current SSR remains limited to visible screen-space information. Planar, hierarchical, and off-screen reflection coverage remain future work.

### HDR presentation and temporal reconstruction

The current presentation path provides:

- bloom;
- finite exposure control;
- ACES-style tone mapping;
- a restrained rendered grade;
- reconstruction sharpening;
- bounded temporal history;
- motion data;
- previous-depth validation;
- disocclusion safeguards;
- reactive-mask safeguards;
- history clamping.

Screen-space indirect diffuse, ambient occlusion, and SSR remain in the linear HDR target until the final exposure, tone-map, and presentation transform.

Reflection falls back to environment, probe, or analytical lighting when a valid roughness attachment is unavailable.

### Fullscreen presentation geometry

The shared fullscreen-triangle path maps its oversized clip-space triangle to the complete normalized texture domain.

Material Preview and Rendered preserve Scene View composition while applying their respective shading and post-processing paths. The Windows regression checks normalized edge/corner coverage and the fullscreen-triangle contract. Application captures remain the visual authority for subject placement and quality.

## Reference-scene overview

The Sandbox3D package contains deterministic Henka-owned realism fixtures. These fixtures use the same scene, material, texture, lighting, environment, and post-processing paths exercised by the editor showcase.

### Available reference profiles

```text
--capture-realism-reference wide|close solid|material_preview|rendered
--capture-realism-reference normal_map wide|close solid|material_preview|rendered
--capture-realism-reference color_space wide|close solid|material_preview|rendered
--capture-realism-reference energy wide|close solid|material_preview|rendered
--capture-realism-reference ibl wide|close rendered
--capture-realism-reference scene_probe wide|close rendered
--capture-realism-reference lighting wide|close solid|material_preview|rendered
--capture-realism-reference sss close opaque|thin|thick rendered
--capture-realism-reference ssgi wide|close rendered
--capture-realism-reference ssgi_motion wide|close rendered output_directory
--capture-realism-reference ssgi_performance close rendered
--capture-realism-reference hdr close -2|0|2 rendered
```

Ordinary reference captures emit `CAPTURE_READY_REFERENCE`. Specialized profiles emit their own readiness metadata described below.

The reference system is calibration and regression infrastructure. Human visual review remains authoritative for appearance, composition, anatomy, material quality, and other subjective visual properties.

## PBR material reference

The main PBR reference scene contains nine bounded UV-sphere subjects:

1. rough metal;
2. polished metal;
3. painted clearcoat;
4. plastic;
5. stone;
6. fabric sheen;
7. dry wood;
8. wet/dry stone;
9. subsurface wax approximation.

### Layouts

**Wide** places all nine subjects in one deterministic row for overview comparison.

**Close** uses a non-overlapping 3x3 grid so every material receives useful inspection area.

### Fixture lighting

The PBR, normal-map, color-space, energy, IBL, and scene-probe profiles include a restrained neutral, shadowless fixture fill. The fill keeps subjects readable while preserving directional-key response, shadows, falloff, and probe contrast.

The lighting and subsurface profiles keep their dedicated spatial-light arrangements.

### Deterministic detail maps

Reference detail maps are generated at 64x64:

| Map | Color/data semantics |
| --- | --- |
| Normal | Linear normal data |
| Macro | Color texture |
| Wood | Color texture |
| Wet/dry | Linear metallic/roughness data |

The generated maps use tileable multi-scale value noise with bounded mid-scale octaves. Capture readiness reads runtime texture dimensions and fails below the required minimum.

### Rough/polished metal guard

The close PBR board gives rough-metal and polished-metal subjects the same neutral base color. The checker samples matched regions and requires distinguishable rendered luminance responses under deterministic lighting.

This requirement is fixture-specific.

### Contact-shadow guard

The close capture compares normalized ground beneath the subjects with an unobstructed ground control. The checker requires measurable contact-shadow contrast.

## Normal-map reference

The normal-map fixture uses the same neutral material and camera for:

- four flat controls;
- five subjects using the generated linear normal map.

The map is bound through the ordinary OpenGL material path.

```text
--capture-realism-reference normal_map wide|close solid|material_preview|rendered
```

The dedicated checker requires the five mapped regions to show measurable local rendered response above the four flat controls.

This validates active normal-map binding and tangent-space response. Production surface-authoring quality remains outside this fixture's scope.

## Color-space reference

The color-space fixture uses identical RGBA8 source bytes for:

- four sRGB-tagged subjects;
- five linear-tagged subjects.

The color-space descriptor travels through the ordinary material and OpenGL texture paths.

Base-color textures accept explicit sRGB or linear semantics. Normal, metallic/roughness, occlusion, transmission, and thickness data retain linear semantics.

```text
--capture-realism-reference color_space wide|close solid|material_preview|rendered
```

The checker requires sRGB and linear metadata and measures their paired rendered response.

Universal color management, display mastering, and scene-wide energy calibration remain outside this fixture's scope.

## Energy-response reference

The energy fixture keeps geometry, neutral base color, lighting, environment, and camera fixed.

It contains:

- three dielectric subjects;
- three metallic subjects;
- three secondary-response subjects covering transmission, clearcoat, and sheen.

```text
--capture-realism-reference energy wide|close solid|material_preview|rendered
```

The readiness record identifies the response groups and clipped-channel limit.

The checker requires:

- matching composition across Solid, Material Preview, and Rendered;
- visible group separation;
- bounded Rendered luminance;
- bounded clipping;
- a non-flat ordinary OpenGL PBR response.

This is a gross-energy regression guard for the supported fixture. It does not establish universal energy conservation across arbitrary materials, lights, exposures, or displays.

## IBL reference

The rendered-only IBL fixture uses a dedicated nine-subject metallic roughness ladder from `0.05` through `0.95`.

```text
--capture-realism-reference ibl wide|close rendered
```

`CAPTURE_READY_IBL_REFERENCE` is emitted only after the renderer reports:

- generated irradiance cube ready at resolution 32;
- seven-level prefilter chain ready;
- split-sum BRDF LUT ready at resolution 128.

The prefilter and split-sum integration use bounded 128-sample Hammersley sequences.

The checker verifies:

- visible roughness resolution across the prefiltered environment response;
- bounded image output;
- all nine close-view subjects remain readable.

The shared UV-sphere fixture keeps triangle winding aligned with authored outward normals.

Production HDRI authoring, probe-grid blending, and universal IBL accuracy remain future work.

## Scene-probe reference

`SCENE_PROBE_REFERENCE` is rendered-only and uses the same nine-subject calibration fixture.

```text
--capture-realism-reference scene_probe wide|close rendered
```

Readiness requires two enabled local probes with the current scene content revision. The renderer must report:

- bounded probe diffuse transfer;
- probe prefiltering;
- overlap blending.

A restrained shadowless fill keeps all nine subjects readable while directional lighting, falloff, and probe contrast remain visible.

The image guard requires populated, bounded output and nine legible subjects.

The current two-probe OpenGL foundation does not provide a production irradiance volume, production probe grid, or guaranteed hidden/off-screen global illumination.

## Lighting and shadow reference

The lighting fixture isolates spatial light response using:

- nine same-material UV-sphere subjects;
- scene-owned directional/key/fill/rim light sources;
- the shared ground receiver;
- real shadow maps;
- the ordinary OpenGL Rendered path.

```text
--capture-realism-reference lighting wide|close solid|material_preview|rendered
```

Each lighting capture emits `CAPTURE_READY_LIGHTING_REFERENCE` metadata.

The checker requires:

- nine settled centered subjects;
- stable composition across Solid, Material Preview, and Rendered;
- a meaningful Rendered/Material Preview difference;
- measurable luminance differences between deterministic subject regions.

Rendered readiness also requires complete directional, cascade, and point shadow targets and records that state in capture metadata.

The PBR close checker independently requires contact-shadow contrast.

The lighting profile captures Rendered twice and bounds repeat-to-repeat image difference. This guards deterministic shadow/light output across fresh runs. Temporal anti-aliasing and hardware-independent pixel identity are not claimed by this test.

## Subsurface reference

The subsurface fixture uses nine subjects with the same camera, geometry, and lighting and captures three authored variants:

```text
--capture-realism-reference sss close opaque rendered
--capture-realism-reference sss close thin rendered
--capture-realism-reference sss close thick rendered
```

The checker requires:

- matching composition metadata;
- nine settled subjects;
- measurable image response between variants;
- measurable center-subject color response between variants.

This validates that authored subsurface amount and thickness reach the OpenGL material path with stable pixel response.

Production multi-scatter, screen-space, and ray-traced subsurface transport remain future work.

## SSGI reference and motion evidence

### Static SSGI reference

The SSGI profile is Rendered-only because the current indirect-diffuse term runs as a fullscreen post-process over HDR color and depth.

```text
--capture-realism-reference ssgi wide|close rendered
```

`CAPTURE_READY_SSGI_REFERENCE` is emitted only after the renderer reports an active screen-space indirect path for a settled frame.

The checker requires:

- deterministic nine-subject composition;
- non-flat image content;
- all evaluated subject patches remain legible in close capture;
- bounded HDR clipping;
- bounded over-bright pixels;
- bounded bright subject-edge halos;
- at least two enabled local probes;
- nonzero current probe generation;
- zero probe-capture failures.

The motion and performance profiles carry the same probe-health fields.

These checks cover activation, presentation stability, and gross artifacts in the supported OpenGL reference.

### Motion-stability reference

```text
--capture-realism-reference ssgi_motion close rendered output_directory
```

The motion profile emits `CAPTURE_READY_SSGI_MOTION_REFERENCE` records for settled `before` and `after` phases from one process.

The capture flow:

1. settles the initial frame;
2. performs one bounded in-process camera translation;
3. requests completed frames through application-owned OpenGL framebuffer readback;
4. converts the bounded readbacks into inspection images.

The Windows harness verifies:

- deterministic camera delta;
- matching dimensions;
- non-flat content;
- gross brightness bounds;
- bounded luminance-distribution change;
- a real sampled pixel difference.

Each phase also reports:

- temporal history readiness;
- allocation failures;
- motion-vector readiness.

The static baseline can begin with the safe fallback before its first history copy. Once history is valid, settled and moved perspective frames keep the bounded jitter/resolve path active. The moved phase must report valid history, enabled jitter, and a nonzero resolve count.

This evidence covers current-frame continuity for the supported OpenGL fixture. Artifact-free motion at every speed, complete temporal reconstruction quality, and production global illumination remain future work.

## SSGI performance evidence

```text
--capture-realism-reference ssgi_performance close rendered
```

The performance reference uses the same nine-subject scene at a fixed `1280x720` viewport.

The measurement flow:

1. wait for three settled frames;
2. record 32 bounded frame-time samples;
3. record 32 bounded scene-CPU timing samples;
4. record available OpenGL scene-GPU timing samples while SSGI is active.

The checker requires timer-query availability and rejects gross GPU runaway above the 100 ms reference budget.

The 100 ms threshold is a fixed-reference regression budget. It is not a universal frame-rate target or an isolated SSGI shader-cost measurement.

## Screen-space indirect diffuse lighting

Rendered presentation contains a bounded screen-space indirect-diffuse approximation.

For each receiver, the path:

- reconstructs current position and normal from depth;
- samples nearby visible HDR surfaces in eight directions;
- rejects samples outside bounded distance and thickness limits;
- caps source radiance;
- adds gathered indirect contribution in HDR before bloom and tone mapping.

The gather uses:

- symmetric receiver-depth reconstruction;
- depth-edge confidence;
- a small cross-filter on source radiance;
- bounded reconstructed source-facing response.

Back-facing depth samples do not contribute as valid emitters. Source-neighbor depth failure keeps the approximation conservative.

The close-reference image guard compares subject-edge brightness with a paired outward background sample. A localized bright annulus fails the guard. Normal scene illumination and background gradients remain valid.

Current screen-space indirect diffuse cannot see geometry that is outside the current view, hidden behind another surface, or absent from the depth/color buffers. Multi-bounce transport, probe-volume GI, hardware ray tracing, and path tracing are not implemented by this path.

## Scene-probe diffuse transfer

Captured scene-reflection probes can provide opaque materials with a bounded local diffuse color-transfer approximation.

The path uses five clamped cubemap samples around the receiver normal and reports activation through `rendered_reflection_probe_diffuse_active`.

SSGI reference readiness requires:

- screen-space indirect diffuse active;
- probe diffuse active;
- seven-level probe prefilter active;
- bounded two-probe overlap active.

Missing shader support or unavailable probe data falls back to the existing environment-lighting path.

### Probe overlap behavior

When two captured probe volumes contain a receiver, the renderer:

1. deterministically ranks a primary and secondary probe;
2. computes bounded inverse-score weights;
3. blends specular contributions;
4. blends diffuse contributions.

When one capture is valid, that probe is used. The shared environment path remains the fallback when no local capture is valid.

This is a bounded overlap implementation. Production probe-grid filtering, irradiance volumes, guaranteed hidden/off-screen contribution, and multi-bounce transport remain future work.

## Exposure and HDR-range reference

The renderer applies viewport exposure in linear HDR before the final ACES-style tone map.

The public engine boundary accepts finite exposure values from `-16` to `+16` stops and rejects invalid values.

The deterministic HDR-range reference captures the same nine-subject fixture, camera, layout, environment, and Rendered path at:

```text
--capture-realism-reference hdr close -2 rendered
--capture-realism-reference hdr close 0 rendered
--capture-realism-reference hdr close 2 rendered
```

`HDR_RANGE_REFERENCE` readiness records the requested exposure.

`scripts/check_hdr_range_visual_evidence_windows.ps1` verifies:

- identical composition;
- monotonic luminance response;
- spatially non-flat output;
- bounded clipping at `+2` stops.

This validates the supported OpenGL exposure/HDR response. Automatic exposure, HDR display calibration, and full HDR mastering remain future work.

Spectral rendering is not part of the current implementation. It remains research work until RGB PBR, indirect lighting, material import, color management, and reference-scene validation are mature.

## Direction

Future realism work should continue to use deterministic reference scenes and concrete visual defects as its evidence base.

Current reference coverage includes:

- PBR material response;
- lighting and shadows;
- normal mapping;
- texture color space;
- bounded energy response;
- IBL activation and roughness response;
- local scene probes;
- exposure/HDR range;
- bounded subsurface response;
- SSGI activation;
- SSGI motion continuity;
- bounded fixed-reference SSGI performance.

### Follow-up tracks

1. Extend PBR, lighting, and HDR references for roughness/metallic response, deeper IBL calibration, and light/shadow stability.
2. Replace the bounded subsurface approximation when production SSS profile, thickness, ownership, and performance contracts are defined.
3. Extend screen-space indirect-diffuse validation for leaks, halos, over-brightening, and deeper hardware-specific performance characterization.
4. Extend scene-probe diffuse transfer into a bounded irradiance-probe or probe-grid system with explicit capture, filtering, blending, and performance contracts.
5. Extend local reflection-probe placement, capture policy, and indirect-diffuse integration. Bounded two-probe overlap and roughness-aware SSR remain current foundations. Planar, hierarchical, off-screen, and production probe-grid reflection remain future work.
6. Retain rasterization as the broad hardware baseline while future renderer backends prepare for optional hardware ray tracing.
7. Evaluate a path-traced reference renderer later as a visual ground-truth tool for renderer development.
