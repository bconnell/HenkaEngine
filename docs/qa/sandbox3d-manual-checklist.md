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
5. If there was no existing packaged settings file, confirm the UI starts in a docked scene-first layout with a dedicated viewport.
6. Confirm the scene renders inside its own viewport region.
7. Confirm docked panels do not cover scene graphics.
8. Confirm the main scene objects remain visible while the UI is open.
9. Press `F4` and confirm the visible UI appears or hides as expected.
10. With the panels hidden, confirm a small in-window recall hint appears inside the sandbox window.
11. Confirm the hint is readable and does not block the main object cluster.
12. Press `F4` again and confirm the panels return.
13. Press `F5` and confirm the layout cycles between `View`, `Inspect`, and `Full Tools`.
14. Confirm `View` has the largest viewport.
15. Confirm `Inspect` keeps Scene Objects and Object Details docked beside the viewport.
16. Confirm `Full Tools` still leaves a dedicated viewport.
17. Confirm `Reset Layout` restores a usable default.
18. Click `Open Native Panel Test` and confirm a separate OS-level window opens with visible ID, focus, size, and last-event content.
19. Move and resize the native test window, then confirm its displayed state updates and the main Scene View remains unaffected.
20. Click inside the native test window and confirm it does not select an object, start a gizmo drag, orbit, or pan the main Scene View.
21. Close the native test window and confirm only that window closes, then reopen it successfully.
22. Open the native test window again, use `Reset Layout`, and confirm the test window closes safely.
23. Drag the `Controls` header and confirm it undocks directly inside the main sandbox window without requiring a button or opening a transient menu.
24. Keep dragging `Controls` and confirm it visibly follows the cursor inside that window, then remains floating when released away from a dock target.
25. Drag the floating `Controls` header more than once and confirm movement is repeatable.
26. Resize floating `Controls` from its lower-right grip and confirm its controls remain readable.
27. Drag floating `Controls` onto a visible valid dock target and confirm it redocks; also confirm `Home` is a reliable secondary default redock path.
28. Repeat direct header undock, move, resize, and redock recovery for Scene Objects, Object Details, and Utility as they become visible.
29. Confirm stable floating-panel `L` or `R` controls dock onto an available requested side if retained.
30. Drag each visible dock splitter and confirm the Scene View resizes without becoming unusable.
31. Confirm the debug strip reports hovered panel, panel header hover, panel move or resize state, dock target, and last workspace action.
32. Confirm panel drag, resize, and splitter drag do not select objects or begin viewport tools behind the panel.
33. Confirm `Reset Layout` redocks panels and restores safe dock widths after moving and resizing them.
34. Confirm window resize keeps a valid viewport and workspace layout.
35. Confirm panel text is readable by eye.
36. Confirm panel background contrast is readable against the workspace.
37. Confirm the Controls panel `Main` and `Panels/Status` pages are readable and that page switching is obvious.
38. Confirm the Scene Objects panel can reach every sample object through its page buttons or mouse wheel paging.
39. Confirm utility tabs are readable and the active utility state is obvious.
40. Confirm Help, Scene Legend, Object Info, Paths, Settings, and Diagnostics are usable in-window.
41. Confirm clicking docked or floating panels does not pick scene objects.
42. Left-click a visible object inside the viewport and confirm picking updates the selection.
43. Confirm the selected real scene object has a visible viewport highlight that follows the object.
44. Click empty viewport space in Select mode or use Clear Selection and confirm the viewport highlight disappears.
45. Confirm Object Details and Diagnostics both report no selected object after deselection.
46. Confirm the selected object shows a visible gizmo in the viewport when a transform tool is active.
47. Confirm the compact strip continues to update viewport tool, highlight, gizmo, ownership, and workspace fields while panels are moved.
48. Open `Diagnostics` directly from Controls and confirm interaction fields update.
49. Confirm Object Details labels optional object interaction availability as `Object Use`, separate from transform status.
50. Open `Transform QA` and confirm direct move, rotate, scale, and reset controls visibly change the selected real object.
51. Click `Orbit` and left-drag more than once; confirm it remains repeatable after floating and redocking panels.
52. Click `Pan` and left-drag more than once; confirm it remains repeatable after floating and redocking panels.
53. Use the mouse wheel over the viewport and confirm zoom works; wheel over a panel must not zoom the viewport.
54. Press `F` and `Home` and confirm camera framing and reset remain usable.
55. Select `Textured Cube`, use Move X/Y/Z, Rotate X/Y/Z, and Scale, and confirm the selected object transforms after dock movement and resize.
56. Toggle `Hit Boxes` and confirm the viewport still shows handle regions used for hit testing.
57. Confirm Object Details updates after viewport manipulation.
58. Confirm no gizmo helper becomes the selected object.
59. Hide the selected object during a gizmo interaction and confirm it stops safely.
60. Confirm Reset Transform and Focus Camera still work after workspace changes.
61. Confirm `F4`, `F5`, `Escape`, and close-window behavior remain clean.
62. Confirm no named-engine comparisons appear in runtime text or packaged help.
63. Confirm the runtime and help do not present in-window panels as separate OS-level windows or imply that Scene View can detach.

## Physics QA Verification

1. Launch clean and confirm panels are visible automatically, Controls is visible, and `Physics QA` is reachable without pressing `F4`.
2. Click `Enable` and confirm the dynamic cube bodies and sphere-collider marker move under gravity.
3. Confirm moving bodies collide with the ground instead of falling through it in normal playback.
4. Click `Pause`, then `Step`, and confirm one fixed-step movement occurs while paused.
5. Click `Reset Demo` and confirm the test positions and velocities reset.
6. Toggle `Gravity` off and confirm dynamic motion no longer accelerates downward; restore it afterward.
7. Toggle `Colliders` and confirm sphere, AABB, plane, and trigger outlines correspond to the linked visible samples.
8. Toggle `Contacts` and confirm contact normal lines appear during collisions without becoming selectable.
9. Select a dynamic physics-linked sample, use `Impulse Up` and `Impulse Forward`, and confirm visible movement and updated status.
10. Use `Clear Velocity` and confirm the selected body motion changes predictably.
11. Change a selected supported sample between `Static`, `Dynamic`, and `Kinematic`, and confirm the UI explains that only Dynamic bodies fall from gravity.
12. Use `Make Dynamic + Drop` on a supported moved body and confirm gravity runs and the body drops.
12. Use `Camera Raycast` while aiming toward a physics sample and confirm the latest-action result reports a hit or miss.
13. Confirm collision and trigger event reporting updates while bodies collide or pass through the trigger volume.
14. Pause physics, move a linked selected object with the gizmo or Transform QA, and confirm the body and object remain synchronized.
15. Confirm physics debug overlays do not become selected objects and do not interfere with gizmos.
16. With Physics QA active, repeat Move, Rotate, Scale, Orbit, Pan, Zoom, `F4`, `F5`, and Native Panel Test checks to confirm the existing baseline remains usable.

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
- `Left Mouse`: select or manipulate inside the viewport when mouse capture is released
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
- On a first packaged run with no existing settings file, the UI starts in a docked scene-first layout.
- The scene stays inside its own dedicated viewport when panels are visible.
- Opening the UI releases mouse capture.
- Mouse look pauses while the UI is open.
- `F5` cycles View, Inspect, and Full Tools layouts.
- The `Controls` panel can toggle the debug grid and wireframe state with the mouse.
- The `Controls` panel looks lighter in `View` mode and keeps a visible in-window status area.
- The `Controls` panel can switch between `Select`, `Orbit`, `Pan`, `Move`, `Rotate`, and `Scale`.
- Snap can be toggled for transform dragging.
- The `Hit Boxes` toggle can show the current gizmo hit regions.
- Docked panels stay outside the dedicated scene viewport in normal workspace modes.
- The `Controls` panel can adjust mouse sensitivity and camera speed with the mouse.
- The `Controls` panel can reset the camera, save settings, reset sandbox settings, print help, print the scene legend, and reset the panel layout.
- The `Scene Objects` panel lists the current scene examples by name.
- The `Diagnostics` utility reports current viewport, selection, and interaction state.
- The `Transform QA` utility can move, rotate, scale, and reset the selected object through direct controls.
- Clicking an object updates the `Object Details` panel.
- The `Object Details` panel can toggle visibility, focus the camera, reset the selected transform, and print object info.
- The `Object Details` panel shows tag and interaction state when those foundations are available.
- A selected object shows a visible transform gizmo in the viewport, and the visible handle matches the clickable region.
- Move, Rotate, and Scale mode handles should drag only inside the viewport.
- The visible handle under the cursor should be the handle that becomes active.
- Internal gizmo helper pieces should not appear as selected sandbox objects or visible standalone scene content.
- Gizmo dragging should stop safely if the viewport changes, the selected object becomes hidden, or the panels reopen mid-drag.
- Pressing `F4` again closes the UI.
- When the UI is closed, a small in-window hint reminds you that `F4` restores panels and `F5` changes layout.
- After the UI closes, `Right Mouse` and `Tab` can capture the mouse again.
- With mouse capture released, left-click can select a visible sample object or drag the active gizmo in the viewport.
- Picking should only respond to clicks inside the viewport.
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
- Controls feel lighter and less cramped
- Toggle labels and `ON` or `OFF` text are clear
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
- UI gizmo mode tabs work
- UI snap toggle works
- Viewport move gizmo works
- Viewport rotate gizmo works
- Viewport scale gizmo works
- Viewport uniform scale handle works
- Visible handle and mouse alignment feel correct
- Snap affects transform dragging
- UI print object info works
- Utility tabs are readable
- Active utility state is clear
- In-window status feedback works
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
- Floating production panels currently remain inside the main sandbox window. `Native Panel Test` is a separate OS-level validation surface; production native panel detachment and detachable Scene View behavior are not implemented yet.
- Local action and viewport interaction tests now prove more basic selection and transform outcomes, but manual QA is still required for visual feel and drag comfort.
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
- UI gizmo modes:
- UI snap toggle:
- Viewport move gizmo:
- Viewport rotate gizmo:
- Viewport scale gizmo:
- Viewport uniform scale:
- Transform snapping:
- Utility tabs:
- Active utility state:
- In-window status:
- Persisted UI state:
- H help:
- Escape behavior:
- Window close:

Resize behavior:

Notes:
```
