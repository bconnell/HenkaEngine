# Rendering Realism

Henka's realism work is built as a layered, truthful rendering stack. The current renderer is a rasterized OpenGL path with physically based material inputs, HDR presentation, image-based lighting, local reflection probes, shadow maps, screen-space effects, and bounded temporal reconstruction. These systems improve realism without claiming capabilities that are not present.

## Current realism stack

The current Rendered path includes:

- glTF-oriented PBR material inputs for base color, metallic/roughness, normals, occlusion, emissive response, specular controls, IOR, transmission, volume attenuation, clearcoat, sheen, alpha modes, and double-sided rendering, plus runtime-authored subsurface amount/tint controls;
- transmission uses the authored IOR for a bounded environment-refraction direction and authored volume attenuation; KHR_materials_transmission scalar textures and KHR_materials_volume thickness textures are manager-owned linear data that modulate the corresponding response. Screen-space refraction, layered volumes, and production glass remain unfinished;
- a bounded view-aware three-lobe direct-light diffusion-profile approximation for subsurface-tinted materials, with authored thickness and the shared linear thickness texture widening the profile and a bounded back-facing environment contribution; diffuse energy is reserved so the response is not simply added on top of full diffuse. The shared material-instance/editor path can assign, clear, restore, inspect, and transactionally refresh the imported thickness texture without creating a second material authority. This is still not true multi-scatter diffusion, a skin/wax profile, or screen-space/ray-traced SSS;
- HDR environment lighting with transactionally derived 32-sample cosine-weighted irradiance, 32-sample GGX-prefiltered specular environment data across bounded mips, and a 32-sample split-sum BRDF lookup texture; this improves the rasterized environment response without claiming path tracing or full-scene global illumination;
- local reflection probes with a bounded five-level generated cubemap mip chain
  for roughness-dependent filtering; this is a box-filtered local probe
  approximation, not the GGX-prefiltered global IBL path;
- directional, cascade, spot, and point shadow-map foundations;
- depth-derived ambient occlusion;
- a validated environment/probe reflection fallback; the retained depth-derived
  screen-space reflection implementation is fail-closed until the post-process
  has per-pixel material roughness data;
- bloom, exposure, ACES-style tone mapping, a restrained rendered grade, and reconstruction sharpening;
- bounded temporal history with motion, previous-depth, disocclusion, reactive-mask, and history-clamping safeguards.

Rendered post-processing keeps the calibrated screen-space indirect-diffuse and
ambient-occlusion contributions in the linear HDR target until the single final
exposure/tone-map/presentation transform. Reflection uses the environment,
probe, or analytical fallback while the retained screen-space reflection
implementation is disabled without a material-aware roughness buffer. This
improves color-space correctness without emitting false self-reflections, but
does not turn the screen-space paths into full-scene reflections or production
GTAO.

The shared fullscreen-triangle presentation path maps its oversized clip-space
triangle to the complete normalized texture domain. Material Preview and
Rendered therefore preserve the same scene composition as Solid while applying
their intended shading and post-processing differences. The Windows regression
covers the triangle contract and normalized edge/corner coverage; application
captures remain the visual authority for subject placement and quality.

## Deterministic reference scene

The Sandbox3D package contains a Henka-owned PBR reference scene made from nine
bounded UV-sphere subjects: rough metal, polished metal, painted clearcoat,
plastic, stone, fabric sheen, dry wood, wet/dry stone, and a subsurface wax
approximation. The fixture uses the same scene, material, texture, lighting,
environment, and post-processing paths as the editor showcase; it is not a
showcase-only shader path.

The wide reference keeps the nine subjects in a single deterministic row for
overview comparison. The close reference uses a non-overlapping 3x3 grid so
each material receives useful screen area for visual inspection without
changing the authored fixture or its material assignments.

The reference fixture's deterministic detail maps are generated at 32x32: the
normal map is linear normal data, the macro and wood maps are color textures,
and the wet/dry map is linear metallic/roughness data. Capture readiness reads
the runtime texture dimensions and fails closed below that minimum.

The close PBR board uses the same neutral base color for its rough-metal and
polished-metal subjects. The visual checker samples those matched regions and
requires their rendered luminance responses to remain distinguishable under the
deterministic reference lighting. This is a fixture-specific calibration guard,
not a claim that one universal luminance ordering applies to every scene.

The close capture also compares a normalized ground region beneath the subjects
with an unobstructed ground control. The checker requires measurable contact
shadow contrast, providing an initial lighting/shadow regression without
confusing ambient darkening with a claimed full lighting benchmark.

The package also exposes a matched normal-map reference fixture. It uses the
same neutral material and camera for four flat controls and five subjects with
the generated linear normal map bound through the ordinary OpenGL material
path:

```text
--capture-realism-reference normal_map wide|close solid|material_preview|rendered
```

The dedicated checker requires the five mapped regions to produce a measurable
local rendered response above the four flat controls. This proves that the
normal-map binding and tangent-space response are active; it is a bounded
calibration fixture, not a claim that the generated detail texture represents
production surface authoring quality.

The package also exposes a dedicated color-space reference fixture. It uses the
same RGBA8 source bytes for four sRGB-tagged subjects and five linear-tagged
subjects, with the color-space descriptor carried through the ordinary material
and OpenGL texture paths. Base-color textures accept either explicit sRGB or
linear semantics; normal, metallic/roughness, occlusion, transmission, and
thickness data retain their required linear contract.

```text
--capture-realism-reference color_space wide|close solid|material_preview|rendered
```

The checker requires the sRGB and linear texture metadata to be present and
measures the paired rendered response. This proves that the supported texture
color-space distinction reaches the renderer; it is a bounded response fixture,
not a claim of universal color-management, display-mastering, or scene-wide
energy calibration.

The package also exposes a separate lighting reference fixture. It uses nine
same-material UV-sphere subjects, scene-owned directional/key/fill/rim light
sources, the shared ground receiver, real shadow maps, and the same OpenGL
Rendered path. Its purpose is to isolate spatial light response from the PBR
material board; it does not replace the PBR fixture or claim production
lighting, global illumination, or cinematic light-authoring coverage.

The lighting fixture is selected explicitly and remains deterministic:

```text
--capture-realism-reference lighting wide|close solid|material_preview|rendered
```

Each lighting capture emits `CAPTURE_READY_LIGHTING_REFERENCE` metadata. The
lighting checker requires nine settled, centered subjects, stable composition
across Solid, Material Preview, and Rendered, a meaningful Rendered-versus-
Preview difference, and a measurable luminance difference between deterministic
subject regions. The existing PBR close checker continues to require contact
shadow contrast; together these checks cover the initial reference-scene
lighting/shadow contract without treating automated metrics as human visual
approval.

The package also exposes a dedicated subsurface reference fixture. It uses nine
same-camera, same-geometry, same-light UV-sphere subjects and captures the
bounded material response at three explicit variants:

```text
--capture-realism-reference sss close opaque rendered
--capture-realism-reference sss close thin rendered
--capture-realism-reference sss close thick rendered
```

The SSS checker requires matching composition metadata, nine settled subjects,
and measurable image and center-subject color response between the opaque,
thin, and thick variants. This proves that authored subsurface amount and
thickness reach the OpenGL material path with a stable pixel response. It does
not promote the bounded three-lobe approximation to production multi-scatter,
screen-space, or ray-traced subsurface transport.

The Windows visual-evidence harness exposes the scene with:

```text
--capture-realism-reference wide|close solid|material_preview|rendered
--capture-realism-reference normal_map wide|close solid|material_preview|rendered
--capture-realism-reference color_space wide|close solid|material_preview|rendered
--capture-realism-reference lighting wide|close solid|material_preview|rendered
--capture-realism-reference sss close opaque|thin|thick rendered
--capture-realism-reference ssgi wide|close rendered
--capture-realism-reference ssgi_motion wide|close rendered output_directory
```

The ordinary reference captures emit `CAPTURE_READY_REFERENCE` metadata proving the selected
PBR reference view, all nine settled subjects, the deterministic camera, and
the centered reference bounds. Lighting captures emit the corresponding
lighting-prefixed metadata and use the dedicated lighting checker. These are
calibration and regression fixtures, not proof that every material or renderer
effect is production-complete. The SSGI motion profile emits its own
`CAPTURE_READY_SSGI_MOTION_REFERENCE` phase records as described below.

The SSGI reference is Rendered-only because the bounded indirect-diffuse term
is a fullscreen post-process over the HDR color and depth targets:

```text
--capture-realism-reference ssgi wide|close rendered
```

It emits `CAPTURE_READY_SSGI_REFERENCE` only after the renderer reports that
the screen-space indirect path was enabled for a settled frame. The checker
also requires the deterministic nine-subject composition and a non-flat
image. It also applies bounded fixture-specific sanity limits for excessive
HDR clipping, over-bright pixels, and bright subject-edge halos. This is an
activation, presentation-stability, and gross-artifact guard for the current
supported OpenGL path, not proof that every pixel receives indirect light or
that subtle leaks, camera-motion stability, and performance are solved.

The motion-stability reference is a separate Rendered-only capture:

```text
--capture-realism-reference ssgi_motion close rendered output_directory
```

It emits settled `before` and `after` readiness records from one process,
applies one bounded in-process camera translation, and requests the next
completed frames through Henka's application-owned OpenGL framebuffer
readback. The Windows harness converts those bounded readbacks to inspection
images and verifies the deterministic camera delta, matching dimensions,
non-flat content, gross brightness limits, bounded luminance-distribution
change, and a real sampled pixel difference. This guards current-frame
continuity in the supported OpenGL path without claiming artifact-free motion
at every speed, full temporal reconstruction quality, or production global
illumination.

The performance reference is a separate Rendered-only measurement:

```text
--capture-realism-reference ssgi_performance close rendered
```

It holds the same nine-subject reference at a fixed 1280x720 viewport, waits
for three settled frames, then records 32 bounded frame, scene-CPU, and
available OpenGL scene-GPU timing samples while the SSGI path is active. The
checker requires the timer query to be available and rejects a gross GPU
runaway above the 100 ms reference budget. The budget is a regression guard
for this fixed supported OpenGL reference; it is not a universal frame-rate
promise or an isolated SSGI-shader cost measurement.

## Screen-space indirect diffuse lighting

Rendered presentation also contains a bounded screen-space indirect diffuse approximation. It reconstructs the current receiver position and normal from depth, samples nearby visible HDR surfaces in eight directions, rejects samples outside bounded distance and thickness limits, caps source radiance, and adds the gathered indirect contribution in HDR before bloom and tone mapping.

The purpose is to introduce visible local diffuse light transfer and color bleeding while preserving the existing OpenGL baseline.
The screen-space gather uses symmetric receiver-depth reconstruction, depth-edge confidence, and a small cross-filter on source radiance. Those filters intentionally suppress unstable high-frequency contributions at silhouettes, thin geometry, and other screen-space discontinuities instead of allowing the indirect term to amplify subpixel edge variation.

This is not full global illumination. The screen-space method cannot see geometry that is outside the current view, hidden behind another surface, or otherwise absent from the depth/color buffers. It is single-frame and bounded; it does not claim multi-bounce transport, probe-volume GI, hardware ray tracing, or path tracing.

## Scene-probe diffuse transfer

When the Rendered path has a captured scene-reflection probe, opaque scene
materials can also receive a bounded local diffuse color-transfer approximation
from five clamped cubemap samples around the receiver normal. This reuses the
existing scene-probe capture boundary and is reported through
`rendered_reflection_probe_diffuse_active`; the SSGI reference metadata requires
the screen-space indirect path, probe-diffuse path, five-level probe prefilter,
and bounded two-probe overlap path to be active.

This is scene-space support for the current reference fixture, not a full
irradiance volume or global-illumination solution. It does not provide
probe-grid blending, production irradiance filtering, multi-bounce transport,
or guaranteed hidden/off-screen contribution beyond what the captured probe
contains. Missing shader support or an unavailable probe fails closed to the
existing environment-lighting path.

Local reflection-probe specular now allocates and generates five cubemap mip
levels (64, 32, 16, 8, and 4 pixels per face). The material roughness LOD
selection therefore has a real bounded filtered source instead of requesting
roughness levels from a level-zero-only texture. The generated chain is a
stability and plausibility improvement for the supported OpenGL path; it is
not a production GGX convolution or probe-grid system. When two captured probe
volumes contain a receiver, the renderer deterministically ranks a primary and
secondary probe and blends their specular and diffuse contributions with a
bounded inverse-score weight. If only one capture is valid, it promotes that
capture and uses the shared environment fallback otherwise. This is a bounded
overlap path, not a production probe-grid or multi-probe filtering solution.

## Direction

The next realism work should build from reference scenes. Effects outside those scenes should wait until a concrete visual need is identified. The current reference coverage includes deterministic normal-map, exposure-range, and bounded subsurface-response fixtures in addition to the PBR and lighting fixtures. Important follow-up tracks are:

1. extend the PBR, lighting, and HDR reference scenes to validate energy response, roughness/metallic response, IBL calibration, and light/shadow stability; dedicated normal-map and color-space A/B foundations are now in place;
2. replace the bounded subsurface approximation with a production SSS solution when profile, thickness, ownership, and performance contracts are defined; the current SSS fixture only guards the existing bounded response;
3. extend the bounded screen-space indirect diffuse validation for leaks, halos, over-brightening, and deeper hardware-specific performance characterization; current activation, camera-motion, and gross fixed-reference performance guards are in place;
4. extend the current scene-probe diffuse foundation into a bounded
   irradiance-probe or probe-grid solution that can represent off-screen and
   hidden contributors with explicit capture, filtering, blending, and
   performance contracts;

5. extend local reflection-probe placement, capture policy, and interaction with
   indirect diffuse lighting; bounded two-probe overlap blending is now part of
   the supported OpenGL foundation, while probe-grid scale and production
   filtering remain future work;
6. retain rasterization as the broad hardware baseline while designing future renderer-backend boundaries for optional hardware ray tracing;
7. consider a path-traced reference renderer later as a visual ground-truth tool, even if production games continue to use hybrid real-time rendering.

## Exposure and HDR-range reference

The renderer applies viewport exposure in linear HDR before the final ACES-style tone map. The public engine boundary accepts finite exposure values from -16 to +16 stops and rejects invalid values. The deterministic HDR-range reference captures the same nine-subject fixture, camera, layout, environment, and rendered path at three explicit settings:

```text
--capture-realism-reference hdr close -2 rendered
--capture-realism-reference hdr close 0 rendered
--capture-realism-reference hdr close 2 rendered
```

The `HDR_RANGE_REFERENCE` profile records the requested exposure in readiness metadata and `scripts/check_hdr_range_visual_evidence_windows.ps1` verifies that the captures retain the same composition, show a monotonic luminance response, remain spatially non-flat, and do not become overwhelmingly clipped at +2 stops. This proves bounded exposure/HDR response in the supported OpenGL presentation path; it does not claim automatic exposure, HDR display calibration, or full HDR mastering.

Spectral rendering is not part of the current implementation. It should remain research work until RGB PBR, indirect lighting, material import, color management, and reference-scene validation are mature.
