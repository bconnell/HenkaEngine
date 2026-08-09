# External Game Minimal Template

This template shows one small way to build a separate game project against a local Henka Engine checkout.

## What this template is for

Use this when you want to start a new game repository without putting game content inside the Henka Engine repo.

## Configure

Pass the path to your Henka Engine checkout with `HENKA_ENGINE_DIR`.

Example:

```powershell
cmake -S . -B build -DHENKA_ENGINE_DIR="C:/Path/To/HenkaEngine"
```

## Build

```powershell
cmake --build build --config Debug
```

## Run

```powershell
.\build\Debug\external_game_minimal.exe
```

The executable is also a bounded Terrain consumer smoke test. It uses only
public C17 APIs to validate the shared four-layer Terrain material contract,
deterministic raise and paint commands, collision raycast, CPU render-mesh
rebuild, transactional region save, and restart reload. A passing run prints
the Terrain workflow marker; it does not depend on Sandbox source.

## Template notes

- `src/main.c` is intentionally small and exercises a reusable runtime-facing
  Terrain workflow rather than being a graphical Sandbox copy.
- `assets/` is where your game-specific content can start.
- `.gitignore` ignores local build and user data output.
- The template turns off Henka example and test targets so your game build stays focused on the engine library plus your own project.
- This template is generic on purpose. It does not include story, characters, or game-specific content.
- The smoke path validates CPU render data. A graphical game should create its
  own `henka_engine`/`henka_scene` and pass its validated material to the public
  Terrain render owner; renderer-private types are not part of this template's
  contract.
