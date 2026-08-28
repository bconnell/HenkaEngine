# Scene authoring architecture

This document describes the durable ownership boundaries for Henka's Game
Authoring Foundation and serves as an architecture reference for contributors.

## Two identities for one authored object

An authored scene object has a persistent `henka_scene_document_id`. This ID is
owned by the Scene Document and remains stable across save/load, duplication,
and editor sessions. Duplicating an object allocates a new persistent ID.

`henka_entity` remains the runtime scene handle. It is generation-checked,
may change after a scene is rebuilt, and is never serialized into a scene
document. The Sandbox adapter maintains a bounded mapping between persistent
IDs and the current runtime entities. The mapping is an adapter, not a second
scene authority.

When an authoring document is reloaded, bindings are restored by persistent ID
first. A missing ID may fall back to a unique authored object name; ambiguous
names reject the candidate document rather than silently binding the wrong
object.

## Scene Document ownership

The Scene Document is pure authoring data. It owns bounded object records,
source references, transforms, renderer references and authored component
values. It does not own renderer resources, asset-manager entries, physics
bodies, UI state, pointers, or runtime handles.

Source references use explicit kinds:

- none;
- primitive, with validated primitive parameters;
- authoring mesh, with a confined project-relative source path; and
- asset, with a confined project-relative asset reference and asset kind.

The runtime materialization layer resolves these references through the
existing asset manager, authoring-mesh loader, and `henka_scene` APIs. Asset
manager resources remain manager-owned borrowed objects. A failed resolution
does not partially replace the active scene.

Interaction authoring is represented by the existing
`henka_interaction_desc` value. Physics authoring is represented as pure
component data that can be translated into the existing
`henka_physics_body_desc`; the runtime continues to use one
`henka_physics_world` implementation.

## `.hscene` representation

Henka does not currently have a general-purpose owned JSON, YAML, or TOML
writer/parser. Its existing JSON reader is a bounded glTF-specific parser,
while terrain storage already provides explicit little-endian fields,
versioning, checksums, bounded records, and atomic replacement.

For that reason, Scene Document V1 uses a deterministic little-endian binary
format with the `HSCN` magic. This is a deliberate format choice based on the
current repository capabilities.

The V1 contract is:

- a documented header containing magic, format version, header size, payload
  size, object count, and checksum;
- explicit field encoding rather than C-struct or pointer serialization;
- fixed upper bounds for object count, strings, references, and total file
  size;
- canonical object ordering and canonical field ordering for deterministic
  output;
- finite-number, enum, ID, path, and cross-reference validation on decode;
- rejection of unknown V1-invalid fields and truncated or trailing records;
- transactional load through a validated candidate document; and
- temporary-file write followed by platform-aware atomic replacement.

The public document API will also provide a bounded inspection/dump path so
tooling can report the header, version, object IDs, source kinds, and component
presence without exposing runtime pointers. Future format versions use an
explicit migration boundary: a newer version is rejected by V1 code rather
than guessed, and a later loader may migrate a validated older document into
the current in-memory schema before committing it.

The format is intended to be source-controlled and externally usable even
though V1 is binary. Deterministic ordering, stable IDs, checksums, and the
inspection path make diffs, corruption diagnosis, and future tooling practical
without requiring an unsafe general parser. Semantic round-trip tests are a
release requirement.

## Authored components versus runtime state

The Inspector edits the Scene Document and the existing `henka_scene` working
representation only while the editor is in an authoring state. Component
changes are validated as a candidate and published together with dependent
scene, bounds, material, and collider updates.

Authored physics data includes body type, collider shape and dimensions,
trigger state, collision masks/layers, and material values. It does not include
runtime body IDs, contact history, velocities produced by Play, or pointers.

Authored interaction data includes enabled state, prompt, and interaction
distance through the existing scene descriptor. It is not duplicated in a
second interaction runtime.

## Play-session lifecycle

Play is owned by a focused Play-session module rather than `main.c`. It has an
explicit lifecycle:

1. validate authored scene data and clone the authored scene into an independent
   runtime scene while preserving generation-checked entity handles;
2. materialize runtime bodies against the runtime scene, with renderer-owned
   resources borrowed rather than duplicated;
3. run, pause, resume, or perform one bounded fixed step;
4. keep runtime transforms, velocities, contacts, and body IDs out of authored
   persistence; and
5. stop by cleaning up runtime bodies and discarding the runtime scene, leaving
   the authored scene unchanged.

Save/reload and authoring mutations are rejected while Play is active. A
materialization or fixed-step failure transitions to a failed session without
silently committing runtime state as authoring state. Context switching may
change presentation, but it does not replace the Play-session owner or its
runtime-scene isolation contract.

## Input ownership boundary

Scene View RMB navigation is application-local. A press inside the Scene View
creates a candidate; movement beyond the four logical-pixel threshold commits
the pan and captures the interaction until release. Camera and navigation
target movement use `henka_camera_pan_target`.

Panel ownership, active transforms, engine mouse capture, automation input
ownership, and higher-priority editor interactions continue to win. A click
that never crosses the threshold retains existing legitimate context-click
behavior. No global operating-system input blocking is used.

When automation ownership is enabled, the platform keeps a separate logical
pointer state and admits only bounded, fully parsed events from the configured
test stream. Main-window physical pointer motion, buttons, wheel input, and
ordinary keys are ignored for application interaction; physical Escape is the
explicit emergency-abort path. Focus loss releases held logical buttons and
keys, malformed events fail closed and request shutdown, and ownership is
released during engine teardown. The PowerShell packaged gate is required to
use this stream rather than OS-level cursor or key injection.

## Module boundary rule

New substantial behavior belongs in focused engine or Sandbox modules. The
Sandbox executable remains the composition root, but the campaign adds only
the ownership seams needed for Scene View navigation, the persistent-ID
adapter, Inspector component transactions, and Play. It does not perform an
unrelated full rewrite of `main.c`.
