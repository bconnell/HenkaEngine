# Editor Typography and UI Scale — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Review status:** Owner-approved for native implementation on 2026-10-11. Production implementation is authorized after current-main reconciliation.

**Goal:** Establish a bundled, measurable, scale-aware editor-font system and one persisted Editor UI Scale authority; after auditing the then-current implementation base, migrate every remaining fixed-bitmap product text path to that authority and prove readability and behavior in the exact packaged editor.

**Architecture:** Keep font-byte acquisition at the existing application/asset boundary; use an opaque UI font resource plus a shared text-layout authority; keep GL atlas residency per actual context; represent solid and glyph quads in insertion order; let global editor preferences own the four-value app scale while workspace geometry consumes it. Keep OS display scale as a separate output transform.

**Tech Stack:** C11, existing pinned `stb_truetype` / `stb_rect_pack` revision, SDL3 APIs at the repository-pinned version, OpenGL UI renderer, existing settings/persistence, CMake/CTest, and Henka's Windows build/package/test scripts.

**Spec:** `docs/design/editor-typography.md` (owner-approved direction and amended authority contract; implementation plan approved 2026-10-11).

## Global Constraints

- **Plan revision:** 2026-10-11 — incorporates the owner-approved global scale authority and explicit one-time legacy-layout migration marker/recovery rules, retirement of competing scale heuristics, verified bundled-byte bootstrap, failure-atomic metrics/layout/atlas publication, one measurement authority, independent app/display-scale tests, bounded native/detached-context DPI replacement, and exact static-TTF provenance. Font comparisons hold OS display scale constant; a separate real-DPI package control holds app scale and logical window dimensions constant. Owner approved native implementation on 2026-10-11.
- The design inspection at `b311915ffcc35acb2de99f93d06e08374b865718` is historical, not the implementation base. The implementation worktree has been fast-forwarded to published `main` at `dab197e23938d6d6af6e12041b1ea90e3ba630b2`; revalidate repository APIs, persistence, renderer-context ownership, SDL assumptions, and affected caller locations before coding. Preserve these review artifacts and unrelated files; do not reset, clean, stash, rebase, overwrite, or move other work.
- Keep all generated state under the repository-owned `_local` root and use the canonical Henka toolchain/build/test/package lifecycle.
- Do not add another responsibility to the oversized Sandbox `main.c`; keep its Settings integration to a narrow call into a focused scale-control module.
- Do not add FreeType, a second stb pin, arbitrary font-path loading in UI code, a public general texture API, or game-runtime typography. The application/asset bootstrap alone resolves the verified packaged font and passes bounded bytes plus stable content identity into the opaque resource; neither core text code nor `ui.c` owns filesystem, package-path, or Sandbox-specific loading.
- Do not publish both typefaces as production fonts. Atkinson Hyperlegible is the lead; Noto Sans is the comparison. Bundle only the evidence-selected winner and its exact license/provenance in the final production candidate.
- No silent fixed-bitmap fallback. Failure stage and font identity must be diagnosable; packaged acceptance must reject fallback use.
- Editor UI Scale has exactly four persisted values—100%, 125%, 150%, and 200%—owned globally by editor preferences. Workspace/layout models consume it; presets and custom layouts never own or override it.
- Migrate only the legacy global `custom_layout_ui_scale` scalar / `ui.workspace.custom_layout.ui_scale` key. Preserve per-slot `ui_scale` values verbatim; never infer a selected slot or read those values as a migration or restore authority. Use `ui.editor.scale_migration_version` with absent/0 = pending, 1 = retired; fail closed without overwriting any other stored marker value.
- Startup migration is itself a scale transition: prepare candidate metrics, required atlases, and workspace geometry before atomically persisting the canonical scale plus migration version; publish by no-fail swap before workspace restore can expose geometry at another scale. A pending migration preserves a valid canonical value; otherwise it converts only the legacy global scalar or defaults to 100%. After version 1, missing/invalid canonical state recovers to 100% without consulting legacy values. Task 2 defines and tests preference/conversion authority only; Task 6 owns integrated startup migration and its failure-atomicity proof.
- OS/display scale is a separate output/raster transform. Framebuffer/window ratios, resolution-readability factors, and display-scale heuristics must not overwrite or multiply the persisted app scale.
- Preserve the full `[Hidden]` text marker, canonical object names, existing controls, hit-target sizes, and all existing layout semantics. Do not alter the hidden-row threshold to accommodate the font.
- Keep exact executable and package provenance truthful. Product executable provenance and checker/evidence provenance remain distinct.
- Treat `HENKA_MAX_TOOL_WINDOWS == 5` plus the main window as the initial maximum of six UI graphics contexts. The approved per-context atlas bounds therefore imply 96 MiB aggregate active coverage and a 192 MiB worst-case all-context replacement peak. Verify actual renderer context ownership before relying on this calculation; fail closed or revise the design with measured evidence if the ownership bound differs.
- Tasks 2–7 end with task-scoped local commits only after their stated tests and gate are green. Task 1 is intentionally local-only provenance research and creates no tracked source changes or commit. Task 8 creates the coherent publication candidate after the full exact-package gate; do not open or publish a PR per implementation task.

## Review Focus

- Does any product layout, fitting, truncation, or row-capacity path retain fixed-cell or context-free measurement authority after migration? Task 5 adds the production-caller audit and recurrence regression.
- Can scale/resource preparation or persistence failure expose mixed layout/atlas state or persist an unpublished scale? Task 6 tests candidate failure and old-state retention.
- Can absent, malformed, or legacy layout scale data lose valid topology, migrate more than once, or let a later layout restore override the global value? Task 2 covers pure conversion/preference semantics; Task 6 proves integrated migration ordering and failure atomicity with font/layout resources.
- Can window/drawable/display transforms be applied twice, mutate app scale, or leave obsolete native/detached-context DPI atlases resident? Task 7 tests independent transforms, preference stability, and bounded replacement.
- Can wrong or corrupt bundled bytes pass as the declared font, silently activate the bitmap fallback, or make the Atkinson/Noto comparison untrustworthy? Task 3 rejects an expected-hash mismatch before resource publication; Task 8 verifies packaged identity, rejects fallback, and owns matched normal-size comparison proof.

---

## Tasks

### Task 1 — Lock the two font candidates and their redistribution provenance

**Files/outputs:** local-only source/provenance staging beneath `_local/typography-font-source/`. Do not add a candidate font to tracked production assets in this task; Task 8 adds only the evidence-selected face and license under `assets/fonts/`. Before that addition, verify that the existing asset boundary and package manifest preserve nested font bytes; if they do not, add only the narrowest typed asset registration/copy rule needed, without adding path loading to UI code. Update `docs/design/editor-typography.md` only if measured source facts require clarifying a candidate constraint.

- [ ] Identify exact upstream static TTF artifacts for Atkinson Hyperlegible and Noto Sans. Record release/revision, source URL/path, binary SHA-256, copyright, OFL 1.1 text, and Reserved Font Name conditions.
- [ ] Confirm each artifact is static TrueType `glyf` data supported by the repository-pinned stb revision; explicitly reject variable or CFF/OTTO artifacts unless the exact pinned parser is proven compatible.
- [ ] Confirm font bytes fit the 2 MiB source bound and inventory the actual shipped editor glyph repertoire against the 512-code-point cap.
- [ ] Verify the current application/asset boundary can supply bounded verified bytes without giving UI/text code a path or filesystem responsibility. If it cannot, specify the smallest typed internal byte-span handoff before proceeding.
- [ ] Preserve candidate binaries and notes locally for comparison only. Do not add either font to production assets until provenance and compatibility are proven.

**Gate:** Both candidates have exact, reproducible source/license provenance and are verified static TrueType candidates within the source-byte bound. Pinned-parser compatibility and repertoire/atlas capacity are proven in Task 3 before either face enters a comparison package. Any licensing uncertainty blocks bundling rather than being guessed around.

**Commit boundary:** No repository commit. This is local-only candidate/source provenance research; retain only its bounded working notes under `_local/typography-font-source/`.

### Task 2 — Establish the global persisted Editor UI Scale and legacy conversion contract

**Files:** `examples/sandbox3d/editor_preferences.h`, `examples/sandbox3d/editor_preferences.c`, `examples/sandbox3d/workspace_tools.h`, `examples/sandbox3d/workspace_tools.c`, `examples/sandbox3d/workspace_persistence.c`, and `tests/test_sandbox3d_workspace.c`. The existing workspace aggregate already owns the scale-preference and geometry-consumer regressions, so no new test registration or editor-layout production change is needed. This task removes saved-layout scale authority and establishes preference/conversion semantics; startup migration publication is wired only in Task 6 after resource preparation exists.

- [x] Add failing tests `editor_scale_accepts_only_four_values` and `workspace_geometry_consumes_global_scale`; assert exactly 100/125/150/200 are accepted, each drives its corresponding workspace geometry, and invalid choices do not alter the prior value.
- [x] Add failing pure-conversion tests `editor_scale_legacy_multiplier_maps_to_supported_percent` and `editor_scale_legacy_multiplier_handles_missing_invalid_extreme_and_tie_values`; use the confirmed legacy member `custom_layout_ui_scale` / key `ui.workspace.custom_layout.ui_scale`, whose stored value is a dimensionless multiplier (default `1.0`, accepted range `0.75`–`4.0`). Assert representative mappings `1.0→100`, `1.25→125`, `1.5→150`, and `2.0→200`; clamp finite low/high multipliers to 100/200 without overflow, quantize to the nearest supported value, and break exact midpoint ties downward. These tests define candidate resolution only; they do not write migration state or publish a scale.
- [x] Add `saved_slot_ui_scale_values_never_override_editor_scale` and `layout_restore_does_not_override_editor_scale`; prove deliberately conflicting per-slot values and later preset/custom-layout/slot restores cannot modify the global preference. Integrated migration ordering and failure tests are owned by Task 6.
- [x] Run the new preference/conversion/layout tests before changing production code and confirm the expected failing assertions.
- [x] Keep only `custom_layout_ui_scale` / `ui.workspace.custom_layout.ui_scale` as the one-time migration input. Preserve all existing layouts and per-slot values verbatim; never infer an active slot from order or topology, and ensure preset/custom-layout/slot restore cannot override the effective global scale. Startup migration ordering is owned by Task 6.
- [x] Keep key-presence resolution outside this pure-conversion task: the converter receives an explicit presence bit and no duplicate key-presence API was added. Task 6 must use the existing `henka_settings_has_key()` authority to distinguish missing legacy/canonical keys from stored defaults. Decouple malformed/non-finite legacy scale from workspace-topology validity so it cannot reject otherwise-valid layout data; preserve slots without inferring a migration source from order/topology.
- [x] Remove workspace presets/custom layouts as scale authorities; stop writing new layout-owned scale values while retaining existing per-slot values unchanged as inert compatibility data.
- [x] Add the editor-preference API/model for the four global values and pure legacy-candidate conversion. Do not persist migration, publish a migrated scale at startup, or expose a Settings control in this task; those actions require the prepared font/raster transaction in Task 6.
- [x] After implementation, rerun the focused workspace/layout suite in Debug and Release.

**Gate:** The preference, pure-conversion, saved-layout-preservation, and layout-consumer tests plus existing workspace/layout suites pass in Debug and Release through `scripts/test_windows.ps1`. All four values affect layout as intended, conversion is deterministic, and ordinary layout restore cannot override the global preference. No startup migration write or scale publication is performed yet.

- [x] After this gate is green, create a local commit containing only Task 2 preference, conversion, layout-consumer, and owning regression changes. Do not publish the partial typography boundary. Completed locally as `4d8ec472`; no push or PR was created.

### Task 3 — Build the opaque font resource and single text measurement/layout authority

**Files:** focused modules `engine/src/ui/ui_font.c`, `ui_text_layout.c`, and `ui_font_internal.h`; `engine/CMakeLists.txt`; test-only static TTF/license fixture and `tests/test_ui_font.c`; and the narrow `examples/sandbox3d/editor_font_bootstrap.c/.h` caller compiled by Sandbox and the test aggregate. `ui_internal.h` and production UI drawing remain for later integration tasks.

- [x] Add executable coverage for invalid source rejection without publication, the pinned static TTF, variable-font-table rejection, malformed/unsupported UTF-8 replacement, text bounds/truncation, and source/glyph/atlas limits.
- [x] Exercise malformed-font parsing and undersized-atlas rejection; inject failure at each font-resource allocation and prove output remains unpublished with allocation counts restored. Atlas dimensions and bootstrap byte limits have explicit negative controls.
- [x] Add the owning app-bootstrap regression: wrong pinned SHA and modified bytes fail with identity/stage diagnostics before allocation/publication; oversized bytes fail at the source limit; matching bytes and identity construct the opaque resource.
- [x] Run the new test-first boundary before implementing the bootstrap owner. The intended RED was an unresolved `sandbox3d_editor_font_bootstrap_create` link symbol; this was a missing owner implementation, not a runtime assertion failure.
- [x] Implement an opaque font resource from a bounded byte span and stable identity. The application bootstrap verifies the hash; the resource constructor parses/rasterizes verified bytes and does not resolve or open paths. `ui.c` remains untouched.
- [x] Use the repository-pinned `stb_truetype` and `stb_rect_pack`; implementation macros are confined to `ui_font.c`, with no additional font dependency.
- [x] Implement one text-layout run authority for advances, bearings, baseline/line bounds, kerning, truncation/fitting, and glyph placements; regressions compare the run's measured advance against the emitted placements.
- [x] Keep source, glyph, atlas, and text capacities bounded; replacements are deterministic; failed parse/packing/allocation leaves no published resource and does not leak Henka allocations.
- [x] Run aggregate `henka_tests` in Debug and Release through `scripts/test_windows.ps1`; both passed on the final Task 3 source/test candidate.

**Gate:** `henka_tests` focused UI/font tests prove measurement equals emitted layout bounds, deterministic replacement, hard limits, and atomic resource failure. No production font path is active yet.

- [x] After the gate was green, create local-only commit `0282b2cd77e89e7b67984c0cdf44a0f54df0fad7` containing only Task 3 font-resource, text-layout, and owning test/build-registration changes. Do not publish the partial typography boundary.

### Task 4 — Add scale/output-specific raster profiles and ordered mixed glyph/solid drawing

**Files:** `engine/src/ui/ui_internal.h`, `engine/src/ui/ui.c`, `engine/src/renderer/renderer_opengl.c`, the UI shader declarations in the renderer, focused UI/renderer tests, and `tests/CMakeLists.txt`.

- [x] Add failing tests `ui_atlas_uses_effective_raster_pixel_size`, `ui_atlas_identity_includes_font_app_output_raster_parameters_and_context`, `ui_atlas_cache_is_bounded_per_context`, and `ui_context_profile_replacement_is_atomic`.
- [x] Run the new renderer/profile/order tests before implementation and confirm their expected failing assertions.
- [x] Generate/cache glyph atlases before ordinary frame drawing at the actual requested app and output raster sizes. Never scale one 100% atlas to represent larger app scales. The cache identity is the tuple of font content identity, app-scale percentage, effective output-raster parameters (including glyph pixel size), and graphics-context residency identity.
- [x] Replace the separate solid-rectangle/text draw ordering with a bounded insertion-ordered UI quad/command representation that distinguishes solid-color and atlas-sampled glyph quads. Preserve existing line/triangle semantics and all UI painter order.
- [x] Make the renderer sample glyph coverage and apply the existing text color/alpha. Keep atlas GPU objects local to their owning graphics context; CPU font metrics/layout remain shared.
- [x] Enforce one output-raster profile per context, at most four app-scale atlas entries per profile, at most six UI contexts, 96 MiB active aggregate coverage, and 192 MiB candidate replacement peak. Obsolete DPI profiles are retired after successful replacement, never accumulated.
- [x] Keep the existing public UI draw/measure entry points behaviorally compatible where their contract permits, and route both through the single internal text-layout result from Task 3. Do not add font paths or asset ownership to these APIs.
- [x] After implementation, run the focused UI/renderer tests in Debug and Release.

**Gate:** Focused UI/renderer tests prove mixed draw order, color/alpha, per-context ownership, effective raster identity, and bounded atlas replacement. No app-scale setting is exposed in this task.

- [x] After this gate is green, create a local commit containing only Task 4 raster-profile, ordered-rendering, and owning regression changes. Do not publish the partial typography boundary.

### Task 5 — Migrate product measurement and layout callers to the font authority

**Files:** all production callers, including `examples/sandbox3d/editor_layout.c`, `examples/sandbox3d/main.c`, `examples/sandbox3d/view_compass.c`, and `examples/sandbox3d/script_editor.c`, plus any additional callers found by a full repository search; `engine/include/henka/ui.h` only if the public contract can be preserved without exposing font ownership; relevant tests in `tests/test_ui.c`, `tests/test_sandbox3d_editor_layout.c`, `tests/test_sandbox3d_editor_ui.c`, and `tests/test_sandbox3d_workspace.c`.

- [ ] Inventory raw/context-free `henka_ui_measure_text`, fixed-cell width formulas, truncation, and row-capacity consumers. Add a narrow architecture regression that rejects new product callers of the legacy path.
- [ ] Run the new authority regression before migration and confirm it detects the current product callers that violate the desired single measurement authority.
- [ ] Route product layout, fitting, ellipsis, Scene Objects rows, Object Details, tabs, buttons, status, Utility, and editor overlays through the same active font/layout run authority used to draw.
- [ ] Convert any product caller that lacks a UI context to receive/use the owning context or the same canonical editor-font authority; do not add an independent width formula. Keep the context-free fixed-cell API/table only as explicitly classified bitmap-fallback or isolated regression support, never as a Sandbox/product-layout authority.
- [ ] Make the bounded architecture regression fail if a Sandbox production source file reintroduces a call to the context-free fixed-cell measurement path; packaged acceptance must also fail if the emergency bitmap text path is active.
- [ ] Revalidate existing editor contracts at 1280×720, 1920×1080, and a wider layout, including long names, complete `[Hidden]`, narrow rows, toolbar, scrolling, buttons, status, and hit geometry. Do not change the hidden-row threshold or add panel-specific spacing patches.
- [ ] Run the affected editor layout/UI regressions in Debug and Release.

**Gate:** No unclassified production width authority remains; layout, fitting, truncation, row capacity, hit geometry, and drawing use the active font/layout authority; the bitmap path is not silently active.

- [ ] After this gate is green, create a local commit containing only Task 5 production-caller migration, architecture regression, and affected layout/UI tests. Do not publish the partial typography boundary.

### Task 6 — Apply Editor UI Scale and startup migration transactionally; expose Settings control

**Files:** focused `examples/sandbox3d/editor_scale_control.c` and `.h`, the editor-preference/resource owner established in Task 2, startup/workspace-restore integration in `examples/sandbox3d/workspace_persistence.c` and its narrow owner call, narrow Utility > Settings wiring in `examples/sandbox3d/main.c`, `tests/test_sandbox3d_editor_scale_control.c`, and focused preference/UI/renderer/workspace integration tests in `tests/` and `tests/CMakeLists.txt`.

**Interface:** `henka_result sandbox3d_editor_scale_control_draw(henka_ui_context* ui, henka_ui_rect bounds, uint16_t current_percent, uint16_t* out_selected_percent, bool* out_changed)` returns UI/render errors distinctly from an unchanged frame. `out_selected_percent` is initialized to the current value and changes only after a different supported selection. This preserves the editor's existing error-reporting contract instead of conflating draw failure with no interaction. The control owns presentation and keyboard-operable selection only; it does not persist preferences, prepare resources, or own scale state. The existing editor preference/resource owner consumes the candidate and performs the transition.

- [ ] Add failure-injection tests `editor_scale_candidate_prepare_failure_retains_old_state`, `editor_scale_persist_failure_retains_old_state`, `editor_scale_later_context_failure_retains_all_old_profiles`, `editor_scale_startup_migration_prepares_resources_before_persist_and_layout_restore`, `editor_scale_startup_migration_failure_preserves_legacy_state_and_retries`, `editor_scale_invalid_canonical_uses_legacy_only_while_migration_pending`, and `editor_scale_invalid_canonical_after_migration_uses_default_without_legacy_read`; assert each failure stage, unchanged settings/migration marker where required, unchanged prior metrics/layout/atlas identities, no mixed-scale frame, valid-canonical precedence, and successful later retry. Also prove a valid canonical percentage is retained when a pending marker is completed, and unsupported marker data fails closed without overwrite.
- [ ] Add scale-control tests for exactly 100/125/150/200, keyboard operation, current-value indication, and no emitted selection when the current value is reselected. Prove the control reports a candidate without mutating or persisting app state itself.
- [ ] Run the new transaction/control regressions before implementing the scale transition and Settings control; confirm each fails at the intended stage.
- [ ] Implement one candidate-build-and-publish path for both user-selected scale changes and startup migration. Resolve startup state as follows: a valid canonical percentage always wins; if the migration marker is pending, preserve that valid value or else convert the legacy global scalar (defaulting to 100%); after marker version 1, never read legacy scale data, and recover a missing/invalid canonical value to 100%. Keep the current complete baseline live while building shared metrics, required atlases for every live context, and candidate workspace geometry from preserved layout topology. For migration only, atomically persist the canonical key and `ui.editor.scale_migration_version = 1` after preparation; then publish preference, metrics, layout geometry, and context profiles in a no-fail swap before ordinary workspace restore can publish conflicting geometry. For a user scale selection, persist only the canonical preference after preparation and publish through the same no-fail swap. No fallible resource work remains after persistence succeeds.
- [ ] During startup migration, use the existing `henka_settings_has_key()` authority to distinguish absent keys from stored defaults; do not add a competing presence API.
- [ ] On migration preparation or persistence failure, retain the prior complete usable state (the prepared 100% default at cold start when no canonical preference exists), leave all legacy layout records and the migration marker unchanged, do not expose the candidate scale, report the precise failing stage, and retry migration at the next startup. A malformed legacy scalar alone must not reject or rewrite valid layout topology.
- [ ] For ordinary scale selection, use the same preparation/persistence/no-fail-publication sequence; any preparation, later-context upload, or settings failure retains the old preference and complete runtime state.
- [ ] Implement a real Utility > Settings Editor UI Scale control exposing exactly 100%, 125%, 150%, and 200%, with one-click selection, keyboard operation, and a persistent visible indication of the current value. Wire its selected candidate through the existing preference/resource owner; keep `main.c` to a narrow call and do not put font, layout, persistence, or atlas ownership there. This is product UI, not a test-only scale switch.
- [ ] Run the focused preference/control/renderer tests and prove a selected scale applies immediately, persists, survives restart, and does not expose mixed metrics or atlas state.
- [ ] After implementation, rerun all focused scale, migration, renderer, and workspace integration tests in Debug and Release.

**Gate:** Startup migration and user scale selection both use one candidate transaction. Failure controls retain the prior complete scale/font/layout state and migration records; successful migration is fully prepared, atomically persisted, and published before workspace restore can expose the new scale. A real Settings selection persists exactly one supported value only after every required resource is ready.

- [ ] After this gate is green, create a local commit containing only Task 6 scale control, preference/resource transaction, migration integration, and owning tests. Do not publish the partial typography boundary.

### Task 7 — Separate OS display scale from app scale and prove coordinate transforms

**Files:** pinned SDL integration in `engine/src/platform/platform_sdl.c`, app-scale derivation removal in `examples/sandbox3d/main.c`, UI logical/draw coordinate handling in `engine/src/ui/`, corresponding renderer output sizing in `engine/src/renderer/renderer_opengl.c`, and deterministic platform/UI transform tests in `tests/`.

- [ ] Verify the exact pinned SDL source revision (`d9d5536704d585616d4db3c8ba3c4ff6fc2757e1` at the design base) and its semantics for window logical size, pixel/drawable size, display scale, and pointer coordinates before choosing calls. Use `SDL_GetWindowDisplayScale` only if that exact revision supports the needed behavior.
- [ ] Add red deterministic tests `display_scale_forward_draw_transform` and `display_scale_inverse_hit_test_transform` at 1.25, 1.5, and 2.0; cross-check each independently against all four app-scale values without folding display scale into persisted preference.
- [ ] Run the new transform tests before implementation and confirm they expose the missing/noncanonical conversion behavior.
- [ ] Retire framebuffer-ratio-derived app-scale writes and `henka_ui_effective_text_scale`'s 1600×900 readability multiplier. Window resizing changes available layout extent only.
- [ ] Audit all production scale reads/writes and derived-text-scale paths. Retire every framebuffer/window-ratio, resolution/readability, or display-scale heuristic that can overwrite or multiply Editor UI Scale; retain display scale only for output-coordinate conversion and effective raster sizing. Add `window_or_display_scale_changes_do_not_mutate_editor_scale` covering resize and display-scale changes.
- [ ] Handle a per-context display-scale change with a complete candidate raster profile and bounded replace/reuse. Exercise main and detached/native contexts; prove old context-local GPU resources are retired only after candidate readiness and cannot accumulate across display moves.
- [ ] Add `display_scale_context_replacement_reuses_and_retires_profile` to prove repeated main/detached-context display moves retain only the current profile plus the bounded in-flight replacement and release superseded GPU residency after publication.
- [ ] Add a real packaged non-100% OS display-scale case when the environment supports it. Hold Editor UI Scale and logical window dimensions constant and vary only actual OS display scale; verify output transform, effective glyph raster profile, drawn geometry, and pointer hit geometry. If unavailable, retain deterministic mathematical coverage and classify the packaged DPI case as environment-unavailable rather than substituting app-scale tests.
- [ ] After implementation, rerun the owning platform/UI tests in Debug and Release, then attempt the separate real-DPI package control.

**Gate:** Deterministic transform tests pass; rendered geometry and pointer hit regions agree under non-1.0 display scaling; changing display scale never rewrites the persisted app preference; detached context transition is bounded.

- [ ] After this gate is green, create a local commit containing only Task 7 display-scale transform/profile changes and owning tests. Do not publish the partial typography boundary.

### Task 8 — Package matched font comparison and select the production face

**Files:** selected font and license/provenance under `assets/fonts/`, package asset manifests/copy rules, runtime font identity/scale telemetry, and a focused `scripts/test_editor_typography_windows.ps1` only if the existing packaged harness cannot express this workflow without accumulating another responsibility in `check_packaged_sandbox3d_windows.ps1`. Store comparison captures under `_local/typography-comparison/`; they are local review evidence, not public assets or GitHub artifacts.

- [ ] Use a current exact candidate and the normal build/package scripts; package Atkinson and Noto only in the local comparison package with separate verified identities. Final production package contains only the selected face.
- [ ] At 1280×720 and a wider reference resolution, capture matched editor state, text size, scene, layout, capture path, and the same recorded OS display scale at app scales 100/125/150/200 for both fonts. Include body/status text, tabs, buttons, Scene Objects, Object Details, numerals/punctuation, `I/l/1`, `O/0`, truncation, and compact layouts. Keep this font comparison distinct from Task 7's OS-DPI control.
- [ ] Inspect native working-size captures first; enlarged specimens are secondary. Review legibility, clipping, overlap, useful information density, and metric stability; present the matched native-size Atkinson/Noto comparison for owner review before locking the production face, since this is a user-facing readability preference.
- [ ] Verify package/runtime font hash and license match the exact asset provenance. Add missing/corrupt/unsupported font package negative controls and require explicit fallback telemetry plus package rejection.
- [ ] Exercise Settings scale interaction, keyboard selection, resize, save/reopen, reset, legacy migration, main/native/detached contexts, and supported real display-scale transition in the packaged product. Capture before/after at identical resolution and OS scale.
- [ ] Run focused Debug and Release `scripts/test_windows.ps1` gates, relevant UI/renderer integration tests, `scripts/build_windows.ps1` for the Sandbox target, `scripts/package_sandbox3d_windows.ps1`, the package contract/startup checks, the typography workflow, repository integrity/hygiene, exact-candidate provenance, and `git diff --check` on the exact candidate.

**Gate:** Human-readable normal-size matched captures justify the selected face; all app/display scales and layout regressions pass; package proves bundled-font identity/hash/license and rejects fallback; exact candidate and executable/package provenance are mutually consistent. Only then is the typography slice ready for its normal publication/review lifecycle.

- [ ] After this gate is green, create the final coherent typography publication commit from the validated task commits and exact tested tree. Then follow the standing PR lifecycle: review the entire delta, require exact-head hosted checks, resolve valid review findings, and merge only when all required gates are green.

## Plan Approval Boundary

The owner approved native implementation on 2026-10-11 after the current PR #31 publication boundary closed. The plan's task gates remain mandatory and sequential: execute them from the reconciled `main` base, prove each regression before implementation, and do not treat any planned test or package proof as already performed.
