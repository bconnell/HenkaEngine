# Architecture

Henka Engine is still compact, but it now has enough moving parts that the module boundaries matter. The architecture supports a reusable runtime, a docked and detached development workspace, first 2.5D camera foundations, external game repositories, and a future authoring layer without leaking SDL or OpenGL details into application code.

### Shared runtime and graphical client

The renderer-independent `henka_runtime` static library owns the current
math, logging, memory, persistence, physics, result, time, camera, and scene
foundations. It has no SDL, OpenGL, or KTX link dependencies. The graphical
`henka` compatibility target links that runtime and adds the existing SDL,
renderer, asset, UI, and editor-facing implementation. A dedicated server
consumer is therefore able to link only `henka_runtime`; this boundary is the
foundation for the server and terrain work that follows.

### Network boundary

The public network header defines Henka-owned protocol values and views only;
wire packets use a fixed little-endian header and bounded payloads rather than
native C layouts. Control, terrain, and snapshot traffic have separate logical
channels. The shared runtime privately pins ENet commit
`5a9c537fd464b3c6d3c55e1d3bd47588faf71b42` under its MIT license and exposes
only Henka host, peer, event, message, send, poll, disconnect, and diagnostic
types. The transport foundation provides bounded reliable ordered localhost
delivery, one-second peer liveness pings, a finite 120-second reliable-packet
timeout for slow but serviced validation runs, server-directed disconnect, and
client reconnect. Terrain authority
and revision recovery are exercised through the bounded Terrain session
adapter, including a two-client replica convergence regression after edits from
both peers and a finite repeated process integration soak; relevance-driven
multiplayer state and production-scale soak remain subsequent work.

## Modules

### Core

The core layer owns:

- result codes
- logging
- memory wrappers
- engine lifecycle
- frame timing
- persistence helpers and settings I/O
- shared math types
- asset manager ownership

### Memory

The memory module wraps `malloc`, `calloc`, `realloc`, and `free`. In debug builds it tracks a simple active allocation count so shutdown can warn about likely leaks without introducing a custom allocator too early.

### Logging

Logging is synchronous console output with explicit severity, source file, and line number. It is used during startup, shutdown, and failure paths so problems stay visible.

### Platform

The platform layer currently uses SDL3 internally. It owns:

- window creation
- event polling
- framebuffer resize notifications
- close request state
- swap interval control
- relative mouse capture
- native detached tool windows and routed tool-window input
- focus-loss release synthesis
- collision-safe engine window identifiers
- main and detached OpenGL context transitions

SDL types remain outside the public Henka headers.

### Input

The current input layer is still intentionally small, but it now tracks the state needed for:

- keyboard movement
- mouse delta
- mouse button toggles
- help and wireframe controls
- exit handling
- named input actions with key and mouse-button bindings

### Time

The time system provides delta time, total elapsed time, and a frame counter. Camera motion and general update work are driven from this timing state.

### Math

The math layer provides:

- vectors
- quaternions
- matrices
- transforms
- projection and view helpers

These types are public because they are part of the engine-facing scene and camera API.

### Camera

The current camera module provides:

- perspective camera creation
- orthographic camera creation
- simple fly movement
- clamped mouse look
- camera reset helpers
- camera focus on bounds
- screen-point ray creation

### Assets

The current asset layer is intentionally modest. It loads and caches shaders, textures, and OBJ meshes through canonical confined path identities, owns cached and fallback resources, exposes borrowed manager-owned pointers, preserves fallback entries during failed retries, and keeps asset lifetime tied to the engine runtime. Rooted, UNC, device, drive-qualified, traversal, and URI-like inputs are rejected before canonicalization can erase their meaning. Equivalent slash and dot-segment spellings resolve to one cache identity; Windows identity comparison also folds ASCII case without changing the normalized source spelling reported by metadata. A failed texture source receives its own lightweight alias of the shared error texture, allowing pointer-specific metadata and preserving the borrowed pointer when a later retry installs real GPU data. Allocation and renderer failures remain errors instead of being cached as source fallbacks.

It also now exposes read-only asset metadata so samples can inspect:

- asset type
- source path
- display name
- loaded or fallback state
- short summary strings

### Persistence

The persistence layer is intentionally small and local-first. Right now it provides:

- a text `key=value` settings format
- safe load and save helpers
- a small save-data model separate from settings
- slot-path helpers under the user-data directory

### UI

The current UI layer is intentionally small and dependency-conscious. Right now it provides:

- a lightweight UI context
- frame begin and end flow
- panel, label, button, toggle, tab, and status-chip primitives
- basic hover and click state
- simple built-in text rendering from engine-owned source code glyph data
- screen-space overlay drawing through the existing renderer
- viewport frame drawing for a docked scene-view region
- mouse hover, press, and release handling for basic clickable controls
- sandbox workspace modes, utility views, and short in-window status feedback built on top of those primitives

It is meant to support engine samples and a viewport-first developer workspace without exposing OpenGL or SDL types in the public UI API.

### Gizmos

The current transform gizmo path is a small engine-owned foundation, not a separate editor subsystem. Right now it provides:

- axis labels and snap helpers
- viewport-aware gizmo hit testing
- projected drag math for world-axis move and rotate
- reusable scene-space mesh helpers for lines and rings

The sandbox builds on that foundation with scene entities that render as gizmo handles inside the dedicated viewport. Those helper entities stay outside the normal sample-object list and picking rules so the current scene inspection flow remains predictable.

### Scene

The scene layer is intentionally minimal. It is not a full ECS. Right now it provides:

- scene ownership
- lightweight entity handles
- entity enumeration and lookup for developer inspection
- per-entity labels and visibility state
- per-entity tags
- per-entity transform, mesh, and material assignment
- per-entity local bounds
- per-entity interaction metadata
- nearest-hit picking against simple bounds
- one active scene camera
- one directional light direction, color, intensity, and ambient color
- per-object visibility and debug labels

### Renderer

The renderer layer exposes engine-owned drawing functionality while keeping OpenGL isolated to renderer implementation files. The current OpenGL backend handles:

- context creation
- viewport resize
- shader compilation and linking
- post-link shader contract validation with a minimal geometry variant and a complete material variant covering transforms, lighting, environment, fog, material factors, alpha, vertex color, textures, and shadows; invalid external programs are rejected before publication
- immutable per-program uniform-location tables qualified by the active SDL OpenGL context, explicit contract identity/version, source hash, and generation; tables are populated after link and destroyed with their owning program
- mesh upload
- descriptor-aware texture upload and binding, including sRGB versus linear internal formats, sampler policy, mip selection, and upload-state restoration
- tangent-space vertex attributes preserved from validated imported model data when present, otherwise generated during mesh upload with finite fallbacks for degenerate UVs
- normalized RGBA vertex-color attributes, defaulting generated and OBJ geometry to opaque white
- bounded metallic-roughness material evaluation with base color, normal, metallic-roughness, validated dielectric specular factor/color and IOR, occlusion, emissive data, and a clearcoat lobe
- depth testing
- backface culling
- wireframe toggle
- scene viewport bounds
- scene scissor and viewport state for docked workspaces
- draw submission for scene entities
- draw submission for simple screen-space UI rectangles

External shaders are admitted through an explicit public contract type and version; source-text markers do not select the contract. The current material and minimal-geometry contracts are validated before publication, while the renderer also reserves explicit identities for future environment, IBL, shadow, post, debug, and UI programs. Required locations are queried once during admission into an owned bounded table, and draw-time required-uniform paths use that table. The draw path does not use a process-global cache or repeat GL string lookup. Built-in environment, shadow, tone-map, and viewport programs use the same table mechanism.

### Sandbox

The sandbox is a consumer of the public API only. It creates a scene, shaders, textures, meshes, materials, camera, settings object, and UI context through Henka headers, then hands those objects to the engine run loop through callbacks. It now uses the early UI layer for docked and native detached inspection, utility, diagnostics, transform, and physics surfaces without claiming to be a full production editor. Core inspection surfaces include:

- `Controls`
- `Scene Objects`
- `Object Details`

The current interaction rules pause camera movement and mouse look while the UI is open, release mouse capture when the UI opens, and let `Escape` close the UI before it resumes the normal capture and exit flow. The sandbox now also calculates a docked workspace layout so the scene renders inside a dedicated viewport region while the panels stay in separate docked boxes. Selected objects can show a transform gizmo in that viewport, and manipulation uses viewport-relative rays so docked panel clicks do not affect scene transforms.

## Current boundaries

- Applications talk to the engine through the public Henka headers.
- The engine owns the main loop, timing, scene pointer, renderer lifecycle, action bindings, package mode, and diagnostics snapshot.
- The engine also owns the asset manager and fallback assets.
- The engine resolves runtime assets relative to the executable directory by default, which keeps packaged sandbox runs independent from the repository root.
- The engine also resolves a local user data base path beside the executable by default, which keeps sandbox settings local to the runnable folder.
- The engine can also draw an optional UI context after the 3D scene, which keeps sandbox overlays inside the engine render path instead of requiring an external UI dependency.
- The sandbox reads object selection and details from the scene plus sandbox-owned descriptors. Saved scene files and editor-only data models are not used for this path.
- The sandbox does not include SDL, Windows, or OpenGL headers.
- OpenGL stays in renderer implementation files.
- Scene data is public enough to build with, but renderer details stay private.
- The sandbox is an engine sample and QA target. Real games should live in separate repositories and point at Henka from there.

## Near-term direction

The next steps should continue building upward from these boundaries:

- safer camera orientation controls
- broader material import
- broader model loading beyond the current OBJ subset
- stronger asset management
- broader persistence and external project support once the current local-first path has settled
- production-quality 2.5D sprites, regions, layered depth, sorting, parallax, animation, movement constraints, physics constraints, and authoring tools after current defect repair
- richer engine UI controls built on the existing docked and native detached workspace
- object inspection and transactional authoring that can grow without an editor rewrite
- saved workspace placement, full detached controls, bounded title-bar drag-back recognition, and detachable Scene View
- editable authoring data compiled into runtime assets for later modeling, UV, rigging, and animation workflows
## Viewport shading

The Scene View owns an explicit shading mode. It does not rely on a global polygon toggle. Wireframe draws neutral geometry edges without texture sampling. Solid draws neutral filled surfaces under a neutral editor surface policy while preserving explicit unlit line materials for the editor grid. Material Preview uses the same bounded Cook–Torrance material evaluation and Scene View-sized linear HDR-to-display presentation as Rendered, with deterministic editor lighting instead of scene lighting. Both HDR modes use validated scene-owned environment controls for visible surroundings and diffuse/specular response; Rendered derives its transactional IBL resources from the same environment texture. Rendered uses scene light policy, optional bounded scene fog, exposure, tone mapping, bloom, camera- and object-motion history reprojection, two fitted directional shadow cascades with an overlap blend at their transition, a second bounded depth map for the first enabled spot light, and a bounded cubemap for the first enabled point light. The built-in material shader supplies bounded camera- and object-motion plus reactive attachments; the presentation pass performs depth rejection, history clamping, and bounded reconstruction sharpening, while temporal invalidation and fallback state remain observable. Production TAA remains future work until its documented visual cases are validated. Blended materials render after opaque and masked geometry through a bounded back-to-front queue with deterministic entity-order overflow fallback. Unlit materials bypass lighting, reserved procedural materials are rejected until a real shader model exists, and helper overlays retain their own materials. Mode changes do not rewrite scene materials, and the renderer restores a filled polygon baseline before UI and detached-window presentation. The sandbox studio environment is generated as a linear periodic equirectangular source and validated before IBL ownership, avoiding presentation seams from discontinuous fixture pixels.

Directional shadow receiver filtering uses a bounded 3x3 PCF kernel and expands
to 5x5 only around a detected near-cascade blocker. It does not inject a
minimum visibility value into confirmed occlusion, so contact shadows do not
retain an artificial light leak. Cascade selection uses interpolated forward
view-space depth rather than radial camera distance, so the bounded near/far
overlap remains stable when the camera looks across a wide scene. This improves
cascade presentation without changing the fitted map ownership or the
documented bounded-shadow fallback policy.

The legacy wireframe API remains compatible: enabling it selects Wireframe, while disabling it restores the last valid non-wireframe mode. The sandbox persists the authoritative mode under `ui.scene_view.shading_mode`; older `wireframe_enabled` settings are migrated only when the new key is absent.
