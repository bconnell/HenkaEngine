# Planar Mesh Cut Design

Status: proposed for review; no implementation is included in this change.

## Purpose

Add a mesh-authority planar cut that splits every supported face crossed by one
plane in a single candidate-first operation. The operation extends Henka's
current isolated-quad loop cuts to connected polygon surfaces while keeping
topology, per-corner UV data, face metadata, selection, history, and persistence
consistent.

## Current boundary

`henka_authoring_mesh_loop_cut_face` splits one isolated quad. Its multi-cut
forms operate on isolated quads or bounded independent selections. They reject
shared-edge faces to avoid T-junctions. They do not define a plane and partition
arbitrary connected faces. The Sandbox already has a shared modeling operator
Preview, Apply, Cancel, undo, and redo pattern that can host a new mesh operation.

## Proposed first-version behavior

The public mesh operation accepts a finite point and non-zero normal defining a
plane in mesh-local coordinates. It normalizes the normal and evaluates the
whole authored mesh, not only the currently selected faces.

Every supported crossed polygon is divided into two simple polygons. Shared
source edges are classified once, and each geometric edge-plane intersection
creates one logical vertex reused by every incident face. Each face receives a
cut edge between its intersection points. The resulting cut-edge network is
selected after Apply in Edge mode.

The first version keeps both sides as face regions in the same logical object.
It does not discard either side, create a second object, or cap the cut. This is
a non-destructive surface-topology cut; it does not claim to create two closed
solids. Side removal, capping, and separate-object output remain later
capabilities.

The proposed editor operator is named **Planar Cut**. It exposes a local-space
plane point and normal, previews the candidate and cut edges, and uses the
existing Apply and Cancel controls. A snap-to-existing-vertex option may place
the plane point on a selected vertex. A freehand screen-space knife gesture is
outside this first version.

## Geometry and topology contract

- The complete result is built on a private candidate and validated before the
  committed mesh changes.
- A no-intersection request is an unchanged no-op and reports `changed = false`.
- A shared edge is split at most once, including when multiple incident faces
  use that edge.
- Each affected face must be simple and planar. Its plane intersection must
  produce one unambiguous cut segment. Faces requiring multiple disconnected
  cut segments, self-intersecting output, or ambiguous reconstruction reject
  the entire operation.
- Existing on-plane vertices are reused. Existing edges lying on the cut plane
  are reused. A coplanar face is unchanged. Any remaining ambiguous vertex,
  edge, or winding case fails closed.
- The operation rejects invalid/non-finite plane input, unsupported
  non-manifold topology, degenerate fragments, capacity exhaustion, and any
  result that fails the authoring-mesh validator.
- Rejection leaves mesh contents, stable-ID watermarks, selection, and revision
  unchanged.

## Identity and authored attributes

- Source vertices and edges not intersected by the plane retain their logical
  IDs.
- A split source edge retains its ID on the fragment adjacent to the lower-ID
  endpoint; the other fragment receives a fresh ID. New cut edges receive fresh
  IDs.
- The source face ID remains on the positive-normal-side polygon. The other
  polygon receives a fresh ID. The side convention is deterministic for a
  fixed plane orientation.
- Face material region, smoothing intent, and other face metadata are copied to
  both resulting polygons.
- New corner UV values are interpolated along the crossed source edges;
  per-corner values remain independent across incident faces and seams.
- New cut edges default to soft and non-seam. Existing edge hard/seam metadata
  is preserved on edge fragments.

## Preview, history, and persistence

Preview owns a candidate only and does not publish mesh state or consume
committed revisions. Cancel discards it. Apply rechecks the source revision,
publishes the complete validated candidate once, records one authoring-history
entry, and selects the cut edges. Undo restores the exact prior mesh and
selection; redo restores the cut mesh and cut-edge selection. Save/reload must
preserve the resulting topology, stable IDs, face metadata, and per-corner UVs
through the supported HAMS path.

## Verification boundary

Mesh tests should cover:

- a cube bisected through the centers of opposite faces, producing one
  connected cut-edge loop with no T-junctions;
- a cut passing through existing vertices and along an existing edge;
- a plane that misses the mesh and a plane coplanar with one face;
- UV interpolation and material, smoothing, hard-edge, and seam preservation;
- a concave face with one valid cut segment and a concave face requiring
  multiple segments;
- non-planar, non-manifold, degenerate, and malformed inputs;
- capacity failure and allocation failure with unchanged topology, IDs,
  selection, and revisions;
- deterministic output IDs and stable results for the same source and plane.

Product-path tests should cover:

- normal object selection and editable-mesh authoring;
- operator preview, plane-point snapping, Apply, Cancel, undo, and redo;
- preservation and restoration of component selection;
- save, reload, and reopen of the authored object;
- packaged Sandbox operation through the same editor path.

The executable operation remains the authority. A screenshot or mesh-unit test
alone does not establish editor, history, persistence, or packaged integration.

## Review decisions

The recommended first-version contract is both sides retained in one object,
without caps or side deletion. This keeps the operation non-destructive and
keeps the initial topology authority separate from solid slicing and object
creation.

Review this contract before implementation, especially the retained-both-sides
semantics, face-ID side convention, and the numeric plane controls with optional
vertex snapping. A different cap, side-retention, or plane-placement contract
changes the topology and history design and should be settled in this spec.
