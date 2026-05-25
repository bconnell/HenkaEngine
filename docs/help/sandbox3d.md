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
On a first packaged run with no existing settings file, the UI opens in `View` mode so the controls are visible while most of the scene stays open.

## Controls

- `W A S D`: move across the scene
- `Q / E`: move down / up
- `Shift`: move faster
- `Mouse`: look around while mouse capture is active
- `Right Mouse / Tab`: toggle mouse capture
- `F1`: toggle wireframe
- `F2`: print the scene legend to the console again
- `F3`: show or hide the debug grid
- `F4`: show or hide the sandbox panels
- `F5`: cycle View, Inspect, and Full Tools layouts
- `H`: print controls and the scene legend to the console again
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
- Click the grid and wireframe controls to confirm the in-window UI updates the same engine state as the keyboard shortcuts.
- Select each scene object and confirm the Object Details panel updates.
- Use Focus Camera, Reset Transform, and Print Object Info on a few different objects.
- Use the controls panel to reset the camera, save settings, and reset sandbox settings.

## Sandbox panels

Press `F4` to open the in-window sandbox panels. Press `F5` to cycle between:

- `View`: keeps most of the scene visible and shows a compact controls panel
- `Inspect`: keeps the object panels available while leaving the center view clearer
- `Full Tools`: shows the heavier inspection layout

If the panels do not appear when you expect them to, refresh the packaged sandbox with `.\scripts\package_sandbox3d_windows.ps1`, confirm `out/HenkaSandbox3D/PACKAGE_INFO.txt` was refreshed, and try again.

The `Controls` panel currently includes:

- layout buttons for `View`, `Inspect`, and `Full Tools`
- a debug-grid toggle
- a wireframe toggle
- a camera reset button
- a save-settings button
- a reset-layout button
- panel visibility toggles for the object-inspection panels

`Inspect` and `Full Tools` also expose the wider inspection controls.

`Full Tools` keeps the most status text, adjustment controls, and path details visible.

The `Scene Objects` panel lists the current sandbox examples by name.

- Clicking a row selects that object.
- Hidden objects stay listed and show a hidden state tag.

The `Object Details` panel shows the current selection.

- name
- visible state
- position
- rotation
- scale
- what the object demonstrates
- mesh, material, and texture or fallback summary
- safe actions for visibility, camera focus, transform reset, and console info output

When the UI is open:

- mouse capture is released
- mouse look pauses
- camera movement pauses
- `Right Mouse` and `Tab` can be used again after you close the panel
- `Escape` closes the panel before it returns to the normal mouse-capture and exit flow

## Packaged runs

Packaged Windows builds include `docs/help/sandbox3d.md` beside the executable so the same offline help stays available after you copy the runnable folder elsewhere.
Packaged runs also save sandbox settings in `user/sandbox3d.settings` beside the executable.
The packaged folder also includes `PACKAGE_INFO.txt` so you can confirm the package was refreshed after a new build.

## Current limitations

- The sandbox uses built-in meshes plus a small early OBJ loading path.
- OBJ support is intentionally limited to simple geometry and does not include imported materials, negative indices, or animation.
- The current settings file is a small local key/value format. It is easy to inspect by hand, but it is not a finished save-game system.
- The UI overlay is intentionally small. It is meant for sandbox control and object inspection, not as a full editor or a complete runtime UI system.
- Editor tools, asset browser UI, and broader 2D or 2.5D workflows are not available yet.

More detail about the current UI layer is available in [docs/ui.md](../ui.md).

For a step-by-step manual verification flow, use [docs/qa/sandbox3d-manual-checklist.md](../qa/sandbox3d-manual-checklist.md).
