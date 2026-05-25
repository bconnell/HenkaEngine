# Building

These instructions are currently focused on Windows.

## Requirements

- Visual Studio 2022 with C and C++ build tools installed
- CMake available either on `PATH` or through the Visual Studio installation
- Network access during the first configure step so CMake can fetch SDL3 locally

## Build from the repository root

```powershell
.\scripts\build_windows.ps1
```

The script configures and builds the project in `build/`.

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

On a first packaged run with no local settings file yet, the sandbox opens the in-window UI in `View` mode so the controls are visible without covering most of the viewport.
The packaged sandbox still opens a console window at this stage, but the in-window panels, utilities, and status area are the intended normal workflow.

## Run a packaged sandbox check

```powershell
.\scripts\check_packaged_sandbox3d_windows.ps1
```

The packaged check script confirms that the packaged folder contains the expected files, launches the sandbox, checks the startup help text and package marker, confirms UI state logs when available, exercises a few UI clicks, and confirms the close-window path exits cleanly.

It does not replace human visual QA. You should still confirm by eye that the layout leaves the scene comfortably visible in the packaged window, that the controls are not cramped, and that the in-window utilities and status area feel readable and useful.

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
