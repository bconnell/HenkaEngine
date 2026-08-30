# Sandbox Showcase Assets

Henka's normal Windows Sandbox startup presents two repository-owned sample models: the Anatomical Giraffe Study and the realistic rocket fixture.

> **Purpose:** These assets are deterministic engine dogfood for imported content, material instances, native editing, persistence, packaging, and visual QA. Their provenance remains explicit throughout the workflow.

## Contents

- [Showcase subjects](#showcase-subjects)
- [Provenance](#provenance)
- [Native-authoring bridge](#native-authoring-bridge)
- [Owned material workflow](#owned-material-workflow)
- [Native authored assets](#native-authored-assets)
- [Generation and package ownership](#generation-and-package-ownership)
- [Visual acceptance](#visual-acceptance)
- [Runtime path](#runtime-path)
- [Current limits](#current-limits)

## Showcase subjects

### Anatomical Giraffe Study

`cheeky_giraffe.gltf` is retained as the stable package filename.

The current subject includes:

- a long-legged, narrow-bodied silhouette;
- continuous chest-to-neck loft;
- integrated shoulder and haunch transitions;
- elongated head and muzzle;
- four articulated knee regions;
- grounded hooves;
- compact ears;
- ossicones;
- mane;
- terminal tail tuft;
- paired nostrils;
- recessed dark eyes;
- a neutral short mouth crease;
- deterministic ochre/tan base-color treatment;
- many small irregular reticulated hide cells.

The face treatment keeps compact eyes, a flattened muzzle, and a short level mouth crease.

Warm Giraffe material regions use the renderer's bounded view-aware subsurface material-instance response. Manager-owned linear thickness textures feed those instances. Eyes and hard feature materials retain their authored opaque response.

Capture metadata records thickness-texture assignment and fallback state.

### Realistic rocket fixture

`original_realistic_rocket.gltf` is an original generic heavy-lift launch-vehicle study.

The current subject includes:

- warm insulated central core;
- pale tapered upper vehicle;
- two separately shaded tapered side boosters;
- graphite interstages;
- muted unbranded markings;
- dark thermal separation bands;
- independently shaded service-panel geometry;
- paired avionics bays;
- cable trays;
- fasteners;
- layered interstage and booster insulation collars;
- seven-engine bell/nozzle cluster;
- perimeter fasteners;
- restrained stabilization fins;
- bounded adjacent steel service structure above the launch-pad assembly.

The fixture contains no agency branding, mission markings, or copied mission hardware.

## Provenance

The generated glTF pair and deterministic detail textures use the following evidence classifications:

- `GENERATED_TEST_FIXTURE`;
- `IMPORT_COMPATIBILITY_ASSET`.

They are produced by the repository generator and consumed through the public glTF/material asset path.

The repository also carries:

- `assets/authoring/showcase_giraffe.hams`;
- `assets/authoring/showcase_rocket.hams`.

Those HAMS files are captured through the visible editor workflow using:

- Make Editable;
- component Move;
- Face selection;
- viewport picking;
- Bevel;
- Extrude;
- Save Project;
- Reload Project.

The current HAMS files contain mesh/topology/UV/material-region data and do not contain their own provenance field. Runtime sidecar/evidence classification labels them `HENKA_NATIVE_EDITED_FIXTURE`, which identifies them as persisted editor-owned derivatives of imported fixture geometry.

The HAMS sources prove editor-owned persistence and native editing participation. Independent user design of the recognizable Giraffe and Rocket forms remains outside that evidence claim.

## Native-authoring bridge

The normal editor provides a bounded native editing path for the showcase fixtures.

### Make Editable workflow

1. Start in `Standard` or use `F5` for temporary `Focus Viewport`.
2. Select a `Showcase Giraffe ...` or `Showcase Rocket ...` object in Scene Objects.
3. Open `Object Details > Authoring`.
4. Choose `Make Editable`.
5. Use the normal component, topology, UV, undo/redo, and project controls.

Henka converts the validated imported primitive into an editor-owned authoring mesh connected to the selected scene entity.

### Component picking

Scene View component picking remains active when a frontmost spot or decal primitive from the same showcase asset wins the render ray.

The visible hit routes through the selected native source and the source mesh performs final component-intersection validation.

### Save and restore

The saved native source can be discovered during a later normal packaged launch.

Valid mesh and owned-material sidecars restore transactionally. Malformed or missing saved slots preserve the imported glTF render.

### Checked-in source refresh

`scripts/capture_editor_owned_authoring_sources_windows.ps1` refreshes the checked-in HAMS artifacts through bounded UI automation.

The capture flow performs:

- Face mode selection;
- Scene View face picking;
- generic Bevel;
- generic Extrude;
- Save Project;
- Reload Project.

The script does not generate or directly assemble showcase geometry.

### Delete Faces

Face mode includes bounded `Delete Faces`.

The command removes the current selected face set as one transactional source/render/bounds/history operation. It fails when the operation would leave no renderable face.

Vertex and edge deletion use their own separate controls and contracts.

## Owned material workflow

Imported showcase primitives retain borrowed glTF material identity until the user selects `Own Material`.

`Own Material` adopts a manager-owned runtime material definition and exposes bounded controls for:

- base color;
- metallic;
- roughness;
- emissive strength;
- IOR;
- transmission;
- subsurface amount;
- bounded subsurface thickness;
- subsurface tint;
- in-engine procedural detail-normal texture creation;
- in-engine metallic-roughness texture creation.

The imported glTF source remains unchanged.

### Packaged material dogfood

The packaged Windows workflow exercises:

- Make Editable;
- material edits;
- texture edits;
- component movement;
- base color;
- metallic;
- roughness;
- emissive strength;
- IOR;
- transmission;
- procedural detail-normal creation;
- procedural metallic-roughness creation;
- bounded material undo/redo;
- Face selection;
- Bevel;
- Save Project;
- Reload Project.

### Material sidecar persistence

The project manifest persists:

- mesh source;
- transform;
- visibility.

The bounded `.material` sidecar persists:

- supported PBR scalars;
- colors;
- flags;
- alpha mode;
- seven material texture identities.

Native runtime detail-normal and metallic-roughness textures are recreated from bounded recipes on reload.

Texture painting, native-authored source export, and native multi-material binding remain future work.

## Native authored assets

Henka also has a visible generic native-authoring workflow.

### New Asset workflow

1. Enter an asset name.
2. Choose `New Asset`.
3. Add bounded primitive parts through the primitive chooser.

Available primitives include:

- Box;
- Cylinder;
- Cone;
- UV Sphere;
- all-quad Quad Sphere.

UV Sphere preserves latitude/longitude topology and triangular pole caps.

Quad Sphere uses a closed shared-vertex cubed-sphere topology with four-sided faces.

The resulting document:

- is editor-owned;
- uses `HENKA_PRODUCT_NATIVE_AUTHORED` provenance;
- supports bounded save/close/reopen.

This workflow establishes native asset creation through Henka's visible authoring path.

The default Giraffe and Rocket remain separately classified imported/generated fixtures and editor-owned derivatives.

### Generic showcase editing

After `Make Editable`, the imported showcase subjects use the same generic component, topology, UV, material, and history tools available to other supported authoring meshes.

Multi-face extrusion and material-region assignment operate on current selection without subject-specific Giraffe or Rocket logic.

## Generation and package ownership

The Sandbox target build runs `scripts/generate_showcase_assets.ps1` for both glTF files.

The generator creates deterministic bounded:

- geometry;
- UVs;
- normals;
- tangent `vec4` attributes;
- PBR material definitions;
- sibling binary buffers;
- generated texture sidecars.

It uses no third-party model and no external runtime authoring application.

The generator is classified as a fixture generator. It is not the design source for the checked-in HAMS editor-owned derivatives.

### Refreshing editor-owned HAMS sources

After a validated Windows modeling session, run:

```powershell
.\scripts\capture_editor_owned_authoring_sources_windows.ps1
```

Default behavior processes both subjects through an isolated executable copy.

Single-subject runs are also available:

```powershell
.\scripts\capture_editor_owned_authoring_sources_windows.ps1 -Subject Giraffe
.\scripts\capture_editor_owned_authoring_sources_windows.ps1 -Subject Rocket
```

Each run uses the same visible workflow and does not reuse stale panel coordinates from the other subject.

The capture sequence includes:

- imported-subject selection;
- Make Editable;
- component Move;
- Face selection;
- viewport face picking;
- Bevel;
- Extrude;
- visible Save Project;
- visible Reload Project;
- copying only resulting `.hams` sources into `assets/authoring`.

The resulting SHA-256 hashes form part of the validation record.

### Generated textures and material features

Both generated glTF files use deterministic generated base-color, tangent-space detail-normal, and metallic/roughness textures through the manager-owned semantic texture path.

The Giraffe base-color spot texture is `256x256` and uses irregular multi-harmonic patch boundaries over the tan skin material. Hooves, mane, nostrils, and other facial features remain independently shaded geometry.

The Rocket binds generated base color to the painted ceramic surface. Fasteners, thermal details, engine bells, and other mechanical regions keep distinct material identities.

Both models use supported:

- clearcoat;
- restrained sheen;
- emissive strength.

The primitive-gallery cube texture is not used by these showcase materials.

### Package files

The generated glTF files, sibling binary buffers, and texture sidecars are copied beside the executable.

Packaged execution therefore resolves showcase data from package-owned files without repository-root or runtime authoring dependencies.

These two models form the public visual reference set and should be regenerated and manually reviewed after relevant material, lighting, geometry, and shading changes.

Project-specific scenes and visual references remain outside this repository.

### Studio HDR fixture

The packaged studio HDR fixture is generated in memory and contains bounded asymmetric warm-key and cool-fill area-light lobes.

This gives clearcoat, brushed metal, and emissive engine details stable highlight structure through the derived IBL path while preserving direct local-light and shadow fixtures.

The fixture is an authored deterministic lighting reference. Photographic HDRI coverage is not part of its claim.

### Studio floor

The Sandbox studio floor uses a bounded 64 m graphite plane beneath the independent debug grid.

The surface extends beyond ordinary showcase framing so its finite far edge remains outside normal Material Preview and Rendered composition.

## Visual acceptance

The normal Windows graphical path is the source of truth for showcase visual review.

### Same-camera evidence

The standard visual evidence set covers:

- Solid;
- Material Preview;
- Rendered.

### Dedicated inspection evidence

The mascot/showcase inspection path launches the real Sandbox repeatedly with the packaged glTF scene/material path active and captures:

**Giraffe**

- normal startup frame;
- close front Rendered;
- close three-quarter Rendered;
- close profile Rendered;
- wide silhouette Rendered;
- front Material Preview.

**Rocket**

- close front Rendered;
- close three-quarter Rendered;
- profile Rendered.

Dedicated views hide editor chrome only for application inspection. Scene, materials, lighting, asset management, and renderer execution remain unchanged.

### Capture commands

```powershell
.\scripts\capture_visual_evidence_windows.ps1 `
  -Configuration Debug `
  -OutputDirectory (Join-Path (Get-Location) "build\visual_evidence_giraffe") `
  -IncludeStartupShowcase `
  -IncludeGiraffeInspection

.\scripts\check_showcase_visual_evidence_windows.ps1 `
  -InputDirectory (Join-Path (Get-Location) "build\visual_evidence_giraffe")
```

### Fresh runtime staging

The capture helper stages the selected executable and adjacent assets into a fresh repository-local runtime for each run.

Older generated user saves therefore stay outside the evidence path.

The staged runtime is disposable build output and is not a source/package input.

### Readiness metadata

The capture process waits up to 20 seconds for a `CAPTURE_READY` record from the real application.

Pair captures use `capture_subject=pair` and verify:

- both named subject groups are visible;
- authoritative bounds are ready;
- render meshes are ready;
- final Scene View viewport is known;
- front camera is level;
- combined midpoint is centered;
- both projected rectangles have safety margins;
- three settled frames completed.

Subject-specific captures use:

- `capture_subject=giraffe`;
- `capture_subject=rocket`.

They apply the same readiness, level-camera, centering, and safety-margin checks to the requested subject.

`FULL_SHOWCASE` identifies full showcase evidence. `GIRAFFE_INSPECTION` identifies inspection-only runs and is not accepted by the full validator.

### Automated image guards

The validator requires matching readiness metadata for Solid, Material Preview, and Rendered before image inspection.

It also verifies:

- Henka Sandbox executable identity;
- all required views;
- valid image dimensions;
- non-flat content;
- minimum chroma requirements where relevant.

These automated checks establish structural capture integrity.

### Human review

Human review should inspect:

- silhouette;
- facial assembly;
- neck/head attachment;
- eyes;
- ears;
- ossicones;
- nostrils;
- mouth;
- mane;
- spots;
- geometry intersections;
- gaps;
- material/shading defects;
- rocket proportion and mechanical assembly quality.

Generated evidence remains repository-local and is not committed.

Retained screenshots remain useful only when the capture index binds the exact executable/source revision, source hashes, and capture metadata.

## Runtime path

Normal Sandbox loading uses:

- `henka_assets_load_gltf_scene_asset`;
- `henka_assets_instantiate_gltf_scene`.

Normal non-smoke startup can then restore a compatible checked-in HAMS derivative.

A stale or invalid checked-in derivative is rejected. The validation includes authored-vertex count consistency against the current imported source. Rejection preserves the imported render.

User-persisted project data remains a separate user-owned restore path.

Materials and texture dependencies remain manager-owned and use the normal glTF material path.

Missing or corrupt generated showcase files fail initialization through the normal bounded asset error path. `main.c` does not replace them with hardcoded showcase geometry.

### Engineering primitive gallery

The diagnostic gallery remains available through:

```text
HenkaSandbox3D.exe --primitive-gallery
```

It contains the cube, sphere, marker, fallback, foliage, and realism validation samples used by renderer, material, physics, and editor QA.

### Generator validation

Before writing either generated asset, the generator rejects:

- non-finite vertex data;
- invalid frames;
- out-of-range indices;
- degenerate triangles;
- inward-wound faces.

## Current limits

The showcase system currently leaves the following work open:

- broader generic user-directed modeling;
- native multi-material binding;
- complete production authoring workspace;
- texture painting;
- native-authored source export;
- advanced anatomical modeling tools;
- advanced mechanical modeling tools;
- full production SSS;
- ray-traced subsurface transport;
- production showcase quality closure.

Human visual QA across Solid, Material Preview, and Rendered remains a required acceptance gate.
