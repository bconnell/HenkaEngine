# Professional Authoring and Topology Campaign Implementation Plan

**Goal:** Turn the existing Henka authoring/editor foundation into a connected, professional Build/Game/World workspace with scalable selection, topology analysis and repair, practical modeling operations, truthful showcase currentness, and real external-project validation.

**Architecture:** Preserve the current context-shell and existing engine ownership boundaries. Deliver vertical slices in dependency order: first stabilize the current workspace continuation, then move component selection and topology responsibilities into bounded engine/authoring APIs, then connect modeling, UI, persistence, showcase, and external-game validation. Ordinary user/imported topology remains analysis-only unless the user explicitly invokes a repair command; automatic refresh is limited to explicit repository-owned fixtures at the Sandbox boundary.

**Tech Stack:** C17 public and engine APIs, existing SDL/OpenGL boundary, existing Henka UI primitives, CMake/CTest, Windows PowerShell 5.1 harnesses, checked arithmetic helpers, existing HAMS persistence, and repository-local showcase assets.

**Spec:** User-provided “HENKA ENGINE — PROFESSIONAL AUTHORING, TOPOLOGY, EDITOR UX, SHOWCASE CURRENTNESS, AND SHADOWED REMAINS DOGFOOD CAMPAIGN” specification.

## Global Constraints

- Preserve current local context-shell v1.0.3 work in `examples/sandbox3d/main.c`, `examples/sandbox3d/workspace_tools.c`, `examples/sandbox3d/workspace_tools.h`, and `tests/test_sandbox3d_workspace.c`.
- Do not reset, clean, stash, rebase, force-push, or discard staged, unstaged, untracked, or repository-owned fixture work.
- Keep public APIs C17 and keep any C++ dependency behind an internal C ABI.
- Use checked arithmetic for counts, allocations, offsets, sizes, buffer ranges, and file operations.
- Keep replacement transactional: the previous valid resource remains authoritative until the candidate validates and publishes.
- Never automatically rewrite ordinary user, imported-user, Shadowed Remains, or Highlands Hardware topology.
- Only explicitly authorized repository-owned showcase fixtures may be automatically refreshed.
- Do not add unlicensed dependencies, icons, fonts, models, textures, or other media.
- Do not expose private prompts, local evidence paths, user-private game data, or agent-role language in public source, documentation, UI, or commits.
- Do not claim visual or manual QA from source tests, screenshots, or structural harness output alone.
- Keep generated builds, packages, logs, screenshots, and temporary evidence in bounded repository-local output locations and out of commits.

---

### Task 1: Preserve and close the context-shell v1.0.3 slice

**Files:**
- Modify: `examples/sandbox3d/main.c` only where the current context-shell behavior is incomplete or contradictory.
- Modify: `examples/sandbox3d/workspace_tools.c` and `examples/sandbox3d/workspace_tools.h` only for the context/layout contract.
- Test: `tests/test_sandbox3d_workspace.c`.
- Check: `docs/` workspace and editor documentation that describes the current shell.

**Interfaces:**
- Consumes the existing `sandbox3d_workspace_context_state` and `sandbox3d_workspace_model` APIs.
- Produces a stable Build/Game/World context contract whose context changes promotions and inspector groups without changing saved dock topology or panel geometry.

- [ ] Record the current dirty paths, HEAD, origin/main, staged state, untracked state, active Git operation state, and current build/provenance files before editing.
- [ ] Trace every reader and writer of `workspace.context`, `layout_mode`, `tools_panel_visible`, `active_utility`, and `debug_hud_visible`.
- [ ] Add or strengthen tests proving context changes do not mutate layout topology, Focus Viewport is temporary, startup restores Standard, Debug is hidden by default, and invalid persisted context values fail closed to Build.
- [ ] Fix only the smallest source defects exposed by those tests, preserving the existing local implementation.
- [ ] Run the focused workspace test target and inspect warnings and diagnostics.
- [ ] Run `git diff --check`, review exact changed paths, and commit only when the slice is validated.

### Task 2: Add reusable responsive editor UI primitives

**Files:**
- Inspect and modify: `engine/include/henka/ui.h`, `engine/src/ui/ui.c`, and `engine/src/ui/ui_internal.h`.
- Create if the existing UI boundary has no equivalent: `engine/include/henka/ui_icons.h` and `engine/src/ui/ui_icons.c`.
- Create: `examples/sandbox3d/editor_layout.h` and `examples/sandbox3d/editor_layout.c`.
- Modify: `examples/sandbox3d/main.c` to consume the layout policy rather than inventing new rectangle formulas.
- Test: `tests/test_ui.c`, `tests/test_sandbox3d_editor_ui.c`, and a new focused layout test only if the existing test organization cannot cover breakpoints.

**Interfaces:**
- Produces engine-owned icon drawing, descriptor-based tool buttons, and an editor layout policy with wide/medium/narrow breakpoints.
- Consumers use existing `henka_ui_context` and `henka_ui_rect` contracts; no renderer or SDL types cross the public UI boundary.

- [ ] Map existing button, tab, disclosure, tooltip, and text primitives before adding new API.
- [ ] Write failing tests for icon enumeration stability, disabled-state behavior, tooltip identity, and layout metrics at 1024x768, 1280x720, 1600x900, and 1920x1080.
- [ ] Implement only built-in line/rectangle/polyline icons required by the Build workflow.
- [ ] Implement the tool-button descriptor and centralized layout metrics with checked dimensions and minimum hit targets.
- [ ] Replace one representative hard-coded toolbar group with the new primitives and keep the old behavior reachable.
- [ ] Run UI and workspace tests, then perform packaged runtime inspection for clipping, overlap, hover, selected, disabled, and keyboard-focus states.

### Task 3: Replace the 64-component selection ceiling

**Files:**
- Inspect and modify: `examples/sandbox3d/object_authoring_tools.h` and `examples/sandbox3d/object_authoring_tools.c`.
- Inspect and modify: `examples/sandbox3d/interaction_tools.h` and `examples/sandbox3d/interaction_tools.c`.
- Modify: `examples/sandbox3d/main.c` only at selection ownership, overlay, and operation call sites.
- Test: `tests/test_sandbox3d_object_authoring.c`, `tests/test_sandbox3d_interaction.c`, and the authoring mesh tests.

**Interfaces:**
- Produces per-component checked bitsets, ascending deterministic iteration, active-component tracking, and transactional resize behavior.
- Existing selection consumers continue to ask the authoring object for selected component counts and IDs rather than accessing storage directly.

- [ ] Identify the existing fixed selection arrays, all producers, all consumers, and topology mutation remapping points.
- [ ] Add failing tests selecting, clearing, iterating, growing, and connected-selecting more than 64 vertices, edges, and faces.
- [ ] Implement bitset allocation using checked `(capacity + 63) / 64` arithmetic and preserve the old selection on allocation failure.
- [ ] Implement stable ascending iteration with active-component-first ordering only where an operation explicitly requires it.
- [ ] Add bounded traversal storage sized from the actual mesh capacity for Grow, Connected, Loop, and Ring operations.
- [ ] Update overlays and mutation remapping to use the new storage and prove stale IDs are removed after topology changes.
- [ ] Run focused tests and the affected critical suite.

### Task 4: Establish generic topology analysis and transactional repair

**Files:**
- Modify: `engine/include/henka/authoring_topology.h` and `engine/src/mesh/authoring_topology.c`.
- Inspect and modify: `engine/include/henka/authoring_mesh.h` and `engine/src/mesh/authoring_mesh.c` where cloning, validation, or adjacency contracts are required.
- Test: `tests/test_authoring_topology.c`, `tests/test_authoring_quad_recovery.c`, and new deterministic topology fixtures under `tests/fixtures/` if required.

**Interfaces:**
- Produces `HENKA_AUTHORING_TOPOLOGY_POLICY_VERSION`, profile-aware repair options, a repair report, and `henka_authoring_mesh_repair_topology(...)`.
- Keeps `henka_authoring_mesh_recover_quads(...)` as a lower-level primitive rather than presenting it as complete retopology.

- [ ] Map current stable vertex, edge, face, adjacency, material, UV, hard-edge, and validation ownership.
- [ ] Add failing tests for analysis-only user content, candidate rollback, invalid options, bounded pass counts, deterministic output, and idempotence.
- [ ] Implement candidate clone, pre-analysis, bounded structural passes, validation after each pass, final analysis, integrity comparison, and publish-on-success.
- [ ] Implement read-only topology metrics for coincident vertices, duplicate/degenerate faces, winding, manifold state, boundaries, seams, hard edges, valence, poles, and strip continuity.
- [ ] Ensure repair rejects unsafe welds, duplicate faces, degeneracy, seam/material/hard-edge destruction, invalid winding, and nonmanifold results.
- [ ] Keep normal user content on the analysis-only path and require an explicit repair command for all topology mutations.
- [ ] Run focused topology tests, invalid-input tests, rollback tests, determinism tests, and repeated repair/idempotence tests.

### Task 5: Improve deterministic topology reconstruction

**Files:**
- Modify: `engine/src/mesh/authoring_topology.c` and its public header only through the Task 4 contract.
- Test: `tests/test_authoring_quad_recovery.c`, topology fixture tests, and topology report tooling.

**Interfaces:**
- Consumes stable IDs and adjacency from Task 4.
- Produces deterministic spatial-hash welding candidates, canonical duplicate-face signatures, triangle-dual candidates, quality scoring, bounded local replacement/augmenting improvements, and guarded edge-flip evaluation.

- [ ] Add fixtures that distinguish valid coincident geometry from duplicate or unsafe geometry.
- [ ] Add failing metrics for quad ratio alone being insufficient; include manifold state, seams, hard edges, valence, poles, strip continuity, and boundary preservation.
- [ ] Implement deterministic bucket ordering and stable-ID tie-breaking; never allow hash iteration order to determine output.
- [ ] Implement legal triangle-pair candidate generation and score terms for continuity, shape, area balance, directional flow, valence, UV distortion, poles, and profile preference.
- [ ] Implement bounded local replacement, short augmenting-path improvement, strip reconsideration, and edge-flip reevaluation with adjacency rebuilds.
- [ ] Add organic and hard-surface fixtures and assert deterministic, idempotent, intent-preserving results.
- [ ] Inspect Giraffe and Rocket cages only after generic fixtures pass; do not special-case fixture names in core topology.

### Task 6: Add practical modeling operations and authoritative undo/redo

**Files:**
- Modify: `engine/include/henka/authoring_modeling.h` and `engine/src/mesh/authoring_modeling.c`.
- Modify: `examples/sandbox3d/object_authoring_tools.c`, `examples/sandbox3d/interaction_tools.c`, and `examples/sandbox3d/main.c`.
- Test: `tests/test_sandbox3d_object_authoring.c`, `tests/test_sandbox3d_interaction.c`, and authoring model tests.

**Interfaces:**
- Produces validated vertex, edge, and face operations through the existing authoring object and one authoritative history path.
- Operations update selection remapping, local bounds, render mesh, collider, and persistence through existing ownership seams.

- [ ] Add failing tests for vertex move/scale/grow/connected/merge/dissolve, edge loop/ring/bevel/subdivide/extrude/dissolve, and face normal move/extrude/inset/bevel/subdivide/delete/flip.
- [ ] Implement the smallest complete operation group with candidate validation and failure-retaining rollback.
- [ ] Route every successful operation through the authoritative undo/redo history and clear redo only after a committed new edit.
- [ ] Rebuild dependent render, bounds, and collider state transactionally and preserve the previous valid state on failure.
- [ ] Add save, close/reload, and continued-editing tests for representative edits.

### Task 7: Connect the Build inspector and contextual workflow

**Files:**
- Modify: `examples/sandbox3d/main.c`, `examples/sandbox3d/editor_ui_state.h`, and `examples/sandbox3d/editor_ui_state.c`.
- Modify: `examples/sandbox3d/workspace_tools.c` and `examples/sandbox3d/workspace_tools.h` for reusable panel grouping only.
- Test: `tests/test_sandbox3d_editor_ui.c`, `tests/test_sandbox3d_workspace.c`, and affected authoring tests.

**Interfaces:**
- Consumes context, selection, modeling, topology, and history APIs.
- Produces truthful Object/Vertex/Edge/Face mode presentation, contextual inspector groups, non-dead controls, and visible disabled reasons.

- [ ] Replace permanent Controls-first presentation with contextual Build groups while keeping secondary tools intentional and hidden by default.
- [ ] Remove ambiguous single-letter labels and generic reorder arrows; use disclosure chevrons, drag grips, header dragging, overflow menus, and real resize hit targets.
- [ ] Bind visible state to authoritative mode, selection, active tool, history, and topology-analysis state without hidden overrides.
- [ ] Run responsive runtime checks at all required sizes, detached/redocked layouts, Focus Viewport entry/exit, and narrow docks.

### Task 8: Implement controlled showcase currentness and provenance

**Files:**
- Modify: `examples/sandbox3d/main.c` and `examples/sandbox3d/object_authoring_tools.c` at the Sandbox authority boundary.
- Modify: `engine/src/mesh/authoring_mesh.c` only if explicit source-authority metadata is required by the persisted format.
- Modify: `docs/showcase-assets.md`, `README.md`, and currentness scripts.
- Test: showcase/currentness tests and visible packaged capture workflow.

**Interfaces:**
- Produces explicit source-authority classification for user, imported-user, repository-owned showcase, and generated test fixtures.
- Produces currentness checks that analyze ordinary content without mutation and refresh only authorized Giraffe/Rocket fixtures through visible Henka commands.

- [ ] Replace mutable self-certifying provenance booleans with metadata derived from the controlled fixture/project boundary and persisted operation evidence.
- [ ] Add negative tests proving ordinary imported assets, Shadowed Remains assets, and Highlands assets are not automatically rewritten.
- [ ] Dogfood Giraffe and Rocket through visible load, analyze, controlled repair, cage inspection, save, reload, close, relaunch, and continued editing.
- [ ] Record before/after topology metrics, policy version, determinism, idempotence, and exact package/source identity.
- [ ] Update public documentation to state only behavior actually demonstrated.

### Task 9: Establish real Shadowed Remains integration and Highlands reference gates

**Files:**
- Modify only the separate Shadowed Remains repository for its compatibility/currentness record when that repository is uniquely identified and explicitly in scope.
- Modify Henka only for generic external-project/template validation.
- Test: `scripts/test_external_game_template_windows.ps1` and the private actual-game integration harness outside the public Henka repository.
- Reference: the exact user-supplied Highlands direct-front image; do not copy it into Henka.

**Interfaces:**
- Produces a truthful external consumer gate for Henka contracts and a separate actual-game gate for Shadowed Remains.

- [ ] Identify the existing Shadowed Remains repository through local source evidence without creating a replacement or guessing among ambiguous candidates.
- [ ] Record game repository branch, HEAD, remote, dirty state, and applicable instructions before any external-repository write.
- [ ] Run the generic template gate for public API compatibility.
- [ ] When relevant engine contracts change, configure/build/load/launch the real game and verify the affected renderer, materials, camera, scene, physics, or project compatibility path.
- [ ] Locate and hash the exact direct-front Highlands reference before any storefront comparison; if unavailable, mark the storefront task blocked without guessing.

### Task 10: Harden validation, packaging, and publication gates

**Files:**
- Modify: `CMakeLists.txt`, `.github/workflows/windows-ci.yml`, and the smallest affected test/script files.
- Modify: `scripts/check_repository_integrity.ps1`, `scripts/test_windows.ps1`, and authoring capture scripts only where a concrete gate is missing.
- Test: all affected CTest targets, PowerShell parser checks, package contract checks, and focused runtime harnesses.

**Interfaces:**
- Produces warnings-as-errors for first-party code, static-analysis/sanitizer configurations where the Windows toolchain supports them, negative parser tests, deterministic automation ownership tests, and truthful package provenance.

- [ ] Add first-party `/WX` or `-Werror` configuration without applying third-party warning policy indiscriminately.
- [ ] Add a sanitizer/static-analysis configuration and document platform limitations instead of claiming unsupported race detection.
- [ ] Add fuzz/property coverage for OBJ, glTF, HAMS, settings, network codecs, and bounded topology inputs.
- [ ] Test malformed automation events, detached-window physical input isolation, file errors, stalled streams, and emergency abort behavior.
- [ ] Remove unsafe harness evaluation paths and replace fixed sleeps with bounded readiness/acknowledgement conditions.
- [ ] Before every publication boundary, run `git diff --check`, inspect staged paths, scan secrets/private/generated material, verify package/source identity, and confirm the remote state without destructive Git commands.

## Validation and publication boundary

Each task must have a focused red/green test cycle, affected regression coverage, and a truthful runtime status. Visible editor work additionally requires packaged runtime evidence and coding-agent inspection of application-only captures; automated capture does not replace human visual QA. Commits and pushes occur only for coherent validated slices, never for incomplete scaffolding or disconnected helpers.
