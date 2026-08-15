# Sandbox showcase assets

The normal Windows Sandbox startup presents two repo-owned sample models. It also applies the renderer's bounded view-aware subsurface material-instance response to the warm Giraffe regions, while leaving eyes and hard feature materials on their authored opaque response. This is visual dogfooding of the existing material-instance path, not a claim of full multi-scatter or ray-traced subsurface transport:

Provenance is explicit: the current pair and generated detail textures are `GENERATED_TEST_FIXTURE` / `IMPORT_COMPATIBILITY_ASSET` content produced by the deterministic repository generator and consumed through the public glTF/material asset path. They remain imported fixture content, not `HENKA_NATIVE_AUTHORED` proof. The separate native-authoring bridge now provides a nontrivial user-created rocket path, and the packaged gate saves, reloads, relaunches, and renders that authored source transactionally; native multi-material binding and a fully native-authored Giraffe remain open.

## Bounded native-authoring bridge

The normal editor now provides the shortest native dogfood path for those
fixtures. Press `F5` until `Inspect` or `Full Tools` is active, select a
`Showcase Giraffe ...` or `Showcase Rocket ...` primitive in `Scene Objects`.
Selecting one prioritizes `Object Details > Authoring`; choose `Make Editable`.
Henka converts
that validated primitive into a user-owned authoring mesh connected to the
selected scene entity; the existing component, topology, UV, undo/redo, and
`Save Project` / `Reload Project` controls operate on that source afterward.
While that source is active, Scene View component picks also remain usable when
a frontmost spot or decal primitive from the same showcase asset wins the render
ray; the visible hit is routed through the selected native source and the source
mesh still performs the final component intersection validation.
The saved native source is also discovered on a later normal packaged launch;
valid mesh and owned-material sidecars are restored transactionally, while a
malformed or missing slot retains the imported glTF render.

This is intentionally a bounded bridge rather than a completed content
workspace. It currently targets the imported showcase primitives and preserves
their borrowed glTF material identity until the user explicitly chooses
`Own Material`. That action adopts a manager-owned runtime material definition;
the bounded controls then exercise base-color, metallic, roughness,
emissive-strength, IOR, transmission, subsurface amount, bounded
subsurface-thickness, and subsurface-tint edits
plus in-engine procedural detail-normal and
metallic-roughness texture creation without changing the imported glTF source.
The packaged
Windows dogfood path exercises Make Editable, material and texture edits, a
component move, base-color, metallic, roughness, emissive-strength, IOR, and
transmission edits,
in-engine procedural detail-normal and metallic-roughness texture creation,
bounded material
undo/redo, Face selection, bevel, Save Project, and Reload Project
transactionally. The project
manifest persists the mesh source,
transform, and visibility, while its bounded `.material` sidecar persists the
supported PBR scalars, colors, flags, alpha mode, and seven material texture
identities; native runtime detail-normal and metallic-roughness textures are
recreated from their bounded recipes on reload. Texture painting,
native-authored source export, and native multi-material binding remain open.
The packaged gate already supplies bounded real-user creation/edit,
save/reload, relaunch restoration, and Rendered evidence for the native
rocket; it does not promote the imported fixture pair to native-authored
content.

The editor also exposes `Create Native Rocket`. That user-facing action
creates a bounded multi-part rocket directly through the Henka authoring mesh
API, evaluates it into the scene, adopts a manager-owned material instance,
and emits `HENKA_NATIVE_AUTHORED` provenance before normal component, topology,
material, and project save/reload editing. The packaged relaunch gate restores
the valid authored source and material sidecar transactionally; native
multi-material binding remains explicitly unfinished.

- `cheeky_giraffe.gltf` — an original realism-oriented Cheeky Giraffe mascot with smooth high-curvature body and head forms, surface-fitted spots, a readable mane row, compact flattened ear lobes with inset inner-ear patches, connected tan ossicone stalks, and outward-angled dark-brown caps, layered eye whites/irises/pupils/highlights, nostrils, a defined muzzle/jaw, and a subdued mouth line. The presentation retains restrained personality, while the face is built from separately shaded anatomical features rather than a single cartoon eye material.
- `original_realistic_rocket.gltf` — an original modern launch vehicle reference with a continuous tapered core/fairing profile, interstage and engine-skirt hardware, dark avionics separation bands, a bounded seven-engine bell/nozzle cluster, a restrained heat response, and tapered stabilization fins.

## Ownership and generation

The Sandbox target build runs `scripts/generate_showcase_assets.ps1` for both files. The generator creates deterministic bounded geometry, UVs, normals, tangent vec4 attributes, PBR material definitions, and a sibling binary buffer. It uses no third-party model, no external authoring application at runtime, and no copyrighted vehicle or character model.

The generated glTF files use material factors plus deterministic, generated base-color, tangent-space detail-normal, and metallic/roughness textures through the same manager-owned semantic texture path as imported consumer assets. The Giraffe's flush spot pattern is a base-color texture on its tan skin material; the remaining mane and facial features stay independently shaded geometry. They also use the renderer-supported clearcoat, restrained sheen, and emissive-strength extensions; they do not bind the unrelated primitive-gallery cube texture. Their glTF, sibling binary buffers, and texture sidecars are copied beside the executable, so packaged execution resolves the showcase from package-owned files without a repository-root or runtime authoring dependency. These two models are the public visual reference set and should be regenerated and manually reviewed after material, lighting, geometry, and shading improvements. They are deliberately generic public samples; project-specific scenes and visual references are not part of this repository.

The packaged studio HDR fixture is also generated in memory and now contains bounded asymmetric warm-key and cool-fill area-light lobes. This gives clearcoat, brushed metal, and emissive engine details a stable highlight structure through the derived IBL path while retaining the direct local-light and shadow fixtures. It is an authored lighting reference, not a claim of photographic HDRI coverage.

The Sandbox studio floor uses a bounded 64 m graphite plane beneath the
independent debug grid. The larger surface keeps its finite far edge outside
ordinary showcase framing, so Rendered and Material Preview do not acquire an
unintended diagonal environment seam.

## In-engine visual acceptance

The normal Windows graphical path is the source of truth for showcase review. The
existing same-camera capture covers Solid, Material Preview, and Rendered, while
the dedicated mascot inspection capture launches the real Sandbox repeatedly
with the generated glTF scene loaded and records a normal startup frame plus
Giraffe close front, close three-quarter, close profile, and wide silhouette
Rendered views, a Giraffe front Material Preview comparison, and Rocket close
front, close three-quarter, and profile Rendered views. The dedicated views hide
only editor chrome for application-only inspection; they do not replace the
scene, material, lighting, asset-manager, or renderer path.

```powershell
.\scripts\capture_visual_evidence_windows.ps1 `
  -Configuration Debug `
  -OutputDirectory (Join-Path (Get-Location) "build\visual_evidence_giraffe") `
  -IncludeStartupShowcase `
  -IncludeGiraffeInspection
.\scripts\check_showcase_visual_evidence_windows.ps1 `
  -InputDirectory (Join-Path (Get-Location) "build\visual_evidence_giraffe")
```

The capture process waits up to a bounded 20 seconds for a CAPTURE_READY record
from the real application. That record proves both named subject groups are
visible, their authoritative bounds and render meshes are ready, the final
Scene View viewport is known, the front camera is level, the combined midpoint
is centered, both projected rectangles have safety margins, and three settled
frames have completed. Full showcase captures declare the `FULL_SHOWCASE`
evidence profile; inspection-only runs should use `GIRAFFE_INSPECTION` and must
not be used with this full validator. The validator also requires matching readiness metadata
for Solid, Material Preview, and Rendered before checking the images. It then
checks that the evidence identifies the Henka Sandbox executable,
contains all required views, and rejects missing, flat, low-chroma, or
dimension-invalid frames. Those objective guards do not constitute human visual
approval: inspect the retained images for silhouette, facial-feature assembly,
neck/head attachment, eyes, ears, ossicones, nostrils, mouth, mane, spots,
intersections, gaps, and material/shading defects. Generated evidence remains
repo-local and is not committed.

## Runtime path

Sandbox loads both files through `henka_assets_load_gltf_scene_asset` and instantiates them with `henka_assets_instantiate_gltf_scene`. Materials and texture dependencies remain manager-owned and use the same glTF material path as imported consumer assets. A missing or corrupted generated file fails initialization through the normal bounded asset error path; it is not replaced by hardcoded geometry in `main.c`.

The engineering primitive gallery remains available with:

```text
HenkaSandbox3D.exe --primitive-gallery
```

That opt-in path preserves the cube, sphere, marker, fallback, foliage, and realism validation samples used by renderer, material, physics, and editor QA.

The generator rejects non-finite vertex data, invalid frames, out-of-range indices, degenerate triangles, and inward-wound faces before writing either asset. These remain representative engine samples rather than movie-production assets; human visual QA across Solid, Material Preview, and Rendered remains a required acceptance gate.
