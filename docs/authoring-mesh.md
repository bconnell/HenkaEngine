# Authoring mesh foundation

Henka exposes a bounded polygonal authoring mesh in
`<henka/authoring_mesh.h>`. It is the topology layer for the editor and is
separate from the renderer's evaluated mesh representation.

The current foundation provides:

- stable, non-reused vertex, edge, and face IDs with tombstones for deleted
  elements;
- explicit polygon corners and edges, with deterministic vertex-edge and
  edge-face adjacency and boundary queries;
- bounded material-region and UV data, face smoothing intent, and hard-edge
  intent;
- fail-closed face validation, non-manifold edge rejection, deletion safety,
  and deterministic fan triangulation into caller-owned render buffers;
- evaluated normals that honor smooth-face and hard-edge intent.
- bounded shared topology undo/redo snapshots and versioned transactional
  mesh-file save/load with failed-load retention.

`<henka/authoring_modeling.h>` adds bounded plane and box constructors plus
transactional duplicate, extrude, inset, planar bevel-ring, and face
subdivide operations. Each operation works on a clone and publishes only a
validated result, so capacity or non-manifold rejection leaves the source
mesh unchanged.

`<henka/authoring_uv.h>` provides per-face planar projection on each principal
axis, bounded island transform and single-face packing helpers, finite-value
validation, and seam detection from shared topology. These are deterministic
UV primitives; automatic multi-island unwrap, seam editing UI, and global
packing remain unfinished.

## Connected workflow status

The mesh, modeling, UV, history, evaluation, and file APIs share one bounded
authoring representation: modeling and UV edits validate a clone before
publishing, history snapshots preserve stable topology IDs and metadata, and
evaluation reads the same committed source-of-truth. Versioned mesh-file load
also commits transactionally, so a malformed file does not replace the current
authoring state.

The horizontal editor connection is intentionally still incomplete. Sandbox
scene selection and object identity are not yet backed by an authoring-mesh
asset; evaluated buffers are caller-owned and are not yet transactionally
installed into a scene render mesh; history is not yet the automatic action
boundary for editor modeling commands; and mesh save/load is not yet wired to
the editor's open/save workflow. Material regions retain their numeric
metadata, but material-instance assignment, texture dependencies, bounds,
picking, collision, package ownership, and showcase authoring still need the
shared scene/asset-manager bridge. Those limitations are tracked explicitly
so this API does not claim a disconnected modeling editor or a second material
authority.

The API allocates only within caller-selected bounded capacities. Invalid
faces and capacity failures leave the prior topology unchanged. Render
buffers are caller-owned, so evaluation does not transfer ownership to the
renderer or asset manager.

This is the bounded runtime foundation of the authoring-parity campaign. It is
not yet a full modeling editor: selection UI, weld/split/delete/bridge/loop
cuts, production hard-surface profiles, automatic multi-island UV unwrap and
global packing, material editing, texture painting, editor integration for the
history/file APIs, and showcase rebuild workflows remain unfinished. glTF
and KTX2 material ownership continues through the existing asset paths; this
API does not introduce a second material file format.
