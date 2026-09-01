# Henka Engine UI Modernization Phase 2 - Slice 1 Design

## 1. Objective

Modernize the first high-value editor surfaces with reusable progressive-disclosure and property-group infrastructure.

Slice 1 covers two consumers of the new core UI primitive:

1. Controls/Main, which currently presents too many permanently visible buttons.
2. Object Details, which currently hides useful material information in compact layouts and exposes editable material authoring only through a special imported-marker path.

The slice also corrects the visible Scene View separator so it presents as one thin line while preserving the existing drag target and docking behavior.

## 2. Current Product Problems

### 2.1 Controls/Main

Controls/Main uses a dense fixed button matrix. At the packaged default layout, several labels lose important text because the available width is too small. The panel exposes workspace, viewer, viewport, tool, and debug state at the same visual level.

The updated layout must reduce first-glance density and preserve full command identity at the validated default panel width.

### 2.2 Object Details and Materials

Object Details currently forces a compact layout. That compact path suppresses the normal Material value row. A separate material-authoring block is placed at a fixed vertical offset near the bottom of the panel.

The material-authoring block also uses a special capability gate tied to the imported glTF Marker. Other selected objects can have real scene materials while the inspector still tells the user to select the marker.

Selection and material inspection must follow the selected scene entity.

### 2.3 Scene View Separator

The current vertical separator can read as a wide or doubled gutter because visible borders and the splitter presentation occupy the same area.

The drag target is useful and must remain easy to hit. The visible chrome must be reduced to one thin separator line.

## 3. Scope

### 3.1 In Scope

1. Add reusable disclosure, tree-row, and property-group support to core `henka_ui`.
2. Add deterministic keyboard focus and disclosure navigation.
3. Extend the UI frame descriptor with Up, Down, Left, Right, and Enter pressed state.
4. Add bounded flowing content support for disclosure/property consumers.
5. Convert Controls/Main into four persistent disclosure groups:
   - Workspace
   - Viewer
   - Viewport
   - Viewport Tool
6. Convert Object Details to flowing disclosure/property groups.
7. Make the selected entity's material visible through Object Details whenever the selected entity has a scene material.
8. Preserve editable material-instance controls when the selected material path supports safe editing.
9. Present unsupported or read-only material paths clearly without hiding their material information.
10. Remove fixed-position material-authoring layout from Object Details.
11. Correct the Scene View separator's visible presentation without changing its docking or drag behavior.
12. Extend unit, sandbox, and packaged desktop validation for these behaviors.
13. Update user-facing documentation when the interaction workflow changes.

### 3.2 Explicitly Out of Scope

- Docking topology, redocking rules, detached-window ownership, workspace preset semantics, or splitter geometry.
- Scene Objects hierarchy conversion.
- New runtime support for multiple materials per mesh or submesh.
- A new material-file format or second material authority.
- General asset-browser conversion.
- Renderer settings conversion.
- Search/filter UI.
- Drag-and-drop hierarchy editing.
- Multi-select hierarchy behavior.
- Full icon-system redesign.
- New renderer, lighting, or GI behavior.
- Global replacement of every existing button.
- Broad material-authoring support that exceeds existing safe material-instance contracts.

## 4. Core UI Architecture

The disclosure/property primitive belongs in the core Henka UI layer. Sandbox panels consume that primitive through stable public or internal UI contracts that match existing Henka naming conventions.

Conceptual responsibilities:

```text
tree_begin()
tree_node()
tree_leaf()
property_group()
property_row()
tree_end()
```

The implementation may use different C function names. The behavior and ownership rules in this design are binding.

### 4.1 Transient UI State

The UI context owns:

- focused disclosure-row ID;
- active mouse-row ID;
- visible disclosure-row order for the current frame;
- indentation depth;
- bounded content/clip state;
- per-frame keyboard navigation intent;
- flowing content cursor state required by the consumer currently being drawn.

### 4.2 Application-Owned State

The sandbox owns:

- Controls/Main expansion values;
- Object Details expansion values;
- user settings;
- selected scene entity;
- material edit capability for the selected entity;
- application actions triggered by child controls.

Expansion state remains explicit application data so it can persist across process restarts.

### 4.3 Stable IDs

Disclosure headers and interactive child rows require stable IDs. IDs cannot depend on visible row index, transient addresses, stack buffers, or changing layout positions.

Controls/Main group IDs:

```text
controls.main.workspace
controls.main.viewer
controls.main.viewport
controls.main.viewport_tool
```

Object Details group IDs:

```text
object_details.overview
object_details.transform
object_details.materials
object_details.physics
object_details.interaction
object_details.actions
```

## 5. Controls/Main Information Architecture

Default expansion state:

| Group | Default |
|---|---|
| Workspace | Collapsed |
| Viewer | Expanded |
| Viewport | Expanded |
| Viewport Tool | Collapsed |

Persisted setting keys:

```text
ui.controls.main.workspace.expanded
ui.controls.main.viewer.expanded
ui.controls.main.viewport.expanded
ui.controls.main.viewport_tool.expanded
```

### 5.1 Workspace

Contains:

- View / Inspect / Full Tools workspace selection;
- named workspace presets;
- Save Custom / Restore Custom;
- custom layout slot save/restore;
- Undo Layout / Redo Layout.

Workspace is collapsed by default.

Commands use full-width or width-safe rows. The UI must not shorten a command label until its meaning becomes unclear.

### 5.2 Viewer

Contains Grid visibility.

Grid remains a state control. Viewer is expanded by default.

### 5.3 Viewport

Contains:

- Frame Selected;
- Reset View;
- Zoom In;
- Zoom Out.

These remain command controls. Viewport is expanded by default.

### 5.4 Viewport Tool

Contains:

- Select / Orbit / Pan;
- Move / Rotate / Scale;
- Snap;
- Handle hit-box visibility;
- Reflection Probe Volume visibility.

Exclusive tool choices use selector-style controls. Boolean state uses switches or toggles. QA-only controls stay on the QA page.

Viewport Tool is collapsed by default.

## 6. Object Details Information Architecture

Object Details becomes the second consumer of the same disclosure/property infrastructure.

Default expansion state:

| Group | Default |
|---|---|
| Overview | Expanded |
| Transform | Expanded |
| Materials | Expanded |
| Physics | Collapsed |
| Interaction | Collapsed |
| Actions | Collapsed |

Persisted setting keys:

```text
ui.object_details.overview.expanded
ui.object_details.transform.expanded
ui.object_details.materials.expanded
ui.object_details.physics.expanded
ui.object_details.interaction.expanded
ui.object_details.actions.expanded
```

The expansion state is global inspector preference state. It does not need a separate copy for every entity.

### 6.1 Selection Contract

Viewport selection and Scene Objects selection feed the same selected-entity state.

When the selected entity changes, Object Details reads the new entity on the next frame. Stale material, transform, physics, or interaction data from the previous entity must not remain visible.

An invalid or cleared selection shows the existing no-selection guidance and no interactive material editor.

### 6.2 Overview

Shows the selected object's display name and high-level state such as visibility and the short explanation already exposed by the sandbox.

### 6.3 Transform

Shows Position, Rotation, and Scale.

Transform lock state remains visible. Existing safe transform actions remain available under Actions.

### 6.4 Materials

Materials is always present for a valid selection.

The current scene model exposes one effective material per selected entity. Slice 1 presents that material as `Material 0`. The UI shape may support future material slots, but this slice does not add a new runtime multi-material model.

For a selected entity with a material, the Materials group shows:

- material identity or descriptive name when available;
- effective base color;
- metallic;
- roughness;
- normal-map state or dependency when available;
- emissive state;
- alpha mode;
- double-sided state;
- other currently supported material properties that can be represented truthfully;
- material dependency/revision information when the existing asset path exposes it.

If the selected entity has no material, the group shows `None`.

### 6.5 Material Edit Capability

Material visibility is based on the selected scene entity. It is not gated by a specific hard-coded marker entity.

A small sandbox-side material adapter determines whether the selected material is:

1. inspectable and editable through an existing safe material-instance contract;
2. inspectable but read-only;
3. unavailable.

Editable paths expose the existing transactional controls in the Materials group.

Read-only paths still show the effective material values and a clear `Read-only` state. Built-in procedural materials may remain read-only in this slice if no safe instance-edit contract exists for them.

The imported glTF Marker keeps its existing material-instance functionality, but its editor appears because the selected material reports edit capability. The panel does not use marker identity as its visibility rule.

Material edits keep existing transactional behavior. A rejected edit leaves the previous effective material intact and reports the failure through the established status path.

### 6.6 Physics

Shows current physics body type and velocity when the selected entity has a linked body. Entities without a physics body show the existing truthful empty state.

### 6.7 Interaction

Shows the current interaction availability, prompt, and range state already exposed by the sandbox.

### 6.8 Actions

Contains existing selected-object commands such as:

- Hide / Show Object;
- Focus Camera;
- Lock / Unlock Transform;
- Reset Transform;
- Clear Selection.

These are commands and remain buttons.

## 7. Flowing Content, Clipping, and Scrolling

Controls/Main and Object Details must stop relying on fixed absolute vertical offsets for expanding content.

Each group advances a local content cursor by the height it actually consumes. Child controls are positioned from that cursor. Expansion and collapse update content height deterministically.

Requirements:

- no fixed `panel_bounds.y + constant` position for the material editor;
- material rows stay reachable at the validated packaged default layout;
- rows outside the visible content rectangle are not interactive;
- clipped content cannot claim mouse input;
- content can scroll when the expanded groups exceed panel height;
- adjacent dock geometry does not change because a group expands;
- important command and property labels remain readable at the validated default panel width.

The implementation should reuse existing panel clipping and scrolling behavior where it is already correct. New support must stay inside the UI layer and cannot create a second panel-layout system.

## 8. Mouse and Keyboard Interaction

### 8.1 Mouse

A disclosure header has one primary hit target covering its row.

A completed left-click on that row:

1. focuses the row;
2. toggles its expanded state.

Collapsed children are not drawn as interactive rows and cannot request mouse ownership.

Existing buttons, switches, selectors, and material controls keep their established press/release behavior.

### 8.2 Keyboard

Keyboard disclosure navigation is active only when the UI is visible and a disclosure row owns UI focus.

- Up: move to the previous visible disclosure header.
- Down: move to the next visible disclosure header.
- Left: collapse the focused expanded group.
- Right: expand the focused collapsed group.
- Enter: toggle the focused group.

Keys consumed by the UI cannot also trigger sandbox editor actions in the same frame.

Tab navigation across arbitrary controls remains outside Slice 1.

### 8.3 Focus

Focused disclosure rows receive a visible focus treatment.

If a focused row is not drawn in the next frame, focus clears safely.

Focus IDs cannot point to transient caller memory.

## 9. UI Frame Input

The UI frame descriptor gains only the pressed-key state required by this slice:

- Up;
- Down;
- Left;
- Right;
- Enter.

The sandbox fills those fields before `henka_ui_begin_frame`.

Detached-panel frame descriptors initialize the new fields safely even when no navigation key is active.

Core UI code does not query sandbox input directly.

## 10. Persistence

Controls/Main and Object Details expansion state use the existing sandbox settings path.

Startup:

1. initialize documented defaults;
2. load settings;
3. apply each valid saved boolean;
4. render the resulting state.

Interaction:

1. update application expansion state immediately;
2. update the associated settings value;
3. let the established settings workflow persist it.

Missing or malformed values fall back to documented defaults.

Selection, current material values, and temporary material-edit state are not stored as disclosure preferences.

## 11. Scene View Separator Presentation

This slice treats the wide/doubled separator as a bounded visual defect.

The existing splitter interaction zone remains intact. Docking calculations, drag thresholds, redocking, panel ordering, and detached-window behavior do not change.

Presentation requirements:

- draw one thin visible separator centered in the existing splitter interaction zone;
- avoid a second parallel border from adjacent panel chrome inside the separator zone;
- keep the larger invisible drag target so resize remains easy;
- preserve current cursor/drag behavior;
- preserve the current panel bounds on both sides.

The separator keeps its existing larger drag target while presenting as one thin visible line.

## 12. Error Handling and Invariants

Core UI behavior must fail closed.

Required invariants:

- null UI context returns the established invalid result;
- null or empty stable IDs are rejected;
- zero or negative row bounds do not create hit targets;
- nesting depth cannot underflow;
- unmatched tree/property end calls cannot corrupt later UI state;
- hidden or clipped rows cannot activate;
- duplicate visible IDs cannot create ambiguous activation;
- allocation failure cannot leave partial interaction state;
- expansion values remain application-owned;
- selected-entity material reads fail safely;
- failed material edits preserve the previous effective material;
- clearing or invalidating selection clears inspector material-edit capability immediately.

## 13. Verification scenarios

Production behavior changes start with focused failing tests.

### 13.1 Core UI Unit Tests

Add tests for:

- disclosure-row rendering;
- mouse toggle and focus;
- child visibility while expanded;
- child suppression while collapsed;
- deterministic indentation;
- flowing cursor advancement;
- Up/Down traversal;
- Left collapse;
- Right expand;
- Enter toggle;
- clipping and hidden-row input suppression;
- focus clearing when a row disappears;
- invalid arguments;
- duplicate-ID handling;
- existing button, toggle, selectable, tab, overlay, and text regressions.

### 13.2 Sandbox Tests

Add focused tests for:

- Controls/Main defaults;
- Object Details defaults;
- settings round-trip;
- malformed setting fallback;
- expansion state independent of docking state;
- selected entity drives material inspection;
- entity change cannot retain the previous material view;
- entity without a material reports `None`;
- read-only material remains inspectable;
- editable material-instance path retains transactional failure behavior.

### 13.3 Separator Tests

Add the narrowest useful test around separator presentation data or geometry if the current UI layer exposes it deterministically.

Do not duplicate the desktop harness with a second visual-testing framework.

## 14. Runtime validation scenarios

Extend the existing packaged desktop harness.

Required interaction coverage:

1. Controls/Main shows all four group headers.
2. Workspace and Viewport Tool start collapsed on a clean preference state.
3. Viewer and Viewport start expanded.
4. A group can expand and collapse by mouse.
5. Saved group state survives a clean restart.
6. Controls/Main command labels remain understandable at the validated default panel width.
7. Selecting Ground exposes its Materials group and effective material state.
8. Selecting Textured Cube exposes its Materials group and effective material state.
9. Selecting Material Ball exposes its Materials group and effective material state.
10. Selecting glTF Marker exposes its Materials group and editable material-instance controls.
11. The same material-inspection result is reachable from viewport selection and Scene Objects selection.
12. Clearing selection removes selected-object material controls.
13. A read-only material is clearly identified and remains inspectable.
14. An editable marker material change follows the existing transactional behavior.
15. Object Details remains usable when its groups exceed the visible panel height.
16. The Scene View separator presents as one thin visible line.
17. The separator remains draggable through its existing larger hit target.
18. Existing Controls context menu still works.
19. Existing QA tab remains reachable.
20. Existing Grid workflow remains valid.
21. Native/detached panel validation still passes.
22. Stationary Rendered viewport stability remains inside the established threshold.

The existing viewport stability threshold cannot be relaxed for this UI work.

## 15. Design outcomes

Slice 1 is complete when:

1. Controls/Main is materially less dense at first glance.
2. Its four groups use the approved defaults and persist across restart.
3. Important Controls labels remain understandable at the validated default width.
4. Object Details uses flowing disclosure/property groups.
5. Selecting a material-bearing object exposes its material in Object Details.
6. Ground, Textured Cube, Material Ball, and glTF Marker all expose truthful material state.
7. Material visibility follows selected-entity capability and is not tied to marker identity.
8. Existing safe editable material-instance behavior remains transactional.
9. Read-only materials remain visible and are clearly identified.
10. Material content stays reachable in constrained panel height.
11. Mouse and required keyboard disclosure semantics work.
12. The Scene View separator presents as one thin line and remains easy to drag.
13. Docking and Phase 1 workspace semantics are unchanged.
14. QA surfaces remain separately reachable from the main editing flow.

## 16. Future extensions

After this slice is proven:

1. Convert Scene Objects to a true hierarchy using the same core primitive.
2. Extend Object Details with richer editor property controls where existing engine contracts support them safely.
3. Add search/filter where it provides clear value.
4. Migrate renderer, lighting, GI, reflection, shadow, AO, and post settings.
5. Add deeper hierarchy selection and drag/reparent behavior when a concrete consumer requires it.
6. Add runtime multi-material or submesh material-slot support only through a separate engine design when the scene and mesh contracts are ready for it.
