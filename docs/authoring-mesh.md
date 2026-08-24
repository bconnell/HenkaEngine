# Authoring mesh foundation

Henka exposes a bounded polygonal authoring mesh in
`<henka/authoring_mesh.h>`. It is the topology layer for the editor and is
separate from the renderer's evaluated mesh representation.

The storage and persistence design for decoupling stable logical component
identities from reusable physical slots is documented in
[Stable authoring component identities](authoring-component-identities.md).
That document describes the bounded migration contract; the capability list
below remains the current implementation inventory.

The current foundation provides:

- stable, non-reused logical vertex, edge, and face IDs resolved through
  bounded maps; inactive physical slots are reusable;
- explicit polygon corners and edges, with deterministic vertex-edge and
  edge-face adjacency and boundary queries;
- bounded material-region and UV data, transactional face material-region
  editing, face smoothing intent, and hard-edge intent;
- fail-closed face validation, non-manifold edge rejection, deletion safety,
  and deterministic fan triangulation into caller-owned render buffers;
- evaluated normals that honor smooth-face and hard-edge intent.
- bounded shared topology undo/redo snapshots and versioned transactional
  mesh-file save/load with failed-load retention.

`<henka/authoring_topology.h>` provides non-destructive topology analysis and
an explicit candidate-based repair boundary. Analysis reports component,
boundary, manifold, winding, seam, hard-edge, degeneracy, duplicate-face,
coincident-vertex, valence, and face-shape metrics. Repair is opt-in and
bounded: it can remove isolated vertices, exact duplicate faces only when
their winding, UVs, material, and smoothing metadata agree, and degenerate
faces. The source is replaced only after candidate validation and a final
analysis; unsafe duplicate groups, non-manifold results, vertex welding, and
winding rewrites fail closed.

`<henka/authoring_modeling.h>` adds bounded plane and box constructors plus
transactional duplicate, extrude, inset, planar bevel-ring, face subdivide,
and bounded standalone boundary-edge bevel operations. Each operation works on a clone and publishes only a
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

The first horizontal editor connection is now exercised by the Sandbox's
selected Textured Cube and Add Cube results: each owns a bounded authoring box
and history, and the viewport ray picker resolves a hit to the actual
authoring component identity.
Object Details Authoring exposes bounded Vertex, Edge, and Face selection modes;
Ctrl-click adds components to the active mode, and the viewport draws the
selected vertices as amber crosses, selected edges as cyan segments with endpoint
markers, and selected faces as orange borders with a center marker. The most
recently picked component is the active edit target: it receives a stronger
mode-specific stroke/marker while the rest of a multi-selection remains visible.
Dragging in Scene View performs bounded box selection against projected source
components. Replace, Ctrl-add, and Shift-subtract commit atomically. Normal mode
accepts only front-facing components proven frontmost by a source-mesh ray;
X-Ray keeps the front-facing policy but permits selection through occluding
mesh surfaces. Renderer triangulation is never exposed as authored edges.
The Scene View also shows the active topology mode and selected-component count,
including when the current mode has no component selected yet. Small Move X+, Move Y+, and Move Z+
commands offset the selected components through a cloned mesh and the existing
transactional scene/render/bounds/collider publication path. Face mode also
exposes Normal + and Normal - commands that translate the active face's shared
vertices along its evaluated local-space normal through the same transaction.
This keeps the face connected to neighboring topology while providing a direct
bounded profile-shaping operation for native-authored anatomy and mechanical
forms. The operation rejects malformed or degenerate faces and distances outside
its bounded editor range. Face mode exposes Grow Selection, which expands the
active selection by one topology-adjacent
ring, Select Connected, which continues that expansion to the complete
reachable component within the bounded selection budget, and Scale Selected,
which scales the touched vertices around their median pivot through the same
transactional path. Select All, Select None, Invert, and Shrink operate on the
active topology mode with deterministic sorted component IDs; failed
replacement allocation leaves the prior selection intact. Rotate Selected and
Scale Selected expose explicit median, active-component, and per-face
individual pivot policies through the authoring API, plus world, local, and
face-normal orientation for rotation. The sandbox controls use bounded local
median transforms, while callers can select the other policies explicitly.
Soft Move X+, Soft Move Y+, and Soft Move Z+ apply a bounded
one-ring linear falloff: the active selection receives the full translation and
directly adjacent vertices receive half strength. These are bounded generic
selection/modeling operations rather than showcase-specific geometry rules, and
the falloff is a foundation for shaping rather than final anatomy or mechanical
topology proof.
Face mode also exposes
the selected face plus transactional material
region editing, Extrude, Inset, Bevel, Subdivide, Project UV, Pack UV, Undo,
Redo, Save Project, and Reload Project commands. Save Project writes a bounded
versioned manifest beside the existing transactional `.hams` topology source;
the manifest retains the source path, transform, and visibility needed to
reopen the current bridge. Each edit
evaluates a candidate, creates a normal renderer mesh, updates the scene entity
mesh and local bounds, then checkpoints history; a bound box collider consumes
the same evaluated local bounds as part of that transaction after each
successful editor operation, so the editor does not rely on a later manual
physics refresh. Reload builds a replacement
history from the validated source before swapping the scene representation. Any
evaluation, renderer, scene, bounds, history, or file-parse failure retains the
prior source, render, and (when the linked body is present) spatial state,
including the prior collider. The save/reload buttons use a confined,
engine-owned user-data slot derived from the selected entity identity, so
selecting or duplicating an authored object cannot overwrite another authored
object's source through the editor controls; they do not scan or overwrite
arbitrary files. This remains per-object authoring persistence, not complete
scene/project serialization.
The bridge stores one bounded selected-face identity beside each mesh-history
snapshot. Topology operations select their deterministic result, undo/redo
restores the corresponding prior or next face when it still exists, and a new
edit after undo truncates both histories together. Reload resets the selection
history to the validated replacement source. A missing or malformed project
manifest, source, or transform is rejected without replacing the current scene
mesh, bounds, transform, visibility, or authoring history.
When the bounded authoring wrapper closes, it restores the mesh and local bounds
that belonged to the entity before authoring took ownership, provided another
editor path has not replaced the active evaluated mesh in the meantime.
Scene selection remains the existing generation-checked entity authority rather
than a second selection system.

The broader horizontal connection is still incomplete. General project open/save
and arbitrary authoring-file selection are not yet editor workflows. The
Sandbox bridge clones a validated authoring source into an independent
per-entity history/render/bounds handoff, and the editor keeps a bounded
per-entity registry for authored objects: duplicating an authored object creates
an independent editable source, while selecting another entity activates only
that entity's wrapper. When the source has the Sandbox box-collider contract,
the duplicate receives a separate bounded collider and its collider is retired
with the duplicate; the source body remains owned by its original descriptor.
Imported entities are not yet automatically authoring-enabled. Wrapper-level
vertex merge is present; bounded vertex dissolve/delete/connect are now
available through the core API and Sandbox Vertex Modeling bridge. Dissolve
supports boundary corner removal and unambiguous manifold triangle fans;
ambiguous, hard-edge, UV-seamed, non-triangle, and non-manifold cases fail
closed. Delete removes selected vertices and their incident faces, then
removes only newly orphaned vertices in the affected neighborhood. Connect
splits one face between two non-adjacent corners while preserving the original
face ID and allocating the new face with a fresh logical ID in a reusable
physical slot. Transactional single-edge dissolve is available for compatible
interior edges, and transactional single-edge delete removes the selected
edge's incident face set while preserving vertices. Bounded edge bevel is also
available for one boundary edge whose endpoints belong to one face only; it
  creates two interpolated cut vertices and one quad bevel face transactionally.
  Interior edges, ambiguous boundary vertices, and broader batch edge topology
  editing remain incomplete. A bounded single-quad face loop cut is also
  available: it interpolates two opposite boundary edges, creates two quad
  faces, and rejects shared-boundary faces so it cannot leave a T-junction in
  neighboring topology. Full multi-face loop-cut networks remain unfinished.
Bounded Vertex Extrude is available for one unambiguous boundary corner of a
single face. It creates one cap vertex and two connecting side faces while
preserving the operation's transactional source/render/bounds/collider/undo
boundary; multi-face vertex fans are rejected until their remapping contract
is stable. Vertex Bevel is also available as one atomic multi-selection operation. It
uses a deterministic edge/end-point cut table, rejects non-finite, zero,
overlapping, non-manifold, and capacity-invalid requests, preserves
per-corner UV interpolation and original hard trimmed segments, creates
same-material interior caps with deterministic planar UVs, and leaves normal
boundary vertices open. Successful Sandbox bevels replace Vertex selection with
the live cut vertices and use the same history/render/bounds/collider
transaction as other authoring edits. Bounded Vertex Extrude remains within
the validated face-surface representation and does not create standalone
loose edges or loose vertices.
material-instance assignment, texture
dependencies, general collision integration beyond the bound box contract,
package ownership, topology-aware picking, and showcase rebuilds still need the
shared scene/asset-manager bridge. The current authoring mesh is a validated
surface representation: active edges must belong to at least one face, and
standalone wire edges or loose vertices are rejected by validation. Broader
multi-face vertex-fan extrusion requires a stable remapping contract before
it can be exposed.
Material regions retain their editable numeric metadata, and the evaluated
model-to-render-mesh upload retains the bounded minimum/maximum region range
for diagnostics. They do not yet choose multiple shared material instances in
the renderer. These limitations are tracked explicitly so this API does
not claim a complete modeling editor or a second material authority.

The API allocates only within caller-selected bounded capacities. Invalid
faces and capacity failures leave the prior topology unchanged. Render
buffers are caller-owned, so evaluation does not transfer ownership to the
renderer or asset manager.

Client applications can call `henka_mesh_create_from_authoring_mesh` from
`<henka/mesh.h>` to evaluate the same committed source into an ordinary
renderer-owned triangle mesh. The source remains caller-owned, the output slot
must start empty, counts and indices are bounded and checked, and allocation or
evaluation failure leaves the output slot empty. This is the reusable
authoring-to-render boundary; it does not create a material authority or
replace glTF scene/material ownership.
`henka_authoring_mesh_save_file` writes HAMS v4 with explicitly little-endian
32-bit integers and IEEE-754 float bit patterns. Each save uses a unique
same-directory temporary name and atomically replaces the destination only
after the complete candidate is flushed, so a failed or concurrent save does
not remove the prior valid source. The loader accepts the current v4
format and legacy v2/v3 sources shipped with the repository.
`henka_authoring_mesh_load_file_new` can load a versioned `.hams` source without
requiring the consumer to duplicate the file's capacity header; it validates
the declared bounded capacities before creating the candidate and retains an
empty output slot on any failure.
`henka_authoring_mesh_get_bounds` exposes bounds from the same active source
vertices so a consuming scene can publish local bounds without a second
geometry interpretation.

Vertex merge is available as an explicit candidate operation through
`henka_authoring_mesh_merge_vertices` and
`henka_authoring_mesh_merge_vertices_by_distance`. Center and active-vertex
modes use deterministic stable-ID selection, preserve per-face corner UVs and
face metadata, reconcile active endpoint-pair edges without reusing retired
logical IDs, and publish only after the candidate validates. Distance mode uses
a finite positive tolerance, deterministic stable-ID union-find clustering, a
bounded spatial hash, and double-precision cluster means. A no-op distance
merge returns success without changing topology or history. The Sandbox
authoring bridge exposes these operations only in Vertex selection mode and
keeps the editable merge distance in transient per-object UI state; it is not
serialized into HAMS or project manifests.

The evaluator's tangent value is transport metadata for the bounded authoring
representation, not an authoritative UV-derived tangent basis. At the shared
authoring-to-render boundary, the renderer derives and orthogonalizes a stable
tangent whenever that basis is not authoritative. This keeps axis-aligned
authoring faces from treating non-authoritative tangent metadata as finished
shading data while preserving the single mesh/material ownership path.

This is the bounded runtime foundation of the authoring-parity campaign. It is
not yet a full modeling editor: broader weld/split/bridge and multi-face loop-cut
workflows, production
hard-surface profiles, automatic multi-island UV unwrap and global packing,
broader material authoring beyond the supported bounded material-instance
editing, texture painting, editor integration for the history/file APIs, and
showcase rebuild workflows remain unfinished. glTF and KTX2 material ownership
continues through the existing asset paths; this API does not introduce a
second material file format.
