# UI and Workspace

> **Status:** Engine-owned UI foundation with a docked and detachable Sandbox workspace

Henka includes a small in-window UI layer used by the Sandbox for inspection, controls, diagnostics, and authoring surfaces.

## Contents

- [Current UI capability](#current-ui-capability)
- [Design boundary](#design-boundary)
- [Frame and transaction contract](#frame-and-transaction-contract)
- [Text rendering](#text-rendering)
- [Sandbox workspace](#sandbox-workspace)
- [Panels](#panels)
- [Panel scrolling and presentation state](#panel-scrolling-and-presentation-state)
- [Scene View tools](#scene-view-tools)
- [Selection presentation](#selection-presentation)
- [Diagnostics and QA surfaces](#diagnostics-and-qa-surfaces)
- [Workspace persistence](#workspace-persistence)
- [Input ownership](#input-ownership)
- [Current limitations](#current-limitations)
- [Future direction](#future-direction)

## Current UI capability

The engine-owned UI layer can draw:

- panels;
- labels;
- headings;
- structured value rows;
- buttons;
- toggles;
- tabs;
- status chips;
- borders;
- polylines;
- hints;
- selectables;
- scrollbars.

The current Sandbox uses these primitives for developer-facing inspection and authoring controls.

## Design boundary

The current UI path is self-contained and dependency-conscious.

It currently uses:

- no ImGui dependency;
- no FreeType dependency;
- no external UI library;
- no bundled font file.

Applications use public Henka UI headers. SDL stays inside the platform layer. OpenGL stays inside renderer implementation files.

The UI overlay is rendered after the 3D scene through Henka's existing renderer path.

Current scope excludes a complete runtime game-UI framework, full asset browser, complete hierarchy editor, complete property inspector, and full typography system.

## Frame and transaction contract

UI construction is valid between one successful `henka_ui_begin_frame` and its matching `henka_ui_end_frame`.

The API rejects:

- nested frame-begin calls;
- unmatched frame-end calls;
- widget construction outside an active UI frame.

### Composite control transactions

Composite controls roll back their rectangle and line counts when a later construction step fails.

This applies to:

- panels;
- borders;
- text;
- polylines;
- rows;
- hints;
- buttons;
- selectables;
- tabs;
- toggles;
- status chips.

Release-confirm controls retain active ownership until complete control construction succeeds. Toggle state changes occur after successful rendering of the control.

Text fitting clamps character counts before integer conversion. Failed text measurement clears caller output. Extreme finite line geometry is rejected or converted through bounded double-precision math before backend submission.

The renderer consumes completed UI frames only. Scene/UI draw failure after renderer-frame begin uses the non-presenting abort path and leaves backend frame ownership balanced.

## Text rendering

Henka currently renders text from a small built-in glyph table stored in source code.

### Current capabilities

- fixed-size glyph rendering;
- mixed-case labels used by the Sandbox;
- ASCII-oriented coverage for current engine labels;
- self-contained packaging.

### Current text limitations

- no kerning;
- no text shaping;
- no general word-wrapping system;
- no Unicode layout support;
- no production font stack.

A broader font/text system belongs to the future Runtime Game UI / HUD roadmap work.

## Sandbox workspace

Press `F4` in `henka_sandbox3d` to show or hide the workspace panels.

Normal startup uses the `Standard` shell with no scene object selected. `Focus Viewport` is a temporary scene-dominant mode.

When panels are hidden, a small viewport hint keeps the `F4` and `F5` controls discoverable.

### Visual presentation

The current workspace uses:

- graphite/slate chrome;
- mixed-case built-in text;
- low-contrast one-pixel framing;
- flat secondary controls;
- underline tabs;
- compact switch toggles;
- quiet data-row separators;
- compact single-tab headers;
- icon-like header grips.

### Workspace topology

The workspace uses one bounded validated split topology.

Internal split nodes own:

- split orientation;
- divider;
- ratio.

Leaf nodes retain stable panel identity.

Supported topology actions include:

- close section;
- close active tab;
- restore last closed section/tab group;
- merge sections into tab groups;
- reorder tabs in a section;
- split with an available singleton panel;
- equalize an immediate split;
- maximize/restore a section;
- detach a panel into a native window;
- redock a panel;
- undo/redo workspace layout changes.

The dock stack projects visible section order from the validated topology.

### Dividers and resizing

Each internal split owns one visual divider and one larger logical hit target.

At 100% UI scale:

- visual divider: 1 framebuffer pixel;
- logical divider target: 10 logical pixels.

Divider dragging is transactional. Minimum section extents are enforced. Direct leaf children can use the bounded drag-to-close threshold. Escape or cancellation restores the pre-drag topology.

Double-clicking a divider equalizes that immediate split through the same topology transaction path.

System horizontal/vertical resize cursors appear on divider and dock-splitter hit regions.

### Section context menu

Right-clicking section-header chrome opens the bounded section context menu.

Current actions include:

- close section;
- restore last closed content;
- merge/tab-group operations;
- equalize;
- maximize/restore;
- native detach;
- chooser-backed splits into available singleton panels.

The menu supports `Up`, `Down`, `Enter`, and `Escape` keyboard navigation.

### Tab groups

Merged sections expose header tabs and project the active tab's content into the section.

Supported interactions include:

- click to activate;
- drag to reorder within a section;
- center-drop to join another tab group;
- `Left`/`Right` cycling on a hovered tab strip;
- tab extraction into a floating/detached state;
- redocking into another group or side region;
- Escape/focus-loss rollback to the prior tab order/layout.

### Focus and maximize

`Tab` and `Shift+Tab` cycle focus across visible workspace panels with wraparound.

The focused panel header receives a visible accent.

`Ctrl+M` maximizes the focused or hovered section and restores it when pressed again.

### Presets and history

The Tools panel exposes bounded workspace presets:

- Default;
- Modeling;
- Materials;
- Scene Assembly;
- Debugging;
- Minimal Viewport.

Preset changes participate in the bounded eight-entry workspace layout undo/redo history.

With the workspace visible:

- `Ctrl+Z` = undo layout;
- `Ctrl+Y` = redo layout;
- `Ctrl+Shift+Z` = redo layout.

`Reset Layout` restores the default topology, redocks detached panels, restores default presentation state, and preserves valid named layout slots.

## Panels

### Tools

The current Tools panel provides:

- Build, Game, and World work contexts;
- Main and Panels/Status pages;
- grid toggle;
- wireframe toggle;
- Frame Selected;
- Reset View;
- Zoom In/Out;
- Select, Orbit, Pan, Move, Rotate, Scale viewport tools;
- transform hotkeys and axis constraints;
- snap enable and snap values;
- `Hit Boxes` debug toggle;
- Physics QA;
- Native Panel Test;
- settings save;
- layout reset;
- Help, Scene Legend, Paths, Settings, Diagnostics, and Transform QA utilities;
- short in-window status feedback.

The main Tools page exposes Diagnostics, Transform QA, and Physics QA immediately on normal/reset-style startup.

### Scene Objects

The Scene Objects panel can:

- list current Sandbox examples;
- show hidden state;
- select one logical object at a time;
- page through the list in short docks;
- stay aligned with scene tags and bounds.

### Object Details

The Object Details panel can display:

- name;
- scene tag;
- visibility;
- transform;
- purpose/diagnostic description;
- mesh summary;
- material summary;
- texture/fallback summary;
- interaction availability;
- material definition identity and effective-instance data for supported imported content;
- bounded material dependencies;
- authored Physics and Interaction values for registered Game Authoring objects.

Available actions include:

- visibility change;
- camera focus;
- transform reset;
- Object Info utility;
- console object info;
- supported material-instance editing/reimport/reset;
- bounded Lua/HenkaScript attachment and source editing in the Game Authoring workflow.

## Panel scrolling and presentation state

Tools Main and Object Details share one bounded panel-body presentation model.

The core UI API owns:

- presentation-only scroll state;
- scrollbar thumb geometry;
- bounded thumb-drag mapping.

Sandbox editor state persists:

- scroll offsets;
- group expansion/collapse;
- Object Details group order.

Stable group IDs keep presentation state separate from selected-object state.

Content height is recomputed after group changes and resize. Scroll offsets are clamped to the current measured range. Short content does not display an invalid scrollbar.

Wheel and fractional touchpad deltas belong to a panel while the pointer is over its body. Scene View owns wheel zoom when the pointer is over Scene View.

Detached production panels use their native framebuffer geometry for the same scroll and thumb-drag behavior.

Object Details group headers provide bounded up/down ordering actions. The resulting order uses stable group IDs and persists through the local settings authority.

`Reset Layout` restores default group expansion, Object Details order, and panel scroll positions.

## Scene View tools

The Scene View is the main center viewport.

### Tool modes

| Tool | Current behavior |
| --- | --- |
| Select | Object picking; empty left-drag beyond threshold pans for touchpad use |
| Orbit | Left-drag around selected object or current view target |
| Pan | Left-drag camera and target together |
| Move | World-axis translation handles |
| Rotate | World-axis rotation rings |
| Scale | Uniform center handle in the current Sandbox path |

Optional navigation shortcuts include:

- `Alt + Left Mouse`: orbit;
- `Middle Mouse`: pan;
- `Mouse Wheel` / touchpad scroll: zoom;
- `F`: frame selected;
- `Home`: reset view.

### Gizmo ownership

The Sandbox draws transform gizmos as viewport overlays using the same projected handle model used by hit testing and drag start.

Gizmo helper data is internal to the viewport tool path. It is excluded from normal selection, Object Details, persisted selection, and scene picking.

Active drags cancel safely when:

- the selected object becomes invalid;
- the selected object becomes hidden;
- the selected object becomes locked;
- the viewport changes;
- tool ownership changes.

### Transform hotkeys

The editor-control profile provides action-based Move, Rotate, Scale, axis constraint, confirm, cancel, stepped-adjustment, and fine-adjustment commands.

See [editor-controls.md](editor-controls.md) for the current bindings and profile format.

### Camera framing

Normal startup and `Home` share the scene-first framing path after final Scene View dimensions are known.

With the default showcase pair loaded, framing targets the front side of the Giraffe and Rocket around their shared midpoint.

Capture mode reapplies deterministic framing after final docked/full-viewport aspect is known.

Older transient camera-pose settings are no longer restored automatically on normal startup. Valid editor settings such as movement speed remain preserved.

## Selection presentation

Editable selection is shown by a non-selectable viewport outline around the selected logical scene object.

### Imported multi-primitive objects

Imported multi-primitive roots use one logical selection owner for ordinary object selection.

Render children retain their own entity identity for:

- materials;
- dependencies;
- diagnostics;
- component validation.

Component picking and component overlays can retain the editable source child when it belongs to the selected logical owner.

### Outline generation

The cached bounded outline:

- aggregates source-bound render children;
- namespaces child topology during aggregation;
- follows projected indexed-triangle boundaries;
- tracks front/back transitions;
- preserves concavities;
- preserves disconnected islands.

Small and medium meshes receive bounded projected coverage/depth subdivision. Dense imported meshes use a fixed screen-space spatial index. Overflow uses conservative projected topology.

The outline is clipped to Scene View and does not cover workspace panels or the diagnostic strip.

New authoring wrappers begin with no component selected. The logical-object outline remains visible until the user selects a vertex, edge, or face.

Locked objects remain selectable and inspectable. They do not display the transform highlight or gizmo.

## Diagnostics and QA surfaces

### Compact Scene View strip

The compact strip below Scene View reports current interaction state, including:

- active tool;
- selected object;
- selected-highlight state;
- mouse capture;
- viewport cursor ownership;
- panel pointer ownership;
- gizmo validity;
- handle count;
- hovered handle;
- drag state;
- rejection reason;
- hovered panel;
- draggable-header state;
- active panel move/resize;
- dock target;
- latest workspace action.

The strip is informational and does not consume viewport input.

### Utility panel

The current Utility panel provides:

- Help;
- Scene Legend;
- Object Info;
- Paths;
- Settings;
- Diagnostics;
- Transform QA;
- Physics QA.

### Diagnostics

Diagnostics surface:

- viewport tool;
- gizmo mode;
- mouse capture;
- UI mouse ownership;
- cursor position;
- selected object;
- gizmo validity;
- overlay primitive count;
- hovered handle;
- active drag;
- latest rejected-interaction reason;
- latest Action API command/result;
- compact native-window state.

### Transform QA

Transform QA provides direct Move, Rotate, Scale, and Reset controls through the same local Action API used by normal object manipulation.

### Physics QA

Physics QA exposes:

- enable;
- pause/resume;
- fixed step;
- demo reset;
- gravity;
- collider/contact debug;
- impulse;
- velocity clear;
- body-type changes;
- Make Dynamic + Drop;
- camera raycast.

Static bodies remain unaffected by gravity/forces/impulses. Dynamic bodies participate in gravity and collision response. Kinematic bodies move through explicit tool or code movement.

Collider overlays are generated from the same collider descriptions used by physics and are clipped to Scene View.

### Native Panel Test

`Open Native Panel Test` creates a separate OS-level test window for multi-window foundation validation.

It reports native-window identity, focus, size, routed events, and close behavior.

## Workspace persistence

The local settings file persists bounded workspace state.

Persisted values include:

- floating panel rectangles;
- dock widths;
- dock assignment;
- last valid dock;
- detached virtual-screen coordinates;
- topology graph;
- split ratios;
- tab membership/order;
- closed-section mask;
- maximize state;
- active tab;
- selected workspace preset;
- named layout slots;
- UI scale;
- panel scroll offsets;
- group expansion;
- Object Details group order.

### Versioned topology snapshot

The current V2 loader can migrate the prior V1 snapshot shape.

Malformed, future, or incompatible snapshots are rejected and leave safe defaults active.

### Named layout slots

Three bounded named slots are available:

- Custom;
- Studio;
- Assembly.

Each stores validated topology, tab order, closed/maximized state, dock assignments, dock widths, and UI scale.

Restoring a slot redocks detached panels before applying the saved topology.

### Native detached panels

Production panels can detach into separate OS windows with:

- matching panel controls;
- native movement and resize;
- bounded persisted placement;
- release-confirm input;
- Dock L / Dock R / Home return controls;
- close-to-redock behavior;
- title-bar drag-back recognition when the focused window enters the main-window envelope.

Malformed or out-of-range saved positions use safe default placement.

`Reset Layout` closes detached windows and `Native Panel Test`, restores safe dock widths, clears active move/resize state, and restores the default topology.

Detachable Scene View remains future work.

## Input ownership

When the UI is open:

- mouse capture is released;
- mouse look pauses;
- camera movement pauses;
- left mouse interacts with UI controls;
- panel wheel input stays with the panel body;
- Scene View wheel input controls camera zoom;
- `Escape` cancels active workspace move/resize transactions before broader UI/exit handling.

Picking and gizmo dragging use viewport-relative coordinates. Docked panel and detached-window clicks do not enter Scene View picking.

## Current limitations

- Text rendering is ASCII-oriented and lacks shaping, kerning, Unicode layout, and general wrapping.
- Scale gizmo interaction is currently uniform-only in the Sandbox path.
- Scene View cannot detach into a native window yet.
- Full hierarchy editing is planned separately.
- Full asset browsing is planned separately.
- Complete numeric property editing is planned separately.
- Runtime game UI/HUD is a separate future system.
- Human desktop QA remains required for readability, drag feel, handle clarity, panel balance, and overall workspace presentation.
- The packaged Sandbox still opens a console window.

## Future direction

The UI foundation should continue growing through focused production needs.

Planned adjacent systems include:

- Runtime Game UI / HUD;
- Scene Hierarchy / Parenting;
- Prefabs;
- production asset database/browser workflows;
- richer property editing;
- broader typography and localization support;
- detachable Scene View after multi-window rendering and input ownership mature;
- game-facing accessibility controls.

The workspace closure path already supports reversible tab movement, floating/detached panel movement, cross-group redocking, exact Escape rollback, topology-aware section context menus, compact chrome, and deterministic settled-frame presentation for the current validated Sandbox path.
