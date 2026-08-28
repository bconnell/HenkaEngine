# Sandbox 3D Help

`henka_sandbox3d` is Henka Engine's current visible example, editor workspace, and QA surface.

> **Default startup:** The Sandbox opens the `Standard` workspace with no selected scene object. The normal scene contains the Anatomical Giraffe Study, the realistic rocket fixture, a manager-owned graphite ground surface, and the editor grid.

## Contents

- [Default scene](#default-scene)
- [Quick controls](#quick-controls)
- [Compass and camera](#compass-and-camera)
- [Scene legend](#scene-legend)
- [Primitive gallery](#primitive-gallery)
- [Authoring workflow](#authoring-workflow)
- [Viewport tools and transforms](#viewport-tools-and-transforms)
- [Sandbox panels](#sandbox-panels)
- [Game authoring and Play](#game-authoring-and-play)
- [Scripting source panel](#scripting-source-panel)
- [Physics QA](#physics-qa)
- [Terrain tools](#terrain-tools)
- [Workspace docking and detached windows](#workspace-docking-and-detached-windows)
- [Diagnostics](#diagnostics)
- [Viewport shading](#viewport-shading)
- [Stress and validation modes](#stress-and-validation-modes)
- [Packaged runs](#packaged-runs)
- [Current limitations](#current-limitations)

## Default scene

The normal showcase contains:

- **Showcase Giraffe** — Anatomical Giraffe Study;
- **Showcase Rocket** — realistic rocket fixture;
- **Ground** — restrained graphite/slate surface with subtle albedo, normal, and wet/dry roughness variation;
- **Debug Grid** — floor grid for scale, depth, and movement reference.

The glTF files are deterministic repository-owned fixture assets generated during the Windows build and packaged beside their binary buffers.

Normal startup follows this content path:

1. load and instantiate the packaged glTF scene/material data;
2. restore checked-in HAMS geometry when available;
3. restore valid saved local Sandbox/editor state.

The HAMS files are persisted editor-owned derivatives of fixture geometry. Their provenance does not establish independent user-authored geometry proof.

The default showcase materials use their own material factors. The primitive-gallery cube texture belongs to the engineering sample path.

### Local settings

The Sandbox stores local settings for:

- wireframe/shading state;
- grid visibility;
- mouse sensitivity;
- camera preset;
- camera pose where currently supported;
- orthographic zoom;
- panel visibility;
- workspace state;
- supported Compass settings;
- saved layout and panel presentation state.

`Focus Viewport` is a temporary layout and is not the startup state.

## Quick controls

| Input | Action |
| --- | --- |
| `W A S D` | Move across the scene |
| `Q / E` | Move down / up |
| `Shift` | Move faster |
| Mouse | Look while mouse capture is active |
| `Right Mouse` / `Tab` | Toggle mouse capture |
| `Left Mouse` | Use the active viewport tool while capture is released |
| `Alt + Left Mouse` | Orbit around the selected object or current view target |
| `Middle Mouse` | Pan |
| Mouse Wheel | Dolly Perspective view or change orthographic height when over Scene View |
| Compass click | Snap to Front, Back, Left, Right, Top, or Bottom |
| Compass drag | Orbit around the navigation target |
| Compass `P/O` | Toggle Perspective / Orthographic projection |
| `F1` | Toggle legacy wireframe state |
| `F2` | Print scene legend to console |
| `F3` | Show/hide debug grid |
| `F4` | Show/hide Sandbox panels |
| `F5` | Switch Standard / Focus Viewport layouts |
| `F` | Frame selected object |
| `H` | Print controls and scene legend |
| `Home` | Reset camera view |
| `Escape` | Close UI, release mouse, then exit through the normal sequence |
| Window close | Exit |

### Transform hotkeys

With a visible unlocked selection:

| Input | Action |
| --- | --- |
| `M` / `G` | Start Move |
| `R` | Start Rotate |
| `S` | Start Scale |
| `X`, `Y`, `Z` | Choose transform axis |
| `Enter` / `Left Mouse` | Confirm |
| `Escape` / `Right Mouse` | Cancel and restore original transform |
| `Left Ctrl` | Stepped adjustment |
| `Left Shift` | Finer movement |

Changing selection, clearing selection, hiding or locking the target, or changing tools cancels the active transform session.

Custom profiles are documented in [editor-controls.md](../editor-controls.md).

## Compass and camera

The Scene View contains a procedural Compass overlay in the upper-right by default.

The Compass uses the active camera basis and provides:

- `N/E/S/W` orientation;
- Top and Bottom targets;
- canonical view snaps;
- drag orbit around the current navigation target;
- `P/O` projection toggle;
- an info strip for orientation, position, and target modes.

Utility > Settings controls:

- visibility;
- left/right placement;
- size;
- info-strip visibility;
- info mode;
- smooth snap navigation.

Compass preferences share one editor settings model and save transactionally to the local settings file.

A missing navigation target disables target-dependent snap/projection actions and reports the missing target directly.

### Camera presets

`Camera/Status` exposes:

- Perspective 3D;
- Side 2.5D;
- Top-down 2.5D;
- Isometric 2.5D.

Use Orbit, Pan, Mouse Wheel or touchpad scrolling, `F`, and `Home` to test framing, projection-aware zoom, navigation, and preset reset.

Normal startup and `Home` share the scene-first framing path. Old transient camera-pose settings are not automatically restored.

## Scene legend

| Object | Purpose |
| --- | --- |
| `Showcase Giraffe` | Anatomical Giraffe Study through the packaged glTF scene/material path |
| `Showcase Rocket` | Realistic rocket fixture through the packaged glTF scene/material path |
| `Ground` | Slate/graphite receiver for scale and lighting |
| `Debug Grid` | Floor reference for position, depth, and movement |

## Primitive gallery

Run:

```text
henka_sandbox3d.exe --primitive-gallery
```

This opens the engineering sample content, including:

- `Textured Cube`;
- `Material Ball`;
- `glTF Marker`;
- `Missing Texture`;
- `Missing Model`;
- foliage;
- realism material row;
- engineering primitives.

The fallback-texture and fallback-model examples prove visible failure behavior without stopping the engine.

## Authoring workflow

### Textured Cube

In `--primitive-gallery`, select `Textured Cube` and open Object Details > Authoring.

The current authoring workflow provides:

- Vertex mode;
- Edge mode;
- Face mode;
- Extrude;
- Inset;
- Bevel;
- Subdivide;
- Project UV;
- Pack UV;
- Undo;
- Redo;
- Save Source;
- Reload Source.

Viewport picking resolves directly against authored polygons and component identities.

### Selection presentation

| Mode | Visual presentation |
| --- | --- |
| Vertex | Amber cross markers |
| Edge | Cyan segments with endpoint markers |
| Face | Orange outline with center marker |

The most recently picked component is the active edit target and receives a stronger mode-specific highlight.

The viewport also shows:

- active topology mode;
- selected-component count;
- cached projected topology outline for editable subjects.

Newly created or imported authoring wrappers begin with no component selection. The whole-object outline remains visible until a component is selected.

### Selection operations

Current bounded selection operations include:

- click select;
- `Ctrl`-click add;
- box replace;
- `Ctrl`-drag add;
- `Shift`-drag subtract;
- Grow Selection;
- Select Connected;
- Select All;
- Select None;
- Invert;
- Shrink.

Normal selection accepts visible front-facing authored components. X-Ray permits front-facing source selection through occluding mesh surfaces while preserving material and asset data.

Small and medium meshes use projected coverage/depth subdivision. Dense imported meshes use a fixed screen-space spatial index. Index overflow uses conservative topology presentation to keep selection feedback bounded.

Frontmost same-showcase spot/decal primitives route back through the selected source mesh so visible layered content remains pickable through the authoring representation.

### Component transforms

Current bounded component transforms include:

- Move X+;
- Move Y+;
- Move Z+;
- Rotate Selected;
- Scale Selected;
- Soft Move X+;
- Soft Move Y+;
- Soft Move Z+;
- face-normal translation;
- selected extrusion workflows supported by the active component domain.

Sandbox Rotate and Scale use the median pivot. The authoring API also exposes active-component and per-face individual pivots plus world, local, and face-normal rotation orientation.

Soft Move uses a one-ring linear falloff:

- selected vertices receive full movement;
- directly adjacent vertices receive half movement.

All successful operations publish through the shared topology, evaluated mesh, bounds, linked physics, and undo path.

### Imported showcase editing

For Showcase Giraffe or Showcase Rocket:

1. Open Object Details > Authoring.
2. Choose `Make Editable`.
3. Select a visible Vertex, Edge, or Face component.
4. Use Grow Selection or Select Connected as needed.
5. Apply supported Move, Scale, Soft Move, Bevel, or Extrude operations.
6. Use Undo/Redo to verify authoring history.
7. Use Save Project / Reload Project to verify persisted authoring state.

The visible workflow uses generic Henka authoring tools. Showcase-specific geometry shortcuts are not part of the production authoring path.

### Own Material

`Own Material` promotes the selected manager-owned definition into the bounded editable material-instance path.

Current compact controls include:

- base color;
- metallic;
- roughness;
- emissive strength;
- IOR;
- transmission;
- subsurface amount;
- subsurface thickness;
- subsurface tint;
- bounded procedural detail-normal texture creation;
- bounded metallic-roughness texture creation;
- Undo Mat;
- Redo Mat.

Save Project can persist the supported owned-material sidecar with PBR scalars, colors, flags, alpha mode, and material texture identities.

### Native authored assets

The visible native-authoring workflow supports:

1. enter an asset name;
2. choose `New Asset`;
3. add bounded primitive parts.

Available primitive parts include:

- Box;
- Cylinder;
- Cone;
- UV Sphere;
- all-quad Quad Sphere.

UV Sphere retains latitude/longitude topology with triangular pole caps. Quad Sphere uses closed shared-vertex cubed-sphere topology with four-sided faces.

The resulting document is editor-owned, uses `HENKA_PRODUCT_NATIVE_AUTHORED` provenance, and supports bounded save/close/reopen.

### Repository-owned showcase source refresh

`scripts/capture_editor_owned_authoring_sources_windows.ps1` refreshes repository-owned showcase authoring sources through the visible workflow.

The script:

- uses an isolated executable copy;
- does not generate or directly assemble showcase geometry;
- records SHA-256 hashes for resulting `.hams` files.

### Authoring persistence and history

Save Source and Reload Source use a confined Sandbox user-data slot.

Candidate success is required before evaluated mesh and scene bounds are replaced.

Save Project writes the bounded native source and optional owned-material sidecar. A valid saved showcase state restores transactionally on a later normal Sandbox launch. Malformed or missing saved state preserves the imported render.

Face selection participates in authoring history:

- topology edits select their result;
- Undo/Redo restores matching face identity when valid;
- editing after Undo clears the redo branch.

The Authoring section reports evaluated render-mesh material-region range after successful edit, undo, redo, save, and reload. Region values currently provide metadata continuity diagnostics. Multiple renderer material-instance selection by region remains future work.

General arbitrary mesh-file open/save remains unfinished.

### Loose components

The authoring representation supports:

- polygonal surfaces;
- loose source vertices;
- standalone wire edges.

The Sandbox renders loose vertices and standalone edges as viewport point/line primitives and exposes bounded create/edit controls.

Generalized loose-component editing and broad topology coverage remain future work.

## Viewport tools and transforms

The Viewport Tool section exposes:

- Select;
- Orbit;
- Pan;
- Move;
- Rotate;
- Scale.

### Tool behavior

**Select** keeps normal object picking active.

**Orbit** uses left drag around the current view target.

**Pan** uses left drag to move the current view target.

**Move** drags the selected object along a world axis.

**Rotate** rotates the selected object around a world axis.

**Scale** uses the center square for uniform scale in the current Sandbox path.

The Tools panel also provides snapping state and current move, rotate, and scale snap values.

### Touchpad-friendly Select behavior

In Select mode, dragging empty Scene View space beyond the small drag threshold pans the camera. Ordinary clicks continue to perform selection.

### Gizmo rules

The transform gizmo:

- uses viewport-relative framebuffer coordinates;
- uses the same projected handle model for drawing and hit testing;
- stays clipped to Scene View;
- stops safely if the selected object becomes hidden or invalid;
- stops safely when viewport state changes during drag;
- uses internal helper pieces excluded from normal runtime selection and Scene Objects.

Hidden objects are automatically transform-locked. Showing an object does not unlock it. `Unlock Transform` is explicit.

Direct Transform QA controls exercise selected-object mutation through the same Action API and provide a diagnostic path when gizmo interaction needs investigation.

## Sandbox panels

Press `F4` to show or hide panels. Press `F5` to switch the primary workspace mode.

### Workspace modes

- **Standard** — stable editor shell with dedicated Scene View.
- **Focus Viewport** — temporary scene-dominant layout.
- **Saved/custom layouts** — user-controlled panel topology and placement.

When panels are hidden, a small viewport hint shows the `F4` and `F5` controls.

If expected panels are missing after a build, refresh the package:

```powershell
.\scripts\package_sandbox3d_windows.ps1
```

Then verify that `out/HenkaSandbox3D/PACKAGE_INFO.txt` was refreshed.

### Tools panel

The Tools panel currently provides:

- Build, Game, and World work-context controls;
- saved/custom layouts;
- `Main` page;
- `Panels/Status` page;
- Grid toggle;
- Wire toggle;
- Frame Selected;
- Reset View;
- Zoom In / Zoom Out;
- Camera/Status;
- Perspective / Side / Top-down / Isometric camera presets;
- Select / Orbit / Pan / Move / Rotate / Scale viewport tools;
- snap controls;
- Hit Boxes toggle;
- Save Settings;
- Reset Layout;
- panel visibility toggles;
- Help;
- Scene Legend;
- Paths;
- Settings;
- Diagnostics;
- Transform QA;
- Object Info;
- Physics QA;
- Open Native Panel Test;
- current action/warning status.

### Scene Objects panel

Scene Objects lists current Sandbox examples by name.

- Clicking a row selects the object.
- Hidden objects remain listed and show a hidden state.
- The selected row identifies the current object.
- Editable visible unlocked selections show the yellow viewport transform highlight.
- Locked objects remain selectable and inspectable without the transform highlight or gizmo.
- Ground begins locked.
- Unlocking Ground requires an explicit Object Details action.
- Selection overlays remain clipped to Scene View.
- Paging keeps all sample objects reachable in short docks.

Startup has no selected scene object. Object Details, Physics QA, Diagnostics, and the compact strip report that state directly.

### Object Details panel

Object Details reports:

- name;
- tag when available;
- visibility;
- position;
- scale;
- object purpose;
- mesh summary;
- material summary;
- texture/fallback summary;
- interaction eligibility.

Full material detail also exposes:

- effective material description;
- shared definition identity;
- instance override values;
- semantic texture dependencies;
- transactional reimport status.

Safe object actions include:

- visibility changes;
- Focus Camera;
- Reset Transform;
- Print Object Info.

## Game authoring and Play

In the Game authoring context, registered Scene Document objects expose authored Physics and Interaction groups.

### Authored Physics

Current fields include:

- enabled / disabled;
- Static / Dynamic body type;
- Box / Sphere shape;
- Trigger / Solid Collider.

These fields are Scene Document authoring data.

### Authored Interaction

Current fields include:

- enabled state;
- authored prompt.

### Scene save/reload

The Actions group provides Save Scene and Reload Scene for the confined `sandbox3d_scene.hscene` file.

While Play is running or paused, scene authoring edits, scene save/reload, and other scene mutations are rejected.

### Play lifecycle

`Start Play` creates:

- an independent runtime scene clone;
- bounded runtime bodies from authored values;
- runtime behavior state;
- persisted Audio emitters through the production Play path when configured.

Available controls are:

- Start;
- Pause;
- Resume;
- Step;
- Stop.

Stop destroys runtime Play state and leaves authored transforms, visibility, and document values unchanged.

## Scripting source panel

Object Details provides bounded:

- `Add Lua`;
- `Add HenkaScript`.

Each action creates a confined project-relative source template and attaches it transactionally to the authored object. Existing source files are not overwritten.

Save Scene is required to persist the attachment.

The source panel supports:

- compiler-backed editing;
- diagnostics;
- Save;
- Revert;
- Reload.

Reload uses the Play-session runtime seam while Play is running or paused and reloads persisted source through the non-Play path when Play is inactive.

Failed candidate reload preserves the active behavior. Compiler/backend line, column, and message diagnostics are retained for the failed candidate.

Exported properties and debugger presentation remain future work.

## Physics QA

Physics QA is an opt-in fixed-step rigid-body demonstration.

Available controls and diagnostics include:

- Enable;
- pause/resume/step/reset;
- gravity;
- body type changes;
- Make Dynamic + Drop;
- impulse actions;
- velocity clearing;
- camera raycasts;
- collision/trigger events;
- collider overlays;
- contact overlays.

`Make Dynamic + Drop` activates only the selected supported body at its current transform. `Enable` starts the arranged multi-body demonstration.

Supported current collider types are:

- sphere;
- axis-aligned box;
- plane.

Body behavior:

- Static bodies ignore gravity, force, and impulse motion.
- Dynamic bodies fall and respond to physics.
- Kinematic bodies ignore gravity and move through explicit tool/code movement.

Collider/contact overlays are separate from selection highlighting and remain clipped to Scene View.

Fixed substeps commit atomically. Allocation failure and numeric-range failure preserve prior bodies, contacts, events, pair history, accumulator, and linked scene transforms. No partial event or scene update is published.

With a bound Textured Cube physics body, use Extrude and Undo and inspect Physics QA to verify that collider bounds follow evaluated authoring bounds.

## Terrain tools

Utility > Terrain exposes current Terrain editing and dependency inspection.

### Material layers

The live manager-owned layer set contains:

- Grass;
- Dirt;
- Rock;
- Wet.

The Material layers section reports, for base color, normal, and metallic/roughness textures:

- dimensions;
- GPU format;
- resident mip count;
- total mip count.

This is read-only dependency inspection. Complete viewport material-preview authoring remains future work.

### Terrain editing

Current buttons include:

- Raise;
- Lower;
- Flatten;
- Smooth;
- Paint.

Brush controls include:

- radius;
- strength;
- material layer;
- falloff.

Normal editor use can place the brush from viewport Terrain ray picking. When Terrain and a scene object both produce valid hits, the nearest valid hit wins.

Height edits use the bounded collision-runtime queue for the full edit footprint and neighbor coverage. Paint-only edits do not rebuild collision.

Dirty CPU-resident Terrain regions autosave transactionally every ten seconds. Failed saves remain Dirty and can retry later.

The automated smoke path uses a fixed repeatable sample center.

## Workspace docking and detached windows

### Docking

`DRAG` marks a live panel header.

To dock a panel:

1. drag the header;
2. move over a valid left/right dock outline;
3. release.

Panels can stack in an occupied side dock.

Release away from valid dock outlines to detach the panel into a separate OS-level window.

### Detached windows

Detached production panels support:

- matching controls;
- OS frame move/resize;
- bounded saved placement;
- safe close-to-redock;
- explicit dock-return control;
- title-bar drag-back recognition when a focused detached window enters the main-window envelope;
- the same wheel and scrollbar behavior through the detached input path.

Close returns the panel to its last valid dock.

`Open Native Panel Test` creates a separate native test window that reports:

- engine/native identifier;
- focus;
- size;
- last routed event;
- close guidance.

Scene View remains the main viewport and is not detachable yet.

### Dock resizing and layout slots

- Drag the narrow bars beside Scene View to resize occupied docks.
- `Save Custom` / `Restore Custom` manage the primary named workspace.
- Studio and Assembly provide two additional bounded local layout snapshots.
- Restoring a slot redocks detached panels before applying the snapshot.
- `Tab` / `Shift+Tab` cycles focus across visible workspace panels.
- The focused header shows a green accent.
- Merged workspace tabs can activate, reorder, and accept dropped panels.
- Tools > Undo Layout / Redo Layout operate on bounded workspace history.
- With panels visible, `Ctrl+Z` undoes layout and `Ctrl+Y` or `Ctrl+Shift+Z` redoes it.
- Reset Layout restores default topology and disclosure/scroll state, redocks detached panels, and preserves valid named layout slots.
- Divider and splitter hover uses matching system resize cursors.

### Scroll behavior

Tools Main and Object Details use fixed headers with bounded scrollable bodies.

Property groups have:

- stable internal identity;
- persisted expanded/collapsed state;
- recomputed scroll extents after collapse/resize.

Wheel and continuous touchpad deltas remain with the panel body while hovered. Scene View retains wheel ownership for camera zoom.

Object Details supports bounded up/down property-group reordering. The order is presentation-only and persists independently of selected-object state.

## Diagnostics

The compact strip under Scene View reports live interaction state, including:

- active tool;
- selection;
- selection-highlight state;
- pointer ownership;
- gizmo state;
- hover state;
- drag state;
- rejection state;
- hovered panel/header state;
- workspace drag state.

Full Diagnostics reports additional:

- viewport-local cursor state;
- selected-object validity;
- gizmo-model validity;
- overlay primitive count;
- hovered handle;
- active drag target;
- last rejected interaction reason;
- last Action API result;
- native test-window open/focus/size state;
- active shading/exposure state;
- texture-residency bytes;
- configured texture budget;
- residency queue depth;
- active texture pin count;
- stale-request cancellations;
- readable source-failure bytes;
- unknown-size request/source failure counts.

Transform QA provides direct move, rotate, scale, and reset controls using the selected real object and the normal Action API.

`Object Use` reports the selected object's interaction prompt and range. It is separate from transform and gizmo state.

## Viewport shading

Scene View owns explicit shading modes:

- Wireframe;
- Solid;
- Material Preview;
- Rendered.

### Wireframe

Wireframe draws neutral geometry edges without texture sampling.

### Solid

Solid draws neutral filled surfaces.

### Material Preview

Material Preview evaluates supported metallic-roughness materials under stable editor lighting and the shared linear HDR-to-display presentation.

### Rendered

Rendered uses scene lighting and the active environment and includes:

- transactional IBL from supported HDR environments;
- bounded local reflection-probe capture;
- ordinary and box-projection local probe sampling;
- two fitted directional shadow cascades;
- one bounded spot shadow map for the first enabled spot;
- one bounded 256² point-shadow cubemap for the first enabled point light;
- exposure;
- bloom;
- bounded depth-neighborhood ambient occlusion;
- camera/object motion history reprojection;
- current-depth consistency rejection;
- ACES-fitted final tone mapping.

One changed local reflection probe is captured at a time using deterministic six-face views. Probe sampling is disabled during capture. Shared IBL remains available when local capture is unavailable.

Enabled probe volumes can be visualized as a non-scene editor overlay. Box-projection probes receive distinct presentation.

### Transparency

Blended materials render after opaque and masked geometry through a bounded 4,096-item back-to-front queue.

Overflow uses deterministic entity order.

The current transparency model is sorted straight-alpha blending. Order-independent transparency is not implemented.

### AO and temporal state

The AO term is a bounded four-direction, two-sided, multi-step view-space horizon search with:

- radius;
- thickness;
- falloff;
- bias;
- intensity;
- depth-agreement edge confidence.

Temporal AO history, multi-frame denoise, and production GTAO validation remain unfinished.

Rendered temporal presentation includes:

- camera/object reprojection;
- depth-neighborhood rejection;
- reactive handling for transparency, transmission, and emissive pixels;
- history clamping;
- bounded sharpening;
- fallback/invalidation diagnostics.

Production TAA validation across cuts, resize, disocclusion, and moving-object cases remains unfinished.

HDR target size, generation, completeness, shadow resolution, and resize failure are exposed through engine diagnostics.

Unlit materials bypass lighting. Reserved procedural materials remain unavailable until implemented. Helper overlays keep their own materials. Mode changes preserve scene materials. The renderer restores a filled polygon baseline before UI and detached-window presentation.

### Legacy wireframe API

Enabling the legacy wireframe API selects Wireframe. Disabling it restores the last valid non-Wireframe mode.

The Sandbox stores the authoritative mode under `ui.scene_view.shading_mode`. `wireframe_enabled` migrates only when the new key is absent.

### Material inspection

Standard Object Details and Object Info show the selected effective material description.

Object Info also reports borrowed semantic texture dependency count.

Imported scene entities can use bounded persistent material instances with:

- identity-routed Reimport;
- dependency inspection;
- revision refresh;
- per-override reset;
- reset-all.

Text-entry import, drag/drop, material-file authoring, and a dedicated dependency-graph panel remain future work.

## Stress and validation modes

### Terrain stream stress

Run:

```text
henka_sandbox3d.exe --terrain-stream-stress
```

The check:

1. seeds or reuses four procedural regions under local `terrain-sandbox-v2`;
2. verifies a bounded one-region CPU/physics/render window around the active camera;
3. moves from `(0,0)` to `(2,0)` and `(2,2)`;
4. returns to the starting region;
5. verifies rendered return on both axes;
6. verifies bounded collision-patch overlap on return;
7. reports request failures and resident-region capacity.

The active camera region receives priority for bounded collision coverage.

This is a runtime foundation check. Residency-wide collision coverage, broad-world streaming, and human visual approval remain outside its current scope.

### Authoring checks

Useful manual authoring checks include:

- select multiple Textured Cube faces and verify authored IDs track visible topology;
- run Extrude / Inset / Bevel / Subdivide / UV operations and verify source, render, bounds, and history remain connected;
- Save Source, edit, Reload Source, and verify transactional replacement;
- Make Editable on Giraffe or Rocket and verify generic component editing;
- hide an object and verify transform lock;
- show it again and verify unlock remains explicit;
- use Physics QA to verify collider bounds after authoring changes;
- exercise Own Material, Undo Mat, Redo Mat, Save Project, and Reload Project.

### Workspace checks

Useful manual workspace checks include:

- `F4` panel visibility;
- `F5` Standard / Focus Viewport;
- dock/undock;
- side-dock stacking;
- detached move/resize;
- close-to-redock;
- title-bar drag-back;
- splitter resize;
- Save/Restore Custom;
- Studio/Assembly slots;
- layout Undo/Redo;
- tab cycling and reordering;
- Reset Layout recovery.

## Packaged runs

Packaged Windows output includes this help file beside the executable.

Packaged runs also include:

- `user/sandbox3d.settings` for local Sandbox settings;
- `PACKAGE_INFO.txt` for package identity/provenance;
- runtime Development/Packaged mode reporting.

The packaged folder is intended to remain usable after being copied away from the repository root.

## Current limitations

The current Sandbox remains an early engine/editor workspace.

### Asset and model limits

- Built-in meshes, bounded OBJ, and bounded glTF paths are available.
- OBJ supports positive/negative indices and triangle, quad, and bounded n-gon fan triangulation.
- glTF supports the documented bounded geometry, node hierarchy, and shared PBR material path.
- OBJ material libraries remain unsupported.
- Concave-polygon correction beyond basic fan triangulation remains unsupported.
- Skeletal animation, skinning, and morph targets remain unavailable.
- Editor hierarchy authoring remains future work.

### Save and project limits

- Local settings use a small key/value format.
- Separate save-data foundations cover scene ID, camera pose, and simple flags.
- Complete shipped-game save systems remain future work.
- Complete scene/project authoring remains future work.

### UI and 2.5D limits

- The current UI is a bounded Sandbox/editor control layer.
- Complete runtime Game UI remains future work.
- 2.5D currently provides camera foundations.
- Sprites, texture regions, layers, parallax, animation, and movement-plane constraints remain future work.

### Workspace and transform limits

- Production panels can detach into native windows.
- Scene View is not detachable yet.
- Current viewport gizmo scope covers world-axis move, rotate, and uniform scale.
- Per-axis scale handles remain unavailable.
- Numeric transform editing remains future work.
- Manual desktop QA remains required for gizmo feel, hover clarity, orbit/pan feel, detached-window interaction, and general viewport ergonomics.

### Physics limits

- Physics V1 uses sphere, axis-aligned box, and plane colliders.
- Advanced collider/solver features remain future work.
- Manual desktop QA remains required for collision feel and debug-overlay readability.

### Console

The packaged Sandbox currently opens a console window. In-window utilities and status provide the primary viewer workflow. Console output remains available for logs, warnings, and automated checks.

More UI detail is available in [ui.md](../ui.md).

For step-by-step manual verification, use [qa/sandbox3d-manual-checklist.md](../qa/sandbox3d-manual-checklist.md).
