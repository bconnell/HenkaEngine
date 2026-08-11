# Building

These instructions are currently focused on Windows.

## Requirements

- Visual Studio 2022 with C and C++ build tools installed
- CMake available either on `PATH` or through the Visual Studio installation
- Network access during the first configure step so CMake can fetch the pinned
  SDL3 and KTX-Software sources. A populated `build/_deps/sdl3-src` or
  `build/_deps/ktxsoftware-src` directory is optional offline acceleration;
  clean builds clear absent local-source overrides and use the pinned network
fallback instead. The clean KTX clone enables Git long-path handling
locally for Windows, so the normal external-template build does not require
a machine-wide Git or MSBuild setting or a populated dependency folder. The
top-level project applies the same bounded Visual Studio path hardening to
ordinary clean-clone builds.

## Build from the repository root

```powershell
.\scripts\build_windows.ps1
```

The script configures and builds the project in `build/`.

`build/` and `out/` are generated roots, not durable source. The external game
and server template checks reuse the stable scratch roots
`build/tv/external_game_minimal/` and `build/tv/external_server_minimal/`; they
retire older timestamped validation roots and never copy a generated validation
tree back into its own source. The Terrain process integration check similarly
reuses `out/terrain-process-integration/` and keeps only the latest bounded
session evidence. Use `scripts/check_generated_output_lifecycle_windows.ps1`
to inspect these bounds; it fails before an abnormal generated tree can be
mistaken for normal validation output.

The normal client build keeps the graphical compatibility target `henka` and
the sandbox enabled. A renderer-free runtime-only configuration is also
validated independently:

```powershell
cmake -S . -B out/headless -DHENKA_BUILD_CLIENT=OFF -DHENKA_BUILD_DEDICATED_SERVER=OFF -DHENKA_BUILD_EXAMPLES=OFF -DHENKA_ENABLE_KTX2_TRANSCODER=OFF
cmake --build out/headless --config Debug --target henka_headless_runtime_tests
ctest --test-dir out/headless -C Debug -R henka_headless_runtime_tests --output-on-failure
```

`henka_runtime` is the public static library for renderer-independent
consumers. `henka` links it underneath the existing graphical API. The
dedicated-server executable is a headless network host with bounded
command-line/configuration input, fixed-tick physics servicing, Terrain
snapshot recovery, a bounded physics-resident Terrain collision rebuild path,
loopback message handling, transactional smoke persistence, and graceful
client shutdown. A headless deployment package and restart-persistence check
are available; relevance-driven late-join orchestration and broad multiplayer
soak remain future work. ENet is fetched at the pinned commit recorded in
`docs/architecture.md`; its license is included in `third_party/licenses/enet.txt`.

For a developer-owned deployment package, run:

```powershell
.\scripts\package_dedicated_server_windows.ps1 -Configuration Release
.\scripts\check_packaged_dedicated_server_windows.ps1
```

The package is written to `out/HenkaDedicatedServer` and contains the
renderer-free server, sample configuration, server documentation, provenance,
and an operator-owned `save/` directory. It does not include graphical
assets, SDL, OpenGL, UI files, or KTX tooling. The package check launches the
server without a window, proves a local loopback client and bind, and runs the
same save root twice to verify committed Terrain revision recovery. Developers
may deploy that package on another development PC, a physical server, or a VPS
they control; Henka does not provide a hosted service.

The C17 `templates/external_server_minimal` project links only
`henka_runtime`. Validate it with
`scripts/test_external_server_template_windows.ps1`; pass
`-NoLocalProviders` to verify the normal network-capable pinned ENet fallback.

Run `scripts/test_terrain_process_integration_windows.ps1` for the bounded
multi-process Terrain authority check. It launches the dedicated server and two
independent runtime-only clients, proves one accepted and one stale edit, adds
a late observer and compares its resident-region checksum with the accepted
client, reconnects another client after an accepted edit, and restarts the
server against the same save root to verify the committed revision and checksum
are restored exactly. The check remains bounded session-info/relevance coverage;
it is not application authentication or a production multiplayer soak.

Repeat that complete bounded scenario for a finite number of isolated sessions
with:

```powershell
.\scripts\soak_terrain_process_integration_windows.ps1 -Iterations 3
```

Each iteration starts fresh server/client processes and resets the owned stable
save root before use. This proves repeatable cleanup and restart recovery for
the advertised resident region contract without retaining one full save tree
per iteration; it does not claim relevance-driven multi-region orchestration or
production-scale multiplayer capacity.

## Run tests

```powershell
.\scripts\test_windows.ps1
```

## Run the sandbox

```powershell
.\scripts\run_sandbox3d.ps1
```

The development run script launches the built sandbox from the executable directory inside `build/`.

## Package a run-ready sandbox folder

```powershell
.\scripts\package_sandbox3d_windows.ps1
```

The package script creates:

- `out/HenkaSandbox3D/HenkaSandbox3D.exe`
- `out/HenkaSandbox3D/assets/`
- `out/HenkaSandbox3D/assets/models/` with the generated Cheeky Giraffe and Original Realistic Rocket glTF scenes and sibling binary buffers
- `out/HenkaSandbox3D/assets/textures/residency/` with the bounded residency
  stress fixtures used by `--residency-stress`
- `out/HenkaSandbox3D/docs/help/sandbox3d.md`
- `out/HenkaSandbox3D/PACKAGE_INFO.txt`
- `out/HenkaSandbox3D/README.txt`
- `out/HenkaSandbox3D/user/` when local sandbox settings have already been created

The packaged executable also accepts `--terrain-stream-stress`. It uses the
same public Sandbox path to seed the bounded 2x2 fixture, prove the active
camera's one-region CPU/physics/render demand window, cross into generated
regions, and return to the original rendered region and collision patch while
reporting request failures and resident-region capacity; collision validation
uses the bounded overlap patch rather than claiming residency-wide coverage.
This is a runtime streaming foundation check, not a claim of broad-world
streaming or automatic background regeneration.

## Capture same-camera shading evidence

After a Debug build, the application-only capture helper records the same
camera in Solid, Material Preview, and Rendered mode:

```powershell
.\scripts\capture_visual_evidence_windows.ps1
```

The PNGs and `INDEX.txt` are generated under `build/visual_evidence/`. Capture
mode runs use one deterministic two-model showcase camera for all three modes and
do not save sandbox settings, so ordinary user camera state is unchanged and no
user-profile file is changed by this evidence path. Rendered uses scene
lighting, the shadow path, transactional HDR/IBL presentation, bloom, and
temporal presentation; Material Preview uses its deterministic preview-light
policy and is not a substitute for the Rendered path.

The same helper can target the packaged executable without changing its
working directory or settings:

```powershell
.\scripts\capture_visual_evidence_windows.ps1 `
    -Configuration Release `
    -ExecutablePath .\out\HenkaSandbox3D\HenkaSandbox3D.exe `
    -IncludeTerrain `
    -OutputDirectory .\build\visual_evidence\packaged-terrain
```

With `-IncludeTerrain`, the helper validates the three application-only
Terrain images with the bounded Scene View guard. This proves packaged launch
and Rendered-path distinction; it remains automated evidence rather than human
visual approval or complete four-way corner validation.

If any runtime DLLs are needed beside the executable, the package script copies them into the same folder.

By default, packaging refreshes the executable, assets, and offline help while keeping `out/HenkaSandbox3D/user/` in place. That preserves local sandbox settings across repackaging.

To intentionally clear the packaged sandbox settings:

```powershell
.\scripts\package_sandbox3d_windows.ps1 -ResetUserData
```

## Launch the packaged sandbox

You can launch the packaged sandbox in either of these ways:

- open `out/HenkaSandbox3D` in Explorer and double-click `HenkaSandbox3D.exe`
- run `.\scripts\run_packaged_sandbox3d_windows.ps1`

On a first packaged run with no local settings file yet, the sandbox opens the docked workspace in `View` mode so the controls are visible without covering most of the viewport.
The packaged sandbox still opens a console window at this stage, but the in-window panels, utilities, and status area are the intended normal workflow.
At startup, the sandbox also reports whether it detected `Development` or `Packaged` runtime mode.

## Run a bounded packaged soak

After packaging, the repeatable smoke/closure check runs ten isolated startup
iterations by default and requires both the normal completion marker and the
engine's clean allocation-shutdown report on every iteration:

```powershell
.\scripts\soak_packaged_sandbox3d_windows.ps1
```

Use `-Iterations` for a longer bounded run. This check does not save sandbox
settings or modify personal files. Hosted Windows CI passes
`-AllowHeadlessUnavailable`: if the runner has no OpenGL-capable desktop video
driver, that specific infrastructure limitation is recorded as a skip; other
process failures remain fatal. Local runs remain strict by default.

## Run a packaged sandbox check

```powershell
.\scripts\check_packaged_sandbox3d_windows.ps1
```

The packaged check script confirms that the packaged folder contains the expected files, launches the sandbox, checks the startup help text and package marker, confirms UI state logs when available, exercises a few UI clicks, and confirms the close-window path exits cleanly.
It also checks the packaged runtime marker and runtime-mode output when that signal is available.

It does not replace human visual QA. You should still confirm by eye that the scene stays inside its own viewport, that docked panels do not cover scene graphics, that the controls are not cramped, and that the in-window utilities and status area feel readable and useful.

## Validate the external game template

```powershell
.\scripts\test_external_game_template_windows.ps1
```

This script copies `templates/external_game_minimal` into the stable repo-local
validation scratch root under `build/tv/external_game_minimal/`, configures it
against the current Henka checkout, builds it, copies generic shader fixtures
beside the executable, and runs the public-API Terrain consumer smokes. The
test covers the Terrain material contract, shared edits, collision raycast, CPU
render-mesh rebuild, transactional persistence, restart reload, and a
graphical Rendered draw with HDR/shadow diagnostics. Repeated validation reuses
that root instead of creating a complete timestamped nested build each time.

The packaged sandbox does not rely on the repository root as its working directory. Assets resolve relative to the executable folder by default.

Sandbox settings are also written relative to the executable folder by default. In a packaged run, the default settings file is:

- `out/HenkaSandbox3D/user/sandbox3d.settings`

## Manual CMake commands

If you prefer direct commands, use a Developer PowerShell or let the scripts locate the Visual Studio CMake install:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug
```

If `cmake` is not on `PATH`, use the full path from the Visual Studio installation.

## Runtime assets

The sandbox runtime assets live under:

- `assets/shaders/`
- `assets/textures/`
- `assets/models/`

CMake copies the `assets/` directory next to the sandbox executable after build.

Packaged output in `out/` is generated locally and should not be committed.

The packaged sandbox `user/` folder is also generated locally and should not be committed.

If you run `.\scripts\clean_windows.ps1`, it removes the generated `out/` folder as well as `build/`. That also removes any package-local sandbox settings stored under `out/HenkaSandbox3D/user/`.
