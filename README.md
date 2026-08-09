# Henka Engine

Henka Engine is an early-stage open source engine written in C. The active order is runtime stability, production-quality 2.5D, and then integrated modeling and content-authoring tools, while retaining compatibility with external asset pipelines. The engine now also includes a small local persistence layer and guidance for keeping real games in separate repositories.

## Support Henka Engine

Henka Engine is an open source C game engine and development workspace moving from runtime stability to production-quality 2.5D and then integrated modeling and content authoring.

Sponsorship helps support development time, testing, documentation, examples, packaged builds, and future workspace tooling. The project will remain open source under its current license.

Sponsors help the project move forward, but sponsorship does not purchase feature priority, private support, ownership, or a different license. Feature decisions are still based on project direction, stability, maintainability, and usefulness to the wider engine.

Use the Sponsor button on this repository to support the project through GitHub Sponsors.

See SUPPORT.md for what sponsorship supports and what it does not promise.

## Current status

Henka Engine is still early, but the sandbox now renders a visible 3D scene with textured and untextured materials through Henka systems.

### What currently exists

- C17 build through CMake
- `henka` static library target
- renderer-independent `henka_runtime` static library target for headless consumers
- `henka_sandbox3d` example target
- renderer-free `henka_dedicated_server` host target with bounded config-file loading, validated optional Terrain base-world loading, fixed-tick simulation, Terrain snapshot recovery, loopback smoke connectivity, transactional edit persistence, graceful client shutdown, and a bounded `--run-for-ms` integration mode; a headless deployment package and restart-persistence check are available, while broader relevance-driven late-join orchestration and production-scale multiplayer soak remain unfinished; the bounded process integration soak repeats the full two-client/reconnect/restart scenario for a configured finite number of sessions
- `henka_tests` unit test target with CTest integration
- SDL3-backed platform layer hidden behind Henka headers
- OpenGL renderer backend isolated inside renderer implementation files
- Public math, time, camera, mesh, texture, shader, scene, and asset APIs
- Input action foundation for named engine-level controls
- Generation-checked 64-bit scene entity identities that invalidate destroyed handles before slot reuse, protecting selection, actions, physics links, and future authoring references
- Reusable camera helpers for reset, focus, screen-ray creation, stable vertical view bases, orthographic zoom, and Perspective 3D, Side 2.5D, Top-down 2.5D, and Isometric 2.5D presets
- Local action-command foundation for validated scene and object operations, including signed scale transforms for mirror workflows
- Asset metadata with cache-owned source and display strings, plus stronger material summaries
- Local save-data foundation with confined slot paths, complete-file validation, and transactional state replacement
- Package mode and engine diagnostics foundation, including Release resource-closure reporting for bounded packaged soaks
- Run-once engine lifecycle with copied configuration strings, reentrant-run rejection, exit-before-update rendering, checked frame rollback, and main-window presentation only after detached-window UI succeeds
- Shared overlay-handle transform gizmo foundation for selected object manipulation, with visual feel still being hardened through manual QA
- Viewport interaction test helpers for reducing manual QA around selection, gizmo hit testing, and transform changes
- Asset manager foundation with rooted/UNC/device/drive/URI path rejection, platform-aware canonical cache identities, preserved source spelling, checked texture uploads, path-specific stable fallback identities, deterministic hard-failure propagation, and transactional texture and OBJ fallback retries
- Early OBJ model loading with bounded source and output sizes, finite-number validation, negative indices, n-gon fan triangulation, degenerate-face rejection, and explicit failed-mesh retry support
- Bounded glTF/GLB geometry and shared PBR material import with manager-owned texture dependencies, transactional material-asset reload, strict bounded JSON/accessor validation, IOR/transmission controls, and bounded KHR volume attenuation
- glTF-derived material definitions expose stack-owned instances with validated scalar/vector, alpha-mode, and semantic-texture overrides, effective dependency inspection, revision-aware refresh, and transactional reimport; the shared dependency inspection contract also enumerates all twelve optional Terrain layer texture semantics while retaining the standard five material-instance texture slots; a separate Henka-only material file authority is still not present
- Sandbox Object Details and the Object Info utility expose the selected object's effective material description; Object Info also reports the count of borrowed semantic texture dependencies. The imported glTF Marker now has a bounded in-panel material-instance editor covering the shared scalar/vector, boolean, alpha-mode, and semantic-texture override contracts, plus dependency/revision inspection, reset, and transactional reimport. The Utility Assets view enumerates manager-known textures, materials, and meshes without a filesystem scan, uses deterministic bounded paging, and preserves stable metadata-index selection; a selected manager-owned texture can be assigned, cleared, or restored to its material definition on the editable instance only, with semantic validation and rollback. External C consumers can use the same validated instance APIs. Text-entry import, drag/drop, material-file authoring, and dedicated dependency-graph panels remain unfinished
- The sandbox editor now treats valid non-helper scene entities as authoring targets rather than requiring demo descriptors: Scene Objects enumerates the live scene, selection and Object Details share generation-checked entity identity, and the panel exposes validated Add Cube, Duplicate, and Delete operations. Transform hotkeys/gizmo edits, visibility, transform locks, reset, material inspection, and scene-object details remain session-only; text-entry rename and durable scene serialization are not yet editor workflows
- Bounded glTF/GLB scene import with selected scene roots, node hierarchy transforms, cameras, punctual lights, meshless camera/light scenes, and fail-closed accessor-reference validation
- Terrain now has a shared built-in four-layer Lit material contract: painted uint8 weights are normalized and consumed as base color, normal, metallic, and roughness blends with validated semantic texture slots and world-space meter tiling. The Sandbox binds four deterministic 16x16 Sandbox-owned procedural grass, dirt, rock, and wet base-color tiles to the reference fixture, while factor fallbacks remain valid when optional normal or metallic/roughness textures are absent. The graphical owner transports finite tangent vec4 bases with handedness into the normal Rendered shader, mesh generation uses available neighboring authoritative regions for border-normal continuity, and resident one-level LOD differences use non-degenerate bounded edge morphing with per-edge fallback skirts for missing neighbors; automated all-edge corner topology is covered, while four-way corner visual approval remains unfinished
- Transactional HDR environment lighting with derived IBL resources, fitted directional shadows with bounded receiver-aware contact tightening and far-cascade diagnostics, one deterministic bounded spot-light shadow map, bloom, tone mapping, fog, bounded local probe capture, and explicit Material Preview versus Rendered shading policies
- Rendered presentation now includes a bounded screen-space indirect diffuse lighting approximation that reconstructs receiver geometry from depth and gathers nearby visible HDR radiance before bloom and tone mapping. It can provide local diffuse color bleeding from visible surfaces, but it is intentionally described as SSGI. It is not full-scene global illumination: off-screen, hidden, and multi-bounce transport remain future work, and hardware ray tracing/path tracing are not claimed.
- Rendered presentation includes bounded RG camera- and object-motion history reprojection with an 8-sample subpixel jitter sequence, transactionally retained previous-frame depth for motion/disocclusion rejection plus 3x3 depth-neighborhood rejection, reactive handling for transparency/transmission/emissive pixels, neighborhood history clamping, bounded reconstruction sharpening, cumulative resolve/fallback-frame diagnostics, counted transactional history-allocation failures, and previous-resource-retention state, plus a four-direction, two-sided, multi-step view-space horizon-search ambient-occlusion approximation with bounded radius/thickness/falloff/bias/intensity controls; production GTAO validation and production TAA visual validation across camera cuts, resize, disocclusion, and moving-object cases remain unfinished
- The opt-in Windows sandbox command `--temporal-stress` drives the public scene path through a viewport resize and restore, camera translation, projection change, entity disocclusion and reappearance, and recovery frames; it reports and checks history-ready recovery, the cleared `history valid` state after a successful color/depth commit, resolve/fallback/invalidation, and transactional history-allocation-retention diagnostics, returns failure if that bounded check does not recover, and does not claim production TAA visual closure
- The opt-in Windows sandbox command `--material-stress` exercises every supported shared glTF material-instance override family (scalar, vector, base-color, boolean, alpha-mode, and semantic-texture), verifies invalid-edit retention, applies the effective value transactionally to the scene, refreshes the definition, resets overrides, and checks clean shutdown; it does not create a second material-file authority
- Checked KTX2/Basis texture loading with active-OpenGL capability-selected BC/ETC2/ASTC mip uploads; uncompressed and Basis sources have a truthful RGBA8 fallback when compressed upload is unavailable, while unsupported native-compressed sources fail closed; block data is never presented as RGBA8. Texture inspection reports the selected resident GPU format as well as compressed-versus-fallback state
- Texture objects report exact resident GPU bytes and mip counts; manager-owned KTX2 textures support synchronous transactional top-mip replacement, a bounded coalescing residency request queue that retains the strongest target for repeated references, deterministic largest-texture trim-to-budget, explicit configured-budget enforcement, active-frame pinning of visible manager-owned textures, revision-checked stale-request cancellation, and fail-closed residency diagnostics. Trim demotes eligible KTX2 textures and reports trimmed/demoted bytes separately from true eviction; whole-resource eviction is not claimed. The same budget, resident-byte, cumulative uploaded/failed/source-failed/trimmed/demoted bytes, known-versus-unknown request and source-failure counts, queue, completion/failure/cancellation, trim, pinned-byte, and progression-mode counters are surfaced through the engine diagnostics snapshot; failed bytes count resident payloads known to have been rejected after creation or replacement, source-failed bytes count readable encoded files rejected before residency, and unreadable or missing sources remain explicitly unknown-size. Visible scene materials enqueue KTX2 mip targets from a bounded projected-radius heuristic with deterministic distance fallback, semantic-slot priority, and a small threshold hysteresis band to avoid minor-distance thrashing; the engine services at most one queued request and one configured-budget trim per frame; background I/O streaming and broader automatic residency policy remain unfinished
- Scene rendering exposes bounded distance-based LOD selection, frustum culling, draw budgets, and LOD fallback diagnostics; LOD selection does not claim texture residency or streaming
- The public `--residency-stress` scenario also resets its live KTX2 fixture to one mip and records the rendered far/near/return visibility trace, then generates a native BC1 KTX2 mip chain. When the active device exposes BC1, it proves compressed residency trim and promotion; otherwise the native-compressed source fails closed and the scenario reports that capability result. This is bounded visibility promotion/demotion and compressed-format pressure coverage, not asynchronous streaming or a complete budget-convergence policy
- Visible KTX2 texture residency requests now carry deterministic projected-radius, distance-fallback, and semantic-slot priorities; the bounded queue coalesces strongest mip demand and services higher-priority references first, can promote or demote to the current target, cancels requests made stale by a texture replacement, and active-frame pins keep referenced manager-owned textures out of trim. The public asset API can explicitly end a residency scope to release pins immediately, and active scene replacement releases pins and cancels pending work before installing the new scene. Background I/O and whole-resource automatic eviction policy remain unfinished
- The opt-in Windows sandbox command `--residency-stress` exercises the public asset path with 65 path-distinct manager-owned textures, shared-cache identity, configured-budget rejection, active-frame pinning, full-queue saturation and recovery, failed request processing, cancellation, generated pinned-KTX2 promotion/demotion, a readable corrupt-source byte-count case, live-scene texture binding, a far/near/return camera phase, and shutdown cleanup. The generated KTX2 fixture is an uncompressed bounded mip-chain; compressed-format budget pressure and background I/O remain separate unfinished validation tracks
- Scene rendering batches contiguous opaque or masked entities with identical mesh and material state through a fixed 256-instance OpenGL upload buffer when the optional instancing contract is available; probe-bearing, LOD-bearing, blended, incompatible, and unsupported-shader submissions fall back to ordinary draws, with instance counts exposed in diagnostics
- Scene rendering has a bounded previous-frame OpenGL occlusion-query reuse path for unchanged non-LOD entities; camera, scene-revision, transform, unavailable-result, and unsupported-query cases conservatively draw normally, so this is an occlusion foundation, not a complete hierarchical visibility system
- Bounded realism validation materials and scene samples covering metal, clearcoat, plastic, stone, sheen, wood, wet/dry variation, detail normals, and masked foliage
- Descriptor-aware RGBA8 textures with explicit sRGB/linear, sampler, wrap, mip, flip, usage, alpha, source-class, and content-revision metadata
- Bounded single-read texture decoding with truthful rejection of HDR and 16-bit sources, plus path-specific white/error fallback aliases
- Shader-based rendering of built-in primitives
- Sandbox window titled `Henka Engine Sandbox 3D`
- Ground plane, UV material ball, cubes, debug grid, a loaded glTF PBR marker, textured materials, and visible fallback behavior for missing texture and model assets
- The editor grid is an explicitly unlit, restrained graphite/slate line surface; its studio environment source is periodic and validated so Rendered presentation does not introduce a center seam. The procedural ground plane uses front-face winding consistent with its +Y normals.
- Keyboard movement, mouse look when capture is active, viewport-local Wireframe, Solid, Material Preview, and Rendered shading controls, and offline runtime help
- Bounded local settings persistence with transactional loads and replace-on-success writes
- In-window editor UI with mixed-case built-in text, restrained graphite/slate surfaces, lower-contrast one-pixel framing, flat secondary controls, underline-only tabs, compact switch toggles, quieter structured rows, clear selected-state accents, and release-confirm control activation
- Transactional UI frame construction with nested-frame rejection, frame-only widget admission, all-or-nothing composite draw commands, bounded text fitting, and state changes committed only after rendering succeeds
- Checked workspace and viewport calculations with deterministic failure outputs, framebuffer clipping, custom viewport preservation across resize, and overflow-safe OpenGL coordinate conversion
- Deterministic per-entity texture binding, checked frame-abort context restoration, and safer main and detached-window OpenGL resource cleanup
- Locked scene objects remain inspectable without a transform highlight or gizmo; transform hotkeys require a visible unlocked selection and stale transform-session ownership is cleared on selection, visibility, lock, and tool changes
- Scene Objects, Object Details, and Utility panels with synchronized arbitrary scene-object selection, entity-aware inspection, and bounded lifecycle actions for the sandbox editor
- Sandbox workspace panels with hidden-section dock compaction, compact single-tab headers, icon-like drag grips, stacked side docks, header drag, cross-zone redocking, native detached-window panels with routed mouse input, safer tool-window renderer context recovery, a validated single-root split-topology model with stable section leaves, nested dock projection, 10 px logical divider hit targets, transactional close-threshold foundation, double-click divider equalization, bounded eight-entry workspace layout undo/redo history with Ctrl+Z/Ctrl+Y/Ctrl+Shift+Z keyboard shortcuts, focus-loss/Escape interaction cancellation, DPI-aware horizontal and vertical resize cursors, dock splitter, bounded tab selection/reordering, Left/Right keyboard tab cycling on hovered merged headers, Tab/Shift+Tab traversal across visible workspace panels with a visible focus cue, Up/Down/Enter/Escape section-chooser navigation, guarded Ctrl+M maximize/restore for the focused or hovered section, idle hover hints for resize/drag affordances and merged-tab activation/reorder/center-drop behavior, and reset-layout recovery controls
- Multi-window platform foundation with focus-loss release synthesis, per-frame detached-window event state, collision-safe engine window identifiers, validated native identifiers, bounded native position read/write, truthful capability diagnostics, and transactional mouse-capture changes
- Separate `Native Panel Test` window for close, focus, resize, and event-routing QA, opened from the bounded Controls `QA` page
- Transactional recovery for incompatible saved live workspace panel/topology state; bounded legacy floating-panel dimensions migrate to current minimums, current safe defaults replace structurally invalid live layouts, and repaired state is rewritten on clean shutdown without discarding valid saved custom layouts or named slots
- Rigid-body physics v1 with atomic fixed substeps, allocation-safe rollback, static/dynamic/kinematic bodies, sphere/AABB/plane/static-heightfield colliders, triggers, events, raycasts, copied Terrain samples, and sandbox debug controls
- Transactional packaged-sandbox refreshes that preserve user data by default and retain the prior package until activation succeeds
- Generic documentation and starter template for external game repositories; the external C17 template now runs bounded public-API Terrain CPU and graphical consumer smokes covering the shared material contract, deterministic raise/paint, collision raycast, CPU render-mesh rebuild, transactional save/restart reload, normal engine/scene Terrain ownership, and a Rendered draw with HDR/shadow diagnostics
- C17 external server template and Windows validation path that links only `henka_runtime` plus its private network implementation
- Headless-only CMake configuration with `HENKA_BUILD_CLIENT=OFF`, `HENKA_BUILD_DEDICATED_SERVER=OFF`, and `HENKA_ENABLE_KTX2_TRANSCODER=OFF`; the resulting runtime target does not configure SDL, OpenGL, or KTX
- Version-1 bounded Henka network packet codec and localhost ENet transport with explicit little-endian headers, three logical channels, reliable ordered delivery, a 64 KiB packet ceiling, 32 KiB snapshot fragments, authoritative Terrain deltas/snapshots, renderer-free client session ownership, explicit client reconnect, and server-directed disconnect; a bounded control session-info message validates world/base identity and advertises up to 16 current resident regions in deterministic row-major coordinate order so empty clients can bootstrap late-join snapshots consistently after residency-slot reuse, while application handshake/authentication, relevance-based region selection, reconnect policy beyond the advertised bound, and production-scale multiplayer soak remain unfinished; a finite repeated process soak covers the existing bounded scenario
- Terrain v1 core contract with an 8192 m x 8192 m default descriptor, 512 m regions, 64 m chunks, 65 x 65 full-resolution chunk samples, signed millimeter heights, deterministic four-layer weight normalization, integer region/chunk identities, and separately bounded CPU/physics/render residency state; bounded physics patch synchronization now follows physics-resident regions, while full residency-wide collision coverage beyond physics capacity remains unfinished and the graphical owner derives a bounded nearest render working set automatically
- Bounded Terrain v1 world manifests and region records use explicit little-endian descriptor, world/base identity, region, generation, revision, sample, and checksum fields; manifests are atomically created/validated, append-only BEGIN/REGION/COMMIT journaling atomically replaces validated region snapshots, ignores incomplete transactions during recovery, and supports atomic committed-history compaction
- Bounded Windows Terrain streaming foundation with a renderer-free worker, coalesced region requests, fixed request/completion queues, observer records, cancellation of stale queued and active observer demands while preserving explicit caller-driven requests, candidate validation, main-thread region swaps, observer-driven CPU/physics/render residency flags, deterministic render/physics/CPU-radius request priority with stable sequence tie breaking, deterministic clean-region eviction, optional unload hysteresis, current and high-water queue/observer diagnostics, fail-closed saturation counters, and an optional worker-side bounded generator for valid regions without persisted snapshots; persisted regions remain authoritative and generator failures remain visible; the Sandbox now seeds a persistent 2x2 procedural fixture in a four-region CPU budget, uses the same deterministic generator for broader camera movement, feeds the active camera into that observer, and routes physics residency, edit footprints, and neighbor coverage through the bounded collision-runtime queue; the opt-in `--terrain-stream-stress` path proves rendered and collision-patch `(0,0) -> (1,0) -> (1,1) -> (0,0)` return with no failed requests, while background physics/render regeneration remains unfinished
- Shared deterministic Terrain edit commands for raise/lower, flatten, smooth, and paint use algorithm version 1, integer sample centers/radii, fixed-point falloffs, candidate-region preflight, atomic multi-region swaps, revisions, and dirty state; the Sandbox Terrain utility produces the same commands with bounded brush controls, resident-patch viewport ray-pick, hovered brush preview, picked-sample brush placement, and validated persistence of brush radius, strength, layer, falloff, and operation settings, while broader saved Terrain-world authoring remains unfinished
- Explicit bounded Terrain edit request, acceptance, and rejection codecs carry world/base identity, client nonce, versioned command fields, affected-region expected revisions, server command identity, and fail-closed rejection reasons without transmitting native C layouts
- Bounded Terrain authority validation checks peer rate limits, permissions, world/base identity, exact affected-region sets, and expected revisions, applies deterministic edits, commits all affected region snapshots before returning acceptance, and restores live samples/revisions on persistence failure; the server retains a fixed 64-entry delta history for complete revision-gap recovery and falls back to a transactional region snapshot when history is exhausted
- Renderer-independent Terrain server session routing decodes bounded edit requests from the public ENet server, invokes authority validation, sends encoded acceptance/rejection responses, echoes control pings, sends bounded world/base session info with current resident-region revisions in deterministic row-major coordinate order on connect, disconnects malformed edit peers, and exposes dispatch diagnostics; the client/session tests cover forced disconnect, explicit reconnect, authoritative server-wrapper restart on the same endpoint, and exact resident-sample convergence, while the dedicated two-client process harness proves a late observer joins the committed region, a reconnecting client receives a fresh connection and continues editing, and restart recovery preserves the checksum. The bounded process integration soak repeats that complete scenario for a finite session count. Application handshake/authentication, relevance-based late-join selection, reconnect policy beyond the advertised bound, and production-scale multiplayer soak remain unfinished
- Authoritative Terrain edit deltas use explicit bounded world/base identity, client nonce, server command ID, deterministic command fields, and resulting region revisions; accepted deltas are broadcast reliably to connected peers and retained in a 64-entry bounded history, while the client adapter applies exact-revision deltas, requests the missing range on a gap, and uses the snapshot path when history is unavailable; broader relevance-driven reconnect recovery remains unfinished
- Dedicated-server Terrain snapshot serving validates bounded requests, reads the live authoritative region (materializing a persisted region when needed), and sends transfer-identified fragments below the 32 KiB snapshot ceiling; the client adapter assembles and atomically applies those fragments, and the connect-time session-info message requests snapshots for up to 16 advertised resident regions. The Windows process harness proves this bounded late-join path, explicit reconnect, and exact restart checksum recovery; broader relevance-driven orchestration remains unfinished
- Renderer-independent Terrain client session ownership routes public network events through the bounded replica, validates connect-time session-info world/base identity, requests snapshots only for advertised regions whose local revision/generation is missing or stale, sends edit/snapshot/recovery requests, retries decoded-but-rejected snapshot transfers at most four times per connection, records acceptance/rejection diagnostics, replays a bounded predicted world over authoritative state, and requests retained deltas or a region snapshot when a delta gap or nonresident region requires recovery; the client tests prove exact resident-sample preservation across forced disconnect, reconnect, and authoritative server-wrapper restart, plus byte-identical convergence of two public client replicas after edits from both peers. The process harness and its finite repeated soak additionally cover bounded late join and explicit reconnect with a deterministic resident-sample checksum. Application handshake/authentication, broader relevance selection, reconnect policy beyond the advertised set, and production-scale multiplayer soak remain unfinished
- Dedicated-server edit handling lazily materializes requested persisted regions within the configured residency bound before authority validation, avoiding a world-sized preload; streaming now has observer-driven bounded CPU-region eviction with optional unload hysteresis, while background regeneration beyond the bounded physics/render queues remains unfinished
- Renderer-independent Terrain replica ownership applies authoritative deltas only across exact revision steps, treats complete duplicates idempotently, assembles bounded snapshot fragments with transfer metadata and an exact full-size/non-final fragment shape, rejects duplicate or corrupt transfers without publishing partial samples, and atomically swaps the decoded region; session-info-driven bounded late-join bootstrap now feeds that same snapshot path, while broader reconnect orchestration and render/physics residency remain unfinished
- Deterministic allocation-free Terrain LOD selection scans render-resident chunks in stable region/chunk order, applies bounded distance bands through LOD 3, reports considered/culled/selected counts, and fails closed on output limits; the graphical terrain owner adds fixed-capacity scene entities, renderer bounds culling, hysteretic four-band selection, deterministic nearest-working-set scheduling, adjacent-chunk LOD clamping, non-degenerate edge morphing for one-level resident LOD differences, bounded fallback skirts only for missing/invalid neighbors, revision-aware 3x3 region dirty propagation, dynamic height-derived bounds, high-water queue/resident/visible diagnostics, and transactional mesh replacement, while automated all-edge corner topology is covered and four-way corner visual validation remains unfinished
- Renderer-independent Terrain chunk mesh generation converts a render-resident chunk into caller-owned LOD 0-3 vertices and indices with finite height-derived normals, orthogonal tangent vec4 bases with deterministic handedness, stable world-space UV transport, material weights, source revision/generation identity, and an explicit four-edge transition mask; the graphical mesh API uploads that bounded data through the existing renderer and `<henka/terrain_render.h>` owns borrowed-world scene integration, automatically derives a bounded nearest set from render-resident regions, refreshes stale uploaded revisions transactionally, exposes validated edit-footprint requeue with one-chunk dependency coverage for resident slots, and deterministically destroys slots and meshes
- The Sandbox reference scene exercises a render-resident Terrain region through the graphical owner, which derives its bounded nearest chunk working set and camera-driven LOD update; a new world is seeded with a deterministic rolling valley, steep ridge/cliff, and continuous grass/dirt/rock/wet four-layer weight field, while persisted worlds retain their committed samples; its active camera also drives the bounded Terrain streaming observer and the Utility Terrain row exposes the current stream queue; the Terrain utility exposes bounded sculpt/paint controls through the shared command API, and viewport clicks ray-pick resident physics patches to place the next command, while human visual approval remains unfinished
- Sandbox smoke additionally sends shared integer raise and paint commands through the Terrain world, verifies the painted layer weight reaches a newer rendered mesh revision, explicitly requeues the accepted edit footprint and one-chunk render dependency border, refreshes the bounded collision patch for height edits, asserts the Terrain entity submits through the Rendered color and shadow passes, reports exact world-owned CPU and resident graphical-owner vertex/index/material GPU bytes, and forces a failed candidate mesh replacement to prove the previous mesh/revision stays live before recovery; the Terrain utility can save and transactionally reload the resident region through the shared journal, compact committed history, and preserve the prior presentation on reload failure, while networked edit authority remains a separate integration
- Renderer-independent Terrain collision patch extraction copies a physics-resident chunk into a bounded 65 x 65 signed-millimeter height patch with revision and generation identity; a bounded transactional Terrain physics patch owner retains replacements, answers deterministic height/normal queries, and exposes normalized allocation-free raycasts over resident patches, while the rigid-body API owns copied static heightfield colliders with sphere/box contacts and bounded raycasts; a renderer-free coalescing rebuild queue synchronizes missing/stale physics-resident chunks, exposes current/high-water pending counts, replaces patches transactionally, and now has regression coverage for queue saturation/recovery plus failed-rebuild preservation and retry; coverage beyond physics capacity and broader runtime collision stress remain unfinished
- The headless Terrain workflow test drives shared raise and paint commands through collision rebuild, commits the resulting region, verifies restart reload, and proves an abandoned journal transaction leaves the last committed revision intact
- Deterministic Windows CI package contract checks that avoid hosted graphics-session assumptions, while local validation still performs packaged runtime smoke, desktop interaction, and application-only screenshot checks
- Repository integrity checks for tracked artifacts, credential signatures, script parsing, dependency pins, and workflow action pins

### What does not exist yet

- Full production editor and project-authoring workflow
- Full asset browser, import/reimport, dependency-graph, and project authoring workflow (the sandbox has only a bounded manager-known asset view and editable-instance texture-slot assignment)
- Audio
- Relevance-driven reconnect/late-join Terrain recovery orchestration and production-scale dedicated-server multiplayer soak tooling; a bounded repeated process soak is available for the existing advertised-resident-region contract
- Scripting
- Full 2D renderer
- Full 2.5D sprite, layered-depth, parallax, animation, and constrained-movement workflow
- Integrated modeling, UV, rigging, animation-authoring, and content-creation workspace
- Additional renderer backends
- Complete cross-backend KTX2/Basis production coverage and GPU-native stress validation, background texture streaming and automatic policy eviction, broader instancing/batching/occlusion scale work, and full refraction/layered-volume/glass rendering remain unfinished. Rendered now has a bounded depth-derived screen-space reflection attempt with thickness, maximum-distance, roughness, confidence, edge-fade, and miss handling when derived IBL resources are ready; scenes without IBL, unsupported materials, missed rays, and out-of-view rays use the existing environment/analytical fallback. glTF transmission and volume attenuation use bounded environment responses. Temporal fallback state, invalidation count/reason, reactive-mask support, and bounded sharpening are exposed, but production TAA still requires the documented visual validation cases. Current paths expose truthful fallbacks or bounded foundations
- Temporal history replacement is fail-closed: a failed new history allocation retains the previous GPU object but disables accumulation for the requested viewport and reports the fallback state. Production temporal visual validation remains unfinished.
- AO uses a bounded depth-agreement edge confidence around its horizon search to reduce discontinuity halos; temporal AO history, multi-frame denoise, and production GTAO validation remain unfinished.

## Repository layout

```text
assets/              Runtime shader, texture, and model assets
engine/              Core library and public headers
examples/sandbox3d/  Visible 3D sandbox application
tests/               Headless unit tests
docs/                Architecture, build, roadmap, and help documents
scripts/             Windows helper scripts
templates/           Generic starter content for separate game repositories
third_party/         Bundled third-party source used by the engine
```

## Try the sandbox

On Windows, the quickest way to try the current sandbox is:

```powershell
.\scripts\build_windows.ps1 -Configuration Debug
.\scripts\package_sandbox3d_windows.ps1 -Configuration Debug
```

The sandbox is an engine sample and QA target. It is not a game, and real games built with Henka should live in separate repositories.

## Build

Windows build instructions are documented in [docs/building.md](docs/building.md).

Quick start from the repository root:

```powershell
.\scripts\build_windows.ps1 -Configuration Debug
```

To create a run-ready Windows folder that you can open in Explorer and launch by double-clicking:

```powershell
.\scripts\package_sandbox3d_windows.ps1 -Configuration Debug
```

## Run the sandbox

```powershell
.\scripts\run_sandbox3d.ps1 -Configuration Debug
```

## Run the packaged sandbox

After packaging, open `out/HenkaSandbox3D/` in Explorer and double-click `HenkaSandbox3D.exe`.

You can also launch it from PowerShell:

```powershell
.\scripts\run_packaged_sandbox3d_windows.ps1
```

The sandbox starts a visible 3D scene with:

- a textured cube
- a textured ground plane
- a rounded material ball
- a loaded OBJ marker
- a debug grid
- a fallback-texture example for missing texture loads
- a fallback-model example for missing OBJ loads

Sandbox settings are saved locally in a `user/` folder beside the executable. In a packaged run, the settings file is `out/HenkaSandbox3D/user/sandbox3d.settings`.
The packaged folder also includes `PACKAGE_INFO.txt` so you can tell when the package was last refreshed.

### Persistence safety

- Engine-managed assets accept confined relative paths beneath the configured asset directory.
- Save-slot names use a bounded portable identifier and cannot contain traversal or path separators.
- Settings reject structural control characters and enforce bounded keys, values, entry counts, and numeric conversions.
- Settings and save-data files are fully validated before replacing existing in-memory state.
- Writes complete in a same-directory temporary file and replace the destination only after the file is flushed and closed successfully.

### Resource bounds

- Shader source files are limited to 1 MiB each and must be read completely.
- OBJ sources are limited to 16 MiB, individual lines to 4,096 bytes, and parsed arrays to fixed safe maxima.
- Texture sources are limited to 64 MiB encoded and 16,384 pixels per axis; decoded RGBA8 data is limited to 256 MiB.
- Mesh uploads validate counts, byte sizes, primitive types, and every index before reaching OpenGL.
- Shared checked-arithmetic helpers protect capacity growth, size multiplication, and narrowing conversions.
- Asset caches, scene entities, action default transforms, physics contacts and events, and related runtime arrays use bounded checked growth.
- Procedural grids and circle rings reject non-finite or excessive dimensions before allocation.
- Asset paths are bounded to 4,096 bytes and scene-owned names, tags, material names, and interaction prompts are bounded to 1,024 bytes.

### Runtime metadata ownership

- Scene names, tags, material names, and interaction prompts are copied into bounded scene-owned storage.
- Asset metadata source and display strings use asset-manager-owned storage and do not depend on caller buffers remaining alive.
- Local bounds reject non-finite centers, non-finite extents, and negative extents.
- Materials reject invalid types, non-finite or out-of-range physically based values, and textured configurations without a texture. Texture slots also validate semantic usage and linear versus sRGB color-space requirements.
- Interaction ranges reject negative and non-finite values, and eligibility checks reject non-finite observer positions.
- Camera constructors sanitize invalid projection inputs, camera projection helpers fail closed on non-finite state, and scene camera assignment accepts only valid camera state.
- Primitive actions validate bounded names and primitive types before dry-run success, then roll back partially created entities if any setup step fails.

### Physics activation safety

- Sandbox samples start with static bodies so enabling one selected body cannot start unrelated samples.
- `Make Dynamic + Drop` synchronizes the selected object's current transform, activates only that supported body, clears its velocity, and leaves other samples still.
- `Enable` remains the explicit full-scene demonstration path and assigns the intended dynamic sample set before playback.
- Automated coverage proves that an unrelated marker keeps its transform while the selected cube falls.
- Physics rejects non-finite and collapsed physics scales. Fixed substeps also reject finite calculations that exceed representable engine state with `HENKA_ERROR_NUMERIC_RANGE`; numeric and allocation failures retain the prior bodies, contacts, events, pair history, accumulator, and linked scene transforms so callers can correct inputs and retry. Destroying a body removes only its contacts and pair history, appends one EXIT event for each active pair, and preserves unrelated queued events and survivor contact continuity. Physics allocations remain visible in engine memory diagnostics.

### Validated platform and package identity

- The fully validated build, test, packaging, and external-project path currently targets 64-bit Windows with MSVC.
- Other operating systems are not presented as supported until their complete path is exercised.
- Every Windows build records the full commit, source state, configuration, architecture, CMake version, executable path, and executable SHA-256.
- Packaging requires that build record and rejects stale, mismatched, or cross-configuration executables.
- `PACKAGE_INFO.txt` carries the verified identity and hashes into the runnable folder.

See [Platform Support](docs/platform-support.md) and [Package Provenance](docs/package-provenance.md).

### Package refresh safety

- Build provenance supports branch and detached checkouts and is written transactionally.
- Packaging assembles a complete staging directory before replacing the active package.
- Existing user data is copied into the staged package unless `-ResetUserData` is requested.
- A failed activation restores the prior package instead of deleting the only preserved user-data copy.
- Package inputs containing reparse points are rejected before copying.
- A retained backup from a previously activated, complete package is recovered on the next guarded run; incomplete staging state still stops for inspection.

### Sandbox controls

- `W A S D`: move across the scene
- `Q / E`: move down / up
- `Shift`: move faster
- `Mouse`: look around while mouse capture is active
- `Right Mouse / Tab`: toggle mouse capture
- `Left Mouse`: uses the active viewport tool when mouse capture is released
- `Alt + Left Mouse`: optional orbit shortcut around the selected object or current view target
- `Middle Mouse`: optional pan shortcut
- `Mouse Wheel`: zoom the viewport when the cursor is over the scene view
- `F1`: enter Wireframe or return to the last non-wireframe viewport shading mode
- `F2`: print the scene legend again
- `F3`: show or hide the debug grid
- `F4`: show or hide the sandbox panels
- `F5`: cycle View, Inspect, and Full Tools layouts
- `F`: frame the selected object
- `H`: print controls and the scene legend again
- `Home`: reset the camera view
- `M` or `G`: move the selected visible and unlocked object
- `R`: rotate the selected visible and unlocked object
- `S`: scale the selected visible and unlocked object
- `X`, `Y`, or `Z`: constrain an active transform
- `Enter` or `Left Mouse`: confirm an active transform
- `Escape` or `Right Mouse`: cancel an active transform
- `Left Ctrl` / `Left Shift`: stepped or fine transform adjustment
- `Escape`: when no transform is active, close the UI first, then release the mouse, then exit

The Scene View header provides Wireframe, Solid, Material Preview, and Rendered shading. Solid uses neutral filled geometry. Material Preview evaluates the metallic-roughness Cook-Torrance material model under deterministic editor lighting and intentionally omits scene-dependent post-processing. Rendered evaluates the same material model with the scene directional light plus bounded point/spot lights and depth shadow maps into a Scene View-sized linear HDR target, then applies scene post-processing including exposure, an ACES-fitted tone map, a Rendered-only color grade, bloom, depth-neighborhood AO, and bounded temporal reconstruction. The directional coverage uses a fitted, stabilized near cascade and a bounded far cascade; the first enabled spot light receives a bounded 512² map and the first enabled point light receives a bounded 256² cubemap with linearized depth. Rendered writes bounded camera- and object-motion vectors, uses an eight-sample subpixel jitter sequence, applies motion-compensated history reprojection with depth-neighborhood rejection and reactive masking for unstable pixels, and applies bounded reconstruction sharpening. This remains a temporal foundation pending production visual validation across camera cuts, resize, disocclusion, and moving-object cases. A scene may also provide a borrowed linear Radiance HDR equirectangular texture for the background and material environment response; the analytical gradient remains the fallback. HDR target dimensions, generation, completeness, shadow resolution, resize failures, temporal-history state, fallback/invalidation diagnostics, jitter state, motion-vector attachment state, and local-probe capture counts/failures are available through engine diagnostics, including the bounded point-shadow target. Exposure is independently persisted and can be adjusted from Settings. The current forward transparency path supports straight-alpha blending in entity order; it is not order-independent transparency. The legacy `F1` wireframe control remains compatible and restores the last non-wireframe mode when switched off. Shading mode is saved independently from workspace geometry, so Reset Layout does not discard it.

The sandbox panels open automatically on startup and reset-style launches so Controls and `Physics QA` are discoverable without knowing `F4` first. Starts have no selected scene object until the user selects one. Press `F4` to hide or show panels, and press `F5` to cycle between `View`, `Inspect`, and `Full Tools`. UI buttons, toggles, tabs, and selectable rows activate on mouse release inside the active control so press, drag-away, and release behavior is safer. Active control IDs are copied into bounded UI-owned storage instead of retaining caller stack pointers, and non-finite UI geometry is rejected before draw-list insertion. `DRAG` marks a live panel header. Release on a valid left or right dock outline to redock there, release away from the outlines to keep the panel as an in-app floating panel, or use `Pop` on a floating panel to move it into a separate native tool window. Detached workspace panels now render their matching panel content in native tool windows and route per-window mouse input for release-confirm controls. When two panels share a side, the dock stacks them vertically instead of letting one cover the other. Closing a detached tool window returns its panel to the last valid dock, and `Reset Layout` restores the default workspace. Renderer context recovery is hardened around tool-window drawing. Select a scene object, then use `M` or `G`, `R`, and `S` to start move, rotate, and scale transforms. Active transforms support `X`, `Y`, or `Z` constraints, confirm, cancel, stepped adjustment, and fine adjustment through the action-based local control profile. Negative scale is preserved as an intentional mirror transform, while zero and near-zero scale are rejected to avoid collapsed objects. Locked objects, including the default Ground, stay selectable for inspection without a yellow transform highlight or gizmo and require an explicit unlock action before movement. Selection, visibility, lock, and tool changes clear active transform-session ownership. UI draw construction is valid only between a matched begin and end call; failed composite controls roll back their draw commands and do not consume release events or mutate toggle state. Open `Physics QA` to inspect the opt-in rigid-body demo. Manual desktop QA is still required before physics feel, native window behavior, panel drag comfort, and transform workflow feel can be called fully complete.

Offline help is also available in [docs/help/sandbox3d.md](docs/help/sandbox3d.md).
Model loading notes are documented in [docs/model-loading.md](docs/model-loading.md).
A persistence overview is available in [docs/persistence.md](docs/persistence.md).
A local action-command overview is available in [docs/action-api.md](docs/action-api.md).
A rigid-body physics overview is available in [docs/physics.md](docs/physics.md).
An editor controls overview is available in [docs/editor-controls.md](docs/editor-controls.md).
A runtime foundation overview is available in [docs/runtime-foundations.md](docs/runtime-foundations.md).
A UI overview is available in [docs/ui.md](docs/ui.md).
A guide for separate game repositories is available in [docs/external-game-projects.md](docs/external-game-projects.md).
Package identity and recovery behavior are documented in [docs/package-provenance.md](docs/package-provenance.md).
Repository checks are documented in [docs/repository-integrity.md](docs/repository-integrity.md).
A manual verification checklist is available in [docs/qa/sandbox3d-manual-checklist.md](docs/qa/sandbox3d-manual-checklist.md).
[Support Henka Engine](SUPPORT.md)
Packaged output is generated under `out/` and should not be committed.

For deterministic packaged startup checks, use `.\scripts\check_packaged_sandbox3d_windows.ps1 -NonInteractive`.
For the full local desktop interaction check, use `.\scripts\check_packaged_sandbox3d_windows.ps1`.
To validate the generic external game template against the current Henka checkout, use `.\scripts\test_external_game_template_windows.ps1`.

## Run validation

```powershell
.\scripts\check_public_repo_hygiene.ps1
.\scripts\check_repository_integrity.ps1
.\scripts\test_windows.ps1
```

## Current limitations

- The sandbox uses built-in plane, cube, UV-sphere, and debug-grid primitives plus bounded OBJ and glTF loading paths.
- Missing textures fall back safely to an error texture, and missing OBJ assets fall back to a visible mesh. Failed OBJ mesh fallbacks can be retried explicitly after the source asset is fixed.
- OBJ support is intentionally limited to bounded local files containing comments, blank lines, finite positions, optional finite UVs and normals, positive and negative indices, and triangle/quad/n-gon faces through basic fan triangulation.
- OBJ material libraries, concave polygon correction beyond basic fan triangulation, model hierarchies, and animation are not supported yet.
- The local settings format is bounded, transactionally loaded, and written through a replace-on-success temporary file.
- The local save-data foundation validates slot names, finite camera values, complete camera records, and boolean flags before replacing existing state.
- Remote saves, registry storage, encryption, network-backed persistence, symlink-aware confinement, migration tooling, and per-game save policy remain outside this local foundation.
- The in-window UI overlay is intentionally small. It now enforces matched frame construction, transactional composite drawing, release-confirm controls, object inspection, utility views, and short status feedback, but it is still not a full editor or a general UI toolkit yet.
- In-app floating panel rectangles, dock widths, dock assignment, and last valid dock are persisted through the local settings file; detached OS windows save and restore bounded virtual-screen positions through the public tool-window state/position path, while malformed or out-of-range positions are ignored. `Reset Layout` remains the recovery path and preserves valid named layout snapshots. The bounded topology graph, split ratios, tab membership/order, closed-section mask, maximize state, active tab, and selected bounded workspace preset now use a versioned validated settings snapshot; the current v2 loader migrates the prior v1 snapshot shape and rejects malformed, future, or incompatible versions without replacing defaults. The workspace header chrome opens the required section context menu without stealing tool-content right-clicks, and executes bounded close, merge/tab-group, equalize, maximize/restore, native-detach, last-closed restoration, and available-singleton split actions. `Close this section` removes the complete section and its tab group; the separate tab-close path removes only the active tab and removes the section when its final tab closes. The existing last-closed restore transaction reopens the prior tab group on failure recovery. Maximizing a docked section now expands that section across the workspace while keeping the scene underneath as a stable presentation surface. The menu also exposes a visible keyboard selection state with Up/Down, Enter, and Escape navigation. Dock stack section order and membership now project from the validated topology, including nested dock section rectangles, one shared thin topology divider per internal split, suppression of closed and merged-away sections, and a DPI-scaled logical divider hit target (10 px at 100%); the visible wire remains one framebuffer pixel. Merged sections expose bounded header tab controls, center-drop drag-to-tab joining, active-content projection, and same-section tab reordering with click-preserving release and rollback on cancellation. The Controls panel exposes deterministic Default, Modeling, Materials, Scene Assembly, Debugging, and Minimal Viewport workspace presets; switching presets participates in the bounded layout undo/redo history, closes detached tool windows through the normal redock path, and subsequent topology edits are labeled Custom. It also exposes three bounded named layout slots—Custom, Studio, and Assembly—that persist validated topology, tab order, closed/maximized state, dock assignments, dock widths, and UI scale and restore after redocking detached panels. Slot names and contents are bounded and local; an unbounded layout marketplace and detachable Scene View remain open.
- Production tool panels can detach into separate OS-level windows with full matching panel controls, routed mouse input, an explicit Dock L/Dock R/Home return bar, bounded saved virtual-screen placement, safe close-to-redock recovery, and bounded OS-title-bar drag-back docking when a focused detached window is moved into the main-window envelope. Detachable Scene View remains future work.
- Viewport transform hotkeys use local action profiles. The current profile editor is config-based; a richer in-window controls editor remains future work.
- The viewport transform gizmo is intentionally scoped to world-axis move, rotate, and scale behavior for the current sandbox object model. The sandbox now also exposes explicit viewport tool modes, diagnostics, and direct transform fallback controls so interaction failures can be diagnosed without assuming the gizmo is the only path.
- Signed negative scale is preserved for mirror transforms and bounds remain usable, but advanced mirrored normal, winding, and material-authoring workflows are still early.
- Rigid-body physics v1 supports static, dynamic, and kinematic bodies with sphere, axis-aligned box, and plane colliders; mesh collision, constraints, controllers, and advanced simulation remain future work.
- The first 2.5D camera foundation is available through perspective, side, top-down, and isometric sandbox presets with orthographic zoom. Sprites, layered depth, parallax, animation, and movement-plane constraints are not implemented yet.
- Visual and interaction checks still need manual QA on a local desktop session.
- HenkaSandbox3D is an engine sample and QA target, not a game. Real games built with Henka should live in separate repositories.

## License

Henka Engine is available under the [MIT License](LICENSE).
