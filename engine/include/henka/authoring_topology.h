#ifndef HENKA_AUTHORING_TOPOLOGY_H
#define HENKA_AUTHORING_TOPOLOGY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/authoring_mesh.h>
#include <henka/result.h>

#define HENKA_AUTHORING_TOPOLOGY_POLICY_VERSION UINT32_C(1)

typedef enum henka_authoring_topology_profile
{
    HENKA_AUTHORING_TOPOLOGY_PROFILE_GENERAL = 0,
    HENKA_AUTHORING_TOPOLOGY_PROFILE_ORGANIC,
    HENKA_AUTHORING_TOPOLOGY_PROFILE_HARD_SURFACE,
    HENKA_AUTHORING_TOPOLOGY_PROFILE_COUNT
} henka_authoring_topology_profile;

typedef struct henka_authoring_topology_profile_guidance
{
    double recommended_minimum_quad_ratio;
    bool prefer_manifold;
    bool allow_planar_ngons;
} henka_authoring_topology_profile_guidance;

typedef struct henka_authoring_topology_options
{
    float coincident_vertex_tolerance;
    float uv_seam_tolerance;
    double degenerate_normal_epsilon;
} henka_authoring_topology_options;

typedef struct henka_authoring_topology_report
{
    size_t vertex_count;
    size_t edge_count;
    size_t face_count;

    size_t triangle_count;
    size_t quad_count;
    size_t ngon_count;
    double quad_face_ratio;
    size_t max_face_corners;

    size_t connected_component_count;
    size_t isolated_vertex_count;

    size_t boundary_edge_count;
    size_t nonmanifold_edge_count;
    size_t hard_edge_count;
    size_t uv_seam_edge_count;
    size_t inconsistent_winding_edge_count;

    size_t degenerate_face_count;
    size_t duplicate_face_count;
    size_t coincident_vertex_pair_count;

    size_t valence_two_or_less_vertex_count;
    size_t valence_three_vertex_count;
    size_t valence_four_vertex_count;
    size_t valence_five_plus_vertex_count;
    size_t max_vertex_valence;
} henka_authoring_topology_report;

typedef struct henka_authoring_topology_repair_options
{
    uint32_t policy_version;
    henka_authoring_topology_profile profile;
    henka_authoring_topology_options analysis;
    size_t max_passes;
    bool remove_isolated_vertices;
    bool remove_duplicate_faces;
    bool remove_degenerate_faces;
} henka_authoring_topology_repair_options;

typedef struct henka_authoring_topology_repair_report
{
    bool changed;
    size_t passes;
    size_t removed_isolated_vertices;
    size_t removed_duplicate_faces;
    size_t removed_degenerate_faces;
    henka_authoring_topology_report before;
    henka_authoring_topology_report after;
} henka_authoring_topology_repair_report;

typedef struct henka_authoring_cage_edge
{
    henka_authoring_edge_id edge_id;
    henka_authoring_vertex_id vertices[2];
    bool boundary;
    bool hard;
} henka_authoring_cage_edge;

typedef struct henka_authoring_quad_strip_step
{
    henka_authoring_face_id face_id;
    henka_authoring_edge_id entry_edge_id;
    henka_authoring_edge_id exit_edge_id;
} henka_authoring_quad_strip_step;

henka_authoring_topology_options
henka_authoring_topology_options_default(void);

henka_authoring_topology_profile_guidance
henka_authoring_topology_profile_get_guidance(
    henka_authoring_topology_profile profile);

henka_authoring_topology_repair_options
henka_authoring_topology_repair_options_default(void);

/* Performs non-destructive topology-quality analysis.
 *
 * Structural mesh validity remains the responsibility of
 * henka_authoring_mesh_validate().  This report intentionally measures
 * modeling quality separately from structural correctness.
 */
henka_result henka_authoring_topology_analyze(
    const henka_authoring_mesh* mesh,
    const henka_authoring_topology_options* options,
    henka_authoring_topology_report* out_report);

/* Applies only explicitly enabled, deterministic safe repairs to a candidate
 * mesh. The source mesh is replaced only after every pass and final analysis
 * succeeds. Repairs never weld vertices, rewrite winding, or cross material,
 * UV, smoothing, or hard-edge boundaries implicitly. */
henka_result henka_authoring_mesh_repair_topology(
    henka_authoring_mesh* mesh,
    const henka_authoring_topology_repair_options* options,
    henka_authoring_topology_repair_report* out_report);

/* Returns only authoritative authoring edges.
 *
 * Evaluator-generated triangle diagonals are intentionally absent.  Calling
 * with out_edges == NULL and capacity == 0 queries the required edge count.
 */
henka_result henka_authoring_topology_get_cage_edges(
    const henka_authoring_mesh* mesh,
    henka_authoring_cage_edge* out_edges,
    size_t capacity,
    size_t* out_count);

/* Walks one deterministic compatible quad strip from start_edge_id. A
 * boundary start walks to the opposite boundary; an interior start walks
 * until it closes back to the same face/edge. The walk rejects triangles,
 * n-gons, non-manifold branches, hard/shared-material/smoothing boundaries,
 * and UV seams rather than crossing unsupported topology. Query the required
 * step count with out_steps == NULL and capacity == 0 before allocating a
 * bounded result buffer. */
henka_result henka_authoring_topology_walk_quad_strip(
    const henka_authoring_mesh* mesh,
    henka_authoring_edge_id start_edge_id,
    henka_authoring_quad_strip_step* out_steps,
    size_t capacity,
    size_t* out_count,
    bool* out_closed);

/* Orders one connected edge chain or cycle deterministically. The selected
 * edges must be unique, have degree one or two within the selection, and
 * contain either zero or two degree-one endpoints. Boundary, manifold, and
 * loose edges remain distinct. This helper reports graph ordering only;
 * callers apply face and metadata compatibility policy. The output is ordered
 * from the lowest endpoint for an open chain, or from the lowest endpoint of
 * the lowest edge for a closed cycle. */
henka_result henka_authoring_topology_order_edge_loop(
    const henka_authoring_mesh* mesh,
    const henka_authoring_edge_id* edge_ids,
    size_t edge_count,
    henka_authoring_edge_id* out_edge_ids,
    size_t capacity,
    size_t* out_count,
    bool* out_closed);

#endif
