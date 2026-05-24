# Sandbox 3D Help

`henka_sandbox3d` is the current visible Henka Engine example. It opens a small 3D scene with:

- a textured cube
- a textured ground plane
- a colored cube
- a debug grid
- a fallback-texture example that stays visible when a texture file is missing

## Controls

- `W A S D`: move across the scene
- `Q / E`: move down / up
- `Shift`: move faster
- `Mouse`: look around while mouse capture is active
- `Right Mouse / Tab`: toggle mouse capture
- `F1`: toggle wireframe
- `H`: print help to the console again
- `Escape`: release the mouse first, then exit
- Window close: exit

## What to try

- Walk around the cube and the grid.
- Toggle wireframe to inspect the scene layout.
- Toggle mouse capture and use the mouse to look around.
- Find the fallback-texture example to confirm that missing textures fail visibly without stopping the engine.

## Current limitations

- The sandbox uses built-in meshes and local shader and texture assets only.
- There is no model loading yet.
- Help is printed to the console because in-window text and UI rendering do not exist yet.
- Editor tools, asset browser UI, and broader 2D or 2.5D workflows are not available yet.
