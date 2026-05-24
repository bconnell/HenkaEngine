# Coding Standard

## Language and naming

- Use C17.
- Use the `henka_` prefix for public types and functions.
- Keep public headers in `engine/include/henka/`.

## Ownership

- Public APIs should document ownership where it matters.
- Match create and destroy functions clearly.
- Do not return borrowed memory without saying so.

## Error handling

- Use `henka_result` for structured failures.
- Validate arguments where practical.
- Do not allow silent failure paths.

## File organization

- Keep files focused on a single responsibility.
- Prefer small headers with explicit includes over tangled transitive dependencies.
- Remove dead code instead of parking it for later.

## Logging

- Log startup, shutdown, and failure paths.
- Prefer actionable messages over vague noise.

## Scope discipline

- Build the next useful layer without pretending later systems already exist.
- Avoid speculative abstractions that are not needed by the current code.
