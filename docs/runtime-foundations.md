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

glTF-backed material assets now use the same canonical source identity as the
mesh import path. They retain the shared `henka_material` model, borrow only
manager-owned semantic textures, and expose a copied material instance for
scene assignment. Reload builds a complete candidate through bounded parsing,
confined dependency resolution, and semantic validation before replacing the
stable asset value; failures leave the previous value and identity intact.

Manager-loaded shaders, textures, and OBJ meshes use canonical confined cache identities. Rooted, UNC, device, drive-qualified, traversal, and URI-like inputs fail without producing an asset or cache entry. Equivalent slash and dot-segment path spellings share one entry. On Windows, ASCII case-only variants also share one identity, while metadata retains the normalized spelling used for the first load. Case folding is not applied on platforms with case-sensitive path identity. Returned pointers are borrowed and manager-owned; public destroy calls ignore them so a caller cannot leave a dangling cache entry or cause manager shutdown to destroy the same resource twice.

Texture and OBJ fallback retries are transactional. Texture source failures create path-specific lightweight aliases of the shared error texture rather than storing the shared pointer directly. A failed texture retry leaves the alias, metadata, and caller output unchanged; a successful retry moves the new owned backend into that same alias, so existing materials immediately observe the real texture without pointer rebinding. Allocation failures, invalid API state, and OpenGL upload failures are propagated and are not cached as ordinary source fallbacks. Texture creation reads an encoded source once into a bounded buffer, uses that exact buffer for stb_image inspection and decode, supports Radiance HDR as finite linear RGBA32F uploaded to RGBA16F, rejects unsupported 16-bit sources rather than quantizing them silently, clears outputs before validation, temporarily uses the main OpenGL context, restores the previous context, texture-unit, binding, and unpack state, checks upload errors, and destroys only backends it owns. Descriptor semantics select sRGB or linear GPU storage and are immutable on the texture object. Mesh fallback metadata remains path-based because mesh retries do not yet use stable per-path aliases.

The KTX boundary also validates every bounded mip's dimensions and expected
pixel/block byte count before a payload is published. Texture info now reports the exact logical resident GPU byte count, total and
resident mip counts, whether the backend chose a compressed GPU format, and
the selected BC, ETC2, ASTC, or RGBA8 resident format. Uncompressed and Basis
sources can fall back to RGBA8 when compressed upload is unavailable; native
compressed payloads without a matching GPU capability fail closed because the
bounded KTX boundary does not reinterpret compressed blocks as RGBA8.
The asset manager can enforce a configured texture residency budget before a
new source load is published and exposes rejection, resident-byte, managed
count, and fallback-count diagnostics. Manager-owned KTX2 textures also
support a synchronous request for a bounded top-mip prefix: the source is
reread, validated, and uploaded into a replacement backend before the existing
texture identity is changed; budget or source failure leaves the old backend
and cache entry unchanged. PNG/HDR sources are not streamable through this
API. Manager-owned KTX2 requests can be coalesced into a bounded queue and
processed with completion/failure counters; visible active scene materials
enqueue distance-bounded KTX2 mip targets, and the engine frame lifecycle
services at most one queued request while the renderer context is active, but
the upload itself remains synchronous. Repeated requests for one texture retain
the strongest target and priority so a farther reference cannot demote a nearer one;
visible requests use deterministic distance and semantic-slot priority. A deterministic trim operation can reduce the
largest eligible KTX2 textures to one resident mip until a caller-provided
target is met, with separate trimmed/demoted-byte diagnostics and transactional rollback on
failure. This path does not claim whole-resource eviction. Callers can apply the configured non-zero budget through a bounded
enforcement API; a zero budget remains a no-op and a capped trim count
returns `HENKA_ERROR_LIMIT` if the target cannot be reached. This is explicit
policy enforcement, and the frame lifecycle applies at most one configured-
budget trim per frame. Visible manager-owned textures are pinned for the
active frame before this trim step, and diagnostics report the bounded pin count.
Visible KTX2 targets use projected texture radius from validated entity bounds,
camera projection, and scene viewport, with deterministic distance fallback when
those inputs are invalid. Background I/O and broader automatic frame policy remain
unfinished.

The Windows sandbox `--residency-stress` mode uses only public asset APIs to load
65 path-distinct manager-owned PNG textures (alternating the two packaged source
images), verifies shared-path cache identity, applies a configured-budget rejection
pass, pins two active references, fills and recovers the bounded queue, processes
failed non-KTX2 residency requests, and cancels pending work. With the pinned KTX
dependency enabled, it also generates a build-local uncompressed three-level KTX2
fixture, loads it through the manager, promotes it from one to two and three
resident mips, trims it back to one, and checks the transactional diagnostics before
shutdown. This proves ownership, queue, budget, mip, and lifetime behavior at
application runtime; compressed-format pressure, visibility-threshold transitions,
and background I/O streaming remain separate unfinished tracks.

Residency diagnostics also retain cumulative bytes successfully uploaded through
manager-owned texture creation or replacement, bytes removed by successful trim
demotions, resident payload bytes known to have been rejected after a successful
replacement upload was prepared, and encoded source bytes for readable files
that were rejected before residency. Missing or unreadable sources retain a
separate unknown-source-failure count rather than fabricating a byte count.
These counters describe the bounded synchronous lifecycle; they are not evidence
of background I/O streaming.

Materials now expose a bounded metallic-roughness subset: base color, metallic, roughness, tangent-space normal scale, occlusion strength, emissive color and strength, bounded clearcoat and optional sheen color and roughness, alpha mode, double-sided state, cast/receive-shadow intent, and bounded volume attenuation controls. Material assignment validates texture semantic usage and color space. Manager-owned glTF material definitions now create stack-owned instances with validated parameter overrides, revision-aware refresh after transactional reimport, semantic dependency inspection, transactional reset of one or all overrides, and a transactional bridge that applies the validated effective instance view to a scene entity; the definition remains the shared glTF material authority and no second JSON schema is introduced. The OpenGL path uses GGX distribution, correlated Smith visibility, Schlick Fresnel, a dielectric F0, energy-conserving diffuse/specular separation, and bounded clearcoat and sheen lobes whose base-layer transmission is attenuated before the secondary response is added. Scene environments can now borrow a validated linear HDR equirectangular texture for the background and material environment response while retaining the analytical gradient fallback; irradiance, prefiltered specular, and BRDF-LUT resources are derived transactionally when the GPU path is available. HDR target and shadow diagnostics report dimensions, generations, completeness, and bounded failure text. Generated UV spheres use nondegenerate fan caps, and mesh-upload tangent accumulation starts from zero so valid UV derivatives are not biased by fallback axes; degenerate UVs still receive finite orthogonal fallbacks. Model data can now carry a finite imported tangent frame and its handedness, which the upload path preserves after normal orthogonalization; legacy and procedural data continue to use generated tangents. Full MikkTSpace conformance remains future work. Blended entities always render after opaque and masked entities. Up to 4,096 blended entities use an allocation-free O(n log n) back-to-front sort with explicit depth/index tie-breaking; overflow falls back to deterministic entity order while preserving the opaque-first pass, and overflow count is exposed in diagnostics. Scenes support a bounded four-light point/spot list with normalized directions, inverse-square/range falloff, and spot cones in the GL 3.3 path; two fitted directional cascades, one deterministic 512² map for the first enabled spot, and one bounded 256² cubemap for the first enabled point provide bounded local shadowing. Distance fog is explicitly selected as linear, exponential, or exponential-squared, is disabled by default, and remains a distance effect rather than volumetric fog. Renderer diagnostics also report bounded distance-based LOD selections and configured-LOD fallback use; this is selection/culling foundation, not texture streaming or residency. Editor-authored material files and other advanced lobes remain future work.

The sandbox Object Details panel now exposes a bounded in-panel editor for the imported glTF Marker: every shared instance parameter can be selected, scalar/vector values can be stepped, booleans and alpha mode can be changed, semantic texture overrides can be cleared or restored, dependencies and definition revision are visible, and reset/reimport preserve transactional failure behavior. Built-in procedural materials remain inspect-only and material-file authoring is still future work.

The HDR environment path now derives irradiance, prefiltered specular, and BRDF-LUT resources transactionally from the validated scene-owned texture; the analytical gradient remains the truthful fallback when derivation cannot complete. Optional local reflection probes own bounded 64x64 RGBA16F cubemaps in the OpenGL backend. One changed probe is captured at a time from six deterministic camera directions, candidate faces are committed only after the complete capture succeeds, and probe sampling is disabled during capture to prevent recursion. Enabled probes are selected deterministically; box projection is applied only when requested, while an uncaptured or invalid probe falls back to the shared IBL resources. The sandbox editor can visualize enabled probe volumes and their box-projection mode without changing scene data.

Double-sided material draws disable face culling and orient backface geometric
normals toward the viewer for consistent lighting.

### Workspace and viewport

HDR presentation includes a bounded half-resolution extract and separable blur bloom pass plus a small Rendered-only post color grade; Material Preview deliberately omits those scene-dependent presentation effects. Its GPU targets are replaced transactionally; if allocation or framebuffer validation fails, tone mapping remains active and the renderer diagnostics expose the fallback.
When a scene supplies a linear HDR equirectangular texture, the renderer derives bounded environment, irradiance, prefiltered-specular, and BRDF-LUT resources through one framebuffer-owned build transaction. The material shader samples those resources for diffuse and roughness-aware specular response; a failed derivation retains the source equirectangular path and reports IBL fallback diagnostics.

The Rendered presentation keeps one bounded display-space RGBA8 history texture, a Scene View-sized RG16F motion attachment, an R8 reactive attachment, and a sampled depth attachment. The built-in material shader writes camera- and object-motion vectors from the current and previous view-projection matrices and marks transparent, transmissive, and emissive pixels as reactive. The tone-map pass reprojects history by motion, rejects invalid or depth-inconsistent samples, reduces accumulation for reactive and high-motion pixels, clamps history to the current 3x3 tone-mapped neighborhood, and applies bounded neighborhood reconstruction sharpening. Rendered projection uses a bounded eight-sample Halton subpixel jitter sequence; history and the previous view-projection are invalidated when the temporal path becomes unavailable, the shading mode changes, the viewport is resized, or a large camera/projection discontinuity is detected. Diagnostics expose whether the temporal path is using its fallback, cumulative history resolves and fallback frames, and the latest invalidation reason/count. History, velocity, reactive, and depth targets are replaced transactionally on resize, and history is invalidated until the first successful copy. Custom shaders without the optional motion contract and entities with no valid vector fall back to zero motion. The AO presentation now uses a bounded four-direction, two-sided, multi-step view-space horizon search with explicit radius, thickness, falloff, bias, and intensity controls; temporal AO history, denoise filtering, and production GTAO validation remain unfinished. The temporal path remains a bounded reconstruction foundation pending visual validation across camera cuts, resize, disocclusion, and moving-object cases; it is not a claim of complete production TAA or SSAO.

The directional shadow path uses two fitted orthographic cascades around the active camera: a 24-unit near coverage and a 72-unit far coverage, each with a texel-stabilized center and bounded PCF. This stabilizes the map during sub-texel camera motion while retaining slope-aware bias and an adaptive near-cascade kernel that widens only near a receiver blocker to tighten directional shadow contacts. The first enabled spot light has a deterministic bounded 512² perspective depth map with a separate PCF path, and the first enabled point light has a bounded 256² six-face cubemap with linearized depth comparison. This is not a screen-space contact-ray pass.

The sandbox now includes a bounded realism validation row covering rough and polished metal, painted clearcoat, plastic, stone-like roughness, fabric sheen, dry wood, and wet/dry roughness variation under the same HDR environment, direct light, shadow, IBL, reflection-probe, and bloom paths. The stone, fabric, wood, and wet/dry samples use renderer-owned procedural macro, detail-normal, wood-grain, and metallic-roughness textures with their required semantic descriptors. A separate double-sided foliage card uses a bounded alpha-mask texture and participates in the normal shadow/material path. Rendered includes a bounded depth-derived screen-space reflection attempt using the HDR color and depth targets when derived IBL resources are ready; scenes without IBL, missed rays, or out-of-view rays retain the existing environment/analytical fallback. glTF `KHR_materials_transmission` factor and `KHR_materials_volume` attenuation controls now feed bounded environment-based transmission/volume responses; refraction, transmission textures, thickness textures, layered volume, and volumetric fog remain explicit fallbacks or future renderer contracts.
The SSR attempt uses a fixed 16-step ray march with bounded thickness, an 8-unit maximum distance, roughness attenuation, edge confidence, and explicit miss fallback; it remains a screen-space approximation and does not claim planar or production hierarchical reflection coverage. Unsupported material categories continue to use the environment/probe fallback.

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
The renderer diagnostics also report the last scene draw-call count, visible-entity count, conservative frustum-cull count, bounded compatible-instance draw/entity counts, previous-frame occlusion-query tested/culled counts, transparent-sort overflow count, CPU scene-render duration, and optional non-stalling OpenGL timer-query duration. Missing timer-query, instancing, or occlusion-query support is reported through ordinary draw/fallback behavior rather than synthesized capability. Occlusion results are reused only while the camera, scene revision, and object transform remain stable; unavailable or changed state draws normally. These are bounded measurements, not complete draw-cost attribution. They also expose categorized mesh, texture, and render-target bytes, current and peak logical GPU estimates, resource counts, and an explicit arithmetic-overflow state; they are not driver-reported physical VRAM.
That diagnostics surface now also includes HDR, directional, spot, and point shadow target resolution, generation, completeness, and bounded failure text, temporal-history readiness/validity, motion-vector attachment readiness, and enabled/captured local-probe counts, capture generation, and failure count, plus sandbox-level interaction state such as viewport tool mode, cursor ownership, selected object validity, gizmo model validity, hovered handle, active drag target, last rejected interaction reason, and last Action API result.
The sandbox keeps the essential gate state in a compact Scene View strip during interaction, including selected-object highlight state, while the fuller Diagnostics, Transform QA, and Physics QA utility views remain available for deeper packaged testing.
The sandbox workspace now tracks dock zones, allowed dock masks, last valid docks, transient header dragging, native detached-window handles, and dock splitter interactions in session state. Detached production-panel surfaces and `Native Panel Test` use separate-window rendering without routing pointer input into Scene View. Closing a detached production panel safely returns it to its last dock. The Scene View remains the main viewport; full detached controls, saved detached placement, and detaching the viewport remain future work.
The sandbox also layers local editor-control profiles over the engine input actions. Transform actions support bounded key and mouse aliases, validated local settings, protected built-in defaults, named custom profiles, safe fallback, and an Escape-consumption path that lets an active transform restore its original object transform before normal UI, mouse-capture, or exit handling continues. Scene rendering performs conservative camera-frustum sphere culling for bounded non-helper entities and enforces a fixed 8,192-draw scene budget; missing or invalid bounds remain visible so culling cannot hide uncertain content, while budget drops are exposed separately in diagnostics. LOD remains deterministic distance selection, not texture streaming.

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

The Scene View owns an explicit shading mode rather than relying on a global polygon toggle. Wireframe draws neutral geometry edges without texture sampling. Solid draws neutral filled surfaces under a neutral editor surface policy. Material Preview uses the same bounded Cook–Torrance material evaluation and Scene View-sized linear HDR-to-display presentation as Rendered, with deterministic editor lighting instead of scene lighting. Both HDR modes use validated scene-owned environment controls for visible surroundings and diffuse/specular response; Rendered derives transactional IBL resources from the same environment texture. Rendered alone applies scene light policy, exposure, tone mapping, bloom, bounded temporal accumulation, depth-neighborhood AO presentation, and the directional shadow path; Material Preview intentionally omits those scene-dependent post-processing effects. Unlit materials bypass lighting, reserved procedural materials are rejected until a real shader model exists, and helper overlays retain their own materials. Mode changes do not rewrite scene materials, and the renderer restores a filled polygon baseline before UI and detached-window presentation.

The legacy wireframe API remains compatible: enabling it selects Wireframe, while disabling it restores the last valid non-wireframe mode. The sandbox persists the authoritative mode under `ui.scene_view.shading_mode`; older `wireframe_enabled` settings are migrated only when the new key is absent.

Material instances layer validated scalar, vector, alpha-mode, and semantic-texture overrides over the shared glTF material definition. Effective dependency inspection reports the borrowed texture slots after those overrides, while revision-aware refresh preserves explicit instance choices across transactional definition reimport.

Temporal history allocation validates the replacement texture before retiring the previous object. If a resize or texture allocation fails, the previous GPU object remains owned, accumulation is marked invalid for the requested viewport, and diagnostics report the fallback instead of presenting stale history as valid.

The bounded AO horizon search now applies a depth-agreement edge confidence to suppress haloing across discontinuities while retaining the existing radius, thickness, falloff, bias, and intensity controls. Temporal AO history, multi-frame denoise, and production GTAO validation remain unfinished.

KTX2 residency requests now retain a bounded priority alongside their strongest mip target. Visible scene references assign deterministic projected-radius, distance-fallback, and semantic-slot priorities, and the manager services the highest-priority request first with mip-count tie breaking and stable queue order. Each queued request snapshots the manager-owned texture content revision; reimport or another transactional replacement cancels stale work before GPU commit and reports a cumulative cancellation count. Active scene replacement can also cancel all pending requests transactionally before the new scene is installed. Residency diagnostics separately report known failed payload bytes, readable encoded source bytes rejected before residency, unknown-size request failures, unknown-size source failures, active pinned bytes, and the explicit synchronous-main-thread progression mode. The active-frame API clears and records bounded pins on manager-owned texture entries; visible references use it so trim cannot evict a texture needed by the current frame, and its explicit end operation releases pins immediately for scene replacement or other early exits. This remains synchronous and does not claim background I/O or automatic residency policy.
