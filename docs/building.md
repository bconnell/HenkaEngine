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
command-line/configuration input, fixed-tick physics servicing, Terrain world
storage/session initialization, a bounded physics-resident Terrain collision
rebuild path, loopback message handling, and graceful client shutdown. Full
packaged deployment and restart orchestration remain future work. ENet is
fetched at the pinned commit recorded in `docs/architecture.md`; its license
is included in `third_party/licenses/enet.txt`.

The C17 `templates/external_server_minimal` project links only
`henka_runtime`. Validate it with
`scripts/test_external_server_template_windows.ps1`; pass
`-NoLocalProviders` to verify the normal network-capable pinned ENet fallback.

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
- `out/HenkaSandbox3D/docs/help/sandbox3d.md`
- `out/HenkaSandbox3D/PACKAGE_INFO.txt`

## Capture same-camera shading evidence

After a Debug build, the application-only capture helper records the same
camera in Solid, Material Preview, and Rendered mode:

```powershell
.\scripts\capture_visual_evidence_windows.ps1
```

The PNGs and `INDEX.txt` are generated under `build/visual_evidence/`. Capture
mode runs do not save sandbox settings, so the camera and scene remain stable
and no user-profile file is changed by this evidence path. Rendered uses scene
lighting, the shadow path, transactional HDR/IBL presentation, bloom, and
temporal presentation; Material Preview uses its deterministic preview-light
policy and is not a substitute for the Rendered path.
- `out/HenkaSandbox3D/README.txt`
- `out/HenkaSandbox3D/user/` when local sandbox settings have already been created

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

This script copies `templates/external_game_minimal` into a repo-local validation folder under `build/`, configures it against the current Henka checkout, builds it, and runs a small smoke test.

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
