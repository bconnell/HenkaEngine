# Henka Engine

Henka Engine is an early-stage open source engine written in C. The long-term direction is first-class 3D, 2D, and 2.5D support, with the current focus on building a solid 3D runtime foundation first. The engine now also includes a small local persistence layer and guidance for keeping real games in separate repositories.

## Support Henka Engine

Henka Engine is an open source C engine project focused on practical 3D, 2D, and 2.5D development foundations.

Sponsorship helps support development time, testing, documentation, examples, packaged builds, and future workspace tooling. The project will remain open source under its current license.

Sponsors help the project move forward, but sponsorship does not purchase feature priority, private support, ownership, or a different license. Feature decisions are still based on project direction, stability, maintainability, and usefulness to the wider engine.

Use the Sponsor button on this repository to support the project through GitHub Sponsors.

See SUPPORT.md for what sponsorship supports and what it does not promise.

## Current status

Henka Engine is still early, but the sandbox now renders a visible 3D scene with textured and untextured materials through Henka systems.

### What currently exists

- C17 build through CMake
- `henka` static library target
- `henka_sandbox3d` example target
- `henka_tests` unit test target with CTest integration
- SDL3-backed platform layer hidden behind Henka headers
- OpenGL renderer backend isolated inside renderer implementation files
- Public math, time, camera, mesh, texture, shader, scene, and asset APIs
- Input action foundation for named engine-level controls
- Scene object metadata, bounds, and interaction foundation
- Reusable camera helpers for reset, focus, and screen-ray creation
- Local action-command foundation for validated scene and object operations
- Asset metadata and stronger material summaries
- Local save-data foundation beyond settings
- Package mode and engine diagnostics foundation
- Shared overlay-handle transform gizmo foundation for selected object manipulation, with visual feel still being hardened through manual QA
- Viewport interaction test helpers for reducing manual QA around selection, gizmo hit testing, and transform changes
- Asset manager foundation for cached shader and texture loading
- Early OBJ model loading with cached mesh assets
- Fallback white and error textures
- Shader-based rendering of built-in primitives
- Sandbox window titled `Henka Engine Sandbox 3D`
- Ground plane, cubes, debug grid, a loaded OBJ marker, textured materials, and visible fallback behavior for missing texture and model assets
- Keyboard movement, mouse look when capture is active, wireframe toggle, and offline runtime help
- Local settings persistence for the sandbox
- Early in-window UI overlay with buttons, toggles, labels, structured rows, simple text rendering, and release-confirm control activation
- Scene Objects, Object Details, and Utility panels for named sandbox object inspection and viewer workflows
- Sandbox workspace panels with stacked side docks, header drag, cross-zone redocking, native detached-window panels with routed mouse input, safer tool-window renderer context recovery, dock splitter, and reset-layout recovery controls
- Multi-window platform foundation with a separate `Native Panel Test` window for close, focus, resize, and event-routing QA
- Rigid-body physics v1 with fixed-step worlds, static/dynamic/kinematic bodies, sphere/AABB/plane colliders, triggers, events, raycasts, and sandbox debug controls
- Packaged sandbox user data that stays in place across package refreshes by default
- Generic documentation and starter template for external game repositories

### What does not exist yet

- Editor UI
- Asset browser UI
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

## Try the sandbox

On Windows, the quickest way to try the current sandbox is:

```powershell
.\scripts\build_windows.ps1
.\scripts\package_sandbox3d_windows.ps1
```

The sandbox is an engine sample and QA target. It is not a game, and real games built with Henka should live in separate repositories.

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
- `Left Mouse`: uses the active viewport tool when mouse capture is released
- `Alt + Left Mouse`: optional orbit shortcut around the selected object or current view target
- `Middle Mouse`: optional pan shortcut
- `Mouse Wheel`: zoom the viewport when the cursor is over the scene view
- `F1`: toggle wireframe
- `F2`: print the scene legend again
- `F3`: show or hide the debug grid
- `F4`: show or hide the sandbox panels
- `F5`: cycle View, Inspect, and Full Tools layouts
- `F`: frame the selected object
- `H`: print controls and the scene legend again
- `Home`: reset the camera view
- `M` or `G`: move the selected object
- `R`: rotate the selected object
- `S`: scale the selected object
- `X`, `Y`, or `Z`: constrain an active transform
- `Enter` or `Left Mouse`: confirm an active transform
- `Escape` or `Right Mouse`: cancel an active transform
- `Left Ctrl` / `Left Shift`: stepped or fine transform adjustment
- `Escape`: when no transform is active, close the UI first, then release the mouse, then exit

The sandbox panels open automatically on startup and reset-style launches so Controls and `Physics QA` are discoverable without knowing `F4` first. Starts have no selected scene object until the user selects one. Press `F4` to hide or show panels, and press `F5` to cycle between `View`, `Inspect`, and `Full Tools`. UI buttons, toggles, tabs, and selectable rows activate on mouse release inside the active control so press, drag-away, and release behavior is safer. `DRAG` marks a live panel header. Release on a valid left or right dock outline to redock there, or release away from the outlines to move the panel into a separate native tool window. Detached workspace panels now render their matching panel content in native tool windows and route per-window mouse input for release-confirm controls. When two panels share a side, the dock stacks them vertically instead of letting one cover the other. Closing a detached tool window returns its panel to the last valid dock, and `Reset Layout` restores the default workspace. Renderer context recovery is hardened around tool-window drawing. Select a scene object, then use `M` or `G`, `R`, and `S` to start move, rotate, and scale transforms. Active transforms support `X`, `Y`, or `Z` constraints, confirm, cancel, stepped adjustment, and fine adjustment through the action-based local control profile. Open `Physics QA` to inspect the opt-in rigid-body demo. Manual desktop QA is still required before physics feel, native window behavior, panel drag comfort, and transform workflow feel can be called fully complete.

Offline help is also available in [docs/help/sandbox3d.md](docs/help/sandbox3d.md).
Model loading notes are documented in [docs/model-loading.md](docs/model-loading.md).
A persistence overview is available in [docs/persistence.md](docs/persistence.md).
A local action-command overview is available in [docs/action-api.md](docs/action-api.md).
A rigid-body physics overview is available in [docs/physics.md](docs/physics.md).
An editor controls overview is available in [docs/editor-controls.md](docs/editor-controls.md).
A runtime foundation overview is available in [docs/runtime-foundations.md](docs/runtime-foundations.md).
A UI overview is available in [docs/ui.md](docs/ui.md).
A guide for separate game repositories is available in [docs/external-game-projects.md](docs/external-game-projects.md).
A manual verification checklist is available in [docs/qa/sandbox3d-manual-checklist.md](docs/qa/sandbox3d-manual-checklist.md).
[Support Henka Engine](SUPPORT.md)
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
- A small save-data foundation now exists for local scene id, camera pose, and simple flags, but it is still intentionally modest.
- Remote saves, usage collection, registry storage, encryption, and network-backed persistence are not implemented.
- The in-window UI overlay is intentionally small. It now supports object inspection, utility views, release-confirm controls, and short status feedback, but it is still not a full editor or a general UI toolkit yet.
- Detached panel placement and resized dock widths are session-only; `Reset Layout` is the recovery path.
- Production tool panels can detach into separate OS-level windows with routed mouse input and safe close-to-redock recovery. Drag-back docking from OS title bars, saved detached placement, and detachable Scene View remain future work.
- Viewport transform hotkeys use local action profiles. The current profile editor is config-based; a richer in-window controls editor remains future work.
- The viewport transform gizmo is intentionally scoped to world-axis move, rotate, and scale behavior for the current sandbox object model. The sandbox now also exposes explicit viewport tool modes, diagnostics, and direct transform fallback controls so interaction failures can be diagnosed without assuming the gizmo is the only path.
- Rigid-body physics v1 supports static, dynamic, and kinematic bodies with sphere, axis-aligned box, and plane colliders; mesh collision, constraints, controllers, and advanced simulation remain future work.
- 2D and 2.5D are part of the engine direction, but those workflows are not implemented yet.
- Visual and interaction checks still need manual QA on a local desktop session.
- HenkaSandbox3D is an engine sample and QA target, not a game. Real games built with Henka should live in separate repositories.

## License

Henka Engine is available under the [MIT License](LICENSE).
