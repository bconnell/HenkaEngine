# Stable authoring component identities

This document describes the storage and persistence design for Henka's
bounded authoring mesh. It is a contributor-facing architecture reference for
the C17 engine and its public mesh, modeling, UV, history, and persistence
interfaces.

## Purpose

Vertex, edge, and face IDs are public logical identities. Physical storage is a
bounded implementation detail. A long authoring session must be able to delete
and create components repeatedly while the active topology remains within its
configured capacities.

Before the identity repair, the slot-based representation used `id - 1` for
lookup and consumed a physical slot for every historical ID. The current
representation separates those concerns: physical capacity limits simultaneous
active topology, while logical IDs remain stable and non-reused for the
lifetime of a mesh.

## Storage model

Each mesh owns fixed-capacity physical arrays for vertices, edges, and faces,
plus active counts. Each component class also owns a bounded open-addressing
map with entries containing a logical ID, physical slot, and explicit map state.
The maps are allocated with the mesh, use checked capacity arithmetic, and do
not grow during normal lookup or editing.

The endpoint-pair lookup for undirected edges remains separate from logical-ID
resolution. It maps a canonical low/high vertex pair to an edge ID and is
rebuilt from active physical records when required. Map placement never affects
public iteration or serialized ordering; iteration is always deterministic
physical-slot order.

Inactive slots are reusable. Allocation selects the lowest available physical
slot and assigns a fresh logical ID. Deletion removes the ID from the live map,
clears the physical record, and leaves the lifetime watermark unchanged. Zero
and `HENKA_AUTHORING_INVALID_ID` are invalid. A duplicate, stale, missing, or
invalid map entry fails validation.

The mesh stores monotonic `next_vertex_id`, `next_edge_id`, and
`next_face_id` watermarks. Allocation rejects zero, the invalid sentinel, and
32-bit exhaustion before publication. A candidate clone may consume IDs in
private storage, but a failed candidate is discarded and cannot advance the
surviving mesh. A successfully published candidate carries its watermarks
forward.

## Transactions and history

Modeling, topology repair, UV editing, and related mesh mutations use the
candidate-first boundary: clone the source, apply and validate the complete
candidate, rebuild maps, and publish only after all checks pass. A failure
retains the source topology, maps, IDs, selection, render inputs, bounds,
collider inputs, and history-visible state.

An exact clone copies active topology, metadata, logical IDs, allocator
watermarks, and map state. History snapshots restore topology and metadata but
never lower a mesh lifetime watermark. Restoration applies the maximum of the
current watermark and the snapshot's required watermark before rebuilding maps
and validating. Undo and redo therefore preserve stable identities, and a new
edit after undo cannot resurrect or alias an abandoned redo branch.

## HAMS persistence

HAMS v5 is the format boundary for decoupled identity, physical storage, and
loose-component semantics. The v5 header stores the four configured
capacities, active vertex/edge/face counts, and the three allocator watermarks.
Active records store their logical
IDs and all current vertex, edge, face, UV, material, smoothing, hard-edge, and
topology metadata. Records are serialized in deterministic physical-slot
order.

HAMS v4 remains loadable as a surface-only compatibility format. It has the
same record layout and identity watermarks, but its validity contract requires
every active edge to have at least one incident face. HAMS v5 is therefore
required for standalone zero-face wire edges and loose vertices; the loader
does not guess which semantic contract a v4 file intended.

Loading builds an independent candidate, inserts and checks every active ID,
validates all references and watermarks, rejects trailing or truncated data,
and swaps the candidate into the destination only after complete topology
validation. Failed loading retains the prior valid destination.

HAMS v2 and v3 remain loadable. Their slot-derived IDs become the logical IDs
of the loaded mesh, storage is initialized independently, and each next-ID
watermark is placed above the IDs represented by the file. HAMS v2, v3, and v4
are migrated in memory only; legacy files are not automatically rewritten as
v5. A loose component encoded under v2, v3, or v4 is rejected as malformed.

Malformed input is rejected for unknown versions, duplicate/zero/invalid IDs,
nonexistent references, invalid watermarks, active counts above capacity,
checked-arithmetic overflow, nonfinite values, malformed loops, and
non-manifold edge relations.

## Observable guarantees

The public contract is demonstrated by reuse of vertex, edge, and face slots;
stale-ID rejection; active-capacity overflow; history undo/redo and branch
semantics; repeated public modeling churn; deterministic evaluation; HAMS v5
round trips; v2/v3/v4 compatibility; loose-component persistence;
malformed-load retention; native reopen and
re-edit behavior; sanitizer coverage; and packaged editor/runtime use.

This design does not claim that arbitrary production assets are authored by a
fixture or that the full modeling roadmap is complete. It defines the bounded
identity foundation on which those higher-level authoring workflows depend.
