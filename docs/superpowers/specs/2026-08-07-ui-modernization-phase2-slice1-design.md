# Henka Engine UI Modernization Phase 2 - Slice 1 Design

Date: 2026-08-07
Status: Approved design, implementation not started
Authoritative starting commit: `993d7667f36b47438322fd27a219e3551ea0092d`

## 1. Objective

Replace the most button-dense part of the current editor UI with reusable progressive-disclosure infrastructure without reopening Phase 1 docking behavior.

Slice 1 introduces a core Henka UI disclosure/tree/property-group primitive and converts only the Controls panel Main page. The Camera/Status page, QA page, Scene Objects panel, Object Details panel, detached-window behavior, workspace docking/topology, renderer behavior, and GI behavior remain outside this implementation slice except where regression validation proves they are unchanged.

## 2. Why Controls/Main Is the First Conversion

The current Controls/Main page combines workspace layout selection, named layouts, custom layout save/restore, layout history, grid state, viewport commands, viewport tool selection, snapping, and developer/debug toggles in one vertically dense flat surface.

That makes it a better first proving ground than Scene Objects because it exercises the interaction types future editor panels need: collapsible categories, property/state rows, mutually exclusive selectors, command buttons, persistent expansion state, focus and keyboard navigation, clipping/scrolling inside a constrained panel, and separation of normal editor controls from QA/developer controls.

The QA page is already segregated and remains unchanged.

## 3. Scope

### 3.1 In Scope

1. Add reusable disclosure/tree/property-group support to the core `henka_ui` layer.
2. Extend UI frame input so the UI can react to navigation keys deliberately instead of consulting sandbox-specific input ad hoc.
3. Add persistent keyboard focus for disclosure rows.
4. Add visible-row keyboard navigation:
   - Up/Down: move focus among visible disclosure rows.
   - Left: collapse the focused expanded group.
   - Right: expand the focused collapsed group.
   - Enter: toggle the focused disclosure group.
5. Mouse behavior:
   - clicking a disclosure header focuses it;
   - clicking the header toggles expanded/collapsed state;
   - child rows are interactive only while their parent group is expanded.
6. Add indentation and a bounded content region suitable for nested rows.
7. Convert Controls/Main into four groups:
   - Workspace
   - Viewer
   - Viewport
   - Viewport Tool
8. Persist group expansion state using sandbox settings.
9. Extend unit and packaged desktop-harness coverage for disclosure behavior and persistence.
10. Update user-facing help/documentation only where the collapsible workflow materially changes the user experience.

### 3.2 Explicitly Out of Scope

- Docking, redocking, detached-window topology, splitter behavior, workspace presets, or Phase 1 layout semantics.
- Scene Objects hierarchy conversion.
- Object Details/Inspector conversion.
- Asset browser/tree work.
- Renderer settings conversion.
- Search/filter UI.
- Drag-and-drop tree reparenting.
- Multi-select tree behavior.
- Full icon system redesign.
- New renderer or GI work.
- General visual redesign unrelated to disclosure hierarchy.
- Replacing all buttons globally.

## 4. Core UI Architecture

The primitive belongs in the core Henka UI layer. Sandbox code consumes the core primitive. It must be reusable by future Scene Objects, Assets, Inspector, renderer settings, diagnostics, rig hierarchies, project files, modeling tools, and agent/LLM workspace surfaces.

Conceptual API responsibilities:

```text
tree_begin()
tree_node()
tree_leaf()
property_group()
property_row()
tree_end()
```

The exact C public API names may follow existing Henka naming conventions. The names shown above describe responsibilities, not required identifiers.

### 4.1 State Ownership

The UI context owns transient interaction state:

- focused row ID;
- active mouse row ID;
- visible disclosure row ordering for the current frame;
- current indentation depth;
- active bounded content/clip region;
- per-frame keyboard-navigation intent.

The application owns durable semantic state:

- whether each top-level Controls/Main group is expanded;
- persisted user preference values;
- application actions invoked by child controls.

Expansion state is not hidden inside the UI context because the sandbox must persist it across process restarts.

### 4.2 Stable IDs

Every disclosure header and interactive child requires a stable ID.

IDs must remain stable across frames and must not depend on array addresses, transient stack storage, visible row index, or layout position.

Controls/Main group IDs:

```text
controls.main.workspace
controls.main.viewer
controls.main.viewport
controls.main.viewport_tool
```

Persisted setting keys:

```text
ui.controls.main.workspace.expanded
ui.controls.main.viewer.expanded
ui.controls.main.viewport.expanded
ui.controls.main.viewport_tool.expanded
```

## 5. Default Expansion State

On first run, or when no saved preference exists:

| Group | Default |
|---|---|
| Workspace | Collapsed |
| Viewer | Expanded |
| Viewport | Expanded |
| Viewport Tool | Collapsed |

A saved valid preference overrides the default on later launches.

## 6. Controls/Main Information Architecture

### 6.1 Workspace

Contains the existing View / Inspect / Full Tools layout selector, named workspace presets, Save Custom / Restore Custom, custom layout slot save/restore commands, and Undo Layout / Redo Layout.

These remain functionally identical. The category is collapsed by default because workspace topology is not an every-frame adjustment.

### 6.2 Viewer

Contains Grid visibility. This is expanded by default.

Grid remains a state control, not a command button.

### 6.3 Viewport

Contains Frame Selected, Reset View, Zoom In, and Zoom Out. These remain command controls.

This group is expanded by default.

### 6.4 Viewport Tool

Contains Select / Orbit / Pan, Move / Rotate / Scale, Snap state, Handle hit-box visibility, and Reflection Probe Volume visibility.

Tool-mode choices remain mutually exclusive selector-style controls. State flags remain toggles. QA-only controls stay on the QA page.

This group is collapsed by default.

## 7. Interaction Model

### 7.1 Mouse

A disclosure row has one primary hit target covering its header row. Releasing the left mouse button on the same active disclosure row toggles it, following the existing Henka UI press/release activation model.

Clicking a disclosure row also assigns keyboard focus.

Child controls keep their existing mouse behavior.

Collapsed children are not drawn, registered as interactive, permitted to request mouse ownership, or included in keyboard row ordering.

### 7.2 Keyboard

Keyboard navigation applies only while the UI is visible and a disclosure row owns UI focus.

Up and Down move among currently visible disclosure headers in deterministic draw order.

Left collapses the focused row when expanded. If already collapsed, it is a no-op in Slice 1.

Right expands the focused row when collapsed. If already expanded, it is a no-op in Slice 1.

Enter toggles the focused row.

Keyboard events used by the UI must be consumed or otherwise prevented from simultaneously triggering sandbox editor actions when the UI has acted on them.

Tab-order navigation between arbitrary controls is not introduced in this slice.

### 7.3 Focus

The focused disclosure row receives a visible focus treatment distinct from hover and active-click state.

If the currently focused row is not drawn in the next frame, focus is cleared safely.

A collapsed parent never leaves focus on one of its hidden children because Slice 1 assigns keyboard focus only to disclosure headers.

## 8. Bounded Content, Clipping, and Scrolling

The primitive must support a bounded panel-content region.

Requirements:

- child rows outside the visible content rectangle are not interactive;
- drawing is clipped or skipped outside the bounded region according to existing Henka capabilities;
- disclosure expansion changes content height deterministically;
- existing panel scrolling/paging behavior must not regress;
- the primitive must not change dock or panel geometry.

Slice 1 should implement the smallest reusable bounded-region mechanism required by Controls/Main and must not introduce a competing layout system.

## 9. Input Plumbing

The current UI frame descriptor carries mouse state but not navigation-key state.

Slice 1 extends the frame descriptor with only the key information required for disclosure navigation:

- Up pressed
- Down pressed
- Left pressed
- Right pressed
- Enter pressed

The sandbox populates those fields from Henka input before `henka_ui_begin_frame`.

This keeps core UI deterministic and testable without making its implementation depend directly on the engine or sandbox.

## 10. Persistence Flow

Startup:

1. Initialize group defaults.
2. Load settings.
3. For each known expansion key, apply a saved valid boolean value when present.
4. Render with the resulting state.

Interaction:

1. User changes disclosure state.
2. Application state changes immediately.
3. The corresponding setting value is updated through the existing settings mechanism.
4. Existing settings persistence writes the preference using the established sandbox workflow.

Restart:

1. Settings reload.
2. The same disclosure state is restored before the first normal Controls/Main interaction.

Missing or malformed settings fall back to the documented defaults without making the UI unusable.

## 11. Error Handling and Invariants

Core primitive behavior must fail closed for invalid arguments.

Required invariants:

- null UI context returns an invalid-argument result or false according to existing API convention;
- zero/negative row bounds never create a clickable area;
- null/empty IDs are rejected;
- nesting depth cannot underflow;
- tree/property end calls cannot silently corrupt UI state;
- hidden/clipped rows cannot capture input;
- duplicate visible IDs in the same frame must not produce ambiguous activation;
- focus must never reference transient caller memory;
- expansion state remains application-owned and survives UI context recreation only through explicit persistence.

No allocation failure may leave the UI frame in partially mutated interaction state.

## 12. Testing Strategy

### 12.1 Core Unit Tests

Extend existing UI tests to cover:

- disclosure row renders while UI is visible;
- mouse press/release toggles a disclosure row;
- click assigns focus;
- expanded state draws/registers children;
- collapsed state suppresses child interaction;
- indentation changes child-row position deterministically;
- Up/Down traverses visible disclosure rows;
- Left collapses;
- Right expands;
- Enter toggles;
- hidden/clipped disclosure rows do not activate;
- focus clears safely when a focused row disappears;
- invalid arguments fail safely;
- existing button, toggle, selectable, tab, overlay, and text behavior remains unchanged.

### 12.2 Sandbox/Workspace Tests

Add focused tests for:

- default expansion state;
- settings load/save round-trip;
- malformed/missing settings fallback;
- Controls/Main group state remains independent of docking/topology state.

### 12.3 Packaged Desktop Harness

Extend the existing packaged UI harness to prove:

1. Controls/Main starts with expected visible group headers.
2. A collapsed default group can be expanded by mouse.
3. A child control becomes visible/interactable only after expansion.
4. The group can be collapsed again.
5. Expansion preference is persisted.
6. After a clean app restart, the saved expansion state is restored.
7. Existing Controls header context menu still works.
8. Existing QA tab remains reachable.
9. Existing grid workflow remains valid.
10. Stationary rendered viewport stability remains within the established gate.
11. Native/detached panel validation still passes.

The existing stability threshold must not be relaxed for this UI work.

## 13. Validation and Publication

Before commit:

- verify exact repository identity and expected parent SHA;
- verify clean tree;
- inspect source before editing;
- run relevant unit tests;
- run Release build;
- run working-tree package/startup or contract validation as appropriate;
- inspect staged scope and `git diff --check`.

After commit at exact HEAD:

- clean-tree gate;
- unit tests;
- Release build;
- Release package with exact provenance;
- package contract;
- full packaged interactive desktop harness;
- external C-template validation;
- public-hygiene/integrity checks already required by repository workflow when applicable.

Push only after every required exact-HEAD gate passes and `origin/main` has not changed.

## 14. Expected Implementation Surface

Discovery indicates a bounded implementation surface similar to:

- public/core UI header for new primitive/frame-input declarations;
- core UI context/internal implementation;
- core UI implementation source;
- UI unit tests;
- `examples/sandbox3d/main.c` for Controls/Main conversion, state, settings, and frame input;
- packaged UI harness;
- user-facing sandbox/help documentation if the interaction workflow materially changes.

Exact paths and names are determined from the current repository at implementation time. No unrelated refactor is authorized.

## 15. Success Criteria

Slice 1 is complete when:

1. Controls/Main is materially less button-dense at first glance.
2. Four disclosure groups exist with the approved defaults.
3. Group state persists across restart.
4. Mouse and required keyboard semantics work.
5. The primitive is reusable and is not sandbox-specific.
6. No docking/Phase-1 behavior is reopened or regressed.
7. QA remains segregated.
8. Existing renderer/GI behavior and stationary stability remain valid.
9. Unit, package, desktop-interaction, external-template, and repository safety gates pass.
10. The exact validated commit is published to `main`.

## 16. Deferred Phase-2 Work

After this slice is proven:

1. Convert Scene Objects to a true hierarchy using the same primitive.
2. Convert Object Details into collapsible property groups.
3. Add search/filter where it provides real value.
4. Migrate renderer/lighting/GI/reflection/shadow/AO/post settings.
5. Extend the primitive for deeper hierarchy, richer selection, drag/reparenting, and other editor workflows only when required by a concrete consumer.
