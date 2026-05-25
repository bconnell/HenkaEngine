# Henka Engine

Henka Engine is an early-stage open source engine written in C. The long-term direction is first-class 3D, 2D, and 2.5D support, with the current focus on building a solid 3D runtime foundation first. The engine now also includes a small local persistence layer and guidance for keeping real games in separate repositories.

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
- Local settings persistence for the sandbox
- Early in-window UI overlay with buttons, toggles, labels, and simple text rendering
- Scene Objects and Object Details panels for named sandbox object inspection
- Packaged sandbox user data that stays in place across package refreshes by default
- Generic documentation and starter template for external game repositories

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
templates/           Generic starter content for separate game repositories
third_party/         Bundled third-party source used by the engine
```

## Build

Windows build instructions are documented in [docs/building.md](docs/building.md).

Quick start from the repository root:

```powershell
.\scripts\build_windows.ps1
```

To create a run-ready Windows folder that you can open in Explorer and launch by double-clicking:

```powershell
.\scripts\package_sandbox3d_windows.ps1
```

## Run the sandbox

```powershell
.\scripts\run_sandbox3d.ps1
```

## Run the packaged sandbox

After packaging, open `out/HenkaSandbox3D/` in Explorer and double-click `HenkaSandbox3D.exe`.

You can also launch it from PowerShell:

```powershell
.\scripts\run_packaged_sandbox3d_windows.ps1
```

The sandbox starts a visible 3D scene with:

- a textured cube
- a textured ground plane
- a colored cube
- a loaded OBJ marker
- a debug grid
- a fallback-texture example for missing texture loads
- a fallback-model example for missing OBJ loads

Sandbox settings are saved locally in a `user/` folder beside the executable. In a packaged run, the settings file is `out/HenkaSandbox3D/user/sandbox3d.settings`.
The packaged folder also includes `PACKAGE_INFO.txt` so you can tell when the package was last refreshed.

### Sandbox controls

- `W A S D`: move across the scene
- `Q / E`: move down / up
- `Shift`: move faster
- `Mouse`: look around while mouse capture is active
- `Right Mouse / Tab`: toggle mouse capture
- `F1`: toggle wireframe
- `F2`: print the scene legend again
- `F3`: show or hide the debug grid
- `F4`: show or hide the sandbox panel
- `H`: print controls and the scene legend again
- `Escape`: close the UI first, then release the mouse, then exit

Press `F4` to open the in-window sandbox panels. On a first run with no local settings file, the packaged sandbox opens the panels automatically so the UI is immediately visible. The controls panel can toggle the grid and wireframe view, reset the camera, adjust mouse sensitivity and camera speed, save settings, reset sandbox settings, and print the same help and scene legend you can reach from the keyboard. The Scene Objects and Object Details panels let you select named scene examples, inspect what they demonstrate, toggle visibility, focus the camera, reset their transforms, and print object info to the console. Mouse look and camera movement pause while the UI is open.

Offline help is also available in [docs/help/sandbox3d.md](docs/help/sandbox3d.md).
Model loading notes are documented in [docs/model-loading.md](docs/model-loading.md).
A persistence overview is available in [docs/persistence.md](docs/persistence.md).
A UI overview is available in [docs/ui.md](docs/ui.md).
A guide for separate game repositories is available in [docs/external-game-projects.md](docs/external-game-projects.md).
A manual verification checklist is available in [docs/qa/sandbox3d-manual-checklist.md](docs/qa/sandbox3d-manual-checklist.md).
Packaged output is generated under `out/` and should not be committed.

For repeated packaged checks on Windows, use `.\scripts\check_packaged_sandbox3d_windows.ps1`.
To validate the generic external game template against the current Henka checkout, use `.\scripts\test_external_game_template_windows.ps1`.

## Run tests

```powershell
.\scripts\test_windows.ps1
```

## Current limitations

- The sandbox uses built-in primitives plus a small early OBJ loading path.
- Missing textures fall back safely to an error texture, and missing OBJ assets fall back to a visible mesh.
- OBJ support is intentionally limited to comments, blank lines, triangles, simple quads, positions, optional UVs, and optional normals.
- OBJ material libraries, negative indices, polygons with more than four vertices, and animation are not supported yet.
- The current settings format is a simple local key/value file. It is meant for engine samples and early projects, not for a finished save pipeline.
- Cloud saves, telemetry, analytics, registry storage, encryption, and network-backed persistence are not implemented.
- The in-window UI overlay is intentionally small. It now supports object inspection and safe scene actions, but it is still not a full editor or a general UI toolkit yet.
- 2D and 2.5D are part of the engine direction, but those workflows are not implemented yet.
- Visual and interaction checks still need manual QA on a local desktop session.
- HenkaSandbox3D is an engine sample and QA target, not a game. Real games built with Henka should live in separate repositories.

## License

Henka Engine is available under the [MIT License](LICENSE).
