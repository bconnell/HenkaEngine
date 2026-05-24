# Sandbox 3D Help

`henka_sandbox3d` is the current visible Henka Engine example. It opens a small 3D scene with:

- a textured cube
- a textured ground plane
- a colored cube
- a loaded OBJ marker
- a debug grid
- a fallback-texture example that stays visible when a texture file is missing
- a fallback-model example that stays visible when an OBJ file is missing

## Controls

- `W A S D`: move across the scene
- `Q / E`: move down / up
- `Shift`: move faster
- `Mouse`: look around while mouse capture is active
- `Right Mouse / Tab`: toggle mouse capture
- `F1`: toggle wireframe
- `F2`: print the scene legend to the console again
- `F3`: show or hide the debug grid
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

## Packaged runs

Packaged Windows builds include `docs/help/sandbox3d.md` beside the executable so the same offline help stays available after you copy the runnable folder elsewhere.

## Current limitations

- The sandbox uses built-in meshes plus a small early OBJ loading path.
- OBJ support is intentionally limited to simple geometry and does not include imported materials, negative indices, or animation.
- Help is printed to the console because in-window text and UI rendering do not exist yet.
- Editor tools, asset browser UI, and broader 2D or 2.5D workflows are not available yet.

For a step-by-step manual verification flow, use [docs/qa/sandbox3d-manual-checklist.md](../qa/sandbox3d-manual-checklist.md).
