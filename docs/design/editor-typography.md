# Editor Typography and UI Scale Foundation

**Typography authority:** Atkinson Hyperlegible Regular is the production editor face; Noto Sans is reserved for explicit comparison validation. Text measurement, fitting, truncation, layout, and drawing share one font authority.
**Editor scale:** The persisted global Editor UI Scale offers 100%, 125%, 150%, and 200%. Operating-system display scale remains a separate output transform.
**Validation boundary:** Deterministic non-1.0 display-scale transforms are covered. Packaged behavior at a non-100% operating-system display scale remains a separate open validation item until the environment supports it.
**Scope:** Bundled typography, editor-scale authority, and their application to Henka editor UI.
## Problem and intent

Henka's earlier editor used fixed 5×7 bitmap glyphs expanded into UI rectangles and fixed-width measurement. Workspace scale also followed the framebuffer-to-window ratio rather than an independent persisted preference. The current text subsystem uses bundled font resources and shared text metrics; the sections below define its ownership, compatibility, and validation requirements.

The goal is a bundled, predictable, readable editor font with one canonical text-layout authority. Font measurement, fitting, truncation, layout decisions, glyph placement, and rendering must agree. A real, persisted Editor UI Scale of 100%, 125%, 150%, or 200% must affect workspace geometry and text together. Operating-system display scale is a separate raster/output concern and must not be applied twice.

## Design direction and constraints

- Reuse the already-pinned `stb` FetchContent revision (`2c980bb59875b0d32144a71867fbdebb2f77cd20`) for `stb_truetype` and `stb_rect_pack`; do not add FreeType or a second font-library pin.
- Build a focused Henka UI font/text subsystem. Do not place substantial font parsing, atlas management, or text-layout responsibility in `ui.c`.
- Completed matched local captures for Atkinson Hyperlegible Regular and Noto Sans Regular at 1280x720 and 1920x1080, with Editor UI Scale at 100%, 125%, 150%, and 200%. Both candidates remained readable at normal working size, with no decisive readability winner. Atkinson Hyperlegible Regular is selected for production because it was the owner's leading candidate before comparison.
- Bundle the selected font files and exact redistribution license/provenance. Do not rely on installed system fonts. Verify the exact chosen font file, version, format compatibility, license text, and any reserved-name conditions before distribution.
- Preserve the current bitmap glyphs only as an explicitly reported emergency fallback during migration or resource failure. Never silently fall back. Packaged acceptance must prove the bundled font is the active text path.
- Keep the existing hidden-row contract unchanged: names remain canonical, and the complete textual `[Hidden]` marker remains visible.
- Do not expand scope into international text shaping, complex scripts, rich text, or game-runtime typography.

## Authority model

```text
Persisted Editor UI Scale ──┬──> canonical workspace/layout geometry
                            └──> canonical text metrics and fitting
Window logical size/input ─────> editor logical coordinate space
Display scale/drawable size ───> logical-to-pixel output and glyph raster size
Bundled font bytes ────────────> shared CPU font/metrics resource
Shared text layout run ────────> measurement, truncation, glyph placement
Per-graphics-context atlas ────> internal renderer texture residency
```

### Font bootstrap authority

Core UI/text code never opens or resolves arbitrary font paths. The application/package bootstrap identifies the verified bundled font through Henka's existing asset/application resource boundary, obtains bounded font bytes there, verifies the expected identity/hash, and passes a byte span plus stable identity to the opaque UI font-resource constructor. The resource constructor parses bytes only; it has no filesystem, package-path, Sandbox, or host-font responsibility. `ui.c` remains a drawing/command consumer and does not acquire filesystem, package-path, font-discovery, or Sandbox-specific bootstrap ownership. If the existing asset boundary has no suitable byte-span operation, add only the narrowest typed internal asset-to-UI handoff; do not expose a general arbitrary-path font loader or widen the public texture API.

The bundled production asset is a static TrueType-outline `.ttf`. Reuse the repository-pinned `stb` revision, but verify the exact static font file with the pinned `stb_truetype` parser before selecting it. Do not use an OTF/CFF or variable-font file unless compatibility is explicitly proven against that exact pinned parser. Record exact upstream revision/release, source path, binary SHA-256, copyright, OFL 1.1 text, and Reserved Font Name conditions for both comparison candidates and the final bundled face.

### Editor scale and OS display scale

Editor UI Scale is one global persisted editor preference with exactly four values: **100%, 125%, 150%, and 200%**. The editor-preference/settings authority owns it. Workspace and layout models consume the value as an input to geometry; no workspace preset, named layout, custom layout, or detached-panel layout owns or overrides it. Expose a one-step, keyboard-accessible control in Utility > Settings. A selection applies immediately and persists only after all required text metrics and raster resources for the candidate scale are ready.

The app scale controls editor control size, spacing, rows, and text as one coherent system. Window resize changes available layout extent, not the selected app scale. Display scale is a separate platform output transform that maps editor logical coordinates to drawable pixels and determines raster resolution; it must not silently rewrite the persisted app preference. Input hit testing, drawing, and reported geometry use one documented transform, applying app and display scale exactly once.

#### Legacy scale migration and retirement

Migration is itself a scale transition, not an earlier preference-only write. Resolve the candidate in authority order (valid canonical setting, otherwise the one-time legacy global scalar, otherwise the 100% default), then prepare candidate metrics, required context atlases, and workspace geometry before persisting the canonical key or migration version. Keep the prior complete state usable during preparation. Persist the canonical key and migration version atomically or through rollback-capable settings, then publish scale, metrics, geometry, and context profiles with a no-fail swap; complete this before workspace restore can publish geometry under another scale. On preparation or persistence failure, retain the prior complete state, leave legacy records and the migration marker untouched, report the failing stage, do not expose the unpersisted candidate, and retry migration at a later startup.

Introduce one canonical integer-percent setting (`ui.editor.scale_percent`) whose only valid values are `100`, `125`, `150`, and `200`. When it is absent, perform a one-time migration before workspace layout restore. At this design base, the legacy workspace model member is `custom_layout_ui_scale` and its persisted setting key is `ui.workspace.custom_layout.ui_scale`. The legacy scalar is a dimensionless multiplier (the current default is `1.0`; the current accepted range is `0.75`–`4.0`), not a percentage. For a finite legacy scalar, convert the multiplier to percent, clamp to 100–200%, then map to the nearest supported percentage, breaking exact ties toward the smaller scale. Implement the clamp without overflowing on large finite legacy values. A missing or non-finite legacy scalar defaults to 100%. The current persisted workspace schema also stores per-slot `ui_scale` multipliers but does not persist which saved slot was last applied, so those per-slot values are not a safe migration source and must not be guessed from slot ordering or topology. Preserve all saved layout records and per-slot values unchanged; after the one-time migration, legacy scale fields are inert compatibility data and never let layout restore override the canonical global preference. A missing, malformed, or out-of-range legacy scale alone must not invalidate otherwise-valid saved topology. Persist the canonical key and migration version as one atomic or rollback-capable settings transaction before normal layout changes. If the transaction fails, leave legacy settings/layout records and the migration marker untouched, report the failing stage, retain the previous complete working state (the ready 100% default state on cold start if no canonical value exists), do not expose the unpersisted migrated candidate, and retry migration at a later startup.

Use `ui.editor.scale_migration_version` as the migration completion key: absent or `0` means pending, and `1` means the legacy workspace scale authority has been retired. Any other stored marker value is unsupported/malformed, is diagnosed, and fails closed without overwrite. While pending, a valid canonical percentage wins; otherwise, convert only the legacy global scalar (or choose 100% when it is missing/non-finite), then atomically persist the canonical percentage and marker `1` after all candidate resources are prepared. A valid canonical value with a pending marker is preserved while the marker is completed through the same prepared transaction. Once marker `1` is present, never consult legacy scale data again. A missing or invalid canonical percentage after that point is diagnosed and recovers to 100% through the normal resource-safe preference transaction, without reactivating legacy layout values. Migration failure leaves both canonical/migration settings and all legacy layout data unchanged.

Keep existing panel arrangements, dock widths, topology, and other layout state intact. The one-time migration may read only the legacy global `custom_layout_ui_scale` scalar and its persisted key. Per-slot `ui_scale` values are preserved verbatim but are never migration inputs or restore-time authorities because the saved data has no active-slot identity. After migration, all legacy scale fields are inert compatibility data: they are ignored by preset/custom-layout restore and are no longer written as layout-owned state. A valid canonical global preference always wins. Regression coverage must prove the scalar migrates once, deliberately conflicting per-slot values cannot change the global scale, and legacy layouts remain otherwise unchanged.

Audit every current read/write and derivation of editor, workspace, text, framebuffer, and display scale. Retire and supersede every framebuffer-ratio, resolution/readability, or display-scale heuristic that would overwrite or multiply the canonical app scale. This includes the per-frame framebuffer-derived workspace-scale assignment and `henka_ui_effective_text_scale`'s 1600×900 readability factor, plus any other path found by the audit. Window resizing is not an app-scale change. OS display-scale conversion may affect only the logical-to-pixel output transform and effective glyph raster parameters; it must never mutate or multiply the persisted Editor UI Scale.

Before implementation, verify the exact pinned SDL source revision (`d9d5536704d585616d4db3c8ba3c4ff6fc2757e1` at this design base) and the semantics of its window-size, pixel-size, display-scale, and pointer-coordinate APIs. Use `SDL_GetWindowDisplayScale` only if that exact revision provides suitable behavior; do not infer availability from a version label. Add deterministic non-1.0 draw/hit-transform regressions and a real non-100% display-scale package run whenever the validation environment supports it. Exercise OS display scale independently from Editor UI Scale, then cross-check their composition without allowing either transform to be applied twice. The API references are [SDL_GetWindowDisplayScale](https://wiki.libsdl.org/SDL3/SDL_GetWindowDisplayScale) and [SDL3 high-DPI guidance](https://wiki.libsdl.org/SDL3/README-highdpi).

### Font and metrics authority

Create an opaque Henka UI font resource and a focused text-layout module. The font resource owns validated font bytes, parsed face metrics, supported glyph records, and scale-specific CPU atlas data. The renderer owns only the GPU texture residency for the active font atlas within its actual graphics context. Main, native-panel, and detached-panel UI contexts share the same font/metrics authority; graphics-context-specific texture handles are not shared as if they were portable.

Do not widen the general/public texture API to prototype glyph rendering. Add the narrowest UI-owned resource/binding boundary that the editor and renderer need. Font load, parse, atlas-pack, upload, and destruction failures must be observable and must leave ownership/lifetime unambiguous.

One text-layout routine is authoritative for:

- glyph advances, bearings, baseline and line height;
- string measurement and line bounds;
- fitting and ellipsis/truncation decisions;
- row/label capacity decisions;
- glyph quad positions and UVs consumed by drawing.

Measurement and drawing may request different output from the same routine, but neither may maintain an independent width formula. Kerning supported by the chosen single-face path is included consistently. Input UTF-8 is validated; unsupported or malformed code points map to a visible replacement glyph rather than disappearing. The initially supported repertoire is bounded and explicitly tested (ASCII, required UI punctuation/symbols, and any Latin characters already present in shipped editor strings). This is not a shaping or internationalization claim.

Before implementation, inventory every production caller of raw/context-free `henka_ui_measure_text` and related fixed-cell width calculations. Product layout, fitting, truncation, row capacity, and drawing must use the active font/text-layout authority. The legacy fixed-width API/table may remain only as explicitly classified emergency-bitmap fallback or isolated regression support; it cannot remain an alternate product-layout authority. Migrate every production caller, including existing context-aware and context-free call sites, and add a narrow regression/architecture check if it can reliably detect reintroduction.

### Rasterization, atlases, and renderer ordering

Use `stb_truetype` to rasterize glyphs at actual pixel sizes appropriate to the selected app scale and current output scale. Use `stb_rect_pack` for bounded atlas packing. Do not enlarge a low-resolution atlas to simulate 125%, 150%, or 200% readability. Rasterize/cache before ordinary frame drawing, not once per frame.

Initial hard bounds are: **2 MiB maximum source bytes per face; 512 supported code points including replacement; 2048×2048 maximum single-channel coverage atlas; four app-scale atlas entries for one effective output-raster profile per graphics context; one active output-raster profile per context**. A display-scale transition builds a complete replacement profile before publication, then retires the prior profile; it may not retain additional output profiles. Thus active atlas coverage is capped at 16 MiB per graphics context, with a bounded temporary replacement peak of 32 MiB per context. The current engine permits at most five tool windows plus the main window, so the initial UI-font residency contract is at most six graphics contexts: **96 MiB aggregate active atlas coverage and 192 MiB maximum while atomically preparing a full-context replacement**. Cache cardinality is therefore bounded at 24 scale-specific atlases total, with one effective output-raster profile per context. The implementation must verify that the renderer's actual GL-context ownership cannot exceed this existing window bound; if that engine limit changes, update and test the aggregate cap rather than allowing an unbounded cache. Validate the audited glyph repertoire against these bounds before implementation; if it does not fit, revise the limit from measured evidence, never by silently dropping glyphs.

Atlas/cache identity includes font content identity, selected app-scale percentage, effective output-raster parameters (including actual glyph pixel size), and graphics-context residency identity. Shared CPU metrics/layout remain common across main/native/detached UI contexts; GL texture objects are local to each actual graphics context. A context moving to another output display scale performs a bounded candidate-profile replace/reuse and evicts the old profile only after the new profile is ready. A packing/capacity/allocation failure is an explicit error; it must not silently drop glyphs or switch the product to the bitmap fallback.

An app-scale change is transactional across every live UI graphics context: build and validate candidate metrics plus each context's required atlas state first. Persist the candidate preference through an atomic or rollback-capable settings operation only after all resources are ready; then publish the scale, workspace geometry, shared metrics, and context profiles as a no-fail state swap. No fallible resource work may remain after persistence succeeds. If resource preparation or persistence fails, retain the previous persisted preference and the previous complete runtime scale/font state, and report the failing stage. A frame may never combine new layout metrics with stale atlas metrics. A display-scale change follows the same candidate-build-and-publish rule for the affected context, then retires/reuses its prior profile only after the replacement is ready.

Preserve the existing UI paint order. The current bitmap text enters the solid-rectangle stream, so textured glyph quads must retain insertion order relative to solid quads; drawing all glyphs as a final overlay would be a visual regression. A narrow ordered quad representation can distinguish solid-color quads from atlas-sampled glyph quads while keeping triangle/line ordering semantics unchanged. The UI renderer samples glyph alpha and applies the caller's existing text color/alpha.

## Font selection and comparison protocol

Atkinson Hyperlegible is the lead candidate; Noto Sans is the control/comparison. Candidate provenance begins at the official upstream sources ([Atkinson Hyperlegible](https://github.com/googlefonts/atkinson-hyperlegible), [Noto Sans](https://github.com/google/fonts/tree/main/ofl/notosans)), but these links do not substitute for verifying the exact bundled files and licenses.

Render both candidates from the real Henka editor using the same executable state, scene, resolution, text size, layout, and capture path. Compare at app scale **100%, 125%, 150%, and 200%**. Use a normal editor view at 1280×720 as the minimum legibility/layout target and an ordinary wider view (for example 1920×1080) to catch unnecessary compression or expansion. Record the OS display scale separately from the app scale. Capture before/after at matched conditions; do not use screenshot overlays or extreme enlargement as primary readability proof.

For every paired Atkinson/Noto comparison, hold the actual OS display scale constant and record it. OS display-scale proof is a separate control: hold Editor UI Scale and logical window dimensions constant while varying only the real OS display scale, then verify the output transform, effective raster profile, drawn geometry, and pointer hit geometry. If the environment cannot provide a real non-100% display scale, classify that packaged case as unavailable; never substitute an Editor UI Scale change for it.

The comparison must include normal-size body/status text, tabs, buttons, Scene Objects, Object Details, numeric values, punctuation, `I/l/1`, `O/0`, narrow rows, truncation, and compact layouts. Inspect actual working-size captures for legibility, practical metrics, clipping, overlap, and layout stability. Choose the winner from that evidence, not family resemblance or enlarged glyph specimens. Package only the selected font in the final product; comparison candidates may both appear in a local validation package if their provenance is independently recorded.

### Comparison outcome

Matched packaged captures at 1280×720 and 1920×1080 covered Editor UI Scale 100%, 125%, 150%, and 200%, with operating-system display scale held at 100%. Both Atkinson and Noto were readable at normal working size, with no clear readability winner; Atkinson is the selected default by preference, not a claim of objective superiority. The comparison manifest records the product executable's build commit, source identity, and hash separately from the clean validation-harness commit, source identity, and hashes, along with font hashes, restart persistence, and the capture set. It never relabels an older executable as built from a newer harness commit.

When the validation harness is newer than the packaged executable, the capture path compares the two Git commits before launch. Cross-commit reuse is accepted only when the complete intervening diff is limited to explicitly approved editor-typography capture scripts, tests, and non-runtime documentation; product source, assets, build definitions, package inputs, or unknown paths force a rebuild. The comparison manifest records both commit identities and the exact non-product changed paths. This bounded source-equivalence check supplements the executable hash and package/build manifests; it does not rewrite their provenance.

## Persistence, settings, and failure behavior

Utility → Settings contains a real, one-click, keyboard-accessible Editor UI Scale control with exactly four choices: 100%, 125%, 150%, and 200%. Selecting a value immediately updates layout and font raster/metrics together and persists through the global editor-settings authority; it is not a test-only switch and does not persist in a workspace preset. The selected value remains clear in Settings and in bounded diagnostic/package telemetry. Reset behavior must be explicit and covered by regression tests. Resource preparation precedes preference publication and persistence; injected preparation or save failure must leave the prior complete working state intact and report the failed stage.

The selected font resource loads from Henka's bundled asset path through the normal product/package path. Missing, corrupt, unsupported, or over-limit font data produces a clear diagnostic with the failed stage and asset identity. During migration, an emergency 5×7 fallback may preserve basic operability only if the fallback is explicitly reported to logs/status/telemetry. It is not a successful font acceptance path. The packaged checker must fail if the expected bundled font identity/hash is not active or if emergency fallback was used.

The package includes the exact required font license/provenance. Asset hashes and runtime-reported active font identity must agree with package provenance. No private local path is emitted in public source, docs, logs, or CI.

## Regression and acceptance gates

### Text subsystem

- Measurement bounds match the glyph positions/bounds emitted from the same layout run, including kerning, spaces, punctuation, and truncation.
- Invalid UTF-8 and unsupported glyphs produce deterministic visible replacement behavior.
- Font loading, atlas packing, glyph-count, byte-count, atlas-size, cache-count, and allocation-limit negative controls fail explicitly without leaks or partial resource publication.
- Ordered mixed solid/glyph quad rendering preserves UI paint order and alpha/color behavior.
- Main/native/detached contexts use one CPU font authority and release context-local GPU resources safely.
- Scale/display-profile replacement failure leaves the old metrics, layout, atlas, and persisted scale usable; no mixed-scale frame is emitted.
- The bitmap emergency fallback is separately testable and never reported as the bundled-font path.

### Scale, layout, and persistence

- Test all four supported values at 1280×720, 1920×1080, and at least one wider layout; also test window resize across layout breakpoints without changing the persisted scale.
- Prove pointer hit regions track the same scaled geometry that is drawn, with no clipping, overlap, unreachable controls, or off-panel interaction.
- Re-run row height, truncation, long Scene Object name, `[Hidden]` marker, tabs, buttons, Object Details, Utility, status, compact-toolbar, and scrolling regressions. Keep the existing hidden-row threshold untouched.
- Save/reload and reset tests prove the exact selected scale and workspace/layout state survive without precedence ambiguity.
- Only the legacy global custom-layout scale scalar can provide the one-time migration value; saved per-slot scale values are preserved unchanged and never consulted. Panel topology remains unchanged, and subsequent workspace restore cannot override the global preference.
- Deterministic input/draw coordinate-transform regressions cover non-1.0 display scales including 1.25, 1.5, and 2.0, independently cross-checked against all four app-scale values. A real non-100% display-scale packaged case is required whenever the validation environment supports it; hold Editor UI Scale and logical window dimensions fixed in that case. OS/display scale is separate from the four app-scale values, and app-scale testing cannot satisfy the OS-scale gate.

### Product/package proof

- Build from an exact candidate and package from that build with truthful source and executable provenance.
- Prove the packaged editor loads the bundled font and reports its exact identity/hash and selected app scale; deliberately missing/corrupt-font controls prove fallback is visible and rejected by packaged acceptance.
- Capture matched Atkinson/Noto views at the four app scales with OS display scale recorded. Inspect native working-size captures and matched before/after views.
- Exercise native/detached UI contexts through a display-scale profile replacement when feasible, proving replacement is atomic and old GPU residency is bounded and retired only after the new profile is ready.
- A detached OpenGL window must not be visually certified by Win32 `PrintWindow`: it can return the native frame and a blank client surface. Until an application-owned detached framebuffer capture exists, report detached font/scale identity and a subsequent engine `render-complete` after tool-window draws while the detached test window remains hidden and non-foreground; mark detached screenshots as not captured.
- Run package contract, startup, settings interaction, resize, row/truncation, and UI hit-test workflows on the exact packaged executable.
- Verify the font and license are present in the package and no host-installed font is required.

## Authority clarifications

These clarifications are normative where earlier prose could be read differently:

1. **One Editor UI Scale authority.** The only app-scale preference is global persisted editor state with exactly 100%, 125%, 150%, or 200%. Workspace and layout models consume it; presets, custom layouts, saved slots, startup restore, and detached layouts cannot own, persist, restore, or override it. Existing `custom_layout_ui_scale` / `ui.workspace.custom_layout.ui_scale` is a one-time legacy migration source only. Preserve all layout records, topology, dock geometry, and per-slot values. Run migration before layout restore; invalid/missing legacy scale alone does not invalidate valid topology. Migration is itself a scale transition: prepare candidate metrics, required context atlases, and workspace geometry before atomically persisting the canonical key/version, then no-fail publish before workspace restore; on failure retain the prior complete state and legacy records/marker and retry. After migration, retire legacy scale writes and restore behavior. Retire every framebuffer-ratio, resolution/readability, and display-scale heuristic that could overwrite or multiply the canonical scale.
2. **Font bootstrap boundary.** Core UI/text code does not open or resolve arbitrary font paths. Verified bundled bytes enter through the narrowest existing Henka asset/application-resource boundary and create the opaque font resource. `ui.c` receives/consumes UI drawing resources only; it gains no filesystem, package-path, font-discovery, or Sandbox ownership. Use a narrow typed byte-span handoff only if the existing authority lacks one.
3. **Atomic scale/resource transition.** A candidate scale change prepares and validates shared text metrics, scale-dependent layout geometry, and every required live-context raster/atlas resource before either the new state is published or its preference is persisted. After persistence succeeds, publication is a no-fail swap. Preparation or persistence failure retains the previous complete usable scale/font/layout state and reports the failing stage; no frame may mix new layout metrics with stale atlas metrics. Atlas identity includes verified font identity plus effective app/output raster parameters and graphics-context residency. Native/detached display-scale moves use bounded replacement/reuse and cannot accumulate an unbounded DPI cache.
4. **Single measurement/layout authority.** Audit all production raw/context-free text-measurement and fixed-cell callers. Product measurement, fitting, truncation, row capacity, glyph placement, and drawing use the same active font/layout-run authority. Legacy fixed-width measurement is allowed only as explicitly classified bitmap-fallback or isolated regression support, never as a parallel product-layout authority.
5. **Separate display-scale proof.** Editor UI Scale and OS/display scale are independent variables. Add deterministic forward-draw and inverse-hit-test tests at non-1.0 display scales, cross-check composition against each supported app scale, and prove resize/display changes do not mutate the app preference. Hold OS display scale constant for paired font comparisons. In the separate real packaged non-100% OS display-scale case, hold Editor UI Scale and logical window dimensions constant and vary only actual OS display scale; if the environment cannot support it, classify that package case as unavailable. App-scale captures do not substitute for it. Exercise native/detached-context scale transitions where feasible.
6. **Exact font compatibility and provenance.** Prefer exact static TrueType TTF artifacts for the initial Atkinson Hyperlegible/Noto Sans comparison. A variable-font artifact is permitted only after compatibility is demonstrated against the exact pinned `stb_truetype`; OTF/CFF is not assumed compatible. Preserve exact version/revision, SHA-256, license, copyright, and Reserved Font Name provenance for both candidates and the selected bundled face.


## Scope exclusions

This slice does not redesign the editor, change hidden-row thresholds, remove or rename controls, shrink fonts or hit targets to fit layouts, author replacement icons, change game-runtime text, implement shaping/complex scripts/rich text, add FreeType, or introduce a public general-purpose texture API. Any newly exposed layout defect is repaired through the existing layout authority and tested, not hidden by per-panel spacing hacks.

## Implementation status

The integrated editor path provides bundled-font startup, shared font metrics, persisted scale selection, and context-specific raster resources. Settings interaction and restart behavior have packaged coverage, and layout regressions cover compact and wider editor arrangements. The packaged detach workflow verifies the per-context font/scale identity and a later successful engine render frame while its window stays hidden; Win32 `PrintWindow` output is not counted as detached OpenGL visual proof. Packaged proof at non-100% operating-system display scale remains open because the available captures report 100%; Editor UI Scale captures do not satisfy that separate check.
