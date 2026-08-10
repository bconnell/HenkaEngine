# Sandbox 3D Help

`henka_sandbox3d` is the current visible Henka Engine example. It opens a small 3D showcase with:

- the original Cheeky Giraffe mascot
- the original realistic rocket sample
- a textured ground plane with a visible floor surface and grid
- a debug grid

The showcase models are deterministic repo-owned glTF assets generated during the Windows build and packaged beside their binary buffers. They use their own material factors rather than the primitive-gallery cube texture. Use `henka_sandbox3d.exe --primitive-gallery` to show the engineering primitives, OBJ marker, fallback samples, foliage, and realism material row again.

The sandbox also saves a small local settings file so wireframe, grid visibility, mouse sensitivity, camera preset, camera pose, orthographic zoom, and panel visibility can carry across runs.
It now also includes small in-window developer panels for inspection and settings tasks.
The docked workspace opens in `View` mode with no selected scene object so the controls are visible while most of the scene stays open.

For a bounded non-interactive streaming check, run
`henka_sandbox3d.exe --terrain-stream-stress`. It seeds or reuses four
procedural regions in the local `terrain-sandbox-v2` data root, verifies the
active camera requests a bounded one-region CPU/physics/render window, then
crosses from `(0,0)` to generated regions `(2,0)` and `(2,2)` before returning.
It verifies rendered return on both axes plus a bounded collision-patch overlap
across the return, and reports request failures and resident-region capacity.
The active camera region is prioritized for bounded collision coverage. This is
a small runtime foundation check, not residency-wide collision
coverage, broad-world streaming, or human visual approval.

## Controls

- `W A S D`: move across the scene
- `Q / E`: move down / up
- `Shift`: move faster
- `Mouse`: look around while mouse capture is active
- `Right Mouse / Tab`: toggle mouse capture
- `Left Mouse`: uses the active viewport tool when mouse capture is released
- `Alt + Left Mouse`: optional orbit shortcut around the selected object or current view target
- `Middle Mouse`: optional pan shortcut
- `Mouse Wheel`: dolly the Perspective 3D view or change orthographic height in Side, Top-down, and Isometric views when the cursor is over Scene View
- `F1`: toggle wireframe
- `F2`: print the scene legend to the console again
- `F3`: show or hide the debug grid
- `F4`: show or hide the sandbox panels
- `F5`: cycle View, Inspect, and Full Tools layouts
- `F`: frame the selected object
- `H`: print controls and the scene legend to the console again
- `Home`: reset the camera view
- `Escape`: close the sandbox UI first, then release the mouse, then exit
- Window close: exit

In `--primitive-gallery`, selecting `Textured Cube` exposes the shared
authoring source in Object Details. The Authoring section shows the selected
topology face and provides bounded `Extrude`, `Inset`, `Undo`, `Redo`, `Save
Source`, and `Reload Source` actions. Clicking the cube in the viewport resolves
the hit against the authoring polygons and updates the real face identity used
by the modeling commands. Save and reload use the sandbox's confined user-data
authoring slot; the evaluated mesh and scene bounds are replaced only after the
candidate succeeds. Vertex/edge modes and general mesh-file open/save remain
unfinished.

The Authoring section also reports the evaluated render-mesh material-region
range after each successful edit, undo, redo, save, or reload. This is metadata
continuity diagnostics; region values do not yet select multiple renderer
material instances.

## Scene legend

- `Showcase Giraffe`: the Cheeky Giraffe mascot loaded through the packaged glTF scene/material path.
- `Showcase Rocket`: the Original Realistic Rocket loaded through the packaged glTF scene/material path.
- `Ground`: under the showcase, shows repeated local texture use.
- `Debug Grid`: spans the floor so you can judge position, depth, and movement.

The engineering sample legend is available with `--primitive-gallery`:

- `Textured Cube`, `Material Ball`, `glTF Marker`, `Missing Texture`, and `Missing Model` retain their diagnostic roles.

## What to try

- Walk around the giraffe, rocket, and grid to confirm the normal showcase is active.
- Run `--primitive-gallery` and walk around the OBJ marker to confirm the diagnostic model path is active.
- Toggle wireframe to inspect the scene layout.
- Toggle mouse capture and use the mouse to look around.
- In `--primitive-gallery`, find the fallback-texture example to confirm that missing textures fail visibly without stopping the engine.
- In `--primitive-gallery`, find the fallback-model example to confirm that missing OBJ assets fail visibly without stopping the engine.
- In `--primitive-gallery`, compare the material ball, textured cube, and OBJ marker so it is easy to tell which material path each object is using.
- Use `F3` to hide the grid briefly, then show it again to confirm the scene layout still reads clearly.
- Press `F4` to open the sandbox panels, then use `F5` to compare the View, Inspect, and Full Tools layouts.
- Release mouse capture, then use the Viewport Tool buttons to switch between Select, Orbit, Pan, Move, Rotate, and Scale.
- Open `Camera/Status` and compare Perspective 3D, Side 2.5D, Top-down 2.5D, and Isometric 2.5D. Use `Orbit` and `Pan` with left drag, plus `Mouse Wheel`, `F`, and `Home`, to test customization, pan, projection-aware zoom, frame selected, and preset reset.
- Use the optional `Alt + Left Mouse` orbit and `Middle Mouse` pan shortcuts if you want to compare them with the explicit tool modes.
- Switch the Viewport Tool section between Select, Move, Rotate, and Scale, then drag the gizmo on a selected object.
- In `--primitive-gallery`, select `Textured Cube`, expand Object Details > Authoring, and use Extrude, Inset, Bevel, Subdivide, Project UV, Pack UV, Undo, and Redo to verify the source topology, scene mesh, bounds, and renderer stay connected.
- Click different visible faces of `Textured Cube` to verify the selected face identity follows the authoring topology before using a modeling command.
- Use Save Source, make another edit, then use Reload Source to verify the saved editable topology replaces the scene render and bounds transactionally; an invalid or missing source retains the prior render.
- With the optional Physics QA body enabled, use Extrude and Undo and inspect Diagnostics/Physics QA to verify the linked Textured Cube box collider follows the evaluated bounds.
- Toggle snapping on and off to compare free movement with stepped adjustments.
- Click the grid and wireframe controls to confirm the in-window UI updates the same engine state as the keyboard shortcuts.
- Open Help, Scene Legend, Paths, Settings, Diagnostics, and Transform QA in the Utility panel so you can inspect the sandbox without relying on the console.
- Choose `Open Native Panel Test` in Controls to open a separate OS-level window that shows its identifier, focus, size, last routed event, and close guidance.
- The in-window panels open on startup and after reset-style launches. `F4` hides or shows them; it is not required for first discovery.
- Starts have no selected scene object; Object Details, Physics QA, Diagnostics, and the compact strip report no selection until you choose one.
- Watch the compact strip below Scene View while testing; it reports tool, selection, selected-highlight state, pointer ownership, gizmo, hover, drag, and rejection state live.
- Use Transform QA first to confirm whether selected-object mutation works even if gizmo dragging or viewport input is failing.
- Open `Physics QA`. `Enable` starts the arranged multi-body demonstration. `Make Dynamic + Drop` instead activates only the selected supported body at its current transform, leaving unrelated samples still.
- `DRAG` marks a live panel header. Drag a docked panel header and release over a valid left or right outline to dock there.
- If a side dock already contains a panel, the incoming panel stacks into the same side instead of covering it.
- Release away from the dock outlines to open a separate native tool window. Move or resize that window with the operating-system frame.
- Close a detached tool window to return its panel to its last valid dock. Detached windows show matching controls, and a focused title-bar move into the main-window envelope requests bounded drag-back docking. `Reset Layout` recovers defaults.
- Drag the narrow bars beside Scene View to resize occupied docks.
- Use `Save Custom` and `Restore Custom` in Controls for the primary named workspace. The adjacent Studio and Assembly slot buttons provide two additional bounded local snapshots; restoring any slot redocks detached panels first.
- Press `Tab` or `Shift+Tab` to cycle focus across visible workspace panels; the focused header is marked with a green accent.
- Hover a merged workspace tab for a compact guide: click to activate, drag to reorder within the group, or drop a panel at the center to join tabs.
- Use Controls > Undo Layout and Redo Layout for the bounded workspace layout history; detached panels are redocked before a snapshot is restored.
- With panels visible, `Ctrl+Z` undoes and `Ctrl+Y` or `Ctrl+Shift+Z` redoes the bounded workspace layout history.
- Reset Layout returns to the default topology, redocks detached panels, and preserves valid saved named layout slots.
- Hover a topology divider or dock splitter to see the matching horizontal or vertical system resize cursor; the cursor returns to normal when the viewport or a tool owns the pointer.
- Confirm the small in-window status area reports common actions such as layout changes, camera reset, saved settings, or object focus.
- Select each scene object and confirm the Object Details panel updates.
- Use Focus Camera, Reset Transform, and Print Object Info on a few different objects.
- Use the controls panel to reset the camera, save settings, and reset sandbox settings.
- Select an object and use `M` or `G`, `R`, or `S` to start a move, rotate, or scale transform. Use `X`, `Y`, or `Z` to constrain it, then confirm or cancel.

## Sandbox panels

Press `F4` to open the in-window sandbox panels. Press `F5` to cycle between:

- `View`: keeps the largest dedicated scene viewport and shows compact docked tools
- `Inspect`: keeps the object panels available beside the dedicated viewport
- `Full Tools`: shows the heavier docked workspace while keeping the scene in its own viewport box

If you hide the panels, a small in-window hint stays in the viewport corner so you can still see that `F4` restores panels and `F5` changes layout.
When the panels are visible, the scene stays inside its own dedicated viewport region instead of drawing underneath the docked panels.

If the panels do not appear when you expect them to, refresh the packaged sandbox with `.\scripts\package_sandbox3d_windows.ps1`, confirm `out/HenkaSandbox3D/PACKAGE_INFO.txt` was refreshed, and try again.

The `Controls` panel currently includes:

- layout buttons for `View`, `Inspect`, and `Full Tools`
- a readable `Main` page and `Panels/Status` page
- a `Grid` toggle
- a `Wire` toggle
- visible `Frame Selected`, `Reset View`, `Zoom In`, and `Zoom Out` controls
- a `Camera/Status` page with Perspective 3D, Side 2.5D, Top-down 2.5D, and Isometric 2.5D preset controls
- Viewport Tool tabs for `Select`, `Orbit`, `Pan`, `Move`, `Rotate`, and `Scale`
- a snap toggle with current move, rotate, and scale snap values
- a `Hit Boxes` toggle for the viewport debug overlay
- a save-settings button
- a reset-layout button
- panel visibility toggles for the object-inspection panels in the heavier layouts
- utility tabs for Help, Scene Legend, Paths, Settings, Diagnostics, Transform QA, and Object Info
- a `Physics QA` utility for opt-in rigid-body playback, debug drawing, body inspection, impulses, and raycasts
- direct `Diagnostics`, `Transform QA`, and `Physics QA` buttons on the main page
- an `Open Native Panel Test` button for multi-window foundation checks
- a small in-window status area for recent actions and warnings

`Inspect` and `Full Tools` also expose the wider inspection controls.
`Full Tools` keeps the most detailed inspection workspace visible.
Transform manipulation happens in the dedicated scene viewport, not inside workspace panels.

The `Scene Objects` panel lists the current sandbox examples by name.

- Clicking a row selects that object.
- Hidden objects stay listed and show a hidden state tag.
- The selected row identifies the current object. Editable selected scene objects also show the yellow viewport transform highlight.
- Locked objects, including the default Ground, remain selectable and inspectable but do not show the yellow transform highlight or a transform gizmo.
- After moving another object and clearing selection, selecting Ground again does not inherit the prior transform session. Unlocking Ground requires an explicit Object Details action.
- Selection overlays are clipped to the Scene View so they do not draw over panels or the status strip.
- If the dock is too short to show the whole list at once, page buttons keep every sample object reachable.

The `Object Details` panel shows the current selection.

- name
- tag when available
- visible state
- position
- scale
- what the object demonstrates
- mesh, material, texture or fallback summary, and interaction availability
- full Object Details mode also shows the bounded effective material description for the selected object; definition identity, instance override values, and dependency graphs remain API/documentation workflows; a complete authoring panel is not implemented yet
- safe actions for visibility, camera focus, transform reset, and console info output

The selected object also shows a visible transform gizmo in the scene viewport.

- `Select` mode keeps normal object picking active.
- `Orbit` mode uses left drag to orbit around the current view target.
- `Pan` mode uses left drag to pan the current view target.
- `Move` mode drags the selected object on the chosen world axis.
- `Rotate` mode drags the selected object around the chosen world axis.
- `Scale` mode uses the center square for uniform scale in the current sandbox pass.
- Snap can be enabled or disabled from the Controls panel.
- Gizmo dragging uses viewport-relative framebuffer coordinates and the same projected handle model that the overlay draws, so the visible handles stay aligned with the mouse inside the dedicated scene viewport.
- The current sandbox path now also shares a local validated action-command layer and deterministic gizmo interaction helpers with the test suite, which reduces manual QA for basic object-selection and transform-mutation outcomes.
- The gizmo helper pieces are internal scene tools. They remain hidden from the normal runtime path, do not become the selected object, do not appear in Scene Objects, and are ignored by normal scene picking.
- If the selected object becomes hidden, invalid, or the viewport changes during a drag, the drag stops safely and the selected real object remains the source of truth.

The viewport also supports action-based transform hotkeys. `M` and `G` start move, `R` starts rotate, and `S` starts scale for the selected visible and unlocked object. Locked selections reject transform hotkeys. While active, `X`, `Y`, and `Z` choose an axis, `Enter` or `Left Mouse` confirms, and `Escape` or `Right Mouse` restores the original transform. Changing selection, clearing selection, hiding or locking the target, or changing tools cancels and clears the active transform session. `Left Ctrl` enables stepped adjustment and `Left Shift` enables finer movement. The active profile and bindings appear in Help. Custom profiles are local config entries documented in [editor-controls.md](../editor-controls.md).

The `Utility` panel provides short in-window views for:

- Help
- Scene Legend
- Object Info
- Paths
- Settings
- Diagnostics
- Transform QA
- Physics QA

Those utilities are the preferred path for normal viewer use. The console remains useful for fallback logs, warnings, and automated checks.
Status messages also appear in-window for common actions so normal packaged use does not depend on the console.
Diagnostics now report input ownership, viewport-local cursor state, selected object state, gizmo model validity, overlay primitive count, hovered handle, active drag target, last rejected interaction reason, last Action API result, and the native test-window open/focus/size state.
Transform QA exposes direct move, rotate, scale, and reset controls that use the selected real object and the same Action API path as the normal object workflow.
Physics QA exposes an opt-in fixed-step rigid-body demo with linked real scene objects, pause/resume/step/reset, gravity, body type changes, isolated Make Dynamic + Drop behavior, impulse actions, velocity clearing, camera raycast results, collision/trigger events, and truthful collider/contact overlays. Its supported colliders are sphere, axis-aligned box, and plane. Static bodies do not move from gravity, forces, or impulses; Dynamic bodies fall and respond to physics; Kinematic bodies do not fall from gravity and move only through explicit tool or code movement. Collider/contact debug overlays are separate from the selected-object highlight and are clipped to the Scene View.

Fixed physics substeps commit atomically. Scratch allocation failure and finite arithmetic that exceeds representable valid physics state return distinct results. In either case the prior bodies, contacts, events, pair history, accumulator, and linked scene transforms remain unchanged, with no partial event or scene update, so the caller can correct the condition and retry.
The compact strip below Scene View keeps essential input-gate, gizmo, hovered-panel, panel-header, and workspace drag state visible while testing, so a rejected viewport or panel gesture can be diagnosed without switching views.
`Object Use` in Object Details reports the optional object interaction prompt and range only; it is separate from transform tools and gizmo state.

Workspace panel placement, dock sizes, and bounded named layout slots persist through the local settings file. Production panels can detach into separate OS-level windows with matching controls, safe close-to-redock recovery, bounded saved placement, and title-bar drag-back recognition when a focused detached window enters the main-window envelope; shared side docks stack panels vertically instead of overlapping them. Detachable Scene View remains future work. `Native Panel Test` remains available for focused multi-window verification.

When the UI is open:

- mouse capture is released
- mouse look pauses
- camera movement pauses
- `Right Mouse` and `Tab` can be used again after you close the panel
- `Escape` closes the panel before it returns to the normal mouse-capture and exit flow

Picking and gizmo hit testing both use the dedicated scene viewport. Clicks in docked panels or detached tool windows do not count as viewport picks or transform drags.
Mouse wheel input over paged panels stays with those panels instead of zooming the scene.

## Packaged runs

Packaged Windows builds include `docs/help/sandbox3d.md` beside the executable so the same offline help stays available after you copy the runnable folder elsewhere.
Packaged runs also save sandbox settings in `user/sandbox3d.settings` beside the executable.
The packaged folder also includes `PACKAGE_INFO.txt` so you can confirm the package was refreshed after a new build.
The runtime also reports whether it is running in `Development` or `Packaged` mode.

## Current limitations

- The sandbox uses built-in meshes plus bounded OBJ and glTF loading paths.
- OBJ support includes positive and negative indices plus triangle, quad, and bounded n-gon fan triangulation; glTF adds the documented bounded geometry and shared PBR material path. Model hierarchies, concave-polygon correction beyond basic fan triangulation, and animation are not supported yet.
- The current settings file is a small local key/value format. It is easy to inspect by hand, but it is not a finished save-game system.
- A separate save-data foundation now exists for scene id, camera pose, and simple flags, but the sandbox still uses settings for its normal viewer state.
- The UI overlay is intentionally small. It is meant for sandbox control and object inspection, not as a full editor or a complete runtime UI system.
- The current 2.5D support is a camera foundation. Sprites, texture regions, layers, parallax, animation, and movement-plane constraints remain future work. Blended materials use a bounded back-to-front transparent queue after opaque and masked geometry; if a scene exceeds the 4,096-item queue, the renderer keeps deterministic entity-order blending as a safe fallback. This is sorted straight-alpha blending, not order-independent transparency.
- Docked workspace panels can detach into separate native windows, move independently, and return to their last valid dock when closed. Detachable Scene View support remains future workspace work.
- The transform gizmo is intentionally scoped to world-axis move, rotate, and scale for the current sandbox object model. Undo, numeric editing, saved workspace arrangements, scene authoring, and broader tool surfaces are separate future work.
- Scale is currently uniform-only in the viewport gizmo path. Per-axis scale handles are intentionally not shown until they are reliable enough to ship.
- Manual desktop QA is still the best way to judge gizmo handle feel, hover clarity, and transform drag comfort.
- Manual desktop QA is also still the best way to judge whether Orbit and Pan feel reliable in a packaged run.
- Rigid-body physics v1 is limited to primitive sphere, axis-aligned box, and plane colliders; advanced collider and solver features remain future work, and manual desktop QA is still required for collision feel and debug overlay readability.
- The packaged sandbox still opens a console window at this stage. In-window utilities and status are the preferred viewer workflow, while the console remains available for fallback logs.
- Editor tools, asset browser UI, and broader 2D or 2.5D workflows are not available yet.

More detail about the current UI layer is available in [docs/ui.md](../ui.md).

For a step-by-step manual verification flow, use [docs/qa/sandbox3d-manual-checklist.md](../qa/sandbox3d-manual-checklist.md).
## Viewport shading

The Scene View owns an explicit shading mode. It does not rely on a global polygon toggle. Wireframe draws neutral geometry edges without texture sampling. Solid draws neutral filled surfaces. Material Preview evaluates the supported metallic-roughness material under stable editor lighting and the same linear HDR-to-display presentation used by Rendered. Both HDR modes show validated scene-owned environment controls, while Rendered derives transactional IBL resources from that environment. Rendered also captures one changed local reflection-probe cubemap at a time using deterministic six-face views; probe sampling is disabled during capture to prevent recursion, and the shared IBL remains the fallback when a capture is unavailable. Enabled probe volumes can be shown in the sandbox editor as a non-scene overlay, with box-projection probes distinguished from ordinary influence volumes. Rendered draws the scene into a Scene View-sized linear HDR target, uses two fitted directional shadow cascades, a bounded map for the first enabled spot light, and a bounded 256² cubemap for the first enabled point light, then applies exposure, bloom, bounded depth-neighborhood ambient occlusion, bounded camera- and object-motion history reprojection with a current-depth consistency rejection, and an ACES-fitted final tone map. The AO term is an approximation without normal-aware GTAO or AO history; full disocclusion handling and production TAA remain unfinished. HDR target size, generation, completeness, shadow resolution, and resize failure are exposed in engine diagnostics. Unlit materials bypass lighting; reserved procedural materials are rejected until implemented. Blended materials render after opaque and masked geometry through a bounded 4,096-item back-to-front queue, with deterministic entity-order fallback beyond that bound; this is sorted straight-alpha blending and does not claim order-independent transparency. Helper overlays retain their own materials, mode changes do not rewrite scene materials, and the renderer restores a filled polygon baseline before UI and detached-window presentation.

The legacy wireframe API remains compatible: enabling it selects Wireframe, while disabling it restores the last valid non-wireframe mode. The sandbox persists the authoritative mode under `ui.scene_view.shading_mode`; older `wireframe_enabled` settings are migrated only when the new key is absent.

The current AO term is a bounded four-direction, two-sided, multi-step view-space horizon-search approximation with safe radius, thickness, falloff, bias, and intensity controls; temporal AO history, denoise filtering, and production GTAO validation remain unfinished. Rendered temporal presentation now includes camera/object reprojection, depth-neighborhood rejection, reactive handling for transparency/transmission/emissive pixels, history clamping, bounded sharpening, and fallback/invalidation diagnostics. Production TAA visual validation across camera cuts, resize, disocclusion, and moving-object cases remains unfinished.

The Utility Diagnostics panel includes the active shading/exposure row plus current texture-residency bytes, configured budget, queue depth, active pin count, stale-request cancellations, readable source-failure bytes, and unknown-size request/source failure counts from the engine diagnostics snapshot. Full Tools Object Details and the Object Info utility show the selected effective material description; Object Info also reports the count of borrowed semantic texture dependencies. Object Details creates bounded persistent instances for selected imported scene entities, with identity-routed Reimport, dependency inspection, revision refresh, and per-override or all-override reset through the shared typed C asset path. Text-entry import, drag/drop, material-file authoring, and a dedicated dependency-graph panel remain unimplemented.

The Utility Terrain tab reports bounded resident/render/collision statistics and
provides raise, lower, flatten, smooth, and paint buttons. Brush radius,
strength, material layer, and falloff controls feed the same deterministic
Terrain command API used by runtime edits. Height edits use the bounded
collision-runtime queue for the full edit footprint plus neighbor coverage;
paint-only edits do not rebuild collision. The automated smoke path uses a fixed
repeatable sample center; normal editor use can place the brush from viewport
Terrain ray-picking. Saved brush state is available, while complete
material-layer preview is not yet claimed.
