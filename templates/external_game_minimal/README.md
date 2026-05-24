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

## Template notes

- `src/main.c` is intentionally small.
- `assets/` is where your game-specific content can start.
- `.gitignore` ignores local build and user data output.
- This template is generic on purpose. It does not include story, characters, or game-specific content.
