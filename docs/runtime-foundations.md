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

This keeps the current scene model lightweight while making object inspection and picking less sandbox-specific. Scene identities are opaque 64-bit generation-checked handles rather than array indexes. Destroying an entity advances its slot generation before reuse, so a stale selection, action record, physics link, or authoring reference cannot silently target the replacement entity. Names, tags, material names, and interaction prompts are copied into bounded scene-owned storage, so callers may use temporary input buffers. Returned text pointers remain valid until the corresponding value is replaced or the entity is destroyed.

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

Camera constructors now sanitize invalid field-of-view, aspect, near-plane, far-plane, and orthographic-height inputs to documented defaults. `henka_camera_is_valid` provides one public contract for scene assignment and projection helpers. Screen-ray and world-to-screen conversion reject non-finite or degenerate state and initialize outputs deterministically on failure. Camera mutation helpers reject non-finite deltas without poisoning persistent camera state.

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

### Engine lifecycle and authoring ownership

Each engine instance owns copied application, asset-base, and user-data path strings after creation. One instance admits one run lifecycle: initialization is protected against recursive entry, a completed or failed run cannot be started again, and destruction is rejected while initialization or the run loop is active. A successful lifecycle schedules at most one shutdown callback.

Close and explicit exit requests stop before another update or render. The renderer retains frame ownership after a failed main swap so the engine can attempt a checked abort. Detached-window UI is drawn and presented before the main window is presented; a detached-window failure therefore aborts the still-unpresented main frame. This is a best-effort multi-window commit boundary rather than a claim of atomic operating-system presentation.

These ownership and transaction boundaries also prepare future 2.5D and modeling work. Editable authoring data should retain stable identities and undoable source state, then compile into runtime rendering and physics data instead of sharing low-level GPU-resource lifetime directly.

### Asset identity and ownership

Manager-loaded shaders, textures, and OBJ meshes use canonical confined cache identities. Rooted, UNC, device, drive-qualified, traversal, and URI-like inputs fail without producing an asset or cache entry. Equivalent slash and dot-segment path spellings share one entry. On Windows, ASCII case-only variants also share one identity, while metadata retains the normalized spelling used for the first load. Case folding is not applied on platforms with case-sensitive path identity. Returned pointers are borrowed and manager-owned; public destroy calls ignore them so a caller cannot leave a dangling cache entry or cause manager shutdown to destroy the same resource twice.

Texture and OBJ fallback retries are transactional. Texture source failures create path-specific lightweight aliases of the shared error texture rather than storing the shared pointer directly. A failed texture retry leaves the alias, metadata, and caller output unchanged; a successful retry moves the new owned backend into that same alias, so existing materials immediately observe the real texture without pointer rebinding. Allocation failures, invalid API state, and OpenGL upload failures are propagated and are not cached as ordinary source fallbacks. Texture creation reads an encoded source once into a bounded buffer, uses that exact buffer for stb_image inspection and decode, rejects HDR and 16-bit sources rather than quantizing them silently, clears outputs before validation, temporarily uses the main OpenGL context, restores the previous context, texture-unit, binding, and unpack state, checks upload errors, and destroys only backends it owns. Descriptor semantics select sRGB or linear GPU storage and are immutable on the texture object. Mesh fallback metadata remains path-based because mesh retries do not yet use stable per-path aliases.

Materials now expose a bounded metallic-roughness subset: base color, metallic, roughness, tangent-space normal scale, occlusion strength, emissive color and strength, alpha mode, double-sided state, and cast/receive-shadow intent. Material assignment validates texture semantic usage and color space. The OpenGL path uses GGX distribution, correlated Smith visibility, Schlick Fresnel, a dielectric F0, energy-conserving diffuse/specular separation, scene camera position, and indirect-only occlusion. HDR target and shadow diagnostics report dimensions, generations, completeness, and bounded failure text. Generated UV spheres use nondegenerate fan caps, and mesh-upload tangent accumulation starts from zero so valid UV derivatives are not biased by fallback axes; degenerate UVs still receive finite orthogonal fallbacks. Imported tangent preservation and MikkTSpace conformance remain future work. Image-based lighting, reusable material files, and advanced lobes remain future work.

### Workspace and viewport

Henka now also includes a small docked workspace helper for viewport-first tools:

- dock region layout math
- dedicated scene viewport bounds
- viewport aspect-ratio helpers
- window-point to viewport-local conversion
- window-point to framebuffer-point conversion for scene-space interaction paths

The sandbox uses this to keep the scene in its own viewport while panels stay in separate docked regions. Workspace calculations use checked double-precision intermediates, clear caller outputs on failure, and convert to integer viewport edges only after clipping them to the framebuffer. Viewports reject negative origins and overflowing edges. The renderer clips custom scene viewports to the framebuffer and preserves them across framebuffer resize instead of silently replacing them with a full-window viewport.
The current sandbox layers safe panel paging and session-only native detached panel placement on top of that docked layout so scene-first modes remain readable while workspace movement is evaluated.
The current side docks can now hold ordered panel groups, so cross-docked panels stack cleanly instead of covering one another.
Henka now has a small multi-window platform foundation: secondary OS-level tool windows receive collision-safe engine identifiers, validate native window identifiers before admission, route close/focus/resize/pointer events separately from the main viewport input path, and can present their own UI-only OpenGL surface. Focus loss synthesizes releases for held main-window keys and mouse buttons, releases a held detached-window left button, and clears transient motion. Detached-window press, release, and resize state is reset per frame while close and resize diagnostics remain aggregated for the full event poll. VSync and mouse capture report failures instead of recording partially applied state, platform queries clear caller outputs on failure, and diagnostics report the latest verified multi-window capability rather than assuming success. Main and detached-window OpenGL context transitions are checked before resource deletion, frame abort reports context-restoration failure without falsely clearing logical frame ownership, and scene drawing binds either the current entity texture or texture zero for every entity so stale texture state cannot leak across objects. The sandbox uses this for `Native Panel Test` and for compact detached production-panel state surfaces. Full detached controls and OS-title-bar drag-back docking remain future work.
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

Boxes are axis-aligned in v1; rotated box collision, mesh colliders, constraints, continuous collision detection, controllers, and advanced simulation remain future work. In the sandbox, physics playback is opt-in through `Physics QA`, leaving normal transform inspection unchanged until the demo is enabled. Physics bodies, contacts, pairs, and events use bounded checked capacity growth. Every fixed substep runs against scratch body, contact, current-pair, previous-pair, and event storage. Only a fully successful candidate replaces the live arrays; allocation failure returns `HENKA_ERROR_OUT_OF_MEMORY`, while finite integration, geometry, contact, impulse, friction, or correction arithmetic outside representable valid state returns `HENKA_ERROR_NUMERIC_RANGE`. Both failures free the candidate and retain the prior bodies, contacts, events, pair history, accumulator, and scene-linked transforms without partial event emission. Numeric limits derive from floating-point representation, the fixed timestep, valid collider geometry, and normalized quaternion/contact contracts rather than gameplay-scale caps. Callers may correct state and retry. Earlier successful substeps in one catch-up update remain committed when a later substep fails, and the remaining accumulator time can be retried. Scene links are written best-effort only after the physics commit. Body destruction reserves required EXIT-event capacity before mutation, removes only records involving that body, preserves unrelated queued events and previous-pair history, and recomputes collision flags only for affected survivors. Query failures clear caller-visible output structures, plane collider offsets affect both contacts and raycasts, and rays beginning inside an AABB return the exit surface with a valid normal. Fixed-step catch-up remains capped at 16 substeps and discards excess whole-step backlog after that cap so later zero-delta calls do not continue stale simulation time. Sandbox bodies start static; isolated drop activates only the selected supported body, while the full demonstration assigns its intended dynamic set explicitly.

### Package validation

The hosted Windows workflow uses a package contract mode that validates provenance, executable hashes, required files, run guidance, and offline help without requiring an OpenGL desktop session from the hosted runner. Local release evidence continues to require the full packaged runtime smoke test, native detached-window desktop harness, clean shutdown, and application-only screenshots. This separates infrastructure capability from engine correctness without weakening the local runtime gates.

### Asset metadata

The asset manager now tracks read-only metadata for cached assets:

- asset type
- source path
- display name
- loaded state
- fallback state
- reload eligibility state
- short summary
- short error summary

This is a small inspection layer, not a content browser or hot-reload pipeline. Source and display names are cache-owned strings rather than borrowed caller buffers. Shader, texture, and mesh cache growth is bounded and checked before allocation, asset paths are bounded, and scene entity storage also uses bounded checked growth.

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
- Vertex Color, which multiplies the supported material surface by the mesh's normalized RGBA vertex color
- Reserved Procedural

The reserved procedural type exists only as a stable enum value for future work. It does not implement procedural shading yet. Material names are copied into scene-owned storage, and material assignment rejects invalid enum values, non-finite colors, and textured configurations without a texture reference. Model vertices expose normalized RGBA color; OBJ and generated primitives default to opaque white, while explicit model colors can include black or transparent values.

### Interactions

Scene objects can now expose generic interaction metadata:

- enabled state
- prompt text
- max distance

Henka also exposes a simple interaction eligibility result so samples can decide whether an object is currently available, disabled, or out of range. Prompt text is copied into scene-owned storage, ranges must be finite and non-negative, and non-finite observer positions are unavailable.

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
The sandbox workspace now tracks dock zones, allowed dock masks, last valid docks, transient header dragging, native detached-window handles, and dock splitter interactions in session state. Detached production-panel surfaces and `Native Panel Test` use separate-window rendering without routing pointer input into Scene View. Closing a detached production panel safely returns it to its last dock. The Scene View remains the main viewport; full detached controls, saved detached placement, and detaching the viewport remain future work.
The sandbox also layers local editor-control profiles over the engine input actions. Transform actions support bounded key and mouse aliases, validated local settings, protected built-in defaults, named custom profiles, safe fallback, and an Escape-consumption path that lets an active transform restore its original object transform before normal UI, mouse-capture, or exit handling continues.

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

### Structural transform and runtime-state safety

Scene entities can carry a transform-lock flag. The Action API blocks move, rotate, and scale mutations for locked entities while still allowing selection, inspection, camera focus, visibility changes, and reset-to-default. The sandbox Ground starts locked and remains selectable for inspection, but locked selections do not display the yellow transform highlight, do not expose gizmos, and cannot start action-based transform hotkeys. Unlocking requires an explicit entity-specific control activation. Selection, visibility, tool-mode, and lock transitions cancel and clear active transform sessions so stale entity targets cannot survive into a later selection. Clicking empty scene background clears the current selection in Select, Move, Rotate, and Scale modes.

Plane physics now rotates local plane normals by the body transform for contacts and raycasts. The sandbox collider overlay and selection highlight use the same transformed geometry instead of assuming a horizontal plane.

Generic confined relative-path resolution remains valid without a base directory for asset and standalone relative-path use. Save-slot construction separately requires a non-empty user-data base path so save files cannot silently fall back to the process working directory. Save camera poses reject out-of-range pitch, failed camera-pose reads clear caller outputs, and save-flag names are bounded so every accepted flag remains serializable after the `flag.` prefix is added. Sandbox camera settings are validated as a complete camera before use, with unsafe speed, pitch, projection height, position, or sensitivity values replaced by safe defaults.

Windows timing uses the monotonic high-resolution performance counter, with a monotonic tick-count fallback. Supported non-Windows builds use `CLOCK_MONOTONIC` when available.

### Numeric geometry boundaries

Vector and quaternion normalization now scale finite inputs before computing magnitude, so very large finite values produce valid unit directions instead of collapsing to zero or overflowing. Quaternion multiplication performs its intermediate products in double precision before normalization.

Scene transforms and local bounds are treated as a paired invariant. A transform or local-bound update is rejected when the derived world center, extents, or minimum and maximum corners cannot remain finite and representable. World-bound query failures clear output deterministically. Scene picking normalizes finite ray directions internally, including very large finite directions.

Physics body creation and collider or transform replacement validate derived collider centers, radii, box extents, plane normals, plane offsets, and representable shape boundaries. Individually finite inputs that would overflow persistent collision geometry are rejected before mutating the body. Physics raycasts normalize finite directions through the same scaled path, including directions whose unnormalized length exceeds the finite float range.
## Viewport shading

The Scene View owns an explicit shading mode rather than relying on a global polygon toggle. Wireframe draws neutral geometry edges without texture sampling. Solid draws neutral filled surfaces under a neutral editor surface policy. Material Preview uses the same bounded Cook–Torrance material evaluation and Scene View-sized linear HDR-to-display presentation as Rendered, with deterministic editor lighting instead of scene lighting. Both HDR modes currently use an engine-owned bounded linear studio environment for visible surroundings and diffuse/specular fallback; external HDR image preprocessing and IBL cache assets remain a separate branch. Rendered uses scene light policy, exposure, tone mapping, and the directional shadow path. Unlit materials bypass lighting, reserved procedural materials are rejected until a real shader model exists, and helper overlays retain their own materials. Mode changes do not rewrite scene materials, and the renderer restores a filled polygon baseline before UI and detached-window presentation.

The legacy wireframe API remains compatible: enabling it selects Wireframe, while disabling it restores the last valid non-wireframe mode. The sandbox persists the authoritative mode under `ui.scene_view.shading_mode`; older `wireframe_enabled` settings are migrated only when the new key is absent.
