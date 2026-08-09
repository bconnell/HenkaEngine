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
durable file. Region snapshots remain the source of truth, so compaction does
not require loading the world-sized terrain or rewriting every region.

## Streaming boundary

The Windows runtime provides a bounded worker-backed stream queue through
`<henka/terrain_streaming.h>`. Workers borrow storage, load and validate one
immutable region candidate at a time, and never touch renderer or live world
objects. The runtime thread pumps bounded completions and performs the
authoritative sample/revision swap. Duplicate requests coalesce, queued or
active requests can be cancelled, observer records are bounded, and queue,
completion, failure, cancellation, and dropped-completion diagnostics are
available. Observer updates request a bounded CPU-radius square and reconcile
resident regions against the union of observer unload-radius squares. Regions
outside that union are released deterministically in row-major order only when
they have no physics/render residency, pending I/O, or dirty edits. A zero
unload radius preserves the CPU radius; a larger unload radius provides bounded
movement hysteresis. Loaded regions now synchronize physics/render residency
flags from the observer radius union; renderer mesh and physics patch
regeneration remain caller-owned asynchronous presentation work.

## Deterministic edits

`<henka/terrain_edit.h>` is the single command path for raise, lower, flatten,
smooth, and paint operations. Commands carry an algorithm version, client
nonce, integer sample center, bounded sample radius, falloff, and operation
values. Linear and smooth falloffs use fixed-point integer weighting. The
runtime determines every affected resident region before allocating candidate
copies; all candidate regions pass validation before any live sample or
revision is swapped. The same ordered command stream therefore produces
byte-identical authoritative samples across runtimes. General editor/runtime
tool integration and asynchronous persistence scheduling are not yet
integrated; the server authority path persists accepted commands
synchronously through the storage transaction described below.

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
current resident regions with revision and generation. The client validates
those identities and requests snapshots for the advertised regions, so an
empty client can enter through the same replica snapshot path used for
recovery. This is a bounded late-join bootstrap, not a full application
handshake, authentication layer, or relevance-based region selection; those
and editor controls remain subsequent integration work. The public client
adapter can request a new transport connection; the connect-time session-info
comparison then refreshes only missing or stale advertised regions. This is
bounded reconnect recovery, not application authentication or
relevance-driven world streaming.

For edit requests, the session lazily materializes missing persisted regions
before authority validation, subject to the world's resident-region limit. It
does not preload the 8 km height field; eviction and asynchronous physics or
render regeneration remain separate work.

The dedicated server recovers complete journal transactions before opening its
network endpoint and loads an existing reserved region snapshot into the live
world. When `--world` is supplied, the path is opened as a validated read-only
Terrain storage root and requires `region_0_0.htr`; the runtime save root is
loaded afterward so committed runtime edits override the base region. Its bounded smoke mode also exercises a loopback client and commits a
deterministic edit when the save root is empty, allowing the packaged restart
check to verify that the same revision is restored. This does not replace a
multi-process multiplayer soak or relevance-driven reconnect/late-join policy.

Accepted edits also produce a bounded delta in the same terrain channel. The
delta repeats world/base identity, client nonce, server command identity, the
algorithm-versioned command, and the resulting revision for each affected
region. The server broadcasts that event reliably after sending the requester
acceptance and retains the last 64 accepted deltas in a fixed ring. The client
session adapter applies deltas only across exact revision steps; a gap sends a
bounded recovery request for the missing regional revision range. Complete
retained history is sent as deltas, while an exhausted or incomplete range
uses the existing transactional regional snapshot path.
The connect-time session-info bootstrap covers only the bounded advertised
resident set; broader reconnect and late-join orchestration and editor
controls remain subsequent work. `<henka/terrain_prediction.h>` owns a separate bounded presentation
world for local commands: it copies CPU-resident authoritative regions, applies
pending commands in submission order, and rebuilds from authoritative state
when a command is accepted or rejected. The authoritative replica is never
used as prediction scratch state, and exceeding the pending-command bound fails
closed.

Snapshot requests identify the world, packaged base, region, and expected
revision. The server reads the validated region record from storage and emits
transfer-identified fragments with the record revision, generation, total
size, index, count, and payload bytes. The transport keeps each fragment under
the existing 32 KiB snapshot payload limit. The client session adapter owns the
bounded fragment assembly through `<henka/terrain_replica.h>` and requests a
snapshot when a delta cannot be applied. Connect-time session info can request
the same bounded snapshot path for up to 16 advertised resident regions;
relevance-driven reconnect and late-join policy are not yet implemented.

`<henka/terrain_replica.h>` is the bounded client-side state owner consumed by
`<henka/terrain_client.h>`. It applies a delta only when every affected region advances
by exactly one revision, accepts an all-duplicate delta idempotently, and
rejects gaps or mixed duplicate/new multi-region states before changing live
samples. Snapshot fragments are accumulated under a configured byte budget;
the validated record is decoded and atomically swapped into the world only
after every fragment arrives. The replica does not own network transport,
reconnect state or render/physics residency policy; the
client adapter does not invent those missing policies.

`<henka/terrain_collision.h>` extracts a physics-resident chunk into a
caller-owned 65×65 signed-millimeter patch without allocating or mutating the
world. The patch carries the source revision and generation so a later physics
owner can reject stale regeneration work. `<henka/terrain_physics.h>` provides
that bounded owner: it copies each patch transactionally, retains at most the
configured patch count, chooses overlapping patches in stable slot order, and
answers bilinear height and finite normal queries with source identity. The
rigid-body API additionally exposes a static heightfield collider whose signed
millimeter source is copied into the body. Sphere and axis-aligned box
contacts, terrain normals, layer/mask filtering, bounded heightfield raycasts,
and transactional replacement are implemented. The renderer-free
`<henka/terrain_collision_runtime.h>` owner provides a fixed-capacity,
coalescing rebuild queue: the runtime thread builds full-resolution patches for
physics-resident chunks and replaces the durable physics representation only
after the candidate succeeds. A failed rebuild leaves the last valid patch in
place. `henka_terrain_collision_runtime_request_edit` derives the accepted
edit footprint plus one chunk of physics-neighbor coverage, and the Terrain
server session can borrow that queue to schedule it after authority acceptance.
Callers still own the pump cadence and may enqueue individual chunks when
residency changes.

`<henka/terrain_mesh.h>` provides the corresponding renderer-independent
geometry boundary. `henka_terrain_mesh_build_chunk` requires a render-resident
chunk and fills caller-owned buffers for LOD 0 through LOD 3. It derives finite
central-difference normals, world-normalized UVs, and copies the four
normalized material weights without allocating. The result carries the source
revision and generation, so a graphics owner can discard a stale upload. GPU
mesh ownership is provided by `<henka/terrain_render.h>` in the graphical
client: it borrows the engine, scene, and world, keeps fixed-capacity chunk
slots and request queues, creates scene entities with bounds for renderer
culling, and replaces meshes transactionally only after a candidate upload
succeeds. Four LOD bands use hysteresis and deterministic adjacent-chunk
selection; render visibility is bounded by the configured outer band. The
owner destroys its entities and meshes without destroying the borrowed world.
Uploaded GPU meshes add bounded downward edge skirts to cover finite LOD
transitions; full neighbor-aware stitching, automatic world-residency
scheduling, and manual visual validation remain subsequent work. The Sandbox also routes one shared
raise command through authoritative integer mutation, refreshes the
transactional physics patch, and refreshes the affected GPU mesh; this is a
runtime smoke path, not persistence or network authority.

The Sandbox reference scene now creates one deterministic bounded region,
marks it render-resident, and attaches four chunks through this owner. It
updates selection from the active camera and pumps at most two replacements
per frame. This is runtime integration coverage, not a claim that editor
sculpt/paint tools or visual human approval are complete. The Utility Terrain
tab now exposes bounded resident/render/collision statistics and raise, lower,
flatten, smooth, and paint controls with radius, strength, layer, and falloff
settings. It uses the same deterministic command API as runtime callers; the
reference controls use a fixed `(32,32)` sample center, so viewport ray-pick,
saved brush state, and complete paint/material preview remain unfinished.

The descriptor stores the format version, world and base identities, all
world/region/chunk relationships, and bounded residency limits. Creating a
world allocates only the configured region and chunk residency tables; it does
not allocate an 8 km height field. Region state separately records CPU,
physics, render, pending-I/O, dirty, revision, and generation state.

## Current boundary

This slice establishes the shared data model and bounded ownership contract.
Full residency-wide dirty-neighbor scheduling, automatic render mesh
residency, cross-LOD seam stitching, and reconnect/late-join orchestration are
subsequent validated runtime slices. Accepted edits can now derive one-chunk
physics-neighbor coverage through the bounded collision queue, and client
prediction is available through the separate presentation-world owner.
They must use this
same world identity, region/chunk mapping, revision, and residency ownership;
they must not introduce a second world-sized representation.
