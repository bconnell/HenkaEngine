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
4. A clearer scene hierarchy.
5. Numeric transform editing.
6. Extend undo and redo beyond the current bounded workspace-layout,
   authoring, and material histories to more basic scene operations.
7. Extend the current settings/save-slot and HAMS authoring persistence into a
   complete scene/project save and load workflow.

These features should appear only when they are wired into the engine, tested, documented, and useful.

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
   Vertex Extrude for one unambiguous boundary corner of a single face.
6. Non-destructive topology analysis and explicit transactional safe repair are
   available. Repair is bounded and deterministic: it can remove enabled
   isolated vertices, exact metadata-preserving duplicate faces, and
   degenerate faces, while rejecting unsafe winding, UV, material, smoothing,
   or non-manifold changes.
7. Face extrude, inset, planar bevel rings, face subdivision, selected-face
   deletion, planar UV projection, island transforms, packing, seam detection,
   Make Editable, HAMS persistence, material promotion, and supported PBR
   material-instance editing are available in the bounded workflow.
8. glTF/GLB import and external modeling-pipeline compatibility remain part of
   the implemented boundary, with explicit limitations.

### Current Development

1. Continue expanding edge topology coverage and the surrounding authoring UX.
   Bounded single-edge dissolve for compatible interior edges and bounded
   single-edge delete of an incident face set are now available; broader edge
   topology operations are not yet claimed as complete.
2. Strengthen transactional modeling, UV, material, persistence, and undo/redo
   paths while keeping failure behavior fail-closed.
3. Improve editor integration, authoring source/project workflows, and the
   usability of the existing operations.
4. Keep showcase fixture work separate from claims of user-authored,
   production-quality anatomy or mechanical topology.

### Future Work

1. Multi-face vertex-fan extrusion, broader edge and vertex topology
   operations, weld/split/bridge/loop-cut workflows, and broader source export.
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
2. Audio.
3. Additional scripting languages or extension support beyond the bounded V1
   Lua and HenkaScript foundations.
4. Additional renderer backends.
5. Release packaging.
6. Versioned builds.
7. Checksums and release verification.
8. Provenance for release artifacts.

These systems will require careful design because they affect safety, project structure, and long-term maintenance.

## External project workflow

Henka Engine should remain a reusable engine repository. Real games built with Henka should live in separate repositories.

### Foundation

1. Separate game and server templates consume the public runtime boundaries;
   the external server links only the renderer-independent runtime.
2. Separate asset and scene roots, build guidance, packaging guidance, and
   Windows configure/build/startup validation are available.
3. Bounded public-API consumer smokes prove that the templates remain usable
   without depending on Sandbox source.

### Current Development

1. Strengthen starter templates and project configuration.
2. Continue external-project validation and improve asset-root diagnostics.

### Future Work

Complete external scene/project editing and serialization remain future work.

## Sponsorship supported work

Henka Engine is open source, and sponsorship helps support the time needed to continue development.

Funding can help with engine development, sandbox usability, documentation, examples, packaged builds, testing, asset workflow improvements, and future workspace tools.

Sponsorship does not change the license, purchase feature priority, or override the project roadmap. Roadmap decisions remain based on stability, maintainability, scope, and usefulness to the engine.

## Current limitations

Henka is still early. Some systems are intentionally limited while the engine foundation is being built.

Current limitations include:

1. The sandbox is an engine sample and QA target, not a game.
2. The transform gizmo workflow still needs manual desktop QA for visual feel and mouse comfort.
3. Scene saving and loading are not complete authoring workflows yet.
4. The docked and detached workspace is useful for inspection, testing, and early authoring behavior, but it is not yet a full production editor and project-authoring workflow.
5. Workspace movement and sizing require desktop QA for feel. Detached placement, matching detached controls, bounded title-bar drag-back recognition, layout history, named slots, and panel-group persistence are implemented; native desktop feel and detachable Scene View remain manual-QA/open items.
6. The native test panel and detached production-panel surfaces use multi-window rendering and event routing; bounded detached controls and title-bar drag-back recognition are implemented, while detachable Scene View remains open.
7. Asset loading is still limited.
8. The 2D workflow and the sprite, layer, parallax, animation, and movement-constraint parts of 2.5D are not implemented yet; the first 2.5D camera presets are available.
9. Physics v1 is intentionally limited to rigid bodies and primitive colliders; mesh collision, joints, character controllers, and advanced simulation remain future work.
10. Audio, end-user scripting workflows, and release distribution remain later
    milestones beyond the current scripting foundation.
