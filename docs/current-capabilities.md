# Current Capabilities

This is the detailed, code-backed inventory for the current Henka Engine checkout. It records implemented foundations and their boundaries without turning the public README into an implementation ledger.

README capability statuses are maintained in the small public contract at [capability-statuses.tsv](capability-statuses.tsv); each entry names the authoritative section below.

> **Reading rule:** This document separates implemented capability from incomplete or planned work. A long list of foundations does not imply production readiness.

## Contents

- [Core / Platform](#core--platform)
- [Renderer](#renderer)
- [Assets / Materials](#assets--materials)
- [Scene / Camera](#scene--camera)
- [Editor Workspace](#editor-workspace)
- [Game Authoring Foundation](#game-authoring-foundation)
- [Modeling / Content Authoring](#modeling--content-authoring)
- [Scripting / Behaviors](#scripting--behaviors)
- [Terrain / World](#terrain--world)
- [Physics](#physics)
- [2.5D](#25d)
- [2D](#2d)
- [Networking / Dedicated Server](#networking--dedicated-server)
- [Persistence](#persistence)
- [External Project Support](#external-project-support)
- [Testing / Packaging](#testing--packaging)
- [Audio](#audio)
- [Known Gaps](#known-gaps)

## Core / Platform

### Available

- Henka is written in C17 and configured with CMake.
- The repository builds the graphical `henka` library, renderer-independent `henka_runtime` library, `henka_sandbox3d` sample, dedicated-server host, and `henka_tests` target.
- Public runtime contracts use bounded checked growth, finite-value checks, generation-checked 64-bit scene identities, explicit ownership, and transactional replacement where a failed operation must retain the prior state.

### Platform boundary

- The validated development and packaging path targets 64-bit Windows with MSVC and PowerShell-compatible scripts.
- SDL3 is behind the platform boundary.
- Other operating systems are not currently claimed as supported.

See [architecture.md](architecture.md), [runtime-foundations.md](runtime-foundations.md), and [platform-support.md](platform-support.md).

## Renderer

### Available

- The Sandbox exposes Wireframe, Solid, Material Preview, and Rendered Scene View policies through the OpenGL renderer path.
- Rendered mode consumes imported PBR materials, scene lighting, HDR targets, bounded environment/IBL fallbacks, directional and bounded local-light shadows, fog, bloom, tone mapping, AO, and a temporal reconstruction foundation.
- The renderer boundary is isolated from renderer-independent runtime and authoring data.

### Remaining hardening

These rendering paths remain subject to broader manual visual validation.

See [architecture.md](architecture.md) and [runtime-foundations.md](runtime-foundations.md).

## Assets / Materials

### Available

- Bounded glTF/GLB import covers scene roots, node transforms, meshes, cameras, punctual lights, and shared PBR material dependencies.
- OBJ loading supports bounded local files, finite positions, optional UVs/normals, positive and negative indices, and basic fan triangulation.
- Asset metadata and texture dependencies are manager-owned.
- Missing textures and supported OBJ failures use visible, validated fallbacks; hard failures propagate when a safe fallback is not valid.
- Material instances support validated scalar/vector, alpha-mode, and semantic-texture overrides, dependency inspection, revision refresh, and transactional reimport.

### Not yet available

- Dedicated user-authored material-file authority
- Text-entry import
- Drag/drop
- Dependency-graph tooling

See [model-loading.md](model-loading.md) for import limits and failure behavior.

## Scene / Camera

### Available

- Scene entities and logical selection ownership
- Cameras and input actions
- Screen rays and framing
- Transform actions
- Shared overlay gizmos
- Perspective 3D, Side 2.5D, Top-down 2.5D, and Isometric 2.5D presets
- Orthographic zoom and deterministic vertical-view bases

The Scene View Compass is a dynamic, camera-driven viewport instrument with axis snapping, orbit dragging, projection switching, persisted placement and scale preferences, and an application-local automation input path for deterministic graphical validation.

### Remaining hardening

The scene/camera feature boundary is functionally present, but broader visual and manual presentation validation remains open.

## Editor Workspace

### Available

The native Sandbox workspace includes:

- Scene View and utility panels;
- docked and detached production-panel surfaces;
- validated split topology;
- tabs;
- layout presets and named layout slots;
- layout history and reset-layout recovery;
- scrollable panel bodies;
- focus-loss/Escape cancellation.

Scene Objects exposes logical scene owners while retaining render-child identities for materials and component editing.

Available object operations include:

- Add Cube
- Duplicate
- Delete
- Make Editable
- visibility
- locks
- transforms
- bounded material inspection

UI controls use bounded IDs, matched frame construction, release-confirm behavior, and transactional composite drawing.

### Remaining work

- Durable scene rename
- Complete project serialization
- Native desktop feel
- Complete manual interaction QA

See [ui.md](ui.md), [editor-controls.md](editor-controls.md), and [help/sandbox3d.md](help/sandbox3d.md).

## Game Authoring Foundation

> **Status:** Foundation

The Sandbox has a bounded Game Authoring V1 path for its registered scene objects.

### Scene identity and authored data

- Persistent Scene Document IDs map to generation-checked runtime entities through a dedicated adapter.
- Runtime handles are not serialized.
- Object Details exposes authored Physics and Interaction values for bound objects.
- Supported values include primitive body/shape and trigger choices, interaction enablement, and prompt inspection.
- Edits validate before the working scene is updated.

### Save and reload

Save Scene and Reload Scene use the confined, checksummed `.hscene` format.

Candidate loads, rebinding, and runtime presentation updates fail closed and retain the prior authoring state on failure.

### Play lifecycle

Play, Pause, Resume, Step, and Stop are owned by a dedicated bounded session.

The Game Authoring path:

1. creates an independent runtime scene clone;
2. borrows renderer resources;
3. preserves generation-checked entity handles;
4. keeps runtime bodies and simulation transforms outside persistence;
5. destroys the runtime scene on Stop;
6. leaves the authored scene unchanged;
7. rejects authoring changes and scene save/reload while Play is active.

Play uses a bounded clone of the Edit baseline, so runtime state cannot leak back into authoring on Stop. Save Play State and Load Play State are explicit operations.

### Scripting and behavior integration already available

The shared scripting foundation includes:

- a scripting API/host schema;
- bounded HenkaScript lexer/parser/type checking;
- bounded HenkaScript callable bytecode/VM execution;
- bounded Lua lifecycle execution;
- fixed-update/destruction and targeted contact lifecycle adapters;
- a language-neutral generation-checked behavior lifecycle runtime;
- a typed, bounded Script Host dispatcher;
- a FIFO event queue;
- isolated Play mapping from persistent object IDs to runtime entities for the current Entity, Transform, Physics, Audio, and Events slice;
- a bounded behavior-state store with explicit sidecar save/load;
- mixed-language `OnEvent` routing.

Persisted `.lua` and `.hks` attachments can be loaded through a bounded, confined-path asset loader and assembled into a mixed-language behavior runtime by persistent Scene Document object identity.

The Sandbox Play session owns that runtime for isolated Create/Start/Update/OnEvent/Stop dispatch.

### Inspector and source authoring already available

The Inspector can:

- create confined Lua or HenkaScript behavior templates;
- attach them transactionally;
- expose a bounded editable source panel;
- derive HenkaScript spans, colors, and insertion indentation from compiler tokenization/token APIs rather than an editor-owned grammar table;
- validate Lua through its backend while preserving persisted source formatting;
- preserve bounded source bytes, indentation, diagnostics, Save, and Revert behavior.

The source panel exposes **Edit**, **Save**, **Revert**, and **Reload**.

The Play-session seam can transactionally reload a persisted behavior backend while preserving the generation-checked slot and active lifecycle state. Reload uses the same coordinator seam in Play and reloads persisted source outside Play. Candidate reload failures preserve bounded source diagnostics through the editor seam.

### Incomplete

This is not a complete game editor. Remaining gaps include:

- hierarchy authoring;
- broader imported-object registration;
- complete source/material/project serialization;
- production gameplay workflows;
- complete host API coverage;
- full Inspector authoring;
- debugger tooling;
- broader runtime/resource mapping;
- complete project scripting workflows.

See [scripting-foundation.md](scripting-foundation.md) for the scripting boundary.

### Runtime hierarchy foundation

The public runtime scene now provides a bounded generation-checked parent/child
transform foundation with cycle rejection, keep-local/keep-world reparenting,
subtree propagation, and parent-destruction promotion. HSCN v6 persists parent
IDs and migrates v1-v5 objects to roots in memory without rewriting the source
file. Hierarchy editing in the Sandbox, hierarchy history, broader imported
object registration, complete source/material/project serialization, and
production gameplay workflows remain open.

## Prefabs / Reusable Scene Objects

- A bounded runtime prefab foundation is available through the public API.
  `henka_prefab_create_from_scene` captures a selected scene root and its active
  descendants in deterministic scene order; `henka_prefab_instantiate` creates
  independent production scene entities, preserves local transforms and
  hierarchy, and applies names, tags, visibility, flags, bounds, interaction,
  and material state through the normal scene APIs.
- Snapshot text is owned by the prefab. Meshes, shaders, textures, and material
  definitions remain borrowed from their existing owners and must outlive the
  prefab and its instances. Instantiation is bounded to 4096 entries and rolls
  back all newly created entities if validation or allocation fails.
- Persistent prefab identities and revisions, inherited-versus-overridden
  values, source-change propagation, duplication, unpacking, serialized
  prefab assets, editor authoring, and packaged/external-project workflows
  remain in progress. The current runtime snapshot is a foundation rather than
  a complete prefab authoring system.

## Modeling / Content Authoring

> **Status:** In Progress

### Integrated workflow

- Object, Vertex, Edge, and Face workflows are integrated into the Sandbox.
- Component selection, connected selection, bounded edge-loop selection, normal/X-Ray box selection, one-ring soft movement, axis-constrained movement, visible authored-face surfaces, and topology feedback are available.
- Box selection uses authored component identities and does not expose renderer triangulation as topology.

### Authoring mesh contract

The authoring mesh API provides:

- stable vertex/edge/face identities;
- connectivity and boundary queries;
- material regions;
- per-corner UV metadata;
- smoothing and hard-edge intent;
- fail-closed polygon validation;
- deterministic caller-owned triangulation;
- bounded shared undo/redo.

### Topology analysis and safe repair

Topology analysis produces a deterministic non-destructive report covering:

- component structure;
- boundaries;
- manifold state;
- winding;
- seams;
- hard edges;
- degeneracy;
- duplicate faces;
- coincident vertices;
- valence;
- face-shape metrics.

Explicit repair can transactionally remove only enabled safe issues:

- isolated vertices;
- exact duplicate faces with matching winding, UVs, material, and smoothing;
- degenerate faces.

Unsafe duplicate groups are rejected. Vertex welding and winding rewrites are not implicit repairs.

### Vertex operations

Published Vertex operations include:

- Merge Center
- Merge Active
- Merge by Distance
- Connect Vertices
- Dissolve Vertex
- Delete Vertex
- Vertex Bevel
- bounded Vertex Extrude for a connected open boundary vertex fan, including the one-face corner case

Bounded Vertex Extrude preserves the base vertex, creates one offset cap vertex, replaces the incident fan, and creates the two boundary side faces transactionally.

Closed, disconnected, loose-edge, and incompatible-normal fans fail closed.

### General transactional operations

Available operations also include:

- plane/box creation;
- duplicate;
- face winding flip;
- face extrude;
- inset;
- planar bevel rings;
- face subdivision;
- selected-face deletion;
- bounded single-quad face Loop Cut;
- bounded uniformly spaced multi-cut for one isolated boundary-only quad;
- planar UV projection;
- island transforms;
- packing;
- seam detection.

The integrated Sandbox panel routes the bounded multi-cut through preview, Apply/Cancel, and undo.

### HAMS persistence

HAMS v5 writes portable little-endian data through unique same-directory temporary files and retains reads for checked-in v2/v3/v4 surface-only legacy sources.

HAMS v5 is required for persisted loose vertices and zero-face wire edges. Legacy files are migrated in memory only and are not silently rewritten.

### Imported-object authoring

Imported nontrivial objects can take the Make Editable path.

The bounded native authoring workflow also includes:

- native source persistence;
- evaluated mesh replacement;
- material promotion;
- supported PBR overrides;
- procedural detail textures;
- material undo/redo.

### Edge authoring boundary

Available edge authoring includes:

- bounded edge-loop/ring selection;
- transactional single-edge dissolve for compatible interior edges;
- single-edge delete of its incident face set;
- bounded standalone boundary-edge bevel;
- bounded multi-edge boundary bevel across distinct faces;
- bounded same-face boundary bevel with shared-endpoint corner caps;
- bounded compatible interior-edge bevel for an isolated two-quad patch;
- bounded surface-connected extrusion for one open boundary edge.

Surface-connected boundary-edge extrusion offsets the edge along its incident face normal, preserves the source face and selected hard-edge intent, and creates one connecting quad transactionally.

Interior/manifold edges and broader edge-set extrusion remain rejected.

Interior bevel rejects:

- hard edges;
- material/smooth/UV discontinuities;
- non-quad faces;
- neighboring shared boundaries;
- ambiguous endpoint fans.

Broader interior edge-set bevel and broader edge topology operations remain in progress.

The shared Sandbox modeling session and Authoring panel expose the bounded preview/cancel/apply path for boundary-edge extrusion.

### Current limitations

This is an integrated authoring foundation, not a production modeling suite.

Still incomplete:

- broader non-manifold or incompatible-normal fan handling;
- broader topology tooling;
- automatic UV unwrap;
- texture painting;
- rigging;
- skinning;
- animation authoring;
- broader source export;
- production-quality showcase anatomy/mechanical topology.

## Scripting / Behaviors

> **Status:** In Progress

### Available foundation

- Bounded HenkaScript and Lua lifecycle adapters
- Compiler-owned HenkaScript editor tokenization
- Scene Document binding
- Play dispatch
- State persistence
- Cross-language events

Broader runtime/resource mapping, complete Inspector authoring, and debugger tooling remain unfinished.

### Loose-component and wire/point authoring support

The core authoring representation preserves explicit loose vertices and standalone wire edges with stable logical IDs, bounded reusable storage, and HAMS v5 save/reload support.

The core modeling API also provides:

- bounded explicit-direction loose-vertex extrusion that preserves the source vertex and creates one metadata-inheriting standalone wire edge transactionally;
- bounded loose-edge extrusion that creates one parallel edge and one quad face.

Both reject unsupported source topology and invalid direction/distance inputs.

The topology overlay presents all authored source vertices and distinguishes loose vertices, boundary edges, and manifold edges with deterministic high-contrast markers.

The shared Sandbox modeling-operator session and Authoring panel can preview, cancel, and apply explicit-axis extrusion for exactly one selected loose vertex or standalone edge through core transactional operations. The same control routes one selected open boundary edge through face-normal surface-connected extrusion.

### Renderer-backed loose geometry

- Homogeneous wire-only and isolated-vertex-only authoring sources have bounded renderer-backed line and point evaluation.
- Mixed surface-plus-loose and no-face wire-plus-point sources use bounded renderer-backed multi-primitive ownership, preserving triangle, wire, and isolated-point parts instead of dropping or rejecting valid source geometry.
- Vertex-mode controls can add a loose vertex from finite X/Y/Z coordinates or add a standalone edge from exactly two selected vertices through the same transactional source/render/history boundary.

Broader loose-component editing and general surface-connected Vertex/Edge Extrude workflows remain unavailable. The bounded boundary-edge path is not a claim of complete editor-integrated Edge Extrude coverage.

### Quad-strip traversal, Loop Cut, and Edge Slide

A bounded deterministic compatible quad-strip traversal foundation is available for modeling operators.

It:

- records ordered face/entry/exit edges;
- terminates at boundaries or reports a closed ring;
- rejects hard, material, smoothing, UV, non-quad, non-manifold, and ambiguous crossings without partial output.

The shared topology layer also orders connected selected edge chains and cycles deterministically for Edge Slide and future edge-set operators.

Available operations include:

- bounded single-quad face Loop Cut;
- one factor-controlled quad-strip Loop Cut across a compatible open strip or closed ring;
- bounded signed-factor Edge Slide for one compatible open edge-loop or closed edge-cycle selection.

For Loop Cut, the editor validates a user-entered factor in the open interval `(0, 1)`, supports Preview/Refresh plus Apply and Cancel, and commits the candidate through the transactional engine boundary.

For Edge Slide, the shared modeling operator session supports preview, numeric factors in `(-1, 1)`, cancel, and one transactional Apply while preserving topology.

Preview changes evaluated render state only. Authoritative source/history changes occur on Apply.

Multiple cuts, broader interior-edge cases, and split/bridge workflows remain incomplete.

See [authoring-mesh.md](authoring-mesh.md), [runtime-foundations.md](runtime-foundations.md), and [showcase-assets.md](showcase-assets.md).

## Terrain / World

### Available foundation

- Deterministic four-layer material contract
- Bounded region persistence
- Streaming observers
- Resident CPU/physics/render owners
- Height and paint edits
- Fixed-budget edit history
- Collision patches
- Neighbor-aware normals
- Bounded LOD transitions
- Transactional mesh replacement

The Sandbox exposes terrain brush interaction, material-layer inspection, save/reload, stream diagnostics, and Rendered presentation.

Headless and dedicated-server paths exercise authority, snapshot recovery, and restart persistence.

### Remaining work

Automated topology and all-edge correspondence checks exist. Four-way corner visual approval, broader-world streaming, and background regeneration remain unfinished.

See [terrain.md](terrain.md) for the authoritative terrain contract.

## Physics

### Available foundation

Physics v1 provides fixed-step static, dynamic, and kinematic bodies with:

- sphere colliders;
- axis-aligned box colliders;
- plane colliders;
- contacts;
- impulses;
- friction;
- restitution;
- trigger events;
- raycasts.

Sandbox Physics QA and selected-body activation are available. Numeric, allocation, and body-replacement failures preserve prior simulation state where the contract requires retry safety.

### Remaining work

- Mesh collision
- Constraints
- Advanced simulation

The public Character Controller foundation is available for a real dynamic
sphere body. It validates bounded planar input, supports grounded jump
queuing, rejects stale physics bodies, and participates in the caller-owned
fixed-step lifecycle.

Full Character Controller movement remains unfinished: capsule geometry,
swept movement, sliding, slope handling, and step offsets are not currently
implemented. Mesh collision, constraints, and advanced simulation also remain
open.

See [physics.md](physics.md) and [help/sandbox3d.md](help/sandbox3d.md).

## 2.5D

Current 2.5D foundations are camera presets, orthographic framing/zoom, stable vertical orientation, and Sandbox persistence.

## 2D

A dedicated 2D renderer, sprites, texture regions, layered depth, parallax, sprite animation, and movement-plane/physics-axis authoring have not yet materially begun.

## Networking / Dedicated Server

### Available foundation

- Renderer-free dedicated-server host
- `henka_runtime` consumer boundary
- Fixed-tick simulation
- Loopback connectivity
- Terrain authority
- Snapshot recovery
- Edit persistence
- Graceful shutdown
- Bounded relevance-filtered reconnect/late-join selection

### Remaining work

Authentication, broader residency orchestration, production-scale capacity, and multiplayer soak remain unfinished.

See [dedicated-server.md](dedicated-server.md) and [external-game-projects.md](external-game-projects.md).

## Persistence

### Available foundation

Settings and save slots use:

- confined paths;
- bounded identifiers;
- complete record validation;
- same-directory temporary files;
- flush/close-before-replace behavior;
- failure retention of prior in-memory state.

Authoring sources use the versioned HAMS format. The Sandbox Game Authoring V1
path uses a bounded, checksummed v6 `.hscene` Scene Document for registered
objects and bounded Lua/HenkaScript behavior attachments. Parent IDs and
authored Audio listener/emitter values are validated during load; v1-v5
documents migrate in memory without rewriting the source file.

V1 documents load with behavior defaults for migration.

### Not yet available

- Complete scene/project serializer
- Remote/network-backed save policy

See [persistence.md](persistence.md) and [authoring-mesh.md](authoring-mesh.md).

## External Project Support

### Available foundation

Real games are intended to live in separate repositories.

- The external game template consumes public authoring and Terrain boundaries without depending on Sandbox source.
- The external server template links only the renderer-independent runtime boundary.
- Windows validation covers configure/build/startup and bounded public-API consumer smokes.

### Remaining work

A complete external scene/project editor and serializer remains future work.

See [external-game-projects.md](external-game-projects.md).

## Testing / Packaging

### Automated coverage

CTest-backed unit tests cover:

- core runtime;
- assets;
- authoring;
- topology;
- persistence;
- UI primitives;
- terrain;
- physics;
- failure paths.

Windows scripts cover repository hygiene, package provenance, packaged startup, external projects, and bounded Terrain process scenarios.

### Package provenance

The packaged Sandbox records the following in `PACKAGE_INFO.txt`:

- commit;
- source state;
- configuration;
- toolchain;
- executable hash.

Packaging is transactional and preserves user data unless reset is explicitly requested.

### Automation and visual evidence

Application-local automation input ownership separates deterministic test pointer/keyboard events from ordinary physical input.

Screenshot captures are application-only evidence; they do not replace human visual QA.

See [building.md](building.md), [package-provenance.md](package-provenance.md), [repository-integrity.md](repository-integrity.md), and [showcase-assets.md](showcase-assets.md).

## Audio

- The renderer-independent runtime provides the supported Audio scope:
  confined resident PCM WAV, Ogg Vorbis, MP3, and FLAC loading, fixed-capacity
  generation-checked voices, Master/Music/SFX/Dialogue/Ambience/UI bus gains,
  listener orientation, distance attenuation and stereo panning, deterministic
  interleaved stereo float-PCM mixing, and resident voice pause/resume/restart/
  seek/gain/pitch controls.
- Voices bind to borrowed production `henka_scene` and `henka_entity` objects.
  The mixer reads the live entity transform each mix operation, rejects stale or
  destroyed entities before they contribute, and exposes bounded diagnostics.
- Scene Documents v6 persist the authored Audio listener, emitter values, and
  resident/streamed storage choice; v1-v3 load with safe listener defaults,
  while v4 loads its authored listener and defaults emitter storage to resident.
  Legacy documents are not rewritten on load. Play applies the authored listener
  to the Audio system before emitter creation, while the graphical Sandbox may
  update it from the live production camera.
- The first integration coverage uses a real scene entity created through the
  public scene API and a WAV loaded through the confined file path. The packaged
  `--audio-smoke-test` additionally proves repository-owned WAV, Ogg Vorbis,
  MP3, and FLAC fixtures through both resident and metadata-first streamed
  asset-manager paths, live emitters, the mixer, and the SDL output boundary.
- The decoder boundary validates resident and metadata-first streamed Ogg
  Vorbis, MP3, and FLAC sources through the private decoder boundary. The
  public runtime has bounded caller-owned frame reads and stream-backed
  voices/emitters. The asset manager caches streams by canonical path alongside
  resident clips; in-place stream reload is validated for PCM WAV, Ogg Vorbis,
  MP3, and FLAC sources.
- Authored emitter configuration, authored listener, and resident/streamed
  storage mode persist in v6 Scene Documents. The Sandbox Play session can
  instantiate those emitters through the normal Game Authoring coordinator. The
  graphical Sandbox owns a client-only, caller-pumped SDL3 playback boundary
  with bounded queue/pump budgets, device diagnostics, event-driven recovery,
  authored-listener application, production-camera listener mapping,
  transactional stream recovery after device loss, and manager-owned resident
  supported-format reload that preserves borrowed clip identity.
- The shared Script Host exposes typed `Audio.Play`, `Audio.Stop`,
  `Audio.Restart`, `Audio.Pause`, `Audio.Resume`, `Audio.IsPlaying`,
  `Audio.SetGain`, `Audio.SetPitch`, `Audio.SetLooping`, `Audio.SetSpatial`,
  `Audio.SetBus`, and `Audio.Seek` bindings to both Lua and HenkaScript in Play.
  They resolve the persisted object-to-emitter mapping and fail closed for
  missing or stale action targets. Long-form packaged content coverage, effects,
  and broader spatial/occlusion support remain in progress. The current runtime
  core and output boundary are single-owner for voice commands and mixing;
  device-thread synchronization remains outside this scope.

## Known Gaps

Henka remains early-stage. The native workspace is not a complete production editor or project-authoring suite.

Major open areas include:

- dedicated 2D workflow;
- advanced Audio effects, broader spatial/occlusion support, and long-form
  packaged-content coverage;
- end-user project script behaviors;
- full Character Controller movement;
- advanced physics;
- broader renderer backends;
- mature Game/Play workflows;
- advanced asset authoring;
- hierarchy and prefab authoring;
- animation and rigging;
- texture painting;
- automatic UV unwrap;
- complete scene serialization;
- remaining renderer and Terrain visual validation.

The default Giraffe and Rocket are deterministic imported/generated fixtures and editor-owned derivatives for dogfooding. They are not proof of user-authored production-quality assets. See [showcase-assets.md](showcase-assets.md).
