# Sandbox 3D Help

`henka_sandbox3d` is the current visible Henka Engine example. It opens a small 3D scene with:

- a textured cube
- a textured ground plane
- a colored cube
- a loaded OBJ marker
- a debug grid
- a fallback-texture example that stays visible when a texture file is missing
- a fallback-model example that stays visible when an OBJ file is missing

The sandbox also saves a small local settings file so wireframe, grid visibility, mouse sensitivity, and camera state can carry across runs.
It now also includes a small in-window control panel for the most useful inspection and settings tasks.

## Controls

- `W A S D`: move across the scene
- `Q / E`: move down / up
- `Shift`: move faster
- `Mouse`: look around while mouse capture is active
- `Right Mouse / Tab`: toggle mouse capture
- `F1`: toggle wireframe
- `F2`: print the scene legend to the console again
- `F3`: show or hide the debug grid
- `F4`: show or hide the sandbox panel
- `H`: print controls and the scene legend to the console again
- `Escape`: release the mouse first, then exit
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
- Press `F4` to open the sandbox panel, then click the grid and wireframe controls to confirm the in-window UI updates the same engine state as the keyboard shortcuts.
- Use the panel to reset the camera, save settings, and reset sandbox settings.

## Sandbox panel

Press `F4` to open the in-window sandbox panel.

The panel currently includes:

- a debug-grid toggle
- a wireframe toggle
- a camera reset button
- a save-settings button
- a reset-settings button
- help and scene-legend buttons
- short status text for mouse capture, wireframe, grid state, frame timing, camera position, asset path, user-data path, and settings path

When the panel is open:

- mouse capture is released
- mouse look pauses
- `Right Mouse` and `Tab` can be used again after you close the panel

## Packaged runs

Packaged Windows builds include `docs/help/sandbox3d.md` beside the executable so the same offline help stays available after you copy the runnable folder elsewhere.
Packaged runs also save sandbox settings in `user/sandbox3d.settings` beside the executable.

## Current limitations

- The sandbox uses built-in meshes plus a small early OBJ loading path.
- OBJ support is intentionally limited to simple geometry and does not include imported materials, negative indices, or animation.
- The current settings file is a small local key/value format. It is easy to inspect by hand, but it is not a finished save-game system.
- The UI overlay is intentionally small. It is meant for sandbox control and inspection, not as a full editor or a complete runtime UI system.
- Editor tools, asset browser UI, and broader 2D or 2.5D workflows are not available yet.

More detail about the current UI layer is available in [docs/ui.md](../ui.md).

For a step-by-step manual verification flow, use [docs/qa/sandbox3d-manual-checklist.md](../qa/sandbox3d-manual-checklist.md).
