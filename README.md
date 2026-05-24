# Henka Engine

Henka Engine is an early-stage open source engine written in C. The current goal is a clean 3D foundation with clear module boundaries, predictable ownership, and a small public API that can grow without dragging early mistakes forward.

2D support is planned later, after the renderer, input, asset, scene, and platform layers are stable enough to support it without turning the codebase into a tangle.

## Current status

This repository is still in the first foundation phase. It is buildable, testable, and honest about how much is not here yet.

### What currently works

- C17 build through CMake
- `henka` static library target
- `henka_sandbox3d` example target
- `henka_tests` unit test target with CTest integration
- SDL3-backed platform layer hidden behind Henka headers
- OpenGL renderer backend that creates a context, clears a frame, handles resize, and presents
- Engine startup and shutdown logging
- Escape key exit handling
- Window close exit handling
- Basic memory wrapper coverage

### What does not exist yet

- Math library
- Shader management
- Meshes, textures, materials, and model loading
- Asset manager
- Scene graph or ECS
- Camera controls
- Audio
- Scripting
- Physics
- Editor tooling
- Additional renderer backends
- 2D renderer

## Repository layout

```text
engine/              Core library and public headers
examples/sandbox3d/  Minimal sandbox application
tests/               Basic unit tests
docs/                Architecture, build, roadmap, and coding notes
scripts/             Windows helper scripts
```

## Build

Windows instructions are documented in [docs/building.md](docs/building.md).

Quick start from the repository root:

```powershell
.\scripts\build_windows.ps1
```

## Run the sandbox

```powershell
.\scripts\run_sandbox3d.ps1
```

The sandbox opens a window titled `Henka Engine Sandbox 3D`, clears the frame to a dark color, logs startup and shutdown, and exits on `Escape` or window close.

## Run tests

```powershell
.\scripts\test_windows.ps1
```

## Current limitations

- The current renderer only clears and presents a frame.
- The current input layer only supports the small set of key state needed for the foundation batch.
- Validation is focused on build, startup, shutdown, and basic core behavior rather than graphics correctness.
- The build depends on fetching SDL3 as a local CMake dependency.

## License

Henka Engine is available under the [MIT License](LICENSE).
