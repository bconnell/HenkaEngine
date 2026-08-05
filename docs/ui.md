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

## Frame and transaction contract

UI construction is valid only between one successful `henka_ui_begin_frame` call and its matching `henka_ui_end_frame` call. Nested begin calls and unmatched end calls fail without discarding the current command list. Public widgets and overlays reject calls outside an active construction frame.

Composite controls are transactional. Panels, borders, text, polylines, rows, hints, buttons, selectables, tabs, toggles, and status chips roll their rectangle and line counts back if any later step fails. Release-confirm controls preserve active ownership until a complete control is drawn successfully, and toggles change caller state only after successful rendering. Text fitting clamps character counts before integer conversion, failed text measurement clears caller outputs, and extreme finite line geometry is rejected or converted through bounded double-precision math before reaching the graphics backend.

The renderer consumes only completed UI frames. Scene or UI draw failures after a renderer frame begins use a non-presenting abort path so backend frame state is balanced without swapping a partial frame.

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
On startup, the UI opens in `View` mode with no selected scene object so the controls are immediately visible without covering most of the scene.
If you hide the panels, a small in-window recall hint stays visible so the viewport can stay clean without losing the `F4` and `F5` cues.

The sandbox now uses a movable workspace layout with a more neutral graphite and slate shell. The workspace model also has a bounded, validated single-root split topology: internal split nodes own their divider and ratio, leaf sections retain stable panel identity, and divider edits can be committed or rolled back transactionally. A right-click on section header chrome opens the required context menu; tool content retains right-click authority. Close, merge/tab-group, equalize, maximize/restore, native-detach, last-closed restoration, and available-singleton split actions are wired to the bounded topology/native-window paths. `Close this section` removes a complete multi-tab section, while the separate tab-close path removes only the active tab and a final-tab close removes the containing section; the previous topology is retained for last-closed restoration. The dock stack now projects visible section order from the validated topology, so closed and merged-away leaves no longer remain in legacy dock ordering. Merged sections expose bounded header tab controls, project the selected tab's content into the section, and support same-section tab reordering with click-preserving release and rollback on cancellation; the remaining topology editor workflow is still open:

- left and right dock regions for panels
- stacked multi-panel side docks that share space instead of covering each other
- a dedicated scene viewport in the center
- a viewport frame that keeps the scene visually separate from the docked tools
- header dragging that redocks across valid zones or opens native detached windows
- native OS frame movement and resizing for detached windows
- visible splitter bars for occupied dock width resizing
- one topology-owned divider per internal split, with a 1 px visual wire and 10 px logical hit target
- idle hover hints near dividers, dock splitters, panel headers, and floating resize handles; hints are suppressed while menus or interaction transactions own the pointer
- system horizontal and vertical resize cursors on the invisible divider and dock-splitter hit targets; the cursor returns to the platform default when panels are hidden, the viewport owns the pointer, or mouse capture is active
- bounded divider drag transactions with minimum section extents, a restrained close-threshold preview, transactional drag-to-close for direct leaf children, and rollback support
- tab-strip drag reordering within a merged section, with a live insertion marker, click-preserving selection, stable active-tab identity, and transactional cancellation
- Left/Right keyboard cycling for a hovered merged tab strip, with wraparound and no viewport ownership when the header is not hovered
- Tab/Shift+Tab cycles focus across visible workspace panels with wraparound; the focused panel header receives a visible accent and focus does not enter hidden or detached-stale panels. Hovering a merged tab now explains activation, same-section reordering, and center-drop tab grouping. Controls exposes a bounded eight-entry workspace layout undo/redo history; undo and redo reconcile detached panels before restoring a validated snapshot.
- Preset changes participate in the bounded layout history. `Reset Layout` restores the default topology while preserving valid saved named layout slots.
- With the workspace visible, `Ctrl+Z`, `Ctrl+Y`, and `Ctrl+Shift+Z` provide keyboard undo/redo; detached panels are reconciled before the snapshot is applied.

Docked panels stay outside the scene. While dragging a docked panel, valid left and right dock targets show a thin outline over the final stack slot. Releasing on an outline redocks there, including across the workspace. Releasing away from the outlines opens a separate native tool window, so the detached panel can move outside the main sandbox frame without clipping. Detached production panels render their matching controls and route their release-confirm input in the native surface; a compact return bar provides Dock L, Dock R, and Home actions. Closing that native window returns the panel to its last valid dock. Bounded detached virtual-screen placement is saved and restored through the public tool-window state/position path; malformed or out-of-range coordinates safely fall back. Moving a focused detached title bar into the main-window envelope requests bounded drag-back docking. Detachable Scene View remains future work. `Reset Layout` closes detached windows and restores the default workspace.

The current `Controls` panel can:

- switch between `View`, `Inspect`, and `Full Tools`
- split its content into a readable `Main` page and a `Panels/Status` page
- toggle the grid
- toggle wireframe
- frame the selected object
- reset the view
- zoom in and out with visible buttons
- switch between explicit `Select`, `Orbit`, `Pan`, `Move`, `Rotate`, and `Scale` viewport tools
- use action-based move, rotate, scale, axis-constraint, confirm, cancel, stepped-adjustment, and fine-adjustment hotkeys
- toggle transform snapping and show the current snap increments
- toggle `Hit Boxes` so the viewport can draw the same handle regions that gizmo hit testing uses
- start with panels visible on startup and reset-style launches so `Diagnostics`, `Transform QA`, and `Physics QA` are reachable from the main Controls page without using `F4` first
- start launches with no selected scene object until the user selects one
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
The Physics QA view exposes real enable, pause/resume, fixed-step, demo reset, gravity, collider/contact debug, impulse, velocity clear, body-type, Make Dynamic + Drop, and camera-raycast controls. It explains that Static bodies do not move from physics, Dynamic bodies fall and respond to gravity, forces, impulses, and collisions, and Kinematic bodies do not fall from gravity because they move only through explicit tool or code movement. Collider overlays are generated from the same collider descriptions used for collision detection, clipped to the Scene View, and physics-linked entities are ordinary selectable scene objects rather than debug helpers.
When Diagnostics, Transform QA, or Physics QA is open in the heavier layout, the utility view uses the right dock directly so its controls do not draw through Object Details.

Editable selection is visible directly in the Scene View through a non-selectable highlighted bounds outline around the selected real scene object. The highlight is clipped to the Scene View and does not draw over workspace panels or the debug strip. Locked objects, including the default Ground, remain selectable and inspectable but do not show the yellow transform highlight or a gizmo. Clearing selection, clicking empty viewport space, hiding or locking the selected object, deleting it, or changing tools clears active transform-session ownership and updates Object Details and Diagnostics.

In-app floating panel rectangles, dock widths, dock assignment, and last valid dock are persisted through the local settings file. Detached OS windows reopen with bounded saved virtual-screen coordinates through the public tool-window state/position API; malformed or out-of-range coordinates safely fall back to the normal window placement. `Reset Layout` redocks the standard panels, closes detached windows, restores safe dock widths, makes panels visible, clears active workspace drag or resize state, and closes `Native Panel Test` if it is open while preserving valid named layout snapshots. The Scene View remains the main center viewport. The bounded topology graph, ratios, tab membership/order, closed-section mask, maximize state, active tab, and selected bounded workspace preset are restored through a versioned validated settings snapshot; the current v2 loader migrates the prior v1 snapshot shape and rejects malformed, future, or incompatible snapshots without replacing defaults. The topology API and section context menu cover bounded close/restore, last-closed restoration, tab-group merge, equalize, maximize/restore, native-detach, chooser-backed available-singleton splits, and visible header tab switching with active-content projection. `Close this section` removes the complete section and its tab group; the separate tab-close path removes only the active tab and removes the section when its final tab closes. The existing last-closed restore path can recover the prior tab group. Maximizing a docked section expands it across the workspace and leaves the scene as a stable presentation surface underneath. Runtime dock stacks now project nested topology rectangles and draw one shared thin divider per internal split with a DPI-scaled logical hit target (10 px at 100%); the visible wire remains one framebuffer pixel. Divider drag updates split ratios transactionally and release near a direct leaf closes it through the existing restoration path. The context menu exposes a visible selection state and Up/Down, Enter, and Escape keyboard navigation. Center-drop panel dragging now previews and joins an existing tab group transactionally. Tab strips also support same-section reordering while preserving the clicked tab as active; Escape/focus-loss cancellation restores the prior order. The Controls panel exposes deterministic Default, Modeling, Materials, Scene Assembly, Debugging, and Minimal Viewport presets; switching a preset participates in bounded layout undo/redo history, closes detached tool windows through the normal redock path, and replaces the validated topology atomically, while later topology edits are labeled Custom. Detached production panels render full matching controls with a native return bar and bounded title-bar drag-back recognition when a focused detached window enters the main-window envelope. Three bounded named layout slots—Custom, Studio, and Assembly—persist validated topology, tab order, closed/maximized state, dock assignments, dock widths, and UI scale; restoring a slot redocks detached panels before applying it. Slot names and contents are bounded and local; an unbounded layout marketplace and detachable Scene View remain future work.

Current builds expose bounded detached virtual-screen coordinates, capture them into sandbox settings, restore valid coordinates on the next detach, and recognize a focused native-window move into the main-window envelope as a title-bar drag-back request; malformed positions safely fall back. Detachable Scene View remains future work.

When the UI is open:

- mouse capture is released
- mouse look pauses
- camera movement pauses
- you can click the UI with the left mouse button
- `Escape` cancels an active workspace drag or resize transaction and restores any topology transaction before closing the panel
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
