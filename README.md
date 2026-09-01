<p align="center">
  <img src="assets/branding/henka_engine_lockup.png" alt="Henka Engine" width="360">
</p>

# Henka Engine

Henka Engine is an early-stage open-source C17 game engine and integrated
development workspace. It has a native 3D runtime/editor path, terrain,
rendering, physics, Audio foundations, 2.5D camera foundations, modeling and
content authoring, asset/material workflows, persistence, and external-project
support.

> **Project status:** Henka is an early-stage engine foundation. Production game-platform maturity is in progress. The repository's Sandbox is the engine sample and QA target. Games built with Henka live in separate repositories.

## At a glance

| Area | Current direction |
| --- | --- |
| Language | C17 |
| Primary validated platform | 64-bit Windows |
| Planned desktop platforms | Linux 64-bit, then macOS |
| Current renderer backend | OpenGL |
| Future renderer direction | Vulkan / Direct3D 12 / Metal through backend isolation |
| Editor | Native integrated workspace |
| Game project boundary | Separate external projects supported through validated templates |
| License | MIT |

Integrated authoring is underway alongside runtime and workspace hardening. The
current validated development and packaging path targets 64-bit Windows with
MSVC, CMake, PowerShell, SDL3, and the OpenGL renderer backend. Linux 64-bit and
macOS are planned desktop targets. Platform support requires build, test,
runtime, packaging, and external-project validation.

## Highlights

- C17 runtime architecture with renderer-independent `henka_runtime`
- Native editor/workspace with docked and detached tools
- Camera-driven Scene View Compass with snap, orbit, projection, and persisted preferences
- glTF/GLB scene and PBR material workflow plus bounded OBJ loading
- Integrated Object/Vertex/Edge/Face authoring with stable mesh-element identities
- Transactional topology operations, UV foundations, and bounded undo/redo
- Terrain streaming, editing, persistence, material layers, and collision ownership
- Rigid-body physics foundation with Sandbox inspection
- Bounded game-audio foundation with buses, spatial emitters, scene/Play integration, SDL3 output, and external public-API validation
- Perspective, side, top-down, and isometric 2.5D camera foundations
- Headless/dedicated-server and external-project template foundations
- Provenanced, packaged Windows Sandbox builds

> **Support Henka Engine** — Development is supported through GitHub Sponsors.
> Use this repository's Sponsor button or read [SUPPORT.md](SUPPORT.md) for
> details.

## Capability matrix

| Area | Current status | Supported scope / highlights |
| --- | --- | --- |
| Core runtime | Available | Current C17 runtime APIs, bounded validation, and generation-checked scene identities |
| Renderer | Available (Unhardened) | Current OpenGL path, Solid/Material Preview/Rendered policies, and PBR foundations |
| Scene and camera | Available (Unhardened) | Current 3D entities, input actions, framing, gizmos, Compass, and 2.5D camera presets |
| Editor workspace | In Progress | Current native docking, detached panels, tabs, layout persistence, and early authoring UI |
| Modeling and authoring | In Progress | Current direct Object/Vertex/Edge/Face modes, bounded topology operations, UV, and HAMS foundations |
| Assets and materials | In Progress | Current glTF/GLB and OBJ loading, manager-owned dependencies, and validated instances |
| Terrain and world | Foundation | Current bounded four-layer terrain, streaming, edits, LOD, persistence, and collision paths |
| Physics | Foundation | Current fixed-step rigid bodies, primitive colliders, contacts, events, and raycasts |
| 2.5D | Foundation | Camera-side foundation: perspective/side/top/isometric workflows and orthographic zoom |
| Networking/server | Foundation | Current renderer-free runtime, dedicated host, and bounded Terrain authority paths |
| External projects | Foundation | Current separate game/server templates with Windows validation |
| Game authoring | Foundation | Current bounded Scene Document, authored Physics/Interaction, runtime hierarchy foundation, Save/Reload, and isolated runtime Play scenes |
| 2D | Planned | No dedicated 2D scope yet; renderer, sprites, layers, parallax, and animation remain open |
| Audio | Available | Bounded resident/streamed PCM WAV, Ogg Vorbis, MP3, and FLAC playback, fixed voices, bus gains, entity spatialization, deterministic stereo PCM mixing, authored listener/editor controls, supported Lua/HenkaScript controls, and caller-pumped SDL3 output/recovery |
| Scripting/behaviors | In Progress | Current bounded HenkaScript/Lua lifecycle adapters, Scene Document binding, Play dispatch, persistence, and cross-language events |

### Status meanings

| Status | Meaning |
| --- | --- |
| **Foundation** | Core architecture exists; the category remains incomplete. |
| **In Progress** | Substantial implementation exists; major functionality remains. |
| **Available (Unhardened)** | The functional category is present; hardening or validation remains. |
| **Available** | The category is functionally complete and hardened for its stated scope. |
| **Planned** | Meaningful implementation has not yet begun. |

The maintained status ownership and authoritative detailed sections are recorded
in [docs/capability-statuses.tsv](docs/capability-statuses.tsv) and
[docs/current-capabilities.md](docs/current-capabilities.md).

## Editor and development workspace

The Sandbox provides a native workspace for Scene View, utilities, object
inspection, physics QA, materials, terrain, authoring, and layout tools.
The Compass is an integrated viewport instrument that tracks the active camera,
supports axis snapping and orbit drag, and exposes
projection and info-strip controls. Detailed controls are in
[docs/help/sandbox3d.md](docs/help/sandbox3d.md) and
[docs/editor-controls.md](docs/editor-controls.md).

## Game-engine runtime

Henka is not only a modeling application. Its runtime includes scenes/entities,
camera and input actions, asset management, rendering, physics, terrain,
persistence, and a renderer-independent headless boundary. Full Game/Play
authoring, full character-controller movement, end-user scripting, and mature project
serialization are not yet available. Audio provides a bounded headless runtime,
graphical Sandbox client output, camera/listener integration, editor authoring,
persistence, and supported scripting controls.

## Modeling and content authoring

The current integrated authoring foundation includes:

- Object, Vertex, Edge, and Face workflows
- Component selection, connected selection, bounded edge-loop selection, and soft movement
- Transform, orientation, pivot, and axis-constrained editing foundations
- Stable vertex/edge/face identities and connectivity queries
- Face winding flip, extrude, inset, bevel-ring, face subdivision, selected-face deletion, UV projection, packing, and undo/redo
- Native editable source persistence and imported-object Make Editable
- Validated material-region and supported PBR material-instance editing

Broader topology, automatic UV unwrap, texture painting, rigging, animation
authoring, and production-quality showcase asset creation remain in progress.
See [docs/authoring-mesh.md](docs/authoring-mesh.md) and
[docs/showcase-assets.md](docs/showcase-assets.md).

## Terrain, world, and 2.5D

Terrain provides bounded region persistence and streaming, four-layer material
data, resident render/physics owners, sculpt/paint commands, collision patches,
LOD transitions, and edit history.

The current 2.5D foundation is camera-side:

- Perspective
- Side
- Top-down
- Isometric
- Orthographic zoom

Sprites, texture regions, layered depth, parallax, animation, and movement
constraints remain future work. See [docs/terrain.md](docs/terrain.md) and
[docs/roadmap.md](docs/roadmap.md).

## Platform direction

The current validated platform is 64-bit Windows.

Planned desktop support includes:

- Linux 64-bit with a validated native build, test, runtime, packaging, and external-project path;
- macOS after the portable runtime/platform boundary and renderer abstraction are ready for a Metal-oriented path.

Renderer backend direction is:

- Windows: OpenGL today, with future Vulkan and Direct3D 12 support;
- Linux: Vulkan as the preferred modern backend, with OpenGL retained where practical;
- macOS: Metal as the intended native modern backend.

See [docs/platform-support.md](docs/platform-support.md) for the platform validation contract.

## Getting started

### Prerequisites

- 64-bit Windows
- Visual Studio with C/C++ build tools
- CMake
- Network access on the first configure unless the pinned dependencies are already cached

### Build and test

```powershell
git clone <repository-url>
cd HenkaEngine
.\scripts\build_windows.ps1 -Configuration Debug
.\scripts\test_windows.ps1 -Configuration Debug
```

The detailed build, dependency, headless, Release, and package instructions are
in [docs/building.md](docs/building.md).

### Run the Sandbox

```powershell
.\scripts\run_sandbox3d.ps1 -Configuration Debug
```

### Create and run a package

```powershell
.\scripts\package_sandbox3d_windows.ps1 -Configuration Debug
.\scripts\run_packaged_sandbox3d_windows.ps1
```

The run-ready folder is `out/HenkaSandbox3D/`. It contains the executable,
assets, offline help, package identity, and local user settings when present.
Packaging and recovery rules are documented in
[docs/package-provenance.md](docs/package-provenance.md).

## Using Henka from another project

Henka keeps engine code and samples separate from actual games. Put game-specific
assets, scenes, saves, scripts, and private content in another repository. The
starter projects under `templates/` demonstrate the public runtime boundary:

```powershell
cmake -S . -B build -DHENKA_ENGINE_DIR="C:/Path/To/HenkaEngine"
.\scripts\test_external_game_template_windows.ps1
```

The external server template links only `henka_runtime`. The external game
template exercises bounded public runtime and authoring paths, including the
current public Audio workflow. Complete game project serialization remains future work. See
[docs/external-game-projects.md](docs/external-game-projects.md).

## Documentation

### Start here

- [Detailed current capabilities](docs/current-capabilities.md)
- [Roadmap](docs/roadmap.md)
- [Architecture](docs/architecture.md)
- [Building and validation](docs/building.md)
- [Documentation presentation standard](docs/documentation-style.md)
- [Platform support](docs/platform-support.md)

### Subsystems and workflows

- [Runtime foundations](docs/runtime-foundations.md)
- [UI and workspace](docs/ui.md)
- [Model loading](docs/model-loading.md)
- [Terrain](docs/terrain.md)
- [Physics](docs/physics.md)
- [Audio runtime](docs/audio.md)
- [Editor controls and Sandbox help](docs/editor-controls.md) · [offline help](docs/help/sandbox3d.md)
- [External game projects](docs/external-game-projects.md)
- [Showcase asset provenance](docs/showcase-assets.md)
- [Rendering realism](docs/realism.md)
- [Branding](docs/branding.md)
- [Repository integrity](docs/repository-integrity.md)
- [Contributing](CONTRIBUTING.md)

## Current limitations

The supported scope for each capability row is stated in its matrix cell above.
Current status applies to that stated scope. Open work inside the stated scope
continues to affect status.

- Henka and its editor are early-stage; the native workspace is not a complete production editor.
- 2D, broader scripting/behavior authoring, full character-controller movement,
  advanced physics, broader renderer backends, mature Game/Play workflows, and
  advanced audio effects/occlusion remain unfinished. A bounded dynamic-body
  character-controller foundation is available; capsule, sweep, slope, and
  step-offset behavior remain open.
- Scene/project serialization, hierarchy authoring, texture painting, automatic UV unwrap, rigging, animation, and several advanced topology tools remain open.
- The default Giraffe and Rocket are deterministic imported/generated sample
  assets and editor-owned derivatives used to exercise current import, material,
  editing, persistence, packaging, and rendering workflows. They do not establish
  complete arbitrary production-asset authoring coverage.
- Editor feel, detached windows, terrain corners, rendering, and modeling quality
  remain active quality areas.

The detailed boundary inventory is maintained in
[docs/current-capabilities.md](docs/current-capabilities.md), with subsystem
contracts owned by the linked documentation above.

## Roadmap

Current priorities are runtime and editor integrity, integrated authoring,
terrain/world usability, renderer and asset hardening, and the next layers of
2.5D workflow. Longer-term work includes complete Game authoring, 2D,
animation, advanced audio expansion, scripting, additional renderer backends, and broader release
distribution. See the [roadmap](docs/roadmap.md) for the maintained direction.

## Support Henka Engine

Henka sponsorship supports development time, testing, documentation, examples,
packaged builds, asset workflow work, and future workspace/tooling work. Use the
[GitHub Sponsors page](https://github.com/sponsors/bconnell) or this repository's
Sponsor button.

Sponsorship is voluntary support. Feature priority, private support, guaranteed
response times, ownership, project-direction control, and alternate licensing
are outside sponsorship terms. Feature decisions remain based on stability,
maintainability, scope, and usefulness to the wider engine. See
[SUPPORT.md](SUPPORT.md) for the full terms and other ways to help.

## License

Henka Engine is available under the [MIT License](LICENSE).
