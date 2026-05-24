# Henka Engine

Henka Engine is an early-stage open source engine written in C. The project direction is first-class 3D, 2D, and 2.5D support, with the current implementation focus on a strong 3D foundation before broader workflows are layered in.

2D and 2.5D are part of the long-term engine direction, but they are not being rushed in ahead of the renderer, input, asset, scene, and platform layers. The immediate priority is getting the core runtime, visible rendering path, and scene plumbing into a shape that can support later expansion cleanly.

## Current status

Henka Engine is still early. It now has a working visible 3D sandbox path, but it is not production-ready and it does not yet include the larger content, tooling, or gameplay systems people would expect from a mature engine.

### What currently works

- C17 build through CMake
- `henka` static library target
- `henka_sandbox3d` example target
- `henka_tests` unit test target with CTest integration
- SDL3-backed platform layer hidden behind Henka headers
- OpenGL renderer backend isolated inside renderer implementation files
- Public math, time, camera, mesh, shader, and scene APIs
- Shader-based rendering of built-in primitives through Henka systems
- Sandbox window titled `Henka Engine Sandbox 3D`
- Ground plane, cube, and debug grid scene content
- Depth-tested rendering
- Keyboard camera movement
- Engine startup and shutdown logging
- Offline sandbox runtime help through console output and docs

### What does not exist yet

- Texture loading
- Model loading
- Asset browser
- Drag and drop workflows
- Full ECS
- Physics
- Audio
- Scripting
- Editor UI
- Full 2D renderer
- 2.5D workflow tooling
- Additional renderer backends

## Repository layout

```text
assets/              Runtime shader assets
engine/              Core library and public headers
examples/sandbox3d/  Visible 3D sandbox application
tests/               Headless unit tests
docs/                Architecture, build, roadmap, and help documents
scripts/             Windows helper scripts
```

## Build

Windows build instructions are documented in [docs/building.md](docs/building.md).

Quick start from the repository root:

```powershell
.\scripts\build_windows.ps1
```

## Run the sandbox

```powershell
.\scripts\run_sandbox3d.ps1
```

The sandbox starts a visible 3D scene through Henka systems and prints its controls to the console at startup.

### Sandbox controls

- `W A S D`: move the camera on the ground plane
- `Q / E`: move down / up
- `Shift`: move faster
- `F1`: toggle wireframe
- `H`: print help again
- `Escape`: exit
- Window close: exit

Offline help is also available in [docs/help/sandbox3d.md](docs/help/sandbox3d.md).

## Run tests

```powershell
.\scripts\test_windows.ps1
```

## Current limitations

- Camera movement is keyboard-only right now. Mouse look is planned after the input layer is expanded more safely.
- The visible scene uses built-in primitives and local shader files only.
- There is no in-window text or overlay help yet.
- 2D and 2.5D are planned engine directions, but the corresponding workflows are not implemented yet.
- Validation currently focuses on build stability, headless tests, and startup behavior. Visual and interaction checks still need manual QA.

## License

Henka Engine is available under the [MIT License](LICENSE).
