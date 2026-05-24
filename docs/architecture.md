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

The current asset layer is intentionally modest. It loads and caches shaders, textures, and OBJ meshes by path, owns fallback textures and a fallback mesh, caches failed path lookups against those fallbacks, and keeps asset lifetime tied to the engine runtime.

### Scene

The scene layer is intentionally minimal. It is not a full ECS. Right now it provides:

- scene ownership
- lightweight entity handles
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

### Sandbox

The sandbox is a consumer of the public API only. It creates a scene, shaders, textures, meshes, materials, and camera through Henka headers, then hands those objects to the engine run loop through callbacks. It also uses scene entity names and visibility state to support console legends and manual verification without adding a separate overlay system.

## Current boundaries

- Applications talk to the engine through the public Henka headers.
- The engine owns the main loop, timing, scene pointer, and renderer lifecycle.
- The engine also owns the asset manager and fallback assets.
- The sandbox does not include SDL, Windows, or OpenGL headers.
- OpenGL stays in renderer implementation files.
- Scene data is public enough to build with, but renderer details stay private.

## Near-term direction

The next steps should continue building upward from these boundaries:

- safer camera orientation controls
- broader material import
- broader model loading beyond the current OBJ subset
- stronger asset management
- early 2.5D-friendly camera and layering rules after the shared runtime is steadier
