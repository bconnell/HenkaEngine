# Platform Support

> **Current validated target:** 64-bit Windows with MSVC, CMake, and PowerShell 5.1-compatible scripts.

Henka is being structured for broader portability, but platform support is claimed only after the relevant build, test, runtime, packaging, and external-project paths are exercised and documented.

## Platform direction

| Platform | Current status | Intended direction |
| --- | --- | --- |
| **Windows 64-bit** | Validated development target | Remain the primary supported desktop platform while renderer/backend options expand. |
| **Linux 64-bit** | Planned | Add a validated native build, test, runtime, packaging, and external-project path after the shared platform boundaries are mature enough to support it cleanly. |
| **macOS** | Planned future target | Add native macOS support after the portable runtime/platform boundary and renderer abstraction are ready for a Metal-oriented path. |

Android, iOS, consoles, and browser/WebAssembly builds are not currently claimed as committed platform targets. They may be evaluated later only when the engine architecture, tooling, and project priorities make those ports credible.

## Renderer/backend direction by platform

This table describes intended backend direction, not current support claims.

| Platform | Current / intended renderer direction |
| --- | --- |
| **Windows** | OpenGL is the current production backend. Future backend isolation should allow Vulkan and Direct3D 12 without changing engine-level rendering contracts. |
| **Linux** | Vulkan is the preferred future modern backend, with OpenGL retained where practical as a compatibility path. |
| **macOS** | Metal is the intended native modern backend. Any OpenGL path should be treated as legacy compatibility rather than the long-term renderer direction; a portability layer such as MoltenVK may be evaluated if it proves useful without weakening the native abstraction. |

The renderer selector should ultimately be capability-driven rather than novelty-driven. A platform/backend combination should appear as supported only after the real production path is validated.

## Current portability foundations

The C source is organized for broader portability, and SDL provides cross-platform foundations, but non-Windows operating systems are not currently claimed as validated targets. Non-Windows CMake configuration reports that status instead of presenting an unverified support claim.

Portable source changes should:

- preserve standard C17 usage where practical;
- keep platform-specific behavior behind focused boundaries;
- avoid leaking Windows-only assumptions into renderer-independent runtime code;
- keep asset, scene, Audio, input, persistence, and gameplay contracts independent of a specific desktop OS where practical;
- preserve external-project compatibility as new platforms are introduced.

## Validation required before a platform is called supported

A platform should not move from **Planned** to a supported status until Henka has exercised and documented, as applicable:

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

Windows CI currently uses a deterministic packaged startup smoke test that does not move the mouse, send keys, or depend on desktop focus. The full packaged UI interaction check remains available for local desktop validation.

The governing rule is simple: portable architecture is not the same as validated platform support.
