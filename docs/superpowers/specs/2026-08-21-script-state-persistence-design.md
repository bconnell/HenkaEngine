# Henka V1 Behavior State Persistence Design

## Status

Approved for implementation as the next vertical slice of the active Game
Authoring Hardening + Scripting Foundation V1 campaign.

## Goal

Give Lua and HenkaScript behaviors a shared, bounded, typed state boundary that
survives an explicit save/load operation without mutating the authoring Scene
Document or allowing Play simulation state to leak into Edit mode.

## Invariants

- The Scene Document remains pure authoring data. Runtime state is never added
  to the document and Play stop never saves it implicitly.
- State is keyed by persistent Scene Document object ID, persistent behavior ID,
  and a bounded numeric state key. Runtime entity handles are not persisted.
- The store uses a fixed-capacity array; it never grows dynamically while
  scripts execute.
- Values are limited to validated bool, signed 32-bit integer, finite float,
  and finite `henka_vec3` forms.
- Load is candidate-based: malformed, oversized, duplicate, non-finite, or
  unknown-version input leaves the current store unchanged.
- Save writes a bounded versioned sidecar through a temporary file and atomic
  replacement. The caller chooses when to save.
- State access is synchronous and single-threaded, matching the existing
  Script Host dispatch contract. Reentrant host calls remain rejected.

## Public boundary

Add `henka_script_state_store` as an engine-owned module. Its public operations
create/destroy, clear, set/get typed values, remove one value, count entries,
load a confined relative sidecar, and save that sidecar atomically. The store
does not own Scene Documents, runtime entities, renderer objects, or script
backends.

The Script Host gains a borrowed store and an execution context containing the
current persistent object ID, behavior ID, and frame. State API calls use the
current context plus a numeric key, so both languages share identical identity
semantics without exposing raw runtime handles to persistence.

The first shared host functions are `State.GetI32`, `State.SetI32`,
`State.GetBool`, and `State.SetBool`. Missing values return deterministic
defaults (`0`/`false`) and a presence flag; writes return `henka_result`.
Lua exposes checked `State` table functions. HenkaScript exposes narrow,
typed `state_get_i32`, `state_set_i32`, `state_get_bool`, and
`state_set_bool` forms through bounded VM opcodes.

## Integration

The Sandbox Play session owns the store for the isolated runtime. The Game
Authoring coordinator exposes explicit state load/save operations under the
project root, using a sidecar adjacent to the authored scene path. These
operations are rejected while Play is running, and failed loads retain the
previous store. The current Play session receives the store as a borrowed host
dependency and never writes it during stop.

## Verification

Focused tests cover fixed-capacity behavior, stale identity isolation,
non-finite rejection, duplicate/unknown-version/truncated files, atomic
failure retention, Lua/HenkaScript parity, Play Edit-vs-Play isolation, and
explicit save/load round trips. Existing full CTest, Lua-disabled, package,
and external game/server gates remain required before publication.
