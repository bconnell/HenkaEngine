# Architecture

Henka Engine is intentionally small at this stage. The current architecture is about establishing boundaries that can survive future growth.

## Modules

### Core

The core layer owns shared engine behavior:

- result codes
- logging
- memory wrappers
- engine lifecycle

### Memory

The memory module wraps `malloc`, `calloc`, `realloc`, and `free`. In debug builds it tracks a simple active allocation count so shutdown can warn about likely leaks without introducing a custom allocator too early.

### Logging

Logging is synchronous console output with explicit severity, source file, and line number. It is used during startup, shutdown, and error handling to avoid silent failures.

### Platform

The platform layer currently uses SDL3 internally. It owns window creation, event polling, resize notifications, and window shutdown state. SDL types are kept out of Henka public headers.

### Input

The input layer is intentionally narrow for the first batch. It tracks a small key state table and the close request state needed for sandbox startup and exit behavior.

### Renderer

The renderer layer exposes a backend-agnostic entry point from the engine side and currently routes to an OpenGL implementation. The first backend handles:

- OpenGL context creation
- viewport resize
- clear and present
- VSync control

### Sandbox

The sandbox is a minimal consumer of the public API. It includes only `<henka/henka.h>`, creates an engine instance, runs it, and shuts it down cleanly.

## Current boundaries

- Applications talk to the engine through the public Henka headers.
- The engine coordinates platform, input, and renderer work.
- The renderer does not expose raw OpenGL objects publicly.
- SDL stays in the platform and renderer implementation files.

## Near-term direction

Once this foundation is stable, the next layers should build on these boundaries instead of bypassing them.
