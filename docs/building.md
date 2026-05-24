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
- `out/HenkaSandbox3D/README.txt`

If any runtime DLLs are needed beside the executable, the package script copies them into the same folder.

## Launch the packaged sandbox

You can launch the packaged sandbox in either of these ways:

- open `out/HenkaSandbox3D` in Explorer and double-click `HenkaSandbox3D.exe`
- run `.\scripts\run_packaged_sandbox3d_windows.ps1`

The packaged sandbox does not rely on the repository root as its working directory. Assets resolve relative to the executable folder by default.

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
