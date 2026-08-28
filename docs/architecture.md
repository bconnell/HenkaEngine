# Architecture

Henka Engine is a compact C17 engine with clear runtime, platform, renderer, asset, editor, and authoring boundaries. The architecture supports a reusable runtime, a docked and detached development workspace, 2.5D camera foundations, external game repositories, and future authoring growth while keeping SDL and OpenGL details inside their owning modules.

> **Current architecture status:** The shared runtime, graphical client, scene, asset, scripting, networking, platform, UI, renderer, persistence, and Sandbox consumer boundaries are implemented foundations. Several systems remain intentionally bounded and continue to mature.

## Contents

- [Top-level boundaries](#top-level-boundaries)
- [Modules](#modules)
- [Current boundaries](#current-boundaries)
- [Near-term direction](#near-term-direction)
- [Viewport shading](#viewport-shading)

## Top-level boundaries

### Shared runtime and graphical client

The renderer-independent `henka_runtime` static library owns the current:

- math;
- logging;
- memory;
- persistence;
- physics;
- result handling;
- time;
- camera;
- scene foundations.

`henka_runtime` has no SDL, OpenGL, or KTX link dependencies.

The graphical `henka` compatibility target links the shared runtime and adds SDL, renderer, asset, UI, and editor-facing implementation. Dedicated-server consumers can link only `henka_runtime`. This boundary supports the current server and terrain work.

### Scripting boundary

The shared runtime owns the public, language-neutral scripting schema in `<henka/script.h>`. It uses stable numeric API IDs and fixed typed signatures. Lua and HenkaScript backends resolve bindings once and retain language-specific native thunks.

The HenkaScript front end in `<henka/henkascript.h>` provides bounded lexing, parsing, and type checking. The current scripting foundation also provides:

- bounded HenkaScript bytecode execution;
- bounded Lua callable execution;
- lifecycle adapters for `OnCreate`, `OnStart`, `OnUpdate`, `OnFixedUpdate`, `OnEvent`, targeted interaction/contact signals, `OnDestroy`, and `OnStop`;
- a language-neutral generation-checked behavior runtime;
- a bounded script asset loader for persisted `.lua` and `.hks` attachments;
- Scene Document behavior assembly by persistent object and behavior identity;
- isolated Sandbox Play-session lifecycle and bounded event dispatch;
- an exact typed, synchronous, non-reentrant Script Host;
- a bounded FIFO event queue shared by Lua and HenkaScript;
- persistent-ID mapping from Play to runtime entities and physics bodies;
- a fixed-capacity behavior-state store with explicit sidecar save/load;
- candidate-first behavior reload with generation-checked lifecycle-slot preservation on success;
- bounded compiler/backend diagnostics on candidate reload failure.

The lifecycle adapters do not resolve project paths or own Scene Document persistence. The behavior-state store remains outside authored Scene Document data and the script asset loader. Game Authoring exposes typed coordinator operations for state access and rejects those operations while Play is active.

The Sandbox provides bounded transactional behavior-template attachment and a source-panel Reload action. Reload uses the same candidate-first coordinator seam.

Broader host API coverage, full Inspector authoring, and debugger tooling remain later layers above this boundary.

### Network boundary

The public network header defines Henka-owned protocol values and views. Wire packets use a fixed little-endian header and bounded payloads. Control, terrain, and snapshot traffic use separate logical channels.

The shared runtime privately pins ENet commit `5a9c537fd464b3c6d3c55e1d3bd47588faf71b42` under its MIT license and exposes Henka-owned host, peer, event, message, send, poll, disconnect, and diagnostic types.

The transport foundation provides:

- bounded reliable ordered localhost delivery;
- one-second peer liveness pings;
- a finite 120-second reliable-packet timeout for slow serviced validation runs;
- server-directed disconnect;
- client reconnect.

Terrain authority and revision recovery use the bounded Terrain session adapter. Current coverage includes a two-client replica-convergence regression after edits from both peers and a finite repeated process integration soak. Relevance-driven multiplayer state and production-scale soak remain future work.

## Modules

### Core

The core layer owns:

- result codes;
- logging;
- memory wrappers;
- engine lifecycle;
- frame timing;
- persistence helpers and settings I/O;
- shared math types;
- asset-manager ownership.

### Memory

The memory module wraps `malloc`, `calloc`, `realloc`, and `free`. Debug builds track a simple active allocation count so shutdown can report likely leaks.

### Logging

Logging is synchronous console output with explicit severity, source file, and line number. Startup, shutdown, and failure paths use the same logging boundary.

### Platform

The platform layer currently uses SDL3 internally and owns:

- window creation;
- event polling;
- framebuffer resize notifications;
- close-request state;
- swap-interval control;
- relative mouse capture;
- native detached tool windows;
- routed tool-window input;
- focus-loss release synthesis;
- collision-safe engine window identifiers;
- main and detached OpenGL context transitions.

SDL types remain outside public Henka headers.

### Input

The current input layer tracks:

- keyboard movement;
- mouse delta;
- mouse-button toggles;
- help and wireframe controls;
- exit handling;
- named input actions with key and mouse-button bindings.

### Time

The time system provides delta time, total elapsed time, and a frame counter. Camera motion and general update work use this timing state.

### Math

The math layer provides:

- vectors;
- quaternions;
- matrices;
- transforms;
- projection helpers;
- view helpers.

These types are public because scene and camera APIs use them directly.

### Camera

The current camera module provides:

- perspective camera creation;
- orthographic camera creation;
- simple fly movement;
- clamped mouse look;
- camera reset helpers;
- camera focus on bounds;
- screen-point ray creation.

### Assets

The asset layer loads and caches shaders, textures, and OBJ meshes through canonical confined path identities. It owns cached and fallback resources and returns borrowed manager-owned pointers. Asset lifetime follows the engine runtime.

Path handling rejects rooted, UNC, device, drive-qualified, traversal, and URI-like inputs before canonicalization. Equivalent slash and dot-segment spellings resolve to one cache identity. Windows identity comparison folds ASCII case while preserving the normalized source spelling reported by metadata.

Texture source failures create lightweight path-specific aliases of the shared error texture. Each alias keeps pointer-specific metadata. A later successful retry installs real GPU data into the same alias. Allocation and renderer failures remain hard errors.

Read-only asset metadata includes:

- asset type;
- source path;
- display name;
- loaded or fallback state;
- short summary strings.

### Persistence

The persistence layer currently provides:

- a text `key=value` settings format;
- safe load and save helpers;
- a save-data model separate from settings;
- slot-path helpers under the user-data directory.

### UI

The UI layer provides:

- a lightweight UI context;
- frame begin/end flow;
- panel, label, button, toggle, tab, and status-chip primitives;
- hover and click state;
- built-in text rendering from engine-owned glyph source data;
- screen-space overlay drawing through the existing renderer;
- viewport-frame drawing for a docked Scene View region;
- mouse hover, press, and release handling for clickable controls;
- Sandbox workspace modes, utility views, and short in-window status feedback.

The public UI API does not expose OpenGL or SDL types.

### Gizmos

The transform-gizmo path is an engine-owned foundation. It provides:

- axis labels and snap helpers;
- viewport-aware gizmo hit testing;
- projected drag math for world-axis move and rotate;
- reusable scene-space mesh helpers for lines and rings.

The Sandbox creates scene entities that render as gizmo handles inside the dedicated viewport. Helper entities stay outside the normal sample-object list and picking rules.

### Scene

The scene layer is currently lightweight and provides:

- scene ownership;
- lightweight entity handles;
- entity enumeration and lookup for developer inspection;
- per-entity labels;
- visibility state;
- tags;
- transform, mesh, and material assignment;
- local bounds;
- interaction metadata;
- nearest-hit picking against simple bounds;
- one active scene camera;
- one directional light direction, color, intensity, and ambient color;
- per-object visibility and debug labels.

The current scene model is not a full ECS.

### Renderer

The renderer exposes engine-owned drawing functionality while OpenGL remains isolated to renderer implementation files.

#### OpenGL backend responsibilities

The current backend handles:

- context creation;
- viewport resize;
- shader compilation and linking;
- post-link shader contract validation;
- immutable per-program uniform-location tables;
- mesh upload;
- descriptor-aware texture upload and binding;
- tangent-space vertex attributes;
- normalized RGBA vertex-color attributes;
- bounded metallic-roughness material evaluation;
- depth testing;
- backface culling;
- wireframe toggle;
- scene viewport bounds;
- scene scissor and viewport state for docked workspaces;
- draw submission for scene entities;
- draw submission for screen-space UI rectangles.

Shader contract validation includes a minimal geometry variant and a complete material variant covering transforms, lighting, environment, fog, material factors, alpha, vertex color, textures, and shadows. Invalid external programs are rejected before publication.

Uniform-location tables are qualified by the active SDL OpenGL context and include explicit contract identity/version, source hash, and generation. Required locations are queried once during admission, stored in the owned table, and destroyed with the program. Draw-time required-uniform paths use the stored table. Built-in environment, shadow, tone-map, and viewport programs use the same mechanism.

Texture upload supports sRGB and linear internal formats, sampler policy, mip selection, and upload-state restoration. Tangents preserve validated imported model data when present. Mesh upload generates tangents for legacy or procedural data and uses finite fallbacks for degenerate UVs. Generated and OBJ geometry default vertex color to opaque white.

The material path evaluates base color, normal, metallic-roughness, validated dielectric specular factor/color and IOR, occlusion, emissive data, and clearcoat.

The renderer reserves explicit program identities for future environment, IBL, shadow, post, debug, and UI contracts.

### Sandbox

The Sandbox consumes public Henka APIs. It creates scenes, shaders, textures, meshes, materials, cameras, settings, and UI through public headers, then provides those objects to the engine run loop through callbacks.

The current workspace uses the UI layer for docked and native detached inspection, utility, diagnostics, transform, and physics surfaces. Core inspection surfaces include:

- `Controls`;
- `Scene Objects`;
- `Object Details`.

Current interaction behavior includes:

- camera movement and mouse look pause while the UI is open;
- mouse capture releases when the UI opens;
- `Escape` closes the UI before normal capture/exit flow resumes;
- the scene renders inside a dedicated docked viewport region;
- panels occupy separate docked regions;
- selected objects can show a transform gizmo;
- viewport-relative rays drive manipulation so docked panel clicks do not affect scene transforms.

The Sandbox remains an engine sample and QA target. It is not a complete production editor.

## Current boundaries

- Applications use public Henka headers.
- The engine owns the main loop, timing, scene pointer, renderer lifecycle, action bindings, package mode, and diagnostics snapshot.
- The engine owns the asset manager and fallback assets.
- Runtime assets resolve relative to the executable directory by default.
- Local user data resolves beside the executable by default.
- Packaged Sandbox runs therefore remain independent from the repository root.
- The engine can draw an optional UI context after the 3D scene.
- Sandbox object selection and details come from the scene plus Sandbox-owned descriptors.
- Saved scene files and editor-only data models are not used for that inspection path.
- The Sandbox does not include SDL, Windows, or OpenGL headers.
- OpenGL stays in renderer implementation files.
- Scene data remains public enough for engine consumers while renderer internals stay private.
- Real games are intended to live in separate repositories and consume Henka through its public boundaries.

## Near-term direction

Planned architecture work includes:

- safer camera orientation controls;
- broader material import;
- broader model loading beyond the current OBJ subset;
- stronger asset management;
- broader persistence and external-project workflows beyond the current local-first save and bounded consumer-validation paths;
- production-quality 2.5D sprites, regions, layered depth, sorting, parallax, animation, movement constraints, physics constraints, and authoring tools after current defect repair;
- richer engine UI controls built on the existing docked and native detached workspace;
- object inspection and transactional authoring that can grow without an editor rewrite;
- saved workspace placement, full detached controls, bounded title-bar drag-back recognition, and detachable Scene View;
- continued editable-authoring data and runtime-asset integration for broader modeling, UV, rigging, and animation workflows.

## Viewport shading

The Scene View owns an explicit shading mode with Wireframe, Solid, Material Preview, and Rendered policies.

### Mode behavior

**Wireframe** draws neutral geometry edges without texture sampling.

**Solid** draws neutral filled surfaces under a neutral editor surface policy and preserves explicit unlit line materials for the editor grid.

**Material Preview** uses the bounded Cook-Torrance material evaluation and Scene View-sized linear HDR-to-display presentation with deterministic editor lighting.

**Rendered** uses scene light policy, optional bounded scene fog, exposure, tone mapping, bloom, temporal history, fitted directional shadows, and bounded local-light shadows.

Material Preview and Rendered both use validated scene-owned environment controls for visible surroundings and diffuse/specular response. Rendered derives transactional IBL resources from the same environment texture.

### Rendered pipeline details

Rendered mode currently provides:

- camera- and object-motion history reprojection;
- two fitted directional shadow cascades;
- overlap blending at the directional cascade transition;
- a bounded depth map for the first enabled spot light;
- a bounded cubemap for the first enabled point light;
- built-in material-shader camera motion, object motion, and reactive attachments;
- presentation-pass depth rejection;
- history clamping;
- bounded reconstruction sharpening;
- observable temporal invalidation and fallback state;
- bounded back-to-front blending after opaque and masked geometry;
- deterministic entity-order overflow fallback for blended materials;
- unlit-material lighting bypass;
- separate helper-overlay materials;
- filled polygon-state restoration before UI and detached-window presentation.

Production TAA remains future work until its documented visual cases are validated. Reserved procedural materials remain unavailable until a production shader model exists. Mode changes preserve scene materials.

The Sandbox studio environment is generated as a linear periodic equirectangular source and validated before IBL ownership. This keeps the source continuous across its horizontal wrap.

### Directional shadow filtering

Directional shadow receiver filtering uses a bounded 3x3 PCF kernel and expands to 5x5 around a detected near-cascade blocker. Confirmed occlusion does not receive a minimum visibility floor, so contact shadows retain full occlusion.

Cascade selection uses interpolated forward view-space depth. The near/far overlap therefore remains stable across wide scenes. The fitted map ownership and bounded-shadow fallback policy remain unchanged.

### Legacy wireframe compatibility

The legacy wireframe API remains compatible:

- enabling it selects Wireframe;
- disabling it restores the last valid non-wireframe mode.

The Sandbox persists the authoritative mode under `ui.scene_view.shading_mode`. Older `wireframe_enabled` settings are migrated only when the new key is absent.
