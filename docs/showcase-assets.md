# Sandbox showcase assets

The normal Windows Sandbox startup presents two repo-owned sample models. The Giraffe uses a restrained face treatment—compact eyes, a flattened muzzle, and a short level mouth crease—rather than a broad smiling arc. It also applies the renderer's bounded view-aware subsurface material-instance response to the warm Giraffe regions, assigning a manager-owned linear thickness texture to those instances while leaving eyes and hard feature materials on their authored opaque response. Capture metadata proves the thickness assignments are loaded with no fallback. This is visual dogfooding of the existing material-instance path, not a claim of full multi-scatter or ray-traced subsurface transport:

Provenance is explicit: the glTF pair and generated detail textures are `GENERATED_TEST_FIXTURE` / `IMPORT_COMPATIBILITY_ASSET` content produced by the deterministic repository generator and consumed through the public glTF/material asset path. They remain imported fixture content, not native-authoring proof. The repository also carries `assets/authoring/showcase_giraffe.hams` and `assets/authoring/showcase_rocket.hams`, captured from the visible Make Editable, component Move, Face selection, viewport picking, Bevel, Extrude, Save Project, and Reload Project workflow. Those HAMS files contain mesh/topology/UV/material-region data but no provenance metadata; runtime sidecar/evidence labels classify the current files as `HENKA_NATIVE_EDITED_FIXTURE`, meaning persisted editor-owned derivatives of imported fixture geometry. They do not independently prove that a user designed the recognizable Giraffe or Rocket forms. The source artifacts prove the editor-owned persistence boundary, not finished anatomical or mechanical modeling quality. Generic user-directed modeling beyond the bounded tools, native multi-material binding, and a complete production-authoring workspace remain open.

## Bounded native-authoring bridge

The normal editor now provides the shortest native dogfood path for those
fixtures. Start in `Standard` or press `F5` for the temporary `Focus Viewport`
layout, then select a
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
malformed or missing slot retains the imported glTF render. The checked-in
`.hams` artifacts are refreshed only by this visible workflow, using
`scripts/capture_editor_owned_authoring_sources_windows.ps1` as bounded UI
automation. The capture selects Face mode, picks a face in the Scene View, and
invokes generic Bevel and Extrude before Save/Reload; it does not generate geometry or
assemble meshes. Face mode also exposes a bounded `Delete Faces` action. It removes
the current selected face set as one transactional source/render/bounds/history
operation and fails closed if that would leave no renderable face. Vertex and edge
deletion are not implied by this control.

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
The packaged gate supplies bounded creation/edit API dogfood, save/reload,
relaunch restoration, and Rendered evidence for the fixture-derived scene, while
the editor-owned source capture proves the same controls produced persisted
`.hams` artifacts for both showcase subjects. It does not promote the glTF
fixture pair or those persisted derivatives to independently user-authored
content.

The visible native-authoring workflow is intentionally generic. Enter an asset
name, choose `New Asset`, then add bounded Box, Cylinder, Cone, UV Sphere, or
all-quad Quad Sphere parts through the primitive chooser. UV Sphere preserves
its latitude/longitude topology and triangular pole caps; Quad Sphere is a
closed shared-vertex cubed sphere containing only four-sided faces. The
resulting document is editor-owned, uses `HENKA_PRODUCT_NATIVE_AUTHORED`
provenance, and supports the bounded save/close/reopen path. This proves the
reusable authoring foundation, not that the default Giraffe or Rocket was
modeled by a user. The default showcase pair continues to use separately
labeled imported/generated fixture content until the full visible modeling,
material, visual-review, and packaged-asset workflow is complete.

`Make Editable` on an imported `Showcase Giraffe ...` or `Showcase Rocket ...`
exposes only generic component, topology, UV, material, and transactional
history tools. Multi-face extrusion and material-region assignment operate on
the current selection without naming or recognizing either showcase subject.
This is native editing dogfood, not a claim that the generated fixture pair
itself is native-authored or that these bounded controls replace full
anatomical or mechanical modeling tools.

- `cheeky_giraffe.gltf` — the original Anatomical Giraffe Study (the stable filename is retained for package compatibility): a long-legged, narrow-bodied silhouette with a continuous chest-to-neck loft, elongated head and muzzle, four articulated knee regions and grounded hooves, compact ears, ossicones, mane, tail, paired nostrils, recessed dark eyes, and a neutral lip crease. Its deterministic ochre base-color texture uses many small irregular reticulated hide cells rather than a few oversized circular patches.
- `original_realistic_rocket.gltf` — an original generic heavy-lift launch-vehicle study: a warm insulated central core, pale tapered upper vehicle, two separately shaded tapered side boosters, graphite interstages, unbranded muted markings, dark thermal separation bands, independently shaded service-panel geometry, a seven-engine bell/nozzle cluster, perimeter fasteners, restrained stabilization fins, and a bounded adjacent steel service structure above the launch-pad assembly. It intentionally contains no agency branding, mission markings, or copied mission hardware.

## Ownership and generation

The Sandbox target build runs `scripts/generate_showcase_assets.ps1` for both glTF files. The generator creates deterministic bounded geometry, UVs, normals, tangent vec4 attributes, PBR material definitions, and a sibling binary buffer. It uses no third-party model, no external authoring application at runtime, and no copyrighted vehicle or character model. It is explicitly a fixture generator; it is not the design source for the checked-in `.hams` authoring artifacts.

To refresh the editor-owned sources after a validated modeling session, build the
Windows Sandbox and run `scripts/capture_editor_owned_authoring_sources_windows.ps1`.
By default the script launches an isolated copy of the executable and processes
both subjects. For deterministic single-subject reruns, pass
`-Subject Giraffe` or `-Subject Rocket`; those runs use the same visible workflow
and do not reuse stale panel coordinates from the other subject. It selects each imported
subject, invokes the visible Make Editable, component Move, Face selection,
viewport face-picking, Bevel, and Extrude controls, saves and reloads through the visible
project controls, then copies only the resulting
`.hams` files into `assets/authoring`. The resulting hashes are part of the
validation record; runtime generated fixtures and hard-coded C constructors do
not count as authoring provenance.

The generated glTF files use material factors plus deterministic, generated base-color, tangent-space detail-normal, and metallic/roughness textures through the same manager-owned semantic texture path as imported consumer assets. The Giraffe's flush spot pattern is a base-color texture on its tan skin material; hooves, mane, nostrils, and remaining facial features stay independently shaded geometry. The Rocket now binds the generated base-color texture to its painted ceramic surface, while fasteners, thermal details, engine bells, and other mechanical regions retain distinct material identities. Both models use the renderer-supported clearcoat, restrained sheen, and emissive-strength extensions; they do not bind the unrelated primitive-gallery cube texture. Their glTF, sibling binary buffers, and texture sidecars are copied beside the executable, so packaged execution resolves the showcase from package-owned files without a repository-root or runtime authoring dependency. These two models are the public visual reference set and should be regenerated and manually reviewed after material, lighting, geometry, and shading improvements. They are deliberately generic public samples; project-specific scenes and visual references are not part of this repository.

The packaged studio HDR fixture is also generated in memory and now contains bounded asymmetric warm-key and cool-fill area-light lobes. This gives clearcoat, brushed metal, and emissive engine details a stable highlight structure through the derived IBL path while retaining the direct local-light and shadow fixtures. It is an authored lighting reference, not a claim of photographic HDRI coverage.

The Sandbox studio floor uses a bounded 64 m graphite plane beneath the
independent debug grid. The larger surface keeps its finite far edge outside
ordinary showcase framing, so Rendered and Material Preview do not acquire an
unintended diagonal environment seam.

## In-engine visual acceptance

The normal Windows graphical path is the source of truth for showcase review. The
existing same-camera capture covers Solid, Material Preview, and Rendered, while
the dedicated mascot inspection capture launches the real Sandbox repeatedly
with the glTF fixture scene/material path loaded and records a normal startup frame plus
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

The capture helper stages the selected executable and its adjacent assets into
a fresh repo-local runtime for each run. This keeps generated user saves from
an older launch out of the evidence path; the staged runtime is disposable
build output and is not a source or package input.

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
dimension-invalid frames. Those objective guards are a structural capture/evidence
guard only; they do not prove user-authored geometry, realism, completion,
anatomical quality, or mechanical quality. Inspect the retained images for silhouette, facial-feature assembly,
neck/head attachment, eyes, ears, ossicones, nostrils, mouth, mane, spots,
intersections, gaps, and material/shading defects. Generated evidence remains
repo-local and is not committed. Retained screenshots are not automatically
evidence for a later worktree: use them only when their capture index binds the
current executable/source commit, source hashes, and capture metadata; otherwise
their relationship to the current implementation is not verified.

## Runtime path

Sandbox loads both files through `henka_assets_load_gltf_scene_asset` and instantiates them with `henka_assets_instantiate_gltf_scene`; normal non-smoke startup may then restore a compatible checked-in HAMS derivative. A stale or invalid checked-in derivative—including one with fewer authored vertices than the current imported source—is rejected with the imported render retained, so a refreshed generator cannot be silently hidden by an older fixture. User-persisted project data remains a separate user-owned restore path. Materials and texture dependencies remain manager-owned and use the same glTF material path as imported consumer assets. A missing or corrupted generated file fails initialization through the normal bounded asset error path; it is not replaced by hardcoded geometry in `main.c`.

The engineering primitive gallery remains available with:

```text
HenkaSandbox3D.exe --primitive-gallery
```

That opt-in path preserves the cube, sphere, marker, fallback, foliage, and realism validation samples used by renderer, material, physics, and editor QA.

The generator rejects non-finite vertex data, invalid frames, out-of-range indices, degenerate triangles, and inward-wound faces before writing either asset. These remain representative engine samples rather than movie-production assets; human visual QA across Solid, Material Preview, and Rendered remains a required acceptance gate.
