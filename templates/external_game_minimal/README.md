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

The executable is also a bounded public authoring and Terrain consumer smoke
test. It creates a box authoring mesh, manipulates stable vertex, edge, and
face identities, evaluates the mesh into a normal renderer mesh, saves and
reloads the authored source, creates and picks a scene entity, creates a linked
physics box, verifies duplicate/delete of a user-owned entity, and validates
an independent public runtime-scene clone whose entity handles remain valid
while authored transforms stay unchanged. The same reloaded mesh is then
handed to the graphical scene alongside the public
Terrain render owner. The run uses only public C17 APIs, validates the shared
four-layer Terrain material contract, deterministic raise and paint commands,
collision raycast, CPU render-mesh rebuild, transactional region save, and
restart reload, then requires a visible Rendered draw with HDR and shadow
diagnostics. It also loads package-owned `.hks` and `.lua` assets through the
public Scene Document behavior runtime and proves shared input, interaction,
and physics host calls, an HKS-to-Lua Henka event, and typed state delivery.
It does not depend on Sandbox source or a machine-global scripting
installation. The Audio workflow creates a real external WAV asset, loads it
through the engine-owned asset manager, attaches it to a real scene entity,
saves and reloads its authored Audio configuration, verifies object and
listener spatial movement through the deterministic mixer, and confirms
stale-entity cleanup. It uses only public Henka APIs.

## Template notes

- `src/main.c` is intentionally small and exercises a reusable runtime-facing
  Terrain workflow rather than being a graphical Sandbox copy.
- `assets/` is where your game-specific content can start.
- `.gitignore` ignores local build and user data output.
- The template turns off Henka example and test targets so your game build stays focused on the engine library plus your own project.
- This template is generic on purpose. It does not include story, characters, or game-specific content.
- The graphical smoke uses only `henka_engine`, `henka_scene`, and the public
  Terrain render owner; renderer-private types are not part of this template's
  contract. CMake copies the generic shader fixtures from the selected engine
  checkout beside the validation executable so the smoke is self-contained.
