# Architecture

Henka Engine is still compact, but it now has enough moving parts that the module boundaries matter. The architecture supports a reusable runtime, a docked and detached development workspace, first 2.5D camera foundations, external game repositories, and a future authoring layer without leaking SDL or OpenGL details into application code.

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
- post-link material shader contract validation for the transform and base-color uniforms used by the renderer; invalid external programs are rejected before publication
- a bounded uniform-location cache qualified by the active SDL OpenGL context and program identity, with explicit cache invalidation when programs are destroyed
- mesh upload
- descriptor-aware texture upload and binding, including sRGB versus linear internal formats, sampler policy, mip selection, and upload-state restoration
- tangent-space vertex attributes generated during mesh upload with finite fallbacks for degenerate UVs
- normalized RGBA vertex-color attributes, defaulting generated and OBJ geometry to opaque white
- bounded metallic-roughness material evaluation with base color, normal, metallic-roughness, occlusion, emissive data, and a clearcoat lobe
- depth testing
- backface culling
- wireframe toggle
- scene viewport bounds
- scene scissor and viewport state for docked workspaces
- draw submission for scene entities
- draw submission for simple screen-space UI rectangles

External material shaders are admitted only after the renderer confirms the minimum contract it will populate (`model`, `view`, `projection`, and `baseColor`). This keeps a successful file read or link from becoming a later draw-time failure. The uniform cache is a bounded transitional safety layer: entries are keyed by both context and program, but broader per-context renderer ownership remains future work as additional OpenGL backends are introduced.

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
- The sandbox reads object selection and details from the scene plus sandbox-owned descriptors rather than from a saved scene file or editor-only data model.
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
- saved workspace placement, full detached controls, drag-back redocking, and detachable Scene View
- editable authoring data compiled into runtime assets for later modeling, UV, rigging, and animation workflows
## Viewport shading

The Scene View owns an explicit shading mode rather than relying on a global polygon toggle. Wireframe draws neutral geometry edges without texture sampling. Solid draws neutral filled surfaces under a neutral editor surface policy. Material Preview uses the same bounded Cook–Torrance material evaluation and Scene View-sized linear HDR-to-display presentation as Rendered, with deterministic editor lighting instead of scene lighting. Both HDR modes currently use validated scene-owned studio-environment controls for visible surroundings and diffuse/specular fallback; external HDR image preprocessing and IBL cache assets remain a separate branch. Rendered uses scene light policy, optional bounded scene fog, exposure, tone mapping, and the directional shadow path. Blended materials render after opaque and masked geometry through a bounded back-to-front queue with deterministic entity-order overflow fallback. Unlit materials bypass lighting, reserved procedural materials are rejected until a real shader model exists, and helper overlays retain their own materials. Mode changes do not rewrite scene materials, and the renderer restores a filled polygon baseline before UI and detached-window presentation.

The legacy wireframe API remains compatible: enabling it selects Wireframe, while disabling it restores the last valid non-wireframe mode. The sandbox persists the authoritative mode under `ui.scene_view.shading_mode`; older `wireframe_enabled` settings are migrated only when the new key is absent.
