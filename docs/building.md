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

The run script changes into the sandbox executable directory before launching so relative shader asset paths resolve consistently.

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

CMake copies the `assets/` directory next to the sandbox executable after build.
