# Henka V1 Behavior State Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an explicit-save, bounded, shared behavior-state boundary for Lua and HenkaScript while preserving Scene Document and Play isolation.

**Architecture:** A fixed-capacity `henka_script_state_store` owns typed values keyed by persistent object ID, behavior ID, and numeric state key. A Script Host execution context points to the current identity and borrowed store; Lua and HenkaScript adapters call the same typed host functions. The Sandbox owns the store, while Game Authoring performs explicit sidecar load/save outside Play.

**Tech Stack:** C17, existing Henka result/memory/persistence APIs, MSVC, CMake/CTest, bounded Lua VM, bounded HenkaScript VM, Windows PowerShell validation.

**Spec:** `docs/superpowers/specs/2026-08-21-script-state-persistence-design.md`

## Global Constraints

- Keep runtime state out of the Scene Document and never auto-save it on Play stop.
- Use fixed-capacity containers for runtime state and reject overflow with `HENKA_ERROR_LIMIT`.
- Reject non-finite numeric values, duplicate records, truncated records, unknown versions, and identity zero values.
- Load into a candidate store and replace only after complete validation.
- Keep host calls synchronous, single-threaded, and non-reentrant.
- Preserve Lua-disabled builds and the existing external game/server consumer contracts.

---

### Task 1: Add the bounded state-store module

**Files:**
- Create: `engine/include/henka/script_state.h`
- Create: `engine/src/scripting/script_state.c`
- Modify: `engine/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/test_script_state.c`

**Interfaces:**
- Produces `henka_script_state_store_create/destroy/clear`, typed set/get/remove/count operations, and atomic confined `load_file/save_file` operations.
- Produces `henka_script_state_identity` and `henka_script_state_value` types used by the host in Task 2.

- [x] **Step 1: Write the failing fixed-capacity and round-trip tests**

  The test must create a store, set bool/i32/f32/vec3 values for two identities,
  verify replacement and lookup, fill `HENKA_SCRIPT_STATE_MAX_VALUES`, assert
  the next write returns `HENKA_ERROR_LIMIT`, save to a temporary sidecar, load
  into a second store, and compare all values through the public getters.

- [x] **Step 2: Write malformed-input tests**

  Add truncated, duplicate-key, unknown-version, zero-identity, non-finite,
  and over-capacity fixture files. Assert each load fails and the destination
  store retains a sentinel value written before the attempted load.

- [x] **Step 3: Implement the fixed store and validation helpers**

  Use a fixed array of entries. Validate key and identity nonzero, enum range,
  finite floats/vectors, and exact type matches. Use checked size arithmetic for
  the versioned binary record format and reject files larger than the documented
  maximum before allocation.

- [x] **Step 4: Implement candidate-based binary load and atomic save**

  Resolve the sidecar through `henka_path_resolve_confined`, read only bounded
  bytes, validate header/version/count/records, then move the candidate into the
  destination. Save to a bounded unique sibling temporary path, flush/close it,
  replace the destination atomically, and remove the temporary file on every
  failure.

- [x] **Step 5: Build and run the focused state tests**

  Run the new CTest target and confirm it passes with zero tracked allocations.

- [x] **Step 6: Commit the integrated state/persistence foundation slice**

  Commit `feat: add bounded script state store` after reviewing `git diff --check`.

### Task 2: Extend the Script Host with state context and typed APIs

**Files:**
- Modify: `engine/include/henka/script.h`
- Modify: `engine/src/scripting/script_host.c`
- Modify: `engine/include/henka/script_runtime.h`
- Modify: `engine/src/scripting/script_runtime.c`
- Modify: `tests/test_script_host.c`
- Modify: `tests/test_script_runtime.c`

**Interfaces:**
- Consumes `henka_script_state_store` from Task 1.
- Produces `henka_script_host_set_state_store`, `henka_script_host_set_execution_context`, and the four typed State schema functions.

- [x] **Step 1: Add host contract tests**

  Bind the State schema, set a context, invoke get/set calls, verify values are
  isolated by behavior identity, verify missing defaults/presence, and assert
  context changes are rejected while a dispatcher is running.

- [x] **Step 2: Add schema IDs and typed value forms**

  Add a dedicated `I32` value type, `STATE` domain, and exact parameter/return
  declarations. Keep State functions bound like existing APIs.

- [x] **Step 3: Implement context-aware state dispatch**

  Store a borrowed state-store pointer and current identity in the host. Handle
  State APIs internally after exact schema validation; leave renderer/physics
  functions on the existing dispatcher. Require a current nonzero identity for
  State access and return deterministic defaults for missing keys.

- [x] **Step 4: Propagate behavior identity around callbacks**

  Add behavior ID to the runtime context/descriptor and set the host context
  immediately before a callback, clearing it afterward on every return path.

- [x] **Step 5: Run host/runtime tests**

  Run `henka_script_host_tests` and `henka_script_runtime_tests` and verify
  stale handles and cross-behavior state cannot alias.

### Task 3: Add Lua and HenkaScript adapters

**Files:**
- Modify: `engine/src/scripting/lua_backend.c`
- Modify: `engine/include/henka/henkascript.h`
- Modify: `engine/src/scripting/henkascript.c`
- Modify: `engine/src/scripting/henkascript_backend.c`
- Modify: `tests/test_lua_backend.c`
- Modify: `tests/test_henkascript.c`
- Modify: `tests/test_script_backends.c`

**Interfaces:**
- Consumes typed State schema and current host context from Task 2.
- Produces identical bool/i32 read/write behavior in both languages.

- [x] **Step 1: Add Lua parity tests**

  Execute a Lua `OnUpdate` that reads a missing counter, increments it, writes
  it, and verifies the next callback observes the increment; assert invalid
  numeric arguments become Lua errors without changing state.

- [x] **Step 2: Add HenkaScript state syntax tests**

  Compile and execute `i32 value = state_get_i32(7); state_set_i32(7, value + 1);`
  twice with the same host context and assert the second invocation observes the
  first write; include bool syntax and host-error propagation.

- [x] **Step 3: Implement adapters with bounded stack/register behavior**

  Register Lua `State.GetI32`, `State.SetI32`, `State.GetBool`, and
  `State.SetBool`. Add only the narrow HKS opcodes required by the tested forms;
  reject invalid keys/types before host invocation.

- [x] **Step 4: Run both backend tests and Lua-disabled compilation**

  Run the focused Lua/HKS targets, then configure/build `henka_runtime` with
  `HENKA_ENABLE_LUA=OFF` to verify the public state API remains linkable.

### Task 4: Integrate explicit sidecar operations into Play/Authoring

**Files:**
- Modify: `examples/sandbox3d/play_session.h`
- Modify: `examples/sandbox3d/play_session.c`
- Modify: `examples/sandbox3d/game_authoring.h`
- Modify: `examples/sandbox3d/game_authoring.c`
- Modify: `tests/test_sandbox3d_play_session.c`
- Modify: `tests/test_sandbox3d_game_authoring.c`

**Interfaces:**
- Consumes the store and host APIs from Tasks 1-3.
- Produces explicit `sandbox3d_game_authoring_load_play_state` and
  `sandbox3d_game_authoring_save_play_state` operations; neither is callable
  while Play is running.

- [x] **Step 1: Add isolation tests first**

  Set runtime state during Play, assert the authored Scene Document and Edit
  scene remain unchanged, stop Play, save explicitly, clear/restart, load the
  sidecar, and assert state returns only after explicit load.

- [x] **Step 2: Make Play own and bind the state store**

  Create the fixed store with the Play session, attach it to the host, and pass
  the current persistent behavior ID through the runtime without retaining
  authoring pointers.

- [x] **Step 3: Add coordinator save/load seams**

  Derive a confined sidecar path beside the authored scene path, reject Play
  state transitions, and preserve the previous store when load fails.

- [x] **Step 4: Run Play/authoring integration tests**

  Confirm explicit save/load and failed-load retention through public APIs.

### Task 5: Document and publish

**Files:**
- Modify: `docs/scripting-foundation.md`
- Modify: `docs/current-capabilities.md`
- Modify: `docs/architecture.md`
- Modify: `docs/roadmap.md`

- [x] **Step 1: Document explicit state persistence and isolation**

  Mark the typed state slice Available, retain event routing/editor authoring
  as open, and document sidecar naming, bounds, failure behavior, and the
  explicit-save rule.

- [x] **Step 2: Run repository verification**

  Run affected CTest, full `scripts/test_windows.ps1 -Configuration Debug`,
  Lua-disabled build, external game/server templates, package contract, and
  `git diff --check`.

- [x] **Step 3: Commit and publish**

  Regenerate provenance/package only from a clean commit, push `main`, and
  verify `HEAD == origin/main`, divergence `0/0`, clean worktree, and no runtime
  processes.
