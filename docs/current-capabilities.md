# Current Capabilities

This is the detailed, code-backed inventory for the current Henka Engine
checkout. It records implemented foundations and their boundaries without
turning the public README into an implementation ledger.

## Core / Platform

- Henka is written in C17 and configured with CMake. The graphical `henka`
  library, renderer-independent `henka_runtime` library, `henka_sandbox3d`
  sample, dedicated-server host, and `henka_tests` target are built from the
  repository.
- The validated development and packaging path targets 64-bit Windows with
  MSVC and PowerShell-compatible scripts. SDL3 is behind the platform
  boundary; other operating systems are not currently claimed as supported.
- Public runtime contracts use bounded checked growth, finite-value checks,
  generation-checked 64-bit scene identities, explicit ownership, and
  transactional replacement where a failed operation must retain the prior
  state.
- Detailed boundaries live in [architecture.md](architecture.md),
  [runtime-foundations.md](runtime-foundations.md), and
  [platform-support.md](platform-support.md).

## Renderer

- The sandbox exposes Wireframe, Solid, Material Preview, and Rendered Scene
  View policies through the OpenGL renderer path.
- Rendered mode consumes imported PBR materials, scene lighting, HDR targets,
  bounded environment/IBL fallbacks, directional and bounded local-light
  shadows, fog, bloom, tone mapping, AO, and a temporal reconstruction
  foundation. These paths remain subject to broader manual visual validation.
- The renderer boundary is isolated from renderer-independent runtime and
  authoring data. See [architecture.md](architecture.md) and
  [runtime-foundations.md](runtime-foundations.md).

## Assets / Materials

- Bounded glTF/GLB import covers scene roots, node transforms, meshes, cameras,
  punctual lights, and shared PBR material dependencies. OBJ loading supports
  bounded local files, finite positions, optional UVs/normals, positive and
  negative indices, and basic fan triangulation.
- Asset metadata and texture dependencies are manager-owned. Missing textures
  and supported OBJ failures use visible, validated fallbacks; hard failures
  propagate when a safe fallback is not valid.
- Material instances support validated scalar/vector, alpha-mode, and
  semantic-texture overrides, dependency inspection, revision refresh, and
  transactional reimport. Dedicated user-authored material-file authority,
  text-entry import, drag/drop, and dependency-graph tooling are not yet
  present.
- See [model-loading.md](model-loading.md) for import limits and failure
  behavior.

## Scene / Camera

- Scene entities, logical selection ownership, cameras, input actions, screen
  rays, framing, transform actions, and shared overlay gizmos are available
  through public or sandbox-facing boundaries.
- Camera presets include Perspective 3D, Side 2.5D, Top-down 2.5D, and
  Isometric 2.5D with orthographic zoom and deterministic vertical-view bases.
- The Scene View Compass is a dynamic, camera-driven viewport instrument with
  axis snapping, orbit dragging, projection switching, persisted placement
  and scale preferences, and an application-local automation input path for
  deterministic graphical validation.

## Editor Workspace

- The native sandbox workspace has Scene View, utility panels, docked and
  detached production-panel surfaces, validated split topology, tabs,
  layout presets, named layout slots, layout history, reset-layout recovery,
  scrollable panel bodies, and focus-loss/Escape cancellation.
- Scene Objects exposes logical scene owners while retaining render-child
  identities for materials and component editing. Add Cube, Duplicate, Delete,
  Make Editable, visibility, locks, transforms, and bounded material
  inspection are available; durable scene rename and complete project
  serialization remain unfinished.
- UI controls use bounded IDs, matched frame construction, release-confirm
  behavior, and transactional composite drawing. Native desktop feel and
  complete manual interaction QA remain open work.
- See [ui.md](ui.md), [editor-controls.md](editor-controls.md), and
  [help/sandbox3d.md](help/sandbox3d.md).

## Game Authoring Foundation

- The Sandbox has a bounded Game Authoring V1 path for its registered scene
  objects. Persistent Scene Document IDs are mapped to generation-checked
  runtime entities through a dedicated adapter; runtime handles are not
  serialized.
- Object Details exposes authored Physics and Interaction values for bound
  objects, including primitive body/shape and trigger choices, interaction
  enablement, and prompt inspection. Edits validate before the working scene
  is updated.
- Save Scene and Reload Scene use the confined, checksummed `.hscene` format.
  Candidate loads, rebinding, and runtime presentation updates fail closed and
  retain the prior authoring state on failure.
- Play, Pause, Resume, Step, and Stop are owned by a dedicated bounded session.
  Game Authoring creates an independent runtime scene clone with borrowed
  renderer resources and generation-preserving entity handles. Runtime bodies
  and simulation transforms remain outside persistence; Stop destroys the
  runtime scene, leaving the authored scene unchanged. Active Play rejects
  authoring changes and scene save/reload.
- This is a foundation, not a complete game editor. Hierarchy authoring,
  broader imported-object registration, complete source/material/project
  serialization, and production gameplay workflows remain open. The shared
  scripting API/host schema and bounded HenkaScript lexer/parser/type-checking
  foundation are available through [scripting-foundation.md](scripting-foundation.md),
   including bounded HenkaScript callable bytecode/VM execution, bounded Lua
   lifecycle execution, fixed-update/destruction and targeted contact
   lifecycle adapters, and a language-neutral
   generation-checked behavior lifecycle runtime. A typed, bounded Script Host
   dispatcher and FIFO event queue are available, and the isolated Play path
   maps persistent object IDs to runtime entities for the current Entity,
   Transform, Physics, and Events slice. A bounded behavior-state store with
   explicit sidecar save/load and mixed-language `OnEvent` routing is also
   available. Play uses a bounded clone of the Edit baseline, so runtime state
   cannot leak back into authoring on Stop; Save Play State and Load Play State
   are explicit operations. Complete host API coverage, full Inspector authoring, and
    debugger tooling remain unfinished. The Inspector can create confined
    Lua or HenkaScript behavior templates and attach them transactionally, and
    provides a bounded editable source panel whose HenkaScript spans, colors,
    and insertion indentation derive from compiler tokenization/token APIs;
    the editor contains no HenkaScript keyword or grammar table. Lua uses its
    backend for validation while preserving persisted source formatting.
    Source bytes, indentation, diagnostics, Save, and Revert are covered by
    the bounded editor model. The Play-session seam
    can transactionally reload a persisted behavior backend while preserving
    the generation-checked slot and active lifecycle state. The source panel
    exposes Edit, Save, Revert, and Reload actions; Reload uses the same
    coordinator seam in Play and reloads the persisted source outside Play.
    Debugger tooling remains unfinished; candidate reload failures preserve
    bounded source diagnostics through the editor seam.
   Persisted `.lua` and `.hks` attachments can now be loaded through a bounded,
   confined-path asset loader and assembled into a mixed-language behavior
   runtime by persistent Scene Document object identity. The Sandbox Play
   session owns that runtime for isolated Create/Start/Update/OnEvent/Stop
   dispatch; runtime-entity identity mapping is available in the isolated Play
   path; broader runtime/resource mapping, full Inspector authoring, and project
   scripting workflows remain unfinished.

## Modeling / Content Authoring

- Object, Vertex, Edge, and Face workflows are integrated into the sandbox.
  Component selection, connected selection, bounded edge-loop selection,
  one-ring soft movement, axis-constrained movement, and visible topology
  feedback are available.
- The authoring mesh API has stable vertex/edge/face identities, connectivity
  and boundary queries, material regions, per-corner UV metadata, smoothing
  and hard-edge intent, fail-closed polygon validation, deterministic
  caller-owned triangulation, and bounded shared undo/redo.
- Topology analysis is available as a non-destructive, deterministic report
  covering component structure, boundaries, manifold state, winding, seams,
  hard edges, degeneracy, duplicate faces, coincident vertices, valence, and
  face-shape metrics. Explicit repair can transactionally remove only enabled
  safe issues: isolated vertices, exact duplicate faces with matching winding,
  UVs, material, and smoothing, and degenerate faces. Unsafe duplicate groups
  are rejected; vertex welding and winding rewrites are not implicit repairs.
- Published Vertex topology operations include Merge Center, Merge Active,
  Merge by Distance, Connect Vertices, Dissolve Vertex, Delete Vertex, and
  Vertex Bevel. These operations are transactional and exposed through the
  integrated Vertex selection workflow.
- Transactional operations also include plane/box creation, duplicate, face
  extrude, inset, planar bevel rings, face subdivision, selected-face deletion,
  planar UV projection, island transforms, packing, and seam detection. HAMS v4
  writes portable little-endian data through unique same-directory temporary
  files and retains reads for checked-in v2/v3 legacy sources.
- Imported nontrivial objects can take the Make Editable path. Native source
  persistence, evaluated mesh replacement, material promotion, supported PBR
  overrides, procedural detail textures, and material undo/redo are present
  in the bounded dogfood workflow.
- This is an integrated authoring foundation, not a production modeling suite.
  Vertex Extrude remains unavailable. Broader vertex tooling, automatic UV
  unwrap, texture painting, rigging, skinning, animation authoring, broader
  source export, and production-quality showcase anatomy/mechanical topology
  remain in progress.
- Edge authoring is a separate capability boundary: bounded edge-loop/ring
  selection is available, but Edge Delete and broader edge topology operations
  are not claimed as complete and remain in progress.
- See [authoring-mesh.md](authoring-mesh.md),
  [runtime-foundations.md](runtime-foundations.md), and
  [showcase-assets.md](showcase-assets.md).

## Terrain / World

- Terrain has a deterministic four-layer material contract, bounded region
  persistence, streaming observers, resident CPU/physics/render owners,
  height and paint edits, fixed-budget edit history, collision patches,
  neighbor-aware normals, bounded LOD transitions, and transactional mesh
  replacement.
- The sandbox exposes terrain brush interaction, material-layer inspection,
  save/reload, stream diagnostics, and Rendered presentation. Headless and
  dedicated-server paths exercise authority, snapshot recovery, and restart
  persistence.
- Automated topology and all-edge correspondence checks exist. Four-way
  corner visual approval, broader-world streaming, and background
  regeneration remain unfinished.
- See [terrain.md](terrain.md) for the authoritative terrain contract.

## Physics

- Physics v1 provides fixed-step static, dynamic, and kinematic bodies with
  sphere, axis-aligned box, and plane colliders, contacts, impulses, friction,
  restitution, trigger events, and raycasts.
- Sandbox Physics QA and selected-body activation are available. Numeric,
  allocation, and body-replacement failures preserve prior simulation state
  where the contract requires retry safety.
- Mesh collision, constraints, character controllers, and advanced simulation
  are not currently implemented.
- See [physics.md](physics.md) and [help/sandbox3d.md](help/sandbox3d.md).

## 2D / 2.5D

- Current 2.5D foundations are camera presets, orthographic framing/zoom,
  stable vertical orientation, and sandbox persistence.
- A dedicated 2D renderer, sprites, texture regions, layered depth, parallax,
  sprite animation, and movement-plane/physics-axis authoring are future work.
- The project direction remains first-class 2D and 2.5D workflows; the current
  camera foundation should not be read as a complete 2D toolchain.

## Networking / Dedicated Server

- A renderer-free dedicated-server host and `henka_runtime` consumer boundary
  are available. The bounded path covers fixed-tick simulation, loopback
  connectivity, Terrain authority, snapshot recovery, edit persistence, and
  graceful shutdown.
- Relevance-filtered reconnect/late-join selection has bounded coverage.
  Authentication, broader residency orchestration, production-scale capacity,
  and multiplayer soak remain unfinished.
- See [dedicated-server.md](dedicated-server.md) and
  [external-game-projects.md](external-game-projects.md).

## Persistence

- Settings and save slots use confined paths, bounded identifiers, complete
  record validation, same-directory temporary files, flush/close-before-replace
  behavior, and failure retention of the prior in-memory state.
- Authoring sources use the versioned HAMS format, and the Sandbox Game
  Authoring V1 path uses a bounded, checksummed v2 `.hscene` Scene Document
  for registered objects and bounded Lua/HenkaScript behavior attachments.
  v1 documents load with behavior defaults for migration. A complete
  scene/project serializer and remote/network-backed save policy are not yet
  available.
- See [persistence.md](persistence.md) and [authoring-mesh.md](authoring-mesh.md).

## External Project Support

- Real games are intended to live in separate repositories. The external game
  template consumes the public authoring and Terrain boundaries without
  depending on Sandbox source; the external server template links only the
  renderer-independent runtime boundary.
- Windows validation covers configure/build/startup and bounded public-API
  consumer smokes. A complete external scene/project editor and serializer is
  still future work.
- See [external-game-projects.md](external-game-projects.md).

## Testing / Packaging

- CTest-backed unit tests cover core runtime, assets, authoring, topology,
  persistence, UI primitives, terrain, physics, and failure paths. Windows
  scripts cover repository hygiene, package provenance, packaged startup,
  external projects, and bounded Terrain process scenarios.
- The packaged sandbox records commit, source state, configuration, toolchain,
  and executable hash in `PACKAGE_INFO.txt`; packaging is transactional and
  preserves user data unless reset is explicitly requested.
- Application-local automation input ownership separates deterministic test
  pointer/keyboard events from ordinary physical input. Screenshot captures
  are application-only evidence; they do not replace human visual QA.
- See [building.md](building.md), [package-provenance.md](package-provenance.md),
  [repository-integrity.md](repository-integrity.md), and
  [showcase-assets.md](showcase-assets.md).

## Known Gaps

- Henka remains early-stage. The native workspace is not a complete production
  editor or project-authoring suite.
- 2D workflow, audio, end-user project script behaviors, character controllers,
  advanced physics, broader renderer backends, and mature Game/Play workflows
  remain future work beyond the current bounded authoring/play foundation.
- Advanced asset authoring, hierarchy authoring, animation/rigging, texture
  painting, automatic UV unwrap, complete scene serialization, and some
  renderer/terrain visual validation remain open.
- The default Giraffe and Rocket are deterministic imported/generated fixtures
  and editor-owned derivatives for dogfooding; they are not proof of
  user-authored production-quality assets. See [showcase-assets.md](showcase-assets.md).
