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

- **Editor workspace:** desktop readability, long-label presentation, dense
  control layout, and human visual QA remain unresolved.
- **Camera and interaction:** laptop/touchpad navigation and reliable
  cold-start scene-first framing still require direct validation.
- **Cross-subsystem completion:** hierarchy, broader persistence,
  project/external-project workflows, and complete runtime/package coverage
  remain foundations rather than closed product workflows.

## Current matrix

| Capability | Supported scope | Production authority and evidence | Persistence / runtime / package boundary | Status | Current gap or next boundary |
| --- | --- | --- | --- | --- | --- |
| Core runtime and platform | C17 engine/runtime libraries, Windows 64-bit MSVC development and packaging path | `CMakeLists.txt`; public headers under `engine/include/henka/`; core, scene, asset, and runtime tests | `henka`, `henka_runtime`, Sandbox3D, dedicated server, and `henka_tests` are built and exercised | **Available** | Other desktop and mobile platform targets remain planned. |
| OpenGL renderer | Wire, Solid, Material Preview, and Rendered scene views on the supported Windows OpenGL path; scene materials, lights, shadows, IBL, fog, bloom, AO, tone mapping, and bounded temporal presentation | `engine/src/renderer/`; `renderer.h`; shader/material tests; packaged startup, renderer, IBL, and stress validation scripts | Renderer consumes runtime scene/material authority; exact Release packaging and bounded runtime checks cover startup, environment, temporal, texture, terrain, and Audio-adjacent scene use | **Available within supported Windows/OpenGL scope** | Other backends and advanced effects outside this declared scope remain future expansion. Modeling-owned construction and content-quality issues remain with their owning rows. |
| Assets and materials | Bounded OBJ and glTF/GLB import; explicit transactional mesh and file-backed texture reimport; mesh, camera, light, PBR material, texture, asset metadata, material assets, and material instances within documented interchange limits | `engine/src/assets/model_obj.c`, `model_gltf.c`, `assets.c`; `assets.h`, `model.h`; `tests/test_model.c`, `tests/test_assets.c`, `tests/test_material.c` | Imported assets can enter scenes and the native editable path; explicit mesh and file-backed texture reimport preserve borrowed resource identity for existing scene and material references; supported material state persists where the owning format/document supports it | **In Progress** | Broader automatic reimport/dependency-scale workflows, broader material editing, texture painting, automatic UV unwrap, and complete external-project material workflows remain open. |
| Scene and entity core | Stable scene/entity identity, transforms, visibility, interaction, bounds, camera ownership, and scene environment | `engine/src/scene/scene.c`, `camera.c`; `scene.h`, `camera.h`; scene, camera, asset, and authoring tests | Scene state is consumed by editor, Play, renderer, Audio, physics, and Scene Document paths | **Available (unhardened)** | Broader project serialization, hierarchy maturity, and cold-start/user-input evidence remain open. |
| Product-native default scene | Ordinary startup contains a centered editable ground plane, scene camera, scene-owned sky/environment, and no showcase or diagnostic entities; explicit capture modes load reference fixtures | `sandbox3d_create_default_ground_authoring`; `DEFAULT_SCENE_READY`; `CAPTURE_READY_STARTUP`; fixture-scope and packaged startup checks | Startup state is created through the normal authoring path and is available to save/reload and runtime workflows | **Available within startup scope** | Broader default-scene authoring and project lifecycle coverage remains part of Game Authoring maturity. |
| Camera and viewport navigation | Perspective/orthographic camera math, presets, framing, orbit/pan/dolly, follow, world/screen conversion, Compass presentation, and persisted viewport preferences | `engine/src/scene/camera.c`, `camera_follow.c`; camera headers; camera and Compass tests; Sandbox camera tools | Camera state is used by the editor and packaged Sandbox; Scene Document stores the supported authored camera boundary | **Available (unhardened)** | Touchpad/laptop navigation and broader camera rigs, blending, collision, shake, and cinematic workflows remain open. |
| Editor workspace and UI | Docked workspace, panels, detached-window foundation, layout persistence, Scene View, Object Details, modeling controls, and application-local interaction validation | `engine/src/ui/`; `examples/sandbox3d/editor_layout.c`, `editor_ui_state.c`, `editor_controls.c`; UI/editor tests and Windows interaction scripts | Packaged Sandbox is the user-facing editor boundary; layout state persists through the supported settings path | **In Progress** | Desktop readability, hierarchy/project workflows, dense control presentation, and human visual QA remain active work. |
| Native modeling and content authoring | Product-native primitives, editable meshes, component selection, topology analysis, bounded transactional vertex/edge/face operations including bounded face-winding flip, bounded compatible interior-triangle edge flip, and transactional batch deletion preserving one renderable face, bounded batch face-corner rip from pairwise-compatible selected surface vertices, connected face-region extrusion, pairwise and multi-chain boundary-edge extrusion, bounded single-sided and pairwise-disjoint compatible interior-edge extrusion, simple connected interior-edge-path extrusion across compatible quad strips, independent simple connected interior-edge-path batch extrusion, bounded three-edge branching interior bevel fans and branching fan-region extrusion, contiguous same-face boundary-edge-chain extrusion, pairwise boundary-vertex extrusion, contiguous same-face boundary-vertex-chain extrusion, batch loose-vertex extrusion, compatible closed interior vertex-fan offset/cap replacement with per-face metadata preservation, pairwise fan-disjoint or connected compatible closed interior vertex-fan batch extrusion, bounded disconnected or full-edge-connected face inset, bounded disconnected or full-edge-connected face bevel, bounded disconnected or full-edge-connected face subdivision, deterministic planar face triangulation for one or a bounded vertex-disjoint face selection, bounded compatible boundary-edge bridging including equal-length open or closed boundary-edge chains, one or more independent closed boundary-loop fills, uniformly spaced multi-cut across compatible open quad strips and closed rings including pairwise-disjoint, vertex-connected, and compatible face-overlapping intersecting quad-strip networks, bounded pairwise-disjoint, contiguous same-face, and independent multi-chain boundary face-backed edge split, bounded pairwise-disjoint face-backed edge-delete batches, bounded pairwise-disjoint compatible interior-edge-dissolve batches, bounded standalone loose-edge midpoint split and delete batches, UV/material state, explicit edge-seam editing, deterministic all-island UV packing, bounded planar-chart, cylindrical, and spherical/ellipsoidal UV unwrap, bounded OBJ geometry/UV export, and editor preview/apply/cancel paths for the integrated operations | `authoring_mesh.h`, `authoring_modeling.h`, `authoring_topology.h`; `object_authoring_tools.h`; mesh/modeling/topology/UV sources; authoring and visible-modeling tests | HAMS v6 persists loose topology and explicit UV seam state; bounded OBJ export covers geometry, face-corner UVs, and standalone line records; native authoring, evaluated replacement, material promotion, seam editing, all-island packing, disconnected or full-edge-connected face inset/bevel/subdivision, planar face triangulation, bounded batch face-corner rip, boundary-edge bridging, one or more independent closed boundary-loop fills, closed-fan cap replacement, connected or fan-disjoint compatible closed-fan batch extrusion, boundary-edge chain and independent boundary-edge chain extrusion, pairwise-disjoint compatible interior-edge extrusion, compatible interior-edge extrusion, connected interior-edge-path extrusion, independent simple connected interior-edge-path batch extrusion, branching interior bevel fans, branching fan-region extrusion, uniformly spaced quad-strip multi-cut and pairwise-disjoint, vertex-connected, and compatible face-overlapping intersecting quad-strip networks, pairwise-disjoint, contiguous same-face, and independent multi-chain face-backed edge splits, bounded face-backed edge-delete batches, bounded face-winding flip, bounded compatible interior-triangle edge flip, bounded vertex-disjoint face-normal translation, bounded vertex-disjoint planar face triangulation, transactional selected-face deletion, bounded compatible interior-edge-dissolve batches, and standalone loose-edge midpoint split and delete batches use the shared source/history boundary; the integrated operations have mesh-level and routed transactional coverage with Sandbox preview/apply/cancel/undo/redo routing | **Available** | Mixed, cyclic, overlapping, or unsupported connected interior fan/edge sets, overlapping or generalized seam-rip workflows, vertex-only, ambiguous multi-edge, or otherwise unsupported batch bevel, subdivision, or triangulation selections, branching bevel or extrusion domains beyond the supported three-edge valence-three fan, generalized interior surface-connected vertex/edge extrusion beyond the supported open-fan, connected or fan-disjoint closed-fan, pairwise-disjoint compatible interior-edge, compatible single-sided interior-edge, and independent simple connected interior-edge-path batch domains, generalized branching edge-bridge or split workflows beyond the bounded pairwise-disjoint, same-face boundary-chain, independent multi-chain face-backed, and standalone loose-edge split/delete domains, general branching, consumed-seed, duplicate-traversal, and otherwise ambiguous loop-cut networks beyond the bounded compatible intersecting quad-strip domain, broader automatic UV unwrap beyond bounded planar-chart, cylindrical side-surface, and spherical/ellipsoidal surface projections, texture painting, rigging, animation authoring, source export beyond the bounded OBJ geometry/UV path, and production-quality arbitrary asset authoring remain outside this bounded foundational scope. |
| Scene Document and Game Authoring | Registered scene objects, authored transforms/identity/visibility/interaction, bounded renderer/material values, direct lighting/fog/environment, value-owned local lights and reflection probes, Audio, Physics including sphere/box/upright-capsule configuration, behavior attachments, Play mapping, manifest-selected startup scenes, prefab-instance provenance and explicit non-root local-transform override metadata, and v1-v15 `.hscene` migration | `scene_document.h`, `scene_document.c`; `scene_document_bridge.c`, `game_authoring.c`, `play_session.c`; Game Authoring and Scene Document tests | Save/load applies authored state to live runtime objects and scene presentation with transactional/fail-closed behavior; `henka.project` selects one confined startup `.hscene`; packaged startup and authoring smoke paths exist | **Foundation** | Complete project/scene serialization, broader component coverage, and end-user Game/Play workflows remain incomplete. |
| Undo/redo and history | Bounded Game Authoring history for hierarchy reparent/unparent and other supported Scene Document object transactions; native mesh, material, terrain, and workspace histories remain subsystem-local | `authoring_mesh.c`, `game_authoring.c`, terrain history, workspace persistence; authoring, hierarchy, material, terrain, and editor tests | Supported operations commit through their owning transactional history boundary; Game Authoring undo/redo restores the authored Scene Document object and live relationship through the same bridge; package-level and cross-owner history is not complete | **Foundation** | Cross-subsystem history, broader editor coverage, and complete project-level history remain open. |
| Scene hierarchy and parenting | Runtime parent/unparent/reparent, keep-local/keep-world transforms, cycle rejection, persisted parent IDs, bounded prefab snapshot hierarchy, parent-first identity-safe Scene Objects projection, explicit Object Details hierarchy controls, and bounded Game Authoring transaction history for hierarchy objects | Scene hierarchy APIs and `scene_document.c`; `examples/sandbox3d/scene_hierarchy_projection.c`, `game_authoring.c`, and `main.c`; physics/controller hierarchy tests | Runtime and Scene Document foundations are connected; the Scene Objects panel and Object Details hierarchy group project and mutate the canonical parent relationship without creating a second hierarchy authority, Game Authoring history replays supported object transactions through the same bridge, and linked physics/controller owners can consume live hierarchy transforms through the explicit pre-step bridge | **In Progress** | Broader subsystem transform propagation, broader project serialization, cross-subsystem/project history, and broader packaged visual proof remain open. |
| Prefabs and reusable scene objects | Bounded `.hprefab` capture/save/load/update, instantiate/under-parent, structural refresh, transform/material overrides, duplicate/detach/unpack, Apply/Revert history, and editor Create/Place/Update From Selected/Apply/Revert/Unpack/Delete workflow | `prefab.h`, `engine/src/core/prefab.c`, `scene_document.h`, `game_authoring.c`, Object Details/Asset Browser, Prefab and Game Authoring regressions | Project-relative Prefab identity/provenance round-trips through HSCN; persisted source-local IDs remain stable across compatible structural refresh; packaged Sandbox public Prefab smoke, packaged Game Authoring source-refresh/save-load/Play-restart smoke, and the external game template exercise public save/load/instantiate/override/duplicate/detach without Sandbox-source dependency | **Available within bounded 3D Prefab scope** | Nested Prefab composition, ambiguous membership-change migration, and cross-project asset relocation resilience remain future expansion beyond this bounded workflow. |
| Terrain and world | Four material layers, height/paint edits, collision patches, normals, bounded LOD, streaming observers, resident CPU/physics/render owners, persistence, and server/client authority | `engine/src/terrain/`; terrain public headers; terrain unit, workflow, process, visual, and server/client tests | Save/reload, streaming, headless/server, and Sandbox Rendered paths are exercised; package visual evidence is bounded | **Foundation** | Four-way corner visual approval, broader-world streaming, background regeneration, and larger-world product workflows remain open. |
| Physics | Bounded current rigid-body v1: fixed-step static, dynamic, and kinematic bodies; sphere, upright capsule, axis-aligned box, plane, bounded heightfield, and bounded static triangle-mesh collision; contacts, impulses, friction, restitution, triggers, raycasts, failure-safe replacement, and persisted authored sphere/box/capsule configuration | `engine/src/core/physics.c`; `physics.h`; Scene Document bridge; physics, heightfield, triangle-mesh, and Sandbox physics tests; packaged `--physics-smoke-test` | Runtime scene/entity transforms, Terrain collision paths, Scene Document save/load, and Play materialization are connected; the packaged Physics smoke uses six real scene-linked bodies for fixed-step, event, raycast, and reset proof plus bounded heightfield and triangle-mesh contact/raycast production checks | **Available** | Mesh/mesh, dynamic or kinematic mesh bodies, arbitrary concave Scene Document authoring, constraints, advanced simulation, and broader gameplay features are outside this declared scope. |
| Character Controller | Real dynamic upright capsule body, bounded planar input, acceleration/deceleration, jump queueing, wall sliding, walkable-plane grounding, bounded kinematic-platform inheritance, teleport, Scene Document values, and Play linkage | `character_controller.c`, `character_controller.h`; `test_character_controller.c` and Game Authoring tests | Authored values migrate and apply through Scene Document; live scene/entity synchronization is production-backed | **Foundation** | Swept movement, advanced slopes/surface response, step offsets, moving-platform maturity, mesh collision, constraints, and full packaged gameplay proof remain open. |
| Lighting and environment | Scene-owned sky/environment, separate sun where configured, local lights, directional shadows, bounded probes/IBL, and Rendered scene presentation | Scene environment API, `renderer_opengl.c`, lighting/shadow/IBL scripts and renderer tests | Environment settings persist through the supported settings path and are consumed by packaged Rendered startup/reference paths | **In Progress** | Broader lighting authoring, probe/post-processing quality, hardware-portability evidence, and full visual-quality acceptance remain open; historical localized reflection artifacts are retained as regression/history and route to Modeling when reproduced in authored geometry. |
| Scripting and behaviors | Bounded HenkaScript and Lua compilation/adapters, compiler-owned editor tokenization, Scene Document attachments, Play dispatch, state persistence, and cross-language events | `engine/src/scripting/`; scripting public headers; script source/host/runtime/backend/state tests and Sandbox script editor | Attachments and state use the Scene Document/Play boundary; packaged end-user project scripting is not complete | **In Progress** | Broader host APIs, debugger presentation, end-user project workflows, and stable script packaging/versioning remain open. |
| Audio | Runtime WAV/Ogg/MP3/FLAC loading, resident/streamed bounded voices, buses, listener/emitter spatialization, real scene-object ownership, persistence, script bindings, and SDL output boundary | `engine/src/core/audio.c`, decoder, SDL output; `audio.h`; audio, asset, Sandbox runtime, and packaged smoke coverage | Scene Document v6, Play, Object Details, asset manager, mixer, and packaged output are connected within the bounded scope | **Available within bounded runtime scope** | Long-form packaged content, effects, occlusion, broader spatial features, and device-thread synchronization remain open. |
| Networking and dedicated server | Renderer-free runtime consumer, fixed ticks, loopback, terrain authority, snapshots, reconnect/late-join selection, persistence, canonical channel/type wire validation, and graceful shutdown | `engine/src/network/`; `network.h`; dedicated-server, terrain, and external-server tests/scripts | External server template and packaged server checks exercise the supported boundary | **Foundation** | Authentication, scale, broader residency orchestration, and multiplayer soak remain open. |
| Packaging and external projects | Windows Sandbox/server packaging, provenance, startup contract, external game/server templates, and bounded public-consumer smoke paths | `package_*`, `check_packaged_*`, external-template scripts; `PACKAGE_INFO.txt` contract; exact-candidate package/test gates | Package records commit/source/config/toolchain/hash; external projects consume public boundaries without Sandbox source | **Foundation** | Broader project/editor serialization, release distribution, platform expansion, and full external authoring workflows remain open. |
| Diagnostics and validation | CTest executable regressions, repository/public-content/integrity gates, package provenance, application-local automation, and visual-evidence capture | `tests/`, `scripts/check_*`, `scripts/test_*`, `scripts/materialize_exact_candidate_windows.ps1`; exact publication validation | Exact candidate can be materialized from Git object state and tested independently; human visual QA remains authoritative for perceptual claims | **Available within Windows validation scope** | More complete semantic visual coverage and broader non-Windows validation remain open. |
| MCP and semantic automation | Current Sandbox MCP/stdio and application-local semantic action scope used for deterministic QA and inspection | `examples/sandbox3d/mcp_server.c`, MCP headers/tests, MCP scripts | Current scope is executable and testable; no new MCP surface is implied by this matrix | **Foundation (frozen)** | Freeze remains in force until the 3D completion audit identifies a necessary product gap. |
| 2.5D | Camera presets, orthographic framing/zoom, stable vertical basis, and Sandbox persistence | Camera APIs/tests and Sandbox camera tools | Current camera foundation is usable in the 3D editor | **Foundation** | First-class 2.5D remains deferred until the current general 3D and first-class 2D completion boundaries have closed. |
| 2D | No dedicated production 2D renderer or complete 2D authoring surface yet | Roadmap and current-capabilities boundary; no material production implementation | No complete 2D persistence/package path exists | **Planned** | Dedicated first-class 2D work begins only after the current general 3D completion boundary closes. |
| Animation and character production | No complete rigging, skinning, animation authoring, or runtime production workflow | Public headers and roadmap references are foundation hooks, not completion evidence | No complete persistence/editor/package chain exists | **Planned** | Requires a deliberate production asset and runtime animation campaign. |

The Native modeling row's bounded boundary-fill scope includes one or more
independent simple closed boundary loops. Shared, branched, mixed, duplicate,
invalid, and capacity-insufficient selections fail without partial publication.

The Native modeling row's bounded Edge Extrude scope also includes one
compatible closed interior edge cycle when the complete cycle either exactly
encloses one source face or separates a unique smaller connected face region.
The selected cycle must be connected, degree-two at every selected vertex, and
made of compatible two-face edges. Equal-sized, non-separating, and otherwise
ambiguous cyclic domains remain outside that declared boundary and fail closed.

The same bounded Edge Extrude scope includes a complete closed interior edge
fan when every selected edge shares one interior vertex, all of that vertex's
incident edges are selected, and the selected edges bound one compatible quad
face fan. Incomplete or ambiguous branching fans fail closed.

The Native modeling row's bounded split/delete scope includes the face-backed
boundary/interior edge split, its pairwise-disjoint batch form, and the
standalone loose-edge midpoint split and pairwise-disjoint split/delete batch
forms, plus pairwise-disjoint face-backed edge-delete batches. All use candidate validation and routed
Sandbox history; broader branching and ambiguous split domains remain outside
that declared scope.

The same bounded loose-component boundary includes standalone loose-edge
extrusion for one or a pairwise-disjoint batch, with routed Preview, Apply,
Cancel, Undo, and Redo coverage.

The Sandbox face-normal operator supports one or a bounded vertex-disjoint
face selection. Shared-vertex selections fail during Preview so the result is
independent of face iteration order and the live mesh remains unchanged.

The Sandbox triangulation operator supports one or a bounded vertex-disjoint
planar face selection. Shared-vertex, duplicate, non-planar, invalid, and
capacity-invalid selections fail during Preview without changing the live mesh.

The Sandbox inset and bevel operators support one or a bounded disconnected or
full-edge-connected simple planar face selection. Vertex-only contact,
ambiguous multi-edge sharing, duplicate, invalid, and capacity-invalid
selections fail during Preview without changing the live mesh.

## Renderer completion ledger

This ledger makes the supported renderer boundary explicit without creating a
second roadmap. It covers the current Windows OpenGL backend only. Vulkan,
Direct3D, Metal, mobile backends, and advanced effects outside the listed
scope are future expansion and do not change the status of this boundary.

| Renderer area | Supported scope | Production evidence | Status | Remaining closure condition |
| --- | --- | --- | --- | --- |
| View modes and viewport presentation | Wire, Solid, Material Preview, and Rendered Scene View policies with bounded viewport resize | `engine/include/henka/renderer.h`; `engine/src/renderer/renderer.c`; `renderer_opengl.c`; packaged startup and viewport validation | **Available** | Direct laptop/touchpad navigation and broader editor presentation remain outside this row. |
| Geometry submission and draw state | Runtime scene meshes, indexed parts, depth/cull/blend state, scene viewport targets, and terrain draw submission | `renderer_opengl.c`; mesh/renderer tests; packaged Sandbox startup and scene rendering | **Available within OpenGL scope** | Broader geometry-quality and modeling claims remain owned by the authoring/assets rows. |
| PBR material evaluation | Bounded base color, metallic/roughness, normal, occlusion, emission, transmission, clearcoat, sheen, and subsurface inputs in the Rendered path | `assets/shaders/basic_lit.frag`; shader contracts and material tests; packaged material/reference paths | **Available within bounded Rendered evaluation scope** | Material authoring, texture generation quality, and broader probe/post-processing workflows remain with Assets/Materials and later expansion rows. |
| Direct lighting and shadows | Scene directional, local spot, point, and bounded near/cascade shadow paths with PCF and contact response | `basic_lit.frag`; `renderer_opengl.c`; shadow tests and packaged shadow evidence | **Available within bounded scope** | Broader lighting authoring and visual-quality expansion remain open; the repaired near-directional regression must remain protected. |
| Environment, IBL, and reflection probes | Scene sky/environment, HDR-to-cubemap conversion, irradiance, prefiltered specular, BRDF LUT, bounded local probes, distinct source/destination probe-prefilter resources, and probe blending | `henka_opengl_build_ibl_resources`; OpenGL probe-prefilter path; IBL shader contracts; IBL/probe tests and diagnostic captures | **Available within bounded Windows/OpenGL scope** | The historical localized reflection-knot regression remains protected and is not reproduced on exact current main. Probe-grid authoring, broader HDRI workflows, and other backends remain future expansion. |
| Post-processing and reconstruction | HDR target, bloom, tone mapping, AO, SSGI, SSR, motion/reactive inputs, temporal history, and bounded fallback behavior | `henka_opengl_present_hdr`; embedded post shaders; post-effect tests and diagnostics | **Available within bounded packaged OpenGL scope** | Advanced reconstruction, broader effect authoring, and cross-backend presentation remain future expansion. |
| Texture, color, and sampling | Bounded OpenGL texture formats, color-space descriptors, mip/filter/wrap state, anisotropy, and material texture binding | `engine/include/henka/texture.h`; `texture.c`; OpenGL texture upload path; texture/material tests | **Available within bounded texture scope** | High-resolution residency, broader compressed-format coverage, and material-detail quality remain open expansions. |
| Renderer resource lifetime and resize | Transactional IBL/HDR/bloom/temporal/probe target creation, failure fallback, deletion, memory accounting, and viewport-driven recreation | `renderer_opengl.c`; renderer diagnostics; resize, failure, probe, and package smoke coverage | **Available within bounded OpenGL scope** | Longer-duration soak and device-loss recovery remain broader platform-hardening work. |
| Editor and authoring integration | Scene View shading controls, runtime scene/material authority, and renderer diagnostics used by the editor | `examples/sandbox3d/main.c`; editor/authoring bridge; packaged interaction scripts | **Available within renderer-integration scope** | Selection/topology presentation, desktop readability, and direct modeling authoring remain Modeling/UI work. |
| Persistence, Play, and package boundary | Renderer-owned scene/material/environment/render-settings/local-light/reflection-probe state and authored Physics configuration consumed through Scene Document, Play, exact package provenance, and bounded external runtime smoke | Scene Document/Game Authoring paths; HSCN v16 persistence tests; package/provenance scripts; hosted Windows package jobs | **Available within bounded renderer-state/package scope** | Broader project/editor serialization, complete end-user Game/Play workflows, and external-project visual acceptance remain with their owning capabilities. Human visual acceptance of the packaged renderer references remains a separate perceptual gate. |

The ledger is intentionally evidence-based: source implementation, shader
contracts, unit tests, or reference fixtures alone do not close a user-facing
renderer area when packaged behavior or human visual inspection remains part
of the declared scope.

The bounded Modeling / Content Authoring scope also includes selected-vertex
Smooth Vertices / Relax through the native core, direct-object route, shared
Preview/Cancel/Apply operator, and undo/redo path. Boundary-constrained,
curvature-preserving, sculpting, and broader non-destructive smoothing remain
outside that declared scope.

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
