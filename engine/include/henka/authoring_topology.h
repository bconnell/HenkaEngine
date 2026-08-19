#ifndef HENKA_AUTHORING_TOPOLOGY_H
#define HENKA_AUTHORING_TOPOLOGY_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/authoring_mesh.h>
#include <henka/result.h>

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

typedef struct henka_authoring_cage_edge
{
    henka_authoring_edge_id edge_id;
    henka_authoring_vertex_id vertices[2];
    bool boundary;
    bool hard;
} henka_authoring_cage_edge;

henka_authoring_topology_options
henka_authoring_topology_options_default(void);

henka_authoring_topology_profile_guidance
henka_authoring_topology_profile_get_guidance(
    henka_authoring_topology_profile profile);

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

#endif