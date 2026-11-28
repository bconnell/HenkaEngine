# Henka Engine Dimensional Expansion and Post-3D Roadmap

This roadmap describes planned improvements to Henka's editor, distribution, dimensional support, and longer-range engine capabilities. Henka is a general-purpose MIT-licensed game engine. The current priority is a complete 3D game-development workflow.

For the current development direction, see [Roadmap](roadmap.md). For implemented capabilities and limitations, see [Current Capabilities](current-capabilities.md). This document describes plans, not a release schedule or claims of current support.

## Planned sequence

1. Complete the general 3D engine, including the integrated editor, packaging, external-project workflows, animation, and whole-system validation.
2. Introduce first-class 2D support on the shared engine foundations.
3. Introduce first-class 2.5D support by combining the mature 2D and 3D systems.
4. Validate integrated 3D, 2D, and 2.5D workflows.

Advanced renderer features, XR, additional devices, and further platform expansion are longer-term possibilities. No dates or release commitments are announced here.

## Editor and product experience

The native Sandbox already provides an integrated docked and detached editor
workspace with Scene View, utility panels, Object Details, and bounded authoring
tools. The 3D roadmap continues to make these workflows discoverable, reliable,
and complete for creating, editing, testing, and packaging game projects.

### Product identity

Further editor identity improvements include consistent application naming,
icons, build and version information, About and licensing information, and
clear startup, project loading, error, and shutdown presentation. Consistent
terminology across menus, inspectors, dialogs, help, diagnostics, and packages
will support familiar navigation.

### Design system

The editor already ships bundled Atkinson Hyperlegible typography and a
persisted global Editor UI Scale. Ongoing design-system work continues to
improve consistency and interaction clarity, including:

- continued text readability, responsive controls, consistent spacing, and
  panel layouts;
- buttons, toggles, checkboxes, segmented controls, dropdowns, numeric and vector inputs, sliders, tabs, trees, toolbars, breadcrumbs, search, context menus, tooltips, dialogs, notifications, progress indicators, and error messages;
- clear hover, pressed, selected, focused, disabled, warning, destructive, invalid, and modified states;
- a refined dark appearance, with additional appearance options planned as the editor matures.

### Information architecture and discoverability

Further workspace improvements will better connect the existing Scene View,
bounded hierarchy and Object Details workflows with project/content browsing,
modeling, materials, terrain, animation, audio, scripting, diagnostics,
packaging, and profiling. Context-sensitive controls and searchable commands
will make relevant tools easier to find.

### Project and content experience

Planned project and asset management features include:

- a project launcher with recent-project selection and project creation, templates, help, and engine-version information;
- stable asset identity across supported path and name changes;
- folders, search, filters, thumbnails, import, rename, move, duplicate, delete, drag-and-drop, dependency inspection, and missing-dependency diagnostics;
- separate project settings and personal editor preferences.

### Error, progress, and dirty-state UX

New/Open/Reload and editor-close transitions already protect dirty Scene
Document and Terrain state with Save, Discard, or Cancel. Further work extends
modified-state tracking to more authored resources and adds broader Save All
and project-switch behavior, with useful recovery messages and safe operation
progress where supported. Descriptive undo/redo labels will help users
understand which action is being reversed or repeated.

### Viewport and authoring polish

Further viewport work refines existing selection feedback, grids, transform
gizmos, snapping, and topology displays while adding capabilities such as
camera previews, expanded pivot controls, and useful debug views.

Henka already provides discoverable component selection and a broad set of
preview/Apply/Cancel topology operations. Further modeling work improves
numeric workflows, snapping, soft editing, topology diagnostics, and material
preview through the graphical editor.

### Accessibility and layout acceptance

The editor has a bundled readable font and global UI scaling. Further
accessibility and layout work includes keyboard navigation, visible focus,
contrast, color-independent status cues, operating-system high-DPI behavior,
and reduced-motion options where relevant. Validation continues across
representative resolutions, display scales, clipping, overlap, control
accessibility, and layout restoration. These plans do not claim complete
accessibility conformance.

## Packaging, distribution, and release engineering

Henka's distribution plans include an editor and runtime that can support normal game-development workflows without requiring a source build for every operation.

### Initial Windows distribution direction

The initial planned distribution is a versioned portable Windows package. An installer and updater may follow as binary layout and migration support mature. Packages are expected to contain the supported editor/runtime components, public SDK headers where applicable, templates, required dependencies and assets, packaging tools, help, build identity, and licensing notices.

### Distribution guarantees

Planned distribution capabilities include:

- engine version, build identity, source provenance, checksums, and reproducible package assembly;
- project/schema compatibility and migration information;
- separation of settings, projects, caches, generated output, and installed engine files;
- crash and recovery diagnostics;
- external-project templates tested against the released SDK/runtime;
- release notes with features, fixes, compatibility changes, migration requirements, and known limitations.

Source builds, CMake, and PowerShell remain supported engineering interfaces. The longer-term game-development workflow is graphical.

### GameDev-oriented documentation transition

As the editor and binary distribution mature, user documentation will cover obtaining Henka, creating and opening projects, authoring and testing scenes, packaging games, and using the editor, scripting, and asset tools. Source-build, architecture, and contributor guides remain available for engine developers.

## First-class 2D

2D is a first-class dimensional mode over Henka's existing canonical foundations, sharing the same scene graph and engine architecture while receiving dedicated 2D runtime, editor, asset, physics, rendering, validation, and release support.

It begins only after the current general 3D completion boundary closes.

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

2.5D is a first-class Henka dimensional capability with dedicated workflows, editor tools, runtime contracts, packaging, validation, and release support. It begins only after both the general 3D and first-class 2D completion boundaries close.

Its implementation should deliberately compose the mature shared 2D and 3D foundations so 2.5D receives full product-level support while preserving one canonical engine architecture.

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

## Advanced expansion after dimensional completion

Further expansion is a longer-term possibility after integrated 3D, 2D, and 2.5D support. The following areas describe potential directions rather than committed features or release dates.

Possible later areas include:

- advanced renderer quality including robust glass/transmission, skin, hair, water, temporal anti-aliasing, streaming/resource residency, and later backend-specific improvements;
- VFX, decals, vegetation, world interaction, destruction, and Smart Assets/Objects;
- production asset database/browser and later catalog/distribution workflows with search, preview, versions, dependencies, licensing/provenance, install/update behavior, and safe project integration;
- replay/deterministic capture, developer console, profiling, frame/resource diagnostics, crash/recovery, and performance tooling;
- plugin/extension SDK, importers, editor/runtime extension points, permissions, versioning, migration, and modding boundaries;
- broader networking/runtime scale, sessions, replication, authority, large-world streaming, and server maturity;
- cinematic/sequencer, richer animation/character workflows, interactive media, advanced Audio, platform expansion, and renderer backends.

The mature engine should absorb these as coherent subsystem improvements with strong integration and production proof, avoiding isolated showcase-only additions.

## XR and immersive-device direction

XR is a potential future presentation and input capability for native 2D, 2.5D, and 3D projects, using the shared engine architecture.

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
