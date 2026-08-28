# Terrain v1

The renderer-independent Terrain v1 core is owned by `henka_runtime` and is available to graphical clients and dedicated-server consumers through `<henka/terrain.h>`.

## Contents

- [World contract](#world-contract)
- [Persistence](#persistence)
- [Streaming](#streaming)
- [Deterministic editing and history](#deterministic-editing-and-history)
- [Network and authority](#network-and-authority)
- [Client recovery and prediction](#client-recovery-and-prediction)
- [Collision and physics](#collision-and-physics)
- [Render ownership and LOD](#render-ownership-and-lod)
- [Terrain materials](#terrain-materials)
- [Sandbox authoring workflow](#sandbox-authoring-workflow)
- [Visual evidence](#visual-evidence)
- [Validation coverage](#validation-coverage)
- [Current boundary](#current-boundary)

## World contract

The default Terrain descriptor represents an 8192 m × 8192 m world.

| Level | Layout | Size |
| --- | --- | --- |
| World | 16 × 16 regions | 8192 m × 8192 m |
| Region | 8 × 8 chunks | 512 m × 512 m |
| Chunk | 65 × 65 full-resolution samples | 64 m × 64 m |
| Sample spacing | — | 1 m |

The authoritative height field uses signed 32-bit integer millimeters. Rendering and physics convert heights to meters at their consumption boundaries.

Each sample reserves exactly four active material weights. The public normalization helper produces a deterministic total weight of 255 using integer arithmetic and a stable largest-remainder tie break.

The world descriptor stores:

- format version;
- world identity;
- packaged base identity;
- world/region/chunk relationships;
- bounded residency limits.

World creation allocates the configured region and chunk residency tables. It does not allocate the complete 8 km height field.

Region state tracks CPU, physics, render, pending-I/O, dirty, revision, and generation state.

## Persistence

`<henka/terrain_storage.h>` defines the versioned Terrain region record.

### Region record

Each binary record contains explicit little-endian fields for:

- world identity;
- packaged base identity;
- region identity;
- generation;
- revision;
- sample dimensions;
- signed millimeter heights;
- four material-weight bytes per sample;
- CRC-32 checksum.

Native C struct layouts are never written to disk. Records are bounded by `HENKA_TERRAIN_MAX_REGION_RECORD_BYTES`.

### Transaction journal

Runtime writes use an append-only journal with:

1. `BEGIN`;
2. bounded `REGION` records;
3. `COMMIT`.

Recovery publishes complete committed transactions only. Each region snapshot is validated, written to a confined temporary path, and atomically replaces the prior snapshot. Interrupted writes preserve the prior valid snapshot. Uncommitted journal records remain inert during recovery.

### Compaction

`henka_terrain_storage_compact`:

1. recovers complete transactions;
2. rejects compaction while a transaction is active;
3. atomically replaces the journal with an empty durable file.

Successful commits also trigger compaction after `HENKA_TERRAIN_STORAGE_AUTO_COMPACT_THRESHOLD_BYTES` is reached.

Region snapshots remain the durable state authority. Compaction does not load the full Terrain world or rewrite every region.

### Dirty-region persistence

The storage API provides a dirty-only transaction that skips clean resident regions. Dirty flags clear only after the bounded multi-region transaction commits.

The Sandbox uses this path for its ten-second normal-editor autosave. Failed autosaves leave dirty regions pending for a later retry. Capture and smoke modes do not schedule persistence.

## Streaming

The Windows runtime exposes a bounded worker-backed stream queue through `<henka/terrain_streaming.h>`.

### Worker boundary

A worker:

- borrows Terrain storage;
- loads and validates one immutable region candidate at a time;
- can run the configured missing-region generator;
- does not mutate renderer state;
- does not mutate live Terrain world state.

The runtime thread pumps bounded completions and performs the authoritative sample/revision publication.

### Request behavior

The streaming layer supports:

- duplicate request coalescing;
- queued-request cancellation;
- active-request cancellation at worker boundaries;
- bounded observer records;
- explicit caller requests;
- observer-generated requests;
- current and high-water diagnostics for queued requests, active work, completions, and observers;
- completion, failure, cancellation, stale-completion, and dropped-completion counters.

Observer-generated work and explicit `henka_terrain_streamer_request_region` work retain separate ownership. Removing or shrinking an observer cancels stale observer demand. Explicit requests remain active until completion or explicit cancellation.

An explicit request can coalesce onto a successful same-region completion already waiting in the pump queue.

### Observer residency

Observer updates request a bounded CPU-radius square. Resident regions are reconciled against the union of observer unload-radius squares.

Regions outside that union are released in deterministic row-major order when they have:

- no physics residency;
- no render residency;
- no pending I/O;
- no dirty edits.

A zero unload radius uses the CPU radius. A larger unload radius provides movement hysteresis.

Loaded regions synchronize their physics/render residency flags from the observer-radius union.

### Missing-region generation

A stream descriptor may provide a bounded generator for missing snapshots. Corrupt persisted snapshots remain errors.

The generator receives:

- the immutable world/layout description;
- one caller-owned sample buffer.

Every generated sample must satisfy the normalized 255-weight invariant. Successful generated regions publish at revision/generation 1 after the main-thread snapshot swap.

Persisted region data has priority over generated content.

### Stale completion protection

Before publication, the pump compares a worker completion's generation/revision with the currently resident authoritative state.

Older completions are discarded and counted in `stale_completion_count`.

Observer-only completions that become irrelevant before the pump are discarded and counted in `cancelled_completion_count`. A coalesced explicit request keeps the completion eligible for publication.

Publishing a validated region also clears its pending-I/O flag and releases the corresponding world pending-I/O budget slot exactly once.

### Working-set diagnostics

The graphical render owner reports high-water values for:

- pending requests;
- resident chunks;
- visible chunks.

The collision runtime reports its high-water pending-chunk count.

The renderer regression drives two render-resident regions through the public observer seam, relocates the camera, verifies nearest bounded working-set replacement, and confirms presentation slots are removed beyond the outer LOD band.

## Deterministic editing and history

`<henka/terrain_edit.h>` is the shared command path for:

- raise;
- lower;
- flatten;
- smooth;
- paint.

Each command carries:

- algorithm version;
- client nonce;
- integer sample center;
- bounded sample radius;
- falloff;
- operation values.

Linear and smooth falloffs use fixed-point integer weighting.

The runtime determines every affected resident region before allocating candidates. All candidates validate before any live sample or revision changes. Identical ordered command streams therefore produce byte-identical authoritative samples across runtimes.

The Sandbox editor and runtime callers use this same command path.

### Edit history

`<henka/terrain_edit_history.h>` records bounded before/after resident-region snapshots and restores revision, generation, and dirty state transactionally.

Undo and Redo operate on this shared history owner.

The Sandbox viewport brush emits one command per bounded cursor segment. Strokes stop when they reach nonresident Terrain. History operations refresh collision and render owners after publication.

Brush configuration is persisted through the Sandbox settings path.

### Network edit encoding

`<henka/terrain_network.h>` uses bounded explicit little-endian payloads for:

- edit requests;
- authoritative acceptance revisions;
- rejection reasons;
- deterministic edit deltas.

Requests contain world/base identity, client nonce, algorithm-versioned command fields, and the expected revision of each affected region.

Payload codecs reject:

- unsupported command fields;
- negative region IDs;
- oversized region lists;
- truncated bytes;
- trailing bytes.

## Network and authority

`<henka/terrain_authority.h>` provides the renderer-independent server validation boundary.

### Authority checks

The authority layer:

- bounds per-peer edit requests;
- optionally invokes a permission callback;
- verifies world identity;
- verifies packaged-base identity;
- requires the exact deterministic affected-region set;
- rejects stale region revisions.

An accepted edit is applied to the live world and persisted across all affected regions in one storage transaction. The server acknowledges the edit after the transaction commits.

Edit or persistence failure restores the previous live samples and revisions and abandons the incomplete transaction.

The authority object borrows the world and storage.

### Server session adapter

`<henka/terrain_server.h>` borrows the public ENet server and authority dependencies.

It handles:

- edit-message decode;
- authority dispatch;
- response encoding;
- control ping echo;
- connect-time session info;
- malformed edit-payload disconnects.

Session info contains world/base identities and up to 16 current resident regions with revision and generation. Regions are sorted in deterministic row-major coordinate order before encoding.

Clients validate the identities and request snapshots for missing or stale advertised regions.

### Lazy region materialization

Edit requests can materialize persisted regions before authority validation, subject to the world's resident-region limit.

Materialization is transactional for the request. Regions created by a request are released if a later region fails to load or publish. Preexisting residency is preserved.

`henka_terrain_server_diagnostics.materialization_failure_count` records failed allocation, load, or publication.

### Dedicated-server recovery

The dedicated server recovers complete journal transactions before opening its network endpoint.

When `--world` is supplied:

1. the path is validated as a read-only Terrain storage root;
2. `region_0_0.htr` is required;
3. the base region is loaded;
4. the runtime save root is loaded afterward;
5. committed runtime edits override base-region data.

The bounded smoke mode connects a loopback client and commits one deterministic edit when the save root is empty. A later run verifies the same committed revision.

### Accepted deltas

Accepted edits emit bounded reliable deltas on the Terrain channel.

Each delta includes:

- world/base identity;
- client nonce;
- server command identity;
- algorithm-versioned command;
- resulting revision for each affected region.

The server retains the last 64 accepted deltas in a fixed ring.

## Client recovery and prediction

### Delta-gap recovery

The client applies deltas only across exact revision steps.

A revision gap creates a bounded recovery request for the missing regional revision range. One pending recovery target is retained per affected region.

The client:

- suppresses repeated equal or older recovery requests;
- counts suppressed requests;
- replaces the pending target when a newer revision is observed;
- clears the pending entry after a successful recovered delta or snapshot;
- clears all pending entries on explicit reconnect.

Complete retained history is returned as deltas. Missing history uses the transactional regional snapshot path.

### Relevance request

Clients with an interest center may submit a bounded relevance request through `<henka/terrain_client.h>`.

The server:

- filters resident regions by Chebyshev radius;
- orders results by squared distance;
- uses coordinate-stable tie breaking;
- caps the response at 16 regions.

The client treats the filtered response transactionally and requests snapshots only for the selected region set.

Per-region session snapshot requests coalesce around the current target revision/generation. Newer advertisements replace the target. Equal or older advertisements are suppressed and reported in diagnostics.

Malformed or identity-mismatched relevance requests disconnect the peer.

### Snapshot transport

Snapshot requests identify:

- world;
- packaged base;
- region;
- expected revision.

The server reads the validated region record and emits transfer-identified fragments with revision, generation, total size, fragment index, fragment count, and payload bytes.

Each fragment stays under the existing 32 KiB snapshot payload limit.

The replica requires:

- the exact `ceil(total-size / payload-limit)` fragment count;
- full-size non-final fragments;
- valid world/base identity;
- valid checksum and decode;
- total data within the configured byte budget.

Valid fragments may arrive out of order. Publication occurs after every required fragment is present and the decoded candidate validates.

Decoded snapshots older than the current resident generation/revision are rejected and counted in `stale_snapshot_count`.

A failed final allocation clears the failed transfer and preserves the current resident region. A later transfer can retry.

### Snapshot retry

When a decoded fragment is rejected, the public Terrain client can request a new snapshot for the same region and target revision once per transfer, up to four times per connection.

`recovery_snapshot_request_count` reports these retries.

Undecodable messages remain hard protocol errors because a trusted retry identity is unavailable.

Disconnect retires all delta-recovery and session-snapshot suppression entries.

### Out-of-interest deltas

A validated delta for a region absent from the client's current resident interest set is a bounded no-op. The client does not create the region or request gap recovery. A later relevance/session response acquires the authoritative snapshot.

### Prediction

`<henka/terrain_prediction.h>` owns a separate bounded presentation world for local commands.

Prediction:

- copies CPU-resident authoritative regions;
- applies pending commands in submission order;
- rebuilds from authoritative state after acceptance or rejection;
- fails closed when the pending-command bound is exceeded.

The authoritative replica remains unchanged by prediction.

## Collision and physics

### Collision patch extraction

`<henka/terrain_collision.h>` extracts one physics-resident chunk into a caller-owned 65 × 65 signed-millimeter patch.

The extraction allocates no memory and does not mutate the Terrain world.

Each patch carries source revision and generation for stale-work rejection.

### Terrain physics owner

`<henka/terrain_physics.h>` owns bounded copied patches and supports:

- stable overlapping-patch selection;
- bilinear height queries;
- finite normal queries;
- source identity reporting;
- allocation-free bounded ray traversal through `henka_terrain_physics_raycast`.

Raycast misses remain misses outside the resident physics set.

### Rigid-body heightfield

The rigid-body API supports a static heightfield collider with copied signed-millimeter source data.

Current heightfield interaction includes:

- sphere contacts;
- axis-aligned box contacts;
- Terrain normals;
- layer/mask filtering;
- bounded heightfield raycasts;
- transactional replacement.

### Collision runtime queue

`<henka/terrain_collision_runtime.h>` owns a fixed-capacity coalescing rebuild queue.

The runtime thread builds full-resolution patches for physics-resident chunks and publishes each replacement after candidate success. A failed rebuild keeps the previous valid patch.

`henka_terrain_collision_runtime_sync_residency`:

- tracks bounded patch identity;
- queues missing or stale physics-resident chunks in stable order;
- removes patches after their regions leave physics residency.

`henka_terrain_collision_runtime_set_focus` gives one deterministic camera/interaction focus region admission priority. When capacity is full, one non-focus patch may be evicted to admit the focus patch.

The configured physics patch capacity is the hard admission bound.

`henka_terrain_collision_runtime_request_edit` derives height-edit coverage plus one physics-neighbor chunk. Paint-only edits do not enqueue collision work because they leave heightfield geometry unchanged.

The Terrain server can borrow the collision queue after authority acceptance. The Sandbox owns this runtime beside its physics world and uses it for camera residency, local sculpt edits, and region reload.

## Render ownership and LOD

### Mesh construction

`<henka/terrain_mesh.h>` provides the renderer-independent mesh boundary.

`henka_terrain_mesh_build_chunk` requires a render-resident chunk and fills caller-owned buffers for LOD 0 through LOD 3.

Generated data includes:

- finite central-difference normals;
- orthogonal tangent `vec4` values with deterministic handedness;
- stable world-space UV transport;
- the four normalized material weights;
- source revision and generation.

The source identity lets a graphics owner discard stale uploads.

### Graphical render owner

`<henka/terrain_render.h>` owns Terrain GPU presentation in the graphical client.

It borrows:

- engine;
- scene;
- Terrain world;
- optional manager-owned material definition and textures.

It owns:

- fixed-capacity chunk slots;
- rebuild queues;
- helper scene entities;
- Terrain GPU meshes;
- separate paint-weight buffers.

Terrain helper entities carry owner marks and bounds used by renderer culling and depth presentation. Generic Scene Objects selection, duplicate, delete, and transform actions reject these helpers.

A missing presentation helper is recreated during the next dirty/residency refresh. Failed recreation preserves the prior slot mesh and records the rebuild failure.

### LOD and transitions

Four LOD bands use hysteresis and deterministic adjacent-chunk selection. Visibility is bounded by the configured outer band.

When an adjacent resident chunk is exactly one LOD coarser, the fine chunk receives a four-edge transition mask.

The transition mesh:

- keeps shared even edge samples;
- redirects odd fine-edge indices to shared coarse endpoints;
- omits collapsed triangles;
- morphs intermediate fine-edge positions onto the coarse boundary;
- renormalizes interpolated normal/tangent data;
- restores the 255-sum material-weight invariant.

A missing or invalid neighbor uses a bounded downward skirt on that edge until the neighbor becomes resident.

Chunk diagnostics record transition and skirt masks.

### Working-set refresh

Each observer update derives a deterministic nearest render working set from render-resident regions.

The owner:

- removes chunks beyond the outer LOD band;
- removes chunks whose regions lose render residency;
- queues bounded missing chunks;
- compares uploaded revision/generation against the world;
- queues stale resident chunks for replacement;
- tracks border-neighbor identity used for normal generation;
- queues dependent chunks when relevant neighboring region identity changes.

Queued work that reaches the pump after its source loses render residency is retired as obsolete without a rebuild-failure count.

`henka_terrain_render_runtime_request_edit` queues resident chunks covered by an accepted edit plus a one-chunk dependency border.

`henka_terrain_render_runtime_refresh_dirty` performs the same revision/dependency check for snapshot, remote-delta, and reload owners.

### Paint-only GPU updates

Paint edits use a separate four-byte-per-vertex Terrain weight stream.

The graphical owner builds a candidate weight payload and swaps the GPU buffer only after successful upload. Mesh VAO, indices, geometry, bounds, and entity identity remain unchanged.

Diagnostics expose:

- `weight_updates`;
- `failed_weight_updates`;
- `gpu_weight_bytes`.

Height edits and topology/LOD/border-normal/dependency changes use full transactional mesh replacement.

## Terrain materials

The built-in Terrain material blends exactly four normalized painted weights in world space.

Each layer supports validated:

- base-color texture;
- normal texture;
- metallic/roughness texture;
- base color factor;
- metallic factor;
- roughness factor;
- normal strength;
- meters-per-tile value.

The normal Rendered shader consumes the Terrain weights. Generic material vertex-color tint is disabled for this material.

### Sandbox material fixture

The Sandbox creates deterministic 64 × 64 manager-owned runtime textures for:

- Grass;
- Dirt;
- Rock;
- Wet.

Each layer provides base color, finite-difference tangent normal, and metallic/roughness content with bounded multi-frequency variation.

Rendered Terrain also applies bounded world-space macro/detail variation to albedo, roughness, and tangent normals.

Current Terrain rendering does not provide authored texture streaming, displacement, or parallax.

### Material ownership

The graphical owner reports exact unique layer-texture count and resident material bytes. The Sandbox reference fixture expects all twelve semantic layer texture slots to participate without duplicate handle counting.

The asset-manager dependency inspection contract enumerates all twelve optional slots and their semantic usage for material definitions and effective instances.

Terrain render descriptors may carry a stable identity returned by `henka_assets_adopt_runtime_material`.

Adoption validates:

- material values;
- shader ownership;
- texture ownership;
- duplicate identity.

The asset manager owns the adopted definition and its identity. Generated runtime definitions have no source-file reload path.

## Sandbox authoring workflow

The Sandbox reference scene seeds four deterministic regions under the persistent `terrain-sandbox-v2` storage root.

The fixture contains:

- rolling ground;
- a valley;
- a steep ridge/cliff;
- continuous Grass/Dirt/Rock/Wet weights.

Existing committed samples are retained.

### Camera-driven residency

The same camera feeds the streaming observer with a bounded four-region CPU budget and one-region CPU, physics, render, and unload windows.

Request priority is deterministic:

1. render-radius regions;
2. physics-radius regions;
3. remaining CPU-radius regions ordered by Chebyshev distance;
4. stable request sequence as the final tie break.

Moving or removing an observer re-scores queued requests and cancels observer-only work that leaves all active CPU-radius demands.

### Stream stress

Run:

```text
--terrain-stream-stress
```

The scenario moves through:

```text
(0,0) -> (2,0) -> (2,2) -> (0,0)
```

It verifies:

- the initial one-region camera window;
- bounded worker/render queues;
- rendered return on both movement axes;
- a bounded collision-patch overlap on return;
- the resident-region bound.

Normal movement is observer-driven and pumps at most two render replacements per frame.

The same deterministic generator supports valid unpersisted regions encountered during movement. Edits become durable through the normal Terrain storage path.

### Terrain utility

The Utility > Terrain panel exposes:

- resident/render/collision statistics;
- dirty-region count;
- Raise;
- Lower;
- Flatten;
- Smooth;
- Paint;
- brush radius;
- strength;
- layer;
- falloff;
- material-layer dependency inspection;
- transactional Save;
- committed-journal Compact.

A resident physics hit supplies the integer sample center for the next edit command.

A bounded horizontal brush preview follows a successful resident physics hit while the Terrain utility is active.

The Scene View displays the current Terrain edit operation, radius, strength, falloff, and paint layer.

When a scene object and Terrain are both under the cursor, the nearest finite hit wins. Nonresident Terrain is never synthesized as a hit.

### Material inspection

The Terrain utility reports live manager-owned texture information for Grass, Dirt, Rock, and Wet:

- dimensions;
- GPU format;
- resident mip count;
- total mip count.

This is dependency inspection and bounded editing support. Complete Terrain material-preview authoring remains future work.

### Reload

Reload decodes into a bounded candidate and then rebuilds physics and render presentation.

Presentation failure restores the previous samples, revision, generation, and collision patch.

## Visual evidence

Run application-only Terrain evidence with:

```powershell
.\scripts\capture_visual_evidence_windows.ps1 -Configuration Release -IncludeTerrain
```

The capture set includes deterministic:

- wide views;
- close material views;
- four-region corner views;
- Solid;
- Material Preview;
- Rendered.

Capture mode refreshes the four fixture regions from their seeded samples so persisted editor edits do not alter the comparison set.

The evidence checks use:

- `scripts/check_terrain_visual_evidence_windows.ps1`;
- `scripts/check_terrain_corner_visual_evidence_windows.ps1`;
- `scripts/check_terrain_close_visual_evidence_windows.ps1`.

The automated guards reject:

- missing images;
- dimension mismatches;
- flat material content;
- low-chroma material content;
- spatially featureless content;
- Rendered output that is not measurably distinct from Material Preview.

Human visual QA remains the authority for overall Terrain appearance, seam quality, corner quality, material quality, and presentation judgment.

## Validation coverage

Current executable coverage includes:

### Headless workflow

`henka_terrain_workflow_tests` validates:

- deterministic raise;
- deterministic paint;
- authoritative region mutation;
- full-resolution collision refresh;
- committed persistence into a new world and physics owner;
- discarded uncommitted journal transactions during recovery.

### Collision runtime

The collision runtime regression:

- fills an intentionally undersized rebuild queue;
- verifies rejected-request accounting;
- reuses released capacity;
- verifies failed rebuild retention;
- verifies successful retry.

### Rendering

The mesh/render regressions cover:

- owner-marked helper behavior;
- cast/receive shadow flag retention;
- selection/delete/picking rejection for Terrain helpers;
- teardown without stale Terrain entities;
- all-four-edge transition generation;
- fewer stitched indices than the regular grid;
- no degenerate transition triangles;
- finite tangent bases;
- all four fine/coarse boundary positions;
- shared-corner behavior;
- odd fine-edge redirection to coarse endpoints;
- failed dirty replacement retaining the prior mesh, revision, and bounds;
- stale-work cancellation after render residency is revoked;
- replacement recovery after residency is restored.

### Sandbox smoke

The Sandbox routes shared raise and paint commands through the production Terrain path.

Raise validates authoritative mutation, collision refresh, and GPU mesh refresh. Paint validates authoritative material-weight and rendered-revision updates while leaving collision geometry unchanged.

Rendered smoke also records Terrain participation in current-frame color, shadow, depth, AO, reflection, temporal, fog, and HDR/IBL consumers where applicable. Terrain helpers are excluded from generic local reflection-probe capture.

Terrain memory reporting includes exact world-owned CPU bytes and graphical-owner vertex/index/weight/material GPU bytes. Borrowed renderer resources are outside those Terrain-owner totals.

### Multiplayer recovery

The public client/server tests cover:

- bounded connect-time resident bootstrap;
- coordinate-stable relevance selection;
- two replicas connected to one authoritative server;
- one edit from each peer;
- complete resident sample convergence;
- forced disconnect;
- reconnect;
- authoritative server-wrapper replacement;
- late observer;
- restart checksum convergence;
- finite repeated process soak.

## Current boundary

Current Terrain v1 includes:

- bounded renderer-independent world ownership;
- deterministic region/chunk/sample mapping;
- transactional region persistence and recovery;
- bounded observer-driven streaming;
- deterministic editing and history;
- authoritative network edits;
- bounded delta and snapshot recovery;
- bounded relevance selection;
- client prediction in a separate presentation owner;
- physics-resident collision patches;
- rigid-body heightfield interaction;
- deterministic collision rebuild queues;
- render-resident chunk ownership;
- LOD 0–3;
- one-level cross-LOD stitched edges and morphing;
- four-layer PBR Terrain materials;
- paint-only GPU weight updates;
- Sandbox editing, persistence, streaming, and evidence paths.

Remaining maturity work includes:

- production-scale streaming and multiplayer capacity validation;
- broader residency-wide dirty-neighbor scheduling beyond bounded physics patch capacity;
- deeper four-way corner visual validation;
- asynchronous background physics/render regeneration;
- broader large-world orchestration;
- fuller Terrain material authoring;
- broader persistence scheduling for nonresident world authoring;
- application authentication and higher-level gameplay networking policy.

Future Terrain systems must keep the current world identity, region/chunk mapping, revision, and residency ownership contracts as the shared authority.
