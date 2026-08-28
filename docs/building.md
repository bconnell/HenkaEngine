# Building Henka Engine

> **Current validated development path:** Windows 64-bit with Visual Studio 2022, MSVC, CMake, and PowerShell 5.1-compatible scripts.

This page covers normal development builds, tests, packaging, visual evidence, external-project validation, and headless/server builds.

## Contents

- [Requirements](#requirements)
- [Normal development build](#normal-development-build)
- [Renderer-free runtime build](#renderer-free-runtime-build)
- [Dedicated server build and package](#dedicated-server-build-and-package)
- [Tests](#tests)
- [Run the Sandbox](#run-the-sandbox)
- [Package the Sandbox](#package-the-sandbox)
- [Visual evidence](#visual-evidence)
- [Packaged validation](#packaged-validation)
- [External project validation](#external-project-validation)
- [Manual CMake commands](#manual-cmake-commands)
- [Generated output and runtime assets](#generated-output-and-runtime-assets)

## Requirements

Install or provide:

- Visual Studio 2022 with C and C++ build tools;
- CMake on `PATH` or available through the Visual Studio installation;
- network access during the first configure when pinned dependencies are not already cached locally.

Henka can use populated local dependency sources under `build/_deps/` as optional offline acceleration. Clean builds clear absent local-source overrides and use the pinned network-capable FetchContent path.

The top-level project applies bounded Visual Studio path hardening for clean-clone builds. The KTX dependency also enables Git long-path handling inside its own clean clone. Machine-wide Git or MSBuild path changes are not required for the normal supported path.

### MSVC runtime policy

MSVC multi-config builds use one DLL CRT policy across Henka and the pinned KTX-Software dependency:

| Configuration | CRT |
| --- | --- |
| Debug | `/MDd` |
| Release | `/MD` |

This keeps KTX application-owned buffers on the same Windows heap as Henka and its test consumers. Rebuild stale binaries after changing configuration or updating dependencies.

## Normal development build

From the repository root:

```powershell
.\scripts\build_windows.ps1
```

The script configures and builds into `build/`.

### Generated roots

`build/` and `out/` are generated output roots. They are not durable source.

Stable validation scratch roots include:

```text
build/tv/external_game_minimal/
build/tv/external_server_minimal/
out/terrain-process-integration/
```

The validation scripts reuse these roots and retire old generated validation trees. Inspect generated-output bounds with:

```powershell
.\scripts\check_generated_output_lifecycle_windows.ps1
```

The check fails when generated-tree growth violates the repository's bounded output policy.

## Renderer-free runtime build

The normal client build produces the graphical compatibility target `henka` and the Sandbox. The renderer-independent public runtime is `henka_runtime`.

Validate a runtime-only configuration with:

```powershell
cmake -S . -B out/headless `
  -DHENKA_BUILD_CLIENT=OFF `
  -DHENKA_BUILD_DEDICATED_SERVER=OFF `
  -DHENKA_BUILD_EXAMPLES=OFF `
  -DHENKA_ENABLE_KTX2_TRANSCODER=OFF

cmake --build out/headless --config Debug --target henka_headless_runtime_tests
ctest --test-dir out/headless -C Debug -R henka_headless_runtime_tests --output-on-failure
```

This path validates the renderer-free runtime boundary used by dedicated-server and headless consumers.

## Dedicated server build and package

The dedicated server is a renderer-free C17 network host. It includes bounded command-line/configuration input, fixed-tick physics, Terrain snapshot recovery, a bounded physics-resident Terrain collision rebuild path, loopback message handling, transactional smoke persistence, and graceful client shutdown.

ENet is fetched at the pinned commit recorded in [architecture.md](architecture.md). Its license is included at `third_party/licenses/enet.txt`.

### Create the deployment package

```powershell
.\scripts\package_dedicated_server_windows.ps1 -Configuration Release
.\scripts\check_packaged_dedicated_server_windows.ps1
```

The package is written to:

```text
out/HenkaDedicatedServer
```

It contains the renderer-free server, sample configuration, server documentation, provenance, and an operator-owned `save/` directory.

Graphical assets, SDL client runtime, OpenGL UI content, and KTX tooling are outside this package.

### External server template

`templates/external_server_minimal` is a C17 consumer that links only `henka_runtime`.

Validate it with:

```powershell
.\scripts\test_external_server_template_windows.ps1
```

Use `-NoLocalProviders` to force the normal pinned ENet FetchContent path.

### Terrain process integration

Run the bounded multi-process Terrain authority test with:

```powershell
.\scripts\test_terrain_process_integration_windows.ps1
```

The test launches the dedicated server and two independent runtime-only clients. It verifies accepted and stale edits, late-observer bootstrap, resident-region checksum convergence, reconnect behavior, server restart, and exact committed revision recovery.

Repeat the bounded scenario with fresh processes and isolated save roots:

```powershell
.\scripts\soak_terrain_process_integration_windows.ps1 -Iterations 3
```

The soak covers repeatability and cleanup for the current resident-region contract. Production-scale multiplayer capacity and relevance-driven multi-region orchestration remain future work.

## Tests

Run the normal Windows validation suite:

```powershell
.\scripts\test_windows.ps1
```

Run the exact Release path with:

```powershell
.\scripts\test_windows.ps1 -Configuration Release
```

The GitHub Windows workflow covers the packaged Debug contract and bounded soak, then runs the Release build-and-test path before the external-game template validation.

### Sanitized runtime gate

Run the first-party memory-safety gate with:

```powershell
.\scripts\test_sanitized_runtime_windows.ps1
```

The renderer-independent runtime and supported tests are built with `HENKA_ENABLE_SANITIZERS=ON`.

| Compiler | Sanitizer policy |
| --- | --- |
| MSVC | AddressSanitizer |
| Clang/GCC | AddressSanitizer + UndefinedBehaviorSanitizer |

Bundled third-party sources are not instrumented by this gate. Graphical, packaging, and external-project validation remain separate required checks.

## Run the Sandbox

```powershell
.\scripts\run_sandbox3d.ps1
```

The development run script launches the built Sandbox from its executable directory under `build/`.

## Package the Sandbox

Create a run-ready Windows Sandbox package with:

```powershell
.\scripts\package_sandbox3d_windows.ps1
```

The package is written under `out/HenkaSandbox3D/`.

### Package contents

The package includes:

- `HenkaSandbox3D.exe`;
- runtime assets under `assets/`;
- generated Giraffe and Rocket glTF scenes and sibling buffers under `assets/models/`;
- residency stress fixtures under `assets/textures/residency/`;
- `docs/help/sandbox3d.md`;
- `PACKAGE_INFO.txt`;
- `README.txt`;
- `user/` when local packaged settings already exist.

Required runtime DLLs are copied beside the executable when needed.

### Terrain stream stress mode

The packaged executable accepts:

```text
--terrain-stream-stress
```

The mode seeds or reuses the bounded Terrain fixture, verifies the active camera's one-region CPU/physics/render demand window, crosses into generated regions, returns to the original region, and checks bounded collision overlap on return.

This is a runtime streaming foundation check. Broad-world streaming and automatic background regeneration remain open work.

### Preserve or reset packaged settings

Normal packaging refreshes executable, assets, and offline help while preserving `out/HenkaSandbox3D/user/`.

Clear packaged Sandbox settings explicitly with:

```powershell
.\scripts\package_sandbox3d_windows.ps1 -ResetUserData
```

## Visual evidence

After a Debug build, capture the same camera in Solid, Material Preview, and Rendered modes:

```powershell
.\scripts\capture_visual_evidence_windows.ps1
```

Evidence is written under:

```text
build/visual_evidence/
```

The capture path uses deterministic framing after the final Scene View aspect is known. Capture runs leave normal user camera settings unchanged.

Rendered mode exercises scene lighting, shadows, HDR/IBL presentation, bloom, and temporal presentation. Material Preview uses the deterministic preview-light policy.

### Capture a packaged executable

```powershell
.\scripts\capture_visual_evidence_windows.ps1 `
    -Configuration Release `
    -ExecutablePath .\out\HenkaSandbox3D\HenkaSandbox3D.exe `
    -IncludeTerrain `
    -OutputDirectory .\build\visual_evidence\packaged-terrain
```

`-IncludeTerrain` captures the three application-only Terrain images plus deterministic material-close and four-region corner views. The automated guard covers packaged launch, Rendered-path distinction, non-flat terrain framing, and bounded viewport composition.

Human visual QA remains required for appearance, readability, topology, material quality, and presentation judgment.

## Packaged validation

### Launch the package

Use either:

```powershell
.\scripts\run_packaged_sandbox3d_windows.ps1
```

or open `out/HenkaSandbox3D` in Explorer and launch `HenkaSandbox3D.exe`.

A first packaged run with no settings file opens the stable `Standard` workspace shell. The Sandbox currently opens a console window. Normal interactive controls live in the in-window workspace.

### Bounded startup soak

```powershell
.\scripts\soak_packaged_sandbox3d_windows.ps1
```

The default run performs ten isolated startup iterations and requires the normal completion marker plus clean allocation shutdown on every iteration.

Hosted Windows CI may use `-AllowHeadlessUnavailable` for runners without an OpenGL-capable desktop video driver. Other process failures remain fatal. Local runs remain strict by default.

### Full packaged check

```powershell
.\scripts\check_packaged_sandbox3d_windows.ps1
```

The packaged check verifies:

- expected packaged files;
- generated glTF and editor-owned HAMS sources;
- startup help text and package marker;
- runtime-mode reporting when available;
- UI state logs when available;
- selected interaction checks;
- clean window-close shutdown;
- non-interactive Terrain streaming;
- texture residency;
- temporal presentation;
- environment stress paths.

The non-interactive path runs from the package root and validates relative asset ownership without repository-root assumptions.

Human desktop QA should still review viewport containment, dock readability, control spacing, utility readability, and overall visual behavior.

## External project validation

### External graphical game template

Validate `templates/external_game_minimal` with:

```powershell
.\scripts\test_external_game_template_windows.ps1
```

The validation uses the stable scratch root at `build/tv/external_game_minimal/` and configures it against the current Henka checkout.

Current public-API coverage includes:

- authoring mesh creation and stable topology identities;
- scene object creation and picking;
- linked physics;
- duplicate/delete behavior;
- Terrain material, editing, collision, render-data, persistence, and restart flow;
- a real graphical Rendered draw with HDR/shadow diagnostics;
- Lua and HenkaScript behavior assets through the public Scene Document runtime;
- shared input, interaction, physics, events, and typed state;
- a real external WAV asset loaded through the Henka asset manager;
- a real scene entity with persisted Audio emitter configuration;
- spatial source movement and listener movement through the production mixer;
- stale-entity Audio cleanup.

The external executable uses public Henka APIs and does not include Sandbox source.

See [External Game Projects](external-game-projects.md) for the consumer model and project layout.

## Manual CMake commands

A Developer PowerShell can run the direct build sequence:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug
```

When `cmake` is absent from `PATH`, use the executable supplied by the Visual Studio installation.

## Generated output and runtime assets

Sandbox runtime assets live under:

```text
assets/shaders/
assets/textures/
assets/models/
```

CMake copies `assets/` beside the Sandbox executable after build.

Packaged output under `out/` is generated locally and should not be committed. Package-local `user/` data is also generated locally.

Running:

```powershell
.\scripts\clean_windows.ps1
```

removes both `build/` and `out/`, including package-local Sandbox settings under `out/HenkaSandbox3D/user/`.
