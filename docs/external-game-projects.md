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

To validate that template against the current Henka checkout from this repository, run:

```powershell
.\scripts\test_external_game_template_windows.ps1
```

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
