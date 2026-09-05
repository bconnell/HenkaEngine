# Henka Engine Post-Foundation Roadmap

This document complements [roadmap.md](roadmap.md) by recording the intended sequence after the current 3D engine and integrated authoring foundation reaches its defined production boundary. It describes direction, dependency order, and acceptance expectations. It does not define dates or change current capability status.

Henka remains a general-purpose MIT-licensed game engine. Later phases should build on the same canonical scene, identity, persistence, asset, input, scripting, Audio, packaging, and external-project boundaries while keeping one shared engine architecture across dimensions and device classes.

## Planned sequence

The intended high-level order is:

1. Complete the current 3D engine and integrated modeling/content-authoring production boundary.
2. Run a dedicated commercial-grade editor/product-experience and branding maturity campaign.
3. Establish versioned binary distribution and release engineering for ordinary game developers.
4. Reframe the public README and primary documentation around the GameDev workflow once the binary product path is ready.
5. Ship and harden a mature Windows release path.
6. Build first-class 2D on the mature common engine foundations.
7. Release and harden the 2D path.
8. Complete first-class 2.5D as deliberate composition of mature 2D and 3D systems.
9. Release and harden the 2.5D path.
10. Reconcile and execute the remaining advanced-engine wishlist against the then-current engine so already-absorbed work is not duplicated.
11. Pursue later immersive/XR and device-expansion work through generic capability-driven engine interfaces.

The ordering is dependency-driven. Independent preparation may overlap when it does not create competing sources of truth, unstable public contracts, or duplicate work.

## Commercial-grade editor and product experience

After the current engine/content-authoring completion campaign, Henka should receive a dedicated product-experience pass. The goal is not cosmetic polish alone. The editor should behave like a mature commercial-style application while remaining MIT licensed and source-available.

### Product identity

The normal developer-facing application should present a consistent Henka Engine / Henka Editor identity and a deliberate end-user product experience, distinct from internal samples, test harnesses, and CMake tooling. The product experience should include:

- consistent application naming, iconography, version/build identity, About information, and license presentation;
- intentional startup, project-open, loading, failure, and shutdown presentation;
- coherent terminology across menus, inspectors, dialogs, help, diagnostics, and packaging;
- restrained use of Henka branding so the workspace remains professional for long work sessions.

The Sandbox may remain an internal reference and QA target, but game developers should interact with a deliberate product surface.

### Design system

The editor should use one documented visual and interaction grammar covering:

- typography hierarchy and readable minimum sizes;
- spacing, control heights, panel rhythm, and gutters;
- buttons, toggles, checkboxes, segmented controls, dropdowns, numeric/vector inputs, sliders, tabs, trees, toolbars, breadcrumbs, search fields, context menus, tooltips, dialogs, toasts, progress indicators, and error banners;
- normal, hover, pressed, selected, focused, disabled, warning, destructive, invalid, and modified states;
- a high-quality dark theme first, followed by additional appearance modes only when they can meet the same standard.

The UI should not imitate another engine closely enough to become derivative. Familiar desktop/game-development conventions should be used where they improve discoverability.

### Information architecture and discoverability

The workspace should provide coherent places for scene hierarchy, Scene View, Object Details/Inspector, project/content browsing, modeling, materials, terrain/world tools, animation, Audio, scripting, diagnostics/output, build/package, and profiling.

Context-sensitive tools should become relevant when the active object or component changes without hiding essential functionality. A searchable command/action surface should eventually provide fast access to commands, settings, assets, objects, and help where appropriate.

### Project and content experience

The mature product should include a practical project/start experience and content browser with:

- recent projects, create/open project, templates, documentation/help entry points, and engine-version visibility;
- stable asset identity underneath path/name changes;
- folders, search, filtering, thumbnails, import, rename, move, duplicate, delete, drag/drop, dependency/reference inspection, and missing-dependency diagnostics;
- clear project settings separated from per-user editor preferences.

### Error, progress, and dirty-state UX

Human-readable diagnostics should explain what failed, why it matters, and the useful recovery action while retaining technical details for debugging. Long-running operations should expose progress and safe cancellation where supported instead of freezing without explanation.

Modified state should be derived from authoritative data and clearly surfaced for scenes, assets, materials, scripts, models, and project settings. Save All and close/project-switch behavior should preserve transaction and failure guarantees.

Undo/redo should use meaningful action labels such as `Undo Bevel Edges` or `Undo Assign Material` when the underlying history can provide them.

### Viewport and authoring polish

Commercial-quality viewport work includes clear selection/hover feedback, unobtrusive grids and overlays, polished gizmos, snapping indicators, camera previews, topology displays, pivots, safe-frame/debug views, and presentation that never lets diagnostics obscure the edited subject.

Modeling UX should make component mode, selection state, preview, numeric input, Apply/Cancel, snapping, proportional/soft editing, topology warnings, material preview, and relevant close-up inspection obvious without requiring knowledge of the C API.

### Accessibility and layout acceptance

The editor should deliberately validate keyboard navigation, focus indicators, UI scaling, readable text, contrast, color-independent status cues, high-DPI behavior, and reduced-motion options where relevant.

Executable layout/visual validation should exercise at least representative 1280x720, 1920x1080, 2560x1440, ultrawide, and high-DPI cases and detect clipping, overlap, inaccessible controls, off-screen dialogs, zero-width regions, and broken layout restoration.

## Binary distribution and release engineering

Binary distribution should become a first-class product boundary before 2D begins so later dimensional expansion lands on a mature user-facing engine instead of a source-only development workflow.

### Initial Windows distribution

The preferred initial mature distribution is a versioned portable Windows package because it is straightforward to reproduce and validate. An installer and updater may follow after the binary layout and migration contracts stabilize.

A release package should include the editor/runtime pieces required by the supported GameDev workflow, public SDK/header material where applicable, templates, required runtime assets/dependencies, packaging tools, offline/help content, exact build identity, and licensing notices.

### Release guarantees

Release engineering should provide:

- explicit engine version and build identity;
- exact source provenance for shipped binaries and supporting assets;
- checksums and release verification;
- reproducible package assembly from one exact candidate;
- project/schema compatibility information;
- migration policy for versioned project data;
- clean separation of user settings, project data, caches, generated outputs, and installed engine files;
- crash/recovery diagnostics appropriate to a distributed application;
- validated external-project templates against the released binary/SDK boundary;
- release notes that distinguish new features, compatibility changes, migration requirements, fixes, and known limitations.

Normal game-development workflows should not require shell commands once the product path is ready. CMake, PowerShell, and source builds remain supported engineering and automation interfaces; the normal product workflow is graphical.

### GameDev-oriented documentation transition

When the binary path is genuinely ready, the README and primary docs should be reorganized around a game developer's questions:

- What is Henka?
- What can I build with it?
- Where do I download it?
- How do I create/open a project?
- How do I author, Play, and package a game?
- Where are the editor, scripting, asset, and packaging guides?

Implementation language, source-building, architecture, and contribution details remain important but should move below the normal product workflow instead of leading it.

## First-class 2D

2D is a first-class dimensional mode over Henka's existing canonical foundations, sharing the same scene graph and engine architecture while receiving dedicated 2D runtime, editor, asset, physics, rendering, validation, and release support.

### Dimensional authority

A formal scene/project dimensional profile should define the 2D world plane and depth/layer policy. A 2D-facing transform view may expose 2D position, scalar rotation, 2D scale, and layer/depth controls, but the canonical scene transform remains authoritative.

The initial 2D profile may use one conventional plane while keeping the mapping architecture explicit enough that additional plane mappings can be added without duplicating scene identity or persistence.

Critical invariants include:

- no independent `Transform2D` truth that can disagree with the scene transform;
- parent/reparent operations preserve declared local/world behavior without leaking off-plane drift;
- save/reload and Play preserve dimensional-profile and ordering state exactly;
- 2D editor tools cannot silently mutate forbidden axes.

### Sprite and texture-region assets

The 2D asset foundation should include:

- sprite components;
- texture regions and stable region identity;
- pivots/origins, flip X/Y, tint, opacity, layer/order, and pixels-per-unit or equivalent sizing authority;
- atlases and sprite sheets;
- nine-slice/nine-patch and tiled presentation where appropriate;
- deliberate filtering/mipmap/pixel-art policies;
- resilience to texture move/rename/reimport through stable asset identity.

Sprite components should reference stable region/asset identities wherever the asset architecture permits; raw pixel coordinates should not become authoritative cross-asset references.

### Dedicated 2D renderer

The renderer should provide a real optimized 2D path with deterministic draw ordering, sprite/atlas batching, camera culling, transparent handling, clipping/scissor support, pixel-art filtering options, color-space correctness, and scalable workload validation.

Completion evidence should use meaningful stress fixtures with large sprite/tile populations and multiple atlases/material states, with workload scale sufficient to prove the real renderer path.

### 2D camera

A first-class 2D camera should support pan, zoom, follow, dead zones, look-ahead, bounds, smoothing, shake, aspect/resolution handling, world/screen conversions, viewport resizing, high-DPI behavior, and an explicit pixel-perfect mode.

Pixel-perfect behavior requires executable tests for subpixel jitter, odd/even resolutions, zoom, camera following, parallax, shake, and DPI changes.

### 2D physics

Henka should provide a genuine 2D physics domain designed for 2D gameplay and integrated with the canonical scene transform. The production boundary should include appropriate static/dynamic/kinematic bodies, box/circle/capsule/polygon/edge or chain shapes, triggers, layers/masks, contacts/events, ray/shape/overlap queries, fixed-step ownership, and a useful initial joint/constraint subset.

The canonical scene transform remains the external spatial truth. One entity must never have competing authoritative 2D and 3D physics writers for the same transform.

### 2D Character Controller

Reusable side/platformer and top-down controller modes should cover the common production behaviors appropriate to each profile, including grounded movement, running, jumping, gravity, slopes, steps, one-way platforms, moving platforms, wall/contact behavior, drop-through policy, free top-down planar movement, acceleration/deceleration, collision sliding, facing policy, keyboard/gamepad input, persistence, scripting, Play, package, and external-project validation.

### Tilesets and tilemaps

Tilesets should use stable tile identity and carry visual region, animation, collision, custom metadata, terrain/autotile, material/light, and navigation information where applicable.

Tilemaps should use chunked sparse storage as the intended scalable architecture. They should support multiple layers, visibility/order, large maps, paint/erase/fill/selection/move, undo/redo, collision generation, localized rebuilding, streaming where necessary, and deterministic save/reload.

Autotile/terrain-rule support should be deterministic and cover adjacency, corners, transitions, and chunk boundaries without duplicated collision seams or rule changes after reload.

### Parallax, animation, lighting, VFX, and navigation

The 2D production path should also include:

- layered parallax with independent factors and repeating backgrounds;
- frame/sprite animation clips with timing, loop/ping-pong policy, events, playback control, and editor timeline support while reusing common animation concepts where practical;
- 2D lighting appropriate to the renderer, including ambient/point-style lighting, normal-mapped sprites, masks/layers, emissive presentation, and supported shadowing;
- 2D sprite-particle/VFX presentation through common VFX foundations with the same production validation expectations as other VFX paths;
- navigation suitable for top-down/tile-based worlds, with debug visualization and tilemap integration where appropriate.

### 2D editor mode

The existing Henka application should gain a dedicated first-class 2D authoring mode. It should provide a 2D grid, pan/zoom, dimensional transform gizmos, pixel/grid snapping, sprite placement, pivot and atlas/region editing, tilemap painting, collision-shape editing, parallax controls, camera bounds, lighting/navigation overlays, and animation-timeline integration.

Selection and canonical object identity should survive switching between 2D, 3D, and later hybrid views.

### 2D acceptance

A 2D capability is not complete at API or renderer level alone. Its normal path must prove authoring, undo/redo, save, project close/reopen, reload, Play, runtime, package, external project, performance, and applicable visual/manual QA.

A permanent 2D end-to-end fixture should combine representative tilemap, controller, moving platform, animated sprite, parallax, lighting, VFX, Audio, runtime UI, scripting, and save-game behavior so integration regressions fail visibly and executably.

## First-class 2.5D

2.5D is a first-class Henka dimensional capability with dedicated workflows, editor tools, runtime contracts, packaging, validation, and release support. Its implementation should deliberately compose the mature shared 2D and 3D foundations so 2.5D receives full product-level support while preserving one canonical engine architecture.

### Supported composition profiles

The editor/runtime should support explicit hybrid profiles such as:

- side-scrolling gameplay constrained to a plane while using true 3D meshes, lighting, environments, and cameras;
- 2D sprites operating in a 3D world;
- isometric hybrid scenes with sprites and 3D props;
- top-down hybrid scenes with 2D gameplay rules and 3D presentation;
- custom constrained-plane profiles where the common authority model supports them.

Profiles should set sensible defaults without hiding the underlying authoritative state.

### Movement-plane constraints

A canonical movement-plane/constraint model should describe origin, normal, tangent axes, optional allowed depth band, and snap/constrain policy. Character Controllers, physics adapters, camera tools, scripting, editor gizmos, persistence, networking, and save-game restoration should consume the same constraint truth.

Plane constraints must survive teleports, parenting, moving platforms, save/reload, network/state restoration, and repeated Play cycles without off-plane drift.

### Hybrid rendering and draw order

Hybrid scenes need one deliberate composition policy for sprites, opaque/transparent 3D meshes, particles, world-space UI, billboards, tilemaps, decals, and shadows.

World depth should remain authoritative by default. Presentation layers/order may provide explicit overrides in defined contexts, but they must not become a competing hidden depth system.

Regression fixtures should target transparent ordering, z-fighting, camera-dependent popping, sprite/mesh occlusion, billboard orientation, perspective scaling, particles against sprites, and layer overrides.

### Sprite orientation and lighting

Explicit sprite orientation modes should include fixed world-facing, camera-facing billboard, vertical-axis billboard, plane-aligned, and custom orientation where supported.

Hybrid lighting should share the real renderer/environment and remain part of the same world presentation. Where supported, sprites may participate through normal maps, emissive response, fog/environment, and compatible shadow models. Unsupported combinations should be explicit in the editor and documentation.

### Physics-domain authority

2.5D may support either 2D physics with 3D visuals or 3D physics constrained to a plane. Mixed-domain interaction may be added only through explicit bridge/query semantics.

Each gameplay entity has one authoritative physics domain for transform publication. Physics2D and Physics3D must never independently write the same entity transform.

### Hybrid tile/world authoring

Tilemaps may participate as world planes alongside 3D props, elevation/depth metadata, hybrid lighting, parallax, and either 2D or constrained-3D collision according to the chosen profile.

### 2.5D editor mode and fixtures

The editor should expose the gameplay plane, depth guides, dimensional object distinctions, constrained gizmos, camera preview, sprite orientation, collision-domain visualization, occlusion/sorting diagnostics, and the ability to inspect the same scene through relevant 2D/3D/hybrid views.

Permanent executable fixtures should include at least a 3D side-scrolling level, a sprite character in a 3D room, an isometric hybrid, a top-down hybrid, and a tilemap-plus-3D-props scene.

## Advanced expansion after 2D and 2.5D

After the dimensional roadmap is complete, Henka should reconcile the maintained long-range wishlist against the mature engine. Each requested item should be classified as already absorbed, partially absorbed, still missing, superseded by a better architecture, or still desired advanced work before implementation begins.

This phase includes the advanced systems already represented in the broader roadmap and should continue to deepen areas such as:

- advanced renderer quality including robust glass/transmission, skin, hair, water, temporal anti-aliasing, streaming/resource residency, and later backend-specific improvements;
- VFX, decals, vegetation, world interaction, destruction, and Smart Assets/Objects;
- production asset database/browser and later catalog/distribution workflows with search, preview, versions, dependencies, licensing/provenance, install/update behavior, and safe project integration;
- replay/deterministic capture, developer console, profiling, frame/resource diagnostics, crash/recovery, and performance tooling;
- plugin/extension SDK, importers, editor/runtime extension points, permissions, versioning, migration, and modding boundaries;
- broader networking/runtime scale, sessions, replication, authority, large-world streaming, and server maturity;
- cinematic/sequencer, richer animation/character workflows, interactive media, advanced Audio, platform expansion, and renderer backends.

The mature engine should absorb these as coherent subsystem improvements with strong integration and production proof, avoiding isolated showcase-only additions.

## XR and immersive-device direction

XR should be treated as a presentation/input capability over native 2D, 2.5D, and 3D worlds, not as another dimensional engine.

Standards, headset runtimes, and hardware capabilities evolve quickly. Henka should preserve backend isolation and reassess the best standards-oriented implementation path when this phase becomes active, keeping the design open to the best available standards and devices at that time.

### Cross-dimensional XR presentation

A project may expose one or more presentation profiles while retaining its native gameplay dimensionality:

- **2D:** baseline virtual-canvas/spatial-panel presentation, with optional layered/diorama enhancement while gameplay coordinates remain 2D;
- **2.5D:** diorama/tabletop, constrained-plane stereoscopic presentation, or compatible immersive hybrid presentation;
- **3D:** native immersive stereoscopic presentation with tracked head pose and appropriate spatial interaction.

Entering XR should not require a 2D/2.5D project to convert its authoritative gameplay representation into fake 3D.

### XR input abstraction

Henka's action system should remain the gameplay-facing authority so keyboard/mouse, gamepad, tracked controllers, gaze, hand tracking, and future devices can map into the same semantic actions.

Pointer-style interaction should provide a canonical conversion from an XR controller/gaze ray through the active presentation surface into the native 2D, 2.5D, or 3D coordinate domain.

Headset pose remains presentation/tracking input and should not silently replace the authoritative gameplay entity transform.

### Immersive I/O capability layer

Later immersive support should expose generic capability-driven interfaces that remain independent of individual hardware brands. Candidate capability families include:

- head, hand, and full-body tracking;
- locomotion devices including omni-directional treadmills;
- controller, vest, suit, and future body haptics;
- optional environmental/sensory feedback where hardware can expose a safe interoperable contract.

Applications should query available capabilities and degrade gracefully across the capability set exposed by the connected hardware.

### Semantic haptics

Games should request semantic effects such as directional impact, sustained pressure/contact, vibration/texture, recoil, environmental pulse, heartbeat-style effect, or supported temperature cue. Device adapters translate those requests to the connected actuator layout.

This keeps authored effects portable across simple controllers, vests, full-body suits, and later device generations.

### Locomotion-device authority

Physical locomotion hardware should report movement intent, speed, orientation, gait/stance, or other supported inputs into Henka's input/controller layer. The Character Controller and physics remain authoritative over legal world movement.

The intended chain is:

`physical locomotion device -> locomotion intent -> Henka action/controller -> authoritative movement`

Direct hardware input does not teleport or otherwise bypass the authoritative scene/controller movement path.

The same model can project locomotion into a 2D or 2.5D gameplay plane or use full 3D movement according to the active dimensional profile.

### Device fallback and safety

Immersive effects should degrade through supported capability levels where useful, for example full-body haptic effect to vest effect to controller vibration to audiovisual cue.

Device-level safety and calibration belong below game scripts. Supported hardware adapters should enforce appropriate amplitude/intensity, duration, repetition, thermal, tracking-loss, emergency-stop, disconnect, and calibration limits independently of application requests.

No project script should be able to bypass the device safety contract by issuing raw stronger commands.

### XR acceptance

An XR capability should not be called supported until the claimed presentation/input/device boundary has executable validation, performance/frame-pacing evidence, tracking-loss and reconnect behavior, device fallback, persistence/configuration, package/external-project proof, and applicable human comfort/usability QA.

## Completion principle for these phases

The same Henka completion standard applies throughout this document.

An API, UI control, sample screenshot, device adapter, or demonstration proves only the scope it actually exercises. A phase reaches its defined production boundary only after the intended GameDev workflow is connected vertically through authoritative data, editor/tooling, persistence, reload, Play/runtime, subsystem derivation, undo/redo where applicable, packaging, external-project use, executable regression coverage, performance evidence, and applicable visual, auditory, accessibility, or human QA.