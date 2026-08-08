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
movement hysteresis. Physics/render radius ownership and asynchronous
regeneration remain separate work.

## Deterministic edits

`<henka/terrain_edit.h>` is the single command path for raise, lower, flatten,
smooth, and paint operations. Commands carry an algorithm version, client
nonce, integer sample center, bounded sample radius, falloff, and operation
values. Linear and smooth falloffs use fixed-point integer weighting. The
runtime determines every affected resident region before allocating candidate
copies; all candidate regions pass validation before any live sample or
revision is swapped. The same ordered command stream therefore produces
byte-identical authoritative samples across runtimes. General editor/runtime
tool integration, client prediction, and asynchronous persistence scheduling
are not yet integrated; the server authority path persists accepted commands
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
encodes the response. It also echoes control pings and disconnects malformed
edit payloads as protocol errors. The server-side delta broadcast and
snapshot-fragment response are described below; client-side application,
reconnect/late-join recovery, prediction, and editor controls remain
subsequent integration work.

For edit requests, the session lazily materializes missing persisted regions
before authority validation, subject to the world's resident-region limit. It
does not preload the 8 km height field; eviction and asynchronous physics or
render regeneration remain separate work.

Accepted edits also produce a bounded delta in the same terrain channel. The
delta repeats world/base identity, client nonce, server command identity, the
algorithm-versioned command, and the resulting revision for each affected
region. The server broadcasts that event reliably after sending the requester
acceptance; client-side delta application, reconnect and late-join recovery,
and prediction/reconciliation remain subsequent work.

Snapshot requests identify the world, packaged base, region, and expected
revision. The server reads the validated region record from storage and emits
transfer-identified fragments with the record revision, generation, total
size, index, count, and payload bytes. The transport keeps each fragment under
the existing 32 KiB snapshot payload limit. The client assembly owner and
reconnect policy are not yet implemented.

`<henka/terrain_replica.h>` is the bounded client-side consumer for those
messages. It applies a delta only when every affected resident region advances
by exactly one revision, accepts an all-duplicate delta idempotently, and
rejects gaps or mixed duplicate/new multi-region states before changing live
samples. Snapshot fragments are accumulated under a configured byte budget;
the validated record is decoded and atomically swapped into the world only
after every fragment arrives. The replica does not own networking, reconnect
state, prediction history, or render/physics residency policy.

`<henka/terrain_collision.h>` extracts a physics-resident chunk into a
caller-owned 65×65 signed-millimeter patch without allocating or mutating the
world. The patch carries the source revision and generation so a later physics
owner can reject stale regeneration work. The current physics API exposes
planes, boxes, and spheres rather than a heightfield shape; body creation,
replacement, and asynchronous regeneration therefore remain subsequent work.

`<henka/terrain_mesh.h>` provides the corresponding renderer-independent
geometry boundary. `henka_terrain_mesh_build_chunk` requires a render-resident
chunk and fills caller-owned buffers for LOD 0 through LOD 3. It derives finite
central-difference normals, world-normalized UVs, and copies the four
normalized material weights without allocating. The result carries the source
revision and generation, so a graphics owner can discard a stale upload. GPU
mesh ownership, seam stitching, render eviction, and visual scene integration
remain subsequent work.

The descriptor stores the format version, world and base identities, all
world/region/chunk relationships, and bounded residency limits. Creating a
world allocates only the configured region and chunk residency tables; it does
not allocate an 8 km height field. Region state separately records CPU,
physics, render, pending-I/O, dirty, revision, and generation state.

## Current boundary

This slice establishes the shared data model and bounded ownership contract.
World manifest integration, journal compaction, collision regeneration, scene
ownership and GPU residency, replication and snapshot recovery, and client
prediction are subsequent validated runtime slices.
They must use this
same world identity, region/chunk mapping, revision, and residency ownership;
they must not introduce a second world-sized representation.
