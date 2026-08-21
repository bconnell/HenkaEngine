# Scripting foundation

Henka currently exposes the first shared scripting seam through the public
`<henka/script.h>` API. This slice defines the language-neutral gameplay API
schema used by future Lua and HenkaScript backends.

## What is available

- An immutable, versioned API schema with stable numeric IDs.
- Explicit domains for Entity, Transform, Input, Physics, Interaction, and
  Events.
- Fixed typed signatures with bounded parameter counts.
- Bind-time lookup by numeric ID or diagnostic name.
- A bounded Script Host that deduplicates resolved API bindings.
- Versioned Scene Document behavior attachments with stable IDs, enabled state,
  language identity, and confined `.lua`/`.hks` project-relative asset paths.

Backends should resolve IDs during load/compile and retain typed native
bindings. Names are for diagnostics and tooling; they are not the runtime
dispatch mechanism.

## What is not available yet

This foundation does not execute source code, load or compile `.lua` or `.hks`
assets, dispatch lifecycle events, or expose a scripting editor. Scene Document
behavior metadata is persisted, but runtime Behavior components, Lua support,
HenkaScript compilation/VM support, Inspector authoring, hot reload, runtime
budgets, and mixed-language events remain subsequent implementation slices.

The current schema is therefore an engine integration contract, not a claim
that scripting is already available to end-user projects.
