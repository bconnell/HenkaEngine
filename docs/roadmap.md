# Roadmap

Henka Engine is an early-stage open source C game engine and integrated development workspace. The active work combines runtime and integrity hardening with the already-underway modeling/content-authoring foundation, terrain/world usability, renderer and asset hardening, and the next layers of 2.5D while retaining external-pipeline compatibility.

This roadmap is a direction guide, not a release schedule. Priorities may change as the engine matures, testing finds issues, or core systems need more hardening.

## Current focus

The current work is focused on repairing and hardening runtime, workspace, renderer, platform, asset, physics, persistence, packaging, and external-project paths while strengthening the integrated authoring foundation already present in the Sandbox.

Current priorities include:

1. Stable engine startup and shutdown.
2. Clear platform, renderer, input, scene, camera, and authoring boundaries.
3. Reliable object/component selection and transform behavior.
4. Transactional modeling, UV, material, persistence, and undo/redo paths.
5. Terrain editing, streaming, collision, and visual validation.
6. Asset loading and material ownership with explicit failure behavior.
7. A packaged sandbox that can be tested without private setup.
8. Documentation that stays aligned with what the engine actually does.
9. Test coverage for core behavior that should not depend on manual QA.

## Near-term priorities

The next development work remains bounded repair and verification.

1. Finish asset cache ownership, identity, retry, metadata, and failure-output contracts.
2. Continue recursive audits across rendering, platform, physics, persistence, scene, workspace, packaging, and external-project paths.
3. Keep viewport interaction helpers aligned with the real sandbox behavior and complete remaining manual transform QA.
4. Keep local and GitHub validation deterministic across build, tests, package provenance, packaged startup, repository integrity, and external-project checks.
5. Keep the README, architecture, roadmap, runtime help, and repository description aligned with the implemented product.
6. Preserve stable identities, transactional editing boundaries, versionable data, and external-tool compatibility needed by 2.5D and later modeling.
7. Shape the existing Action API toward a versioned semantic agent surface, while deferring any MCP/WebMCP-compatible bridge until stable Scene Document identities, explicit capability discovery, permission boundaries, dry-run behavior, auditability, and structured failure contracts are dependable.

### Sequenced composition foundations

After the current Audio campaign reaches a coherent clean boundary and the
Character Controller work reaches its appropriate maturity target, the next
two major foundational projects are planned in this order:

1. **Scene Hierarchy / Parenting Maturity** — establish an authoritative,
   stable-ID parent/child scene model with deterministic traversal, cycle and
   stale-parent rejection, correct local/world transform propagation,
   explicit keep-world and keep-local reparenting, safe destruction,
   persistence, undo/redo, editor/runtime agreement, and shared participation
   by rendering, physics, Audio, cameras, scripting, and Play sessions.
2. **Prefabs / Reusable Scene Objects** — build reusable authored objects and
   hierarchies on that scene foundation, with stable prefab identity and
   revision data, real scene instantiation, explicit inherited-versus-
   overridden values, persistence, duplication, unpacking, and source-change
   behavior.

Both projects must use ordinary production scene objects and remain bounded,
transactional, fail-closed, and package/external-project verifiable. Nested
prefabs, variants, broader gameplay integration, and other advanced extensions
remain staged work until their contracts are implemented and documented.

## Workspace and tools

Henka is moving toward a practical developer workspace, but this should happen in layers.

Current workspace foundations include:

1. Docked panels with a dedicated Scene View.
2. Native detached tool windows with matching production-panel content, close-to-redock recovery, bounded virtual-screen placement persistence, and title-bar drag-back recognition.
3. Resizable occupied dock regions, validated split topology, tab grouping/reordering, bounded layout history, named layout slots, and reset-layout recovery.
4. Scrollable panel bodies with bounded wheel/touchpad ownership, persistent collapsible property groups and scroll offsets, fixed panel headers, integrated scrollbar behavior, and bounded presentation-only Object Details group ordering.
5. Visible workspace and viewport interaction diagnostics.
6. A multi-window platform foundation with a separate native test panel for render and event-routing validation.

Current runtime foundations also include rigid-body physics v1: fixed-step worlds, static/dynamic/kinematic bodies, sphere/AABB/plane collision, impulse response, friction, restitution, trigger events, raycasts, opt-in sandbox QA controls, and viewport selection highlighting for the selected real scene object.

Viewport/editor tooling is a Foundation with active usability work: Scene View,
Compass navigation, docked and detached panels, layout persistence, bounded
interaction diagnostics, and early authoring surfaces are implemented. Native
desktop feel, broader hierarchy/project workflows, numeric transform editing,
and complete manual interaction QA remain open.

Planned workspace improvements include:

1. Complete the remaining native desktop feel and manual QA for detached controls and title-bar drag-back redocking.
2. Add an in-window controls editor for the existing local keybinding profiles.
3. A detachable Scene View after multi-window rendering and viewport input are dependable.
4. Scene Hierarchy / Parenting Maturity as the sequenced next composition
   project after the current Audio and Character Controller priorities.
5. Prefabs / Reusable Scene Objects on top of the mature hierarchy.
6. Numeric transform editing.
7. Extend undo and redo beyond the current bounded workspace-layout,
   authoring, and material histories to more basic scene operations.
8. Extend the current settings/save-slot and HAMS authoring persistence into a
   complete scene/project save and load workflow.

These features should appear only when they are wired into the engine, tested, documented, and useful.

## Agent and automation interface

Henka already has the beginning of the right architectural seam for agent-driven development: the local Action API lets tools and tests request validated engine operations and receive structured results without pretending to be a human operating the editor with a mouse.

The next goal is not to expose every editor control immediately. The agent surface should grow only as the underlying engine contracts become stable enough to make machine-driven operations deterministic, inspectable, and safe.

### Readiness gates

An MCP-compatible agent adapter becomes appropriate once the following foundations are dependable:

1. Action targets use persistent Scene Document identities rather than relying on transient runtime entity handles.
2. Action schemas and capability discovery are versioned so external agents can determine what the running Henka build actually supports.
3. Authoring, scene, asset, play-session, and inspection operations have explicit authority and transaction boundaries instead of bypassing engine ownership.
4. Requests support bounded validation, dry-run behavior where meaningful, structured failures, and enough result state to prove what changed.
5. Local permission and user-approval boundaries prevent arbitrary code execution or silent access to capabilities outside the exposed tool surface.
6. Agent actions produce auditable diagnostics suitable for the same executable validation harness used for normal development and regression testing.
7. Visual evidence remains a separate authority for appearance, readability, anatomy, composition, material response, and other properties that structured state alone cannot prove.

### Planned integration

Once those gates are met, Henka should expose the Action API through a thin standards-oriented adapter rather than building a second automation system. The preferred direction is an MCP-compatible semantic tool surface for desktop development agents, with WebMCP-compatible exposure considered where Henka later has an appropriate browser or web-hosted surface.

The adapter should remain transport-level infrastructure. Engine authority, validation, transactions, identity, undo boundaries, diagnostics, and test evidence stay inside Henka so changing agent protocols does not change the engine's correctness model.

Because MCP and WebMCP are evolving ecosystems, Henka should track their stable capability-discovery, schema, permission, and transport conventions and implement the adapter when doing so reduces custom automation instead of forcing premature compatibility work.

Foreground mouse and keyboard automation remains useful for testing the human interface itself. It should not be the primary control path for an agent performing ordinary scene, authoring, inspection, or validation work when an equivalent semantic engine action exists.

## Game authoring foundation

The Sandbox now has a bounded Game Authoring V1 slice. It is an integrated
foundation that shares the Scene Document, runtime scene, physics, workspace,
and persistence boundaries; it is not a claim that the complete game-editor
roadmap is finished.

### Available

1. Persistent Scene Document object IDs mapped through a bounded adapter to
   generation-checked runtime entities.
2. Authored Physics and Interaction values in Object Details for registered
   scene objects.
3. Confined, checksummed `.hscene` Save Scene and Reload Scene operations with
   candidate validation and failure retention.
4. A dedicated Play session with Start, Pause, Resume, Step, Stop, an isolated
   runtime scene, and an active-session mutation/save barrier.

### In Progress

1. Broader registration and materialization of imported and externally-authored
   objects.
2. More complete source, renderer/material, hierarchy, and scene/project
   serialization while retaining runtime-resource ownership boundaries.
3. Numeric inspector editing, richer gameplay components, and expanded manual
   packaged validation.

### Planned

1. Hierarchy/scene-graph authoring, full Inspector behavior authoring, and a
   complete project serializer. The bounded HenkaScript/Lua lifecycle
   adapters, callable VM, language-neutral host schema, typed host dispatch,
   isolated Play Scene Document binding, bounded behavior state, and explicit
   mixed-language event routing are foundation work already available; editor
   authoring and broader project scripting workflows remain open. Bounded
   Inspector creation, transactional attachment of Lua/HenkaScript behavior
   templates, and the bounded compiler-backed source editor with Save/Revert are
    already available. The Play-session seam now supports candidate-first
    transactional behavior reload and the source-panel Edit, Save, Revert, and
    Reload actions and bounded compiler/backend reload diagnostics are
    available; debugger presentation remains open.
2. Broader gameplay systems, input mapping, controllers, animation, and
   production game-debugging workflows.

## Asset and material workflow

The asset pipeline is still early, but it already has a manager-owned metadata,
dependency, import, fallback, and material-instance foundation.

### Foundation

1. Asset metadata and texture dependencies are manager-owned and inspectable.
2. Bounded glTF/GLB scene/PBR import and bounded OBJ import are available within
   their documented interchange subsets.
3. Missing-asset fallback behavior is explicit and validated.
4. Texture dependencies, scalar/vector values, alpha mode, and semantic PBR
   material-instance overrides can be inspected and edited transactionally.
5. External templates have bounded asset-root and consumer-validation paths.

### Current development

1. Strengthen asset cache ownership, identity, retry, metadata, and
   failure-output contracts.
2. Expand texture/material assignment and material-editing usability while
   preserving manager ownership and transactional updates.
3. Improve external project configuration and asset-root guidance across the
   validated templates.

### Future work

1. Broader model import coverage beyond the current bounded glTF/GLB and OBJ
   subsets.
2. Dedicated user-authored material-file authority, text-entry import,
   drag/drop, and dependency-graph tooling.
3. Shader selection and procedural shader planning with safe parameter
   handling.

Procedural shader work should come after the material system is stable enough to support it cleanly.

## 2D and 2.5D direction

Henka is planned to support 2D and 2.5D as first-class workflows, not as afterthoughts.

Planned 2D work includes:

1. A dedicated 2D renderer path.
2. Sprites.
3. Texture regions.
4. Layers.
5. A 2D camera.
6. A focused 2D sample.

The first 2.5D camera foundation includes:

1. Perspective 3D, side, top-down, and isometric camera presets.
2. Stable exact-vertical top-down camera basis handling.
3. Orthographic zoom and frame-selected sizing.
4. Sandbox controls and local persistence for the selected camera preset.

Next 2.5D work includes:

1. Sprite-facing quad and texture-sampling foundations.
2. Transparent and cutout material render states.
3. Sprite and texture-region data.
4. Layered depth and deterministic sorting (the current renderer provides bounded transparent sorting; sprite/layer authoring remains future work).
5. Parallax.
6. Movement-plane and physics-axis constraints.
7. Tools that make 2D-style layout in 3D space easier to manage.

## Integrated modeling and content authoring

Integrated authoring is already underway in parallel with the 2D/2.5D roadmap.
The camera-side 2.5D foundation and the modeling foundation are separate
tracks that share renderer, asset, persistence, and workspace boundaries.

### Implemented Foundation

1. Editable authoring data is separated from evaluated runtime meshes, render
   buffers, collision data, and material-instance state.
2. Objects and mesh elements use stable identities with connectivity, boundary,
   material-region, UV, smoothing, and hard-edge metadata.
3. Transactional authoring operations have validation, commit/rollback
   behavior, bounded undo/redo, and evaluated mesh replacement.
4. Object, Vertex, Edge, and Face workflows include component selection,
   connected selection, bounded edge-loop/ring selection, soft movement, and
   axis-constrained movement.
5. Vertex operations include Merge Center, Merge Active, Merge by Distance,
   Connect Vertices, Dissolve Vertex, Delete Vertex, Vertex Bevel, and bounded
   Vertex Extrude for connected open boundary vertex fans, including the
   one-face corner case. Closed, disconnected, loose-edge, and incompatible-
   normal fan cases remain fail-closed.
6. Non-destructive topology analysis and explicit transactional safe repair are
   available. Repair is bounded and deterministic: it can remove enabled
   isolated vertices, exact metadata-preserving duplicate faces, and
   degenerate faces, while rejecting unsafe winding, UV, material, smoothing,
   or non-manifold changes.
7. Face winding flip, face extrude, inset, planar bevel rings, face subdivision,
   selected-face deletion, planar UV projection, island transforms, packing, seam detection,
   Make Editable, HAMS persistence, material promotion, and supported PBR
   material-instance editing are available in the bounded workflow. HAMS also
   preserves explicit loose vertices and standalone wire edges with stable IDs
   and bounded reusable storage. The topology overlay presents those source
   vertices and wire edges for inspection. The shared modeling-operator session
   and Authoring panel also provide bounded explicit-axis extrusion for one
   selected loose vertex or standalone edge with numeric Preview, Cancel, and
   transactional Apply. Homogeneous wire-only and isolated-vertex-only sources
   also have bounded renderer-backed line and point evaluation. Mixed
   surface/wire/point sources now use bounded multi-primitive renderer ownership
   without dropping valid loose components.
8. glTF/GLB import and external modeling-pipeline compatibility remain part of
   the implemented boundary, with explicit limitations.

### Current Development

1. Continue expanding edge topology coverage and the surrounding authoring UX.
   Bounded single-edge dissolve for compatible interior edges, bounded
   single-edge delete of an incident face set, standalone boundary-edge bevel,
   bounded multi-edge boundary bevel for pairwise vertex-disjoint edges on
   distinct faces, bounded same-face boundary bevel with shared-endpoint corner
   caps, and compatible interior-edge bevel for an isolated two-quad patch are
   now available; the editor now applies a validated user-entered
   factor for one Loop Cut across a compatible open quad strip or closed ring
   using the reusable traversal foundation, with Preview/Refresh and explicit
   Apply/Cancel publication. Broader interior-edge cases and general loop-cut
   networks remain in progress. Edge mode also provides a signed-factor Edge Slide for
   one compatible open edge-loop or closed edge-cycle selection through the
   shared modeling operator session, including numeric preview, cancel, and
   transactional Apply. Broader edge-loop domains remain planned.
2. Extend the available loose-component creation controls into broader editing
   around the bounded loose extrusion session. The bounded surface-connected
   extrusion operation for one open boundary edge is now available through the
   shared modeling session and Authoring panel, with transactional preview,
   cancel, and Apply. The homogeneous line/point evaluation and bounded
   triangle/wire/point renderer ownership foundations are available; broader
   selection, editing, and surface-connected extrusion workflows remain in
   progress.
3. Strengthen transactional modeling, UV, material, persistence, and undo/redo
   paths while keeping failure behavior fail-closed.
4. Improve editor integration, authoring source/project workflows, and the
   usability of the existing operations.
5. Keep showcase fixture work separate from claims of user-authored,
   production-quality anatomy or mechanical topology.

### Future Work

1. Broader non-manifold or incompatible-normal vertex-fan handling, generalized
   surface-connected Edge Extrude, broader edge and vertex topology operations,
   weld/split/bridge workflows, general loop-cut networks, and broader source
   export. Connected open boundary fan extrusion, bounded loose-vertex wire
   extrusion, and bounded loose-edge quad extrusion are already available in
   the core workflow, but none constitutes complete general Vertex/Edge Extrude
   authoring.
   Closed-ring Loop Cut is also available in the bounded factor-controlled
   workflow; broader loop-cut network authoring remains future work.
2. Automatic multi-island UV unwrap, texture painting, rigging, skinning, and
   animation authoring.
3. Complete scene/project serialization and wider adapter-based interchange
   beyond the current bounded paths.

2D and 2.5D remain first-class roadmap work; their future sprite, layer,
parallax, animation, and movement-constraint systems do not defer or replace
the integrated authoring work already present.

## Persistence and undo/redo

### Foundation

Settings and save slots use confined paths, bounded identifiers, validated
records, same-directory temporary files, flush/close-before-replace behavior,
and failure retention of prior in-memory state. Versioned HAMS authoring
sources, bounded authoring project save/reload, the checksummed V1 `.hscene`
Scene Document path, material history, and workspace layout history are also
available.

### Current Development

The next persistence work is to extend these bounded foundations into broader
scene/project data, imported-object and component coverage, more complete
authoring history, and durable cross-workflow versioning without weakening
transactional failure behavior.

### Future Work

A complete project-wide authoring serializer, hierarchy/project manifests, and
remote or network-backed save policy remain future work beyond the current HAMS
and V1 `.hscene` foundations.

## Scripting and behavior authoring

### Implemented Foundation

1. A versioned, language-neutral Script Host schema provides bounded typed
   gameplay API identities and diagnostics.
2. HenkaScript has a bounded lexer, parser, type checker, callable bytecode
   path, and allocation-free budgeted VM.
3. Lua 5.4.8 and HenkaScript have bounded lifecycle adapters for `OnCreate`,
   `OnStart`, `OnUpdate`, `OnFixedUpdate`, targeted interaction/contact signals,
   `OnDestroy`, and `OnStop`, with fail-closed budgets and deterministic
   missing-handler behavior.
4. The generation-checked behavior runtime owns lifecycle state, borrowed
   callbacks, synchronous non-reentrant dispatch, targeted authored-entity
   signal delivery, failure accounting, and bounded batch reports.
5. Scene Document behavior attachments persist stable IDs, enabled state,
   language identity, and confined project-relative `.lua`/`.hks` paths. The
   bounded asset loader validates those paths and source limits, owns each
   selected backend, and exposes mixed-language runtime descriptors.

### Current Development

1. Generalize runtime-entity and physics-body identity mapping around the Scene
   Document behavior runtime while keeping isolated Play-scene dispatch unable
   to mutate authoring state; the current bounded Play mapping is available
   foundation work.
2. Extend the available typed, non-reentrant Script Host dispatcher beyond the
   current Entity/Transform/Physics/Event slice and resolve the required API
   bindings at load time.
3. Extend behavior-state persistence and authoring ergonomics: the bounded
   state store and explicit sidecar save/load seam are available. Inspector
   template authoring, transactional attachment, and a bounded editable,
   compiler-backed source panel with compiler-derived HenkaScript spans,
    diagnostics, and Save/Revert are available; candidate-first transactional
    behavior reload is available at the Play-session seam and the source-panel
    Reload action is available, while broader Inspector authoring and bounded
    reload diagnostics are available through the source diagnostic seam, while
    debugger presentation remains in progress.
4. Strengthen mixed-language event delivery and lifecycle diagnostics at the
   scene boundary; bounded queueing, Lua/HenkaScript emission, and `OnEvent`
   routing are available foundation work, while richer subscriptions and
   tooling remain open.

### Future Work

1. End-user project scripting workflows, debugger/diagnostic presentation,
   broader host APIs, and a stable script package/versioning policy.
2. Sandboxed script data schemas, deterministic replay integration, and wider
   editor tooling once the scene binding contract is stable.

## Longer-term systems

Longer-term work may include:

1. Expanded physics features such as joints, controllers, and additional collider types.
2. Normal Audio device lifecycle, listener/editor integration, and broader game
   audio workflows beyond the current resident-WAV/mixer and caller-pumped
   SDL3 output foundation.
3. Additional scripting languages or extension support beyond the bounded V1
   Lua and HenkaScript foundations.
4. Additional renderer backends.
5. Release packaging.
6. Versioned builds.
7. Checksums and release verification.
8. Provenance for release artifacts.

These systems will require careful design because they affect safety, project structure, and long-term maintenance.
