# Stable Component Identities and Reusable Physical Storage

## Goal

Repair the bounded authoring mesh so logical vertex, edge, and face IDs remain
stable for a mesh lifetime without consuming one physical slot per historical
ID. Physical storage capacity must limit simultaneous active topology, not the
number of edits ever made to the mesh.

This is a foundation repair. It must finish before any Procedural Asset Graph
work begins.

## Current defect and evidence

The published baseline uses `id - 1` as the physical lookup for every
component. `vertex_slots`, `edge_slots`, and `face_slots` are both physical
high-water marks and ID allocators. Creation appends at those marks, validation
requires `id == index + 1`, and the face-creation rollback intentionally keeps
the edge high-water mark after a failed transaction. Delete only marks an item
inactive. Therefore a long sequence of delete/add operations exhausts the
configured arrays even when the active topology is below capacity. History
clones copy the high-water marks, so restoring a topology snapshot can also
restore an allocator state that is not safe for the current mesh lifetime.

## Invariants

- `0` and `HENKA_AUTHORING_INVALID_ID` are invalid logical IDs.
- A logical ID is never reused during one mesh lifetime.
- A deleted or superseded ID is removed from its live map and remains invalid.
- Reusing a physical slot always assigns a fresh logical ID.
- Physical vertex, edge, and face arrays have exactly the configured bounded
  capacities. Active counts, not historical counts, govern `HENKA_ERROR_LIMIT`.
- Each active element has one map entry and each live map entry resolves to one
  active physical slot. Duplicate IDs, missing IDs, stale map entries, and
  invalid references fail validation.
- Allocator watermarks are monotonic for a mesh lifetime. Candidate meshes may
  consume IDs privately; a failed candidate is discarded and cannot advance the
  surviving mesh. A published candidate carries its allocator state forward.
- A 32-bit logical-ID exhaustion condition returns `HENKA_ERROR_LIMIT` without
  partially mutating the mesh.
- Normal ID lookup is bounded and heap-free after mesh creation. Physical slot
  selection is deterministic, using the lowest available slot.
- Iteration and serialized ordering are physical-slot order, independent of
  hash-map placement.
- Candidate-first modeling, UV, repair, and topology operations publish only a
  validated candidate. Failure retains topology, maps, allocators, selection,
  render/collider inputs, bounds, and history-visible state.
- Undo/redo restores topology and metadata while retaining the maximum
  allocator watermark already reached by the mesh lifetime. It never rolls a
  watermark backward and never makes an abandoned redo branch's IDs live again.

## Representation

`henka_authoring_mesh` keeps bounded physical arrays and explicit active counts.
Each component class has a bounded open-addressing logical-ID map whose entry
contains `{ logical_id, physical_slot, state }`. The map capacity is derived
with checked arithmetic from the configured physical capacity and is allocated
with the mesh. Map rebuild is deterministic and rejects zero, invalid,
duplicate, out-of-range, or inactive references. The edge endpoint-pair map is
separate from logical-ID resolution and remains the canonical lookup for an
existing undirected edge.

The mesh also stores `next_vertex_id`, `next_edge_id`, and `next_face_id` as
the next fresh logical IDs. Allocation checks for zero/invalid/wrap before
publishing an element. Deletion clears the physical slot and removes the map
entry; it does not decrement a lifetime watermark. Physical slot selection
scans from zero for the first inactive slot, which is deterministic and bounded.

## Clone, copy, and history

An exact mesh clone copies active topology, logical IDs, allocator watermarks,
metadata, and rebuilt maps. History snapshots are topology snapshots plus the
watermarks required by that snapshot, but restoration applies:

`restored_next = max(current_next, snapshot_required_next)`

before rebuilding maps and validating. A candidate clone can therefore use
private IDs while the source remains unchanged on failure. A new checkpoint
after undo clears redo snapshots before the next edit can expose them; IDs
allocated after that edit remain fresh and cannot alias IDs from the discarded
branch.

## HAMS v4

HAMS v4 is required because storage position no longer defines identity. The
v4 header persists the four capacities, active vertex/edge/face counts, and the
three allocator watermarks. Active records persist their logical IDs and all
current vertex, edge, face, UV, material, smoothing, hard-edge, and topology
metadata. Records are written in deterministic physical-slot order. Load
allocates storage from the declared capacities, inserts active IDs into maps,
checks every reference and watermark, validates the complete candidate, and
swaps it into the destination only after EOF and validation succeed.

HAMS v2 and v3 remain loadable. Their slot-derived IDs become logical IDs for
the loaded mesh; storage is initialized independently, and the next ID for
each class is above all represented IDs and any required historical watermark
available in that format. Legacy files are not auto-resaved. Unknown versions,
truncated records, duplicate/zero/invalid IDs, invalid watermarks, active
counts over capacity, overflow, nonfinite values, malformed loops, nonexistent
references, and nonmanifold edge relations are rejected transactionally.

## Test contract

Before implementation, public-operation tests must fail for:

1. vertex physical-slot reuse with a fresh ID and stale-ID rejection;
2. edge reuse through face deletion/recreation;
3. face reuse with stable references for surviving topology;
4. undo followed by a new branch with no ID aliasing;
5. redo preserving IDs and maps;
6. a new edit after undo clearing redo while allocating fresh IDs.

The completed suite must additionally cover long-session churn through actual
public modeling operations, active-capacity overflow, candidate failure,
selection/history/render determinism, HAMS v4 round-trip, v2/v3 compatibility,
malformed v4 inputs, native save/close/open/re-edit, sanitizer balance,
MSVC warnings, and packaged visible modeling.

