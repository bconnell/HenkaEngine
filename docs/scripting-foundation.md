# Scripting Foundation

Henka exposes a shared scripting foundation through the public `<henka/script.h>` API. The scripting boundary defines one language-neutral gameplay API schema used by Lua and HenkaScript, with bounded execution adapters for both languages.

> **Status:** In Progress. The schema, Script Host, behavior runtime, Lua and HenkaScript execution, persistent attachments, state storage, source editing, runtime reload, and mixed-language events are implemented foundations. Broader host APIs, richer language features, debugger tooling, replay integration, and full project scripting workflows remain active work.

## Contents

- [Shared API schema](#shared-api-schema)
- [Behavior runtime](#behavior-runtime)
- [HenkaScript](#henkascript)
- [Lua](#lua)
- [Script Host](#script-host)
- [Behavior state](#behavior-state)
- [Scene Document integration](#scene-document-integration)
- [Sandbox Play integration](#sandbox-play-integration)
- [Events](#events)
- [Editor source authoring](#editor-source-authoring)
- [External-project validation](#external-project-validation)
- [Minimal language shapes](#minimal-language-shapes)
- [Execution order](#execution-order)
- [Limits and future work](#limits-and-future-work)

## Shared API schema

`<henka/script.h>` defines an immutable, versioned language-neutral API schema.

Current properties include:

- stable numeric API IDs;
- explicit Entity, Transform, Input, Physics, Interaction, Events, and Audio domains;
- fixed typed signatures;
- bounded parameter counts;
- bind-time lookup by numeric ID;
- diagnostic lookup by name;
- bounded Script Host binding storage;
- deduplication of resolved host bindings.

Backends resolve API IDs during load or compilation and retain typed native bindings. Names are used for diagnostics and tooling.

## Behavior runtime

`<henka/script_runtime.h>` exposes a language-neutral bounded behavior runtime.

### Behavior identity and state

The runtime provides:

- generation-checked behavior handles;
- borrowed backend callbacks;
- disabled state;
- unbound state;
- faulted state;
- deterministic batch-dispatch reports.

### Lifecycle

The shared lifecycle includes:

- `OnCreate`;
- `OnStart`;
- `OnUpdate`;
- `OnFixedUpdate`;
- `OnEvent`;
- targeted interaction signals;
- collision enter/stay/exit;
- trigger enter/stay/exit;
- `OnDestroy`;
- `OnStop`.

An immutable versioned lifecycle registry owns the callback names and bounded argument shapes. Lua and HenkaScript adapters consume the same registry.

Behavior callbacks receive shared context for:

- language;
- entity;
- lifecycle event;
- frame;
- delta time;
- instruction budget.

Dispatch is synchronous and non-reentrant. Runtime mutation requested from inside an active callback is rejected.

## HenkaScript

`<henka/henkascript.h>` exposes the bounded HenkaScript compiler foundation.

### Compiler pipeline

Current compiler support includes:

- lexer;
- parser;
- type checker;
- source locations in diagnostics;
- bounded callable bytecode generation;
- allocation-free typed VM;
- fixed VM stack storage;
- checked arithmetic;
- deterministic execution reports;
- per-callback instruction budget.

The compiler owns HenkaScript syntax authority and the canonical minimal behavior source returned by `henka_hks_get_default_behavior_source`.

HenkaScript editor presentation consumes compiler-owned token spans and lexical presentation classes. The compiler remains the single HenkaScript grammar authority.

### Language constructs

Current bounded syntax includes:

- explicit declarations such as `i32 health = 3;`;
- inferred declarations such as `count := health + 1;`;
- brace-delimited `fn` callables;
- brace-delimited `behavior` callables;
- typed arithmetic;
- comparison expressions;
- assignments;
- returns;
- nested scopes;
- `if` / `else`;
- `while`;
- `for`;
- `break`;
- `continue`.

`let` and `var` are rejected.

### Lifecycle adapter

The HenkaScript behavior adapter maps the supported lifecycle names to the shared behavior runtime.

Missing lifecycle callables are deterministic no-ops. The runtime instruction budget is propagated to the VM.

### Event context

HenkaScript contact and interaction handlers can use:

- `event_other_entity()`;
- `event_type()`;
- Entity equality and inequality;
- `entity_is_valid(entity)`.

`event_other_entity()` and `event_type()` are valid only inside typed signal callbacks. Invalid context fails closed.

`entity_is_valid(entity)` resolves through the shared `Entity.IsValid` host binding and returns a checked boolean.

HenkaScript also provides `audio_play(entity)`, `audio_pause(entity)`,
`audio_resume(entity)`, `audio_set_gain(entity, gain)`,
`audio_set_pitch(entity, pitch)`, `audio_set_looping(entity, looping)`,
`audio_set_spatial(entity, spatial)`, `audio_set_bus(entity, bus)`,
`audio_seek(entity, frame)`, `audio_stop(entity)`,
`audio_restart(entity)`, and `audio_is_playing(entity)` for the same
object-attached emitter boundary.

### Transform and physics host operations

HenkaScript currently provides:

- `vec3(x, y, z)`;
- `transform_get_position(entity)`;
- `transform_set_position(entity, position)`;
- `physics_apply_impulse(entity, impulse)`.

Vector values are finite `f32` triples. Host failures terminate the callback safely.

### Input and interaction host operations

HenkaScript also provides:

- `input_is_action_down(action_id)`;
- `interaction_try(entity)`.

`input_is_action_down` returns a checked boolean.

`interaction_try` returns the stable numeric interaction result:

| Value | Meaning |
| ---: | --- |
| `0` | Unavailable |
| `1` | Disabled |
| `2` | Out of range |
| `3` | Available |

### Audio host operations

The shared typed host surface provides these operations for persisted
object-attached emitters in Play:

- `Audio.Play(entity)`;
- `Audio.Stop(entity)`;
- `Audio.Restart(entity)`;
- `Audio.Pause(entity)`;
- `Audio.Resume(entity)`;
- `Audio.IsPlaying(entity)`;
- `Audio.SetGain(entity, gain)`;
- `Audio.SetPitch(entity, pitch)`;
- `Audio.SetLooping(entity, looping)`;
- `Audio.SetSpatial(entity, spatial)`;
- `Audio.SetBus(entity, bus)`;
- `Audio.Seek(entity, frame)`.

Missing or stale targets fail closed.

These operations resolve through the shared typed host API. Missing required
providers produce an execution error in the HenkaScript path.

## Lua

Henka includes a bounded Lua 5.4.8 behavior adapter using the same shared lifecycle names and no-op rules.

Current Lua execution policy includes:

- bounded contact arguments for collision and trigger callbacks;
- restricted standard-library surface;
- bounded allocator;
- instruction hook enforcing the per-callback budget;
- checked host access through Audio, Entity, Transform, Input, Physics, Interaction, and Events tables;
- backend-owned token and indentation APIs for editor presentation;
- the real Lua parser as syntax acceptance authority.

Lua parser ownership supplies the syntax authority used by editor validation.

## Script Host

The Script Host provides typed synchronous dispatch across the language-neutral schema.

Current host behavior includes:

- exact schema argument validation;
- exact return validation;
- non-reentrant callback protection;
- bounded FIFO event queue;
- deduplicated resolved bindings.

### Current Sandbox providers

When the Sandbox Play coordinator supplies the live input engine, camera
observer snapshot, and configured Audio system, all seven current domains are
operational:

- Entity;
- Transform;
- Input;
- Physics;
- Interaction;
- Events;
- Audio for persisted enabled emitters.

The isolated Play host currently maps persistent Scene Document IDs to runtime entities for:

- `Entity.IsValid`;
- Transform position access;
- Physics impulse application;
- `Events.Emit`;
- Input action queries;
- Interaction eligibility when the required provider exists;
- Audio emitter controls for a configured system and persisted enabled emitter.

Missing optional providers return the documented safe state:

- Lua `Input.IsActionDown` is fail-closed and returns `false`;
- Lua `Interaction.Try` is unavailable and returns `UNAVAILABLE`;
- unavailable Audio targets return the stable fail-closed result;
- unsupported host operations return bounded deterministic results defined by the current host contract.

## Behavior state

Henka provides a fixed-capacity behavior-state store.

### State identity

Each value is keyed by:

1. persistent object/entity ID;
2. behavior ID;
3. caller-defined nonzero state key.

### Supported value types

- `bool`;
- `i32`;
- `float32`;
- `vec3`.

The current store capacity is 512 values.

### Sidecar persistence

Behavior state supports explicit candidate-based sidecar persistence with:

- versioned format;
- little-endian encoding;
- 64 KiB file limit;
- confined project-relative paths;
- unique same-directory temporary files;
- atomic replacement;
- malformed-load retention of the current in-memory store.

### Edit and Play state ownership

Game Authoring exposes typed coordinator-checked state access. Store mutation is available through those coordinator operations.

Authoring state reads and writes are rejected while Play is active.

Every Play start receives an independent bounded clone of the authoring store. Runtime mutations remain inside the Play copy.

Two explicit operations manage retained runtime state:

- **Save Play State** writes the retained stopped Play store;
- **Load Play State** replaces the authoring baseline and discards the retained runtime snapshot.

Behavior-state persistence uses explicit Save Play State and Load Play State operations. Authored `.hscene` data excludes behavior state.

## Scene Document integration

Scene Documents support versioned behavior attachments with:

- stable behavior IDs;
- enabled state;
- language identity;
- confined project-relative `.lua` or `.hks` paths.

The bounded Scene Document behavior asset loader:

- resolves paths inside the project root;
- validates source size;
- validates the language suffix;
- owns the selected backend instance;
- returns a runtime descriptor for mixed-language lifecycle dispatch.

The Scene Document behavior runtime assembles loaded behavior assets by persistent object and behavior identity.

## Sandbox Play integration

The Sandbox Play session owns the behavior runtime for its isolated scene lifecycle.

Current Play integration includes:

- Scene Document behavior attachment loading;
- persistent-ID to runtime-entity mapping;
- physics-body mapping for current supported host calls;
- Create/Start/Update/FixedUpdate/Destroy/Stop dispatch;
- targeted interaction/contact signals;
- bounded event dispatch;
- independent Play behavior-state clone;
- candidate-first behavior reload.

The Play-session reload seam preserves the generation-checked lifecycle slot when the candidate succeeds. Candidate failure leaves the active backend unchanged and retains bounded source diagnostics.

## Events

Lua and HenkaScript share one bounded event queue.

### Emission

HenkaScript can emit through:

```text
emit(i32_event_id);
```

Lua uses the shared Events host table.

### Receipt

Lua receives:

```text
OnEvent(event_id, source_entity)
```

HenkaScript receives `OnEvent` and reads the event identity through:

```text
event_id()
```

### Drain behavior

A Scene behavior-runtime drain pass consumes only the events present at the beginning of that pass.

Events emitted while handling the active batch remain queued for the next drain. This prevents same-batch recursive event dispatch.

## Editor source authoring

Object Details can create confined Lua and HenkaScript behavior templates and attach them transactionally to registered authored objects.

### Source panel

The bounded editable source panel supports:

- range editing that preserves existing source bytes;
- backend-derived indentation;
- compiler/backend diagnostics;
- Save;
- Revert;
- Reload.

HenkaScript preview spans and colors come from the compiler's public lexer and token-class API.

Lua presentation data comes from Lua backend token and indentation APIs. Lua parser validation remains authoritative.

### Reload

Reload uses the shared candidate-first coordinator seam:

- active Play sessions replace the persisted backend only after candidate success;
- inactive Play state reloads the persisted source through the same coordinator contract;
- failed candidates preserve the active behavior;
- failed candidates retain bounded line, column, and source diagnostics.

## External-project validation

The external-game template consumes the public Scene Document behavior runtime through its packaged consumer workflow.

Its packaged smoke workflow loads:

- one `.hks` publisher;
- one `.lua` subscriber.

The external executable verifies:

- a shared Henka event;
- typed state delivery;
- use of the public scripting boundary.

The workflow is self-contained with the packaged scripting dependencies used by the template.

## Minimal language shapes

### Lua

Lua state reads return the value followed by a presence flag. A missing value uses the type's zero default. State writes return a numeric Henka result code.

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

### HenkaScript

HenkaScript uses typed expressions for the same state operations. `event_id()` is valid only inside `OnEvent`.

```text
fn OnUpdate() {
    i32 count = state_get_i32(1);
    state_set_i32(1, count + 1);
}

fn OnEvent() {
    state_set_i32(2, event_id());
}
```

Both examples run under the behavior instruction budget and require a valid Scene Document attachment in a Play session.

The current scripting host surface is limited to the documented gameplay APIs and bounded execution contracts.

## Execution order

The current fixed-tick execution order is:

```text
Update
  ↓
Queued Events
  ↓
FixedUpdate
  ↓
Queued Events
  ↓
Physics
  ↓
Targeted Collision / Trigger Signals
  ↓
Queued Events
```

`OnDestroy` runs before `OnStop`.

## Limits and future work

The current scripting implementation remains bounded by fixed limits for:

- tokens;
- bindings;
- callables;
- AST nodes;
- identifiers;
- diagnostics;
- source size;
- instruction budget;
- behavior-state capacity;
- event queue capacity.

Future scripting work includes:

- richer typed values;
- broader callable parameters;
- broader host API coverage;
- full Inspector authoring;
- exported-property workflows;
- debugger presentation;
- deterministic replay integration;
- broader project scripting workflows;
- stable package/versioning policy for project scripts.

The current compiler, VM, adapters, host, state store, editor source model, Scene Document attachment model, and Play runtime form the production scripting foundation for these later systems.
