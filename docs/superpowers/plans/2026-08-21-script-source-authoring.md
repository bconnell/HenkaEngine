# Henka Script Source Authoring and Reload Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a bounded source document, compiler/backend-backed validation, atomic script saving, structured editor interaction, and transactional Play reload for Lua and HenkaScript.

**Architecture:** The engine owns a fixed-capacity source document and confined atomic-save seam. HenkaScript validation calls the public HKS backend/compiler and Lua validation calls the public Lua backend; the editor consumes diagnostics and token spans without defining syntax. The Play session builds candidate backends first and swaps them only after success.

**Tech Stack:** C11, existing Henka result/memory/persistence APIs, HenkaScript public lexer/compiler, Lua 5.4 backend, SDL3 input, CMake, MSVC Debug warnings-as-errors, existing sandbox and external-project gates.

**Spec:** `docs/superpowers/specs/2026-08-21-script-source-authoring-design.md`

## Global Constraints

- Keep `henka_hks_compile`, `henka_hks_lex`, and the existing Lua backend as language authorities; do not copy keyword tables or grammars into editor code.
- Keep all source and token storage bounded by the existing 256 KiB language limits.
- Resolve every project-relative path through `henka_path_resolve_confined`; never accept traversal or absolute escape paths.
- Invalid source may be saved for recovery, but it must never activate or replace a valid runtime backend.
- Play runtime state and authoring state remain isolated; active Play rejects authoring mutation.
- Automation input remains application-local; do not install global Windows input hooks or blocking.
- Use `apply_patch` for source edits, preserve unrelated work, and require focused tests before broader gates.

---

### Task 1: Add the bounded source-document contract

**Files:**
- Create: `engine/include/henka/script_source.h`
- Create: `engine/src/scripting/script_source.c`
- Modify: `engine/CMakeLists.txt`
- Test: `tests/test_script_source.c`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `henka_script_language`, `henka_hks_behavior_backend_create`, `henka_lua_behavior_backend_create`, `henka_malloc/calloc/free`, and the existing language size/diagnostic constants.
- Produces: `henka_script_source_document`, `henka_script_source_diagnostic`, create/destroy, bounded set/get text, validate/get diagnostic, dirty, and revision APIs exactly as defined in the design spec.

- [ ] **Step 1: Write failing tests** for null arguments, both supported languages, bounded source acceptance, over-limit rejection without mutation, NUL-terminated returned text, dirty/revision transitions, and compiler/backend validation diagnostics.
- [ ] **Step 2: Run the focused test target** and confirm failure because the source-document symbols do not exist.
- [ ] **Step 3: Implement the fixed-capacity heap-owned document.** Allocate `max_source_bytes + 1`, reject lengths above the language limit, copy exactly `source_size` bytes, append NUL, increment revision only after a successful copy, and set dirty without changing the old buffer on failure.
- [ ] **Step 4: Implement validation through existing backends.** For HenkaScript call `henka_hks_behavior_backend_create`; for Lua call `henka_lua_behavior_backend_create`; destroy any temporary backend immediately; copy only bounded line, column, code, and message fields into the language-neutral diagnostic.
- [ ] **Step 5: Build and run `henka_script_source_tests`.** Confirm both valid and invalid sources produce the expected result and no memory-leak report.
- [ ] **Step 6: Commit** with `feat: add bounded script source documents`.

### Task 2: Add confined atomic script-source persistence

**Files:**
- Modify: `engine/include/henka/script_asset.h`
- Modify: `engine/src/scripting/script_asset.c`
- Test: `tests/test_script_asset.c`

**Interfaces:**
- Consumes: `henka_script_source_document` text/language, `henka_path_resolve_confined`, `henka_path_ensure_parent_directory`, and the existing scene-document atomic replacement pattern.
- Produces:
  `henka_script_asset_load_source_document(const char*, const char*, henka_script_source_document**)` and
  `henka_script_asset_save_source_document(const char*, const char*, const henka_script_source_document*)`.

- [ ] **Step 1: Add failing persistence tests** for successful load, language suffix mismatch, traversal rejection, source-size limit rejection, successful replacement, old-file retention after a deliberately invalid destination/path failure, and temporary-file cleanup.
- [ ] **Step 2: Run `henka_script_asset_tests`** and confirm the new symbols/tests fail before implementation.
- [ ] **Step 3: Implement confined load** by resolving the path, reading through the existing bounded source reader, creating a document for the behavior language inferred from the suffix, and setting its loaded text clean.
- [ ] **Step 4: Implement atomic save** with a bounded unique sibling temporary path, `wb`/`fopen_s` on the temporary file, complete write/flush/close checks, platform replacement, and cleanup on every failure path. Do not require validation before writing.
- [ ] **Step 5: Run the script asset tests** and inspect the destination bytes before and after failure to prove transactional retention.
- [ ] **Step 6: Commit** with `feat: add transactional script source persistence`.

### Task 3: Connect source documents to the structured editor

**Files:**
- Modify: `examples/sandbox3d/script_editor.h`
- Modify: `examples/sandbox3d/script_editor.c`
- Modify: `examples/sandbox3d/main.c`
- Modify: `engine/include/henka/ui.h`
- Modify: `engine/src/platform/platform_sdl.c`
- Test: `tests/test_script_editor.c`
- Modify: `examples/sandbox3d/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: source-document load/set/validate/save, HKS public token spans, Lua backend diagnostics, existing `henka_ui_frame_desc`, and SDL3 text/key events.
- Produces: a bounded editor model with explicit Edit/Save/Revert/Reload actions, caret/selection/scroll state, staged invalid text, visible diagnostics, and an application-local text-input event stream.

- [ ] **Step 1: Write failing editor-model tests** for preserved indentation, line/column caret movement, insertion/deletion within the bounded source limit, compiler-derived HKS spans, invalid-source staging, revert to last loaded text, and Edit rejection while Play is active.
- [ ] **Step 2: Extend the platform/UI input seam** with a bounded per-frame text-input buffer and logical cursor coordinates; clear the buffer after consumption and keep physical cursor data separate from automation-owned coordinates.
- [ ] **Step 3: Implement editor state** as fixed-capacity line/caret/selection state over the source document; use compiler token offsets for HenkaScript display and backend diagnostics for error markers; use no copied Lua/HenkaScript grammar.
- [ ] **Step 4: Implement Save/Revert controls** through the script asset API; keep invalid text staged and display the diagnostic while allowing save; never call runtime reload from a failed validation path.
- [ ] **Step 5: Build the sandbox and run editor tests** under MSVC warning-as-error settings; verify the changed Henka modules introduce no warnings.
- [ ] **Step 6: Commit** with `feat: add structured script editor input`.

### Task 4: Add transactional Play reload

**Files:**
- Modify: `examples/sandbox3d/play_session.h`
- Modify: `examples/sandbox3d/play_session.c`
- Modify: `examples/sandbox3d/game_authoring.h`
- Modify: `examples/sandbox3d/game_authoring.c`
- Test: `tests/test_sandbox3d_play_session.c`
- Test: `tests/test_sandbox3d_game_authoring.c`

**Interfaces:**
- Consumes: authored behavior identity, source-document validation, existing asset/backend creation, generation-checked runtime handles, and the borrowed state store.
- Produces:
  `sandbox3d_play_session_reload_behavior(sandbox3d_play_session*, uint64_t entity_id, uint64_t behavior_id, const henka_script_source_document*, henka_script_source_diagnostic*)`.

- [ ] **Step 1: Add failing tests** for successful reload, invalid candidate retention, backend-construction failure retention, active Play authoring rejection, generation-checked identity retention, lifecycle ordering, and state-store value preservation.
- [ ] **Step 2: Run the Play-session tests** and confirm the reload API is absent/fails.
- [ ] **Step 3: Implement candidate construction** using the existing script asset/backend boundary and the source document’s bounded bytes; do not mutate the live runtime before candidate success.
- [ ] **Step 4: Implement commit order**: stop/destroy old backend only after candidate success, bind candidate to the existing behavior slot/generation, preserve authored and typed state identity, and return a bounded diagnostic on failure.
- [ ] **Step 5: Run the focused scripting, scene, Play, and Game Authoring suite** and inspect failure paths for stale callbacks or dangling backend pointers.
- [ ] **Step 6: Commit** with `feat: add transactional behavior reload`.

### Task 5: Package, document, and externally validate the slice

**Files:**
- Modify: `docs/current-capabilities.md`
- Modify: `docs/roadmap.md`
- Modify: `docs/scripting-foundation.md`
- Modify: external template sources under `build/tv/external_game_minimal/external_game_minimal_src` only if the repository validation fixture is tracked and requires the new public seam.
- Test: existing external template gate and documentation truth gate.

- [ ] **Step 1: Update documentation** to mark source-document loading, staged editing, atomic save, and diagnostics as Available/Foundation only where tested; keep full debugger, schema migration, and broader project workflows unfinished.
- [ ] **Step 2: Search public docs** for stale claims such as `source editing`, `hot reload`, `compiler authority`, and `second grammar`, then resolve contradictions against source.
- [ ] **Step 3: Run the focused suite, Debug build, documentation truth check, `git diff --check`, and external mixed-language project gate.**
- [ ] **Step 4: Review the final diff** for duplicated syntax knowledge, unbounded growth, path escapes, ignored errors, and input ownership violations.
- [ ] **Step 5: Commit and push** the integrated slice only after all gates pass.
- [ ] **Step 6: Verify** `HEAD == origin/main`, divergence `0/0`, and clean worktree.
