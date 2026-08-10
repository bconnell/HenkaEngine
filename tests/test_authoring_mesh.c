#include <math.h>
#include <stdio.h>

#include <henka/authoring_mesh.h>

static int fail(const char* message)
{
    fprintf(stderr, "authoring mesh test failed: %s\n", message);
    return 0;
}

static int test_topology_and_evaluation(void)
{
    henka_authoring_mesh_desc desc = henka_authoring_mesh_desc_default();
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertices[5];
    henka_authoring_vertex_id first_face[] = {1U, 2U, 3U, 4U};
    henka_authoring_vertex_id second_face[] = {1U, 4U, 5U};
    henka_authoring_mesh_counts counts;
    henka_authoring_render_vertex render_vertices[8];
    uint32_t indices[12];
    henka_authoring_render_data render = {render_vertices, 8U, 0U, indices, 12U, 0U};
    henka_authoring_face_id first_face_id;
    henka_authoring_face_id second_face_id;
    henka_authoring_edge_id shared_edge = HENKA_AUTHORING_INVALID_ID;
    size_t edge_index;
    int result = 0;

    desc.max_vertices = 8U;
    desc.max_edges = 12U;
    desc.max_faces = 4U;
    desc.max_face_corners = 4U;
    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        return fail("create");
    }
    if (henka_authoring_mesh_add_vertex(mesh, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 2U, &vertices[0]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){1.0f, 0.0f, 0.0f}, (henka_vec2){1.0f, 0.0f}, 2U, &vertices[1]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){1.0f, 1.0f, 0.0f}, (henka_vec2){1.0f, 1.0f}, 2U, &vertices[2]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){0.0f, 1.0f, 0.0f}, (henka_vec2){0.0f, 1.0f}, 2U, &vertices[3]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){-1.0f, 1.0f, 0.0f}, (henka_vec2){-1.0f, 1.0f}, 3U, &vertices[4]) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_add_face(mesh, first_face, 4U, 2U, true, &first_face_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_face(mesh, second_face, 3U, 3U, false, &second_face_id) != HENKA_SUCCESS ||
        first_face_id != 1U || second_face_id != 2U || !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    counts = henka_authoring_mesh_get_counts(mesh);
    if (counts.vertices != 5U || counts.edges != 6U || counts.faces != 2U ||
        henka_authoring_mesh_get_vertex_edge_count(mesh, 1U) != 3U)
    {
        goto cleanup;
    }
    for (edge_index = 1U; edge_index <= counts.edges; ++edge_index)
    {
        const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(mesh, (uint32_t)edge_index);
        if (edge != NULL && edge->vertices[0] == 1U && edge->vertices[1] == 4U)
        {
            shared_edge = edge->id;
        }
    }
    if (shared_edge == HENKA_AUTHORING_INVALID_ID ||
        henka_authoring_mesh_get_edge_face_count(mesh, shared_edge) != 2U ||
        henka_authoring_mesh_edge_is_boundary(mesh, shared_edge) ||
        henka_authoring_mesh_set_edge_hard(mesh, shared_edge, true) != HENKA_SUCCESS ||
        henka_authoring_mesh_evaluate(mesh, &render) != HENKA_SUCCESS ||
        render.vertex_count != 7U || render.index_count != 9U ||
        render.vertices[0].material_region != 2U || render.vertices[4].material_region != 3U ||
        !isfinite(render.vertices[0].normal.z) || fabsf(render.vertices[0].normal.z) < 0.9f)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("topology/evaluation");
}

static int test_rejection_and_tombstones(void)
{
    henka_authoring_mesh_desc desc = henka_authoring_mesh_desc_default();
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id ids[3];
    henka_authoring_vertex_id invalid_face[] = {1U, 1U, 2U};
    henka_authoring_vertex_id valid_face[] = {1U, 2U, 3U};
    henka_authoring_face_id face_id;
    henka_authoring_mesh_counts counts;
    int result = 0;

    desc.max_vertices = 3U;
    desc.max_edges = 3U;
    desc.max_faces = 1U;
    desc.max_face_corners = 3U;
    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U, &ids[0]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){1.0f, 0.0f, 0.0f}, (henka_vec2){1.0f, 0.0f}, 0U, &ids[1]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){0.0f, 1.0f, 0.0f}, (henka_vec2){0.0f, 1.0f}, 0U, &ids[2]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_face(mesh, invalid_face, 3U, 0U, false, &face_id) == HENKA_SUCCESS ||
        henka_authoring_mesh_get_counts(mesh).faces != 0U ||
        henka_authoring_mesh_add_face(mesh, valid_face, 3U, 0U, false, &face_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_remove_vertex(mesh, ids[0]) == HENKA_SUCCESS ||
        henka_authoring_mesh_remove_face(mesh, face_id) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh) || henka_authoring_mesh_get_vertex(mesh, ids[0]) == NULL)
    {
        goto cleanup;
    }
    counts = henka_authoring_mesh_get_counts(mesh);
    if (counts.faces != 0U || counts.edges != 0U ||
        henka_authoring_mesh_remove_vertex(mesh, ids[0]) != HENKA_SUCCESS ||
        henka_authoring_mesh_get_vertex(mesh, ids[0]) != NULL)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("rejection/tombstone");
}

int main(void)
{
    return test_topology_and_evaluation() && test_rejection_and_tombstones() ? 0 : 1;
}
