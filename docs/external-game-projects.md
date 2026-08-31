# External Game Projects

Henka Engine is the engine repository. Games built with Henka should live in separate repositories and consume the engine through its public boundaries.

> **Current support:** Windows external game and dedicated-server templates provide bounded public-consumer validation. Complete project serialization and mature project tooling remain future work.

## Contents

- [Repository boundary](#repository-boundary)
- [External game template](#external-game-template)
- [Current public API coverage](#current-public-api-coverage)
- [Audio validation](#audio-validation)
- [Scripting validation](#scripting-validation)
- [Running the Windows validation](#running-the-windows-validation)
- [External server template](#external-server-template)
- [Suggested project layout](#suggested-project-layout)
- [Persistence](#persistence)
- [Action API](#action-api)
- [Windows dependency and path handling](#windows-dependency-and-path-handling)
- [Current limits](#current-limits)

## Repository boundary

### Henka Engine repository

The engine repository owns:

- engine code;
- engine-facing samples;
- generic runtime assets;
- Sandbox QA content;
- public documentation;
- starter templates for external projects.

### Game repository

A game repository should own:

- game-specific assets;
- story and dialogue;
- game-specific save data;
- project-specific scripts and tools;
- private or commercial content;
- project-specific scenes and configuration.

This separation keeps Henka generic and lets game projects maintain their own history, content policy, release process, and engine upgrade cadence.

## External game template

The current template lives at:

```text
templates/external_game_minimal/
```

Point an external project at a local Henka checkout with:

```powershell
cmake -S . -B build -DHENKA_ENGINE_DIR="C:/Path/To/HenkaEngine"
```

The template builds a real external executable using Henka's public API.

The consuming game owns its own window, scene, camera, assets, and presentation policy.

## Current public API coverage

The external game validation currently exercises several production boundaries.

### Authoring and scene

The template:

- creates a box authoring mesh from code;
- manipulates stable vertex, edge, and face identities;
- evaluates the authored source into an ordinary renderer mesh;
- saves and reloads the authored mesh;
- creates and picks a real scene entity;
- creates a linked physics box;
- verifies duplicate/delete of a user-owned entity;
- uses no Sandbox source for those operations.

The reloaded mesh is also handed to a real engine window, scene, camera, and Terrain render owner through public APIs.

### Rendered path

The graphical validation requires:

- a visible Rendered draw;
- valid HDR diagnostics;
- valid shadow diagnostics;
- the real external engine window and scene path.

### Terrain

The current template exercises the bounded public Terrain workflow, including material, edit, collision, render-data, save, restart, and graphical Rendered behavior covered by the validation executable.

Complete external scene/project serialization remains future work.

## Audio validation

The external template now validates the current Audio foundation through public Henka APIs.

The workflow:

1. creates a real external WAV asset;
2. loads the asset through the engine-owned asset manager;
3. loads a real metadata-first streamed PCM-WAV asset through the public Audio API;
4. creates a real scene entity;
5. attaches both Audio configurations to a Scene Document object;
6. saves the Scene Document;
7. reloads the Scene Document and verifies the Audio configurations;
8. creates runtime Audio emitters using manager-owned assets;
9. mixes deterministic production PCM;
10. moves the scene entity and verifies spatial left/right response;
11. moves the listener and verifies distance response;
12. destroys the entity and verifies stale-emitter cleanup.

A successful external run prints:

```text
External public Audio asset, real scene object, persistence, spatial movement, listener movement, and stale cleanup workflow passed.
```

This validates manager-owned Audio assets, real scene participation, persistence, spatial response, listener response, and stale cleanup from an external public-API consumer.

## Scripting validation

The template also consumes package-owned `.hks` and `.lua` assets through the public Scene Document behavior runtime.

Current external scripting coverage includes:

- shared input host calls;
- interaction host calls;
- physics host calls;
- HKS-to-Lua Henka event delivery;
- typed behavior-state delivery;
- public Scene Document behavior loading;
- no dependency on Sandbox source;
- no machine-global scripting installation requirement.

## Running the Windows validation

From the Henka Engine repository, run:

```powershell
.\scripts\test_external_game_template_windows.ps1
```

The validation script owns and reuses:

```text
build/tv/external_game_minimal/
```

It replaces the generated template source snapshot, reuses the corresponding build directory, and retires legacy timestamped `ext_YYYYMMDD_HHMMSS` trees.

The external target treats compiler warnings as errors on the supported compiler path.

## External server template

The renderer-free C17 server template lives at:

```text
templates/external_server_minimal/
```

It disables:

- graphical client;
- KTX;
- bundled examples;
- tests.

It links only `henka_runtime` while retaining the private Henka network transport required by the server consumer.

Run:

```powershell
.\scripts\test_external_server_template_windows.ps1
```

Use `-NoLocalProviders` to force the pinned ENet FetchContent path when repository-local dependency sources are absent.

The server validator reuses:

```text
build/tv/external_server_minimal/
```

## Suggested project layout

```text
your-game/
  assets/
  src/
  docs/
  user/
  CMakeLists.txt
  README.md
```

Local development settings can live under `user/`. Projects can adopt a broader save layout when their runtime save requirements mature.

## Persistence

External games can use Henka's current settings APIs for:

- graphics preferences;
- input preferences;
- camera defaults;
- prototype save flags.

The current settings format is local and human-readable.

Henka also has separate bounded save-data and Scene Document persistence foundations. Complete shipped-game save and full project serialization remain roadmap work.

## Action API

External projects can use Henka's local Action API for validated scene and object operations in tools and tests.

The current Action API has:

- no network listener;
- no cloud bridge;
- no arbitrary code-execution surface.

Scripting uses the separate bounded Script Host and behavior runtime.

The Action API supports deterministic local testing and validated scene-object tooling through engine-owned authority.

## Windows dependency and path handling

The external template applies local Visual Studio path hardening for nested FetchContent builds.

Machine-wide Git long-path and MSBuild file-tracking changes are not required by the template.

When no local source override is supplied, the pinned KTX-Software dependency uses the normal network-capable FetchContent path.

The repository validation script uses a short ignored build directory and accepts:

```powershell
-NoLocalProviders
```

Windows CI uses that switch to exercise the clean-clone dependency path.

Populated repository-local dependency sources remain available as optional offline acceleration.

## Current limits

Current external-project work still leaves these areas open:

- complete game-project serialization;
- mature project creation/open workflows;
- complete external scene editor workflow;
- complete runtime save-game system;
- broad cross-platform external-project validation;
- mature build/export presets;
- release packaging and distribution;
- additional gameplay systems beyond the current public foundations.

Platform plans and validation requirements are documented in [platform-support.md](platform-support.md). The broader project/tooling direction is maintained in [roadmap.md](roadmap.md).
