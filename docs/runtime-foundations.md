# Runtime Foundations

Henka Engine provides reusable runtime foundations for samples, tools, the Sandbox workspace, dedicated-server paths, and external game repositories.

> **Status:** Active production foundation. Public APIs are bounded, ownership is explicit, mutation paths are transactional where required, and validation is tied to the real runtime path.

## Contents

- [Input actions](#input-actions)
- [Scene objects and identity](#scene-objects-and-identity)
- [Camera](#camera)
- [Action API](#action-api)
- [Engine lifecycle](#engine-lifecycle)
- [Asset identity and ownership](#asset-identity-and-ownership)
- [Texture formats and residency](#texture-formats-and-residency)
- [Material and renderer foundations](#material-and-renderer-foundations)
- [Environment, IBL, and reflection probes](#environment-ibl-and-reflection-probes)
- [Workspace and viewport](#workspace-and-viewport)
- [Temporal reconstruction and AO](#temporal-reconstruction-and-ao)
- [Shadows and realism runtime paths](#shadows-and-realism-runtime-paths)
- [Viewport interaction](#viewport-interaction)
- [Picking and gizmos](#picking-and-gizmos)
- [Rigid-body physics](#rigid-body-physics)
- [Audio](#audio)
- [Asset metadata](#asset-metadata)
- [Interactions](#interactions)
- [Save data](#save-data)
- [Diagnostics](#diagnostics)
- [Package modes and validation](#package-modes-and-validation)
- [Structural and numeric safety](#structural-and-numeric-safety)
- [Viewport shading and environment modes](#viewport-shading-and-environment-modes)
- [Current limits](#current-limits)

## Input actions

Henka exposes a named action layer over raw keyboard and mouse input.

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

Raw input remains available. Sandbox camera movement and panel toggles also use action queries.

Local editor-control profiles layer on top of engine input actions. Transform actions support:

- bounded key aliases;
- bounded mouse aliases;
- validated local settings;
- protected built-in defaults;
- named custom profiles;
- safe fallback;
- Escape consumption for active transform cancellation.

An active transform restores its original object transform before normal UI, mouse-capture, or exit handling continues.

## Scene objects and identity

Scene entities expose:

- opaque stable entity handles;
- name;
- optional tag;
- transform;
- visibility;
- optional local bounds;
- optional interaction metadata;
- material names and other bounded text fields through scene-owned storage.

Scene identities use opaque 64-bit generation-checked handles. Destroying an entity advances its slot generation before reuse. Stale selections, action records, physics links, and authoring references therefore cannot resolve to a replacement entity.

Names, tags, material names, and interaction prompts are copied into bounded scene-owned storage. Callers may supply temporary input buffers. Returned text pointers remain valid until the corresponding value changes or the entity is destroyed.

### Transform lock

Entities can carry a transform-lock flag.

The Action API blocks move, rotate, and scale operations on locked entities. Selection, inspection, camera focus, visibility changes, and reset-to-default remain available.

The Sandbox Ground begins locked and remains inspectable. Locked selections do not display the yellow transform highlight and cannot start action-based transform hotkeys. Unlocking requires an explicit entity control.

Selection, visibility, tool-mode, and lock changes cancel active transform sessions. Clicking empty Scene View background clears selection in Select, Move, Rotate, and Scale modes.

Logical imported-object selection aggregates visible owned render children for topology outline and framing. Hidden children remain available to their rendering and explicit subobject workflows.

## Camera

The reusable camera API provides:

- perspective camera creation;
- orthographic camera creation;
- pitch clamping;
- look-at orientation;
- camera reset;
- relative movement;
- focus on bounds;
- framing on bounds;
- orbit;
- pan;
- dolly;
- screen-point ray creation;
- world-to-screen conversion.

### Startup and reset framing

Normal Sandbox startup and Reset View share one scene-first framing authority.

The framing path:

1. collects visible non-helper mesh bounds;
2. excludes editor grid and ground geometry;
3. prefers showcase-prefixed content when present;
4. uses other meaningful visible meshes for arbitrary projects;
5. falls back to the deterministic empty-scene camera when required.

A versioned settings migration replaces only the legacy generated origin/floor-facing pose. Valid changed user poses remain preserved.

### Camera validation

Constructors sanitize invalid field of view, aspect, near plane, far plane, and orthographic height values to documented defaults.

`henka_camera_is_valid` is the shared validation contract used by scene assignment and projection helpers.

Screen-ray and world-to-screen conversion:

- reject non-finite state;
- reject degenerate state;
- initialize caller outputs deterministically on failure.

Camera mutation helpers reject non-finite deltas without corrupting persistent camera state.

## Action API

Henka exposes a local Action API for validated scene and object operations.

Current V1 coverage includes:

- scene summary;
- object listing;
- object create;
- object delete;
- object rename;
- object select;
- object-details queries;
- position changes;
- rotation changes;
- scale changes;
- move commands;
- rotate commands;
- scale-by-multiplier commands;
- transform reset when a registered default exists;
- visibility actions;
- camera focus when camera context exists;
- dry-run validation;
- structured results.

Engine-owned contexts retain authority for these operations. Tests, tools, and workspace surfaces use the same action boundary.

## Engine lifecycle

Each engine instance owns copied application, asset-base, and user-data path strings after creation.

One instance admits one run lifecycle:

- recursive initialization is rejected;
- completed runs cannot be restarted;
- failed runs cannot be restarted;
- destruction is rejected during initialization or the active run loop;
- a successful lifecycle schedules at most one shutdown callback.

Close and explicit exit requests stop before another update or render.

### Multi-window frame ownership

The renderer retains logical frame ownership after a failed main swap so a checked abort can run.

Detached-window UI is drawn and presented before the main window. A detached-window failure aborts the still-unpresented main frame.

This is a best-effort multi-window commit boundary. Atomic operating-system presentation is not part of the current contract.

### Authoring ownership direction

Editable authoring data retains stable identities and undoable source state. Evaluated runtime rendering and physics data are produced from that source through bounded publication paths.

The Sandbox authoring wrapper also provides bounded Edge-mode loop selection. Starting from the active edge, it walks opposite edges through both sides of a closed manifold quad strip and publishes only after traversal closes.

The operation preserves prior selection on:

- boundary edges;
- non-quad faces;
- malformed connectivity;
- selection-budget exhaustion.

## Asset identity and ownership

### glTF material identity

glTF-backed material assets use the same canonical source identity as the mesh import path.

They:

- retain the shared `henka_material` model;
- borrow manager-owned semantic textures;
- expose copied material instances for scene assignment;
- build complete reload candidates through bounded parsing, confined dependency resolution, and semantic validation;
- replace stable asset values only after candidate success.

Reload failure preserves the previous value and identity.

### Canonical file-backed cache identity

Manager-loaded shaders, textures, OBJ meshes, and manager-owned Audio clips use canonical confined path identities.

The asset boundary rejects:

- rooted paths;
- UNC paths;
- device paths;
- drive-qualified paths;
- traversal paths;
- URI-like paths.

Equivalent slash and dot-segment spellings resolve to one cache entry.

Windows cache identity folds ASCII case while retaining the normalized spelling from the first successful load in metadata. Case-sensitive platforms keep case-sensitive identity.

Returned asset pointers are borrowed and manager-owned. Public destroy calls ignore manager-owned borrowed resources. Manager shutdown performs the owning destruction. Shader, texture, mesh, and Audio loads require empty output slots and preserve non-empty caller slots when rejected or failed.

### Texture fallback and retry

Texture source failures create path-specific lightweight aliases of the shared error texture.

A failed retry preserves:

- alias identity;
- metadata;
- caller output.

A successful retry moves the owned backend into the existing alias. Existing material references therefore observe the real texture without pointer rebinding.

Allocation failure, invalid API state, and OpenGL upload failure propagate as hard errors and are not stored as ordinary source fallbacks.

### Texture creation contract

Texture creation:

- reads encoded source data once into a bounded buffer;
- uses the same bytes for stb_image inspection and decode;
- supports Radiance HDR as finite linear RGBA32F uploaded to RGBA16F;
- rejects unsupported 16-bit sources;
- clears outputs before validation;
- temporarily activates the main OpenGL context;
- restores the previous context;
- restores texture unit, binding, and unpack state;
- checks upload errors;
- destroys only owned backends.

Descriptor semantics select sRGB or linear GPU storage and are immutable on the texture object.

OBJ mesh fallback metadata currently remains path-based. Mesh retries do not yet use stable path-specific aliases.

## Texture formats and residency

### KTX validation

The KTX boundary validates each bounded mip's dimensions and expected pixel/block byte count before publication.

Texture info reports:

- exact logical resident GPU bytes;
- total mip count;
- resident mip count;
- compressed-GPU-format selection;
- selected BC, ETC2, ASTC, or RGBA8 resident format.

Uncompressed and Basis sources can use RGBA8 when compressed upload is unavailable.

Native BC1 RGB/RGBA and BC3 payloads are checked against independent GPU capabilities. DXT1 support does not authorize BC3 upload.

Compressed payloads without matching GPU capability fail closed. The KTX boundary does not reinterpret compressed blocks as RGBA8.

Basis normal-map target preference is:

1. BC5 or ETC2-RG;
2. BC7 or ASTC RGBA when available;
3. checked RGBA8 fallback.

BC1 and BC3 are excluded from Basis normal-map selection because they do not preserve the required two-channel normal representation.

### Residency budget

The asset manager can enforce a configured texture residency budget before publishing a new source load.

Diagnostics expose:

- budget rejection;
- resident bytes;
- managed texture count;
- fallback count.

Applications configure the budget through `henka_engine_config.texture_residency_budget_bytes`.

- `0` keeps the unlimited default.
- A nonzero value enables bounded enforcement.
- A capped trim operation returns `HENKA_ERROR_LIMIT` when the target cannot be reached.

The frame lifecycle performs at most one configured-budget trim per frame.

Visible manager-owned textures are pinned for the active frame before trimming. Diagnostics report bounded pin count and active pinned bytes.

### KTX2 top-mip requests

Manager-owned KTX2 textures support synchronous requests for a bounded top-mip prefix.

The operation:

1. rereads the source;
2. validates it;
3. uploads a replacement backend;
4. changes the existing texture identity only after success.

Budget or source failure preserves the prior backend and cache entry.

PNG and HDR sources are not streamable through this API.

### Request queue and priority

KTX2 requests can be coalesced into a bounded queue with completion, failure, and cancellation counters.

Visible scene materials, including configured Terrain layer texture dependencies, enqueue bounded mip targets using:

- projected texture radius;
- deterministic distance fallback;
- semantic-slot priority.

Repeated requests retain the strongest mip target and priority. A farther reference cannot demote a nearer request.

The manager services the highest-priority request first, using mip-count tie breaking and stable queue order.

Each queued request snapshots the manager-owned texture content revision. Reimport or another transactional replacement cancels stale work before GPU commit. Active scene replacement can cancel all pending requests before installing the new scene.

### Trim policy

A deterministic trim operation can reduce least-recently pinned eligible KTX2 textures to one resident mip until a caller target is met.

Equal-use candidates prefer the larger texture.

Diagnostics report:

- trimmed bytes;
- demoted bytes;
- transactional failure/rollback state.

Whole-resource eviction remains future work.

### Windows asynchronous source-read mode

The default residency mode remains synchronous on every platform.

Windows can opt into bounded asynchronous progression. One worker reads one source file at a time. Render-thread process work still performs validation and GPU replacement.

Worker jobs snapshot:

- texture content revision;
- cancellation sequence.

Reimport, scene replacement, and explicit cancellation prevent stale publication.

Non-Windows builds return an explicit platform result for this mode.

Background decode, automatic full-frame residency policy, and whole-resource eviction remain unfinished.

### Residency stress validation

Windows `--residency-stress` uses public asset APIs to exercise:

- 65 path-distinct manager-owned PNG textures;
- shared-path cache identity;
- configured-budget rejection;
- two pinned active references;
- bounded queue fill and recovery;
- failed non-KTX2 residency requests;
- pending-work cancellation.

With KTX enabled, the stress path also creates a build-local three-level uncompressed KTX2 fixture and validates:

- load through the manager;
- promotion from one to two resident mips;
- promotion from two to three resident mips;
- trim back to one mip;
- transactional diagnostics.

A readable corrupt fixture proves known source-failure bytes. A missing texture proves unknown-source-size accounting.

Before the live-scene pass, the fixture returns to one resident mip. The rendered far/near/return camera phase records resident-mip transitions, including far-camera demotion after near-camera promotion.

The scenario also writes a larger native BC1 KTX2 chain:

- supported BC1 devices validate compressed trim and promotion;
- unsupported devices fail the native-compressed source and record `BC1=unsupported`.

This stress mode covers ownership, queueing, budget enforcement, mip progression, failure accounting, lifetime behavior, visibility thresholds, and the bounded Windows worker-read path.

### Residency byte diagnostics

Diagnostics retain cumulative counts for:

- bytes uploaded through manager-owned texture creation or replacement;
- bytes removed by successful trim demotion;
- resident payload bytes rejected after a successful replacement upload was prepared;
- encoded source bytes from readable files rejected before residency;
- unknown-size missing or unreadable source failures.

No byte count is fabricated.

These counters cover the bounded synchronous lifecycle and optional Windows source-read worker. Complete background decode, automatic residency policy, and whole-resource streaming remain future work.

## Material and renderer foundations

### Material data

The current metallic-roughness material model includes:

- base color;
- metallic;
- roughness;
- tangent-space normal scale;
- occlusion strength;
- emissive color;
- emissive strength;
- bounded clearcoat;
- optional sheen color;
- optional sheen roughness;
- alpha mode;
- double-sided state;
- cast-shadow intent;
- receive-shadow intent;
- bounded volume attenuation;
- runtime-authored subsurface amount;
- runtime-authored subsurface tint.

Material assignment validates texture semantics and color space.

### Material instances

Manager-owned glTF material definitions create stack-owned instances with:

- validated parameter overrides;
- revision-aware refresh after transactional reimport;
- semantic dependency inspection;
- transactional reset for one or all overrides;
- transactional application of the validated effective view to a scene entity.

The glTF definition remains the shared material authority. No second JSON material schema is introduced.

Imported transmission-scalar and volume-thickness textures remain manager-owned linear dependencies.

The Sandbox Object Details panel creates a bounded persistent material instance for selected imported entities that retain material-definition identity.

Per-entity overrides survive definition revision changes. Reimport resolves through the definition identity using the standalone material cache or owning glTF-scene transaction.

Editor controls expose:

- scalar values;
- vector values;
- booleans;
- alpha mode;
- semantic texture clear/restore;
- dependencies;
- definition revision;
- reset.

Texture rows cover the five core material slots plus imported transmission and volume-thickness slots.

Built-in procedural materials remain inspect-only. Text-entry import, drag/drop, material-file authoring, and dedicated dependency-graph panels remain future work.

### PBR evaluation

The OpenGL path uses:

- GGX distribution;
- correlated Smith visibility;
- Schlick Fresnel;
- dielectric F0;
- energy-conserving diffuse/specular separation;
- bounded clearcoat;
- bounded sheen;
- base-layer transmission attenuation before secondary response.

When subsurface amount is nonzero, direct sun, moon, and supported local lights add a bounded three-lobe diffusion-profile approximation tinted by the shared subsurface color.

Authored scalar or glTF linear thickness data widens the response. A geometric-normal derivative provides a bounded curvature proxy at thin transitions.

Current subsurface response is a local direct-light approximation. True multi-scatter diffusion, production skin/wax profiles, screen-space SSS, and ray-traced SSS remain future work.

### Tangents and UV spheres

Generated UV spheres use nondegenerate fan caps.

Mesh-upload tangent accumulation starts from zero. Valid UV derivatives therefore drive the tangent basis. Degenerate UVs receive finite orthogonal fallbacks.

Model data can carry a finite imported tangent frame and handedness. Upload preserves imported tangents after normal orthogonalization. Legacy and procedural data use generated tangents.

Full MikkTSpace conformance remains future work.

### Blended entities

Blended entities render after opaque and masked entities.

Up to 4,096 blended entities use an allocation-free `O(n log n)` back-to-front sort with explicit depth/index tie breaking.

Overflow uses deterministic entity order and preserves the opaque-first pass. Diagnostics expose overflow count.

### Local lights and fog

Scenes support a bounded four-light point/spot list with:

- normalized directions;
- inverse-square/range falloff;
- spot cones.

Current GL 3.3 local shadow resources include:

- two fitted directional cascades;
- one deterministic 512² depth map for the first enabled spot light;
- one bounded 256² cubemap for the first enabled point light.

Distance fog supports:

- linear;
- exponential;
- exponential-squared modes.

Distance fog is disabled by default. Volumetric fog is not implemented.

Renderer diagnostics also report bounded distance-based LOD selection and configured-LOD fallback use. This is a selection/culling foundation. Texture streaming and residency use the separate asset-manager path.

## Environment, IBL, and reflection probes

### Runtime-adopted textures

`henka_assets_adopt_runtime_texture` can move runtime-created textures into the manager dependency graph.

The API validates:

- confined stable identity;
- live GPU payload;
- configured residency budget.

Ownership transfers on success. Duplicate identities are rejected. Metadata reports the lack of source-file reload for adopted runtime textures.

The Sandbox periodic HDR fixture uses this path under `runtime/environment/studio`.

### HDR environment

Scene environments can borrow a validated linear HDR equirectangular texture for background and material environment response.

The renderer derives the following resources transactionally when GPU support is available:

- irradiance;
- prefiltered specular;
- BRDF LUT.

The analytical gradient remains the fallback when derivation fails.

The renderer never owns the borrowed scene environment pointer.

IBL material roughness lookup is capped at the generated 32x32 prefilter level.

### Local reflection probes

Optional local probes own bounded 64x64 RGBA16F cubemaps with a seven-level mip chain.

Roughness lookup is capped at the 16x16 prefilter level.

One changed probe is captured at a time using:

- six deterministic camera directions;
- explicit per-face up vectors;
- the camera's validated roll-aware look-at path.

Probe sampling is disabled during capture to prevent recursion.

Candidate faces are committed only after the complete capture succeeds.

Capture validation and restoration cover:

- latent OpenGL errors;
- caller cube-texture binding;
- framebuffer;
- renderbuffer;
- viewport;
- clear color;
- active cube-texture state;
- per-face draw-path GL errors.

Enabled probes are ranked deterministically. When two captured volumes overlap a receiver, bounded inverse-score weighting blends both specular and diffuse contributions.

Box projection applies independently to each captured probe. Invalid or uncaptured probes use shared IBL resources.

The Sandbox can visualize enabled probe volumes and box-projection mode without mutating scene data.

### Double-sided lighting

Double-sided material draws disable face culling and orient backface geometric normals toward the viewer for consistent lighting.

## Workspace and viewport

Henka exposes a viewport-first dock layout foundation with:

- dock-region layout math;
- dedicated Scene View bounds;
- viewport aspect-ratio helpers;
- window-point to viewport-local conversion;
- window-point to framebuffer-point conversion for scene interaction.

Workspace math uses checked double-precision intermediates. Caller outputs clear on failure. Integer viewport edges are produced only after framebuffer clipping.

Viewports reject:

- negative origins;
- overflowing edges.

Custom Scene View rectangles remain clipped to the framebuffer and survive framebuffer resize when still valid.

### Docking and panel layout

The Sandbox layers:

- safe panel paging;
- persisted native detached-panel placement;
- ordered side-dock panel groups;
- dock zones;
- allowed dock masks;
- last-valid-dock ownership;
- transient header dragging;
- bounded detached-window positions;
- dock splitter interactions.

Cross-docked panels stack in side docks.

### Multi-window platform foundation

Secondary OS-level tool windows:

- receive collision-safe engine identifiers;
- validate native identifiers before admission;
- route close, focus, resize, move, and pointer events separately from Scene View input;
- can present their own UI-only OpenGL surface.

Focus loss synthesizes releases for held main-window keys and mouse buttons, releases a held detached-window left button, and clears transient motion.

Detached-window press, release, resize, and position state updates per frame. Close and resize diagnostics remain aggregated for the full event poll.

VSync and mouse capture report failures. Platform queries clear caller outputs on failure. Diagnostics report the latest verified multi-window capability.

Main and detached-window OpenGL context transitions are checked before resource deletion. Frame abort reports context-restoration failure while preserving logical frame ownership.

Scene drawing binds each entity texture or texture zero for every entity, preventing stale texture state from leaking across objects.

The Sandbox uses this foundation for:

- `Native Panel Test`;
- detached production-panel surfaces;
- routed release-confirm input;
- explicit dock-return controls;
- bounded saved placement;
- title-bar drag-back recognition.

Detachable Scene View remains future work.

### Viewport tools

The Sandbox exposes visible tool modes for:

- Select;
- Orbit;
- Pan;
- Move;
- Rotate;
- Scale.

These tool states drive the same viewport math used by interaction tests and runtime behavior.

## Temporal reconstruction and AO

### HDR presentation

HDR presentation includes:

- bounded half-resolution bloom extract;
- separable blur bloom;
- a small Rendered-only post grade;
- active tone mapping through recoverable target failure.

Material Preview omits scene-dependent post effects.

GPU presentation targets are replaced transactionally. Allocation or framebuffer validation failure keeps tone mapping active and exposes fallback diagnostics.

### Temporal resources

Rendered keeps:

- one bounded display-space RGBA8 history texture;
- a Scene View-sized depth-history texture/FBO;
- a Scene View-sized RG16F motion attachment;
- an R8 reactive attachment;
- a sampled current-depth attachment.

The built-in material shader writes camera and object motion from current and previous view-projection matrices. Transparent, transmissive, and emissive pixels are marked reactive.

The tone-map pass:

1. reprojects history by motion;
2. compares against retained previous depth;
3. rejects invalid or depth-inconsistent history;
4. reduces accumulation for reactive and high-motion pixels;
5. clamps history to the current 3x3 tone-mapped neighborhood;
6. applies bounded neighborhood reconstruction sharpening.

Rendered projection uses an eight-sample Halton jitter sequence.

History and previous view-projection invalidate when:

- the temporal path becomes unavailable;
- shading mode changes;
- viewport size changes;
- a large camera/projection discontinuity occurs.

Color, motion, reactive, current-depth, and previous-depth targets are replaced transactionally on resize. History remains invalid until the first successful color/depth copy.

Custom shaders without the optional motion contract and entities without valid vectors contribute zero motion.

### Temporal diagnostics

Diagnostics report:

- temporal fallback state;
- cumulative history resolves;
- cumulative fallback frames;
- current invalidation/fallback reason;
- invalidation count;
- history-target allocation failures;
- previous-resource retention after failed replacement;
- `history valid` after a successful color/depth commit.

### Temporal stress

Windows `--temporal-stress` exercises the public scene path by:

1. resizing Scene View;
2. restoring Scene View size;
3. moving the camera;
4. changing projection;
5. hiding a visible entity;
6. restoring the entity;
7. returning to the baseline camera.

The normal Rendered loop reports resolve, fallback, invalidation, allocation-failure, and prior-resource-retention state throughout the sequence.

The command fails when bounded recovery does not return history to a valid state.

This is deterministic runtime invalidation coverage. Production TAA visual approval remains open.

### Ambient occlusion

The current AO presentation uses a bounded four-direction, two-sided, multi-step view-space horizon search.

Controls include:

- radius;
- thickness;
- falloff;
- bias;
- intensity;
- depth-agreement edge confidence.

Depth-agreement confidence suppresses haloing across discontinuities.

Temporal AO history, multi-frame denoise, and production GTAO validation remain unfinished.

## Shadows and realism runtime paths

### Directional shadows

The directional path uses two fitted orthographic cascades around the active camera:

- near coverage: 24 units;
- far coverage: 72 units.

Each cascade uses:

- texel-stabilized center;
- bounded PCF;
- nearest depth fetches for manual blocker comparison;
- slope-aware bias.

The receiver shader blends near/far visibility through a bounded overlap around the split. The near-cascade kernel widens only near detected receiver blockers.

### Spot and point shadows

The first enabled spot light uses a deterministic 512² perspective depth map with a dedicated PCF path. Out-of-projection taps use a lit border value.

The first enabled point light uses a bounded 256² six-face cubemap with linearized depth comparison.

Screen-space contact-ray shadows are not implemented.

### Material stress

Windows `--material-stress` uses shared glTF material definitions and stack-owned material instances to exercise:

- supported scalar overrides;
- vector overrides;
- base color;
- booleans;
- alpha mode;
- transmission texture;
- thickness texture;
- other supported semantic texture overrides.

The sequence validates invalid-edit retention, applies the effective material transactionally, refreshes definition revision, resets overrides, and reapplies.

Failure restores the prior instance and scene material.

This is runtime material-instance API coverage. Persisted material-file authoring remains future work.

### Realism validation row

The Sandbox realism row includes:

- rough metal;
- polished metal;
- painted clearcoat;
- plastic;
- stone-like roughness;
- fabric sheen;
- dry wood;
- wet/dry roughness variation;
- subsurface-tinted backscatter sphere.

These subjects share the HDR environment, direct light, shadows, IBL, reflection probes, and bloom paths.

Stone, fabric, wood, and wet/dry samples use manager-owned runtime macro, detail-normal, wood-grain, and metallic-roughness textures with correct semantic descriptors.

A separate double-sided foliage card uses a bounded alpha-mask texture and the normal shadow/material path.

### Transmission, SSS, and SSR runtime path

glTF `KHR_materials_transmission` factor and `KHR_materials_volume` attenuation plus linear thickness-texture controls feed the current bounded transmission/volume response.

Runtime-authored subsurface controls feed the thickness-shaped three-lobe direct-light and back-facing environment approximation.

Current renderer limits include:

- no production screen-space refraction;
- no layered-volume solution;
- no true multi-scatter SSS;
- no production glass;
- no volumetric fog.

Rendered carries authored per-pixel roughness into the HDR target for SSR.

The retained SSR path uses:

- 16 ray-march steps;
- four bounded local hit-refinement samples;
- signed depth-crossing validation;
- valid central depth-neighbor reconstruction;
- back-facing-hit rejection;
- edge-invalid-hit rejection;
- bounded thickness;
- 8-unit maximum distance;
- quadratic roughness attenuation;
- five-tap roughness-aware resolve;
- edge confidence;
- smooth-material eligibility;
- explicit miss fallback.

Rough materials outside the screen-space eligibility bound use filtered environment/probe response. Unsupported material categories also use environment/probe fallback.

Planar and production hierarchical reflections remain future work.

## Viewport interaction

Deterministic viewport interaction helpers include:

- viewport-aware mouse conversion;
- world-to-screen projection;
- projected gizmo handle models;
- screen-space handle hit testing;
- drag-state creation from a visible handle;
- deterministic move drag math;
- deterministic rotate drag math;
- deterministic uniform-scale drag math.

Runtime and tests share the same handle-model and drag helpers.

Interaction-gate helpers expose explicit reject reasons. Runtime diagnostics therefore report why an interaction did not start.

Panel ownership uses current visible panel rectangles and framebuffer-space mouse coordinates, matching selection and gizmo hit-test coordinates.

Manual desktop QA remains required for handle readability, drag comfort, and interaction feel.

## Picking and gizmos

### Picking

Current picking foundations include:

- screen point to world ray;
- viewport-relative window-to-ray conversion;
- ray/bounds tests;
- nearest visible object pick.

Sandbox picking and gizmo drag start only inside the dedicated Scene View when mouse capture is released.

### Transform gizmos

Current transform-gizmo foundations include:

- world-axis move helpers;
- world-axis rotation helpers;
- uniform scale helpers;
- move snapping;
- rotation snapping;
- scale snapping;
- viewport-aware hit testing;
- world-to-screen projection;
- projected handle hit helpers;
- stable drag cancellation across viewport changes and invalid targets;
- shared projected handle-model logic used by drawing, runtime interaction, and tests.

Helper scene entities used for gizmo visualization remain internal to the tool path, hidden from the normal runtime view, excluded from ordinary scene picking, and excluded from persisted/user-facing selection.

The Sandbox also exposes direct Action API transform controls for packaged QA.

## Rigid-body physics

Henka provides deterministic rigid-body physics V1 with:

- physics-world creation;
- gravity;
- fixed-timestep accumulation;
- explicit fixed stepping;
- reset;
- static bodies;
- dynamic bodies;
- kinematic bodies;
- stable body handles;
- linear velocity;
- angular velocity;
- force;
- impulse;
- torque;
- damping;
- restitution;
- friction;
- sphere colliders;
- axis-aligned box colliders;
- plane colliders;
- collision masks;
- trigger flags;
- brute-force pair generation for current small scenes;
- collision response;
- collision and trigger enter/stay/exit events;
- raycasts;
- optional scene-entity transform links;
- debug-shape queries from real collider data.

### Transactional fixed substeps

Bodies, contacts, pairs, and events use bounded checked capacity growth.

Every fixed substep executes against scratch:

- body storage;
- contact storage;
- current-pair storage;
- previous-pair storage;
- event storage.

A fully successful candidate replaces live arrays.

Allocation failure returns `HENKA_ERROR_OUT_OF_MEMORY`.

Integration, geometry, contact, impulse, friction, or correction arithmetic outside representable valid state returns `HENKA_ERROR_NUMERIC_RANGE`.

Both failure classes preserve:

- prior bodies;
- contacts;
- events;
- pair history;
- accumulator;
- scene-linked transforms.

No partial event set is published.

Numeric bounds derive from floating-point representation, fixed timestep, collider geometry, and normalized quaternion/contact contracts. No arbitrary gameplay-scale cap is imposed.

Earlier successful substeps in one catch-up update remain committed when a later substep fails. Remaining accumulator time can be retried.

Scene links are written after physics commit on a best-effort basis.

### Body destruction

Body destruction reserves required EXIT-event capacity before mutation. It removes records involving that body, preserves unrelated queued events and previous-pair history, and recomputes collision flags for affected survivors.

### Additional physics safety

- Query failures clear caller-visible outputs.
- Plane collider offsets affect contacts and raycasts.
- Local plane normals are rotated by body transform for contacts and raycasts.
- Sandbox collider overlays and selection highlights use the same transformed plane geometry.
- Rays beginning inside an AABB return the exit surface with a valid normal.
- Catch-up is capped at 16 fixed substeps.
- Excess whole-step backlog is discarded after the cap.
- Later zero-delta calls do not continue discarded stale simulation time.
- Sandbox bodies begin static.
- Isolated drop activates the selected supported body only.
- Full Physics QA explicitly activates its intended dynamic set.

V1 uses axis-aligned boxes. Rotated-box collision, mesh colliders, constraints, continuous collision detection, full character-controller movement, and advanced simulation remain future work beyond the available dynamic-body controller foundation.

## Audio

Henka's Audio foundation now participates in the shared runtime and public asset path.

Current production foundations include:

- resident PCM WAV clips;
- manager-owned canonical Audio asset identity;
- Audio metadata through the asset manager;
- fixed-capacity generation-checked voices;
- Master, Music, SFX, Dialogue, Ambience, and UI buses;
- per-bus gain;
- listener orientation;
- distance attenuation;
- stereo panning;
- looping;
- pitch;
- deterministic stereo float-PCM mixing;
- scene-entity spatial binding;
- stale-entity cleanup;
- persisted Scene Document emitter configuration;
- Play-session emitter instantiation;
- graphical Sandbox camera/listener mapping;
- caller-pumped SDL3 output;
- renderer-free dedicated-server execution;
- external public-API validation with a real WAV asset and real scene object.

The external consumer validates persistence, object movement, listener movement, spatial response, and stale cleanup through public Henka APIs.

Streaming long-form assets, broader decoding, listener authoring, asset reload, editor controls, scripting bindings, device-loss recovery, and broader packaged proof remain active Audio work.

See [audio.md](audio.md).

## Asset metadata

The asset manager tracks read-only metadata for cached assets:

- asset type;
- source path;
- display name;
- loaded state;
- fallback state;
- reload eligibility;
- short summary;
- short error summary.

Source and display names are cache-owned strings.

Shader, texture, mesh, and Audio cache growth uses bounded checked capacity handling. Asset paths and scene entity storage are bounded as well.

Current metadata is an inspection layer. Full content-browser and general hot-reload workflows remain future work.

## Interactions

Scene objects can expose:

- enabled state;
- prompt text;
- maximum interaction distance.

The runtime returns generic interaction eligibility as:

- available;
- disabled;
- out of range.

Prompt text is scene-owned. Distances must be finite and non-negative. Non-finite observer positions produce unavailable eligibility.

## Save data

Settings and game-save foundations are separate APIs.

`henka_settings` provides key/value preferences.

`henka_save_data` currently provides:

- version;
- scene ID;
- camera pose;
- boolean flags;
- slot-based path construction under the user-data directory.

This is a bounded local save foundation. Complete shipped-game save systems remain future work.

### Save-path safety

Generic confined relative-path resolution remains available for assets and standalone relative paths without a base directory.

Save-slot construction requires a non-empty user-data base path. Save files therefore cannot silently resolve to the process working directory.

Camera poses reject out-of-range pitch. Failed pose reads clear caller outputs. Save-flag names are bounded so accepted names remain serializable after the `flag.` prefix is added.

Sandbox camera speed and input sensitivity are validated before use and fall back to safe defaults when required.

Normal editor startup uses scene-first framing and does not restore old transient camera-pose keys.

## Diagnostics

The engine exposes a compact diagnostics snapshot containing:

- delta time;
- frame time;
- FPS;
- frame index;
- framebuffer size;
- wireframe state;
- mouse-capture state;
- UI visibility;
- package mode.

The Sandbox surfaces this data in-window.

### Renderer and scene diagnostics

Renderer diagnostics also report:

- last scene draw-call count;
- visible-entity count;
- conservative frustum-cull count;
- compatible instance draw/entity counts;
- previous-frame occlusion-query tested/culled counts;
- transparent-sort overflow count;
- CPU scene-render duration;
- optional non-stalling OpenGL timer-query duration;
- categorized mesh bytes;
- texture bytes;
- render-target bytes;
- current logical GPU estimate;
- peak logical GPU estimate;
- resource counts;
- arithmetic-overflow state.

Missing timer-query, instancing, or occlusion-query support follows ordinary fallback/draw behavior. Occlusion results are reused only while camera, scene revision, and object transform remain stable.

Logical GPU byte estimates are not driver-reported physical VRAM.

### Rendering-state diagnostics

Additional state includes:

- HDR target resolution, generation, completeness, and failure text;
- directional shadow state;
- spot shadow state;
- point shadow state;
- temporal-history readiness and validity;
- motion-vector attachment readiness;
- enabled local-probe count;
- captured local-probe count for the current scene revision;
- monotonic local-probe capture generation;
- probe-capture failure count.

The monotonic capture generation remains durable evidence that a transactional capture completed even if a later scene revision invalidates the current-frame probe.

### Interaction diagnostics

Sandbox diagnostics include:

- viewport tool mode;
- cursor ownership;
- selected-object validity;
- selected-object highlight state;
- gizmo-model validity;
- hovered handle;
- active drag target;
- last rejected interaction reason;
- last Action API result.

A compact Scene View strip keeps essential gate state visible during interaction. Full Diagnostics, Transform QA, and Physics QA remain available for deeper packaged validation.

### Scene draw bounds

Scene rendering uses conservative camera-frustum sphere culling for bounded non-helper entities and a fixed 8,192-draw scene budget.

Missing or invalid bounds remain visible. Draw-budget drops are reported separately.

LOD uses deterministic distance selection.

## Package modes and validation

### Package modes

Henka exposes:

- Auto;
- Development;
- Packaged.

In `Auto`, the engine checks for `PACKAGE_INFO.txt` beside runtime assets and reports `Packaged` when present.

This is a runtime package-state distinction. Installer and full release-distribution systems remain future work.

### Hosted package validation

The hosted Windows workflow has a package-contract mode that validates:

- provenance;
- executable hashes;
- required files;
- run guidance;
- offline help.

This hosted path does not require a desktop OpenGL session.

### Local release evidence

Local release evidence requires:

- full packaged runtime smoke test;
- native detached-window desktop harness;
- clean shutdown;
- application-only screenshots.

## Structural and numeric safety

### Timing

Windows timing uses the monotonic high-resolution performance counter with a monotonic tick-count fallback.

Supported non-Windows builds use `CLOCK_MONOTONIC` when available.

### Vector and quaternion normalization

Vector and quaternion normalization scales finite inputs before magnitude computation. Very large finite values therefore remain normalizable without intermediate overflow.

Quaternion multiplication uses double-precision intermediate products before normalization.

### Scene transform/bounds invariant

Scene transforms and local bounds form a paired invariant.

Updates are rejected when derived world center, extents, or min/max corners cannot remain finite and representable.

World-bound query failures clear output deterministically.

Scene picking normalizes finite ray directions internally, including directions whose unnormalized length exceeds finite float range.

### Physics geometry boundaries

Physics body creation and collider/transform replacement validate:

- derived collider centers;
- radii;
- box extents;
- plane normals;
- plane offsets;
- representable shape boundaries.

Finite source values that produce unrepresentable persistent collision geometry are rejected before body mutation.

Physics raycasts use the same scaled finite-direction normalization path.

## Viewport shading and environment modes

The Scene View owns explicit shading modes:

- Wireframe;
- Solid;
- Material Preview;
- Rendered.

### Wireframe

Wireframe draws neutral geometry edges without texture sampling.

### Solid

Solid draws neutral filled surfaces under an editor surface policy. Explicit unlit line materials remain available for the editor grid.

### Material Preview

Material Preview uses the bounded Cook-Torrance material evaluation and Scene View-sized linear HDR-to-display presentation with deterministic editor lighting.

It omits scene-dependent Rendered post effects.

### Rendered

Rendered uses scene light policy and includes:

- exposure;
- tone mapping;
- bloom;
- bounded temporal accumulation;
- depth-neighborhood AO presentation;
- directional shadowing;
- supported local shadows;
- environment response.

Unlit materials bypass lighting. Reserved procedural materials remain unavailable until a production shader model exists. Helper overlays retain their own materials.

Mode changes preserve scene materials. The renderer restores a filled polygon baseline before UI and detached-window presentation.

### Environment descriptor

The shared environment descriptor supports:

- HDRI;
- gradient;
- bounded procedural atmosphere;
- deterministic sun direction;
- headless-safe time-of-day advancement;
- bounded procedural moon disc;
- bounded star presentation;
- shadowless bounded moon direct-light contribution for Rendered PBR materials;
- four shared starting presets.

Rendered sky and material environment sampling consume the same descriptor.

Switching away from HDRI transactionally discards derived IBL and activates the analytical environment path. Gradient and procedural shading therefore cannot retain stale HDR-derived resources.

Sandbox Environment settings persist and reload the bounded scalar/celestial descriptor through the transactional settings file. The stress path exercises an in-memory save/load round trip before rendering. Utility presets use the same scene setter.

Future environment work includes:

- dedicated moon shadow maps;
- arbitrary HDRI/cubemap identity persistence;
- procedural IBL convolution;
- editor environment-asset authoring;
- transactional environment hot reload;
- weather/cloud layers.

### Legacy wireframe compatibility

The legacy wireframe API remains supported:

- enable selects Wireframe;
- disable restores the last valid non-Wireframe mode.

The Sandbox persists the authoritative mode under `ui.scene_view.shading_mode`. Older `wireframe_enabled` settings migrate only when the new key is absent.

### Material-instance dependency view

Material instances layer validated scalar, vector, alpha-mode, and semantic-texture overrides over the shared glTF definition.

Effective dependency inspection reports borrowed texture slots after overrides, including transmission scalar and volume-thickness textures.

Revision-aware refresh preserves explicit instance choices across transactional definition reimport.

### Temporal allocation safety

History replacement validates the new texture before retiring the prior object.

Failed resize or texture allocation:

- preserves the previous GPU object;
- marks accumulation invalid for the requested viewport;
- reports fallback state.

### Current KTX2 progression summary

Visible KTX2 references carry a bounded priority and strongest mip target. Scene references use projected radius, distance fallback, and semantic-slot priority.

The active-frame API records bounded texture pins and releases them through an explicit end operation for scene replacement and other early exits.

The default remains synchronous. The optional Windows worker performs one bounded source read at a time. Validation and GPU replacement remain explicit processing work.

Automatic residency policy, background decode, and whole-resource eviction remain unfinished.

## Current limits

The runtime foundation is intentionally incomplete in several areas.

Current gaps include:

- complete production editor workflows;
- input rebinding UI;
- complete gamepad/device system;
- mature scene hierarchy;
- complete scene/project serialization;
- broader physics picking and advanced physics;
- Character Controller;
- shader graph;
- live shader authoring;
- production dialogue/game-specific interaction systems;
- cloud saves;
- remote logging;
- full release automation;
- production TAA and GTAO validation;
- complete texture streaming/residency policy;
- background texture decode;
- whole-resource eviction;
- production SSS;
- production glass/refraction;
- volumetric fog;
- production probe grids;
- broader material-file authoring;
- detachable Scene View;
- remaining Audio runtime/editor/device/streaming work.

These items are tracked in the [roadmap](roadmap.md) and subsystem documentation.
