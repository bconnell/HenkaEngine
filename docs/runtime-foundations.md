# Runtime Foundations

Henka Engine now includes a thin set of reusable runtime foundations that future samples, tools, and external game repositories can build on.

These foundations are intentionally practical:

- small public APIs
- local-only behavior
- no new third-party runtime dependency
- tested logic where the behavior is unit-testable
- sandbox proof where the feature is useful without turning the sandbox into a full editor

## What exists now

### Input actions

Henka now exposes a small action layer above raw keys and mouse buttons.

Current actions include:

- Move Forward
- Move Back
- Move Left
- Move Right
- Move Up
- Move Down
- Interact
- Open Panels
- Change Layout
- Toggle Mouse Capture

The sandbox still keeps raw input support available, but its camera movement and panel toggles now also use action queries.

### Scene objects

Scene entities now support more read-only object metadata:

- stable entity handle
- name
- optional tag
- transform
- visibility
- optional local bounds
- optional interaction metadata

This keeps the current scene model lightweight while making object inspection and picking less sandbox-specific.

### Cameras

The camera module now exposes reusable helpers for:

- perspective camera creation
- orthographic camera creation
- pitch clamping
- camera reset
- relative camera movement
- camera focus on bounds
- screen-point ray creation

The sandbox still uses a free camera, but the math is now reusable outside the example.

### Picking

Henka now includes a small picking foundation:

- screen point to world ray
- ray versus bounds checks
- nearest visible object pick in a scene

The sandbox uses this for lightweight click selection when the UI is closed and mouse capture is released.

### Asset metadata

The asset manager now tracks read-only metadata for cached assets:

- asset type
- source path
- display name
- loaded state
- fallback state
- reload eligibility placeholder
- short summary
- short error summary

This is a small inspection layer, not a content browser or hot-reload pipeline.

### Materials

Materials now carry lightweight type and naming metadata:

- material name
- material type
- base color
- texture reference
- lighting usage

Current material types include:

- Lit
- Unlit
- Vertex Color
- Procedural Placeholder

The placeholder exists only as a stable enum for future work. It does not implement procedural shading yet.

### Interactions

Scene objects can now expose generic interaction metadata:

- enabled state
- prompt text
- max distance

Henka also exposes a simple interaction eligibility result so samples can decide whether an object is currently available, disabled, or out of range.

### Save data

Settings and save data are now separate concepts.

- `henka_settings` remains a simple key-value preferences layer.
- `henka_save_data` is a small local save foundation with:
  - version
  - scene id
  - camera pose
  - boolean flags
  - slot-based file path helper

This is still intentionally small. It is not a complete shipped-game save pipeline.

### Logging and diagnostics

Henka still uses console logging for development and troubleshooting, but the engine now also exposes a compact diagnostics snapshot with:

- delta time
- frame time
- fps
- frame index
- framebuffer size
- wireframe state
- mouse capture state
- UI visibility
- package mode

The sandbox uses that data in its in-window diagnostics utility so normal inspection does not depend on the console.

### Package modes

Henka now exposes a small package mode concept:

- Auto
- Development
- Packaged

In `Auto`, the engine checks for `PACKAGE_INFO.txt` beside the runtime assets and reports `Packaged` when that marker is present.

This is not a separate installer or release system. It is a lightweight runtime distinction that helps samples and tools explain what kind of run they are in.

## What is intentionally limited

These foundations do not add:

- a full editor
- input rebinding UI
- gamepad support
- scene hierarchy
- scene files
- physics picking
- shader graph
- live shader editing
- dialogue or game-specific interactions
- cloud saves
- remote logging
- release automation

They are building blocks for later work, not a complete toolchain by themselves.
