# Terrain Quality and Realism Campaign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Terrain a fully visible, PBR-materialized, editable, pass-integrated, bounded, recoverable feature through Henka's normal graphical and external-game paths, while closing the remaining connected multiplayer, editor/runtime, package, stress, audit, and documentation gaps.

**Architecture:** Preserve the completed renderer-independent Terrain world, storage, streaming, authority, replica, prediction, physics, and dedicated-server layers. Add Terrain material semantics at the existing public material/scene seam, feed authoritative four-layer weights through the existing mesh vertex channel, and use the existing Rendered shader/pass machinery for lighting, shadows, AO/SSGI/SSR, history, fog, and presentation. Keep renderer ownership in the graphical target and keep the public API C17.

**Tech Stack:** C17, existing Henka scene/material/mesh APIs, OpenGL 3.3 shader assets, CMake, Windows PowerShell 5.1, existing CTest and packaged runtime harnesses, pinned existing dependencies only.

## Global Constraints

- Preserve the exact completed Terrain runtime/network/storage architecture; do not duplicate or redesign it.
- Public Henka API and normal engine implementation remain C17; unavoidable C++ stays behind an internal C ABI.
- No VR, OpenXR, headset, stereo, controller, or VR-specific dependency work.
- Terrain material weights remain four authoritative uint8 values normalized to 255.
- Renderer resource replacement remains transactional; old valid CPU/GPU/collision state survives failure.
- New queues, residency, material resources, topology variants, and diagnostics remain bounded with explicit ownership.
- No ordinary per-frame heap allocation in new rendering paths.
- Update README and relevant public documentation in the same validated slice as public behavior changes.
- Commit coherent slices normally and publish every green, clean coherent slice through the authorized fast-forward gate.
- Before each push: test, diff review, hygiene, integrity, `git fetch origin --prune`, divergence inspection, normal push, and remote SHA verification.

## Current Implementation Matrix

| Area | Current state | Required closure |
| --- | --- | --- |
| Headless runtime, dedicated server, Terrain storage, streaming, authority, prediction, replica, snapshots, physics | COMPLETE for the prior Terrain campaign | Preserve; add only connected fixes and fresh regression coverage |
| Graphical Terrain owner, bounded slots/queue, revision-aware mesh replacement, camera observer scheduling | PARTIAL | Add resource diagnostics and packaged visual proof; material, bounds, and dirty propagation are implemented |
| Terrain material authority | PARTIAL | Four-layer PBR factors/textures, stable linear/sRGB semantics, normal/roughness/metallic blending, truthful fallback; shared dependency inspection includes all twelve optional layer texture slots |
| Terrain tangent basis and border normals | PARTIAL | Finite handed tangents, neighbor-aware normals, world-stable mapping, tests |
| Crack-free LOD | PARTIAL | Reusable transition topology or justified watertight equivalent; topology tests |
| Render-pass participation | PARTIAL | Verify and close color, shadow, depth, AO/SSGI/SSR, probe, temporal, fog, and presentation paths |
| Bounds and culling | PARTIAL | Revision-aware height bounds and deformation/culling tests |
| Editor/runtime tool surface | PARTIAL | Complete controls, ray-pick workflow, shared commands, runtime harness |
| Multiplayer recovery and two-client integration | PARTIAL | Reconnect, late join, retry, corrupt/disconnect recovery, byte-identical convergence |
| Reference realism scene and packaged client | PARTIAL | Production-style terrain materials, views, packaged edit/restart proof, screenshots |
| Stress, failure injection, memory/GPU diagnostics, recursive audits | PARTIAL | Complete bounded matrices and two-pass closure |

---

### Task 1: Establish focused material and mesh regressions

**Files:**
- Modify: `tests/test_terrain_render.c`
- Modify: `tests/test_terrain_mesh.c`
- Modify: `tests/CMakeLists.txt` only if a new focused test executable is indispensable

**Interfaces:**
- Consumes the existing `henka_terrain_render_desc`, `henka_terrain_mesh_build_chunk`, and `henka_material` contracts.
- Produces executable regression expectations for Terrain layer semantics, normalized weights, finite tangents, world UV continuity, and transition topology.

- [ ] Write a failing render test proving the default Terrain material is a four-layer PBR contract rather than `HENKA_MATERIAL_TYPE_VERTEX_COLOR`.
- [ ] Write a failing mesh test asserting generated Terrain vertices expose finite, handed tangent data and world-scale UVs continuous at shared chunk borders.
- [ ] Write a failing topology test for same-LOD and one-level adjacent LOD borders, including a four-chunk corner case.
- [ ] Run the focused tests and record the expected RED failures before production changes.
- [ ] Commit only test changes if the repository convention requires a red-test checkpoint; otherwise keep them with the first green implementation slice.

### Task 2: Add the bounded four-layer Terrain material contract

**Files:**
- Modify: `engine/include/henka/scene.h`
- Modify: `engine/src/scene/scene.c`
- Modify: `engine/include/henka/terrain_render.h`
- Modify: `engine/src/renderer/terrain_render.c`
- Modify: `engine/src/renderer/renderer_opengl.c`
- Modify: `assets/shaders/basic_lit.vert`
- Modify: `assets/shaders/basic_lit.frag`
- Modify: `examples/sandbox3d/main.c`
- Modify: `tests/test_terrain_render.c`
- Modify: `README.md`, `docs/terrain.md`, and relevant rendering documentation

**Interfaces:**
- Add a C17 `henka_material_layer` value containing bounded factors, scale, and borrowed base-color/normal/metallic-roughness textures.
- Add four bounded layer records and a `terrain_layers_enabled` flag to `henka_material`, with validation rejecting invalid texture coordinate sets, non-finite factors, and invalid layer counts.
- Add a `henka_terrain_material_default()` constructor that returns a valid PBR terrain material with deterministic factors and no required external texture.
- Extend renderer uniform binding with four layer texture/factor arrays while retaining existing single-material behavior.

- [ ] Implement validation tests for layer factors, texture pointers, UV sets, and default material semantics.
- [ ] Implement the default contract and validation without changing existing glTF material behavior.
- [ ] Add shader uniforms and bounded layer blending: normalized weights, linear-space color blending, roughness/metallic blending, and stable normal blending in tangent space.
- [ ] Use world-space terrain UVs with per-layer meter-scale tiling and deterministic factor fallback when a texture is absent.
- [ ] Load or generate only small documented reference textures for grass, dirt, rock, and a contrasting fourth layer; keep resource ownership in the existing asset manager.
- [ ] Route Sandbox Terrain through the new material constructor and preserve existing external material consumers.
- [ ] Run focused render/material tests, shader/package smoke, and inspect the diff for accidental public ABI or generated-file changes.
- [ ] Commit `feat: complete Terrain PBR material blending` after green validation.

### Task 3: Correct Terrain tangent space, border normals, and world mapping

**Files:**
- Modify: `engine/include/henka/terrain_mesh.h`
- Modify: `engine/src/terrain/terrain_mesh.c`
- Modify: `engine/src/renderer/mesh.c`
- Modify: `tests/test_terrain_mesh.c`
- Modify: `docs/terrain.md`, `README.md`

**Interfaces:**
- Extend `henka_terrain_mesh_vertex` with tangent xyz and handedness while retaining the existing weight channel.
- Keep mesh generation caller-owned and bounded; neighbor samples are read only from resident authoritative regions.

- [ ] Extend RED tests for tangent orthogonality, handedness, finite fallback, and shared-border normal equality.
- [ ] Implement neighbor-aware sample lookup with deterministic local fallback when a neighbor is nonresident.
- [ ] Implement orthogonalized tangent construction and mirrored-UV-safe handedness.
- [ ] Replace normalized chunk UVs with world-meter-scale UVs and configurable layer scale values.
- [ ] Re-run mesh, renderer, and shader tests; commit `feat: harden Terrain tangent and world mapping`.

### Task 4: Replace skirt-only LOD transitions with watertight bounded topology

**Files:**
- Modify: `engine/include/henka/terrain_mesh.h`
- Modify: `engine/src/terrain/terrain_mesh.c`
- Modify: `engine/src/renderer/terrain_render.c`
- Modify: `engine/src/renderer/mesh.c`
- Modify: `tests/test_terrain_mesh.c`, `tests/test_terrain_render.c`
- Modify: `docs/terrain.md`, `README.md`

**Interfaces:**
- Add a bounded edge-transition mask or equivalent topology selector to mesh build inputs.
- Preserve the fixed maximum vertex/index limits and deterministic neighbor LOD clamp.

- [x] Add RED topology tests for equal LOD, one-level edge transitions, and four-way corners.
- [x] Implement reusable transition index variants using shared edge samples; reject unsupported differences rather than creating holes.
- [x] Keep skirts only as a documented last-resort fallback for invalid/nonresident neighbor state, with diagnostics.
- [x] Validate mesh bounds and GPU upload replacement under each topology variant.
- [ ] Commit `feat: stitch Terrain LOD boundaries`.

### Task 5: Integrate automatic dirty propagation and dynamic bounds

**Files:**
- Modify: `engine/include/henka/terrain_render.h`
- Modify: `engine/src/renderer/terrain_render.c`
- Modify: `engine/src/terrain/terrain_edit.c`, `terrain_replica.c`, `terrain_streaming.c` only where a public revision/dirty seam is missing
- Modify: `engine/src/scene/scene.c` or existing bounds consumer as required
- Modify: `tests/test_terrain_workflow.c`, `tests/test_terrain_render.c`

**Interfaces:**
- Add explicit render dirty/rebuild and weight-only update diagnostics without exposing renderer-private types to `henka_runtime`.
- Preserve render revision/generation identity and transactional candidate replacement.

- [ ] Add RED tests for local edit, remote delta, snapshot replacement, reload, paint-only edit, and failed candidate replacement.
- [ ] Route each source through one dirty propagation seam; dirty neighboring chunks when height borders affect normals/topology.
- [ ] Update dynamic world bounds from resident samples and retain previous valid bounds on invalid replacement.
- [ ] Avoid collision rebuild for paint-only edits and avoid geometry rebuild for separable weight-only updates where safe.
- [ ] Commit `feat: rebuild Terrain render state after edits`.

### Task 6: Complete normal Rendered pass participation and camera-driven residency

**Files:**
- Modify: `engine/src/renderer/terrain_render.c`
- Modify: `engine/src/renderer/renderer_opengl.c`
- Modify: `examples/sandbox3d/main.c`
- Modify: `tests/test_terrain_render.c`, renderer/pass tests
- Modify: `README.md`, `docs/terrain.md`, rendering docs

- [ ] Add pass-level assertions or diagnostics proving Terrain submissions enter color, shadow, depth, AO/SSGI/SSR, probe capture, temporal, fog, and HDR presentation paths where supported.
- [ ] Connect the active Scene View/game camera to Terrain streaming demand without manual region priming.
- [ ] Verify cast/receive shadow flags, culling, selection/picking, and clean scene teardown.
- [ ] Exercise outdoor/studio, sun, slope, local light, fog, AO/SSGI, reflection, Solid, Material Preview, and Rendered modes.
- [ ] Commit `feat: integrate Terrain with Rendered passes`.

### Task 7: Finish editor/runtime tools and reusable external consumer path

**Files:**
- Modify: `examples/sandbox3d/main.c`
- Modify: `templates/external_game_minimal/*`
- Modify: `tests/test_terrain_workflow.c` and/or add `tests/test_external_terrain_consumer.c`
- Modify: `docs/terrain.md`, external-game docs, `README.md`

- [ ] Add RED workflow assertions for status, load/create, sculpt, paint, layer, radius, strength, falloff, diagnostics, and ray-pick.
- [ ] Implement the UI using the existing workspace/disclosure/property system and shared command API.
- [ ] Add a bounded brush preview using existing helper rendering if it does not alter normal ownership.
- [x] Add a public external C17 runtime harness for raycast, edit, collision/render refresh, save, and reload; the template now also runs a public graphical engine/scene/Terrain Rendered smoke with visible-draw and HDR/shadow diagnostics.
- [ ] Validate packaged Sandbox behavior and commit `feat: complete Terrain editor and runtime tools`.

### Task 8: Close multiplayer recovery and package closure

**Files:**
- Modify: `engine/src/terrain/terrain_client.c`, `terrain_replica.c`, `terrain_server.c`, `terrain_network.c` only for demonstrated gaps
- Modify: `tests/test_terrain_client.c`, `test_terrain_replica.c`, `test_terrain_server.c`, process harness
- Modify: `scripts/test_terrain_process_integration_windows.ps1`
- Modify: dedicated/server and external template docs

- [ ] Add RED cases for corrupt snapshot, missing/duplicate fragments, late join, retry, world/base mismatch, and stale smooth/flatten; the client regression now covers a forced server disconnect and explicit reconnect with exact resident-sample comparison.
- [ ] Implement bounded retry/recovery state machines with truthful diagnostics and no partial publication.
- [ ] Extend the two-client process harness to assert identity, revisions, paint weights, command IDs, disconnect/reconnect, late join, and restart byte convergence; the single-client session regression now covers same-endpoint authoritative server-wrapper restart and exact resident-sample convergence.
- [ ] Validate both packages and the remaining multiplayer harness; external game/server templates, full Debug build, CTest, Sandbox smoke, hygiene, and integrity passed in the published recovery slice.

### Task 9: Build the realism reference scene and bounded stress/failure matrix

**Files:**
- Modify: `examples/sandbox3d/main.c` and packaged reference assets under `assets/`
- Modify: tests and scripts for stress/fault scenarios
- Modify: `README.md`, `docs/terrain.md`, realism, building, and package docs

- [ ] Add deterministic bounded rolling, cliff, valley, four-layer, wet/roughness, shadow, AO/SSGI, reflection, and distant-LOD fixtures.
- [ ] Add failure injection for region/mesh/GPU/material/queue/collision/snapshot/persistence paths with previous-state assertions.
- [ ] Add camera-crossing, LOD, rapid edit/paint, boundary, mode-switch, resize, context, multiplayer, restart, and shutdown soak scenarios.
- [ ] Record CPU/GPU/Terrain memory, queue high-water, culling, draw, recovery, GL-error, thread, and clean-shutdown diagnostics.
- [ ] Commit `test: add Terrain realism and bounded stress coverage`.

### Task 10: Recursive audits, final validation, documentation, and publication

**Files:**
- Modify only files identified by the two recursive audits, plus public docs and validation scripts required by evidence.

- [ ] Run two independent connection audits from public API through package, external consumers, tests, diagnostics, and docs; repair every genuine reachable defect.
- [ ] Run Debug/Release builds and complete CTest, server-only build, packages, package checks, external templates, malformed-input, recovery, two-client, stress, shutdown, hygiene, integrity, and private-path scans.
- [ ] Capture application-only Terrain evidence for wide, close material, four-layer transition, cliff, shadow, LOD, AO/SSGI, far streaming, sculpt, paint, and restart states.
- [ ] Update all public documentation and keep VR explicitly roadmap-only.
- [ ] At each major checkpoint, inspect staged diffs and publish only clean validated normal fast-forward commits.
- [ ] Finalize one evidence archive, verify final local/remote SHA equality, and commit no generated evidence.

## Self-Review

- Completed prior Terrain runtime, storage, networking, physics, and bounded streaming work is preserved and explicitly excluded from redesign tasks.
- The first slice has a concrete RED test target: the current default Terrain material is vertex-color-only and the current terrain mesh has no tangent field or neighbor-aware topology.
- Every later slice has focused tests, connected regressions, documentation, and a publication boundary.
- VR is excluded from all tasks and remains roadmap-only.
