# Henka Engine

Henka Engine is an early-stage open source engine written in C. The long-term direction is first-class 3D, 2D, and 2.5D support, with the current focus on building a solid 3D runtime foundation first.

## Current status

Henka Engine is still early, but the sandbox now renders a visible 3D scene with textured and untextured materials through Henka systems.

### What currently works

- C17 build through CMake
- `henka` static library target
- `henka_sandbox3d` example target
- `henka_tests` unit test target with CTest integration
- SDL3-backed platform layer hidden behind Henka headers
- OpenGL renderer backend isolated inside renderer implementation files
- Public math, time, camera, mesh, texture, shader, scene, and asset APIs
- Asset manager foundation for cached shader and texture loading
- Early OBJ model loading with cached mesh assets
- Fallback white and error textures
- Shader-based rendering of built-in primitives
- Sandbox window titled `Henka Engine Sandbox 3D`
- Ground plane, cubes, debug grid, a loaded OBJ marker, textured materials, and visible fallback behavior for missing texture and model assets
- Keyboard movement, mouse look when capture is active, wireframe toggle, and offline runtime help

### What does not exist yet

- Editor UI
- Drag and drop workflows
- Asset browser UI
- Physics
- Audio
- Scripting
- Full 2D renderer
- 2.5D workflow tooling
- Additional renderer backends

## Repository layout

```text
assets/              Runtime shader, texture, and model assets
engine/              Core library and public headers
examples/sandbox3d/  Visible 3D sandbox application
tests/               Headless unit tests
docs/                Architecture, build, roadmap, and help documents
scripts/             Windows helper scripts
third_party/         Bundled third-party source used by the engine
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

The sandbox starts a visible 3D scene with:

- a textured cube
- a textured ground plane
- a colored cube
- a loaded OBJ marker
- a debug grid
- a fallback-texture example for missing texture loads
- a fallback-model example for missing OBJ loads

### Sandbox controls

- `W A S D`: move across the scene
- `Q / E`: move down / up
- `Shift`: move faster
- `Mouse`: look around while mouse capture is active
- `Right Mouse / Tab`: toggle mouse capture
- `F1`: toggle wireframe
- `H`: print help again
- `Escape`: release the mouse first, then exit

Offline help is also available in [docs/help/sandbox3d.md](docs/help/sandbox3d.md).
Model loading notes are documented in [docs/model-loading.md](docs/model-loading.md).

## Run tests

```powershell
.\scripts\test_windows.ps1
```

## Current limitations

- The sandbox uses built-in primitives plus a small early OBJ loading path.
- Missing textures fall back safely to an error texture, and missing OBJ assets fall back to a visible mesh.
- OBJ support is intentionally limited and does not include MTL import or animation.
- There is no in-window text or overlay help yet.
- 2D and 2.5D are part of the engine direction, but those workflows are not implemented yet.
- Visual and interaction checks still need manual QA on a local desktop session.

## License

Henka Engine is available under the [MIT License](LICENSE).
