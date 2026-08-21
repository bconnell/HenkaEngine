<p align="center">
  <img src="assets/branding/henka_engine_lockup.png" alt="Henka Engine" width="360">
</p>

# Henka Engine

Henka Engine is an early-stage open-source game engine and integrated
development workspace written in C. It has a native 3D runtime/editor path,
terrain, rendering, physics, 2.5D camera foundations, modeling and content
authoring, asset/material workflows, persistence, and external-project support.

It is a real engine foundation, not a production-ready game platform. The
repository's visible Sandbox is an engine sample and QA target; games built
with Henka should live in separate repositories.

## Current project status

Integrated authoring is already underway alongside runtime and workspace
hardening. The current validated development and packaging path targets 64-bit
Windows with MSVC, CMake, PowerShell, SDL3, and the OpenGL renderer backend.
Other operating systems are not currently claimed as supported.

## Highlights

- C17 runtime architecture with renderer-independent `henka_runtime`
- Native editor/workspace with docked and detached tools
- Camera-driven Scene View Compass with snap, orbit, projection, and persisted preferences
- glTF/GLB scene and PBR material workflow plus bounded OBJ loading
- Integrated Object/Vertex/Edge/Face authoring with stable mesh-element identities
- Transactional topology operations, UV foundations, and bounded undo/redo
- Terrain streaming, editing, persistence, material layers, and collision ownership
- Rigid-body physics foundation with sandbox inspection
- Perspective, side, top-down, and isometric 2.5D camera foundations
- Headless/dedicated-server and external-project template foundations
- Provenanced, packaged Windows Sandbox builds

> **Support Henka Engine** — Development is supported through GitHub Sponsors.
> Use this repository's Sponsor button or read [SUPPORT.md](SUPPORT.md) for
> details.

## Capability matrix

| Area | Current status | Highlights |
| --- | --- | --- |
| Core runtime | Available | C17 APIs, bounded validation, generation-checked scene identities |
| Renderer | Available | OpenGL path, Solid/Material Preview/Rendered policies, PBR foundations |
| Scene and camera | Available | Entities, input actions, framing, gizmos, Compass, 2.5D presets |
| Editor workspace | Foundation | Docking, detached panels, tabs, layout persistence, early authoring UI |
| Modeling and authoring | In Progress | Object/Vertex/Edge/Face modes, topology operations, UV and HAMS foundations |
| Assets and materials | In Progress | glTF/GLB, OBJ, manager-owned dependencies, validated instances |
| Terrain and world | Foundation | Four-layer terrain, streaming, edits, LOD, persistence, collision paths |
| Physics | Foundation | Fixed-step rigid bodies, primitive colliders, contacts, events, raycasts |
| 2.5D | Foundation | Perspective/side/top/isometric camera workflows and orthographic zoom |
| Networking/server | Foundation | Renderer-free runtime, dedicated host, bounded Terrain authority paths |
| External projects | Foundation | Separate game/server templates with Windows validation |
| Game authoring | Foundation | Bounded Scene Document, authored Physics/Interaction, Save/Reload, and isolated runtime Play scenes |
| 2D | Planned | Dedicated 2D renderer, sprites, layers, parallax, and animation remain open |
| Audio | Planned | No current audio workflow |
| Scripting/behaviors | In Progress | Shared host schema, bounded HenkaScript/Lua lifecycle adapters; scene binding and host dispatch remain open |

For the detailed, code-backed inventory and explicit boundaries, see
[docs/current-capabilities.md](docs/current-capabilities.md).

## Editor and development workspace

The Sandbox provides a native workspace for Scene View, utilities, object
inspection, physics QA, materials, terrain, authoring, and layout tools. Panels
can be docked or detached, and the workspace has validated split topology,
tabs, named layout slots, bounded layout history, and reset-layout recovery.

The Compass is a viewport instrument rather than a separate application: it
tracks the active camera, supports axis snapping and orbit drag, and exposes
projection and info-strip controls. Detailed controls are in
[docs/help/sandbox3d.md](docs/help/sandbox3d.md) and
[docs/editor-controls.md](docs/editor-controls.md).

## Game-engine runtime

Henka is not only a modeling application. Its runtime includes scenes/entities,
camera and input actions, asset management, rendering, physics, terrain,
persistence, and a renderer-independent headless boundary. Full Game/Play
authoring, character controllers, audio, end-user scripting, and mature project
serialization are not yet available.

## Modeling and content authoring

The current integrated authoring foundation includes:

- Object, Vertex, Edge, and Face workflows
- Component selection, connected selection, bounded edge-loop selection, and soft movement
- Transform, orientation, pivot, and axis-constrained editing foundations
- Stable vertex/edge/face identities and connectivity queries
- Extrude, inset, bevel-ring, face subdivision, selected-face deletion, UV projection, packing, and undo/redo
- Native editable source persistence and imported-object Make Editable
- Validated material-region and supported PBR material-instance editing

Broader topology, automatic UV unwrap, texture painting, rigging, animation
authoring, and production-quality showcase asset creation remain in progress.
See [docs/authoring-mesh.md](docs/authoring-mesh.md) and
[docs/showcase-assets.md](docs/showcase-assets.md).

## Terrain, world, and 2.5D

Terrain provides bounded region persistence and streaming, four-layer material
data, resident render/physics owners, sculpt/paint commands, collision patches,
LOD transitions, and edit history. The current 2.5D foundation is camera-side:
Perspective, Side, Top-down, and Isometric presets with orthographic zoom.
Sprites, texture regions, layered depth, parallax, animation, and movement
constraints remain future work. See [docs/terrain.md](docs/terrain.md) and
[docs/roadmap.md](docs/roadmap.md).

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

The detailed build, dependency, headless, Release, and package instructions
are in [docs/building.md](docs/building.md).

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

The external server template links only `henka_runtime`. These templates are
bounded consumer validation paths, not complete game project serializers. See
[docs/external-game-projects.md](docs/external-game-projects.md).

## Documentation

- [Detailed current capabilities](docs/current-capabilities.md)
- [Architecture](docs/architecture.md)
- [Building and validation](docs/building.md)
- [Runtime foundations](docs/runtime-foundations.md)
- [UI and workspace](docs/ui.md)
- [Model loading](docs/model-loading.md)
- [Terrain](docs/terrain.md)
- [Physics](docs/physics.md)
- [Editor controls and Sandbox help](docs/editor-controls.md) · [offline help](docs/help/sandbox3d.md)
- [External game projects](docs/external-game-projects.md)
- [Showcase asset provenance](docs/showcase-assets.md)
- [Roadmap](docs/roadmap.md)
- [Branding](docs/branding.md)
- [Repository integrity](docs/repository-integrity.md)
- [Contributing](CONTRIBUTING.md)

## Current limitations

- Henka and its editor are early-stage; the native workspace is not a complete production editor.
- 2D, audio, scripting/behaviors, character controllers, advanced physics, broader renderer backends, and mature Game/Play workflows are unfinished.
- Scene/project serialization, hierarchy authoring, texture painting, automatic UV unwrap, rigging, animation, and several advanced topology tools remain open.
- The default Giraffe and Rocket are deterministic imported/generated fixtures and editor-owned dogfood derivatives, not proof of user-authored production assets.
- Automated evidence does not replace human visual QA for editor feel, detached windows, terrain corners, rendering, or modeling quality.

The detailed boundary inventory is maintained in
[docs/current-capabilities.md](docs/current-capabilities.md), with subsystem
contracts owned by the linked documentation above.

## Roadmap

Current priorities are runtime and editor integrity, integrated authoring,
terrain/world usability, renderer and asset hardening, and the next layers of
2.5D workflow. Longer-term work includes complete Game authoring, 2D,
animation, audio, scripting, additional renderer backends, and broader release
distribution. See the [roadmap](docs/roadmap.md) for the maintained direction.

## Support Henka Engine

Henka sponsorship supports development time, testing, documentation, examples,
packaged builds, asset workflow work, and future workspace/tooling work. Use the
[GitHub Sponsors page](https://github.com/sponsors/bconnell) or this repository's
Sponsor button.

Sponsorship is voluntary support. It does not purchase feature priority,
private support, guaranteed response times, ownership, project-direction
control, or a different license. Feature decisions remain based on stability,
maintainability, scope, and usefulness to the wider engine. See
[SUPPORT.md](SUPPORT.md) for the full terms and other ways to help.

## License

Henka Engine is available under the [MIT License](LICENSE).
