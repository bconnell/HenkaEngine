# Stable Component Identities and Reusable Physical Storage Implementation Plan

> **For agentic workers:** Execute this plan in the canonical checkout. Do not
> create or switch to an isolated worktree. Commit and push each coherent
> validated slice.

**Goal:** Decouple public logical component IDs from bounded physical vertex,
edge, and face storage so long authoring sessions reuse inactive slots without
reusing IDs.

**Architecture:** Keep fixed-capacity physical arrays with active counts and
deterministic lowest-free-slot allocation. Add bounded per-class logical-ID
maps for resolution, retain a separate endpoint-pair edge lookup, and keep
monotonic per-mesh allocator watermarks. Candidate-first edits publish cloned,
validated maps/topology atomically. History restores topology while taking the
maximum of current and snapshot watermarks. HAMS v4 records active logical IDs,
capacities, watermarks, and all current metadata; v2/v3 load through a legacy
slot-derived-ID migration path.

**Tech Stack:** C11, existing `henka_malloc/calloc/free` and checked arithmetic,
existing public authoring mesh/modeling/UV APIs, CMake, MSVC warnings-as-errors,
native Windows and sanitizer test scripts.

**Spec:** `docs/superpowers/specs/2026-08-23-stable-component-identities-design.md`

## Global constraints

- Preserve the canonical checkout and all existing valid work.
- Do not add unbounded containers, pointer IDs, global identity, or unsafe
  dynamic growth to normal mesh operations.
- Do not weaken existing transactional, malformed-input, topology, rendering,
  or packaged-runtime gates.
- Keep deterministic physical iteration and serialized ordering; map placement
  is never an observable ordering contract.
- Do not claim Procedural Asset Graph work is implemented in this goal.
- Before each PowerShell inspection/test command, perform the repository's
  Gold Standard Rules precheck.

## Task 1: Establish the public regression contract

**Files:**

- Modify: `tests/test_authoring_mesh.c` or create a focused authoring identity
  test and register it in `tests/CMakeLists.txt`.
- Modify: `docs/authoring-mesh.md` only when implementation behavior is proven.

- [ ] Add failing public-operation tests for vertex, edge, and face physical
  slot reuse; fresh logical IDs; stale-ID invalidity; and active-capacity
  overflow.
- [ ] Add failing history tests for undo, redo, and a new branch after undo.
- [ ] Add a bounded churn test that repeatedly performs public modeling edits
  while active counts remain below capacity and asserts no false limit.
- [ ] Run the focused target and record the expected baseline failure caused by
  the current slot high-water allocator.
- [ ] Commit the red tests as a coherent test slice after `git diff --check`.

## Task 2: Add bounded logical-ID maps and reusable slots

**Files:**

- Modify: `engine/src/mesh/authoring_mesh.c`.
- Modify: `engine/src/mesh/authoring_mesh_internal.h` if shared helpers are
  needed.
- Modify: `engine/include/henka/authoring_mesh.h` comments only if the public
  contract wording changes.

- [ ] Replace `id - 1` resolution with bounded per-class maps and explicit
  empty/occupied/deleted states.
- [ ] Convert `*_slots` semantics to physical capacities/iteration or retain
  names only where they clearly mean physical array extent; add active counts.
- [ ] Add checked map-capacity arithmetic and deterministic lowest-free-slot
  selection.
- [ ] Add monotonic next-ID allocation with zero/invalid/wrap rejection.
- [ ] Ensure add/remove/rebuild/validate/edge-pair lookup update maps
  transactionally and reject duplicates or stale references.
- [ ] Update clone/copy and every direct slot assumption in mesh, modeling,
  topology, UV, quad recovery, reports, selection, and external consumers.
- [ ] Run focused red tests to green, then the authoring mesh/topology suites.
- [ ] Commit and push the validated storage/identity slice.

## Task 3: Preserve candidate and history lifetime semantics

**Files:**

- Modify: `engine/src/mesh/authoring_mesh.c`.
- Modify: `engine/src/mesh/authoring_modeling.c`.
- Modify: `engine/src/mesh/authoring_topology.c`.
- Modify: `engine/src/mesh/authoring_uv.c`.
- Modify: any consumer found by the direct-slot audit.

- [ ] Preserve candidate-first publication and ensure failed candidates do not
  mutate the surviving mesh's topology, maps, watermarks, selection, render,
  collider, bounds, or history-visible state.
- [ ] Make history restoration retain `max(current watermark, snapshot
  watermark)` and rebuild maps before publication.
- [ ] Prove redo cannot resurrect stale IDs and a new edit after undo clears
  redo while generating IDs not previously issued in the mesh lifetime.
- [ ] Keep deterministic iteration and output ordering independent of map
  placement; validate render and topology reports after churn.
- [ ] Run modeling, topology repair, UV, quad recovery, selection, render, and
  external-consumer tests; commit and push this coherent slice.

## Task 4: Implement HAMS v4 and legacy migration

**Files:**

- Modify: `engine/src/mesh/authoring_mesh.c`.
- Modify: `tests/test_authoring_mesh.c` and focused persistence tests.
- Modify: `docs/authoring-mesh.md` after fresh evidence.

- [ ] Write/read HAMS v4 with capacities, active counts, active logical IDs,
  allocator watermarks, and all current topology/UV/material/smoothing/hard-edge
  metadata in deterministic physical order.
- [ ] Keep v2/v3 loading and map their slot-derived records into independent
  storage with safe next-ID initialization; do not auto-resave legacy files.
- [ ] Reject unknown versions, truncation, duplicate/zero/invalid IDs,
  nonexistent refs, invalid watermarks, active-over-capacity counts, arithmetic
  overflow, nonfinite values, malformed loops, and nonmanifold relations.
- [ ] Prove failed load preserves the prior valid destination and `load_file_new`
  assigns output only after complete success.
- [ ] Run persistence round-trip, legacy fixtures, malformed-file, native
  save/reopen/re-edit, path/hygiene, and sanitizer gates; commit and push.

## Task 5: Broad verification and publication

- [ ] Run focused authoring tests, full native Debug and Release gates, MSVC
  warning checks, sanitizer runtime, external consumers, packaged runtime, and
  visible modeling evidence.
- [ ] Inspect staged, unstaged, and untracked state; run `git diff --check` and
  repository integrity/hygiene gates.
- [ ] Verify no Henka runtime process remains running.
- [ ] Commit and push the final bounded-repair slice.
- [ ] Verify `HEAD == origin/main`, divergence `0/0`, and clean worktree.
- [ ] Stop before Procedural Asset Graph and report root cause, architecture,
  tests-before/tests-after, migration, HAMS version, stale-ID proof, churn,
  sanitizer, Debug/Release, package, commit SHA, synchronization, and remaining
  modeling gaps.

