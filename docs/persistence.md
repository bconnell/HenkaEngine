# Persistence

Henka Engine includes a small local-first persistence layer for settings and early project state.

## Intended use

The current persistence layer supports sandbox preferences, camera state, simple local project settings, and early external game projects that benefit from a readable text format. It is not a complete shipped-game save system.

Henka separates two persistence concepts:

- `henka_settings` for preferences and workspace state
- `henka_save_data` for local runtime or game-state snapshots

## File format

Settings files use a bounded `key=value` format.

```text
grid_visible=true
wireframe_enabled=false
mouse_sensitivity=0.002500
camera_position_x=0.000000
```

Blank lines and comment lines beginning with `#` or `;` are accepted when loading. Saved files are rewritten as plain records, so comments are not preserved.

Keys use ASCII letters, digits, `.`, `_`, and `-`. Values may contain ordinary printable text but not control characters, tabs, carriage returns, or newlines. These restrictions prevent one setting from adding unintended records.

## Transactional behavior

Settings loads are transactional. The complete file is parsed into a temporary object first. An unreadable, malformed, unsafe, or overlong file returns an error and leaves the destination unchanged.

Save-data loads follow the same rule. The version, scene id, full camera pose, and every boolean flag must validate before the destination changes.

Writes use a same-directory temporary file. Henka flushes the completed temporary file and replaces the destination only after the write succeeds. A failed write does not intentionally truncate the previous destination file.

## Paths and save slots

`henka_path_resolve` remains a generic path joiner for trusted callers that intentionally need absolute inputs.

`henka_path_resolve_confined` accepts safe relative paths only. It rejects absolute and UNC paths, traversal segments, empty segments, Windows reserved device names, trailing spaces or dots, control characters, and invalid Windows path characters. Separators are normalized to `/`.

This is lexical confinement. Applications that expose writable directories to untrusted users must also control symlinks and filesystem permissions.

Asset resolution uses the confined helper, so engine-managed asset loads require paths beneath the configured asset base directory.

Save slot names use 1-64 ASCII letters, digits, `_`, and `-`. Reserved device names are rejected. A valid slot produces a path such as:

```text
user/saves/slot_a.save
```

## Numeric validation

Integer settings outside the C `int` range fall back to the caller-provided default. Floating-point settings must parse completely and be finite. Save-data camera positions and angles must also be finite.

## Current sandbox behavior

The sandbox stores local settings in:

- packaged run: `out/HenkaSandbox3D/user/sandbox3d.settings`
- development run: a `user/` folder beside the built sandbox executable

The sandbox persists display, camera, input, and workspace state. Short status messages are session-only.

## Not provided by this layer

- cloud saves
- telemetry or analytics
- encryption
- registry storage
- binary serialization
- a general content database
- symlink-aware filesystem sandboxing
- per-game save-slot UI
- autosave or background save loops

External games should define their own save policy, migration strategy, backup behavior, and data layout. The current Henka layer is a hardened local foundation rather than a complete save system for shipped games.
