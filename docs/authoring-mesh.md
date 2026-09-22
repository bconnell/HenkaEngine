# Authoring Mesh Foundation

Henka exposes a bounded polygonal authoring mesh through `<henka/authoring_mesh.h>`. This is the editor topology layer. The renderer consumes evaluated mesh data produced from this source.

> **Status:** Available within the bounded foundational Modeling scope. The mesh, topology, modeling, UV, history, persistence, evaluation, scene, renderer, bounds, and bounded collider paths share one transactional source workflow.

Stable logical component identity and reusable physical-slot storage are documented in [Stable authoring component identities](authoring-component-identities.md).

## Contents

- [Core representation](#core-representation)
- [Topology analysis and repair](#topology-analysis-and-repair)
- [Modeling operations](#modeling-operations)
- [UV operations](#uv-operations)
- [Connected Sandbox workflow](#connected-sandbox-workflow)
- [Selection and transforms](#selection-and-transforms)
- [Persistence and history](#persistence-and-history)
- [Vertex modeling](#vertex-modeling)
- [Edge modeling](#edge-modeling)
- [Loop Cut and Edge Slide](#loop-cut-and-edge-slide)
- [Loose components](#loose-components)
- [Evaluation and renderer handoff](#evaluation-and-renderer-handoff)
- [Material-region behavior](#material-region-behavior)
- [Known limits](#known-limits)

## Core representation

The current foundation provides:

- stable, non-reused logical vertex, edge, and face IDs resolved through bounded maps;
- reusable inactive physical slots;
- explicit polygon corners and edges;
- deterministic vertex-edge and edge-face adjacency;
- boundary queries;
- bounded material-region metadata;
- per-corner UV data;
- transactional face material-region editing;
- face smoothing intent;
- hard-edge intent;
- fail-closed face validation;
- non-manifold edge rejection;
- deletion safety;
- deterministic fan triangulation into caller-owned render buffers;
- evaluated normals that honor smooth-face and hard-edge intent;
- bounded shared topology undo/redo snapshots;
- versioned transactional mesh-file save/load with failed-load retention.

The API allocates only within caller-selected bounded capacities. Invalid faces and capacity failures preserve prior topology. Render buffers remain caller-owned.

## Topology analysis and repair

`<henka/authoring_topology.h>` provides non-destructive analysis and explicit candidate-based repair.

### Analysis reports

Analysis covers:

- connected components;
- boundaries;
- manifold state;
- winding;
- seams;
- hard edges;
- degeneracy;
- duplicate faces;
- coincident vertices;
- valence;
- face-shape metrics.

### Repair contract

Repair is opt-in and bounded. Supported safe repairs include:

- isolated-vertex removal;
- exact duplicate-face removal when winding, UVs, material, and smoothing metadata agree;
- degenerate-face removal.

Repair builds and validates a complete candidate before source replacement. A final analysis runs before publication.

The repair path rejects:

- unsafe duplicate groups;
- non-manifold results;
- implicit vertex welding;
- implicit winding rewrites.

## Modeling operations

`<henka/authoring_modeling.h>` provides bounded constructors and transactional modeling operations.

Current operations include:

- plane creation;
- box creation;
- duplicate;
- face winding flip for one or a bounded unique face selection;
- face-normal translation for one or a bounded vertex-disjoint face selection;
- deterministic planar face triangulation for one or a bounded vertex-disjoint
  face selection;
- face extrude;
- bounded vertex-disjoint face inset;
- bounded vertex-disjoint planar face bevel rings;
- bounded vertex-disjoint face subdivision;
- bounded convex planar face poke for one or a connected/disconnected selection;
- transactional deletion of one or more selected faces while preserving at
  least one renderable face;
- selected face-region extrusion with shared caps and transactional topology;
- bounded edge bevel;
- bounded loop-cut operations;
- bounded split of one selected face-backed boundary or interior edge, a
  pairwise-disjoint batch, a contiguous same-face boundary-edge chain, or a
  batch of independent boundary-edge chains at a factor in (0,1), preserving
  per-corner UVs and hard/seam metadata;
- bounded midpoint split of one selected standalone loose edge, preserving
  endpoint metadata and selecting the two replacement edges;
- bounded vertex and edge extrusion paths described below.

Each operation works on a clone and publishes only a validated result. Capacity, topology, geometry, or non-manifold rejection preserves the committed source.

Face-region extrusion accepts a deterministic set of selected faces and moves
the region along its averaged face normal. Selected adjacent faces share one
translated cap and do not receive an internal wall. An isolated region keeps
its source faces as the base; a region connected to unselected surface moves
its source face identities to the translated cap. Material regions, smoothing,
per-corner UVs, hard-edge intent, and seam intent are preserved. Duplicate,
invalid, unsupported, or failed selections leave the source unchanged. The
Sandbox editor routes the selected face region through the shared Preview,
Cancel, Apply, undo, and redo transaction, while the authoring-mesh API
remains the topology authority.

Selected-face deletion accepts a bounded unique face-ID set, validates the
complete candidate before publication, and preserves the source on invalid,
capacity-invalid, or failed requests. The shared Sandbox modeling session
routes the same transaction through Preview, Apply, Cancel, undo, and redo.

The shared Sandbox modeling session routes one or a bounded unique face
selection through the same preview, Apply, Cancel, undo, and redo boundary as
the vertex and edge extrusion paths.

Face flip preserves:

- logical face identity;
- vertex identities;
- edge identities;
- material metadata;
- smoothing metadata;
- per-corner UV correspondence.

The operation reverses only the ordered winding. Invalid or duplicate face
selections leave the committed source unchanged.

Planar face triangulation uses deterministic ear clipping and preserves the
source face identity on the first triangle, with fresh identities for the
additional triangles. The operation preserves material regions, smoothing,
and per-corner UVs. The batch form accepts only vertex-disjoint planar faces;
duplicate, shared-vertex, non-planar, invalid, and capacity-invalid selections
fail without changing the committed source.

Face inset supports one or a bounded vertex-disjoint selection. Each selected
face produces a deterministic inner face and surrounding ring on one candidate
mesh. Shared-vertex, duplicate, invalid, and capacity-invalid selections fail
without changing the committed topology or selection state.

Face bevel supports one or a bounded vertex-disjoint selection of simple planar
faces. Each selected face produces a deterministic planar bevel ring on one
candidate mesh, and Apply selects the resulting bevel faces in Face mode.
Shared-vertex, duplicate, invalid, and capacity-invalid selections fail without
changing the committed topology or selection state.

Face subdivision supports one or a bounded vertex-disjoint selection. Each
selected face produces a center vertex, edge midpoints, and a quad fan on one
candidate mesh. Apply selects the resulting center vertices in Vertex mode.
Shared-vertex, duplicate, invalid, and capacity-invalid selections fail
without changing the committed topology or selection state.

Face poke supports one or a bounded selection of simple convex planar polygons.
Selected polygons may be disconnected or may share complete existing edges. The
operation adds one center vertex for each selected face and replaces each
polygon with a triangle fan on one candidate mesh while preserving the source
face identity, material region, smoothing, and per-corner UV data. Apply selects
the resulting center vertices in input selection order in Vertex mode.
Duplicate, vertex-only shared contact, non-planar, concave, self-intersecting,
degenerate, invalid, and capacity-invalid selections fail without changing the
committed topology or selection state.

## UV operations

`<henka/authoring_uv.h>` currently provides:

- per-face planar projection on each principal axis;
- bounded per-face UV transforms;
- single-face packing helpers;
- bounded scaling and packing of the complete UV island containing a selected face;
- deterministic bounded packing of every UV island into the unit square;
- deterministic planar-chart unwrap for connected planar UV islands;
- deterministic spherical unwrap for bounded ellipsoidal or spherical surfaces
  around the selected X, Y, or Z axis, with centered pole UVs and generated
  longitude-wrap seams;
- finite-value validation;
- seam detection from shared topology and explicit edge seam metadata.

The shared Sandbox modeling session routes one selected face through
transactional UV projection, uniform scaling, and padded unit-square packing.
The island operations affect faces connected through non-seam edges while
preserving existing UV seams as boundaries. The all-islands operation uses a
deterministic bounded grid and preserves each island's relative UV proportions.
Each operation supports preview, Apply, Cancel, and the existing authoring
undo/redo history; UV state persists through the HAMS source path.

In Edge mode, the Sandbox exposes a transactional Toggle UV Seam operation for
the selected edges. It supports preview, Cancel, Apply, and authoring undo/redo;
the explicit seam state is persisted through HAMS v6. The Sandbox Face-mode
panel also provides transactional planar-chart unwrap. It projects each
seam-delimited planar island on its dominant geometric axis and packs the charts
into a padded unit-square grid. Degenerate or non-planar islands are rejected
without changing the source. Spherical unwrap maps normalized longitude and
  latitude for bounded non-degenerate ellipsoidal or spherical surfaces, uses a
  stable centered U value at poles, and marks the generated longitude boundary as
  a seam. Other automatic unwrap strategies remain outside this bounded scope.

## Connected Sandbox workflow

The mesh, modeling, UV, history, evaluation, and file APIs share one bounded representation.

The Sandbox exercises the first horizontal editor connection through the selected Textured Cube and Add Cube results. Each object owns:

- a bounded authoring box;
- independent history;
- evaluated renderer data;
- local bounds;
- object-specific authoring state.

The viewport ray picker resolves hits to authored component identities.

Imported entities are not automatically authoring-enabled. The current Sandbox bridge clones validated authoring sources into independent per-entity authoring state. Duplicating an authored object produces an independent editable source. Selecting another entity activates that entity's wrapper.

For objects using the Sandbox box-collider contract, a duplicate receives a separate bounded collider. The duplicate collider is retired with its object. The source body remains owned by its original descriptor.

## Selection and transforms

Object Details Authoring exposes bounded Vertex, Edge, and Face modes.

### Selection behavior

- `Ctrl`-click adds components to the active mode.
- Scene View drag performs bounded box selection against projected source components.
- Replace, Ctrl-add, and Shift-subtract operations commit atomically.
- Normal mode accepts front-facing components proven frontmost by a source-mesh ray.
- X-Ray keeps the front-facing policy and permits selection through occluding mesh surfaces.
- Renderer triangulation never appears as authored edges.
- The Scene View reports the active topology mode and selected-component count.
- Select All, Select None, Invert, and Shrink use deterministic sorted component IDs.
- Failed replacement allocation preserves the prior selection.

Current selected-component visualization uses:

- amber crosses for vertices;
- cyan segments with endpoint markers for edges;
- orange borders with center markers for faces;
- a stronger mode-specific marker for the active edit target.

### Translation and shaping

The Sandbox exposes bounded Move X+, Move Y+, and Move Z+ operations through cloned-mesh publication.

Face mode also provides Normal + and Normal -. These operations move the active face's shared vertices along its evaluated local-space normal through the same transactional source, render, bounds, and collider path.

Malformed or degenerate faces and distances outside the bounded editor range are rejected.

### Selection growth

Face mode provides:

- Grow Selection for one topology-adjacent ring;
- Select Connected for the complete reachable component within the bounded selection budget.

### Pivot and orientation policy

Rotate Selected and Scale Selected expose:

- median pivot;
- active-component pivot;
- per-face individual pivot;
- world orientation;
- local orientation;
- face-normal orientation for rotation.

Sandbox controls currently use bounded local median transforms. The public authoring API exposes the other policies.

### Soft movement

Soft Move X+, Soft Move Y+, and Soft Move Z+ use a bounded one-ring linear falloff:

- selected vertices receive full translation;
- directly adjacent vertices receive half translation.

This is a generic authoring operation and an early shaping foundation.

## Persistence and history

Face mode exposes:

- material-region editing;
- Flip;
- Extrude;
- Inset;
- Bevel;
- Subdivide;
- Poke Face(s);
- Project UV;
- Pack UV;
- Undo;
- Redo;
- Save Project;
- Reload Project.

Every successful edit follows the same publication sequence:

1. Build a candidate source.
2. Validate the candidate.
3. Evaluate renderer geometry.
4. Create a normal renderer mesh.
5. Update scene-entity mesh and local bounds.
6. Update the bound box collider when present.
7. Checkpoint history.

Any evaluation, renderer, scene, bounds, history, or file-parse failure preserves the prior source, renderer mesh, bounds, and linked collider state.

### Per-object authoring persistence

Save Project writes a bounded versioned manifest beside the transactional `.hams` source. The manifest stores:

- source path;
- transform;
- visibility.

Save/reload controls use a confined engine-owned user-data slot derived from the selected entity identity. Selecting or duplicating one authored object cannot overwrite another object's authoring source.

This persistence currently operates per authored object. Complete scene/project serialization remains unfinished.

### Selection history

The authoring bridge stores one bounded selected-face identity beside each mesh-history snapshot. Face-backed and loose-edge splits additionally store their component-mode selection snapshots so the original edge and the two replacement edges return through undo/redo.

- topology operations select their deterministic result;
- undo/redo restores the matching prior or next face when it still exists;
- a new edit after undo truncates topology and selection history together;
- Reload resets selection history to the validated replacement source.

A missing or malformed project manifest, source, or transform preserves the current scene mesh, bounds, transform, visibility, and authoring history.

When the bounded authoring wrapper closes, it restores the mesh and local bounds owned by the entity before authoring took ownership when no other editor path has replaced the active evaluated mesh.

Scene selection remains the generation-checked scene-entity authority.

## Vertex modeling

### Merge

Vertex merge is available through:

- `henka_authoring_mesh_merge_vertices`;
- `henka_authoring_mesh_merge_vertices_by_distance`.

Center and active-vertex modes use deterministic stable-ID selection, preserve per-face corner UVs and face metadata, reconcile active endpoint-pair edges, and never reuse retired logical IDs.

Distance merge uses:

- a finite positive tolerance;
- deterministic stable-ID union-find clustering;
- a bounded spatial hash;
- double-precision cluster means.

A no-op distance merge returns success without changing topology or history. The Sandbox stores merge distance as transient per-object UI state. It is not serialized into HAMS or project manifests.

### Dissolve, delete, and connect

Bounded core and Sandbox Vertex Modeling paths provide:

- vertex dissolve;
- vertex delete;
- vertex connect.

Dissolve supports boundary corner removal and unambiguous manifold triangle fans. It rejects ambiguous, hard-edge, UV-seamed, non-triangle, and non-manifold cases.

Delete removes selected vertices and their incident faces, then removes only newly orphaned vertices in the affected neighborhood.

Connect splits one face between two non-adjacent corners. The original face ID is preserved. The new face receives a fresh logical ID in a reusable physical slot. The shared Sandbox modeling-operator session exposes this bounded vertex selection through Preview, Cancel, Apply, and the existing undo/redo boundary without publishing the preview into the committed source.

### Rip face corners

`henka_authoring_mesh_rip_vertex_face` separates exactly one incident face
corner from a selected surface vertex. The operation duplicates the selected
vertex, reassigns the lowest logical-ID incident face to the duplicate, and
leaves the original vertex on the remaining faces. Vertex position, UV,
material, face smoothing, per-corner UVs, and the exposed hard/seam edge intent
are preserved. The candidate is validated before publication, so invalid,
loose, unsupported, or capacity-invalid requests leave the committed mesh
unchanged.

The Sandbox exposes this bounded operation as `Rip Face(s)` in Vertex mode. It
accepts one or more selected surface vertices when the lowest logical incident
face chosen for each vertex is pairwise vertex-disjoint. The shared
Preview/Cancel/Apply, selection, undo, and redo boundary is used for the whole
selection, and Apply selects all new duplicate vertices. The core API also
accepts explicit vertex/face pairs for callers that need deterministic face
selection. Overlapping or otherwise generalized seam-rip workflows remain
outside this bounded operation.

### Smooth Vertices / Relax

`henka_authoring_mesh_smooth_vertices` moves selected connected vertices toward
the simultaneous average of their current topological neighbors. The factor is
finite and bounded to `[0,1]`; factor `0` is an intentional no-op and factor
`1` reaches the neighbor average.

The operation evaluates every target from the unchanged source before changing
any position, then validates and publishes one candidate. It preserves stable
component identities, topology, per-corner UVs, material regions, smoothing,
and hard-edge/seam metadata. Duplicate or invalid selections and loose
vertices without incident edges fail closed without partial publication. The
Sandbox exposes the same operation through Vertex mode with Preview, Cancel,
Apply, and the existing direct-object undo/redo boundary.

### Vertex Extrude

Bounded Vertex Extrude supports a connected open boundary vertex fan, including the one-face corner case, and a compatible closed interior fan.

The operation:

- creates one offset cap vertex;
- replaces the incident fan;
- creates two boundary side faces;
- preserves per-face material and smoothing metadata for a closed-fan offset cap;
- publishes through the shared source/render/bounds/collider/undo transaction.

It rejects disconnected, loose-edge, and incompatible-normal fans.

Batch Vertex Extrude also supports pairwise fan-disjoint compatible closed
interior vertices. Each selected fan is evaluated from the original source,
then the complete batch is published as one candidate; the original selected
vertices remain valid loose vertices and each replacement cap preserves its
source face material, smoothing, and corner UV state. Duplicate selections,
overlapping fan neighborhoods, open or loose vertices, unsupported fans, and
capacity exhaustion fail without changing the committed source. The Sandbox
routes the batch through Preview, Cancel, Apply, and the existing Undo/Redo
history boundary.

### Vertex Bevel

Vertex Bevel is an atomic multi-selection operation. It uses a deterministic edge/end-point cut table and:

- rejects non-finite values;
- rejects zero and overlapping requests;
- rejects non-manifold input;
- rejects capacity-invalid requests;
- preserves per-corner UV interpolation;
- preserves original hard trimmed segments;
- creates same-material interior caps with deterministic planar UVs;
- leaves normal boundary vertices open.

Successful Sandbox bevels replace Vertex selection with live cut vertices and use the standard history/render/bounds/collider transaction. The modeling operator routes Vertex Bevel through Preview, Cancel, Apply, Undo, and Redo while restoring the live cut-vertex selection with the corresponding topology state.

## Edge modeling

### Dissolve and delete

Transactional dissolve is available for one compatible interior edge or a
bounded pairwise-disjoint selection of compatible interior edges. Selected
edges may not share endpoints or incident faces; hard, seamed, metadata-
discontinuous, and otherwise incompatible selections fail closed.

Transactional edge delete supports one face-backed edge, removing its incident
face set while preserving vertices, or a bounded pairwise-disjoint selection of
face-backed edges with disjoint endpoints and incident faces. Standalone wire
edges also support a bounded pairwise-disjoint batch that removes only those
edges while preserving their vertices. Mixed domains, overlapping face sets,
duplicate edges, and shared endpoints fail closed.

### Edge Bevel

Bounded edge bevel currently supports:

- one boundary edge whose endpoints belong to one face;
- a pairwise vertex-disjoint selection of boundary edges on distinct faces;
- same-face boundary batches with shared-endpoint corner caps;
- one compatible interior edge in an isolated two-quad patch;
- pairwise vertex-disjoint interior edges across isolated patches; and
- a bounded connected quad-strip selection with disjoint edge endpoints;
- one compatible three-edge branching interior fan around a valence-three
  vertex, with a generated center cap.

These forms share one selected-edge bevel contract and create interpolated cut vertices and quad bevel faces transactionally. The singular API remains as a compatibility wrapper.

Interior bevel rejects:

- hard edges;
- material discontinuities;
- smoothing discontinuities;
- UV discontinuities;
- non-quad faces;
- neighboring shared boundaries;
- ambiguous endpoint fans.

Boundary batch bevel rejects shared faces and unsupported endpoint sharing.
The branching interior form is limited to three compatible non-hard edges in
three matching quad faces around one valence-three vertex. It creates one
center cap and publishes only after the updated faces, bevel quads, cap,
metadata, UVs, and geometry validate together. Mixed selections, larger or
ambiguous branching domains, and broader interior edge-set bevel remain
incomplete.

### Surface-connected Edge Extrude

The core API supports bounded surface-connected extrusion for one open boundary edge, a contiguous boundary-edge chain, a batch of independent boundary-edge chains, a pairwise vertex-disjoint batch on distinct faces, one compatible interior edge shared by two quads, a pairwise-disjoint batch of compatible interior edges, a simple connected interior-edge path across compatible quad strips, a batch of independent simple connected interior-edge paths, and one compatible three-edge branching fan around a valence-three interior vertex.

The operation:

- offsets the edge along its incident face normal;
- replaces that edge in the source face;
- creates one connecting quad per selected edge;
- preserves selected hard-edge intent;
- publishes after topology and geometry validation.

Boundary chains are partitioned by endpoint connectivity. Each connected
component must belong to one boundary face and be an open chain or a simple
closed loop; independent components are applied together through one
candidate-first transaction and may belong to different faces.

For the compatible interior cases, the lower logical-ID incident quad is selected deterministically for each edge. That quad replaces the shared edge with an offset edge and receives one connecting quad; the neighboring quad remains unchanged. The source faces must be quads with matching material and smoothing metadata, and the source edges must not be hard or seamed. Per-corner UVs and face metadata are preserved on the selected quads and connecting quads. Pairwise-disjoint selections and independent simple paths publish all transactions through one candidate-first commit. Each connected path must be simple, pairwise vertex-disjoint, and have exactly two path endpoints; cyclic and unsupported branching selections are rejected as ambiguous. The supported three-edge branch fan is interpreted as its enclosed three-face region and uses the canonical face-region extrusion transaction, preserving the selected faces as the base when the region is closed and creating one translated cap plus boundary side faces.

Other interior/manifold configurations, mixed metadata, non-quad faces, and mixed, shared-endpoint, cyclic, larger branching, or otherwise unsupported batches remain unsupported. Disconnected selections are supported as independent compatible edges and simple connected paths; broader disconnected interior components remain unsupported.

The shared Sandbox modeling session exposes this path through Preview, Cancel,
and Apply for one edge, a contiguous chain, a bounded batch of independent
boundary chains, or a compatible interior path. The Authoring panel uses the
shared amount control, and applied edits use the existing Undo/Redo history
boundary.

### Boundary Edge Bridge

Edge mode can bridge either two distinct compatible boundary edges, two
disjoint equal-length compatible boundary-edge chains, or a bounded even
selection of independent compatible chain pairs. Both chains in each pair must
be open or both must be simple closed loops. The operation creates one
transactional quad per paired edge, uses deterministic endpoint pairing for
open chains or cyclic offset/direction pairing for closed loops, and preserves
source material, smoothing, and endpoint UV data. The Sandbox modeling session
exposes the operation through the Edge-mode
Preview/Apply/Cancel path and the authoritative source/render/history
transaction, with Undo and Redo for the applied edit.

### Boundary Loop Fill

Edge mode can fill one or more independent selected closed boundary edge loops
in one candidate-first transaction. Each selected edge must be a unique
face-backed boundary edge. The selected edges are partitioned into simple
closed cycles; shared-vertex, branched, mixed, duplicate, invalid, and
capacity-insufficient selections fail without publishing a partial result.
New faces inherit the source boundary metadata, and the Sandbox operator
selects all newly filled faces after Apply. Preview, Cancel, Apply, Undo, and
Redo use the shared authoring-object source/render/bounds/physics/history
boundary.

## Loop Cut and Edge Slide

### Single-quad Loop Cut

A bounded single-quad face loop cut:

- interpolates two opposite boundary edges;
- creates two quad faces;
- rejects shared-boundary faces that would create a T-junction in neighboring topology.

### Quad-strip traversal

The shared topology layer provides deterministic compatible quad-strip traversal for modeling operators. It records ordered face, entry-edge, and exit-edge identities.

Traversal can terminate at an open boundary or report a closed ring. It rejects:

- hard crossings;
- material discontinuities;
- smoothing discontinuities;
- UV discontinuities;
- non-quad crossings;
- non-manifold crossings;
- ambiguous crossings.

No partial traversal result is published.

The same topology layer orders connected selected edge chains and cycles deterministically for Edge Slide.

### Isolated multi-face Loop Cut

The core modeling API and Sandbox authoring path support a bounded batch Loop
Cut for a selected set of vertex-disjoint isolated quad faces. Each source
face keeps its identity and receives one fresh second-face identity at the
same validated edge factor. The operation is candidate-first and preserves
material, smoothing, and per-corner UV data through the normal source/render,
bounds, physics, and history transaction.

Duplicate faces, shared vertices, non-isolated faces, non-quad faces, invalid
factors, and insufficient capacity fail without publishing any part of the
batch. Preview, Apply, Cancel, Undo, and Redo use the same authoring-object
transaction boundary as the existing single-face and quad-strip operations.
When more than one face is selected in the Sandbox Authoring panel, the
factor-controlled Loop Cut control routes this bounded isolated-face batch
operation before attempting any connected quad-strip route.

### Factor-controlled and uniformly spaced Loop Cut

The editor Loop Cut operator accepts a validated user-entered factor and supports compatible open strips and closed rings.

The workflow provides:

- Preview/Refresh;
- Apply;
- Cancel.

Preview changes evaluated render state only. Apply publishes the complete candidate through the transactional authoring path.

The core API provides a bounded uniformly spaced multi-cut variant across a
compatible open quad strip or closed quad ring. The Sandbox authoring path can
choose the longest compatible strip from the selected quad, then routes the
candidate through Preview, Apply, Cancel, and the existing undo/redo history.
It creates quad faces only and preserves the source material, smoothing, and
per-corner UV state.

The core API and Sandbox authoring path also support a bounded batch of
pairwise-disjoint compatible quad strips. Multi-face selection supplies one
deterministic strip start per selected quad, and the complete batch remains
candidate-first: overlapping strips or unsupported selections fail without
publishing partial topology. Preview, Apply, Cancel, Undo, and Redo use the
same transaction boundary as the single-strip workflow.

Branching, overlapping, or ambiguous loop-cut networks and generalized split workflows beyond
the bounded face-backed boundary/interior batch and standalone loose-edge
operations remain unfinished.

### Edge Slide

Edge mode provides signed-factor Edge Slide for one or more pairwise
vertex-disjoint compatible open edge-loops or closed edge-cycles selected in
one transaction.

The modeling session supports:

- numeric factors in `(-1, 1)`;
- Preview;
- Cancel;
- one transactional Apply for the complete selection.

The operation moves every selected loop toward its deterministic adjacent side
while preserving topology and uses the shared source/render/bounds/collider/
undo publication path. Mixed, overlapping, boundary, hard/seamed, or
metadata-incompatible loop selections fail without publishing a partial move.

### Cylindrical UV unwrap

The core authoring-mesh API and Sandbox Authoring panel provide a bounded
cylindrical unwrap around the selected X, Y, or Z axis. The operation maps the
angular coordinate to U, the axial coordinate to V, applies the requested
unit-square padding, and marks the generated angular wrap boundary as a UV
seam. It is intended for non-degenerate cylindrical side surfaces; input with
no axial span or a vertex on the selected axis is rejected without changing
the committed mesh.

The operation is candidate-first and preserves topology, material regions,
smoothing, and existing seam state outside the generated wrap boundary. The
Sandbox routes it through Preview, Apply, Cancel, Undo, and Redo alongside the
existing planar and island UV operations.

## Loose components

The authoring representation supports explicit loose vertices and standalone wire edges with:

- stable logical IDs;
- bounded physical storage;
- deterministic endpoint ordering;
- HAMS v6 transactional persistence with explicit seam metadata.

A standalone edge connects two distinct active vertices. It has zero incident faces until a face consumes that endpoint pair. It can be removed explicitly while face-less.

### Loose-vertex Extrude

The core modeling API supports bounded explicit-direction loose-vertex extrusion.

It:

- preserves the source vertex;
- inherits UV/material metadata;
- creates one standalone wire edge to the new vertex.

It rejects zero directions, zero distances, connected vertices, invalid geometry, and capacity exhaustion.

### Loose-edge Extrude

The core modeling API supports bounded explicit-direction loose-edge extrusion for
one or a pairwise-disjoint batch of standalone wire edges.

It:

- creates one parallel edge and one quad face per selected source edge;
- inherits endpoint UV/material metadata;
- preserves source-edge hard intent.

The complete selection is evaluated against the original mesh and published as
one validated candidate. The Sandbox Edge-mode operator routes the operation
through Preview, Apply, Cancel, Undo, and Redo.

It rejects face-backed edges, mismatched endpoint materials, degenerate offsets, and capacity exhaustion.

### Loose-edge Split

The core modeling API and Sandbox Edge-mode path support splitting one selected
standalone wire edge, or a bounded pairwise-disjoint batch of standalone wire
edges, at each midpoint. Each new vertex interpolates endpoint UVs and inherits
the endpoint material region. Each pair of replacement edges preserves the
source hard-edge and seam intent. The operation rejects face-backed edges,
mismatched endpoint materials, shared endpoints, duplicate selections,
degenerate source edges, and capacity exhaustion without publishing a partial
mesh. The Sandbox selects all replacement edges and restores the prior edge
selection through undo/redo.

### Loose-edge Delete

The core modeling API, object-authoring route, and shared modeling operator
support deleting one or a bounded pairwise-disjoint selection of standalone wire
edges. The operation preserves every loose-edge vertex and its remaining
metadata, rejects face-backed, duplicate, shared-endpoint, invalid, or
capacity-exhausting selections, and publishes the candidate only after the full
deletion set validates. Preview, Apply, Cancel, undo, and redo use the same
source/render/history transaction as the existing face-backed edge delete path.

### Face-backed Edge Split

The core modeling API supports splitting one selected face-backed boundary or
interior edge at a factor strictly between zero and one, a bounded batch of
pairwise-disjoint face-backed edges, one contiguous same-face boundary chain,
or a bounded batch of independent boundary chains. The new vertex interpolates position and
per-corner UV state. Each incident face receives the new corner, and the two
replacement edges preserve the source hard-edge and seam intent. Boundary and
two-face interior edges are supported; non-manifold edges, incompatible
endpoint material regions, invalid factors, overlapping or duplicate batch
selections, and capacity exhaustion fail closed without publishing a partial
mesh.

The Sandbox Edge-mode operator exposes the same operation through Preview,
Apply, Cancel, and undo/redo. Preview leaves the committed source unchanged;
Apply selects the two replacement edges for each split. A selected contiguous
boundary chain may share endpoints when all edges belong to one source face;
independent boundary chains may be selected together when their components do
not share endpoints and each component belongs to one source face. Mixed-face
chains, branched components, duplicate edges, and ambiguous shared-endpoint
components are rejected. The existing pairwise-disjoint boundary or interior
batch path remains available for selections outside this boundary-chain form.

### Sandbox loose-component session

The shared Sandbox modeling-operator session previews, cancels, and applies
extrusion for exactly one selected face, loose vertex, or standalone edge.
The Authoring panel exposes the same bounded Preview/Apply/Cancel path with a
face-normal amount for faces or a numeric Y-axis amount for loose components.

Dedicated broader loose-component creation and generalized extrusion workflows remain unfinished.

## Evaluation and renderer handoff

The authoring mesh is the committed topology source. Evaluation produces renderer-consumable primitive data.

Current evaluation supports:

- face-backed triangle output;
- homogeneous wire-only output as bounded lines;
- isolated-vertex-only output as bounded points;
- mixed surface-plus-loose output as a bounded composite mesh with triangle, line, and point parts.

The Sandbox topology overlay presents committed source vertices and wire edges for inspection and selection and gives loose components a distinct visual treatment.

### Public authoring-to-render API

Client applications can call `henka_mesh_create_from_authoring_mesh` from `<henka/mesh.h>`.

The contract requires:

- caller-owned authoring source;
- an empty output slot at entry;
- bounded and checked counts and indices;
- an empty output slot after allocation or evaluation failure.

The renderer owns the resulting mesh resource. The function does not create material authority. glTF scene/material ownership remains in the existing asset path.

### Bounds

`henka_authoring_mesh_get_bounds` computes bounds from active source vertices. Consuming scenes can publish local bounds from the same geometry source.

### Tangents

The evaluator tangent field is transport metadata for the bounded authoring representation. The renderer derives and orthogonalizes a stable tangent at the authoring-to-render boundary when the basis is not authoritative.

This keeps shading basis generation in the shared renderer path while preserving authored topology and UV ownership.

## HAMS file format

`henka_authoring_mesh_save_file` writes HAMS v6 using explicit little-endian 32-bit integers and IEEE-754 float bit patterns.

Each save:

1. writes to a unique same-directory temporary path;
2. flushes the complete candidate;
3. atomically replaces the destination only after successful completion.

A failed or concurrent save preserves the prior valid source.

HAMS v5 is the first version whose validity contract includes loose vertices and zero-face wire edges.

HAMS v6 retains that loose-topology contract and adds one explicit seam byte to
each modern active-edge record. The seam state is independent of hard-edge
intent and is used as an island boundary by UV authoring and topology analysis.

### OBJ source export

`henka_authoring_mesh_save_obj` writes authored surface geometry and face-corner
UVs through an atomic temporary-file replacement. Standalone face-less edges
are emitted as OBJ `l` line records. Stable Henka IDs, material regions,
hard-edge flags, seam flags, smoothing metadata, and loose vertices without an
edge remain HAMS-only state. The Sandbox Object Details panel exposes separate
Export HAMS and Export OBJ actions; OBJ export does not change the canonical
HAMS source path.

The loader accepts:

- current HAMS v6;
- loose-topology HAMS v5, with explicit seam state defaulting to false;
- repository-supported surface-only HAMS v2;
- surface-only HAMS v3;
- surface-only HAMS v4.

A legacy v4 file containing a loose edge is rejected. Legacy files are validated and migrated in memory only. Automatic rewriting is not performed.

`henka_authoring_mesh_load_file_new` reads bounded capacities from the file header, validates them, creates the candidate, and leaves the output slot empty on failure.

## Material-region behavior

Material regions retain editable numeric metadata. Evaluated model-to-render upload retains the bounded minimum/maximum region range for diagnostics.

Multiple shared material-instance selection from authoring material regions is not implemented yet. The current material authority remains the existing asset/material system.

Additional connected work is still needed for:

- material-instance assignment across broader authoring cases;
- texture dependencies;
- general collision integration beyond the bound box contract;
- package ownership;
- broader topology-aware picking;
- showcase rebuild workflows through shared scene/asset-manager paths.

## Known limits

The current authoring mesh is a validated modeling foundation. Remaining work includes:

- broader non-manifold vertex-fan handling;
- incompatible-normal fan handling;
- generalized surface-connected Vertex/Edge Extrude beyond the bounded open-fan,
  closed-fan batch, boundary-edge, pairwise-disjoint compatible interior-edge,
  and compatible single-sided interior-edge cases;
- generalized closed-loop, branching, and broader weld/split/bridge workflows
  beyond the bounded face-backed boundary/interior batch and standalone loose-edge
  operations;
- connected, branching, and general loop-cut networks beyond the bounded
  isolated multi-face and pairwise-disjoint quad-strip operations;
- branching and broader interior edge-set bevel;
- broader hard-surface modeling profiles;
- broader automatic UV unwrap beyond the bounded planar-chart, cylindrical
  side-surface, and spherical/ellipsoidal surface projections;
- texture painting;
- broader material authoring beyond current bounded material-instance editing;
- full editor workflows for arbitrary authoring-file selection;
- complete scene/project serialization;
- source export beyond HAMS and bounded OBJ geometry/UV output;
- production showcase rebuild workflows;
- package-level authoring ownership completion.

The bounded fan extrusion remains limited to connected open fans and pairwise
fan-disjoint compatible closed interior fans. The loose-component,
boundary-edge, pairwise-disjoint compatible interior-edge, and compatible
single-sided interior-edge extrusion paths cover their documented domains only.
glTF and KTX2 material ownership continues
through the existing asset system.
