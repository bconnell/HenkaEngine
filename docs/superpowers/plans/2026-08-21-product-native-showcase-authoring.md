# Product-native showcase authoring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver normal visible Henka authoring that creates, saves, reloads,
re-edits, packages, and renders the final Giraffe and Rocket without an
imported, generator, direct-file, or asset-specific source path.

**Architecture:** A bounded native authoring-document module owns a
project-relative authored asset and scene-connected editable parts. It delegates
all mesh work to the existing authoring-object transaction, owns explicit
material bindings and persistence, and exposes generic editor actions through a
focused UI module rather than adding recipe logic to `main.c`.

**Tech Stack:** C17, MSVC `/W4 /WX`, CMake, Henka authoring mesh/scene/assets,
Sandbox editor, and PowerShell graphical/package harnesses.

**Spec:** `docs/superpowers/specs/2026-08-21-product-native-showcase-authoring.md`

## Global Constraints

- Work in the canonical checkout; do not create, switch, stash, reset, or rebase a worktree.
- Preserve unrelated staged, unstaged, and untracked files exactly.
- Final showcase sources originate only through visible Henka authoring actions.
- No generator, direct HAMS/native-file writer, imported finished mesh, hidden fixture constructor, or asset-specific source path may create a final model.
- New operations are generic, bounded, transactional, undoable, MSVC-warning-clean, and fail closed.
- Imported and ordinary user assets remain unchanged until explicit user action.
- Harness automation may drive editor UI only; it may not write a final authored source file.

---

## File structure

- `examples/sandbox3d/authoring_asset_document.[ch]`: bounded native asset
  document, part ownership, material binding, provenance, and candidate
  save/reload publication.
- `examples/sandbox3d/authoring_asset_ui.[ch]`: visible lifecycle and primitive
  controls; maps UI actions to document operations.
- `examples/sandbox3d/object_authoring_tools.[ch]`: generic source constructors
  and missing transactional component/material adapters only.
- `examples/sandbox3d/main.c`: state ownership and module wiring only.
- `tests/test_sandbox3d_authoring_asset_document.c`: document lifecycle,
  transaction, persistence, and provenance regressions.
- `scripts/*authoring*windows.ps1`: visible-editor evidence only.
- `assets/authored/*`: final editor-created sources only after visible workflow
  creates them.

### Task 1: Establish the native-document contract

**Files:**
- Create: `examples/sandbox3d/authoring_asset_document.h`
- Create: `examples/sandbox3d/authoring_asset_document.c`
- Create: `tests/test_sandbox3d_authoring_asset_document.c`
- Modify: `examples/sandbox3d/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces `sandbox3d_authoring_asset_document_create`,
  `sandbox3d_authoring_asset_document_destroy`,
  `sandbox3d_authoring_asset_document_get_provenance`, and
  `sandbox3d_authoring_asset_document_get_part_count`.
- Uses `sandbox3d_authoring_object` as the only editable mesh owner.

- [ ] **Step 1: Write the failing lifecycle test.**

```c
sandbox3d_authoring_asset_document* document = NULL;
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_create(
    engine, scene, "test_asset", &document) == HENKA_SUCCESS);
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(document) == 0U);
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_provenance(document) ==
    SANDBOX3D_AUTHORING_PROVENANCE_PRODUCT_NATIVE_AUTHORED);
sandbox3d_authoring_asset_document_destroy(document);
```

- [ ] **Step 2: Build `henka_tests` and verify the missing interface fails.**

Run: `MSBuild build/HenkaEngine.slnx /t:henka_tests /p:Configuration=Debug /p:Platform=x64`

Expected: compile failure naming the missing document API.

- [ ] **Step 3: Implement fixed-capacity document creation.**

```c
henka_result sandbox3d_authoring_asset_document_create(
    henka_engine* engine, henka_scene* scene, const char* name,
    sandbox3d_authoring_asset_document** out_document);
```

Validate a nonempty bounded name, initialize the document only after every
field validates, and release all owned authoring objects and bindings in
destruction.

- [ ] **Step 4: Rebuild and run `henka_tests`.**

Expected: lifecycle passes with no first-party warning.

### Task 2: Add generic editable primitive parts

**Files:**
- Modify: `examples/sandbox3d/authoring_asset_document.[ch]`
- Modify: `examples/sandbox3d/object_authoring_tools.[ch]`
- Modify: `tests/test_sandbox3d_authoring_asset_document.c`

**Interfaces:**
- Consumes the Task 1 document.
- Produces `sandbox3d_authoring_asset_document_add_primitive` and generic box,
  plane, cylinder, cone, and UV-sphere kinds.

- [ ] **Step 1: Write failing primitive transaction tests.**

```c
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_add_primitive(
    document, SANDBOX3D_AUTHORING_PRIMITIVE_CYLINDER,
    &(sandbox3d_authoring_primitive_desc){.radius = 0.5f, .height = 2.0f, .segments = 16U},
    &part_index) == HENKA_SUCCESS);
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(document) == 1U);
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_add_primitive(
    document, SANDBOX3D_AUTHORING_PRIMITIVE_CYLINDER,
    &(sandbox3d_authoring_primitive_desc){.radius = 0.0f, .height = 2.0f, .segments = 16U},
    &part_index) != HENKA_SUCCESS);
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(document) == 1U);
```

- [ ] **Step 2: Build `henka_tests` and verify the new API fails.**

Expected: missing enum/API compile failure.

- [ ] **Step 3: Implement candidate-first generic part publication.**

Create the source with an existing bounded mesh constructor, create a temporary
scene entity, bridge it with `sandbox3d_authoring_object_create_from_mesh`, then
append the part only after each operation succeeds. On failure, destroy the
temporary bridge/entity and preserve count and selection.

- [ ] **Step 4: Rebuild and run `henka_tests`.**

Expected: valid primitives become editable; invalid dimensions leave no
entity, history, or part mutation.

### Task 3: Expose normal visible asset creation and remove special controls

**Files:**
- Create: `examples/sandbox3d/authoring_asset_ui.h`
- Create: `examples/sandbox3d/authoring_asset_ui.c`
- Modify: `examples/sandbox3d/main.c`
- Modify: `examples/sandbox3d/CMakeLists.txt`
- Modify: `tests/test_sandbox3d_editor_ui.c`
- Modify: `tests/test_sandbox3d_authoring_asset_document.c`

**Interfaces:**
- Produces `sandbox3d_authoring_asset_ui_handle_action` for New Asset, Add
  Primitive, Select Part, Save Asset, Reload Asset, Undo, and Redo.

- [ ] **Step 1: Write failing UI-routing tests.**

```c
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_ui_handle_action(
    &ui, SANDBOX3D_AUTHORING_ASSET_UI_NEW_ASSET) == HENKA_SUCCESS);
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_ui_handle_action(
    &ui, SANDBOX3D_AUTHORING_ASSET_UI_ADD_UV_SPHERE) == HENKA_SUCCESS);
HENKA_TEST_ASSERT(!sandbox3d_editor_ui_has_action(&ui_state, "Create Native Rocket"));
```

- [ ] **Step 2: Build `henka_tests` and verify action routing fails.**

Expected: missing UI interface or surviving special Rocket action.

- [ ] **Step 3: Implement UI delegation.**

Place generic controls in the current modeling workflow and report current
success/failure feedback. Move only state wiring into `main.c`; do not add
construction, source writing, or asset names there. Remove the special Rocket
action and its execution path.

- [ ] **Step 4: Rebuild and run `henka_tests`.**

Expected: generic controls create only document parts, with no asset identity in
authoring creation code.

### Task 4: Persist native materials, UVs, and sources atomically

**Files:**
- Modify: `examples/sandbox3d/authoring_asset_document.[ch]`
- Modify: `examples/sandbox3d/object_authoring_tools.[ch]`
- Modify: `examples/sandbox3d/authoring_asset_ui.c`
- Modify: `tests/test_sandbox3d_authoring_asset_document.c`
- Modify: `tests/test_sandbox3d_object_authoring.c`

**Interfaces:**
- Produces document-level `assign_material_region`, `save`, `reload`, `undo`,
  and `redo` operations.

- [ ] **Step 1: Write failing round-trip and atomic-failure tests.**

```c
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_assign_material_region(
    document, part_index, 0U, &material) == HENKA_SUCCESS);
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_save(document, source_path) == HENKA_SUCCESS);
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_reload(document, source_path) == HENKA_SUCCESS);
HENKA_TEST_ASSERT(sandbox3d_authoring_object_extrude_selected_face(part, 0.125f) == HENKA_SUCCESS);
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_reload(document, missing_path) != HENKA_SUCCESS);
HENKA_TEST_ASSERT(sandbox3d_authoring_asset_document_get_part_count(document) == expected_part_count);
```

- [ ] **Step 2: Build `henka_tests` and verify persistence behavior fails.**

Expected: missing persistence API or failing round-trip assertion.

- [ ] **Step 3: Implement candidate load and atomic replace.**

Serialize project-relative normalized paths, finite material values, bounded
part/material counts, mesh source data, UVs, and provenance. Write a temporary
sibling, validate fully, then replace the destination. Reload into a candidate
document and swap only after scene publication succeeds.

- [ ] **Step 4: Rebuild and run `henka_tests`.**

Expected: material/UV data and a second post-reload edit survive; malformed or
missing sources cannot partially alter the live document.

### Task 5: Close only demonstrated generic modeling gaps

**Files:**
- Modify only the generic engine/Sandbox source and direct test identified by a
  failed normal-editor operation.

**Interfaces:**
- Consumes a document part and its visible component selection.
- Produces a generic command with an authoritative transaction result.

- [ ] **Step 1: Record a concrete failed normal-editor operation on a non-showcase exercise mesh.**

Record selection mode, mesh topology, command, result, and generic expected
result. Do not add an operation only because it appears in a feature list.

- [ ] **Step 2: Add a direct failing regression preserving source and selection on failure.**

```c
before = henka_authoring_mesh_get_counts(mesh);
HENKA_TEST_ASSERT(generic_operation(object, selection) != HENKA_SUCCESS);
HENKA_TEST_ASSERT(henka_authoring_mesh_get_counts(mesh).faces == before.faces);
```

- [ ] **Step 3: Implement the bounded candidate operation through the current transaction owner.**

Validate finite parameters, capacity, topology, and selection remapping before
publication. Publish scene, render mesh, bounds, physics, and history once.

- [ ] **Step 4: Run the direct regression and `henka_tests`.**

Expected: success is undoable/redoable and failure leaves source, selection, and
rendered entity unchanged.

### Task 6: Author both assets through visible Henka controls

**Files:**
- Create: `assets/authored/giraffe/*` only through the editor workflow
- Create: `assets/authored/rocket/*` only through the editor workflow
- Modify: visible harness scripts and tests only to drive/verify editor actions

**Interfaces:**
- Produces two `HENKA_PRODUCT_NATIVE_AUTHORED` documents.

- [ ] **Step 1: Add a failing harness gate rejecting non-editor provenance.**

```powershell
if ($evidence.AuthoringPath -ne 'VisibleEditorActions' -or
    $asset.Provenance -ne 'HENKA_PRODUCT_NATIVE_AUTHORED') {
    throw 'Showcase source lacks product-native authoring provenance.'
}
```

- [ ] **Step 2: Run the harness and confirm fixture/import sources fail.**

Expected: the previous generated/imported showcase path is rejected.

- [ ] **Step 3: Author each asset using only normal UI controls.**

Create the document, refine primitive source geometry through component editing,
assign material/UV/shading data, save, release/reopen, make a second edit, and
save again. A needed new feature returns to Task 5 first.

- [ ] **Step 4: Run save/reload/re-edit and package evidence.**

Expected: geometry, material, UV, shading, and provenance survive in editor and
package.

### Task 7: Validate visual quality and update public truth

**Files:**
- Modify: `docs/showcase-assets.md`, `README.md`, affected help/QA/build docs
- Modify: package and visual-evidence checks

**Interfaces:**
- Consumes Task 6 authored documents and packaged output.

- [ ] **Step 1: Add failing final-provenance, package-inclusion, and render-evidence checks.**

```powershell
Assert-Equal $manifest.giraffe.provenance 'HENKA_PRODUCT_NATIVE_AUTHORED'
Assert-Equal $manifest.rocket.provenance 'HENKA_PRODUCT_NATIVE_AUTHORED'
Assert-True (Test-Path $packagedGiraffeSource)
Assert-True (Test-Path $packagedRocketSource)
```

- [ ] **Step 2: Run checks and confirm old fixture labels fail final acceptance.**

Expected: generated fixture provenance or absent native sources fails.

- [ ] **Step 3: Update package, evidence checks, and documentation after current proof exists.**

Document the reproducible workflow, user-asset protection, remaining limits, and
human visual-review status. Keep separate test-fixture labels truthful.

- [ ] **Step 4: Run focused gates, full CTest, package smoke, visual capture, and manual multi-angle review.**

Expected: package resolves both authored assets and visual review finds no
low-fidelity geometry, crude primitive assembly, broken normals, or visibly unfinished
material/shading.

## Self-review

- The plan assigns generic creation, visible discoverability, transactional
  topology, material/UV ownership, persistence, packaging, editor-only proof,
  human visual QA, and truthful documentation to explicit tasks.
- No task creates finished showcase geometry outside normal editor controls.
- Task 5 requires a demonstrated generic failure and a test before adding any
  modeling operation.
