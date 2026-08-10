#include <math.h>
#include <stdio.h>

#include <henka/authoring_mesh.h>
#include <henka/authoring_modeling.h>
#include <henka/authoring_uv.h>

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
        henka_authoring_mesh_set_face_corner_uv(mesh, first_face_id, 0U, (henka_vec2){2.0f, 3.0f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_evaluate(mesh, &render) != HENKA_SUCCESS ||
        render.vertex_count != 7U || render.index_count != 9U ||
        render.vertices[0].material_region != 2U || render.vertices[4].material_region != 3U ||
        fabsf(render.vertices[0].uv.x - 2.0f) > 0.0001f ||
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

static int test_history_and_persistence(void)
{
    const char* path = "authoring_mesh_checkpoint.bin";
    henka_authoring_mesh_desc desc = {3U, 3U, 1U, 3U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_mesh_history* history = NULL;
    henka_authoring_vertex_id ids[3];
    henka_authoring_vertex_id face[] = {1U, 2U, 3U};
    henka_authoring_face_id face_id;
    const henka_authoring_vertex* vertex;
    FILE* corrupt;
    int corrupt_ok;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U, &ids[0]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){1.0f, 0.0f, 0.0f}, (henka_vec2){1.0f, 0.0f}, 0U, &ids[1]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){0.0f, 1.0f, 0.0f}, (henka_vec2){0.0f, 1.0f}, 0U, &ids[2]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_face(mesh, face, 3U, 0U, false, &face_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_history_create(mesh, 4U, &history) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_set_vertex_uv(mesh, ids[0], (henka_vec2){0.5f, 0.5f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_history_checkpoint(history, mesh) != HENKA_SUCCESS ||
        !henka_authoring_mesh_history_can_undo(history) ||
        henka_authoring_mesh_history_undo(history, mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_history_can_undo(history) ||
        henka_authoring_mesh_history_redo(history, mesh) != HENKA_SUCCESS ||
        !henka_authoring_mesh_history_can_undo(history))
    {
        goto cleanup;
    }
    vertex = henka_authoring_mesh_get_vertex(mesh, ids[0]);
    if (vertex == NULL || fabsf(vertex->uv.x - 0.5f) > 0.0001f ||
        henka_authoring_mesh_set_face_corner_uv(mesh, face_id, 0U, (henka_vec2){4.0f, 5.0f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_save_file(mesh, path) != HENKA_SUCCESS ||
        henka_authoring_mesh_set_vertex_uv(mesh, ids[0], (henka_vec2){9.0f, 9.0f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_load_file(mesh, path) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    vertex = henka_authoring_mesh_get_vertex(mesh, ids[0]);
    {
        henka_vec2 corner_uv;
        if (vertex == NULL || fabsf(vertex->uv.x - 0.5f) > 0.0001f ||
            henka_authoring_mesh_get_face_corner_uv(mesh, face_id, 0U, &corner_uv) != HENKA_SUCCESS ||
            fabsf(corner_uv.x - 4.0f) > 0.0001f)
        {
            goto cleanup;
        }
    }
    corrupt = NULL;
    if (fopen_s(&corrupt, path, "wb") != 0)
    {
        corrupt = NULL;
    }
    corrupt_ok = corrupt != NULL && fwrite("bad", 3U, 1U, corrupt) == 1U;
    if (corrupt != NULL && fclose(corrupt) != 0)
    {
        corrupt_ok = 0;
    }
    if (!corrupt_ok)
    {
        if (corrupt != NULL)
        {
            fclose(corrupt);
        }
        goto cleanup;
    }
    if (henka_authoring_mesh_set_vertex_uv(mesh, ids[0], (henka_vec2){8.0f, 8.0f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_load_file(mesh, path) == HENKA_SUCCESS)
    {
        goto cleanup;
    }
    vertex = henka_authoring_mesh_get_vertex(mesh, ids[0]);
    result = vertex != NULL && fabsf(vertex->uv.x - 8.0f) < 0.0001f;

cleanup:
    remove(path);
    henka_authoring_mesh_history_destroy(history);
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("history/persistence");
}

static int test_modeling_operations(void)
{
    const henka_authoring_mesh_desc desc = {64U, 128U, 64U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id center_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_mesh_counts counts;
    int result = 0;

    if (henka_authoring_mesh_create_box(&desc, 2.0f, 3.0f, 4.0f, &mesh) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh) || henka_authoring_mesh_get_counts(mesh).vertices != 8U ||
        henka_authoring_mesh_get_counts(mesh).edges != 12U || henka_authoring_mesh_get_counts(mesh).faces != 6U)
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    counts = henka_authoring_mesh_get_counts(mesh);
    if (counts.vertices != 4U || counts.edges != 4U || counts.faces != 1U ||
        henka_authoring_mesh_duplicate_face(mesh, 1U, (henka_vec3){0.0f, 1.0f, 0.0f}, &new_face_id) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh) || henka_authoring_mesh_get_counts(mesh).faces != 2U)
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_extrude_face(mesh, 1U, 0.5f, &new_face_id) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh) || henka_authoring_mesh_get_counts(mesh).vertices != 8U ||
        henka_authoring_mesh_get_counts(mesh).faces != 6U)
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_inset_face(mesh, 1U, 0.5f, &face_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_bevel_face(mesh, face_id, 0.25f, &new_face_id) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh) || henka_authoring_mesh_get_counts(mesh).faces != 9U)
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_subdivide_face(mesh, 1U, &center_id) != HENKA_SUCCESS ||
        center_id == HENKA_AUTHORING_INVALID_ID || !henka_authoring_mesh_validate(mesh) ||
        henka_authoring_mesh_get_counts(mesh).faces != 4U)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("modeling operations");
}

static int test_uv_authoring(void)
{
    const henka_authoring_mesh_desc desc = {64U, 128U, 64U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_vec2 uv;
    int result = 0;

    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_project_face_uv(mesh, 1U, HENKA_AUTHORING_UV_PROJECT_Y) != HENKA_SUCCESS ||
        henka_authoring_mesh_get_face_corner_uv(mesh, 1U, 2U, &uv) != HENKA_SUCCESS ||
        fabsf(uv.x - 1.0f) > 0.0001f || fabsf(uv.y - 1.0f) > 0.0001f ||
        henka_authoring_mesh_transform_face_uv(mesh, 1U, (henka_vec2){2.0f, 2.0f}, (henka_vec2){-1.0f, -1.0f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_pack_face_uv(mesh, 1U, 0.1f) != HENKA_SUCCESS ||
        !henka_authoring_mesh_face_uvs_are_finite(mesh, 1U) ||
        henka_authoring_mesh_get_face_corner_uv(mesh, 1U, 0U, &uv) != HENKA_SUCCESS ||
        uv.x < 0.099f || uv.y < 0.099f)
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_box(&desc, 2.0f, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_set_face_corner_uv(mesh, 1U, 1U, (henka_vec2){9.0f, 9.0f}) != HENKA_SUCCESS ||
        !henka_authoring_mesh_faces_share_uv_seam(mesh, 1U, 3U))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("UV authoring");
}

static int test_modeling_material_region_and_uv_continuity(void)
{
    const char* path = "authoring_material_regions.hams";
    const henka_authoring_mesh_desc desc = {64U, 128U, 64U, 8U};
    const henka_vec3 positions[4] =
    {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    };
    const henka_vec2 vertex_uvs[4] =
    {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };
    const henka_authoring_vertex_id face_vertices[4] = {1U, 2U, 3U, 4U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertex_ids[4];
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_render_vertex render_vertices[24];
    uint32_t render_indices[36];
    henka_authoring_render_data render =
        {render_vertices, 24U, 0U, render_indices, 36U, 0U};
    henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
    size_t index;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < 4U; ++index)
    {
        if (henka_authoring_mesh_add_vertex(
                mesh, positions[index], vertex_uvs[index], 7U, &vertex_ids[index]) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (henka_authoring_mesh_add_face(mesh, face_vertices, 4U, 11U, true, &face_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_set_face_corner_uv(mesh, face_id, 0U, (henka_vec2){0.2f, 0.3f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_set_face_corner_uv(mesh, face_id, 1U, (henka_vec2){1.2f, 0.3f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_set_face_corner_uv(mesh, face_id, 2U, (henka_vec2){1.2f, 1.3f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_set_face_corner_uv(mesh, face_id, 3U, (henka_vec2){0.2f, 1.3f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_extrude_face(mesh, face_id, 0.25f, &new_face_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_get_counts(mesh).faces != 6U ||
        henka_authoring_mesh_get_face(mesh, new_face_id) == NULL ||
        henka_authoring_mesh_get_face(mesh, new_face_id)->material_region != 11U)
    {
        goto cleanup;
    }
    for (index = 1U; index <= henka_authoring_mesh_get_counts(mesh).faces + 2U; ++index)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            mesh, (henka_authoring_face_id)index);
        if (face != NULL && face->material_region != 11U)
        {
            goto cleanup;
        }
    }
    if (henka_authoring_mesh_evaluate(mesh, &render) != HENKA_SUCCESS ||
        render.vertex_count != 24U || render.index_count != 36U)
    {
        goto cleanup;
    }
    for (index = 0U; index < render.vertex_count; ++index)
    {
        if (render.vertices[index].material_region != 11U)
        {
            goto cleanup;
        }
    }
    if (henka_authoring_mesh_save_file(mesh, path) != HENKA_SUCCESS ||
        henka_authoring_mesh_set_face_corner_uv(mesh, new_face_id, 0U, (henka_vec2){9.0f, 9.0f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_load_file(mesh, path) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    {
        henka_vec2 restored_uv;
        const henka_authoring_face* restored_face = henka_authoring_mesh_get_face(mesh, new_face_id);
        if (restored_face == NULL || restored_face->material_region != 11U ||
            henka_authoring_mesh_get_face_corner_uv(mesh, new_face_id, 0U, &restored_uv) != HENKA_SUCCESS ||
            fabsf(restored_uv.x - 0.2f) > 0.0001f || fabsf(restored_uv.y - 0.3f) > 0.0001f)
        {
            goto cleanup;
        }
    }
    result = 1;

cleanup:
    remove(path);
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("modeling material-region/UV continuity");
}

int main(void)
{
    return test_topology_and_evaluation() && test_rejection_and_tombstones() &&
        test_history_and_persistence() && test_modeling_operations() && test_uv_authoring() &&
        test_modeling_material_region_and_uv_continuity() ? 0 : 1;
}
