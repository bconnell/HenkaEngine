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
- a docking system
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

The sandbox now uses a docked workspace layout:

- left and right dock regions for panels
- a dedicated scene viewport in the center
- a viewport frame that keeps the scene visually separate from the docked tools

The panels no longer draw on top of the scene in normal docked modes.

The current `Controls` panel can:

- switch between `View`, `Inspect`, and `Full Tools`
- toggle the grid
- toggle wireframe
- switch between `Select`, `Move`, `Rotate`, and `Scale` gizmo modes
- toggle transform snapping and show the current snap increments
- reset the camera
- save sandbox settings
- reset the layout
- open in-window utilities for help, legend, paths, settings, and diagnostics
- show short in-window status feedback for recent actions

`Inspect` and `Full Tools` keep the object panels available. `Full Tools` also keeps the heavier adjustment and status text visible.

The current `Scene Objects` panel can:

- list the current sandbox examples by name
- show hidden state
- let you select one object at a time
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
- `Move` exposes world-axis translation handles.
- `Rotate` exposes world-axis rotation rings.
- `Scale` currently exposes a uniform center handle for the current sandbox pass.
- Snapping can be toggled from the Controls panel.
- Gizmo hit testing uses the active scene viewport plus projected handle bounds, so the visible handles and the mouse stay aligned at normal window sizes.
- The gizmo helper pieces are internal to the viewport tool path and do not appear as normal sandbox objects in selection, object details, persisted selection state, or normal scene picking.
- Dragging cancels safely if the selected object becomes invalid, hidden, or the active viewport changes during manipulation.

The current `Utility` panel can show:

- Help
- Scene Legend
- Object Info
- Paths
- Settings
- Diagnostics

That keeps normal viewer use in the window while the console remains available for fallback logs and automation.
The packaged sandbox still opens a console window at this stage, but normal viewer interaction is meant to stay inside the viewport and panels rather than depending on console output.

The sandbox also uses the current engine diagnostics snapshot in the Utility panel, and object picking can update selection when mouse capture is released. Picking and gizmo dragging use viewport-relative coordinates, so docked panel clicks do not trigger scene picks or transform drags.

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

The packaged QA script can confirm startup logs and UI state output, but it still does not replace a human visual check for readability, drag feel, handle alignment, or gizmo handle clarity.

## Future direction

This layer is a foundation for better engine-side inspection and sample controls. It is not yet meant to replace planned editor work, hierarchy tooling, numeric property editing, floating panels, or a broader UI toolkit.
