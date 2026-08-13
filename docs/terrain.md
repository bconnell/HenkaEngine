# Terrain v1

The renderer-independent Terrain v1 core is owned by `henka_runtime` and is
available to both graphical clients and dedicated-server consumers through
`<henka/terrain.h>`.

## World contract

The default descriptor represents an 8192 m by 8192 m world as 16 by 16 regions.
Each region is 512 m across and contains 8 by 8 chunks. Each chunk is 64 m
across with 65 by 65 full-resolution samples at 1 m spacing. The authoritative
height field is signed 32-bit integer millimeters. Rendering and physics may
convert those values to meters at their consumption boundary.

Each sample reserves exactly four active material weights. The public
normalization helper produces a deterministic sum of 255 using integer
arithmetic and a stable largest-remainder tie break.

## Region persistence contract

`<henka/terrain_storage.h>` defines a versioned binary region record with
explicit little-endian fields for the world identity, packaged base identity,
region identity, generation, revision, sample dimensions, signed millimeter
heights, four material-weight bytes per sample, and a CRC-32 checksum. Native C
struct layout is never written to disk, and records are bounded by
`HENKA_TERRAIN_MAX_REGION_RECORD_BYTES`.

Runtime writes use an append-only journal containing BEGIN, bounded REGION, and
COMMIT records. Recovery applies only complete committed transactions. Each
region snapshot is validated and written to a temporary confined path before an
atomic replacement, so an interrupted write retains the previous valid
snapshot. Uncommitted records remain harmless journal history and are ignored
by subsequent recovery.

`henka_terrain_storage_compact` first recovers complete transactions, rejects
an active transaction, then atomically replaces the journal with an empty
durable file. Successful commits also compact automatically after the bounded
`HENKA_TERRAIN_STORAGE_AUTO_COMPACT_THRESHOLD_BYTES` threshold is reached.
Region snapshots remain the source of truth, so compaction does not require
loading the world-sized terrain or rewriting every region. This keeps repeated
tests, dedicated-server sessions, and runtime saves from accumulating committed
journal history without limit.

## Streaming boundary

The Windows runtime provides a bounded worker-backed stream queue through
`<henka/terrain_streaming.h>`. Workers borrow storage, load and validate one
immutable region candidate at a time, and never touch renderer or live world
objects. The runtime thread pumps bounded completions and performs the
authoritative sample/revision swap. Duplicate requests coalesce, queued or
active requests can be cancelled, observer records are bounded, and queue,
completion, failure, cancellation, and dropped-completion diagnostics are
available, including current and high-water counts for queued requests,
active work, completions, and observers. Observer updates request a bounded CPU-radius square and reconcile
resident regions against the union of observer unload-radius squares. Regions
outside that union are released deterministically in row-major order only when
they have no physics/render residency, pending I/O, or dirty edits. A zero
unload radius preserves the CPU radius; a larger unload radius provides bounded
movement hysteresis. Loaded regions now synchronize physics/render residency
flags from the observer radius union. A stream descriptor may also provide a
bounded region generator for missing (but not corrupt) snapshots; it runs on
the worker, receives only the immutable world/layout description and one
caller-owned sample buffer, validates every generated sample's normalized
255-weight invariant, and publishes revision/generation one only after the
main-thread snapshot swap. Persisted regions always win over the generator,
and callback or validation failures remain failed requests. Renderer mesh and
physics patch regeneration remain caller-owned asynchronous presentation work.
Publishing a validated region snapshot also clears that region's pending-I/O
flag and releases its corresponding world pending-I/O budget slot exactly once.
The graphical
render owner reports high-water pending-request, resident-chunk, and
visible-chunk counts; the collision queue reports its high-water pending chunk
count so callers can distinguish a bounded budget from a transient drain state.
The renderer-owner regression also drives two render-resident regions through
the public observer seam, verifies nearest bounded working-set replacement after
camera relocation, and confirms that all presentation slots are removed beyond
the outer LOD distance band while resident/visible high-water diagnostics remain
valid. This is bounded runtime coverage, not production-scale streaming or
human visual approval.
Observer-driven requests are marked separately from explicit
`henka_terrain_streamer_request_region` calls: shrinking or removing an
observer cancels only stale observer demands, so an explicit caller request
remains queued or active until it completes or is explicitly canceled.

Before publishing a successful worker completion, the pump compares its
region generation/revision with the currently resident authoritative state.
An older completion is discarded instead of overwriting newer edits,
recovery, or reload state, and is counted in `stale_completion_count` for
diagnostics. This preserves the transactional ownership boundary without
making a stale asynchronous load look like an I/O failure.

Queued completions retain whether their request came only from an observer.
If that observer moves before the completion is pumped, the result is
discarded and counted in `cancelled_completion_count`; explicit requests that
were coalesced onto the same load clear that observer-only ownership and are
still allowed to complete. An explicit request also coalesces onto a
successful same-region completion already waiting in the bounded pump queue,
preventing duplicate worker loads.

## Deterministic edits

`<henka/terrain_edit.h>` is the single command path for raise, lower, flatten,
smooth, and paint operations. Commands carry an algorithm version, client
nonce, integer sample center, bounded sample radius, falloff, and operation
values. Linear and smooth falloffs use fixed-point integer weighting. The
runtime determines every affected resident region before allocating candidate
copies; all candidate regions pass validation before any live sample or
revision is swapped. The same ordered command stream therefore produces
byte-identical authoritative samples across runtimes. The Sandbox editor and
runtime callers use this same command path; saved brush state and asynchronous
persistence scheduling are not yet integrated. The server authority path
persists accepted commands synchronously through the storage transaction
described below.

Terrain network payloads in `<henka/terrain_network.h>` use explicit bounded
little-endian encoding for edit requests, authoritative acceptance revisions,
rejection reasons, and deterministic edit deltas. Requests carry world/base
identity, client nonce,
algorithm-versioned command fields, and the expected revision for each affected
region. Payload codecs reject unsupported command fields, negative region IDs,
oversized region lists, and trailing/truncated bytes; transport framing and
authority policy remain separate layers.

## Authority contract

`<henka/terrain_authority.h>` provides the renderer-independent server-side
validation boundary. It bounds per-peer edit requests, optionally invokes a
permission callback, verifies the world and packaged-base identities, requires
the exact deterministic affected-region set, and rejects stale region
revisions. An accepted command is applied to the live world, written to every
affected region in one storage transaction, and acknowledged only after the
transaction commits. If the edit or persistence path fails, the live samples
and revisions are restored and the incomplete transaction is abandoned.

The authority object does not own the world or storage. The
`<henka/terrain_server.h>` session adapter owns neither: it borrows the
public ENet server, decodes edit messages, routes them through authority, and
encodes the response. It also echoes control pings, sends a bounded control
session-info message on connect, and disconnects malformed edit payloads as
protocol errors. Session info carries the world/base identities plus up to 16
current resident regions with revision and generation, sorted in deterministic
row-major coordinate order before encoding so residency-slot reuse cannot
change a capped manifest's order. The client validates
those identities and requests snapshots for the advertised regions, so an
empty client can enter through the same replica snapshot path used for
recovery. This is a bounded late-join bootstrap, not a full application
handshake, authentication layer, or relevance-based region selection; those
remain subsequent integration work. The public client adapter can request a
new transport connection; the connect-time session-info comparison then
refreshes only missing or stale advertised regions. The client recovery
coverage also forces a server-directed disconnect and validates reconnect
after replacing the authoritative server wrapper on the same endpoint, with
exact resident sample convergence. This is bounded reconnect recovery, not
application authentication or relevance-driven world streaming.

For edit requests, the session lazily materializes missing persisted regions
before authority validation, subject to the world's resident-region limit. It
does not preload the 8 km height field; eviction and asynchronous physics or
render regeneration remain separate work. `henka_terrain_server_diagnostics`
reports `materialization_failure_count` when a requested region cannot be
allocated, loaded, or published into the live world; the edit remains rejected
and no partial authority operation is accepted. Materialization is
all-or-nothing for the request: regions loaded by a request are released again
when a later requested region fails.

The dedicated server recovers complete journal transactions before opening its
network endpoint and loads an existing reserved region snapshot into the live
world. When `--world` is supplied, the path is opened as a validated read-only
Terrain storage root and requires `region_0_0.htr`; the runtime save root is
loaded afterward so committed runtime edits override the base region. Its bounded smoke mode also exercises a loopback client and commits a
deterministic edit when the save root is empty, allowing the packaged restart
check to verify that the same revision is restored. This does not replace a
multi-process multiplayer soak or relevance-driven reconnect/late-join policy.
The bounded Windows process integration soak repeats that complete scenario
for a finite session count with isolated save roots; production-scale capacity
is not claimed.

Accepted edits also produce a bounded delta in the same terrain channel. The
delta repeats world/base identity, client nonce, server command identity, the
algorithm-versioned command, and the resulting revision for each affected
region. The server broadcasts that event reliably after sending the requester
acceptance and retains the last 64 accepted deltas in a fixed ring. The client
session adapter applies deltas only across exact revision steps; a gap sends a
bounded recovery request for the missing regional revision range. At most one
pending recovery request is retained per affected region; repeated copies of
the same or an older gap are suppressed and counted, while a newer target
revision replaces the pending target. A successful recovered delta or snapshot
clears that region's pending entry, and an explicit reconnect clears all
entries before the new connection's session bootstrap. Complete retained
history is sent as deltas, while an exhausted or incomplete range uses the
existing transactional regional snapshot path. The bounded pending count and
suppression count are exposed by the public client diagnostics.
The connect-time session-info bootstrap covers only the bounded advertised
resident set. Clients that know their interest center can opt into a second,
bounded relevance request through `<henka/terrain_client.h>`; the server
filters resident regions by Chebyshev radius, orders them by squared distance
with coordinate-stable ties, and caps the response at 16 regions. The
filtered response is transactional at the client session boundary: the
client ignores the initial legacy summary while the opt-in request is pending,
then requests snapshots only for the selected response. Per-region session
snapshot requests are coalesced while a target revision/generation is pending;
a newer advertisement replaces that bounded target, while equal or older
advertisements are suppressed with diagnostics. Request radius and response
count are validated before any world state is touched; malformed or
identity-mismatched requests disconnect the peer. This is bounded selection,
not application authentication or render/physics residency orchestration.
The recovery test covers one bounded resident set through forced disconnect,
reconnect, and server-wrapper restart, while a server regression covers
coordinate-stable relevance selection. Production-scale multiplayer soak
remains subsequent work. A separate
public client-session regression connects two replicas to the same authoritative
server, bootstraps the advertised region, sends one edit from each peer, and
compares the complete resident sample arrays against the server after both
revisions. This proves bounded two-client convergence; the finite process soak
repeats the multi-process scenario, but neither is application-level
authentication or production-scale capacity coverage.
`<henka/terrain_prediction.h>` owns a separate bounded presentation
world for local commands: it copies CPU-resident authoritative regions, applies
pending commands in submission order, and rebuilds from authoritative state
when a command is accepted or rejected. The authoritative replica is never
used as prediction scratch state, and exceeding the pending-command bound fails
closed.

Snapshot requests identify the world, packaged base, region, and expected
revision. The server reads the validated region record from storage and emits
transfer-identified fragments with the record revision, generation, total
size, index, count, and payload bytes. The transport keeps each fragment under
the existing 32 KiB snapshot payload limit, and the replica requires the exact
ceil(total-size / payload-limit) fragment count plus full-size non-final
fragments so malformed transfers fail closed instead of hanging assembly. The
client session adapter owns the
bounded fragment assembly through `<henka/terrain_replica.h>`; a delta gap
first requests the retained revision range and uses a snapshot when that
range is unavailable. Connect-time session info can request the same bounded
snapshot path for up to 16 advertised resident regions. An opt-in
session-interest request narrows that list before snapshot requests using a
validated center, radius, and maximum count; the response is marked so old
unfiltered connect summaries are not applied twice. The Windows process
harness exercises the bounded late-observer path, explicit client reconnect,
and restart checksum convergence; the finite process soak repeats this bounded
policy. The selection remains a runtime transport foundation, not application
authentication, relevance-aware render/physics residency, or production-scale
multiplayer capacity.

`<henka/terrain_replica.h>` is the bounded client-side state owner consumed by
`<henka/terrain_client.h>`. It applies a delta only when every affected region advances
by exactly one revision, accepts an all-duplicate delta idempotently, and
rejects gaps or mixed duplicate/new multi-region states before changing live
samples. Snapshot fragments are accumulated under a configured byte budget;
duplicate fragments, malformed sizing, world/base identity mismatches, and
checksum/decode failures are rejected without publishing partial samples; a
valid transfer may arrive out of order and commits only after every expected
fragment arrives. A new transfer can retry and the validated record is decoded
and atomically swapped into the world only after every fragment arrives. The
replica rejects a decoded snapshot whose generation/revision is older than the
currently resident region, counts it in `stale_snapshot_count`, and leaves the
newer state untouched. The client diagnostics expose the same count so
external consumers can distinguish superseded recovery packets from malformed
or failed transfers. The
replica does not own network transport,
reconnect state or render/physics residency policy; the
client adapter does not invent those missing policies. A revision-gap result
is the only delta failure eligible for bounded recovery; identity, protocol,
validation, and allocation failures remain hard errors and do not trigger a
request derived from the rejected message.

The replica's final decode/sample allocation is also transactional: an
allocation failure after all fragments arrive leaves the previous resident
region state untouched, clears the failed transfer, and permits a later
transfer to retry cleanly.

When a decoded fragment is rejected, the public Terrain client requests a new
snapshot for the same region and target revision, once per transfer and up to
four times per connection. Undecodable messages remain hard errors because
their identity cannot be trusted for a retry; the bounded retry count is
reported through `recovery_snapshot_request_count`. A disconnect retires all
delta-recovery and session-snapshot suppression entries immediately, so a
subsequent reconnect cannot inherit requests from the old transport.

`<henka/terrain_collision.h>` extracts a physics-resident chunk into a
caller-owned 65×65 signed-millimeter patch without allocating or mutating the
world. The patch carries the source revision and generation so a later physics
owner can reject stale regeneration work. `<henka/terrain_physics.h>` provides
that bounded owner: it copies each patch transactionally, retains at most the
configured patch count, chooses overlapping patches in stable slot order, and
answers bilinear height and finite normal queries with source identity. The
`henka_terrain_physics_raycast` API adds a bounded allocation-free traversal
over those resident patches, with normalized ray distance, hit position, and
source identity; a miss is reported without inventing terrain outside the
resident physics set. The
rigid-body API additionally exposes a static heightfield collider whose signed
millimeter source is copied into the body. Sphere and axis-aligned box
contacts, terrain normals, layer/mask filtering, bounded heightfield raycasts,
and transactional replacement are implemented. The renderer-free
`<henka/terrain_collision_runtime.h>` owner provides a fixed-capacity,
coalescing rebuild queue: the runtime thread builds full-resolution patches for
physics-resident chunks and replaces the durable physics representation only
after the candidate succeeds. A failed rebuild leaves the last valid patch in
place. `henka_terrain_collision_runtime_sync_residency` tracks bounded patch
identity, queues missing or stale physics-resident chunks in stable order, and
removes patches when their regions leave physics residency; callers may set a
deterministic camera/interaction focus region with
`henka_terrain_collision_runtime_set_focus`, which admits that region's
representative patch first and evicts one non-focus patch when the bounded
physics capacity is full. The physics owner's patch capacity is the hard
admission bound. `henka_terrain_collision_runtime_request_edit`
derives height-edit coverage plus one chunk of physics-neighbor coverage;
paint-only edits validate and return without queueing collision work because
they change material weights rather than the heightfield. The Terrain server
session can borrow that queue to schedule height edits after authority
acceptance. The Sandbox graphical path owns this runtime beside the physics
world: camera residency sync, local sculpt edits, and region reloads use the
queue and bounded pump, while paint refreshes render material weights only.
Height edits crossing chunk boundaries do not silently refresh only the
fixture's first chunk. Callers still own the pump cadence.

`<henka/terrain_mesh.h>` provides the corresponding renderer-independent
geometry boundary. `henka_terrain_mesh_build_chunk` requires a render-resident
chunk and fills caller-owned buffers for LOD 0 through LOD 3. It derives finite
central-difference normals, an orthogonal tangent vec4 basis with deterministic
handedness, stable world-space UV transport, and copies the four normalized
material weights without allocating. The result carries the source revision
and generation, so a graphics owner can discard a stale upload. GPU
mesh ownership is provided by `<henka/terrain_render.h>` in the graphical
client: it borrows the engine, scene, and world, keeps fixed-capacity chunk
slots and request queues, creates scene entities with bounds for renderer
culling and ordinary scene selection, and replaces meshes transactionally only
after a candidate upload succeeds. Four LOD bands use hysteresis and
deterministic adjacent-chunk
selection; render visibility is bounded by the configured outer band. The
owner destroys its entities and meshes without destroying the borrowed world.
The built-in Terrain material uses exactly four normalized painted weights as
world-space PBR layer blends. Each layer has validated base-color, normal, and
metallic/roughness texture semantics plus base color, metallic, roughness,
normal-strength, and meters-per-tile factors. The normal Rendered shader
consumes these weights; ordinary material vertex-color tint remains disabled
for this material. The Sandbox reference fixture creates four deterministic
32x32 asset-manager-owned runtime grass, dirt, rock, and wet base-color with
tangent-normal, and metallic/roughness tiles using bounded multi-frequency
variation. The Rendered Terrain shader also adds bounded world-space macro
variation to albedo and roughness to reduce large-scale tile repetition; this
does not claim authored texture streaming.
The material retains deterministic factor fallback for replacement or unavailable
optional sources. The graphical
owner reports the exact unique layer-texture count and resident material bytes;
the Sandbox reference fixture therefore expects all twelve semantic layer slots
to contribute without duplicating shared handles. The existing asset-manager dependency inspection contract
also enumerates each of the twelve optional layer texture slots with its
semantic usage for both material definitions and effective instances; the
manager remains the owner of those borrowed texture handles. Terrain render
descriptors may optionally carry a stable identity returned by
`henka_assets_adopt_runtime_material`. When present, newly resident chunk
entities bind that manager-owned definition identity together with the
validated effective material value. Adoption rejects shader or texture
dependencies that are not owned by the same manager, duplicate identities, and
invalid material values; the manager owns the definition and its identity, but
does not invent a source-file reload path for generated runtime definitions.
The descriptor still borrows the manager-owned definition and its dependencies,
so they must outlive the graphical Terrain runtime.
Uploaded GPU meshes use a four-edge transition mask when an adjacent resident
chunk is exactly one LOD coarser. The mesh keeps shared even edge samples and
redirects odd fine-edge indices to those shared coarse endpoints, omitting
triangles that collapse under the mapping. Intervening fine edge samples are
also morphed onto the linear boundary between the shared coarse endpoints.
Transition vertices re-normalize their interpolated normal/tangent basis and
restore the 255-sum material-layer invariant before upload. This is bounded
per-edge stitched topology plus geometry morphing for one-level differences; a
missing or invalid neighbor uses a bounded downward skirt only on that edge
until the neighbor is resident; the owner records both masks in chunk
diagnostics. On each observer update the owner now derives a deterministic
nearest working set from render-resident regions, removes slots whose regions
leave render residency or the outer LOD band, and queues only bounded missing
chunks. It also compares uploaded revision/generation identity with the
borrowed world and queues stale resident chunks for transactional replacement;
it does not mutate the borrowed world or allocate per-frame working arrays.
Neighbor-aware border-normal sampling uses available authoritative regions and
falls back to the resident edge until a neighbor streams in. Uploaded chunks
track the bounded 3x3 region revision/generation identities used by that border
sampling, so a neighbor edit queues dependent chunks as well; height-derived
scene bounds are replaced with the candidate mesh and restored on failure.
Observer synchronization runs this same stale-dependency check before working-
set admission and propagates the first bounded queue/admission error instead of
silently dropping it; already queued replacements remain available for a later
pump/retry. Callers that already accepted a deterministic edit may use
`henka_terrain_render_runtime_request_edit` to requeue resident chunks covered
by the edit plus a one-chunk dependency border. The call validates the command,
never admits nonresident chunks, coalesces into the fixed render queue, and
leaves observer working-set admission and pump cadence with the caller. Region
snapshot, remote-delta, and transactional reload owners can use
`henka_terrain_render_runtime_refresh_dirty` before pumping to route the same
revision/dependency check without forcing an unconditional chunk rebuild;
successful stale-slot requeues are included in bounded render diagnostics.
Paint-only edits use a separate four-byte-per-vertex Terrain weight stream.
The graphical owner builds a bounded candidate weight payload and swaps its GPU
buffer only after the upload succeeds, preserving the existing mesh VAO,
indices, geometry, bounds, and scene entity identity. `weight_updates` and
`failed_weight_updates` distinguish this path from geometry rebuilds. This is
not a general-purpose mesh attribute update API, and height edits or topology,
LOD, border-normal, and dependency changes still use transactional mesh
replacement.
The Sandbox resource line reports that counter alongside owner memory totals.
`henka_terrain_render_stats.gpu_weight_bytes` reports the exact resident bytes
owned by those weight buffers; it is kept separate from the interleaved mesh
vertex and index totals so diagnostics do not hide the additional upload.
The render regression suite verifies resident Terrain entities retain the
shared material's cast/receive shadow flags, are visible to
the normal scene ray picker and that observer-driven removal leaves no stale
Terrain entities after graphical-owner teardown. The mesh regression suite
also builds an all-four-edge transition, verifies
stitched output emits fewer indices than the regular grid, and rejects
degenerate triangles or non-finite tangent bases at the corners. Manual visual
corner validation remains subsequent work; the suite also compares all four
fine/coarse boundary positions, including shared corners, and verifies that
odd fine-edge indices redirect to shared coarse endpoints without degenerate
triangles. The Sandbox also routes one shared
raise command through authoritative integer mutation, refreshes the
transactional physics patch, and refreshes the affected GPU mesh; this is a
runtime smoke path, not persistence or network authority. It also applies a
shared paint command, verifies the authoritative layer weight and rendered
revision advance, and leaves collision untouched for that paint-only mutation.
Sandbox smoke also checks the per-frame Terrain color and shadow submission
diagnostics while the viewport is Rendered; generic depth, AO, reflection,
temporal, fog, and HDR processing consume the same scene submission path. The
engine diagnostics bitmask identifies Terrain participation in those consumers
for the current frame, including probe capture only when a Terrain entity is
actually submitted to that capture. It is a path diagnostic, not a claim of
production visual validation for every effect.
The Terrain utility reports exact world-owned CPU bytes and graphical-owner
vertex/index/weight/material GPU bytes for resident resources; these values exclude
borrowed renderer resources outside the Terrain owner.
For application-only visual evidence, run
`scripts/capture_visual_evidence_windows.ps1 -Configuration Release -IncludeTerrain`.
The optional terrain capture uses deterministic wide, close-material, and
four-region-corner cameras for Solid, Material Preview, and Rendered. Capture
mode refreshes the four bounded fixture regions from their seeded samples so
persisted editor edits cannot change the comparison; material and presentation
comparisons do not change scene materials or scene lights. The helper can target
the development or packaged executable. The wide set runs
`scripts/check_terrain_visual_evidence_windows.ps1`, the corner set runs
`scripts/check_terrain_corner_visual_evidence_windows.ps1`, and the close set
runs `scripts/check_terrain_close_visual_evidence_windows.ps1`. All three sample
only the normalized Scene View interior and reject missing or dimension-
mismatched images, flat content in any mode, and a Rendered image that is not
measurably distinct from Material Preview. This is an automated
presentation-path guard, not a baseline-image or human visual approval or
complete topology QA.
The same graphical
smoke revokes render residency after the upload, forces a candidate mesh
failure, verifies the previous mesh and revision remain resident, then restores
residency and proves replacement recovery; this is bounded failure-injection
coverage. The renderer test suite additionally forces candidate allocation
failure during a dirty replacement and verifies the prior mesh, revision, and
scene bounds remain live before a retry succeeds; this is not complete stress
or visual QA.

The Sandbox reference scene seeds four deterministic regions in a persistent
`terrain-sandbox-v2` storage root, marks the initial region render-resident, and
lets this owner discover its bounded chunk working set from the active camera.
The fixture contains rolling ground, a valley, a steep ridge/cliff, and
continuous grass/dirt/rock/wet four-layer weights; existing committed samples
are retained. The same camera feeds the public streaming observer under a
four-region CPU budget with a one-region CPU, physics, render, and unload
window. Stream requests are admitted in deterministic priority
order: render-radius regions first, then physics-radius regions, then the
remaining CPU-radius regions by Chebyshev distance, with stable request
sequence tie breaking. Updating or removing an observer re-scores queued
requests while holding the stream lock and cancels observer-demand work that
leaves every observer's CPU radius; an active stale observer load is canceled
at its next worker boundary as well, so a moving camera cannot spend bounded
capacity on obsolete requests. The opt-in Windows `--terrain-stream-stress` path
proves the initial one-region camera window at `(0,0)`, then crosses
`(2,0) -> (2,2) -> (0,0)`, waits only through bounded
worker/render queues, checks rendered return at both axes and a bounded
collision-patch overlap, and reports the resident-region bound. Normal movement remains
observer-driven and pumps at most two render replacements per frame. The
Sandbox also supplies the same deterministic generator to the stream worker,
so a camera can move into a valid unpersisted region without manual region
priming; edits become persistent only through the normal transactional storage
path. This proves bounded procedural broad-world regeneration and a small
persistent camera crossing fixture, not asynchronous background physics/render
regeneration or human visual approval.
The Utility Terrain tab now exposes bounded resident/render/collision statistics
and raise, lower, flatten, smooth, and paint controls with radius, strength,
layer, and falloff settings. It uses the same deterministic command API as
runtime callers; a resident physics hit now supplies the integer sample center
for the next command. A bounded horizontal brush preview follows a successful
resident physics hit while the Terrain utility is active. Brush radius,
strength, active layer, falloff, and operation are persisted through the normal
Sandbox settings file with range validation on load. Its read-only Material
layers section reports Grass, Dirt, Rock, and Wet base-color, normal, and
metallic/roughness texture dimensions, GPU formats, and resident/total mip
counts directly from the manager-owned live textures. This is dependency
inspection and edit support, not a separate material authority or a claim of
complete viewport material-preview authoring.

The Terrain utility also projects the scene-view cursor through the shared
camera ray API and queries the resident physics patch owner. A successful
resident hit is shown with chunk/source identity and becomes the integer
sample center for the next raise, lower, flatten, smooth, or paint command;
when a scene-bound object is also under the cursor, the nearest finite hit wins
so a Terrain chunk does not hide an object in front of it. Nonresident terrain
is not invented and a miss leaves the previous command center unchanged. This
is an editor/runtime command bridge, not network authority. The same utility
opens a user-data-local
`terrain-sandbox-v2` storage root, recovers or loads region `(0,0)` at startup,
and exposes transactional Save for every currently CPU-resident region plus
committed-journal Compact actions. The storage API also provides a dirty-only
transaction that skips clean resident regions, which is also the path used by
the Sandbox normal-editor ten-second dirty-only autosave. Dirty flags clear
only after the bounded multi-region transaction commits, and failed saves leave
the live world unchanged because storage owns the transaction; a failed
autosave leaves the regions pending for a later interval. Capture and smoke
modes do not schedule persistence. Nonresident world authoring and general
background persistence scheduling are not claimed.
Reload uses a bounded temporary decode, then rebuilds the physics patch and
render mesh; a presentation failure restores the previous samples, revision,
generation, and collision patch.

`henka_terrain_world_get_stats` reports `dirty_region_count` alongside resident
and pending-I/O counts. The Sandbox Terrain utility displays that count before
the Render and Collision rows, so an editor user can see unsaved active regions
without inferring it from the storage journal. This is a diagnostic only; it
does not schedule persistence or change the transactional Save contract.

The descriptor stores the format version, world and base identities, all
world/region/chunk relationships, and bounded residency limits. Creating a
world allocates only the configured region and chunk residency tables; it does
not allocate an 8 km height field. Region state separately records CPU,
physics, render, pending-I/O, dirty, revision, and generation state.

## Current boundary

The current validated boundary includes bounded observer-driven render working
set discovery, stale render identity refresh, one-level cross-LOD stitched edge
topology and morphing, automated four-edge transition-mesh checks, and physics
patch synchronization
with deterministic capacity-based admission and removal. Full residency-wide
dirty-neighbor scheduling beyond the bounded physics patch capacity, four-way
corner visual QA, and production-scale multiplayer soak remain subsequent
validated runtime slices; the bounded
process integration soak now repeats the advertised resident-region scenario
for a finite session count.
Accepted edits can now derive
one-chunk physics-neighbor coverage through the bounded collision queue, and
client prediction is available through the separate presentation-world owner.
They must use this
same world identity, region/chunk mapping, revision, and residency ownership;
they must not introduce a second world-sized representation.

The collision-runtime regression target also fills a deliberately undersized
rebuild queue, verifies a rejected request is counted and can be admitted after
capacity is reused, and verifies that a failed rebuild retains the last valid
physics patch until a later retry succeeds. This is bounded queue-pressure and
transactional-failure coverage; it does not claim broad runtime collision
stress across an unbounded world or physics capacity beyond the configured
patch limit.

The headless `henka_terrain_workflow_tests` target exercises the runtime path
without renderer dependencies: deterministic raise and paint commands change
the authoritative region, a full-resolution collision patch follows the edit,
the committed region reloads into a new world and physics owner, and an
uncommitted journal transaction is discarded during recovery.
Terrain server authority rate records are bounded by the configured simultaneous
client capacity and are retired on transport disconnect before a later peer can
reuse the slot. Request-time loading of persisted regions records the regions it
created and releases only those regions when materialization or later authority
validation rejects the edit; preexisting residency is preserved.

Accepted deltas remain globally delivered by the bounded transport, but a client
that does not currently own an affected region treats a validated out-of-interest
delta as a bounded no-op. It does not invent a region or request revision-gap
recovery; a later relevance/session response acquires the authoritative snapshot.
Valid snapshot requests also receive an explicit terminal failure message when
bounded storage, allocation, or encoding work fails before the first fragment.
Malformed or identity-invalid requests remain protocol failures, not retryable
successes.
