#ifndef HENKA_AUTHORING_MODELING_H
#define HENKA_AUTHORING_MODELING_H

#include <henka/authoring_mesh.h>

typedef enum henka_authoring_vertex_merge_mode
{
    HENKA_AUTHORING_VERTEX_MERGE_CENTER = 0,
    HENKA_AUTHORING_VERTEX_MERGE_ACTIVE
} henka_authoring_vertex_merge_mode;

/* Caller-owned summary of one successful modeling candidate publication. */
typedef struct henka_authoring_modeling_report
{
    bool changed;
    size_t created_vertices;
    size_t removed_vertices;
    size_t created_edges;
    size_t removed_edges;
    size_t created_faces;
    size_t removed_faces;
    henka_authoring_vertex_id primary_vertex_id;
    henka_authoring_edge_id primary_edge_id;
    henka_authoring_face_id primary_face_id;
} henka_authoring_modeling_report;

/* Bounded authoring constructors and topology tools. Every operation edits a
 * candidate mesh and commits only after the resulting topology validates. */
henka_result henka_authoring_mesh_create_plane(
    const henka_authoring_mesh_desc* desc,
    float width,
    float depth,
    henka_authoring_mesh** out_mesh);
henka_result henka_authoring_mesh_create_box(
    const henka_authoring_mesh_desc* desc,
    float width,
    float height,
    float depth,
    henka_authoring_mesh** out_mesh);
/* Creates closed bounded primitive sources centred at the origin. Segment
 * values are constrained to the authoring face-corner hard limit so caps and
 * temporary constructor storage never require unbounded allocation. */
henka_result henka_authoring_mesh_create_cylinder(
    const henka_authoring_mesh_desc* desc,
    float radius,
    float height,
    size_t segments,
    henka_authoring_mesh** out_mesh);
henka_result henka_authoring_mesh_create_cone(
    const henka_authoring_mesh_desc* desc,
    float radius,
    float height,
    size_t segments,
    henka_authoring_mesh** out_mesh);
henka_result henka_authoring_mesh_create_uv_sphere(
    const henka_authoring_mesh_desc* desc,
    float radius,
    size_t longitude_segments,
    size_t latitude_segments,
    henka_authoring_mesh** out_mesh);
/* Creates a closed cubed-sphere source with shared manifold vertices and only
 * four-sided faces. Subdivisions is the number of quads along each cube edge. */
henka_result henka_authoring_mesh_create_quad_sphere(
    const henka_authoring_mesh_desc* desc,
    float radius,
    size_t subdivisions,
    henka_authoring_mesh** out_mesh);

henka_result henka_authoring_mesh_duplicate_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    henka_vec3 offset,
    henka_authoring_face_id* out_face_id);
/* Reverses one face's winding while preserving its logical face identity,
 * vertex identity, material/smoothing metadata, and per-corner UV mapping.
 * The candidate is published only after the complete mesh validates. */
henka_result henka_authoring_mesh_flip_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id);
/* Reverses a bounded unique selection of face windings while preserving face,
 * vertex, material, smoothing, and per-corner UV metadata. Invalid or
 * duplicate selections fail without changing the source mesh. */
henka_result henka_authoring_mesh_flip_faces(
    henka_authoring_mesh* mesh,
    const henka_authoring_face_id* face_ids,
    size_t face_count,
    henka_authoring_modeling_report* out_report);
henka_result henka_authoring_mesh_extrude_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    float distance,
    henka_authoring_face_id* out_face_id);
/* Extrudes a deterministic selected face region along its averaged face
 * normal. Adjacent selected faces share one translated cap and do not create
 * an internal side wall. An isolated region retains its source faces as the
 * base; a region joined to unselected surface moves its source faces to the
 * translated cap. The candidate is published only after topology and
 * geometry validation, and duplicate/invalid selections fail closed. */
henka_result henka_authoring_mesh_extrude_face_region(
    henka_authoring_mesh* mesh,
    const henka_authoring_face_id* face_ids,
    size_t face_count,
    float distance,
    henka_authoring_modeling_report* out_report);
henka_result henka_authoring_mesh_inset_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    float factor,
    henka_authoring_face_id* out_face_id);
/* Insets a bounded unique selection of simple planar faces transactionally.
 * Selected faces must be vertex-disjoint so the result is independent of face
 * iteration order. Invalid, duplicate, shared-vertex, and capacity-invalid
 * selections fail without changing the source mesh. */
henka_result henka_authoring_mesh_inset_faces(
    henka_authoring_mesh* mesh,
    const henka_authoring_face_id* face_ids,
    size_t face_count,
    float factor,
    henka_authoring_face_id* out_face_ids,
    size_t out_face_capacity,
    size_t* out_face_count,
    henka_authoring_modeling_report* out_report);
henka_result henka_authoring_mesh_bevel_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    float width,
    henka_authoring_face_id* out_face_id);
/* Bevels a bounded unique selection of vertex-disjoint simple planar faces
 * transactionally. Output face IDs correspond to input order. */
henka_result henka_authoring_mesh_bevel_faces(
    henka_authoring_mesh* mesh,
    const henka_authoring_face_id* face_ids,
    size_t face_count,
    float width,
    henka_authoring_face_id* out_face_ids,
    size_t out_face_capacity,
    size_t* out_face_count,
    henka_authoring_modeling_report* out_report);
henka_result henka_authoring_mesh_subdivide_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    henka_authoring_vertex_id* out_center_vertex_id);
/* Subdivides a bounded unique selection of vertex-disjoint faces
 * transactionally. The output center IDs correspond to the input order. */
henka_result henka_authoring_mesh_subdivide_faces(
    henka_authoring_mesh* mesh,
    const henka_authoring_face_id* face_ids,
    size_t face_count,
    henka_authoring_vertex_id* out_center_vertex_ids,
    size_t out_center_vertex_capacity,
    size_t* out_center_vertex_count,
    henka_authoring_modeling_report* out_report);
/* Triangulates one simple planar polygon face with a deterministic ear-clipping
 * candidate. The source face identity is retained by the first triangle;
 * additional triangles receive fresh identities. Materials, smoothing, edge
 * metadata, and per-corner UVs are preserved. Invalid or non-planar polygons
 * fail closed without changing the source mesh. */
henka_result henka_authoring_mesh_triangulate_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    henka_authoring_modeling_report* out_report);
/* Triangulates a bounded unique selection of simple planar polygon faces
 * transactionally. The selected faces must be vertex-disjoint so the result
 * is independent of face iteration order. Invalid, duplicate, shared-vertex,
 * non-planar, and capacity-invalid selections fail without changing source. */
henka_result henka_authoring_mesh_triangulate_faces(
    henka_authoring_mesh* mesh,
    const henka_authoring_face_id* face_ids,
    size_t face_count,
    henka_authoring_modeling_report* out_report);

henka_result henka_authoring_mesh_merge_vertices(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    henka_authoring_vertex_merge_mode mode,
    henka_authoring_vertex_id active_vertex_id,
    henka_authoring_vertex_id* out_surviving_vertices,
    size_t survivor_capacity,
    size_t* out_survivor_count,
    henka_authoring_modeling_report* out_report);

henka_result henka_authoring_mesh_merge_vertices_by_distance(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    float tolerance,
    henka_authoring_vertex_id* out_surviving_vertices,
    size_t survivor_capacity,
    size_t* out_survivor_count,
    henka_authoring_modeling_report* out_report);

/* Smooths selected vertices toward the average position of their current
 * topological neighbors. All targets are calculated from the source before
 * any position is changed, so a multi-vertex selection is deterministic.
 * factor is constrained to [0,1]; topology, per-vertex UV/material metadata,
 * and all logical component identities are preserved. A loose vertex without
 * an incident edge is rejected. The candidate is published only after the
 * complete mesh validates. */
henka_result henka_authoring_mesh_smooth_vertices(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    float factor,
    henka_authoring_modeling_report* out_report);

henka_result henka_authoring_mesh_dissolve_vertices(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    henka_authoring_modeling_report* out_report);

henka_result henka_authoring_mesh_delete_vertices(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    henka_authoring_modeling_report* out_report);

/* Separates one selected surface vertex from one of its incident faces by
 * creating a colocated vertex for that face corner. The complete candidate is
 * validated before publication; the source vertex, face metadata, corner UVs,
 * and hard/seam intent on the separated boundary are preserved. A face that
 * does not contain the selected vertex, a loose vertex, and capacity failure
 * are rejected without changing the source mesh. */
henka_result henka_authoring_mesh_rip_vertex_face(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id vertex_id,
    henka_authoring_face_id face_id,
    henka_authoring_vertex_id* out_new_vertex_id,
    henka_authoring_modeling_report* out_report);

/* Separates a bounded pairwise vertex-disjoint set of selected surface
 * vertices from one deterministic incident face each. The selected face for
 * every vertex is supplied explicitly so callers can preserve an authored
 * face-selection decision; duplicate vertices, duplicate faces, overlapping
 * target faces, loose vertices, and capacity failure are rejected without
 * changing the source mesh. All new vertex IDs are returned only after the
 * complete candidate validates. */
henka_result henka_authoring_mesh_rip_vertex_faces(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    const henka_authoring_face_id* face_ids,
    size_t pair_count,
    henka_authoring_vertex_id* out_new_vertex_ids,
    size_t out_vertex_capacity,
    size_t* out_vertex_count,
    henka_authoring_modeling_report* out_report);

henka_result henka_authoring_mesh_connect_vertices(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id first_vertex_id,
    henka_authoring_vertex_id second_vertex_id,
    henka_authoring_face_id* out_new_face_id,
    henka_authoring_modeling_report* out_report);

/* Splits one isolated quad face into two quads at a bounded edge fraction.
 * All four source edges must be boundary edges so the operation cannot leave
 * a T-junction in neighboring topology. */
henka_result henka_authoring_mesh_loop_cut_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    size_t edge_offset,
    float factor,
    henka_authoring_face_id* out_new_face_id,
    henka_authoring_modeling_report* out_report);

/* Splits one boundary-only quad with a bounded set of uniformly spaced cuts
 * across opposite edges. The source face keeps its logical identity, each
 * additional strip receives a fresh face identity, and the candidate is
 * published only after topology and geometry validation succeeds. Shared
 * boundary faces, zero cuts, invalid capacities, and non-finite results are
 * rejected without changing the source mesh. */
henka_result henka_authoring_mesh_loop_cut_face_multi(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    size_t edge_offset,
    size_t cut_count,
    henka_authoring_face_id* out_last_face_id,
    henka_authoring_modeling_report* out_report);

/* Splits a bounded vertex-disjoint selection of isolated quad faces at the
 * same edge fraction. Each selected face keeps its logical identity and the
 * corresponding output entry receives the fresh second-face identity. The
 * complete batch is candidate-first; duplicate, shared-vertex, non-isolated,
 * invalid, and capacity-invalid selections fail without changing the source
 * mesh. */
henka_result henka_authoring_mesh_loop_cut_faces(
    henka_authoring_mesh* mesh,
    const henka_authoring_face_id* face_ids,
    size_t face_count,
    size_t edge_offset,
    float factor,
    henka_authoring_face_id* out_new_face_ids,
    size_t out_face_capacity,
    size_t* out_face_count,
    henka_authoring_modeling_report* out_report);

/* Splits every compatible quad in the ordered strip. Open strips start at a
 * boundary edge; closed rings use one cut vertex per traversed ring edge.
 * Both forms create two quads per source face and publish one transactional
 * candidate. Unsupported branches and metadata seams are rejected. */
henka_result henka_authoring_mesh_loop_cut_quad_strip(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id start_edge_id,
    float factor,
    henka_authoring_face_id* out_new_face_id,
    henka_authoring_edge_id* out_primary_cut_edge_id,
    bool* out_closed,
    henka_authoring_modeling_report* out_report);

/* Splits every compatible quad in the ordered strip with uniformly spaced
 * cuts. Open strips start at a boundary edge; closed rings use one cut vertex
 * per traversed ring edge for each cut. The operation is one transactional
 * candidate and rejects zero cuts, unsupported branches, metadata seams, and
 * capacity-invalid requests without changing the source mesh. */
henka_result henka_authoring_mesh_loop_cut_quad_strip_multi(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id start_edge_id,
    size_t cut_count,
    henka_authoring_face_id* out_last_face_id,
    henka_authoring_edge_id* out_primary_cut_edge_id,
    bool* out_closed,
    henka_authoring_modeling_report* out_report);

/* Splits a bounded batch of pairwise-disjoint compatible quad strips with
 * uniformly spaced cuts. Each start edge identifies one open strip or closed
 * ring; strips may not share faces or vertices. The complete batch is
 * candidate-first and publishes only after every strip validates. */
henka_result henka_authoring_mesh_loop_cut_quad_strips_multi(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* start_edge_ids,
    size_t strip_count,
    size_t cut_count,
    henka_authoring_face_id* out_last_face_ids,
    henka_authoring_edge_id* out_primary_cut_edge_ids,
    bool* out_closed,
    henka_authoring_modeling_report* out_report);

/* Extrudes one connected compatible vertex fan transactionally. For an open
 * boundary fan, the original vertex remains as the base, one offset vertex
 * replaces the selected fan corner, and two boundary side faces are created.
 * For a compatible closed interior fan, the incident fan is replaced by an
 * offset cap and the original vertex remains as a valid loose vertex; each
 * replacement face retains its source material and smoothing metadata. This
 * avoids publishing a non-manifold single-vertex side wall. A single-face
 * corner is the smallest supported open fan. Disconnected, loose-edge, and
 * incompatible-normal fans fail closed. */
henka_result henka_authoring_mesh_extrude_vertex(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id vertex_id,
    float distance,
    henka_authoring_vertex_id* out_new_vertex_id,
    henka_authoring_modeling_report* out_report);

/* Extrudes a pairwise fan-disjoint set of surface vertices whose incident
 * topology is compatible with the boundary-fan vertex operation. Each
 * selected vertex is evaluated against the original mesh, then the complete
 * batch is published transactionally. Loose vertices, duplicate selections,
 * overlapping face/edge neighborhoods, unsupported vertex fans, invalid
 * distances, and capacity failures are rejected without changing the source
 * mesh. */
henka_result henka_authoring_mesh_extrude_boundary_vertices(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    float distance,
    henka_authoring_modeling_report* out_report);

/* Extrudes a pairwise fan-disjoint set of compatible closed interior vertex
 * fans. Each selected source vertex remains as a valid loose vertex while its
 * incident fan is replaced by a metadata-preserving offset cap. The complete
 * batch is evaluated against the original mesh and published transactionally;
 * open, loose, duplicate, overlapping, incompatible, and capacity-invalid
 * selections fail without changing the source mesh. */
henka_result henka_authoring_mesh_extrude_interior_vertices(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    float distance,
    henka_authoring_modeling_report* out_report);

/* Extrudes one loose vertex along an explicit direction into a standalone
 * wire edge. The source vertex remains in place, the new vertex inherits its
 * UV/material metadata, and no face is synthesized without surface context.
 * The operation is transactional and rejects connected vertices, zero
 * directions, zero distances, and capacity exhaustion. */
henka_result henka_authoring_mesh_extrude_loose_vertex(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id vertex_id,
    henka_vec3 direction,
    float distance,
    henka_authoring_vertex_id* out_new_vertex_id,
    henka_authoring_edge_id* out_new_edge_id,
    henka_authoring_modeling_report* out_report);

/* Extrudes a pairwise distinct selection of loose vertices along one explicit
 * direction. Each source vertex remains in place and receives one standalone
 * wire edge to its metadata-inheriting offset vertex. The whole batch is
 * transactional: invalid, connected, duplicate, or capacity-invalid input
 * is rejected without changing the source mesh. */
henka_result henka_authoring_mesh_extrude_loose_vertices(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    henka_vec3 direction,
    float distance,
    henka_authoring_modeling_report* out_report);

/* Extrudes one loose edge along an explicit direction into a parallel edge
 * and one quad face. The source endpoints remain in place and their
 * UV/material metadata is inherited by the new endpoints and face. The
 * operation is transactional and rejects face-backed edges, mismatched
 * endpoint material regions, degenerate offsets, and capacity exhaustion. */
henka_result henka_authoring_mesh_extrude_loose_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    henka_vec3 direction,
    float distance,
    henka_authoring_edge_id* out_new_edge_id,
    henka_authoring_face_id* out_new_face_id,
    henka_authoring_modeling_report* out_report);

/* Extrudes a bounded pairwise-disjoint selection of standalone wire edges
 * along one explicit direction. Each source edge remains in place and gains
 * one parallel edge plus one quad face. The whole batch is candidate-first:
 * duplicate, shared-endpoint, face-backed, mismatched-material, degenerate,
 * and capacity-invalid input is rejected without changing the source mesh. */
henka_result henka_authoring_mesh_extrude_loose_edges(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    henka_vec3 direction,
    float distance,
    henka_authoring_modeling_report* out_report);

/* Splits one standalone wire edge at its midpoint. The source edge must have
 * no incident faces. The new midpoint inherits the endpoint material region
 * and interpolated UV; both replacement edges inherit hard-edge and seam
 * intent. The operation is candidate-first and rejects face-backed,
 * mismatched-material, degenerate, and capacity-invalid requests without
 * changing the source mesh. */
henka_result henka_authoring_mesh_split_loose_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    henka_authoring_vertex_id* out_split_vertex_id,
    henka_authoring_edge_id* out_first_edge_id,
    henka_authoring_edge_id* out_second_edge_id,
    henka_authoring_modeling_report* out_report);

/* Splits a bounded batch of pairwise-disjoint standalone wire edges at their
 * midpoints. Each midpoint inherits the endpoint material region and
 * interpolated UV; replacement edges inherit hard-edge and seam intent. The
 * candidate is published only after every split validates, and face-backed,
 * duplicate, shared-endpoint, mismatched-material, and capacity-invalid
 * requests fail without changing the source mesh. */
henka_result henka_authoring_mesh_split_loose_edges(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    henka_authoring_vertex_id* out_split_vertex_ids,
    henka_authoring_edge_id* out_first_edge_ids,
    henka_authoring_edge_id* out_second_edge_ids,
    henka_authoring_modeling_report* out_report);

/* Splits one face-backed boundary or interior edge at a factor strictly
 * between zero and one. The incident face loops gain one interpolated corner,
 * the new vertex receives interpolated position/UV data, and both replacement
 * edges preserve the source hard/seam intent. The candidate is published only
 * after the complete topology and geometry validate; unsupported non-manifold,
 * ambiguous, and capacity-invalid requests fail without source mutation. */
henka_result henka_authoring_mesh_split_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    float factor,
    henka_authoring_vertex_id* out_split_vertex_id,
    henka_authoring_edge_id* out_first_edge_id,
    henka_authoring_edge_id* out_second_edge_id,
    henka_authoring_modeling_report* out_report);

/* Splits a bounded batch of pairwise-disjoint face-backed edges. Each selected
 * edge must have one or two incident faces; selected edges may not share
 * endpoints or incident faces. The candidate is published only after every
 * split validates, and all output arrays have edge_count entries. */
henka_result henka_authoring_mesh_split_edges(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    float factor,
    henka_authoring_vertex_id* out_split_vertex_ids,
    henka_authoring_edge_id* out_first_edge_ids,
    henka_authoring_edge_id* out_second_edge_ids,
    henka_authoring_modeling_report* out_report);

/* Splits one simple contiguous boundary-edge chain belonging to one source
 * face. Adjacent selected edges may share endpoints; each output array has
 * edge_count entries and the complete chain remains candidate-first. Mixed
 * faces, interior edges, branches, duplicate edges, and disconnected chains
 * are rejected without changing the source mesh. */
henka_result henka_authoring_mesh_split_boundary_edge_chain(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    float factor,
    henka_authoring_vertex_id* out_split_vertex_ids,
    henka_authoring_edge_id* out_first_edge_ids,
    henka_authoring_edge_id* out_second_edge_ids,
    henka_authoring_modeling_report* out_report);

/* Splits a bounded batch of independent contiguous boundary-edge chains.
 * Each connected selected component must belong to one face, while separate
 * components may belong to the same or different faces. Shared-endpoint
 * components, mixed-face chains, interior edges, branches, duplicates, and
 * disconnected selections that cannot be partitioned into simple chains are
 * rejected without changing the source mesh. */
henka_result henka_authoring_mesh_split_boundary_edge_chains(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    float factor,
    henka_authoring_vertex_id* out_split_vertex_ids,
    henka_authoring_edge_id* out_first_edge_ids,
    henka_authoring_edge_id* out_second_edge_ids,
    henka_authoring_modeling_report* out_report);

/* Extrudes one compatible edge transactionally. An open boundary edge moves
 * along its incident face normal and creates one connecting quad. An interior
 * edge is supported as a single-sided operation for one same-material,
 * same-smoothing, non-seamed pair of quads: the deterministically selected
 * incident quad moves to the offset edge and one connecting quad preserves its
 * material, smoothing, and per-corner UV state. The neighboring quad remains
 * unchanged. The candidate is published only after bounded topology and
 * geometry validation; unsupported or capacity-invalid requests are rejected
 * without changing the source mesh. */
henka_result henka_authoring_mesh_extrude_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    float distance,
    henka_authoring_edge_id* out_new_edge_id,
    henka_authoring_face_id* out_new_face_id,
    henka_authoring_modeling_report* out_report);

/* Extrudes a selected set of compatible interior edges transactionally. The
 * bounded supported domains include independent compatible edges, one simple
 * connected quad-strip path, or a batch of independent simple quad-strip
 * paths. Selected edges are interior, non-hard, non-seamed, and share
 * material, smoothing, and UV continuity with their incident quads. The
 * domains also include one compatible three-edge branching fan around a
 * valence-three interior vertex; that selection is interpreted as the enclosed
 * three-face region through the canonical face-region transaction. The
 * candidate is published only after the complete set validates. Mixed,
 * cyclic, larger or ambiguous branching, incompatible, and capacity-invalid
 * selections fail without changing the source mesh. */
henka_result henka_authoring_mesh_extrude_interior_edges(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    float distance,
    henka_authoring_modeling_report* out_report);

/* Extrudes a pairwise vertex-disjoint set of open boundary edges on distinct
 * faces along each edge's incident face normal. Each selected edge creates
 * two vertices, three edges, and one connecting quad in one transaction. The
 * candidate is published only after validation; mixed, interior, duplicate,
 * shared-endpoint, same-face, invalid-distance, and capacity-invalid
 * selections are rejected without changing the source mesh. */
henka_result henka_authoring_mesh_extrude_boundary_edges(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    float distance,
    henka_authoring_modeling_report* out_report);

/* Extrudes one contiguous chain of boundary edges from one face along that
 * face's normal. The selected chain may wrap around the face boundary, and a
 * complete face boundary is accepted as a closed chain. The source face's
 * material, smoothing, and corner UV state are preserved; the operation adds
 * one connecting quad per selected edge and publishes only after candidate
 * validation. Disconnected, mixed-face, interior, duplicate, ambiguous, and
 * capacity-invalid selections fail without changing the source mesh. */
henka_result henka_authoring_mesh_extrude_boundary_edge_chain(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    float distance,
    henka_authoring_modeling_report* out_report);

/* Extrudes a bounded batch of independent contiguous boundary-edge chains.
 * Each connected selected component must belong to one face and may be open
 * or a simple closed boundary loop. Components must not share endpoints;
 * mixed-face chains, branches, interior edges, duplicates, and unsupported
 * topology fail without changing the source mesh. */
henka_result henka_authoring_mesh_extrude_boundary_edge_chains(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    float distance,
    henka_authoring_modeling_report* out_report);

/* Dissolves one compatible interior edge into its two adjacent face loops.
 * Boundary, hard, UV-seamed, material-discontinuous, and capacity-invalid
 * requests are rejected without changing the source mesh. */
henka_result henka_authoring_mesh_dissolve_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    henka_authoring_modeling_report* out_report);

/* Dissolves a bounded pairwise-disjoint set of compatible interior edges.
 * Selected edges must each have two compatible incident faces and may not
 * share endpoints or incident faces. Invalid, duplicate, overlapping,
 * hard/seamed, material-discontinuous, and capacity-invalid selections fail
 * without publishing a partial candidate. */
henka_result henka_authoring_mesh_dissolve_edges(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    henka_authoring_modeling_report* out_report);

/* Bridges two distinct boundary edges from different faces with one
 * deterministic quad. The source faces must agree on material and smoothing;
 * endpoint pairing follows the shorter geometric pairing and the new face is
 * oriented to the first source face. Existing edge hard/seam metadata and
 * per-corner UVs are preserved. Unsupported, degenerate, non-manifold, and
 * capacity-invalid requests fail without changing the source mesh. */
henka_result henka_authoring_mesh_bridge_boundary_edges(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id first_edge_id,
    henka_authoring_edge_id second_edge_id,
    henka_authoring_face_id* out_face_id,
    henka_authoring_modeling_report* out_report);

/* Bridges two distinct, equal-length simple boundary edge chains with one
 * deterministic quad per paired edge. The chains may both be open or both be
 * closed loops; they must be boundary-only, disjoint, hard/seam-free, and
 * have matching material and smoothing metadata along their source faces.
 * Open-chain pairing chooses the shorter geometric direction. Closed-loop
 * pairing chooses a deterministic cyclic offset and direction. Per-corner UVs
 * are preserved. Mixed open/closed, unequal or ambiguous chains, unsupported
 * metadata, degenerate geometry, and capacity failures are rejected without
 * changing the source mesh. */
henka_result henka_authoring_mesh_bridge_boundary_edge_chains(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* first_edge_ids,
    size_t first_edge_count,
    const henka_authoring_edge_id* second_edge_ids,
    size_t second_edge_count,
    henka_authoring_face_id* out_first_face_id,
    henka_authoring_modeling_report* out_report);

/* Bridges a bounded even selection of independent boundary-edge chains as
 * deterministic pairs. Each connected component must be a simple boundary
 * chain or loop; components are paired in first-seen input order and each
 * pair must have equal length and compatible material, smoothing, and UV
 * metadata. The complete candidate is published only after every pair
 * validates. Odd, branched, duplicate, mixed, incompatible, or
 * capacity-invalid selections fail without changing the source mesh. */
henka_result henka_authoring_mesh_bridge_boundary_edge_chain_pairs(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    henka_authoring_face_id* out_face_ids,
    size_t out_face_capacity,
    size_t* out_face_count,
    henka_authoring_modeling_report* out_report);

/* Fills one closed boundary edge loop with a deterministic polygon. The loop
 * must be simple, boundary-only, and fit the authoring face-corner limit;
 * material and smoothing metadata are inherited from its boundary faces and
 * per-corner UVs are copied from the first matching boundary corner. The
 * candidate is published only after topology and geometry validation. */
henka_result henka_authoring_mesh_fill_boundary_loop(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    henka_authoring_face_id* out_face_id,
    henka_authoring_modeling_report* out_report);
/* Fills a bounded selection of independent closed boundary edge loops in one
 * candidate-first transaction. Each selected edge must be a unique boundary
 * edge; components must be simple closed loops of at least three edges and
 * may not share vertices. Material, smoothing, and per-corner UV metadata are
 * inherited by each new face. Ambiguous, mixed, branched, or capacity-invalid
 * selections fail without changing the source mesh. */
henka_result henka_authoring_mesh_fill_boundary_loops(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    henka_authoring_face_id* out_face_ids,
    size_t out_face_capacity,
    size_t* out_face_count,
    henka_authoring_modeling_report* out_report);

/* Deletes one selected edge and its incident face set, preserving vertices.
 * The operation rejects requests that would leave an empty or invalid source
 * mesh and never commits a partial candidate. */
henka_result henka_authoring_mesh_delete_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    henka_authoring_modeling_report* out_report);

/* Removes a bounded selection of faces while preserving at least one
 * renderable face. Duplicate, invalid, and capacity-invalid selections fail
 * without changing the source mesh; the complete deletion publishes only
 * after the candidate validates. */
henka_result henka_authoring_mesh_delete_faces(
    henka_authoring_mesh* mesh,
    const henka_authoring_face_id* face_ids,
    size_t face_count,
    henka_authoring_modeling_report* out_report);

/* Removes a bounded pairwise-disjoint set of face-backed edges and their
 * incident face sets while preserving vertices. Selected edges may not share
 * endpoints or incident faces, and at least one renderable face must remain.
 * Invalid, duplicate, overlapping, and capacity-exhausting requests fail
 * without publishing a partial candidate. */
henka_result henka_authoring_mesh_delete_face_edges(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    henka_authoring_modeling_report* out_report);

/* Removes a bounded pairwise-disjoint set of standalone wire edges while
 * preserving their vertices. Face-backed, duplicate, shared-endpoint, and
 * invalid selections fail without changing the source mesh. The candidate is
 * published only after the complete deletion set validates. */
henka_result henka_authoring_mesh_delete_loose_edges(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    henka_authoring_modeling_report* out_report);

/* Compatibility wrapper for the selected-edge bevel operation. It supports
 * one compatible boundary edge, or one isolated two-quad interior edge. */
henka_result henka_authoring_mesh_bevel_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    float width,
    henka_authoring_modeling_report* out_report);

/* Bevels a bounded selected edge set in one transaction. One compatible
 * interior edge, pairwise vertex-disjoint interior edges from isolated
 * two-quad patches, a bounded connected quad-strip selection, and one
 * compatible three-edge branching fan around a valence-three interior vertex
 * are supported. The branching fan creates a validated center cap.
 * Boundary selections whose endpoints have one incident face are also
 * supported; boundary selections may be pairwise vertex-disjoint across
 * distinct faces or may belong to one face. The same-face path creates a
 * bounded inset center, side quads, and corner caps for selected shared
 * endpoints. Mixed interior/boundary selections, unsupported branching or
 * connected interior domains, invalid widths, and capacity failures are
 * rejected without changing the source mesh. */
henka_result henka_authoring_mesh_bevel_edges(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    float width,
    henka_authoring_modeling_report* out_report);

/* Slides one connected, compatible quad edge loop toward either adjacent side.
 * The selected edges may form an open chain or a closed cycle of two-sided
 * quads with matching material, smoothing, and UV continuity. factor is
 * bounded to the open interval (-1, 1); zero is a validated no-op and
 * positive/negative values move toward deterministic opposite sides. The
 * candidate is published only after topology and geometry validation succeeds. */
henka_result henka_authoring_mesh_slide_edge_loop(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    float factor,
    henka_authoring_modeling_report* out_report);

/* Slides a bounded selection of pairwise vertex-disjoint compatible quad edge
 * loops in one transaction. Each connected component must be an open chain or
 * closed cycle of two-sided quads with matching material, smoothing, and UV
 * continuity. All loops use the same signed factor and the candidate is
 * published only after every component validates. Mixed, overlapping,
 * boundary, hard/seamed, incompatible, and capacity-invalid selections fail
 * without changing the source mesh. */
henka_result henka_authoring_mesh_slide_edge_loops(
    henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    float factor,
    henka_authoring_modeling_report* out_report);

henka_result henka_authoring_mesh_bevel_vertices(
    henka_authoring_mesh* mesh,
    const henka_authoring_vertex_id* vertex_ids,
    size_t vertex_count,
    float width,
    henka_authoring_vertex_id* out_result_vertices,
    size_t result_vertex_capacity,
    size_t* out_result_vertex_count,
    henka_authoring_modeling_report* out_report);

#endif
