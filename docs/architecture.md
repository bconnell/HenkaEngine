# Architecture

Henka Engine is still compact, but it now has enough moving parts that the module boundaries matter. The current architecture is aimed at supporting a visible 3D rendering path without leaking SDL or OpenGL details into application code.

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

SDL types remain outside the public Henka headers.

### Input

The current input layer is still intentionally small, but it now tracks the state needed for:

- keyboard movement
- mouse delta
- mouse button toggles
- help and wireframe controls
- exit handling

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

The current camera module provides a perspective camera, simple fly movement, and clamped mouse look.

### Assets

The current asset layer is intentionally modest. It loads and caches shaders, textures, and OBJ meshes by path, resolves relative asset paths against the engine asset base directory, owns fallback textures and a fallback mesh, caches failed path lookups against those fallbacks, and keeps asset lifetime tied to the engine runtime.

### Persistence

The persistence layer is intentionally small and local-first. Right now it provides:

- a text `key=value` settings format
- safe load and save helpers

### UI

The current UI layer is intentionally small and dependency-conscious. Right now it provides:

- a lightweight UI context
- frame begin and end flow
- panel, label, button, toggle, tab, and status-chip primitives
- basic hover and click state
- simple built-in text rendering from engine-owned source code glyph data
- screen-space overlay drawing through the existing renderer
- mouse hover, press, and release handling for basic clickable controls
- sandbox workspace modes, utility views, and short in-window status feedback built on top of those primitives

It is meant to support engine samples and a viewport-first developer workspace without exposing OpenGL or SDL types in the public UI API.

### Scene

The scene layer is intentionally minimal. It is not a full ECS. Right now it provides:

- scene ownership
- lightweight entity handles
- entity enumeration and lookup for developer inspection
- per-entity labels and visibility state
- per-entity transform, mesh, and material assignment
- one active scene camera
- one directional light direction and ambient color
- per-object visibility and debug labels

### Renderer

The renderer layer exposes engine-owned drawing functionality while keeping OpenGL isolated to renderer implementation files. The current OpenGL backend handles:

- context creation
- viewport resize
- shader compilation and linking
- mesh upload
- texture upload and binding
- depth testing
- backface culling
- wireframe toggle
- draw submission for scene entities
- draw submission for simple screen-space UI rectangles

### Sandbox

The sandbox is a consumer of the public API only. It creates a scene, shaders, textures, meshes, materials, camera, settings object, and UI context through Henka headers, then hands those objects to the engine run loop through callbacks. It uses scene entity names and visibility state for the console legend, and it now uses the early UI layer for three small developer-facing panels without turning the sandbox into an editor:

- `Controls`
- `Scene Objects`
- `Object Details`

The current interaction rules pause camera movement and mouse look while the UI is open, release mouse capture when the UI opens, and let `Escape` close the UI before it resumes the normal capture and exit flow.

## Current boundaries

- Applications talk to the engine through the public Henka headers.
- The engine owns the main loop, timing, scene pointer, and renderer lifecycle.
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
- early 2.5D-friendly camera and layering rules after the shared runtime is steadier
- richer engine UI controls after the current lightweight overlay has settled
- object inspection that can grow into broader developer tooling without requiring an editor rewrite first
