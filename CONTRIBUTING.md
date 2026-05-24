# Contributing

Henka Engine is in an early foundation phase. Contributions should favor clarity, scoped changes, and honest documentation over feature sprawl.

## Ground rules

- Build from the repository root with CMake.
- Keep the public API small and explicit.
- Use the `henka_` prefix for public symbols.
- Match every create function with a destroy function.
- Validate inputs and return structured errors instead of failing silently.
- Keep files focused. If a source file starts carrying multiple responsibilities, split it.
- Update docs when behavior or project status changes.
- Write user-facing text for the person using the engine or sandbox, not for the developer implementing the feature.

## Suggested workflow

1. Create a branch for the change.
2. Build the project.
3. Run the tests.
4. Run the sandbox if the change affects startup, platform, input, or rendering.
5. Keep commits descriptive and scoped to the change.

## Before opening a pull request

- Confirm generated files and local build output are not staged.
- Make sure README and docs still describe the current state accurately.
- Call out known limitations rather than glossing over them.
- Check runtime help and visible error messages for clarity and tone.
