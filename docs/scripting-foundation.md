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
  borrowed backend callbacks, deterministic Create/Start/Update/FixedUpdate/
  Destroy/Stop state transitions, targeted interaction/contact signals,
  disabled/unbound/faulted states, and batch dispatch reports.
- The same header exposes an immutable versioned lifecycle registry containing
  callback names and bounded argument shapes. Lua and HenkaScript adapters
  consume that registry for callback discovery instead of maintaining separate
  public lifecycle-name contracts.
- Behavior callbacks receive the shared language, entity, lifecycle event,
  frame, delta-time, and instruction-budget context. Dispatch is synchronous
  and non-reentrant; runtime mutation from a callback is rejected.
- A bounded HenkaScript lexer/parser/type checker exposed through
  `<henka/henkascript.h>`.
- Bounded callable bytecode generation and an allocation-free typed VM with
  fixed stack storage, checked arithmetic, deterministic reports, and an
  instruction budget.
- A HenkaScript behavior adapter that maps the exact `OnCreate`, `OnStart`,
  `OnUpdate`, `OnFixedUpdate`, `OnInteract`, collision/trigger enter-stay-exit,
  `OnDestroy`, and `OnStop` callable names to the shared lifecycle runtime;
  absent lifecycle callables are deterministic no-ops and the runtime budget
  is propagated to the VM.
- A Lua 5.4.8 behavior adapter with the same lifecycle names and no-op rules,
  bounded contact arguments for collision/trigger callbacks, a restricted
  standard-library surface, a bounded allocator, and an instruction hook that
  fails closed when the per-callback budget is spent.
- A bounded behavior-state store keyed by the persistent object/entity ID,
  behavior ID, and a caller-defined nonzero state key. It supports typed
  `bool`, `i32`, `float32`, and `vec3` values, with a fixed 512-value capacity.
- Explicit, candidate-based state sidecar persistence with a versioned
  little-endian format, a 64 KiB file limit, confined project-relative paths,
  same-directory temporary files, and atomic replacement. Loading malformed
  state retains the existing in-memory store.
- Explicit typed declarations (`i32 health = 3;`) and inferred declarations
  (`count := health + 1;`).
- Brace-delimited `fn` and `behavior` callables, typed arithmetic and
  comparison expressions, assignments, returns, nested scopes, bounded
  `if`/`else`, `while`, and `for` control flow, `break`/`continue`, and
  source locations in diagnostics.
- HenkaScript contact/interaction handlers can read the bounded
  `event_other_entity()` and `event_type()` context values; those built-ins
  fail closed when executed outside a typed signal callback, and Entity
  equality/inequality is available for bounded signal branching. The typed
  `entity_is_valid(entity)` intrinsic resolves through the shared
  `Entity.IsValid` host binding and returns a checked boolean.
- HenkaScript also has a bounded `vec3(x, y, z)` value constructor plus
  `transform_get_position(entity)`, `transform_set_position(entity, position)`,
  and `physics_apply_impulse(entity, impulse)` host operations. Vector values
  are finite `f32` triples and host failures terminate the callback safely.
- HenkaScript now exposes the same bounded context contract for
  `input_is_action_down(action_id)` and `interaction_try(entity)`. The first
  returns a checked boolean; the second returns the documented numeric
  interaction-result value (`0` unavailable, `1` disabled, `2` out of range,
  `3` available). Both operations resolve through the shared typed host API,
  so an absent provider is an execution error rather than an invented result.
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
- The external-game template consumes the same public Scene Document behavior
  runtime without Sandbox source. Its packaged smoke path loads one `.hks`
  publisher and one `.lua` subscriber, then verifies a shared Henka event and
  typed state delivery from the external executable.
- Lua behaviors can call the shared host surface through checked `Entity`,
  `Transform`, `Input`, `Physics`, `Interaction`, and `Events` tables. In the
  current Sandbox Play dispatcher all six domains are operational when the
  coordinator supplies the live input engine and camera observer snapshot.
  Without those providers, `Input.IsActionDown` is fail-closed to `false` and
  `Interaction.Try` is unavailable, returning the stable `UNAVAILABLE` result.
  HenkaScript has the matching
  `input_is_action_down(action_id)` and `interaction_try(entity)` host calls
  with the same provider requirements. The HenkaScript surface can call the
  shared `Entity.IsValid` operation and exposes the same event identity through
  the bounded `emit(i32_event_id);` builtin. Both languages can receive the same
  queued event through `OnEvent`; Lua receives `(event_id, source_entity)` and
  HenkaScript reads the event ID through `event_id()`.
- The Scene behavior runtime drains only the events present at the beginning
  of a drain pass. Events emitted while handling that pass remain queued for
  the next pass, preventing same-batch recursive dispatch.

Backends should resolve IDs during load/compile and retain typed native
bindings. Names are for diagnostics and tooling; they are not the runtime
dispatch mechanism.

## Minimal language shapes

Lua state reads return the value followed by a presence flag. A missing value
uses the type's zero default; the flag lets a behavior distinguish that case.
State writes return a numeric Henka result code.

```lua
function OnUpdate()
    local count, present = State.GetI32(1)
    if not present then count = 0 end
    State.SetI32(1, count + 1)
end

function OnEvent(event_id, source_entity)
    State.SetI32(2, event_id)
end
```

HenkaScript uses typed expressions for the same operations. `event_id()` is
valid only inside `OnEvent`.

```text
fn OnUpdate() {
    i32 count = state_get_i32(1);
    state_set_i32(1, count + 1);
}

fn OnEvent() {
    state_set_i32(2, event_id());
}
```

Both examples are bounded by the behavior instruction budget and require a
valid Scene Document attachment in a Play session. They do not provide
arbitrary filesystem, process, network, or renderer access.

## What is not available yet

The HenkaScript compiler and VM currently execute bounded callable-local
bytecode, and both language adapters can drive their bounded execution through
the language-neutral behavior runtime. The bounded loader can create owned
backend instances from persisted `.lua` and `.hks` attachments, and the Scene
Document behavior runtime can assemble and dispatch them by persistent object
identity. The Sandbox Play session owns that runtime for its isolated scene
lifecycle and fixed-tick dispatch, with a bounded host mapping for the current
Entity/Transform/Physics/Event slice and an explicit behavior-state sidecar
save/load seam. Persistent state is not implicitly saved on Stop, is not part
of the authored `.hscene` document. The Inspector can create confined Lua or
HenkaScript behavior templates and attach them transactionally, and it provides
a bounded read-only source preview. HenkaScript preview colors are derived from
the compiler's public tokenization; source editing and reload are not included.
Richer typed values and callable parameters, full Inspector authoring, hot reload,
debugger tooling,
replay integration, and broader project scripting workflows remain future work.

The current schema, HenkaScript compiler, and bounded VM are therefore engine
integration foundations, not a claim that complete executable scripting is
already available to end-user projects. The current fixed-tick order is
`Update`, queued events, `FixedUpdate`, queued events, physics, targeted
collision/trigger signals, and queued events; `Destroy` runs before `Stop`.
