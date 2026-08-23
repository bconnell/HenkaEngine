#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/authoring_mesh.h>
#include <henka/authoring_modeling.h>
#include <henka/authoring_uv.h>

static int fail(const char* message)
{
    fprintf(stderr, "authoring mesh test failed: %s\n", message);
    return 0;
}

static FILE* test_open_file(const char* path, const char* mode)
{
    FILE* file = NULL;
#ifdef _WIN32
    if (fopen_s(&file, path, mode) != 0)
    {
        return NULL;
    }
#else
    file = fopen(path, mode);
#endif
    return file;
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
    {
        henka_vec3 center;
        henka_vec3 extents;
        if (henka_authoring_mesh_get_bounds(mesh, &center, &extents) != HENKA_SUCCESS ||
            fabsf(center.x) > 0.0001f || fabsf(center.y - 0.5f) > 0.0001f ||
            fabsf(extents.x - 1.0f) > 0.0001f || fabsf(extents.y - 0.5f) > 0.0001f)
        {
            goto cleanup;
        }
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
    const char* path = "build/test_tmp/authoring_nested/authoring_mesh_checkpoint.bin";
    henka_authoring_mesh_desc desc = {3U, 3U, 1U, 3U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_mesh* loaded = NULL;
    henka_authoring_mesh_history* history = NULL;
    henka_authoring_vertex_id ids[3];
    henka_authoring_vertex_id face[] = {1U, 2U, 3U};
    henka_authoring_face_id face_id;
    const henka_authoring_vertex* vertex;
    FILE* saved;
    unsigned char header[8];
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
    if (henka_authoring_mesh_set_vertex_position(mesh, ids[0], (henka_vec3){2.0f, 3.0f, 4.0f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_get_vertex(mesh, ids[0])->position.z != 4.0f ||
        henka_authoring_mesh_set_vertex_uv(mesh, ids[0], (henka_vec2){0.5f, 0.5f}) != HENKA_SUCCESS ||
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
        henka_authoring_mesh_save_file(mesh, path) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    saved = test_open_file(path, "rb");
    if (saved == NULL)
    {
        goto cleanup;
    }
    if (fread(header, sizeof(header), 1U, saved) != 1U)
    {
        fclose(saved);
        saved = NULL;
        goto cleanup;
    }
    if (fclose(saved) != 0)
    {
        saved = NULL;
        goto cleanup;
    }
    saved = NULL;
    if (memcmp(header, "HAMS\x03\0\0\0", sizeof(header)) != 0)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_set_vertex_uv(mesh, ids[0], (henka_vec2){9.0f, 9.0f}) != HENKA_SUCCESS ||
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
    if (henka_authoring_mesh_load_file_new(path, &loaded) != HENKA_SUCCESS ||
        loaded == NULL || !henka_authoring_mesh_validate(loaded) ||
        henka_authoring_mesh_get_counts(loaded).faces != 1U ||
        henka_authoring_mesh_get_vertex(loaded, ids[0]) == NULL)
    {
        goto cleanup;
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
    henka_authoring_mesh_destroy(loaded);
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

static int test_vertex_merge_operations(void)
{
    const henka_authoring_mesh_desc desc = {64U, 128U, 64U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id survivors[8] = {0U};
    henka_authoring_modeling_report report;
    size_t survivor_count = 0U;
    henka_authoring_mesh_counts before;
    henka_result merge_result;
    int result = 0;

    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_merge_vertices(
            mesh, (const henka_authoring_vertex_id[]){1U, 2U}, 2U,
            HENKA_AUTHORING_VERTEX_MERGE_CENTER, HENKA_AUTHORING_INVALID_ID,
            survivors, 8U, &survivor_count, &report) != HENKA_SUCCESS ||
        survivor_count != 1U || survivors[0] != 1U || !report.changed ||
        report.removed_vertices != 1U || henka_authoring_mesh_get_counts(mesh).vertices != 3U ||
        henka_authoring_mesh_get_counts(mesh).faces != 1U || !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_merge_vertices(
            mesh, (const henka_authoring_vertex_id[]){1U, 2U}, 2U,
            HENKA_AUTHORING_VERTEX_MERGE_ACTIVE, 2U,
            survivors, 8U, &survivor_count, &report) != HENKA_SUCCESS ||
        survivor_count != 1U || survivors[0] != 2U ||
        henka_authoring_mesh_get_vertex(mesh, 1U) != NULL ||
        henka_authoring_mesh_get_vertex(mesh, 2U) == NULL)
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_merge_vertices_by_distance(
            mesh, (const henka_authoring_vertex_id[]){1U, 2U, 3U, 4U}, 4U,
            0.01f, survivors, 8U, &survivor_count, &report) != HENKA_SUCCESS ||
        report.changed || survivor_count != 4U ||
        henka_authoring_mesh_get_counts(mesh).vertices != before.vertices ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    merge_result = henka_authoring_mesh_set_vertex_position(
        mesh, 2U, (henka_vec3){-0.99f, 0.0f, -1.0f});
    if (merge_result == HENKA_SUCCESS)
    {
        merge_result = henka_authoring_mesh_merge_vertices_by_distance(
            mesh, (const henka_authoring_vertex_id[]){1U, 2U, 3U, 4U}, 4U,
            0.02f, survivors, 8U, &survivor_count, &report);
    }
    if (merge_result != HENKA_SUCCESS || !report.changed || survivor_count != 3U || survivors[0] != 1U ||
        henka_authoring_mesh_get_vertex(mesh, 2U) != NULL || !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_merge_vertices(
            mesh, (const henka_authoring_vertex_id[]){1U, 2U, 3U, 4U}, 4U,
            HENKA_AUTHORING_VERTEX_MERGE_CENTER, HENKA_AUTHORING_INVALID_ID,
            survivors, 8U, &survivor_count, &report) == HENKA_SUCCESS ||
        henka_authoring_mesh_get_counts(mesh).vertices != before.vertices ||
        henka_authoring_mesh_get_counts(mesh).faces != before.faces)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("vertex merge operations");
}

static int test_vertex_topology_operations(void)
{
    const henka_authoring_mesh_desc desc = {64U, 128U, 64U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_modeling_report report;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    int result = 0;

    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_dissolve_vertices(mesh, (const henka_authoring_vertex_id[]){1U}, 1U, &report) != HENKA_SUCCESS ||
        !report.changed || henka_authoring_mesh_get_counts(mesh).vertices != 3U ||
        henka_authoring_mesh_get_face(mesh, 1U)->corner_count != 3U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U, &(henka_authoring_vertex_id){0U}) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    {
        henka_authoring_vertex_id ids[4];
        const henka_vec3 positions[3] = {{1.0f, 0.0f, 0.0f}, {-0.5f, 0.0f, 0.8660254f}, {-0.5f, 0.0f, -0.8660254f}};
        const henka_authoring_vertex_id faces[3][3] = {{1U, 2U, 3U}, {1U, 3U, 4U}, {1U, 4U, 2U}};
        size_t index;
        for (index = 0U; index < 3U; ++index)
        {
            if (henka_authoring_mesh_add_vertex(mesh, positions[index], (henka_vec2){0.0f, 0.0f}, 0U, &ids[index + 1U]) != HENKA_SUCCESS) goto cleanup;
        }
        for (index = 0U; index < 3U; ++index)
        {
            if (henka_authoring_mesh_add_face(mesh, faces[index], 3U, 0U, false, &(henka_authoring_face_id){0U}) != HENKA_SUCCESS) goto cleanup;
        }
    }
    if (henka_authoring_mesh_dissolve_vertices(mesh, (const henka_authoring_vertex_id[]){1U}, 1U, &report) != HENKA_SUCCESS ||
        henka_authoring_mesh_get_vertex(mesh, 1U) != NULL || henka_authoring_mesh_get_counts(mesh).faces != 1U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_box(&desc, 2.0f, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_delete_vertices(mesh, (const henka_authoring_vertex_id[]){1U}, 1U, &report) != HENKA_SUCCESS ||
        henka_authoring_mesh_get_vertex(mesh, 1U) != NULL || henka_authoring_mesh_get_counts(mesh).faces != 3U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_connect_vertices(mesh, 1U, 3U, &new_face_id, &report) != HENKA_SUCCESS ||
        new_face_id != 2U || henka_authoring_mesh_get_counts(mesh).faces != 2U ||
        henka_authoring_mesh_get_face(mesh, 1U)->corner_count != 3U ||
        henka_authoring_mesh_get_face(mesh, new_face_id)->corner_count != 3U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("vertex topology operations");
}

static int test_vertex_bevel_operations(void)
{
    const henka_authoring_mesh_desc desc = {64U, 128U, 64U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id result_vertices[16];
    henka_authoring_modeling_report report;
    size_t result_count = 0U;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    int result = 0;

    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_bevel_vertices(
            mesh, (const henka_authoring_vertex_id[]){1U}, 1U, 0.2f,
            result_vertices, 16U, &result_count, &report) != HENKA_SUCCESS ||
        !report.changed || result_count != 2U ||
        henka_authoring_mesh_get_counts(mesh).vertices != before.vertices + 1U ||
        henka_authoring_mesh_get_vertex(mesh, 1U) != NULL ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U, &(henka_authoring_vertex_id){0U}) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    {
        const henka_vec3 positions[3] = {
            {1.0f, 0.0f, 0.0f}, {-0.5f, 0.0f, 0.8660254f}, {-0.5f, 0.0f, -0.8660254f}};
        const henka_authoring_vertex_id faces[3][3] = {{1U, 2U, 3U}, {1U, 3U, 4U}, {1U, 4U, 2U}};
        size_t index;
        for (index = 0U; index < 3U; ++index)
        {
            if (henka_authoring_mesh_add_vertex(mesh, positions[index], (henka_vec2){0.0f, 0.0f}, 0U, &(henka_authoring_vertex_id){0U}) != HENKA_SUCCESS) goto cleanup;
        }
        for (index = 0U; index < 3U; ++index)
        {
            if (henka_authoring_mesh_add_face(mesh, faces[index], 3U, 0U, false, &(henka_authoring_face_id){0U}) != HENKA_SUCCESS) goto cleanup;
        }
    }
    if (henka_authoring_mesh_bevel_vertices(
            mesh, (const henka_authoring_vertex_id[]){1U}, 1U, 0.1f,
            result_vertices, 16U, &result_count, &report) != HENKA_SUCCESS ||
        result_count != 3U || henka_authoring_mesh_get_counts(mesh).faces != 4U ||
        henka_authoring_mesh_get_vertex(mesh, 1U) != NULL ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_bevel_vertices(
            mesh, (const henka_authoring_vertex_id[]){1U}, 1U, 0.0f,
            result_vertices, 16U, &result_count, &report) == HENKA_SUCCESS ||
        (after = henka_authoring_mesh_get_counts(mesh), memcmp(&before, &after, sizeof(before)) != 0) ||
        henka_authoring_mesh_get_vertex(mesh, 1U) == NULL)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("vertex bevel operations");
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
        henka_authoring_mesh_set_face_material_region(mesh, face_id, 13U) != HENKA_SUCCESS ||
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
        henka_authoring_mesh_get_face(mesh, new_face_id)->material_region != 13U)
    {
        goto cleanup;
    }
    for (index = 1U; index <= henka_authoring_mesh_get_counts(mesh).faces + 2U; ++index)
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(
            mesh, (henka_authoring_face_id)index);
        if (face != NULL && face->material_region != 13U)
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
        if (render.vertices[index].material_region != 13U)
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
        if (restored_face == NULL || restored_face->material_region != 13U ||
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

static int test_bounded_primitive_constructors(void)
{
    const henka_authoring_mesh_desc desc = {96U, 192U, 96U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_mesh_counts counts;
    henka_vec3 center;
    henka_vec3 extents;
    int result = 0;

    if (henka_authoring_mesh_create_cylinder(&desc, 1.0f, 2.0f, 8U, &mesh) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    counts = henka_authoring_mesh_get_counts(mesh);
    if (counts.vertices != 16U || counts.faces != 10U ||
        henka_authoring_mesh_get_bounds(mesh, &center, &extents) != HENKA_SUCCESS ||
        fabsf(center.x) > 0.0001f || fabsf(center.y) > 0.0001f || fabsf(center.z) > 0.0001f ||
        fabsf(extents.x - 1.0f) > 0.0001f || fabsf(extents.y - 1.0f) > 0.0001f ||
        fabsf(extents.z - 1.0f) > 0.0001f)
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;

    if (henka_authoring_mesh_create_cone(&desc, 1.0f, 2.0f, 8U, &mesh) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    counts = henka_authoring_mesh_get_counts(mesh);
    if (counts.vertices != 9U || counts.faces != 9U)
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;

    if (henka_authoring_mesh_create_uv_sphere(&desc, 1.0f, 8U, 4U, &mesh) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    counts = henka_authoring_mesh_get_counts(mesh);
    if (counts.vertices != 26U || counts.faces != 32U ||
        henka_authoring_mesh_get_bounds(mesh, &center, &extents) != HENKA_SUCCESS ||
        fabsf(extents.x - 1.0f) > 0.0001f || fabsf(extents.y - 1.0f) > 0.0001f ||
        fabsf(extents.z - 1.0f) > 0.0001f)
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;

    if (henka_authoring_mesh_create_cylinder(&desc, 0.0f, 2.0f, 8U, &mesh) != HENKA_ERROR_INVALID_ARGUMENT ||
        mesh != NULL ||
        henka_authoring_mesh_create_cone(&desc, 1.0f, -2.0f, 8U, &mesh) != HENKA_ERROR_INVALID_ARGUMENT ||
        mesh != NULL ||
        henka_authoring_mesh_create_uv_sphere(&desc, 1.0f, 2U, 4U, &mesh) != HENKA_ERROR_INVALID_ARGUMENT ||
        mesh != NULL)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("bounded primitive constructors");
}

static int test_logical_identity_reuse_and_history(void)
{
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_mesh_history* history = NULL;
    henka_authoring_vertex_id first_vertex_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id second_vertex_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id vertices[3];
    henka_authoring_face_id first_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id second_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id branch_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id retired_edge_id = HENKA_AUTHORING_INVALID_ID;
    int result = 0;

    {
        const henka_authoring_mesh_desc desc = {1U, 1U, 1U, 3U};
        if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
            henka_authoring_mesh_add_vertex(
                mesh,
                (henka_vec3){0.0f, 0.0f, 0.0f},
                (henka_vec2){0.0f, 0.0f},
                0U,
                &first_vertex_id) != HENKA_SUCCESS ||
            first_vertex_id == 0U ||
            henka_authoring_mesh_remove_vertex(mesh, first_vertex_id) != HENKA_SUCCESS ||
            henka_authoring_mesh_add_vertex(
                mesh,
                (henka_vec3){1.0f, 0.0f, 0.0f},
                (henka_vec2){1.0f, 0.0f},
                0U,
                &second_vertex_id) != HENKA_SUCCESS ||
            second_vertex_id == first_vertex_id ||
            henka_authoring_mesh_get_vertex(mesh, first_vertex_id) != NULL ||
            henka_authoring_mesh_get_vertex(mesh, second_vertex_id) == NULL ||
            !henka_authoring_mesh_validate(mesh))
        {
            goto cleanup;
        }
        henka_authoring_mesh_destroy(mesh);
        mesh = NULL;
    }

    {
        const henka_authoring_mesh_desc desc = {3U, 3U, 1U, 3U};
        const henka_authoring_face* first_face;
        const henka_authoring_face* second_face;
        if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
            henka_authoring_mesh_add_vertex(
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
            henka_authoring_mesh_add_face(
                mesh,
                vertices,
                3U,
                0U,
                false,
                &first_face_id) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        first_face = henka_authoring_mesh_get_face(mesh, first_face_id);
        if (first_face == NULL)
        {
            goto cleanup;
        }
        retired_edge_id = first_face->edges[0];
        if (henka_authoring_mesh_remove_face(mesh, first_face_id) != HENKA_SUCCESS ||
            henka_authoring_mesh_get_face(mesh, first_face_id) != NULL ||
            henka_authoring_mesh_get_edge(mesh, retired_edge_id) != NULL ||
            henka_authoring_mesh_add_face(
                mesh,
                vertices,
                3U,
                0U,
                false,
                &second_face_id) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        second_face = henka_authoring_mesh_get_face(mesh, second_face_id);
        if (second_face == NULL || second_face_id == first_face_id ||
            second_face->edges[0] == retired_edge_id ||
            henka_authoring_mesh_get_edge(mesh, retired_edge_id) != NULL ||
            !henka_authoring_mesh_validate(mesh))
        {
            goto cleanup;
        }
        henka_authoring_mesh_destroy(mesh);
        mesh = NULL;
    }

    {
        const henka_authoring_mesh_desc desc = {3U, 3U, 1U, 3U};
        if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
            henka_authoring_mesh_add_vertex(
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
            henka_authoring_mesh_add_face(
                mesh,
                vertices,
                3U,
                0U,
                false,
                &first_face_id) != HENKA_SUCCESS ||
            henka_authoring_mesh_history_create(mesh, 8U, &history) != HENKA_SUCCESS ||
            henka_authoring_mesh_remove_face(mesh, first_face_id) != HENKA_SUCCESS ||
            henka_authoring_mesh_history_checkpoint(history, mesh) != HENKA_SUCCESS ||
            henka_authoring_mesh_add_face(
                mesh,
                vertices,
                3U,
                0U,
                false,
                &second_face_id) != HENKA_SUCCESS ||
            second_face_id == first_face_id ||
            henka_authoring_mesh_history_checkpoint(history, mesh) != HENKA_SUCCESS ||
            henka_authoring_mesh_history_undo(history, mesh) != HENKA_SUCCESS ||
            henka_authoring_mesh_get_face(mesh, second_face_id) != NULL ||
            !henka_authoring_mesh_history_can_redo(history) ||
            henka_authoring_mesh_history_redo(history, mesh) != HENKA_SUCCESS ||
            henka_authoring_mesh_get_face(mesh, second_face_id) == NULL ||
            henka_authoring_mesh_history_undo(history, mesh) != HENKA_SUCCESS ||
            henka_authoring_mesh_add_face(
                mesh,
                vertices,
                3U,
                0U,
                false,
                &branch_face_id) != HENKA_SUCCESS ||
            branch_face_id == first_face_id || branch_face_id == second_face_id ||
            henka_authoring_mesh_history_checkpoint(history, mesh) != HENKA_SUCCESS ||
            henka_authoring_mesh_history_can_redo(history) ||
            henka_authoring_mesh_get_face(mesh, second_face_id) != NULL ||
            !henka_authoring_mesh_validate(mesh))
        {
            goto cleanup;
        }
        henka_authoring_mesh_history_destroy(history);
        history = NULL;
        henka_authoring_mesh_destroy(mesh);
        mesh = NULL;
    }

    {
        const henka_authoring_mesh_desc desc = {8U, 12U, 4U, 4U};
        size_t iteration;
        if (henka_authoring_mesh_create_plane(&desc, 1.0f, 1.0f, &mesh) != HENKA_SUCCESS ||
            henka_authoring_mesh_get_face(mesh, 1U) == NULL)
        {
            goto cleanup;
        }
        for (iteration = 0U; iteration < 32U; ++iteration)
        {
            henka_authoring_face_id duplicate_face_id;
            henka_authoring_vertex_id duplicate_vertices[4];
            const henka_authoring_face* duplicate_face;
            henka_authoring_modeling_report report;
            size_t duplicate_corner_count;
            size_t corner;
            if (henka_authoring_mesh_duplicate_face(
                    mesh,
                    1U,
                    (henka_vec3){(float)iteration * 0.01f, 0.0f, 0.0f},
                    &duplicate_face_id) != HENKA_SUCCESS)
            {
                goto cleanup;
            }
            duplicate_face = henka_authoring_mesh_get_face(mesh, duplicate_face_id);
            if (duplicate_face == NULL || duplicate_face->corner_count > 4U)
            {
                goto cleanup;
            }
            duplicate_corner_count = duplicate_face->corner_count;
            for (corner = 0U; corner < duplicate_corner_count; ++corner)
            {
                duplicate_vertices[corner] = duplicate_face->vertices[corner];
            }
            if (henka_authoring_mesh_remove_face(mesh, duplicate_face_id) != HENKA_SUCCESS ||
                henka_authoring_mesh_delete_vertices(
                    mesh,
                    duplicate_vertices,
                    duplicate_corner_count,
                    &report) != HENKA_SUCCESS ||
                henka_authoring_mesh_get_counts(mesh).vertices != 4U ||
                henka_authoring_mesh_get_counts(mesh).faces != 1U ||
                !henka_authoring_mesh_validate(mesh))
            {
                goto cleanup;
            }
        }
    }
    result = 1;

cleanup:
    henka_authoring_mesh_history_destroy(history);
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("logical identity reuse/history/churn");
}

int main(void)
{
    return test_topology_and_evaluation() && test_rejection_and_tombstones() &&
        test_history_and_persistence() && test_modeling_operations() && test_vertex_merge_operations() &&
        test_vertex_topology_operations() && test_vertex_bevel_operations() && test_uv_authoring() &&
        test_modeling_material_region_and_uv_continuity() &&
        test_bounded_primitive_constructors() &&
        test_logical_identity_reuse_and_history() ? 0 : 1;
}
