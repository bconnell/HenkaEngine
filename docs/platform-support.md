# Platform Support

Henka Engine's validated development, test, packaging, and external-project path currently targets 64-bit Windows with MSVC and PowerShell 5.1-compatible scripts.

The C source is organized for broader portability, and SDL provides cross-platform foundations, but other operating systems are not currently claimed as validated targets. Non-Windows CMake configuration reports that status rather than presenting an unverified support claim.

Portable source changes should preserve standard C17 usage where practical and keep platform-specific behavior behind focused boundaries. A platform becomes supported only after its build, tests, runtime launch, and external-project path are exercised and documented.
