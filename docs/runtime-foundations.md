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
- look-at orientation
- camera reset
- relative camera movement
- camera focus on bounds
- camera framing on bounds
- orbit, pan, and dolly behavior around a target
- screen-point ray creation

The sandbox still uses a free camera, but the math is now reusable outside the example.

### Action commands

Henka now includes a small local Action API for validated scene and object operations.

Current Action API v1 coverage includes:

- scene summary and object listing
- object create, delete, rename, select, and details queries
- position, rotation, and scale changes
- move, rotate, and scale-by-multiplier commands
- transform reset when a default transform is registered
- visibility actions
- camera focus when a camera context exists
- dry-run validation with structured results

The engine still owns authority. Tests, tools, and future workspace panels request actions through a context instead of reaching into scene state blindly.

### Workspace and viewport

Henka now also includes a small docked workspace helper for viewport-first tools:

- dock region layout math
- dedicated scene viewport bounds
- viewport aspect-ratio helpers
- window-point to viewport-local conversion
- window-point to framebuffer-point conversion for scene-space interaction paths

The sandbox uses this to keep the scene in its own viewport while panels stay in separate docked regions.
The current sandbox also layers safe panel paging and session-only in-window floating panel placement on top of that docked layout so scene-first modes remain readable while workspace movement is evaluated.
Henka now has a small multi-window platform foundation: secondary OS-level tool windows receive stable engine identifiers, route close/focus/resize/pointer events separately from the main viewport input path, and can present their own UI-only OpenGL surface. The sandbox exposes this foundation through `Native Panel Test`; production tool panels still use the in-window workspace.
The sandbox now also uses explicit viewport tool modes on top of that viewport math so selection, orbit, pan, and gizmo manipulation can route through one visible user-facing tool state instead of relying only on hidden mouse modifiers.

### Viewport interaction testing

Henka now also exposes deterministic viewport interaction helpers around the current transform gizmo path:

- viewport-aware mouse conversion
- world-to-screen projection
- projected gizmo handle models
- screen-space handle hit testing
- drag-state creation from a visible handle
- deterministic move, rotate, and uniform-scale drag math

The sandbox uses the same handle-model and drag helpers that the tests now exercise. This reduces manual QA for basic object-selection and transform-mutation outcomes without claiming that visual feel is fully automated.
The current sandbox layer also exposes compact interaction-gate helpers and reject reasons so runtime diagnostics can say why a viewport interaction did not start instead of failing silently.
Panel ownership checks use current visible panel rectangles and framebuffer-space mouse coordinates, matching the coordinate space used by viewport selection and gizmo hit testing.

### Picking

Henka now includes a small picking foundation:

- screen point to world ray
- viewport-relative window-to-ray conversion
- ray versus bounds checks
- nearest visible object pick in a scene

The sandbox uses this for lightweight click selection and gizmo dragging when mouse capture is released. With the docked workspace layout, picks only start from the dedicated scene viewport.

### Transform gizmos

Henka now also includes a small transform gizmo foundation for selected scene objects:

- world-axis move helpers
- world-axis rotation helpers
- uniform scale helpers
- snap helpers for move, rotate, and scale
- viewport-aware gizmo hit testing
- world-to-screen projection for viewport tools
- projected screen-space handle hit helpers
- stable drag cancellation around viewport changes and invalid targets
- shared projected handle-model logic used by both overlay drawing, runtime interaction, and tests

The sandbox uses these helpers to draw overlay gizmos inside the dedicated viewport and manipulate selected objects without turning the current sample into a full editor.
The helper scene pieces that used to visualize those gizmos stay internal to the tool path, are hidden from the normal runtime view, are excluded from normal scene picking, and are not treated as persisted or user-facing scene selection targets.
The current automated coverage now proves real selected-object mutation more directly, but manual desktop QA is still needed for handle readability, drag comfort, and general interaction feel.
The sandbox also now keeps direct transform fallback controls beside those gizmo helpers so packaged QA can confirm selected-object mutation through the Action API even when viewport input or handle hit testing still needs investigation.

### Rigid-body physics

Henka now exposes a deterministic rigid-body physics v1 API with:

- physics-world creation, gravity, fixed timestep accumulation, explicit fixed stepping, and reset
- static, dynamic, and kinematic bodies with stable handles
- linear and angular velocity, force, impulse, torque, damping, restitution, and friction
- sphere, axis-aligned box, and plane colliders with collision masks and trigger flags
- brute-force pair generation suitable for current small scenes
- collision response, collision and trigger enter/stay/exit events, and raycasts
- optional links that write simulated transforms to real scene entities
- debug-shape queries so visual overlays use the actual collider data

Boxes are axis-aligned in v1; rotated box collision, mesh colliders, constraints, continuous collision detection, controllers, and advanced simulation remain future work. In the sandbox, physics playback is opt-in through `Physics QA`, leaving normal transform inspection unchanged until the demo is enabled.

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
That diagnostics surface now also includes sandbox-level interaction state such as viewport tool mode, cursor ownership, selected object validity, gizmo model validity, hovered handle, active drag target, last rejected interaction reason, and last Action API result.
The sandbox keeps the essential gate state in a compact Scene View strip during interaction, including selected-object highlight state, while the fuller Diagnostics, Transform QA, and Physics QA utility views remain available for deeper packaged testing.
The sandbox workspace now tracks docked or in-window floating panel rectangles, z order, title dragging, floating resize grips, and dock splitter interactions in session state. The same visible rectangles are fed back into mouse ownership before viewport tools run. `Native Panel Test` uses the separate-window surface and reports focus, size, and routed-event state without routing its pointer input into Scene View. The Scene View remains the main viewport; converting production panels and detaching the viewport remain future work.

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
