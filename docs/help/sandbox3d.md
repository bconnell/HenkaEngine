# Sandbox 3D Help

`henka_sandbox3d` is the current visible Henka Engine example. It opens a small 3D scene with:

- a textured cube
- a textured ground plane
- a colored cube
- a loaded OBJ marker
- a debug grid
- a fallback-texture example that stays visible when a texture file is missing
- a fallback-model example that stays visible when an OBJ file is missing

The sandbox also saves a small local settings file so wireframe, grid visibility, mouse sensitivity, camera state, selected object, and panel visibility can carry across runs.
It now also includes small in-window developer panels for inspection and settings tasks.
On a first packaged run with no existing settings file, the docked workspace opens in `View` mode so the controls are visible while most of the scene stays open.

## Controls

- `W A S D`: move across the scene
- `Q / E`: move down / up
- `Shift`: move faster
- `Mouse`: look around while mouse capture is active
- `Right Mouse / Tab`: toggle mouse capture
- `Left Mouse`: select or manipulate inside the scene viewport when mouse capture is released
- `Alt + Left Mouse`: orbit around the selected object or current view target
- `Middle Mouse`: pan the viewport
- `Mouse Wheel`: zoom the viewport when the cursor is over the scene view
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

## Scene legend

- `Textured Cube`: centered, shows texture material rendering.
- `Ground`: under the scene, shows repeated local texture use.
- `Colored Cube`: left side, shows untextured material color.
- `OBJ Marker`: farther left, shows the current OBJ loading path.
- `Missing Texture`: right side, shows the error texture fallback.
- `Missing Model`: farther right, shows the fallback mesh.
- `Debug Grid`: spans the floor so you can judge position, depth, and movement.

## What to try

- Walk around the cube and the grid.
- Walk around the OBJ marker to confirm model loading is active.
- Toggle wireframe to inspect the scene layout.
- Toggle mouse capture and use the mouse to look around.
- Find the fallback-texture example to confirm that missing textures fail visibly without stopping the engine.
- Find the fallback-model example to confirm that missing OBJ assets fail visibly without stopping the engine.
- Compare the colored cube, textured cube, and OBJ marker so it is easy to tell which material path each object is using.
- Use `F3` to hide the grid briefly, then show it again to confirm the scene layout still reads clearly.
- Press `F4` to open the sandbox panels, then use `F5` to compare the View, Inspect, and Full Tools layouts.
- Release mouse capture, then left-click a visible object in the viewport to test selection and transform dragging.
- Use `Alt + Left Mouse`, `Middle Mouse`, `Mouse Wheel`, `F`, and `Home` to test orbit, pan, zoom, frame selected, and reset view.
- Switch the Transform section between Select, Move, Rotate, and Scale, then drag the gizmo on a selected object.
- Toggle snapping on and off to compare free movement with stepped adjustments.
- Click the grid and wireframe controls to confirm the in-window UI updates the same engine state as the keyboard shortcuts.
- Open Help, Scene Legend, Paths, Settings, and Diagnostics in the Utility panel so you can inspect the sandbox without relying on the console.
- Confirm the small in-window status area reports common actions such as layout changes, camera reset, saved settings, or object focus.
- Select each scene object and confirm the Object Details panel updates.
- Use Focus Camera, Reset Transform, and Print Object Info on a few different objects.
- Use the controls panel to reset the camera, save settings, and reset sandbox settings.

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
- Transform tabs for `Select`, `Move`, `Rotate`, and `Scale`
- a snap toggle with current move, rotate, and scale snap values
- a save-settings button
- a reset-layout button
- panel visibility toggles for the object-inspection panels in the heavier layouts
- utility tabs for Help, Scene Legend, Paths, Settings, Diagnostics, and Object Info
- a small in-window status area for recent actions and warnings

`Inspect` and `Full Tools` also expose the wider inspection controls.
`Full Tools` keeps the most detailed inspection workspace visible.
Transform manipulation happens in the dedicated scene viewport, not inside the docked panels.

The `Scene Objects` panel lists the current sandbox examples by name.

- Clicking a row selects that object.
- Hidden objects stay listed and show a hidden state tag.
- The selected row stays highlighted so the current object is easy to track.
- If the dock is too short to show the whole list at once, page buttons keep every sample object reachable.

The `Object Details` panel shows the current selection.

- name
- tag when available
- visible state
- position
- scale
- what the object demonstrates
- mesh, material, texture or fallback summary, and interaction availability
- safe actions for visibility, camera focus, transform reset, and console info output

The selected object also shows a visible transform gizmo in the scene viewport.

- `Select` mode keeps normal object picking active.
- `Move` mode drags the selected object on the chosen world axis.
- `Rotate` mode drags the selected object around the chosen world axis.
- `Scale` mode uses the center square for uniform scale in the current sandbox pass.
- Snap can be enabled or disabled from the Controls panel.
- Gizmo dragging uses viewport-relative framebuffer coordinates and the same projected handle model that the overlay draws, so the visible handles stay aligned with the mouse inside the dedicated scene viewport.
- The current sandbox path now also shares a local validated action-command layer and deterministic gizmo interaction helpers with the test suite, which reduces manual QA for basic object-selection and transform-mutation outcomes.
- The gizmo helper pieces are internal scene tools. They remain hidden from the normal runtime path, do not become the selected object, do not appear in Scene Objects, and are ignored by normal scene picking.
- If the selected object becomes hidden, invalid, or the viewport changes during a drag, the drag stops safely and the selected real object remains the source of truth.

The `Utility` panel provides short in-window views for:

- Help
- Scene Legend
- Object Info
- Paths
- Settings
- Diagnostics

Those utilities are the preferred path for normal viewer use. The console remains useful for fallback logs, warnings, and automated checks.
Status messages also appear in-window for common actions so normal packaged use does not depend on the console.

When the UI is open:

- mouse capture is released
- mouse look pauses
- camera movement pauses
- `Right Mouse` and `Tab` can be used again after you close the panel
- `Escape` closes the panel before it returns to the normal mouse-capture and exit flow

Picking and gizmo hit testing both use the dedicated scene viewport. Clicks in docked panels do not count as viewport picks or transform drags.
Mouse wheel input over paged panels stays with those panels instead of zooming the scene.

## Packaged runs

Packaged Windows builds include `docs/help/sandbox3d.md` beside the executable so the same offline help stays available after you copy the runnable folder elsewhere.
Packaged runs also save sandbox settings in `user/sandbox3d.settings` beside the executable.
The packaged folder also includes `PACKAGE_INFO.txt` so you can confirm the package was refreshed after a new build.
The runtime also reports whether it is running in `Development` or `Packaged` mode.

## Current limitations

- The sandbox uses built-in meshes plus a small early OBJ loading path.
- OBJ support is intentionally limited to simple geometry and does not include imported materials, negative indices, or animation.
- The current settings file is a small local key/value format. It is easy to inspect by hand, but it is not a finished save-game system.
- A separate save-data foundation now exists for scene id, camera pose, and simple flags, but the sandbox still uses settings for its normal viewer state.
- The UI overlay is intentionally small. It is meant for sandbox control and object inspection, not as a full editor or a complete runtime UI system.
- The transform gizmo is intentionally scoped to world-axis move, rotate, and scale for the current sandbox object model. Undo, numeric editing, floating panels, and resizable docks are separate future work.
- Scale is currently uniform-only in the viewport gizmo path. Per-axis scale handles are intentionally not shown until they are reliable enough to ship.
- Manual desktop QA is still the best way to judge gizmo handle feel, hover clarity, and transform drag comfort.
- The packaged sandbox still opens a console window at this stage. In-window utilities and status are the preferred viewer workflow, while the console remains available for fallback logs.
- Editor tools, asset browser UI, and broader 2D or 2.5D workflows are not available yet.

More detail about the current UI layer is available in [docs/ui.md](../ui.md).

For a step-by-step manual verification flow, use [docs/qa/sandbox3d-manual-checklist.md](../qa/sandbox3d-manual-checklist.md).
