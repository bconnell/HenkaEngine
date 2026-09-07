# 3D Engine Completion Matrix

This matrix records the current 3D engine boundary across the production
implementation, public APIs, editor paths, persistence, runtime use,
packaging, and executable validation. It is an evidence summary, not a claim
that every future engine feature is complete.

Review baseline: the matrix contents apply to the commit containing this file.
Exact commit identity, build provenance, and hosted Windows CI status are
recorded by the repository's validation systems; this document does not encode
a future `main` SHA or claim that an unpublished run has completed. The
Git-object candidate mechanism, canonical Windows dependency configuration,
repository integrity, public-content hygiene, documentation truth, and the
product-native modeling evidence boundary are published.

## Status rules

- **Available** means the declared supported scope is connected through the
  production path and has executable evidence at that boundary.
- **Foundation** means a real production subsystem or API exists, but one or
  more user-facing, persistence, runtime, package, or cross-subsystem links
  remain incomplete.
- **In Progress** means the supported scope is actively implemented but still
  has material gaps inside that scope.
- **Planned** means the capability has not materially entered the production
  implementation.

The scope column is part of each status. A future backend, advanced mode, or
larger workflow outside that scope does not lower the current status. A gap
inside the declared scope does lower it.

## Current closure blockers

These are the material open items that currently prevent the 3D completion gate
from closing:

- **Modeling and authoring:** the product-native generic workflow is validated
  for the current bounded topology slice, but broader authoring coverage,
  production-quality arbitrary asset authoring, and the remaining packaged
  native Box-add validation contradiction remain open.
- **Editor workspace:** desktop readability, long-label presentation, dense
  control layout, and human visual QA remain unresolved.
- **Camera and interaction:** laptop/touchpad navigation and reliable
  cold-start scene-first framing still require direct validation.
- **Rendering and environment:** the remaining IBL/specular visual defect is
  separate from the completed near-directional shadow repair and remains open.
- **Cross-subsystem completion:** hierarchy, prefabs, broader persistence,
  project/external-project workflows, and complete runtime/package coverage
  remain foundations rather than closed product workflows.

## Current matrix

| Capability | Supported scope | Production authority and evidence | Persistence / runtime / package boundary | Status | Current gap or next boundary |
| --- | --- | --- | --- | --- | --- |
| Core runtime and platform | C17 engine/runtime libraries, Windows 64-bit MSVC development and packaging path | `CMakeLists.txt`; public headers under `engine/include/henka/`; core, scene, asset, and runtime tests | `henka`, `henka_runtime`, Sandbox3D, dedicated server, and `henka_tests` are built and exercised | **Available** | Other desktop and mobile platform targets remain planned. |
| OpenGL renderer | Wire, Solid, Material Preview, and Rendered scene views on the supported Windows OpenGL path; scene materials, lights, shadows, IBL, fog, bloom, AO, tone mapping, and temporal foundation | `engine/src/renderer/`; `renderer.h`; shader/material tests; packaged startup and renderer/IBL validation scripts | Renderer consumes runtime scene/material authority; packaged Sandbox validation covers startup and selected visual paths | **Available (unhardened)** | Visual quality, broader renderer backends, and some advanced rendering paths remain open. |
| Assets and materials | Bounded OBJ and glTF/GLB import; mesh, camera, light, PBR material, texture, asset metadata, material assets, and material instances within documented interchange limits | `engine/src/assets/model_obj.c`, `model_gltf.c`, `assets.c`; `assets.h`, `model.h`; `tests/test_model.c`, `tests/test_assets.c`, `tests/test_material.c` | Imported assets can enter scenes and the native editable path; supported material state persists where the owning format/document supports it | **In Progress** | Reimport/dependency-scale workflows, broader material editing, texture painting, automatic UV unwrap, and complete external-project material workflows remain open. |
| Scene and entity core | Stable scene/entity identity, transforms, visibility, interaction, bounds, camera ownership, and scene environment | `engine/src/scene/scene.c`, `camera.c`; `scene.h`, `camera.h`; scene, camera, asset, and authoring tests | Scene state is consumed by editor, Play, renderer, Audio, physics, and Scene Document paths | **Available (unhardened)** | Broader project serialization, hierarchy maturity, and cold-start/user-input evidence remain open. |
| Product-native default scene | Ordinary startup contains a centered editable ground plane, scene camera, scene-owned sky/environment, and no showcase or diagnostic entities; explicit capture modes load reference fixtures | `sandbox3d_create_default_ground_authoring`; `DEFAULT_SCENE_READY`; `CAPTURE_READY_STARTUP`; fixture-scope and packaged startup checks | Startup state is created through the normal authoring path and is available to save/reload and runtime workflows | **Available within startup scope** | Broader default-scene authoring and project lifecycle coverage remains part of Game Authoring maturity. |
| Camera and viewport navigation | Perspective/orthographic camera math, presets, framing, orbit/pan/dolly, follow, world/screen conversion, Compass presentation, and persisted viewport preferences | `engine/src/scene/camera.c`, `camera_follow.c`; camera headers; camera and Compass tests; Sandbox camera tools | Camera state is used by the editor and packaged Sandbox; Scene Document stores the supported authored camera boundary | **Available (unhardened)** | Touchpad/laptop navigation and broader camera rigs, blending, collision, shake, and cinematic workflows remain open. |
| Editor workspace and UI | Docked workspace, panels, detached-window foundation, layout persistence, Scene View, Object Details, modeling controls, and application-local interaction validation | `engine/src/ui/`; `examples/sandbox3d/editor_layout.c`, `editor_ui_state.c`, `editor_controls.c`; UI/editor tests and Windows interaction scripts | Packaged Sandbox is the user-facing editor boundary; layout state persists through the supported settings path | **In Progress** | Desktop readability, hierarchy/project workflows, dense control presentation, and human visual QA remain active work. |
| Native modeling and content authoring | Product-native primitives, editable meshes, component selection, topology analysis, bounded transactional vertex/edge/face operations, UV/material state, and editor preview/apply/cancel paths | `authoring_mesh.h`, `authoring_modeling.h`, `authoring_topology.h`; mesh/modeling/topology/UV sources; authoring and visible-modeling tests | HAMS v5 persists loose topology; native authoring, evaluated replacement, material promotion, and undo/redo use the shared source/history boundary | **In Progress** | Broader edge sets, automatic UV unwrap, texture painting, rigging, animation authoring, export, and production-quality arbitrary asset authoring remain open. |
| Scene Document and Game Authoring | Registered scene objects, authored transforms/identity/visibility/interaction, bounded renderer/material values, Audio, behavior attachments, Play mapping, and v6 `.hscene` migration | `scene_document.h`, `scene_document.c`; `scene_document_bridge.c`, `game_authoring.c`, `play_session.c`; Game Authoring and Scene Document tests | Save/load applies authored state to live runtime objects with transactional/fail-closed behavior; packaged startup and authoring smoke paths exist | **Foundation** | Complete project/scene serialization, broader component coverage, and end-user Game/Play workflows remain incomplete. |
| Undo/redo and history | Bounded Game Authoring history for hierarchy reparent/unparent and other supported Scene Document object transactions; native mesh, material, terrain, and workspace histories remain subsystem-local | `authoring_mesh.c`, `game_authoring.c`, terrain history, workspace persistence; authoring, hierarchy, material, terrain, and editor tests | Supported operations commit through their owning transactional history boundary; Game Authoring undo/redo restores the authored Scene Document object and live relationship through the same bridge; package-level and cross-owner history is not complete | **Foundation** | Cross-subsystem history, broader editor coverage, and complete project-level history remain open. |
| Scene hierarchy and parenting | Runtime parent/unparent/reparent, keep-local/keep-world transforms, cycle rejection, persisted parent IDs, bounded prefab snapshot hierarchy, parent-first identity-safe Scene Objects projection, explicit Object Details hierarchy controls, and bounded Game Authoring transaction history for hierarchy objects | Scene hierarchy APIs and `scene_document.c`; `examples/sandbox3d/scene_hierarchy_projection.c`, `game_authoring.c`, and `main.c`; hierarchy, projection, and Game Authoring tests | Runtime and Scene Document foundations are connected; the Scene Objects panel and Object Details hierarchy group project and mutate the canonical parent relationship without creating a second hierarchy authority, while Game Authoring history replays supported object transactions through the same bridge | **In Progress** | Cross-subsystem transform propagation, durable project manifests, cross-subsystem/project history, and broader packaged visual proof remain open. |
| Prefabs and reusable scene objects | Bounded in-memory snapshot capture, deterministic source ordering, instantiate, instantiate-under-parent, source-index lookup, destroy, revision, and refresh | `prefab.h`, `engine/src/core/prefab.c`; scene/game-authoring source and tests | Snapshot use is runtime-capable and transactional; persistent prefab assets are not yet the normal editor/project path | **Foundation** | Persistent identities/revisions, overrides, editor authoring, serialization, unpacking, and packaged/external-project workflows remain open. |
| Terrain and world | Four material layers, height/paint edits, collision patches, normals, bounded LOD, streaming observers, resident CPU/physics/render owners, persistence, and server/client authority | `engine/src/terrain/`; terrain public headers; terrain unit, workflow, process, visual, and server/client tests | Save/reload, streaming, headless/server, and Sandbox Rendered paths are exercised; package visual evidence is bounded | **Foundation** | Four-way corner visual approval, broader-world streaming, background regeneration, and larger-world product workflows remain open. |
| Physics | Fixed-step static, dynamic, and kinematic bodies; sphere/box/plane collision, contacts, impulses, friction, restitution, triggers, raycasts, and failure-safe replacement | `engine/src/core/physics.c`; `physics.h`; physics, heightfield, and Sandbox physics tests | Runtime scene/entity transforms and Terrain collision paths are connected; packaged gameplay proof remains bounded | **Foundation** | Mesh collision, constraints, advanced simulation, and broader gameplay integration remain open. |
| Character Controller | Real dynamic upright capsule body, bounded planar input, acceleration/deceleration, jump queueing, wall sliding, walkable-plane grounding, bounded kinematic-platform inheritance, teleport, Scene Document values, and Play linkage | `character_controller.c`, `character_controller.h`; `test_character_controller.c` and Game Authoring tests | Authored values migrate and apply through Scene Document; live scene/entity synchronization is production-backed | **Foundation** | Swept movement, advanced slopes/surface response, step offsets, moving-platform maturity, mesh collision, constraints, and full packaged gameplay proof remain open. |
| Lighting and environment | Scene-owned sky/environment, separate sun where configured, local lights, directional shadows, bounded probes/IBL, and Rendered scene presentation | Scene environment API, `renderer_opengl.c`, lighting/shadow/IBL scripts and renderer tests | Environment settings persist through the supported settings path and are consumed by packaged Rendered startup/reference paths | **In Progress** | Remaining IBL/specular visual defects, broader lighting authoring, and full visual-quality acceptance remain open. |
| Scripting and behaviors | Bounded HenkaScript and Lua compilation/adapters, compiler-owned editor tokenization, Scene Document attachments, Play dispatch, state persistence, and cross-language events | `engine/src/scripting/`; scripting public headers; script source/host/runtime/backend/state tests and Sandbox script editor | Attachments and state use the Scene Document/Play boundary; packaged end-user project scripting is not complete | **In Progress** | Broader host APIs, debugger presentation, end-user project workflows, and stable script packaging/versioning remain open. |
| Audio | Runtime WAV/Ogg/MP3/FLAC loading, resident/streamed bounded voices, buses, listener/emitter spatialization, real scene-object ownership, persistence, script bindings, and SDL output boundary | `engine/src/core/audio.c`, decoder, SDL output; `audio.h`; audio, asset, Sandbox runtime, and packaged smoke coverage | Scene Document v6, Play, Object Details, asset manager, mixer, and packaged output are connected within the bounded scope | **Available within bounded runtime scope** | Long-form packaged content, effects, occlusion, broader spatial features, and device-thread synchronization remain open. |
| Networking and dedicated server | Renderer-free runtime consumer, fixed ticks, loopback, terrain authority, snapshots, reconnect/late-join selection, persistence, and graceful shutdown | `engine/src/network/`; `network.h`; dedicated-server, terrain, and external-server tests/scripts | External server template and packaged server checks exercise the supported boundary | **Foundation** | Authentication, scale, broader residency orchestration, and multiplayer soak remain open. |
| Packaging and external projects | Windows Sandbox/server packaging, provenance, startup contract, external game/server templates, and bounded public-consumer smoke paths | `package_*`, `check_packaged_*`, external-template scripts; `PACKAGE_INFO.txt` contract; exact-candidate package/test gates | Package records commit/source/config/toolchain/hash; external projects consume public boundaries without Sandbox source | **Foundation** | Broader project/editor serialization, release distribution, platform expansion, and full external authoring workflows remain open. |
| Diagnostics and validation | CTest executable regressions, repository/public-content/integrity gates, package provenance, application-local automation, and visual-evidence capture | `tests/`, `scripts/check_*`, `scripts/test_*`, `scripts/materialize_exact_candidate_windows.ps1`; exact publication validation | Exact candidate can be materialized from Git object state and tested independently; human visual QA remains authoritative for perceptual claims | **Available within Windows validation scope** | More complete semantic visual coverage and broader non-Windows validation remain open. |
| MCP and semantic automation | Current Sandbox MCP/stdio and application-local semantic action scope used for deterministic QA and inspection | `examples/sandbox3d/mcp_server.c`, MCP headers/tests, MCP scripts | Current scope is executable and testable; no new MCP surface is implied by this matrix | **Foundation (frozen)** | Freeze remains in force until the 3D completion audit identifies a necessary product gap. |
| 2.5D | Camera presets, orthographic framing/zoom, stable vertical basis, and Sandbox persistence | Camera APIs/tests and Sandbox camera tools | Current camera foundation is usable in the 3D editor | **Foundation** | Sprite/layer authoring, depth sorting, movement-plane constraints, and a complete 2.5D workflow remain future work. |
| 2D | No dedicated production 2D renderer or complete 2D authoring surface yet | Roadmap and current-capabilities boundary; no material production implementation | No complete 2D persistence/package path exists | **Planned** | Renderer, sprites, cameras, animation, physics, tile/world authoring, effects, scripting, persistence, and packaging require a dedicated campaign. |
| Animation and character production | No complete rigging, skinning, animation authoring, or runtime production workflow | Public headers and roadmap references are foundation hooks, not completion evidence | No complete persistence/editor/package chain exists | **Planned** | Requires a deliberate production asset and runtime animation campaign. |

## Completion interpretation

The matrix separates a real production foundation from a completed user-facing
workflow. A public API, a test-only path, a fixture, or a documentation claim
does not close a row by itself. A row moves to **Available** only when its
declared scope is connected through the relevant runtime, editor, persistence,
packaging, and executable-validation boundaries. Visual and interaction claims
also require human review when automated checks cannot establish perceptual
quality.

The Giraffe and Rocket remain repository-owned deterministic regression and
reference fixtures. They are not ordinary startup content and do not establish
arbitrary user production-asset authoring coverage. The normal startup contract
is the product-native ground/camera/environment scene described above.

This matrix should be updated with the owning capability documentation whenever
a row's supported scope or evidence boundary changes.
