# UI

Henka Engine now includes a small in-window UI overlay for the sandbox.

## What it is

The current UI layer is an early engine-owned overlay that can draw:

- panels
- labels
- headings
- structured value rows
- buttons
- toggles
- tabs
- status chips

It is meant to make engine samples easier to inspect and control without pulling in a larger third-party UI stack.

## What it is not

The current UI layer is not:

- a full editor
- a scene hierarchy
- a full inspector
- an asset browser
- a full runtime UI framework

## Current design

The current UI path is intentionally dependency-conscious:

- no ImGui
- no FreeType
- no external UI library
- no bundled font file

Text is rendered from a small built-in glyph table that lives in Henka source code. That keeps the current UI self-contained and easy to package.

## Renderer path

The UI overlay draws through the existing renderer after the 3D scene.

- Applications use the public UI API through Henka headers.
- OpenGL details stay inside renderer implementation files.
- SDL details stay inside the platform layer.

This keeps the public UI-facing API small and engine-oriented instead of exposing backend types directly.

## Current text limitations

The built-in text path is intentionally simple:

- fixed-size glyphs
- basic ASCII-oriented coverage for current sandbox labels
- no kerning
- no shaping
- no word wrapping system
- no Unicode layout support

That is enough for the current sandbox panel, status text, and small runtime controls.

## Sandbox panels

In `henka_sandbox3d`, press `F4` to open the panels.
On a first packaged run with no local settings file, the UI opens in `View` mode so the controls are immediately visible without covering most of the scene.
If you hide the panels, a small in-window recall hint stays visible so the viewport can stay clean without losing the `F4` and `F5` cues.

The sandbox now uses a movable in-window workspace layout:

- left and right dock regions for panels
- a dedicated scene viewport in the center
- a viewport frame that keeps the scene visually separate from the docked tools
- header dragging that undocks docked panels directly and moves floating panels
- lower-right resize grips on floating panels
- `L`, `R`, and `Home` controls as secondary reliable redocking paths
- visible splitter bars for occupied dock width resizing

Docked panels stay outside the scene. Floating workspace panels remain overlay rectangles inside the main sandbox window and can cover scene pixels visually, but they own their full visible input rectangle so clicks, drag, and resize do not leak through to viewport tools. The sandbox also provides `Open Native Panel Test`, which opens a separate OS-level UI window for validating the new multi-window foundation; it is not yet a replacement for the workspace panels.

The current `Controls` panel can:

- switch between `View`, `Inspect`, and `Full Tools`
- split its content into a readable `Main` page and a `Panels/Status` page
- toggle the grid
- toggle wireframe
- frame the selected object
- reset the view
- zoom in and out with visible buttons
- switch between explicit `Select`, `Orbit`, `Pan`, `Move`, `Rotate`, and `Scale` viewport tools
- toggle transform snapping and show the current snap increments
- toggle `Hit Boxes` so the viewport can draw the same handle regions that gizmo hit testing uses
- start with panels visible on startup and reset-style launches so `Diagnostics`, `Transform QA`, and `Physics QA` are reachable from the main Controls page without using `F4` first
- open `Physics QA` for the opt-in rigid-body demo, playback controls, selected-body actions, and debug visualization
- open `Native Panel Test` as a separate OS-level foundation window
- save sandbox settings
- reset the layout
- open in-window utilities for help, legend, paths, settings, diagnostics, and transform QA
- show short in-window status feedback for recent actions

`Inspect` and `Full Tools` keep the object panels available. `Full Tools` also keeps the heavier adjustment and status text visible.

The current `Scene Objects` panel can:

- list the current sandbox examples by name
- show hidden state
- let you select one object at a time
- page through the list when the dock height is tighter than the full object list
- stay aligned with the scene object tag and bounds foundation behind the sandbox descriptors

The current `Object Details` panel can:

- show the selected object name
- show a scene tag when available
- show visibility and transform state
- explain what the object demonstrates
- show mesh, material, and texture or fallback summary
- show whether the current object is interactable from the current camera position
- toggle visibility
- focus the camera
- reset the default transform
- open object info in the utility panel and still print it to the console

The selected object also shows a transform gizmo inside the dedicated scene viewport.

- `Select` keeps normal viewport selection active.
- `Orbit` uses left drag in the viewport to orbit around the selected object or current view target.
- `Pan` uses left drag in the viewport to pan the camera and target together.
- `Move` exposes world-axis translation handles.
- `Rotate` exposes world-axis rotation rings.
- `Scale` currently exposes a uniform center handle for the current sandbox pass.
- Snapping can be toggled from the Controls panel.
- The sandbox now draws the normal runtime gizmo as a viewport overlay from the same projected handle model that hit testing and drag start use.
- Gizmo hit testing uses the active scene viewport plus those same projected handle bounds, so the visible handles and the mouse stay aligned at normal window sizes.
- The gizmo helper pieces remain internal to the viewport tool path and do not appear as normal sandbox objects in selection, object details, persisted selection state, or normal scene picking.
- Dragging cancels safely if the selected object becomes invalid, hidden, or the active viewport changes during manipulation.
- The current sandbox path shares its projected handle model, overlay conversion, and drag math with automated tests, which helps catch real selection and transform regressions earlier.
- If the direct transform QA buttons work but gizmo dragging does not, the current failure is likely in viewport input routing or handle hit testing rather than the selected-object mutation path.

The viewport now also supports direct navigation while mouse capture is released:

- `Orbit` tool plus `Left Mouse`: orbit around the selected object or current view target
- `Pan` tool plus `Left Mouse`: pan the view
- `Alt + Left Mouse`: optional orbit shortcut
- `Middle Mouse`: optional pan shortcut
- `Mouse Wheel`: zoom the view when the cursor is over the viewport
- `F`: frame the selected object
- `Home`: reset the default camera view

Mouse wheel input over the `Controls` or `Scene Objects` panels is routed to panel paging instead of the viewport, so panel interaction does not leak into scene zooming.

A compact diagnostic strip stays visible immediately below the Scene View while panels are open. It shows the active tool, selected object, selected-highlight state, mouse capture state, whether the cursor is in the viewport, whether a visible panel owns the pointer, gizmo model state, handle count, hovered handle, drag state, last rejection reason, hovered panel, whether the cursor is on a draggable header, active panel movement or resize, dock target, and latest workspace action. The strip is informational and does not consume viewport input.

The current `Utility` panel can show:

- Help
- Scene Legend
- Object Info
- Paths
- Settings
- Diagnostics
- Transform QA
- Physics QA

That keeps normal viewer use in the window while the console remains available for fallback logs and automation.
The packaged sandbox still opens a console window at this stage, but normal viewer interaction is meant to stay inside the viewport and panels rather than depending on console output.

The sandbox also uses the current engine diagnostics snapshot in the Utility panel, and object picking can update selection when mouse capture is released. Picking and gizmo dragging use viewport-relative coordinates, so docked panel clicks do not trigger scene picks or transform drags.
The diagnostics view now surfaces the current viewport tool, gizmo mode, mouse capture state, UI mouse ownership, cursor position, selected object, gizmo validity, overlay primitive count, hovered handle, active drag state, last rejected interaction reason, last Action API command, last Action API result, and compact native test-window state.
The Transform QA view exposes direct move, rotate, scale, and reset controls that use the same local Action API path as normal object manipulation, which makes it easier to separate Action API failures from gizmo or input failures during packaged QA.
The Physics QA view exposes real enable, pause/resume, fixed-step, demo reset, gravity, collider/contact debug, impulse, velocity clear, body-type, Make Dynamic + Drop, and camera-raycast controls. It explains that Static bodies do not move from physics, Dynamic bodies fall and respond to gravity, forces, impulses, and collisions, and Kinematic bodies do not fall from gravity because they move only through explicit tool or code movement. Collider overlays are generated from the same collider descriptions used for collision detection, and physics-linked entities are ordinary selectable scene objects rather than debug helpers.
When Diagnostics, Transform QA, or Physics QA is open in the heavier layout, the utility view uses the right dock directly so its controls do not draw through Object Details.

Selection is also visible directly in the Scene View through a non-selectable highlighted bounds outline around the selected real scene object. Clearing selection, clicking empty viewport space in Select mode, hiding the selected object, or deleting it removes the highlight and updates Object Details and Diagnostics.

Panel placement and dock resizing are session-only in the current sandbox. `Reset Layout` redocks the standard panels, restores safe dock widths, makes panels visible, clears active workspace drag or resize state, and closes `Native Panel Test` if it is open. The Scene View remains the main center viewport. The test window establishes separate-window rendering and event routing; production tool-panel conversion and detachable Scene View support remain future work.

When the UI is open:

- mouse capture is released
- mouse look pauses
- camera movement pauses
- you can click the UI with the left mouse button
- `Escape` closes the panel before it returns to the usual mouse-capture and exit flow

`F5` cycles the current layout mode:

- `View`: compact controls, scene-first
- `Inspect`: object selection, concise details, and utility support
- `Full Tools`: larger inspection footprint with more utility space

If the packaged sandbox opens but you do not see the panels:

- refresh the package with `.\scripts\package_sandbox3d_windows.ps1`
- check `out/HenkaSandbox3D/PACKAGE_INFO.txt` to confirm the package was refreshed
- launch `out/HenkaSandbox3D/HenkaSandbox3D.exe` again
- confirm the startup console help mentions `F4`

The packaged QA script can confirm startup logs and UI state output, and the local viewport interaction tests can now prove more basic transform outcomes, camera helpers, workspace sizing, and shared gizmo overlay geometry, but neither replaces a human visual check for readability, drag feel, handle alignment, or gizmo handle clarity.

## Future direction

This layer is a foundation for better engine-side inspection and sample controls. It is not yet meant to replace planned editor work, hierarchy tooling, numeric property editing, saved workspace layouts, or a broader UI toolkit.
