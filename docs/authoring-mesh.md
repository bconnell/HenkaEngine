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

The API allocates only within caller-selected bounded capacities. Invalid
faces and capacity failures leave the prior topology unchanged. Render
buffers are caller-owned, so evaluation does not transfer ownership to the
renderer or asset manager.

This is checkpoint A of the authoring-parity campaign. It is not yet a full
modeling editor: primitive tools, extrude/inset/bevel/subdivide/weld/split,
UV unwrap and packing, material editing, texture painting, editor integration
for the history/file APIs, and showcase rebuild workflows remain unfinished.
glTF
and KTX2 material ownership continues through the existing asset paths; this
API does not introduce a second material file format.
