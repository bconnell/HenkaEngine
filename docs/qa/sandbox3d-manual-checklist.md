# Sandbox 3D Manual Checklist

Use this checklist when you want to confirm that `henka_sandbox3d` is visually readable and that its basic controls still behave as expected.

## Before you start

- Build the project from the repository root.
- Make sure the sandbox executable exists.
- Run the sandbox from the repo helper script so the local assets resolve correctly.

## Launch

```powershell
.\scripts\run_sandbox3d.ps1
```

## Packaged executable test

1. Run:

   ```powershell
   .\scripts\package_sandbox3d_windows.ps1
   ```

2. Open `out/HenkaSandbox3D` in Explorer.
3. Double-click `HenkaSandbox3D.exe`.
4. Confirm the window opens and the scene loads from the packaged `assets/` folder.
5. Confirm the console help still prints and `docs/help/sandbox3d.md` is present beside the executable.
6. Confirm there are no unexpected shader, texture, model, or asset path errors.
7. Confirm the missing-texture and missing-model examples still use the intended fallback visuals.

## Expected startup behavior

- A window opens with the title `Henka Engine Sandbox 3D`.
- Console help prints once at startup.
- The console legend explains what each example object represents.
- The sandbox keeps running until you close it or press `Escape` while mouse capture is released.

## Expected scene examples

You should be able to identify these examples:

- `Textured Cube`: centered in the scene and rotating.
- `Ground`: textured plane under the scene.
- `Colored Cube`: left side of the scene.
- `OBJ Marker`: farther left, using the current OBJ loading path.
- `Missing Texture`: right side, using the visible error-texture fallback.
- `Missing Model`: farther right, using the fallback mesh for a missing OBJ path.
- `Debug Grid`: floor reference for depth, scale, and movement.

## Expected controls

- `W A S D`: move across the scene
- `Q / E`: move down / up
- `Shift`: move faster
- `Mouse`: look around while mouse capture is active
- `Right Mouse / Tab`: toggle mouse capture
- `F1`: toggle wireframe
- `F2`: print the scene legend again
- `F3`: show or hide the debug grid
- `H`: print controls and the scene legend again
- `Escape`: release mouse capture first, then exit
- Window close: exit

## Expected fallback behavior

- The missing-texture example stays visible with the magenta checker fallback.
- The missing-model example stays visible with the fallback mesh.
- The sandbox should continue running after both fallback cases are logged.

## Expected resize behavior

- Resize the window wider and taller.
- The scene should remain visible and readable.
- The camera view should continue to feel stable instead of stretching unpredictably.

## What to record

Mark each item as `Pass`, `Needs Review`, or `Fail`.

- Window title is correct
- Textured cube is visible
- Ground is visible
- Colored cube is visible
- OBJ marker is visible
- Missing-texture fallback object is visible
- Missing-model fallback object is visible
- Debug grid is visible
- Lighting keeps the scene readable
- `W A S D` movement works
- `Q / E` vertical movement works
- `Shift` speed increase works
- `Tab` toggles mouse capture
- `Right Mouse` toggles mouse capture
- Mouse look works
- Pitch clamp feels correct
- `F1` wireframe toggle works
- `F2` scene legend prints again
- `F3` debug grid visibility toggle works
- `H` help prints again
- `Escape` releases capture first, then exits
- Window close exits cleanly
- Resize behavior remains readable
- Packaged executable launches from `out/HenkaSandbox3D`
- Packaged assets load without relying on the repo root
- Offline help file is included with the packaged output

## Safe screenshots

Safe screenshots usually include only the sandbox window and its example scene.

Do not commit screenshots unless that is an intentional, reviewed part of the repo update.

## Known limitations

- OBJ loading is still early and limited to the documented subset in [docs/model-loading.md](../model-loading.md).
- Help is printed to the console because the sandbox does not have in-window text rendering yet.
- Manual visual inspection is still the best way to confirm scene readability and interaction feel.
- The packaged Windows folder is meant for local manual testing, not as a full installer or release pipeline.

## Manual results template

```text
Date:
Tester:
Build:

Scene visibility:
- Textured Cube:
- Ground:
- Colored Cube:
- OBJ Marker:
- Missing Texture:
- Missing Model:
- Debug Grid:

Controls:
- W/A/S/D:
- Q/E:
- Shift:
- Tab capture:
- Right Mouse capture:
- Mouse look:
- Pitch clamp:
- F1 wireframe:
- F2 legend:
- F3 grid:
- H help:
- Escape behavior:
- Window close:

Resize behavior:

Notes:
```
