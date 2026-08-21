# Scripting foundation

Henka currently exposes the first shared scripting seam through the public
`<henka/script.h>` API. This slice defines the language-neutral gameplay API
schema used by the Lua and HenkaScript backends, and provides bounded V1
execution adapters for both languages.

## What is available

- An immutable, versioned API schema with stable numeric IDs.
- Explicit domains for Entity, Transform, Input, Physics, Interaction, and
  Events.
- Fixed typed signatures with bounded parameter counts.
- Bind-time lookup by numeric ID or diagnostic name.
- A bounded Script Host that deduplicates resolved API bindings.
- A language-neutral bounded behavior runtime exposed through
  `<henka/script_runtime.h>` with generation-checked behavior handles,
  borrowed backend callbacks, deterministic Create/Start/Update/Stop state
  transitions, disabled/unbound/faulted states, and batch dispatch reports.
- Behavior callbacks receive the shared language, entity, lifecycle event,
  frame, delta-time, and instruction-budget context. Dispatch is synchronous
  and non-reentrant; runtime mutation from a callback is rejected.
- A bounded HenkaScript lexer/parser/type checker exposed through
  `<henka/henkascript.h>`.
- Bounded callable bytecode generation and an allocation-free typed VM with
  fixed stack storage, checked arithmetic, deterministic reports, and an
  instruction budget.
- A HenkaScript behavior adapter that maps the exact `OnCreate`, `OnStart`,
  `OnUpdate`, and `OnStop` callable names to the shared lifecycle runtime;
  absent lifecycle callables are deterministic no-ops and the runtime budget
  is propagated to the VM.
- A Lua 5.4.8 behavior adapter with the same lifecycle names and no-op rules,
  a restricted standard-library surface, a bounded allocator, and an
  instruction hook that fails closed when the per-callback budget is spent.
- Explicit typed declarations (`i32 health = 3;`) and inferred declarations
  (`count := health + 1;`).
- Brace-delimited `fn` and `behavior` callables, arithmetic expressions,
  assignments, returns, nested scopes, and source locations in diagnostics.
- Fixed token, binding, callable, AST-node, identifier, diagnostic, and source
  size limits. `let` and `var` are rejected rather than treated as alternate
  declaration semantics.
- Versioned Scene Document behavior attachments with stable IDs, enabled state,
  language identity, and confined `.lua`/`.hks` project-relative asset paths.
- A bounded Scene Document behavior asset loader that resolves those paths
  inside the project root, validates source size and language suffix, owns the
  selected backend, and returns a runtime descriptor for mixed-language
  lifecycle dispatch.
- A typed, synchronous Script Host dispatcher with exact schema argument and
  return validation, non-reentrant callback protection, and a bounded FIFO
  event queue.
- The isolated Sandbox Play session binds the host to persistent Scene
  Document IDs for Entity validity, Transform position access, Physics impulse
  application, and Events.Emit. Unsupported domains return safe deterministic
  results rather than reaching through renderer or authoring pointers.
- Lua behaviors can call the shared host surface through checked `Entity`,
  `Transform`, `Input`, `Physics`, `Interaction`, and `Events` tables. The
  HenkaScript V1 surface currently exposes the same event identity through the
  bounded `emit(i32_event_id);` builtin.

Backends should resolve IDs during load/compile and retain typed native
bindings. Names are for diagnostics and tooling; they are not the runtime
dispatch mechanism.

## What is not available yet

The HenkaScript compiler and VM currently execute bounded callable-local
bytecode, and both language adapters can drive their bounded execution through
the language-neutral behavior runtime. The bounded loader can create owned
backend instances from persisted `.lua` and `.hks` attachments, and the Scene
Document behavior runtime can assemble and dispatch them by persistent object
identity. The Sandbox Play session now owns that runtime for its isolated
scene lifecycle and fixed-tick dispatch, with a bounded host mapping for the
current Entity/Transform/Physics/Event slice. Persistent global state, full
host API coverage in both languages, Inspector authoring, hot reload, and
mixed-language event delivery/routing remain subsequent implementation
slices.

The current schema, HenkaScript compiler, and bounded VM are therefore engine
integration foundations, not a claim that complete executable scripting is
already available to end-user projects.
