# External Game Projects

Henka Engine is the engine repository. Real games built with Henka should live in separate repositories.

## Why keep games separate

Keeping engine code and game code in separate repositories helps you:

- keep the engine public and generic
- keep private or commercial game content out of the engine repo
- manage game-specific assets, saves, and story files on their own terms
- upgrade Henka without mixing unrelated engine work into game history

## What belongs in Henka Engine

This repository is the right place for:

- engine code
- engine-facing samples
- generic runtime assets
- sandbox QA content
- public documentation
- starter templates for separate projects

## What belongs in your game repository

A separate game repository should own:

- game-specific assets
- story and dialogue
- game-specific save data
- project-specific scripts and tools
- private or commercial content
- anything that should not ship as a generic engine sample

## Using Henka from an external game

Right now the simplest approach is to point your game project at a local Henka checkout with a CMake variable such as:

```powershell
cmake -S . -B build -DHENKA_ENGINE_DIR="C:/Path/To/HenkaEngine"
```

The template under `templates/external_game_minimal/` shows one way to do that.
Its executable is a bounded public-API Terrain consumer smoke test: it
validates the shared four-layer material contract, deterministic raise/paint,
collision raycast, CPU render-mesh rebuild, transactional save, and restart
reload without including Sandbox source. It then creates a real engine window,
scene, camera, and Terrain render owner through public APIs, runs the normal
Rendered path, and requires a visible draw plus HDR/shadow diagnostics. The
consuming game still owns its window, scene, camera, and presentation policy;
the template proves that Terrain can cross that public graphical boundary.

To validate that template against the current Henka checkout from this repository, run:

```powershell
.\scripts\test_external_game_template_windows.ps1
```

The validation script owns and reuses `build/tv/external_game_minimal/` as a
bounded scratch tree. It replaces only the generated template source snapshot,
reuses the corresponding build directory, and retires legacy timestamped
`ext_YYYYMMDD_HHMMSS` trees. The server template follows the same policy under
`build/tv/external_server_minimal/`. Repeated checks therefore do not create a
new complete nested engine build on every run.

## Using the external server template

`templates/external_server_minimal/` is the renderer-free C17 counterpart. It
disables the graphical client, KTX, bundled examples, and tests, then links
only `henka_runtime`. It still enables the private Henka network transport so
the consumer validates the same headless server dependency boundary. Run
`scripts/test_external_server_template_windows.ps1` for a fresh configure,
build, and initialization smoke test; `-NoLocalProviders` forces the pinned
ENet FetchContent fallback when repository-local dependency sources are absent.

## Suggested external project layout

```text
your-game/
  assets/
  src/
  docs/
  user/
  CMakeLists.txt
  README.md
```

You can keep local settings in a `user/` folder during development, or choose a different policy once your game needs a broader save strategy.

## Using the current persistence layer

External games can reuse Henka's small settings API for:

- graphics preferences
- input preferences
- camera defaults
- prototype save flags

The current format is local-only and human-readable. It is a good fit for early project settings, but it is not a full save pipeline yet.

## Using the current action foundation

External game repositories can also use Henka's local Action API for validated scene and object operations in tools or tests.

That API is intentionally local-only:

- no network listener
- no cloud bridge
- no scripting runtime
- no arbitrary code execution

It is useful for deterministic local testing, basic scene-object workflows, and future editor-style tool surfaces that need validated requests instead of direct unchecked mutation.

On Windows, the template also applies local Visual Studio path-hardening for
nested FetchContent builds. It does not require machine-wide Git long-path or
MSBuild file-tracking settings; the engine's pinned KTX-Software source still
comes from the normal network-capable FetchContent path when no local source
override is supplied. The repository validation script uses a fresh short
ignored build folder and accepts `-NoLocalProviders` to force this clean-clone
path; Windows CI uses that switch. Without it, populated repository-local
sources remain optional offline acceleration only.
