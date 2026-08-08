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
available. Hysteresis, deterministic eviction, and automatic residency policy
remain subsequent work.

## Deterministic edits

`<henka/terrain_edit.h>` is the single command path for raise, lower, flatten,
smooth, and paint operations. Commands carry an algorithm version, client
nonce, integer sample center, bounded sample radius, falloff, and operation
values. Linear and smooth falloffs use fixed-point integer weighting. The
runtime determines every affected resident region before allocating candidate
copies; all candidate regions pass validation before any live sample or
revision is swapped. The same ordered command stream therefore produces
byte-identical authoritative samples across runtimes. Persistence enqueue,
server permissions/revision checks, client prediction, and editor controls are
not yet integrated.

Terrain network payloads in `<henka/terrain_network.h>` use explicit bounded
little-endian encoding for edit requests, authoritative acceptance revisions,
and rejection reasons. Requests carry world/base identity, client nonce,
algorithm-versioned command fields, and the expected revision for each affected
region. Payload codecs reject unsupported command fields, negative region IDs,
oversized region lists, and trailing/truncated bytes; transport framing and
authority policy remain separate layers.

The descriptor stores the format version, world and base identities, all
world/region/chunk relationships, and bounded residency limits. Creating a
world allocates only the configured region and chunk residency tables; it does
not allocate an 8 km height field. Region state separately records CPU,
physics, render, pending-I/O, dirty, revision, and generation state.

## Current boundary

This slice establishes the shared data model and bounded ownership contract.
World manifest integration, journal compaction, streaming hysteresis and
eviction, integer mutation commands, collision regeneration, client
LOD/rendering, and server authority are subsequent validated runtime slices.
They must use this
same world identity, region/chunk mapping, revision, and residency ownership;
they must not introduce a second world-sized representation.
