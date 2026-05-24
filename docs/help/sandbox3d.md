# Sandbox 3D Help

`henka_sandbox3d` is the current visible Henka Engine example. It opens a 3D scene with a ground plane, a cube, and a debug grid rendered through Henka systems.

## Controls

- `W A S D`: move the camera on the ground plane
- `Q / E`: move down / up
- `Shift`: move faster
- `F1`: toggle wireframe
- `H`: print help to the console again
- `Escape`: exit
- Window close: exit

## Current limitations

- Movement is keyboard-only right now.
- Mouse look is not implemented yet.
- Help is printed to the console because in-window text and UI rendering do not exist yet.
- The sandbox uses built-in primitives and local shader assets only.

## Planned improvements

- mouse look when input support is ready
- in-window help overlay after text and UI rendering exist
- richer scene content
- broader material and asset workflows
