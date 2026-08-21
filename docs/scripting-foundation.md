# Scripting foundation

Henka currently exposes the first shared scripting seam through the public
`<henka/script.h>` API. This slice defines the language-neutral gameplay API
schema used by the Lua and HenkaScript backends, and adds the bounded
HenkaScript V1 front-end.

## What is available

- An immutable, versioned API schema with stable numeric IDs.
- Explicit domains for Entity, Transform, Input, Physics, Interaction, and
  Events.
- Fixed typed signatures with bounded parameter counts.
- Bind-time lookup by numeric ID or diagnostic name.
- A bounded Script Host that deduplicates resolved API bindings.
- A bounded HenkaScript lexer/parser/type checker exposed through
  `<henka/henkascript.h>`.
- Bounded callable bytecode generation and an allocation-free typed VM with
  fixed stack storage, checked arithmetic, deterministic reports, and an
  instruction budget.
- Explicit typed declarations (`i32 health = 3;`) and inferred declarations
  (`count := health + 1;`).
- Brace-delimited `fn` and `behavior` callables, arithmetic expressions,
  assignments, returns, nested scopes, and source locations in diagnostics.
- Fixed token, binding, callable, AST-node, identifier, diagnostic, and source
  size limits. `let` and `var` are rejected rather than treated as alternate
  declaration semantics.
- Versioned Scene Document behavior attachments with stable IDs, enabled state,
  language identity, and confined `.lua`/`.hks` project-relative asset paths.

Backends should resolve IDs during load/compile and retain typed native
bindings. Names are for diagnostics and tooling; they are not the runtime
dispatch mechanism.

## What is not available yet

The HenkaScript compiler and VM currently execute bounded callable-local
bytecode. They do not yet initialize persistent global state, resolve the
language-neutral Script Host bindings, load `.lua` or `.hks` assets, dispatch
lifecycle events, or expose a scripting editor. Scene Document behavior
metadata is persisted, but runtime Behavior components, Lua compilation/VM
support, Inspector authoring, hot reload, and mixed-language events remain
subsequent implementation slices. VM instruction budgets are available now;
per-behavior runtime ownership and lifecycle budgets are not.

The current schema, HenkaScript compiler, and bounded VM are therefore engine
integration foundations, not a claim that complete executable scripting is
already available to end-user projects.
