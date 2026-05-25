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
8. Change `F1` or `F3`, exit cleanly, and confirm `out/HenkaSandbox3D/user/sandbox3d.settings` is created.
9. Run the package script again and confirm the `user/` folder is preserved by default.
10. If you intentionally test `-ResetUserData`, confirm the settings file is removed only when that switch is used.
11. Press `F4` and confirm the sandbox panels appear in the packaged run.
12. If you want a quick automated packaged check first, run:

   ```powershell
   .\scripts\check_packaged_sandbox3d_windows.ps1
   ```

## Visible UI verification

1. Run:

   ```powershell
   .\scripts\package_sandbox3d_windows.ps1
   ```

2. Open `out/HenkaSandbox3D\HenkaSandbox3D.exe`.
3. Confirm the startup help says `F4` opens the in-window panels.
4. Confirm `out/HenkaSandbox3D/PACKAGE_INFO.txt` was refreshed for the current package.
5. If there was no existing packaged settings file, confirm the UI starts in a compact layout that leaves most of the scene visible.
6. Confirm the main scene objects remain visible while the UI is open.
7. Press `F4` and confirm the visible UI appears or hides as expected.
8. Press `F5` and confirm the layout cycles between `View`, `Inspect`, and `Full Tools`.
9. Confirm `View` leaves the scene mostly visible.
10. Confirm `Inspect` keeps Scene Objects and Object Details usable.
11. Confirm `Full Tools` shows the heavier inspection layout.
12. Confirm `Reset Layout` restores a usable default.
13. Confirm panel text is readable by eye.
14. Confirm panel background contrast is readable against the scene.
15. Confirm `Escape` behavior still works.
16. Confirm close-window exit remains clean.
17. If the UI is not visible or feels wrong, note whether the console reports `Sandbox panel: shown`, `Sandbox panel: hidden`, or `Sandbox UI ready`.

## Expected startup behavior

- A window opens with the title `Henka Engine Sandbox 3D`.
- Console help prints once at startup.
- The console legend explains what each example object represents.
- The sandbox keeps running until you close it or press `Escape` while mouse capture is released.
- If `user/sandbox3d.settings` is missing, the sandbox should still start with safe defaults.

## Expected scene examples

You should be able to identify these examples:

- `Textured Cube`: centered in the scene.
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
- `F4`: show or hide the sandbox panels
- `F5`: cycle View, Inspect, and Full Tools
- `H`: print controls and the scene legend again
- `Escape`: close the UI first, then release mouse capture, then exit
- Window close: exit

## Expected sandbox panel behavior

- Pressing `F4` opens panels titled `Controls`, `Scene Objects`, and `Object Details`.
- On a first packaged run with no existing settings file, the UI starts in a scene-first layout.
- Opening the UI releases mouse capture.
- Mouse look pauses while the UI is open.
- `F5` cycles View, Inspect, and Full Tools layouts.
- The `Controls` panel can toggle the debug grid and wireframe state with the mouse.
- The `Controls` panel can adjust mouse sensitivity and camera speed with the mouse.
- The `Controls` panel can reset the camera, save settings, reset sandbox settings, print help, print the scene legend, and reset the panel layout.
- The `Scene Objects` panel lists the current scene examples by name.
- Clicking an object updates the `Object Details` panel.
- The `Object Details` panel can toggle visibility, focus the camera, reset the selected transform, and print object info.
- Pressing `F4` again closes the UI.
- After the UI closes, `Right Mouse` and `Tab` can capture the mouse again.
- Pressing `Escape` while the UI is open closes the UI first.

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
- `F4` sandbox panel toggle works
- UI panels appear and are readable
- UI releases mouse capture when opened
- Scene Objects panel appears
- Object Details panel appears
- Object selection updates the details panel
- UI grid toggle works
- UI wireframe toggle works
- UI mouse sensitivity controls work
- UI camera speed controls work
- UI reset camera works
- UI save settings works
- UI reset sandbox settings works
- UI visibility toggle works
- UI focus camera works
- UI reset transform works
- UI print object info works
- `Escape` closes the UI first
- Persisted state reloads after a clean restart
- `H` help prints again
- `Escape` releases capture first, then exits
- Window close exits cleanly
- Resize behavior remains readable
- Packaged executable launches from `out/HenkaSandbox3D`
- Packaged assets load without relying on the repo root
- Offline help file is included with the packaged output
- Packaged settings file is created after a clean exit
- Packaged user folder is preserved across a normal package refresh
- Packaged user folder resets only when requested explicitly

## Safe screenshots

Safe screenshots usually include only the sandbox window and its example scene.

Do not commit screenshots unless that is an intentional, reviewed part of the repo update.

## Known limitations

- OBJ loading is still early and limited to the documented subset in [docs/model-loading.md](../model-loading.md).
- The UI overlay is still intentionally small and is not an editor.
- Manual visual inspection is still the best way to confirm scene readability and interaction feel.
- The packaged Windows folder is meant for local manual testing, not as a full installer or release pipeline.
- The current persistence layer is local-only and stores simple key/value settings, not encrypted save data.

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
- F4 panel:
- F5 layout:
- Scene Objects panel:
- Object Details panel:
- Object selection:
- UI grid toggle:
- UI wireframe toggle:
- UI mouse sensitivity:
- UI camera speed:
- UI reset camera:
- UI save settings:
- UI reset settings:
- UI visibility toggle:
- UI focus camera:
- UI reset transform:
- UI print object info:
- Persisted UI state:
- H help:
- Escape behavior:
- Window close:

Resize behavior:

Notes:
```
