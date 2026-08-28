# Authoring Mesh Foundation

Henka exposes a bounded polygonal authoring mesh through `<henka/authoring_mesh.h>`. This is the editor topology layer. The renderer consumes evaluated mesh data produced from this source.

> **Status:** Integrated authoring foundation. The mesh, topology, modeling, UV, history, persistence, evaluation, scene, renderer, bounds, and bounded collider paths share one transactional source workflow.

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
- face winding flip;
- face extrude;
- inset;
- planar bevel ring;
- face subdivision;
- bounded edge bevel;
- bounded loop-cut operations;
- bounded vertex and edge extrusion paths described below.

Each operation works on a clone and publishes only a validated result. Capacity, topology, geometry, or non-manifold rejection preserves the committed source.

Face flip preserves:

- logical face identity;
- vertex identities;
- edge identities;
- material metadata;
- smoothing metadata;
- per-corner UV correspondence.

The operation reverses only the ordered winding.

## UV operations

`<henka/authoring_uv.h>` currently provides:

- per-face planar projection on each principal axis;
- bounded island transforms;
- single-face packing helpers;
- finite-value validation;
- seam detection from shared topology.

Automatic multi-island unwrap, seam-editing UI, and global packing remain unfinished.

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

The authoring bridge stores one bounded selected-face identity beside each mesh-history snapshot.

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

Connect splits one face between two non-adjacent corners. The original face ID is preserved. The new face receives a fresh logical ID in a reusable physical slot.

### Vertex Extrude

Bounded Vertex Extrude supports a connected open boundary vertex fan, including the one-face corner case.

The operation:

- creates one offset cap vertex;
- replaces the incident fan;
- creates two boundary side faces;
- publishes through the shared source/render/bounds/collider/undo transaction.

It rejects closed, disconnected, loose-edge, and incompatible-normal fans.

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

Successful Sandbox bevels replace Vertex selection with live cut vertices and use the standard history/render/bounds/collider transaction.

## Edge modeling

### Dissolve and delete

Transactional single-edge dissolve is available for compatible interior edges.

Transactional single-edge delete removes the selected edge's incident face set while preserving vertices.

### Edge Bevel

Bounded edge bevel currently supports:

- one boundary edge whose endpoints belong to one face;
- a pairwise vertex-disjoint selection of boundary edges on distinct faces;
- same-face boundary batches with shared-endpoint corner caps;
- one compatible interior edge in an isolated two-quad patch.

These forms share one selected-edge bevel contract and create interpolated cut vertices and quad bevel faces transactionally. The singular API remains as a compatibility wrapper.

Interior bevel rejects:

- hard edges;
- material discontinuities;
- smoothing discontinuities;
- UV discontinuities;
- non-quad faces;
- neighboring shared boundaries;
- ambiguous endpoint fans.

Boundary batch bevel rejects shared faces and unsupported endpoint sharing. Mixed selections and broader interior edge-set bevel remain incomplete.

### Surface-connected Edge Extrude

The core API supports bounded surface-connected extrusion for one open boundary edge.

The operation:

- offsets the edge along its incident face normal;
- replaces that edge in the source face;
- creates one connecting quad;
- preserves selected hard-edge intent;
- publishes after topology and geometry validation.

Interior/manifold edges and broader edge-set extrusion remain unsupported.

The shared Sandbox modeling session exposes this path through Preview, Cancel, and Apply. The Authoring panel uses the shared amount control.

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

### Factor-controlled Loop Cut

The editor Loop Cut operator accepts a validated user-entered factor and supports compatible open strips and closed rings.

The workflow provides:

- Preview/Refresh;
- Apply;
- Cancel.

Preview changes evaluated render state only. Apply publishes the complete candidate through the transactional authoring path.

The core API and editor also provide a bounded uniformly spaced multi-cut variant for one isolated boundary-only quad. It creates quad faces only and participates in preview, apply, cancel, and undo.

Broader multi-cut spacing, interior cases, and general loop-cut networks remain unfinished.

### Edge Slide

Edge mode provides signed-factor Edge Slide for one compatible open edge-loop or closed edge-cycle selection.

The modeling session supports:

- numeric factors in `(-1, 1)`;
- Preview;
- Cancel;
- one transactional Apply.

The operation moves the loop toward deterministic adjacent sides while preserving topology and uses the shared source/render/bounds/collider/undo publication path.

## Loose components

The authoring representation supports explicit loose vertices and standalone wire edges with:

- stable logical IDs;
- bounded physical storage;
- deterministic endpoint ordering;
- HAMS v5 transactional persistence.

A standalone edge connects two distinct active vertices. It has zero incident faces until a face consumes that endpoint pair. It can be removed explicitly while face-less.

### Loose-vertex Extrude

The core modeling API supports bounded explicit-direction loose-vertex extrusion.

It:

- preserves the source vertex;
- inherits UV/material metadata;
- creates one standalone wire edge to the new vertex.

It rejects zero directions, zero distances, connected vertices, invalid geometry, and capacity exhaustion.

### Loose-edge Extrude

The core modeling API also supports bounded explicit-direction loose-edge extrusion.

It:

- creates a parallel edge;
- creates one quad face;
- inherits endpoint UV/material metadata;
- preserves source-edge hard intent.

It rejects face-backed edges, mismatched endpoint materials, degenerate offsets, and capacity exhaustion.

### Sandbox loose-component session

The shared Sandbox modeling-operator session previews, cancels, and applies explicit-axis extrusion for exactly one selected loose vertex or standalone edge. The Authoring panel exposes the same bounded Preview/Apply/Cancel path with a numeric Y-axis amount.

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

`henka_authoring_mesh_save_file` writes HAMS v5 using explicit little-endian 32-bit integers and IEEE-754 float bit patterns.

Each save:

1. writes to a unique same-directory temporary path;
2. flushes the complete candidate;
3. atomically replaces the destination only after successful completion.

A failed or concurrent save preserves the prior valid source.

HAMS v5 is the first version whose validity contract includes loose vertices and zero-face wire edges.

The loader accepts:

- current HAMS v5;
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
- generalized surface-connected Vertex/Edge Extrude;
- broader weld/split/bridge workflows;
- multi-face and general loop-cut networks;
- broader interior edge-set bevel;
- broader hard-surface modeling profiles;
- automatic multi-island UV unwrap;
- global UV packing;
- seam-editing UI;
- texture painting;
- broader material authoring beyond current bounded material-instance editing;
- full editor workflows for arbitrary authoring-file selection;
- complete scene/project serialization;
- broader source export;
- production showcase rebuild workflows;
- package-level authoring ownership completion.

The bounded fan extrusion remains limited to connected open fans. The loose-component and boundary-edge extrusion paths cover their documented domains only. glTF and KTX2 material ownership continues through the existing asset system.
