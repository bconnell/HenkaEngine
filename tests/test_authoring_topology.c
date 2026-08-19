#include <math.h>
#include <stdio.h>

#include <henka/authoring_mesh.h>
#include <henka/authoring_topology.h>

static int fail(const char* message)
{
    fprintf(stderr, "authoring topology test failed: %s\n", message);
    return 0;
}

static int edge_is_pair(
    const henka_authoring_cage_edge* edge,
    henka_authoring_vertex_id first,
    henka_authoring_vertex_id second)
{
    return edge != NULL &&
        ((edge->vertices[0] == first && edge->vertices[1] == second) ||
         (edge->vertices[0] == second && edge->vertices[1] == first));
}

static int test_quad_cage_and_quality(void)
{
    const henka_authoring_mesh_desc desc = {16U, 32U, 8U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertices[5];
    henka_authoring_vertex_id face_vertices[4];
    henka_authoring_face_id face_id;
    henka_authoring_topology_options options =
        henka_authoring_topology_options_default();
    henka_authoring_topology_report report;
    henka_authoring_cage_edge cage[8];
    size_t cage_count = 0U;
    henka_authoring_render_vertex render_vertices[8];
    uint32_t render_indices[12];
    henka_authoring_render_data render = {
        render_vertices,
        8U,
        0U,
        render_indices,
        12U,
        0U};
    size_t index;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        return fail("quad create");
    }

    if (henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_vec2){0.0f, 0.0f},
            0U,
            &vertices[0]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){1.0f, 0.0f, 0.0f},
            (henka_vec2){1.0f, 0.0f},
            0U,
            &vertices[1]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){1.0f, 1.0f, 0.0f},
            (henka_vec2){1.0f, 1.0f},
            0U,
            &vertices[2]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){0.0f, 1.0f, 0.0f},
            (henka_vec2){0.0f, 1.0f},
            0U,
            &vertices[3]) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    face_vertices[0] = vertices[0];
    face_vertices[1] = vertices[1];
    face_vertices[2] = vertices[2];
    face_vertices[3] = vertices[3];

    if (henka_authoring_mesh_add_face(
            mesh,
            face_vertices,
            4U,
            0U,
            true,
            &face_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    if (henka_authoring_topology_analyze(
            mesh,
            &options,
            &report) != HENKA_SUCCESS ||
        report.vertex_count != 4U ||
        report.edge_count != 4U ||
        report.face_count != 1U ||
        report.triangle_count != 0U ||
        report.quad_count != 1U ||
        report.ngon_count != 0U ||
        report.connected_component_count != 1U ||
        report.boundary_edge_count != 4U ||
        report.nonmanifold_edge_count != 0U ||
        report.isolated_vertex_count != 0U ||
        report.degenerate_face_count != 0U ||
        report.duplicate_face_count != 0U ||
        fabs(report.quad_face_ratio - 1.0) > 0.000001)
    {
        goto cleanup;
    }

    if (henka_authoring_topology_get_cage_edges(
            mesh,
            NULL,
            0U,
            &cage_count) != HENKA_SUCCESS ||
        cage_count != 4U ||
        henka_authoring_topology_get_cage_edges(
            mesh,
            cage,
            8U,
            &cage_count) != HENKA_SUCCESS ||
        cage_count != 4U)
    {
        goto cleanup;
    }

    for (index = 0U; index < cage_count; ++index)
    {
        if (edge_is_pair(
                &cage[index],
                vertices[0],
                vertices[2]) ||
            edge_is_pair(
                &cage[index],
                vertices[1],
                vertices[3]))
        {
            goto cleanup;
        }
    }

    if (henka_authoring_mesh_evaluate(
            mesh,
            &render) != HENKA_SUCCESS ||
        render.index_count != 6U)
    {
        goto cleanup;
    }

    if (henka_authoring_mesh_set_edge_hard(
            mesh,
            cage[0].edge_id,
            true) != HENKA_SUCCESS ||
        henka_authoring_topology_analyze(
            mesh,
            &options,
            &report) != HENKA_SUCCESS ||
        report.hard_edge_count != 1U)
    {
        goto cleanup;
    }

    if (henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_vec2){0.0f, 0.0f},
            0U,
            &vertices[4]) != HENKA_SUCCESS ||
        henka_authoring_topology_analyze(
            mesh,
            &options,
            &report) != HENKA_SUCCESS ||
        report.isolated_vertex_count != 1U ||
        report.connected_component_count != 2U ||
        report.coincident_vertex_pair_count < 1U)
    {
        goto cleanup;
    }

    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("quad cage/quality");
}

static int test_winding_and_uv_seam(void)
{
    const henka_authoring_mesh_desc desc = {8U, 16U, 4U, 4U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertices[4];
    henka_authoring_vertex_id first[3];
    henka_authoring_vertex_id second[3];
    henka_authoring_face_id first_face;
    henka_authoring_face_id second_face;
    henka_authoring_topology_options options =
        henka_authoring_topology_options_default();
    henka_authoring_topology_report report;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        return fail("winding create");
    }

    if (henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_vec2){0.0f, 0.0f},
            0U,
            &vertices[0]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){1.0f, 0.0f, 0.0f},
            (henka_vec2){1.0f, 0.0f},
            0U,
            &vertices[1]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){0.0f, 1.0f, 0.0f},
            (henka_vec2){0.0f, 1.0f},
            0U,
            &vertices[2]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){1.0f, 1.0f, 0.0f},
            (henka_vec2){1.0f, 1.0f},
            0U,
            &vertices[3]) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    first[0] = vertices[0];
    first[1] = vertices[1];
    first[2] = vertices[2];

    second[0] = vertices[1];
    second[1] = vertices[2];
    second[2] = vertices[3];

    if (henka_authoring_mesh_add_face(
            mesh,
            first,
            3U,
            0U,
            true,
            &first_face) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_face(
            mesh,
            second,
            3U,
            0U,
            true,
            &second_face) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    if (henka_authoring_mesh_set_face_corner_uv(
            mesh,
            second_face,
            0U,
            (henka_vec2){9.0f, 9.0f}) != HENKA_SUCCESS ||
        henka_authoring_topology_analyze(
            mesh,
            &options,
            &report) != HENKA_SUCCESS ||
        report.triangle_count != 2U ||
        report.boundary_edge_count != 4U ||
        report.inconsistent_winding_edge_count != 1U ||
        report.uv_seam_edge_count != 1U)
    {
        goto cleanup;
    }

    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("winding/uv seam");
}

static int test_profiles(void)
{
    const henka_authoring_topology_profile_guidance general =
        henka_authoring_topology_profile_get_guidance(
            HENKA_AUTHORING_TOPOLOGY_PROFILE_GENERAL);
    const henka_authoring_topology_profile_guidance organic =
        henka_authoring_topology_profile_get_guidance(
            HENKA_AUTHORING_TOPOLOGY_PROFILE_ORGANIC);
    const henka_authoring_topology_profile_guidance hard_surface =
        henka_authoring_topology_profile_get_guidance(
            HENKA_AUTHORING_TOPOLOGY_PROFILE_HARD_SURFACE);

    if (general.recommended_minimum_quad_ratio != 0.0 ||
        fabs(organic.recommended_minimum_quad_ratio - 0.85) > 0.000001 ||
        fabs(hard_surface.recommended_minimum_quad_ratio - 0.60) > 0.000001 ||
        !organic.prefer_manifold ||
        organic.allow_planar_ngons ||
        !hard_surface.allow_planar_ngons)
    {
        return fail("profile guidance");
    }

    return 1;
}

int main(void)
{
    if (!test_quad_cage_and_quality() ||
        !test_winding_and_uv_seam() ||
        !test_profiles())
    {
        return 1;
    }

    printf("authoring topology tests passed\n");
    return 0;
}