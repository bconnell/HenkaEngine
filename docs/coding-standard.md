# Coding Standard

## Language and naming

- Use C17.
- Use the `henka_` prefix for public types and functions.
- Keep public headers in `engine/include/henka/`.

## Ownership

- Public APIs should document ownership where it matters.
- Match create and destroy functions clearly.
- Do not return borrowed memory without saying so.
- Keep renderer-owned resources explicit. Meshes and shaders should have obvious lifecycle calls.
- Make shared asset ownership obvious when an asset manager returns cached resources.

## Error handling

- Use `henka_result` for structured failures.
- Validate arguments where practical.
- Do not allow silent failure paths.
- Log meaningful failure information for startup, asset, platform, and renderer problems.

## File organization

- Keep files focused on a single responsibility.
- Prefer small headers with explicit includes over tangled transitive dependencies.
- Remove dead code instead of parking it for later.
- Do not add future-only stubs that are not wired into the engine or tests.

## Rendering boundaries

- Application code should use Henka APIs, not raw OpenGL.
- Keep SDL out of public headers.
- Keep OpenGL in renderer implementation files.
- Keep texture upload and binding behind the renderer abstraction.

## User-facing text

- Write runtime help, README usage text, and visible error messages for the person using the engine or sandbox.
- Explain what is visible, what controls are available, what the user can try, and what is currently limited.
- Keep internal implementation or workflow language out of user-facing text.

## Scope discipline

- Build the next useful layer without pretending later systems already exist.
- Avoid speculative abstractions that are not needed by the current code.
- Prefer a small real scene path over a large unfinished framework.
