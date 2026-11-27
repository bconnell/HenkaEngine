# Platform Support

> **Current validated target:** 64-bit Windows with MSVC, CMake, and PowerShell 5.1-compatible scripts.

Henka is being structured for broader portability. Platform support requires documented build, test, runtime, packaging, and external-project validation.

## Platform direction

| Platform | Current status | Intended direction |
| --- | --- | --- |
| **Windows 64-bit** | Validated development target | Primary supported desktop platform while renderer/backend options expand. |
| **Linux 64-bit** | Planned | Native build, test, runtime, packaging, and external-project validation after shared platform boundaries reach the required maturity. |
| **macOS** | Planned future target | Native macOS support after the portable runtime/platform boundary and renderer abstraction are ready for a Metal-oriented path. |

Future platform evaluation candidates include Android, iOS, consoles, and browser/WebAssembly builds. Their commitment status remains open.

## Renderer/backend direction by platform

| Platform | Current and intended renderer direction |
| --- | --- |
| **Windows** | OpenGL is the current production backend. Future backend isolation should allow Vulkan and Direct3D 12 while preserving engine-level rendering contracts. |
| **Linux** | Vulkan is the preferred future modern backend. OpenGL remains a compatibility path where practical. |
| **macOS** | Metal is the intended native modern backend. OpenGL is legacy on macOS. MoltenVK remains an evaluation option for portability and maintenance. |

Renderer selection should be capability-driven. Supported platform/backend combinations require validated production paths.

## Current portability foundations

The C source is organized for broader portability, and SDL provides cross-platform foundations. Windows 64-bit is the current validated operating-system target. Non-Windows CMake configuration reports the current support status.

Portable source changes should:

- preserve standard C17 usage where practical;
- keep platform-specific behavior behind focused boundaries;
- keep Windows-specific assumptions out of renderer-independent runtime code;
- keep asset, scene, Audio, input, persistence, and gameplay contracts independent of a specific desktop OS where practical;
- preserve external-project compatibility as new platforms are introduced.

## Platform support acceptance

Supported status requires exercised and documented coverage for the applicable areas:

1. configure and build;
2. unit and integration tests;
3. runtime startup and shutdown;
4. editor/workspace launch for graphical targets;
5. renderer initialization and representative rendering;
6. input and window lifecycle;
7. Audio device lifecycle;
8. persistence and confined paths;
9. packaged execution;
10. external game/server project consumption;
11. dependency acquisition or documented offline/cached behavior;
12. platform-specific failure and recovery paths.

The hosted Windows workflow runs Debug tests and builds a packaged Sandbox,
checks its package contract, exercises packaged startup/Game Authoring and a
visible native-modeling path, checks bounded smoke progress, runs a bounded
packaged soak, validates the external game and server templates, and checks
generated-output lifecycle behavior. Separate Windows jobs run Release tests
and the first-party sanitizer runtime gate. The workflow prepares the pinned
software OpenGL fallback for its graphical jobs.

These are bounded automated functional checks on Windows 2022. They do not
replace human visual review, broad editor-usability evaluation, non-100% OS
display-scale acceptance, or validation on Linux, macOS, and mobile platforms.

Validated platform support requires an exercised production path.
