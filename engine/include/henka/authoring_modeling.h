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
henka_result henka_authoring_mesh_extrude_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    float distance,
    henka_authoring_face_id* out_face_id);
henka_result henka_authoring_mesh_inset_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    float factor,
    henka_authoring_face_id* out_face_id);
henka_result henka_authoring_mesh_bevel_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    float width,
    henka_authoring_face_id* out_face_id);
henka_result henka_authoring_mesh_subdivide_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id,
    henka_authoring_vertex_id* out_center_vertex_id);

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

henka_result henka_authoring_mesh_connect_vertices(
    henka_authoring_mesh* mesh,
    henka_authoring_vertex_id first_vertex_id,
    henka_authoring_vertex_id second_vertex_id,
    henka_authoring_face_id* out_new_face_id,
    henka_authoring_modeling_report* out_report);

/* Dissolves one compatible interior edge into its two adjacent face loops.
 * Boundary, hard, UV-seamed, material-discontinuous, and capacity-invalid
 * requests are rejected without changing the source mesh. */
henka_result henka_authoring_mesh_dissolve_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
    henka_authoring_modeling_report* out_report);

/* Deletes one selected edge and its incident face set, preserving vertices.
 * The operation rejects requests that would leave an empty or invalid source
 * mesh and never commits a partial candidate. */
henka_result henka_authoring_mesh_delete_edge(
    henka_authoring_mesh* mesh,
    henka_authoring_edge_id edge_id,
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
