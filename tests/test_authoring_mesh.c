#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <henka/authoring_mesh.h>
#include <henka/authoring_modeling.h>
#include <henka/authoring_topology.h>
#include <henka/authoring_uv.h>

/* Red-test seam for the next bounded modeling operation.  The public
 * declaration is added only after this test proves the current API is
 * missing the behavior. */
extern henka_result henka_authoring_mesh_flip_face(
    henka_authoring_mesh* mesh,
    henka_authoring_face_id face_id);

static int fail(const char* message)
{
    fprintf(stderr, "authoring mesh test failed: %s\n", message);
    return 0;
}

static henka_result test_find_edge_between_vertices(
    const henka_authoring_mesh* mesh,
    henka_authoring_vertex_id first,
    henka_authoring_vertex_id second,
    henka_authoring_edge_id* out_edge_id)
{
    const henka_authoring_mesh_desc desc = henka_authoring_mesh_get_desc(mesh);
    size_t slot;
    if (mesh == NULL || out_edge_id == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_edge_id = HENKA_AUTHORING_INVALID_ID;
    for (slot = 0U; slot < desc.max_edges; ++slot)
    {
        henka_authoring_edge_id edge_id;
        const henka_authoring_edge* edge;
        if (henka_authoring_mesh_get_edge_id_at(mesh, slot, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        edge = henka_authoring_mesh_get_edge(mesh, edge_id);
        if (edge != NULL &&
            ((edge->vertices[0] == first && edge->vertices[1] == second) ||
             (edge->vertices[0] == second && edge->vertices[1] == first)))
        {
            *out_edge_id = edge_id;
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
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

static int test_write_u32(FILE* file, uint32_t value)
{
    const unsigned char bytes[4] = {
        (unsigned char)(value & 0xffU),
        (unsigned char)((value >> 8U) & 0xffU),
        (unsigned char)((value >> 16U) & 0xffU),
        (unsigned char)((value >> 24U) & 0xffU)};
    return fwrite(bytes, sizeof(bytes), 1U, file) == 1U;
}

static int test_write_f32(FILE* file, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return test_write_u32(file, bits);
}

static int test_write_vec3(FILE* file, henka_vec3 value)
{
    return test_write_f32(file, value.x) && test_write_f32(file, value.y) && test_write_f32(file, value.z);
}

static int test_write_vec2(FILE* file, henka_vec2 value)
{
    return test_write_f32(file, value.x) && test_write_f32(file, value.y);
}

static int test_write_legacy_fixture(const char* path, uint32_t version)
{
    FILE* file = test_open_file(path, "wb");
    const uint32_t invalid_id = HENKA_AUTHORING_INVALID_ID;
    const henka_vec3 positions[3] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}};
    const henka_vec2 uvs[3] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {0.0f, 1.0f}};
    const uint32_t edge_vertices[3][2] = {{1U, 2U}, {2U, 3U}, {1U, 3U}};
    const uint32_t face_edges[3] = {1U, 2U, 3U};
    size_t index;
    int ok = file != NULL;

    if (!ok) return 0;
    ok = fwrite("HAMS", 4U, 1U, file) == 1U &&
        test_write_u32(file, version) &&
        test_write_u32(file, 3U) && test_write_u32(file, 3U) &&
        test_write_u32(file, 1U) && test_write_u32(file, 3U) &&
        test_write_u32(file, 3U) && test_write_u32(file, 3U) && test_write_u32(file, 1U);
    for (index = 0U; ok && index < 3U; ++index)
    {
        ok = fputc(1, file) != EOF && test_write_vec3(file, positions[index]) &&
            test_write_vec2(file, uvs[index]) && test_write_u32(file, 0U);
    }
    for (index = 0U; ok && index < 3U; ++index)
    {
        ok = fputc(1, file) != EOF && test_write_u32(file, edge_vertices[index][0]) &&
            test_write_u32(file, edge_vertices[index][1]) && test_write_u32(file, 1U) &&
            test_write_u32(file, invalid_id) && test_write_u32(file, 1U) &&
            fputc(0, file) != EOF;
    }
    if (ok)
    {
        ok = fputc(1, file) != EOF && test_write_u32(file, 3U) && test_write_u32(file, 0U) &&
            fputc(0, file) != EOF;
        for (index = 0U; ok && index < 3U; ++index)
        {
            ok = test_write_u32(file, (uint32_t)(index + 1U));
        }
        for (index = 0U; ok && index < 3U; ++index)
        {
            ok = test_write_vec2(file, uvs[index]);
        }
        for (index = 0U; ok && index < 3U; ++index)
        {
            ok = test_write_u32(file, face_edges[index]);
        }
    }
    if (fclose(file) != 0) ok = 0;
    return ok;
}

static int test_append_byte(const char* path, unsigned char value)
{
    FILE* file = test_open_file(path, "ab");
    const int ok = file != NULL && fputc(value, file) != EOF;
    if (file != NULL && fclose(file) != 0) return 0;
    return ok;
}

static int test_patch_hams_u32_at(const char* path, long offset, uint32_t value)
{
    FILE* file = test_open_file(path, "rb+");
    int ok;
    if (file == NULL) return 0;
    ok = fseek(file, offset, SEEK_SET) == 0 && test_write_u32(file, value);
    if (fclose(file) != 0) ok = 0;
    return ok;
}

static int test_patch_hams_version(const char* path, uint32_t version)
{
    return test_patch_hams_u32_at(path, 4L, version);
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

static int test_evaluation_failure_clears_output_counts(void)
{
    const henka_authoring_mesh_desc desc = {4U, 4U, 1U, 4U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_render_vertex vertices[4];
    uint32_t indices[6];
    henka_authoring_render_data render = {
        vertices, 4U, 91U, indices, 6U, 73U};
    henka_authoring_render_data undersized = {
        vertices, 1U, 17U, indices, 1U, 29U};
    int result = 0;

    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_evaluate(mesh, &render) != HENKA_SUCCESS ||
        render.vertex_count != 4U || render.index_count != 6U)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_evaluate(mesh, &undersized) != HENKA_ERROR_LIMIT ||
        undersized.vertex_count != 0U || undersized.index_count != 0U)
    {
        goto cleanup;
    }
    render.vertex_count = 31U;
    render.index_count = 37U;
    render.vertices = NULL;
    if (henka_authoring_mesh_evaluate(mesh, &render) != HENKA_ERROR_INVALID_ARGUMENT ||
        render.vertex_count != 0U || render.index_count != 0U)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("evaluation failure output state");
}

static int test_face_operation_outputs_fail_closed(void)
{
    const henka_authoring_mesh_desc desc = {64U, 128U, 64U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_face_id output_face_id = HENKA_AUTHORING_INVALID_ID;
    int result = 0;

    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    output_face_id = 123U;
    if (henka_authoring_mesh_duplicate_face(
            mesh, HENKA_AUTHORING_INVALID_ID, (henka_vec3){0.0f, 1.0f, 0.0f},
            &output_face_id) != HENKA_ERROR_INVALID_ARGUMENT ||
        output_face_id != HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    output_face_id = 123U;
    if (henka_authoring_mesh_extrude_face(
            mesh, HENKA_AUTHORING_INVALID_ID, 0.5f, &output_face_id) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        output_face_id != HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    output_face_id = 123U;
    if (henka_authoring_mesh_inset_face(
            mesh, HENKA_AUTHORING_INVALID_ID, 0.5f, &output_face_id) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        output_face_id != HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    output_face_id = 123U;
    if (henka_authoring_mesh_bevel_face(
            mesh, HENKA_AUTHORING_INVALID_ID, 0.25f, &output_face_id) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        output_face_id != HENKA_AUTHORING_INVALID_ID ||
        henka_authoring_mesh_get_counts(mesh).faces != 1U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("face operation output state");
}

static int test_primitive_constructor_outputs_fail_closed(void)
{
    const henka_authoring_mesh_desc desc = {64U, 128U, 64U, 8U};
    henka_authoring_mesh* mesh;
    henka_authoring_mesh* const sentinel = (henka_authoring_mesh*)1;
    int result = 0;

    mesh = sentinel;
    if (henka_authoring_mesh_create_plane(&desc, 0.0f, 2.0f, &mesh) !=
            HENKA_ERROR_INVALID_ARGUMENT || mesh != NULL)
    {
        goto cleanup;
    }
    mesh = sentinel;
    if (henka_authoring_mesh_create_box(&desc, 2.0f, 0.0f, 2.0f, &mesh) !=
            HENKA_ERROR_INVALID_ARGUMENT || mesh != NULL)
    {
        goto cleanup;
    }
    mesh = sentinel;
    if (henka_authoring_mesh_create_cylinder(&desc, 0.0f, 2.0f, 8U, &mesh) !=
            HENKA_ERROR_INVALID_ARGUMENT || mesh != NULL)
    {
        goto cleanup;
    }
    mesh = sentinel;
    if (henka_authoring_mesh_create_cone(&desc, 1.0f, -2.0f, 8U, &mesh) !=
            HENKA_ERROR_INVALID_ARGUMENT || mesh != NULL)
    {
        goto cleanup;
    }
    mesh = sentinel;
    if (henka_authoring_mesh_create_uv_sphere(&desc, 1.0f, 2U, 4U, &mesh) !=
            HENKA_ERROR_INVALID_ARGUMENT || mesh != NULL)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    if (mesh != sentinel)
    {
        henka_authoring_mesh_destroy(mesh);
    }
    return result ? 1 : fail("primitive constructor output state");
}

static int test_mesh_create_output_fails_closed(void)
{
    const henka_authoring_mesh_desc invalid_desc = {0U, 4U, 1U, 3U};
    henka_authoring_mesh* mesh = (henka_authoring_mesh*)1;

    if (henka_authoring_mesh_create(&invalid_desc, &mesh) != HENKA_ERROR_INVALID_ARGUMENT ||
        mesh != NULL)
    {
        if (mesh != NULL && mesh != (henka_authoring_mesh*)1)
        {
            henka_authoring_mesh_destroy(mesh);
        }
        return fail("mesh create output state");
    }
    return 1;
}

static int test_topology_add_outputs_fail_closed(void)
{
    const henka_authoring_mesh_desc desc = {3U, 3U, 1U, 3U};
    const henka_authoring_vertex_id face[] = {1U, 2U, 3U};
    const henka_authoring_vertex_id invalid_face[] = {1U, 1U, 2U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertex_id = 123U;
    henka_authoring_face_id face_id = 123U;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_vec2){0.0f, 0.0f}, 0U, &vertex_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){1.0f, 0.0f, 0.0f},
            (henka_vec2){1.0f, 0.0f}, 0U, &vertex_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(mesh, (henka_vec3){0.0f, 1.0f, 0.0f},
            (henka_vec2){0.0f, 1.0f}, 0U, &vertex_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    vertex_id = 123U;
    if (henka_authoring_mesh_add_vertex(mesh, (henka_vec3){NAN, 0.0f, 0.0f},
            (henka_vec2){0.0f, 0.0f}, 0U, &vertex_id) != HENKA_ERROR_INVALID_ARGUMENT ||
        vertex_id != HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    face_id = 123U;
    if (henka_authoring_mesh_add_face(mesh, invalid_face, 3U, 0U, false, &face_id) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        face_id != HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_add_face(mesh, face, 3U, 0U, false, &face_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    face_id = 123U;
    if (henka_authoring_mesh_add_face(mesh, face, 3U, 0U, false, &face_id) != HENKA_ERROR_LIMIT ||
        face_id != HENKA_AUTHORING_INVALID_ID || !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("topology add output state");
}

static int test_query_outputs_fail_closed(void)
{
    const henka_authoring_mesh_desc desc = {4U, 4U, 1U, 4U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_mesh* empty = NULL;
    henka_authoring_vertex_id vertex_id = 123U;
    henka_authoring_edge_id edge_id = 123U;
    henka_authoring_face_id face_id = 123U;
    henka_vec2 uv = {1.0f, 2.0f};
    henka_vec3 center = {1.0f, 2.0f, 3.0f};
    henka_vec3 extents = {4.0f, 5.0f, 6.0f};
    int result = 0;

    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_create(&desc, &empty) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_get_vertex_id_at(mesh, desc.max_vertices, &vertex_id) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        vertex_id != HENKA_AUTHORING_INVALID_ID ||
        henka_authoring_mesh_get_edge_id_at(mesh, desc.max_edges, &edge_id) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        edge_id != HENKA_AUTHORING_INVALID_ID ||
        henka_authoring_mesh_get_face_id_at(mesh, desc.max_faces, &face_id) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        face_id != HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_get_face_corner_uv(
            mesh, HENKA_AUTHORING_INVALID_ID, 0U, &uv) != HENKA_ERROR_INVALID_ARGUMENT ||
        uv.x != 0.0f || uv.y != 0.0f)
    {
        goto cleanup;
    }
    edge_id = 123U;
    if (henka_authoring_mesh_get_vertex_edge_at(mesh, 1U, desc.max_edges, &edge_id) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        edge_id != HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    face_id = 123U;
    if (henka_authoring_mesh_get_edge_face_at(mesh, 1U, 1U, &face_id) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        face_id != HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_get_bounds(empty, &center, &extents) != HENKA_ERROR_INVALID_ARGUMENT ||
        center.x != 0.0f || center.y != 0.0f || center.z != 0.0f ||
        extents.x != 0.0f || extents.y != 0.0f || extents.z != 0.0f)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(empty);
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("query output state");
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
    if (memcmp(header, "HAMS\x05\0\0\0", sizeof(header)) != 0)
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
    center_id = 123U;
    if (henka_authoring_mesh_subdivide_face(
            mesh, HENKA_AUTHORING_INVALID_ID, &center_id) != HENKA_ERROR_INVALID_ARGUMENT ||
        center_id != HENKA_AUTHORING_INVALID_ID ||
        henka_authoring_mesh_get_counts(mesh).faces != 4U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("modeling operations");
}

static int test_face_flip_operation(void)
{
    henka_authoring_mesh_desc desc = henka_authoring_mesh_desc_default();
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id invalid_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id before_vertices[4];
    henka_authoring_edge_id before_edges[4];
    henka_vec2 before_uvs[4];
    size_t corner;
    int result = 0;

    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_get_face_id_at(mesh, 0U, &face_id) != HENKA_SUCCESS)
    {
        return fail("face flip setup");
    }
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(mesh, face_id);
        if (face == NULL || face->corner_count != 4U)
        {
            henka_authoring_mesh_destroy(mesh);
            return fail("face flip source face");
        }
        for (corner = 0U; corner < face->corner_count; ++corner)
        {
            before_vertices[corner] = face->vertices[corner];
            before_edges[corner] = face->edges[corner];
            before_uvs[corner] = face->uvs[corner];
        }
    }

    if (henka_authoring_mesh_flip_face(mesh, face_id) != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(mesh);
        return fail("face flip operation");
    }
    {
        const henka_authoring_face* face = henka_authoring_mesh_get_face(mesh, face_id);
        if (face == NULL || !henka_authoring_mesh_validate(mesh))
        {
            henka_authoring_mesh_destroy(mesh);
            return fail("face flip validation");
        }
        for (corner = 0U; corner < 4U; ++corner)
        {
            const size_t source_corner = corner == 0U ? 0U : 4U - corner;
            const size_t source_edge = 3U - corner;
            if (face->vertices[corner] != before_vertices[source_corner] ||
                face->edges[corner] != before_edges[source_edge] ||
                face->uvs[corner].x != before_uvs[source_corner].x ||
                face->uvs[corner].y != before_uvs[source_corner].y)
            {
                henka_authoring_mesh_destroy(mesh);
                return fail("face flip winding or corner metadata");
            }
        }
    }

    if (henka_authoring_mesh_flip_face(mesh, invalid_face_id) == HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh))
    {
        henka_authoring_mesh_destroy(mesh);
        return fail("face flip invalid handle");
    }
    result = 1;
    henka_authoring_mesh_destroy(mesh);
    return result;
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

static int test_boundary_edge_bevel_operation(void)
{
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_modeling_report report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    size_t face_slot;
    int result = 0;

    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_bevel_edge(mesh, 1U, 0.2f, &report) != HENKA_SUCCESS ||
        !report.changed || report.created_vertices != 2U || report.created_faces != 1U ||
        henka_authoring_mesh_get_counts(mesh).vertices != before.vertices + 2U ||
        henka_authoring_mesh_get_counts(mesh).edges != before.edges + 3U ||
        henka_authoring_mesh_get_counts(mesh).faces != before.faces + 1U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    for (face_slot = 0U; face_slot < henka_authoring_mesh_get_desc(mesh).max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
        const henka_authoring_face* face;
        if (henka_authoring_mesh_get_face_id_at(mesh, face_slot, &face_id) != HENKA_SUCCESS)
        {
            continue;
        }
        face = henka_authoring_mesh_get_face(mesh, face_id);
        if (face == NULL || face->corner_count != 4U)
        {
            goto cleanup;
        }
    }
    before = henka_authoring_mesh_get_counts(mesh);
    report = (henka_authoring_modeling_report){0};
    if (henka_authoring_mesh_bevel_edge(mesh, 1U, 2.0f, &report) == HENKA_SUCCESS ||
        report.changed)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (memcmp(&before, &after, sizeof(before)) != 0 ||
        henka_authoring_mesh_get_vertex(mesh, 1U) == NULL ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_box(&desc, 2.0f, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_get_edge(mesh, 1U) == NULL ||
        henka_authoring_mesh_get_edge(mesh, 1U)->face_count != 2U)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    report = (henka_authoring_modeling_report){0};
    if (henka_authoring_mesh_bevel_edge(mesh, 1U, 0.2f, &report) == HENKA_SUCCESS ||
        report.changed)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (memcmp(&before, &after, sizeof(before)) != 0 ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("boundary edge bevel operation");
}

static int test_boundary_edge_batch_bevel_operation(void)
{
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    const henka_vec3 positions[8] = {
        {-1.5f, -1.0f, 0.0f}, {-0.5f, -1.0f, 0.0f}, {-0.5f, 0.0f, 0.0f},
        {-1.5f, 0.0f, 0.0f}, {1.5f, -1.0f, 0.0f}, {2.5f, -1.0f, 0.0f},
        {2.5f, 0.0f, 0.0f}, {1.5f, 0.0f, 0.0f}};
    const henka_authoring_vertex_id face_vertices[2][4] = {
        {1U, 2U, 3U, 4U}, {5U, 6U, 7U, 8U}};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_edge_id selected_edges[2] = {
        HENKA_AUTHORING_INVALID_ID, HENKA_AUTHORING_INVALID_ID};
    henka_authoring_edge_id adjacent_edges[2] = {
        HENKA_AUTHORING_INVALID_ID, HENKA_AUTHORING_INVALID_ID};
    henka_authoring_modeling_report report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id edge_id;
    henka_result batch_result;
    size_t edge_slot;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (edge_slot = 0U; edge_slot < 8U; ++edge_slot)
    {
        if (henka_authoring_mesh_add_vertex(
                mesh, positions[edge_slot], (henka_vec2){0.0f, 0.0f}, 0U,
                &(henka_authoring_vertex_id){0U}) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (henka_authoring_mesh_add_face(
            mesh, face_vertices[0], 4U, 0U, true, &face_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_face(
            mesh, face_vertices[1], 4U, 0U, true, &face_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (edge_slot = 0U; edge_slot < henka_authoring_mesh_get_desc(mesh).max_edges; ++edge_slot)
    {
        if (henka_authoring_mesh_get_edge_id_at(mesh, edge_slot, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(mesh, edge_id);
            if (edge == NULL || edge->face_count != 1U)
            {
                goto cleanup;
            }
            if ((edge->vertices[0] == 1U && edge->vertices[1] == 2U) ||
                (edge->vertices[0] == 2U && edge->vertices[1] == 1U))
            {
                selected_edges[0] = edge_id;
            }
            else if ((edge->vertices[0] == 5U && edge->vertices[1] == 6U) ||
                (edge->vertices[0] == 6U && edge->vertices[1] == 5U))
            {
                selected_edges[1] = edge_id;
            }
            else if ((edge->vertices[0] == 2U && edge->vertices[1] == 3U) ||
                (edge->vertices[0] == 3U && edge->vertices[1] == 2U))
            {
                adjacent_edges[0] = edge_id;
            }
            else if ((edge->vertices[0] == 6U && edge->vertices[1] == 7U) ||
                (edge->vertices[0] == 7U && edge->vertices[1] == 6U))
            {
                adjacent_edges[1] = edge_id;
            }
        }
    }
    if (selected_edges[0] == HENKA_AUTHORING_INVALID_ID ||
        selected_edges[1] == HENKA_AUTHORING_INVALID_ID ||
        adjacent_edges[0] == HENKA_AUTHORING_INVALID_ID ||
        adjacent_edges[1] == HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    batch_result = henka_authoring_mesh_bevel_edges(
        mesh, selected_edges, 2U, 0.1f, &report);
    if (batch_result != HENKA_SUCCESS ||
        !report.changed || report.created_vertices != 4U || report.created_faces != 2U ||
        henka_authoring_mesh_get_counts(mesh).vertices != before.vertices + 4U ||
        henka_authoring_mesh_get_counts(mesh).edges != before.edges + 6U ||
        henka_authoring_mesh_get_counts(mesh).faces != before.faces + 2U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    report = (henka_authoring_modeling_report){0};
    batch_result = henka_authoring_mesh_bevel_edges(
        mesh, adjacent_edges, 2U, 0.1f, &report);
    if (batch_result == HENKA_SUCCESS ||
        report.changed)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (memcmp(&before, &after, sizeof(before)) != 0 ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("boundary edge batch bevel operation");
}

static int test_same_face_boundary_edge_batch_bevel_operation(void)
{
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    const henka_vec3 positions[4] = {
        {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    const henka_authoring_vertex_id face_vertices[4] = {1U, 2U, 3U, 4U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_edge_id selected_edges[2] = {
        HENKA_AUTHORING_INVALID_ID, HENKA_AUTHORING_INVALID_ID};
    henka_authoring_modeling_report report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id edge_id;
    size_t edge_slot;
    size_t selected_count = 0U;
    size_t vertex_slot;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (vertex_slot = 0U; vertex_slot < 4U; ++vertex_slot)
    {
        henka_authoring_vertex_id vertex_id = HENKA_AUTHORING_INVALID_ID;
        if (henka_authoring_mesh_add_vertex(
                mesh, positions[vertex_slot], (henka_vec2){0.0f, 0.0f}, 0U,
                &vertex_id) != HENKA_SUCCESS || vertex_id != vertex_slot + 1U)
        {
            goto cleanup;
        }
    }
    if (henka_authoring_mesh_add_face(
            mesh, face_vertices, 4U, 0U, true, &face_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (edge_slot = 0U; edge_slot < desc.max_edges; ++edge_slot)
    {
        const henka_authoring_edge* edge;
        if (henka_authoring_mesh_get_edge_id_at(mesh, edge_slot, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        edge = henka_authoring_mesh_get_edge(mesh, edge_id);
        if (edge == NULL || edge->face_count != 1U)
        {
            goto cleanup;
        }
        if ((edge->vertices[0] == 1U && edge->vertices[1] == 2U) ||
            (edge->vertices[0] == 2U && edge->vertices[1] == 1U) ||
            (edge->vertices[0] == 2U && edge->vertices[1] == 3U) ||
            (edge->vertices[0] == 3U && edge->vertices[1] == 2U))
        {
            if (selected_count >= 2U)
            {
                goto cleanup;
            }
            selected_edges[selected_count++] = edge_id;
        }
    }
    if (selected_count != 2U)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_bevel_edges(
            mesh, selected_edges, 2U, 2.0f, &report) == HENKA_SUCCESS ||
        report.changed)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (memcmp(&before, &after, sizeof(before)) != 0 ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_bevel_edges(
            mesh, selected_edges, 2U, 0.1f, &report) != HENKA_SUCCESS ||
        !report.changed || report.created_vertices != 4U ||
        report.created_faces != 3U ||
        henka_authoring_mesh_get_counts(mesh).vertices != before.vertices + 4U ||
        henka_authoring_mesh_get_counts(mesh).faces != before.faces + 3U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (after.edges <= before.edges)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("same-face boundary edge batch bevel operation");
}

static int test_single_quad_face_cut_operation(void)
{
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_modeling_report report = {0};
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    size_t face_slot;
    int result = 0;

    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_loop_cut_face(
            mesh, 1U, 0U, 0.5f, &new_face_id, &report) != HENKA_SUCCESS ||
        !report.changed || new_face_id == HENKA_AUTHORING_INVALID_ID ||
        report.created_vertices != 2U || report.created_faces != 1U ||
        henka_authoring_mesh_get_face(mesh, new_face_id) == NULL ||
        henka_authoring_mesh_get_counts(mesh).vertices != before.vertices + 2U ||
        henka_authoring_mesh_get_counts(mesh).edges != before.edges + 3U ||
        henka_authoring_mesh_get_counts(mesh).faces != before.faces + 1U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    for (face_slot = 0U; face_slot < henka_authoring_mesh_get_desc(mesh).max_faces; ++face_slot)
    {
        henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
        const henka_authoring_face* face;
        if (henka_authoring_mesh_get_face_id_at(mesh, face_slot, &face_id) != HENKA_SUCCESS)
        {
            continue;
        }
        face = henka_authoring_mesh_get_face(mesh, face_id);
        if (face == NULL || face->corner_count != 4U)
        {
            goto cleanup;
        }
    }
    before = henka_authoring_mesh_get_counts(mesh);
    report = (henka_authoring_modeling_report){0};
    if (henka_authoring_mesh_loop_cut_face(
            mesh, 1U, 0U, 0.0f, &new_face_id, &report) == HENKA_SUCCESS ||
        report.changed)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (memcmp(&before, &after, sizeof(before)) != 0 ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create_box(&desc, 2.0f, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    report = (henka_authoring_modeling_report){0};
    if (henka_authoring_mesh_loop_cut_face(
            mesh, 1U, 0U, 0.5f, &new_face_id, &report) == HENKA_SUCCESS ||
        report.changed)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (memcmp(&before, &after, sizeof(before)) != 0 ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("single quad face cut operation");
}

static int test_multi_cut_single_quad_operation(void)
{
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    const henka_vec3 positions[4] = {
        {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},
        {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    const henka_authoring_vertex_id face_vertices[4] = {1U, 2U, 3U, 4U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_face_id face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id last_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_modeling_report report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    size_t face_slot;
    size_t vertex_slot;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (size_t index = 0U; index < 4U; ++index)
    {
        if (henka_authoring_mesh_add_vertex(
                mesh, positions[index], (henka_vec2){0.0f, 0.0f}, 0U,
                &(henka_authoring_vertex_id){0U}) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (henka_authoring_mesh_add_face(
            mesh, face_vertices, 4U, 0U, true, &face_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_loop_cut_face_multi(
            mesh, face_id, 0U, 2U, &last_face_id, &report) != HENKA_SUCCESS ||
        !report.changed || report.created_vertices != 4U ||
        report.created_edges != 6U || report.created_faces != 2U ||
        last_face_id == HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (after.vertices != before.vertices + 4U ||
        after.edges != before.edges + 6U || after.faces != before.faces + 2U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    for (vertex_slot = 0U; vertex_slot < henka_authoring_mesh_get_desc(mesh).max_vertices;
         ++vertex_slot)
    {
        henka_authoring_vertex_id vertex_id = HENKA_AUTHORING_INVALID_ID;
        const henka_authoring_vertex* vertex;
        if (henka_authoring_mesh_get_vertex_id_at(mesh, vertex_slot, &vertex_id) != HENKA_SUCCESS ||
            vertex_id <= 4U)
        {
            continue;
        }
        vertex = henka_authoring_mesh_get_vertex(mesh, vertex_id);
        if (vertex == NULL || fabsf(fabsf(vertex->position.x) - (1.0f / 3.0f)) > 0.0001f ||
            (fabsf(vertex->position.y + 1.0f) > 0.0001f &&
             fabsf(vertex->position.y - 1.0f) > 0.0001f))
        {
            goto cleanup;
        }
    }
    for (face_slot = 0U; face_slot < henka_authoring_mesh_get_desc(mesh).max_faces; ++face_slot)
    {
        henka_authoring_face_id active_face_id = HENKA_AUTHORING_INVALID_ID;
        const henka_authoring_face* face;
        if (henka_authoring_mesh_get_face_id_at(mesh, face_slot, &active_face_id) != HENKA_SUCCESS)
        {
            continue;
        }
        face = henka_authoring_mesh_get_face(mesh, active_face_id);
        if (face == NULL || face->corner_count != 4U)
        {
            goto cleanup;
        }
    }
    before = henka_authoring_mesh_get_counts(mesh);
    report = (henka_authoring_modeling_report){0};
    if (henka_authoring_mesh_loop_cut_face_multi(
            mesh, face_id, 0U, 0U, &last_face_id, &report) == HENKA_SUCCESS ||
        report.changed)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (memcmp(&before, &after, sizeof(before)) != 0)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    last_face_id = HENKA_AUTHORING_INVALID_ID;
    report = (henka_authoring_modeling_report){0};
    if (henka_authoring_mesh_loop_cut_face_multi(
            mesh, face_id, 0U, 1U, &last_face_id, &report) == HENKA_SUCCESS ||
        report.changed || last_face_id != HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (memcmp(&before, &after, sizeof(before)) != 0)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("multi-cut single quad operation");
}

static int test_interior_edge_bevel_operation(void)
{
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    const henka_vec3 positions[6] = {
        {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        {-1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};
    const henka_vec2 uvs[6] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
        {0.0f, 1.0f}, {2.0f, 0.0f}, {2.0f, 1.0f}};
    const henka_authoring_vertex_id first_face[] = {1U, 2U, 3U, 4U};
    const henka_authoring_vertex_id second_face[] = {2U, 5U, 6U, 3U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_modeling_report report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_authoring_edge_id shared_edge = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id face_id;
    henka_authoring_edge_id edge_id;
    size_t index;
    size_t face_slot;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < 6U; ++index)
    {
        if (henka_authoring_mesh_add_vertex(
                mesh, positions[index], uvs[index], 0U, &(henka_authoring_vertex_id){0U}) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (henka_authoring_mesh_add_face(mesh, first_face, 4U, 0U, true, &face_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_face(mesh, second_face, 4U, 0U, true, &face_id) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    for (index = 0U; index < henka_authoring_mesh_get_desc(mesh).max_edges; ++index)
    {
        if (henka_authoring_mesh_get_edge_id_at(mesh, index, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(mesh, edge_id);
            if (edge != NULL && edge->face_count == 2U &&
                ((edge->vertices[0] == 2U && edge->vertices[1] == 3U) ||
                 (edge->vertices[0] == 3U && edge->vertices[1] == 2U)))
            {
                shared_edge = edge_id;
                break;
            }
        }
    }
    if (shared_edge == HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_bevel_edges(
            mesh, &shared_edge, 1U, 0.2f, &report) != HENKA_SUCCESS ||
        !report.changed || report.created_vertices != 2U || report.removed_vertices != 0U ||
        report.created_faces != 1U ||
        henka_authoring_mesh_get_counts(mesh).vertices != before.vertices + 2U ||
        henka_authoring_mesh_get_counts(mesh).edges != before.edges + 3U ||
        henka_authoring_mesh_get_counts(mesh).faces != before.faces + 1U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    for (face_slot = 0U; face_slot < henka_authoring_mesh_get_desc(mesh).max_faces; ++face_slot)
    {
        henka_authoring_face_id active_face_id = HENKA_AUTHORING_INVALID_ID;
        const henka_authoring_face* face;
        if (henka_authoring_mesh_get_face_id_at(mesh, face_slot, &active_face_id) != HENKA_SUCCESS)
        {
            continue;
        }
        face = henka_authoring_mesh_get_face(mesh, active_face_id);
        if (face == NULL || face->corner_count != 4U)
        {
            goto cleanup;
        }
    }
    before = henka_authoring_mesh_get_counts(mesh);
    report = (henka_authoring_modeling_report){0};
    if (henka_authoring_mesh_bevel_edge(mesh, shared_edge, 0.0f, &report) == HENKA_SUCCESS ||
        report.changed)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (memcmp(&before, &after, sizeof(before)) != 0 ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("interior edge bevel operation");
}

static int test_quad_strip_loop_cut_operation(void)
{
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    const henka_vec3 positions[8] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f},
        {3.0f, 0.0f, 0.0f}, {3.0f, 1.0f, 0.0f}};
    const henka_vec2 uvs[8] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},
        {2.0f, 0.0f}, {2.0f, 1.0f}, {3.0f, 0.0f}, {3.0f, 1.0f}};
    const henka_authoring_vertex_id faces[3][4] = {
        {1U, 2U, 3U, 4U}, {2U, 5U, 6U, 3U}, {5U, 7U, 8U, 6U}};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_face_id face_id;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id edge_id;
    henka_authoring_edge_id start_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id primary_cut_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_modeling_report report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    bool closed = true;
    size_t index;
    size_t face_slot;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < 8U; ++index)
    {
        if (henka_authoring_mesh_add_vertex(
                mesh, positions[index], uvs[index], 0U,
                &(henka_authoring_vertex_id){0U}) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    for (index = 0U; index < 3U; ++index)
    {
        if (henka_authoring_mesh_add_face(
                mesh, faces[index], 4U, 0U, true, &face_id) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    for (index = 0U; index < henka_authoring_mesh_get_desc(mesh).max_edges; ++index)
    {
        if (henka_authoring_mesh_get_edge_id_at(mesh, index, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        {
            const henka_authoring_edge* edge = henka_authoring_mesh_get_edge(mesh, edge_id);
            if (edge != NULL && edge->face_count == 1U &&
                ((edge->vertices[0] == 1U && edge->vertices[1] == 4U) ||
                 (edge->vertices[0] == 4U && edge->vertices[1] == 1U)))
            {
                start_edge_id = edge_id;
                break;
            }
        }
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (start_edge_id == HENKA_AUTHORING_INVALID_ID ||
        henka_authoring_mesh_loop_cut_quad_strip(
            mesh, start_edge_id, 0.5f, &new_face_id, &primary_cut_edge_id,
            &closed, &report) != HENKA_SUCCESS || closed || !report.changed ||
        report.created_vertices != 4U || report.created_edges != 7U ||
        report.created_faces != 3U || new_face_id == HENKA_AUTHORING_INVALID_ID ||
        primary_cut_edge_id == HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (after.vertices != before.vertices + 4U ||
        after.edges != before.edges + 7U || after.faces != before.faces + 3U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    for (face_slot = 0U; face_slot < henka_authoring_mesh_get_desc(mesh).max_faces; ++face_slot)
    {
        henka_authoring_face_id active_face_id = HENKA_AUTHORING_INVALID_ID;
        const henka_authoring_face* face;
        if (henka_authoring_mesh_get_face_id_at(mesh, face_slot, &active_face_id) != HENKA_SUCCESS)
        {
            continue;
        }
        face = henka_authoring_mesh_get_face(mesh, active_face_id);
        if (face == NULL || face->corner_count != 4U)
        {
            goto cleanup;
        }
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("quad strip loop cut operation");
}

static int test_closed_quad_ring_loop_cut_operation(void)
{
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    const henka_vec3 positions[8] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}};
    const henka_vec2 uvs[8] = {
        {0.00f, 0.0f}, {0.25f, 0.0f}, {0.50f, 0.0f}, {0.75f, 0.0f},
        {0.00f, 1.0f}, {0.25f, 1.0f}, {0.50f, 1.0f}, {0.75f, 1.0f}};
    const henka_authoring_vertex_id faces[4][4] = {
        {1U, 5U, 6U, 2U}, {2U, 6U, 7U, 3U},
        {3U, 7U, 8U, 4U}, {4U, 8U, 5U, 1U}};
    const henka_vec2 face_uvs[4][4] = {
        {{0.00f, 0.0f}, {0.00f, 1.0f}, {0.00f, 1.0f}, {0.00f, 0.0f}},
        {{0.00f, 0.0f}, {0.00f, 1.0f}, {0.00f, 1.0f}, {0.00f, 0.0f}},
        {{0.00f, 0.0f}, {0.00f, 1.0f}, {0.00f, 1.0f}, {0.00f, 0.0f}},
        {{0.00f, 0.0f}, {0.00f, 1.0f}, {0.00f, 1.0f}, {0.00f, 0.0f}}};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_face_id face_id;
    henka_authoring_edge_id start_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id primary_cut_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id slide_edges[4] = {
        HENKA_AUTHORING_INVALID_ID, HENKA_AUTHORING_INVALID_ID,
        HENKA_AUTHORING_INVALID_ID, HENKA_AUTHORING_INVALID_ID};
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_modeling_report report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    bool closed = false;
    size_t index;
    size_t corner;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < 8U; ++index)
    {
        if (henka_authoring_mesh_add_vertex(
                mesh, positions[index], uvs[index], 0U,
                &(henka_authoring_vertex_id){0U}) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    for (index = 0U; index < 4U; ++index)
    {
        if (henka_authoring_mesh_add_face(
                mesh, faces[index], 4U, 0U, true, &face_id) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        for (corner = 0U; corner < 4U; ++corner)
        {
            if (henka_authoring_mesh_set_face_corner_uv(
                    mesh, face_id, corner, face_uvs[index][corner]) != HENKA_SUCCESS)
            {
                goto cleanup;
            }
        }
    }
    for (index = 0U; index < henka_authoring_mesh_get_desc(mesh).max_edges; ++index)
    {
        henka_authoring_edge_id edge_id;
        const henka_authoring_edge* edge;
        if (henka_authoring_mesh_get_edge_id_at(mesh, index, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        edge = henka_authoring_mesh_get_edge(mesh, edge_id);
        if (edge != NULL && edge->face_count == 2U &&
            ((edge->vertices[0] == 1U && edge->vertices[1] == 5U) ||
             (edge->vertices[0] == 5U && edge->vertices[1] == 1U)))
        {
            start_edge_id = edge_id;
            break;
        }
    }
    before = henka_authoring_mesh_get_counts(mesh);
    {
        henka_authoring_quad_strip_step steps[8];
        size_t step_count = 0U;
        bool walk_closed = false;
        const henka_result walk_result = henka_authoring_topology_walk_quad_strip(
            mesh, start_edge_id, steps, 8U, &step_count, &walk_closed);
        if (walk_result != HENKA_SUCCESS || step_count != 4U || !walk_closed)
        {
            goto cleanup;
        }
    }
    if (start_edge_id == HENKA_AUTHORING_INVALID_ID ||
        henka_authoring_mesh_loop_cut_quad_strip(
            mesh, start_edge_id, 0.5f, &new_face_id, &primary_cut_edge_id,
            &closed, &report) != HENKA_SUCCESS || !closed || !report.changed ||
        report.created_vertices != 4U || report.created_edges != 8U ||
        report.created_faces != 4U || new_face_id == HENKA_AUTHORING_INVALID_ID ||
        primary_cut_edge_id == HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (after.vertices != before.vertices + 4U ||
        after.edges != before.edges + 8U || after.faces != before.faces + 4U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    {
        size_t slide_edge_count = 0U;
        henka_result slide_result;
        for (index = 0U; index < henka_authoring_mesh_get_desc(mesh).max_edges; ++index)
        {
            henka_authoring_edge_id edge_id;
            const henka_authoring_edge* edge;
            if (henka_authoring_mesh_get_edge_id_at(mesh, index, &edge_id) != HENKA_SUCCESS)
            {
                continue;
            }
            edge = henka_authoring_mesh_get_edge(mesh, edge_id);
            if (edge != NULL && edge->face_count == 2U &&
                edge->vertices[0] >= 9U && edge->vertices[0] <= 12U &&
                edge->vertices[1] >= 9U && edge->vertices[1] <= 12U)
            {
                if (slide_edge_count >= 4U)
                {
                    goto cleanup;
                }
                slide_edges[slide_edge_count++] = edge_id;
            }
        }
        {
            henka_authoring_edge_id ordered_edges[4];
            size_t ordered_count = 0U;
            bool ordered_closed = false;
            if (slide_edge_count != 4U ||
                henka_authoring_topology_order_edge_loop(
                    mesh, slide_edges, slide_edge_count, ordered_edges, 4U,
                    &ordered_count, &ordered_closed) != HENKA_SUCCESS ||
                ordered_count != 4U || !ordered_closed)
            {
                goto cleanup;
            }
        }
        slide_result = henka_authoring_mesh_slide_edge_loop(
            mesh, slide_edges, slide_edge_count, 0.5f, &report);
        if (slide_edge_count != 4U || slide_result != HENKA_SUCCESS ||
            !report.changed || report.created_vertices != 0U ||
            report.created_edges != 0U || report.created_faces != 0U)
        {
            goto cleanup;
        }
        for (index = 9U; index <= 12U; ++index)
        {
            const henka_authoring_vertex* vertex =
                henka_authoring_mesh_get_vertex(mesh, (henka_authoring_vertex_id)index);
            if (vertex == NULL ||
                (fabsf(vertex->position.y - 0.25f) > 0.0001f &&
                 fabsf(vertex->position.y - 0.75f) > 0.0001f))
            {
                goto cleanup;
            }
        }
        before = after;
        after = henka_authoring_mesh_get_counts(mesh);
        if (!henka_authoring_mesh_validate(mesh) ||
            memcmp(&before, &after, sizeof(before)) != 0)
        {
            goto cleanup;
        }
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("closed quad ring loop cut operation");
}

static int test_edge_loop_slide_operation(void)
{
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    const henka_vec3 positions[12] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {2.0f, 1.0f, 0.0f},
        {0.0f, 2.0f, 0.0f}, {1.0f, 2.0f, 0.0f}, {2.0f, 2.0f, 0.0f},
        {0.0f, 3.0f, 0.0f}, {1.0f, 3.0f, 0.0f}, {2.0f, 3.0f, 0.0f}};
    const henka_authoring_vertex_id faces[6][4] = {
        {1U, 2U, 5U, 4U}, {2U, 3U, 6U, 5U},
        {4U, 5U, 8U, 7U}, {5U, 6U, 9U, 8U},
        {7U, 8U, 11U, 10U}, {8U, 9U, 12U, 11U}};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_edge_id loop_edges[3] = {
        HENKA_AUTHORING_INVALID_ID,
        HENKA_AUTHORING_INVALID_ID,
        HENKA_AUTHORING_INVALID_ID};
    henka_authoring_edge_id ordered_edges_first[3];
    henka_authoring_edge_id ordered_edges_second[3];
    henka_authoring_edge_id boundary_edge = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id duplicate_edges[2];
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    henka_authoring_modeling_report report = {0};
    henka_authoring_face_id face_id;
    henka_authoring_edge_id edge_id;
    size_t row;
    size_t edge_slot;
    size_t ordered_count = 0U;
    bool ordered_closed = false;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (row = 0U; row < 12U; ++row)
    {
        if (henka_authoring_mesh_add_vertex(
                mesh, positions[row], (henka_vec2){positions[row].x, positions[row].y},
                0U, &(henka_authoring_vertex_id){0U}) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    for (row = 0U; row < 6U; ++row)
    {
        if (henka_authoring_mesh_add_face(
                mesh, faces[row], 4U, 0U, true, &face_id) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    for (row = 0U; row < 3U; ++row)
    {
        if (test_find_edge_between_vertices(
                mesh, (henka_authoring_vertex_id)(2U + row * 3U),
                (henka_authoring_vertex_id)(5U + row * 3U), &loop_edges[row]) !=
            HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (henka_authoring_topology_order_edge_loop(
            mesh, loop_edges, 3U, ordered_edges_first, 3U,
            &ordered_count, &ordered_closed) != HENKA_SUCCESS ||
        ordered_count != 3U || ordered_closed ||
        henka_authoring_topology_order_edge_loop(
            mesh, loop_edges, 3U, ordered_edges_second, 3U,
            &ordered_count, &ordered_closed) != HENKA_SUCCESS ||
        ordered_count != 3U || ordered_closed ||
        memcmp(ordered_edges_first, ordered_edges_second,
            sizeof(ordered_edges_first)) != 0)
    {
        goto cleanup;
    }
    for (edge_slot = 0U;
         edge_slot < henka_authoring_mesh_get_desc(mesh).max_edges;
         ++edge_slot)
    {
        if (henka_authoring_mesh_get_edge_id_at(mesh, edge_slot, &edge_id) ==
                HENKA_SUCCESS &&
            henka_authoring_mesh_get_edge(mesh, edge_id)->face_count == 1U)
        {
            boundary_edge = edge_id;
            break;
        }
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_slide_edge_loop(
            mesh, loop_edges, 3U, 0.5f, &report) != HENKA_SUCCESS ||
        !report.changed || report.created_vertices != 0U ||
        report.created_edges != 0U || report.created_faces != 0U)
    {
        goto cleanup;
    }
    for (row = 0U; row < 4U; ++row)
    {
        const henka_authoring_vertex* vertex = henka_authoring_mesh_get_vertex(
            mesh, (henka_authoring_vertex_id)(2U + row * 3U));
        if (vertex == NULL || fabsf(vertex->position.x - 1.5f) > 0.0001f ||
            fabsf(vertex->position.y - (float)row) > 0.0001f)
        {
            goto cleanup;
        }
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (memcmp(&before, &after, sizeof(before)) != 0 ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    duplicate_edges[0] = loop_edges[0];
    duplicate_edges[1] = loop_edges[0];
    before = after;
    if (henka_authoring_mesh_slide_edge_loop(
            mesh, duplicate_edges, 2U, 0.25f, &report) == HENKA_SUCCESS)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (memcmp(&before, &after, sizeof(before)) != 0 ||
        (boundary_edge != HENKA_AUTHORING_INVALID_ID &&
         henka_authoring_mesh_slide_edge_loop(
             mesh, &boundary_edge, 1U, 0.25f, &report) == HENKA_SUCCESS))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("edge loop slide operation");
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
    const henka_authoring_mesh_desc desc = {128U, 256U, 128U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_mesh_counts counts;
    henka_vec3 center;
    henka_vec3 extents;
    size_t slot;
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

    if (henka_authoring_mesh_create_quad_sphere(&desc, 1.0f, 4U, &mesh) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    counts = henka_authoring_mesh_get_counts(mesh);
    if (counts.vertices != 98U || counts.edges != 192U || counts.faces != 96U ||
        counts.vertices + counts.faces != counts.edges + 2U ||
        henka_authoring_mesh_get_bounds(mesh, &center, &extents) != HENKA_SUCCESS ||
        fabsf(center.x) > 0.0001f || fabsf(center.y) > 0.0001f || fabsf(center.z) > 0.0001f ||
        fabsf(extents.x - 1.0f) > 0.0001f || fabsf(extents.y - 1.0f) > 0.0001f ||
        fabsf(extents.z - 1.0f) > 0.0001f)
    {
        goto cleanup;
    }
    for (slot = 0U; slot < desc.max_vertices; ++slot)
    {
        henka_authoring_vertex_id vertex_id;
        const henka_authoring_vertex* vertex;
        if (henka_authoring_mesh_get_vertex_id_at(mesh, slot, &vertex_id) != HENKA_SUCCESS)
        {
            continue;
        }
        vertex = henka_authoring_mesh_get_vertex(mesh, vertex_id);
        if (vertex == NULL || fabsf(henka_vec3_length(vertex->position) - 1.0f) > 0.0001f)
        {
            goto cleanup;
        }
    }
    for (slot = 0U; slot < desc.max_edges; ++slot)
    {
        henka_authoring_edge_id edge_id;
        const henka_authoring_edge* edge;
        if (henka_authoring_mesh_get_edge_id_at(mesh, slot, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        edge = henka_authoring_mesh_get_edge(mesh, edge_id);
        if (edge == NULL || edge->face_count != 2U)
        {
            goto cleanup;
        }
    }
    for (slot = 0U; slot < desc.max_faces; ++slot)
    {
        henka_authoring_face_id face_id;
        const henka_authoring_face* face;
        if (henka_authoring_mesh_get_face_id_at(mesh, slot, &face_id) != HENKA_SUCCESS)
        {
            continue;
        }
        face = henka_authoring_mesh_get_face(mesh, face_id);
        if (face == NULL || face->corner_count != 4U || !face->smooth)
        {
            goto cleanup;
        }
        {
            const henka_authoring_vertex* first =
                henka_authoring_mesh_get_vertex(mesh, face->vertices[0]);
            const henka_authoring_vertex* second =
                henka_authoring_mesh_get_vertex(mesh, face->vertices[1]);
            const henka_authoring_vertex* third =
                henka_authoring_mesh_get_vertex(mesh, face->vertices[2]);
            henka_vec3 normal;
            size_t corner;
            if (first == NULL || second == NULL || third == NULL)
            {
                goto cleanup;
            }
            normal = henka_vec3_cross(
                henka_vec3_subtract(second->position, first->position),
                henka_vec3_subtract(third->position, first->position));
            if (henka_vec3_dot(normal, first->position) <= 0.0f)
            {
                goto cleanup;
            }
            for (corner = 0U; corner < face->corner_count; ++corner)
            {
                henka_vec2 uv;
                if (henka_authoring_mesh_get_face_corner_uv(
                        mesh, face_id, corner, &uv) != HENKA_SUCCESS ||
                    !isfinite(uv.x) || !isfinite(uv.y) ||
                    uv.x < 0.0f || uv.x > 1.0f ||
                    uv.y < 0.0f || uv.y > 1.0f)
                {
                    goto cleanup;
                }
            }
        }
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;

    if (henka_authoring_mesh_create_cylinder(&desc, 0.0f, 2.0f, 8U, &mesh) != HENKA_ERROR_INVALID_ARGUMENT ||
        mesh != NULL ||
        henka_authoring_mesh_create_cone(&desc, 1.0f, -2.0f, 8U, &mesh) != HENKA_ERROR_INVALID_ARGUMENT ||
        mesh != NULL ||
        henka_authoring_mesh_create_uv_sphere(&desc, 1.0f, 2U, 4U, &mesh) != HENKA_ERROR_INVALID_ARGUMENT ||
        mesh != NULL ||
        henka_authoring_mesh_create_quad_sphere(&desc, 1.0f, 0U, &mesh) != HENKA_ERROR_INVALID_ARGUMENT ||
        mesh != NULL ||
        henka_authoring_mesh_create_quad_sphere(
            &desc, 1.0f, SIZE_MAX, &mesh) != HENKA_ERROR_LIMIT ||
        mesh != NULL ||
        henka_authoring_mesh_create_quad_sphere(
            &(henka_authoring_mesh_desc){97U, 192U, 96U, 4U},
            1.0f,
            4U,
            &mesh) != HENKA_ERROR_LIMIT ||
        mesh != NULL)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("bounded primitive constructors");
}

static int test_edge_dissolve_operation(void)
{
    const henka_authoring_mesh_desc desc = {128U, 256U, 128U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_edge_id selected_edge = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_modeling_report report = {0};
    size_t slot;
    bool dissolved = false;
    int result = 0;

    if (henka_authoring_mesh_create_quad_sphere(&desc, 1.0f, 2U, &mesh) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    for (slot = 0U; slot < desc.max_edges; ++slot)
    {
        henka_authoring_edge_id edge_id;
        const henka_authoring_edge* edge;
        henka_authoring_mesh_counts before;
        henka_authoring_mesh_counts after;
        henka_result dissolve_result;
        if (henka_authoring_mesh_get_edge_id_at(mesh, slot, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        edge = henka_authoring_mesh_get_edge(mesh, edge_id);
        if (edge == NULL || edge->face_count != 2U || edge->hard)
        {
            continue;
        }
        before = henka_authoring_mesh_get_counts(mesh);
        dissolve_result = henka_authoring_mesh_dissolve_edge(mesh, edge_id, &report);
        after = henka_authoring_mesh_get_counts(mesh);
        if (dissolve_result == HENKA_SUCCESS)
        {
            const henka_authoring_face* merged_face =
                henka_authoring_mesh_get_face(mesh, report.primary_face_id);
            if (merged_face == NULL || merged_face->corner_count != 6U ||
                henka_authoring_mesh_get_edge(mesh, edge_id) != NULL ||
                !report.changed || report.removed_edges != 1U ||
                report.removed_faces != 1U || after.vertices != before.vertices ||
                after.edges + 1U != before.edges || after.faces + 1U != before.faces ||
                !henka_authoring_mesh_validate(mesh))
            {
                goto cleanup;
            }
            selected_edge = edge_id;
            dissolved = true;
            break;
        }
        if (dissolve_result != HENKA_ERROR_INVALID_ARGUMENT || report.changed ||
            after.vertices != before.vertices || after.edges != before.edges ||
            after.faces != before.faces || !henka_authoring_mesh_validate(mesh))
        {
            goto cleanup;
        }
    }
    if (!dissolved || selected_edge == HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;

    if (henka_authoring_mesh_create_quad_sphere(&desc, 1.0f, 2U, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (slot = 0U; slot < desc.max_edges; ++slot)
    {
        henka_authoring_edge_id edge_id;
        const henka_authoring_edge* edge;
        henka_authoring_mesh_counts before;
        henka_authoring_mesh_counts after;
        if (henka_authoring_mesh_get_edge_id_at(mesh, slot, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        edge = henka_authoring_mesh_get_edge(mesh, edge_id);
        if (edge == NULL || edge->face_count != 2U)
        {
            continue;
        }
        if (henka_authoring_mesh_set_edge_hard(mesh, edge_id, true) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        before = henka_authoring_mesh_get_counts(mesh);
        if (henka_authoring_mesh_dissolve_edge(mesh, edge_id, &report) != HENKA_ERROR_INVALID_ARGUMENT)
        {
            goto cleanup;
        }
        after = henka_authoring_mesh_get_counts(mesh);
        if (report.changed || after.vertices != before.vertices || after.edges != before.edges ||
            after.faces != before.faces || !henka_authoring_mesh_validate(mesh))
        {
            goto cleanup;
        }
        result = 1;
        break;
    }

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("transactional edge dissolve");
}

static int test_edge_delete_operation(void)
{
    const henka_authoring_mesh_desc desc = {128U, 256U, 128U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_edge_id edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_modeling_report report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    size_t slot;
    int result = 0;

    if (henka_authoring_mesh_create_quad_sphere(&desc, 1.0f, 2U, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (slot = 0U; slot < desc.max_edges; ++slot)
    {
        const henka_authoring_edge* edge;
        if (henka_authoring_mesh_get_edge_id_at(mesh, slot, &edge_id) != HENKA_SUCCESS)
        {
            continue;
        }
        edge = henka_authoring_mesh_get_edge(mesh, edge_id);
        if (edge != NULL && edge->face_count == 2U)
        {
            break;
        }
        edge_id = HENKA_AUTHORING_INVALID_ID;
    }
    if (edge_id == HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_delete_edge(mesh, edge_id, &report) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    if (!report.changed || report.removed_faces != 2U ||
        henka_authoring_mesh_get_edge(mesh, edge_id) != NULL ||
        after.vertices != before.vertices || after.faces + 2U != before.faces ||
        after.edges >= before.edges || !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("transactional edge delete");
}

static int test_vertex_extrude_operation(void)
{
    const henka_authoring_mesh_desc desc = {32U, 64U, 16U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id new_vertex_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_modeling_report report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    const henka_authoring_vertex* new_vertex;
    int result = 0;

    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_extrude_vertex(
            mesh, 1U, 0.5f, &new_vertex_id, &report) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    new_vertex = henka_authoring_mesh_get_vertex(mesh, new_vertex_id);
    if (!report.changed || report.created_vertices != 1U || report.created_faces != 2U ||
        new_vertex_id == HENKA_AUTHORING_INVALID_ID || new_vertex == NULL ||
        after.vertices != before.vertices + 1U || after.faces != before.faces + 2U ||
        after.edges <= before.edges || !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    {
        henka_authoring_mesh* box = NULL;
        henka_authoring_modeling_report rejected_report = {0};
        henka_authoring_mesh_counts box_before;
        henka_authoring_mesh_counts box_after;
        henka_authoring_vertex_id rejected_vertex_id = HENKA_AUTHORING_INVALID_ID;
        if (henka_authoring_mesh_create_box(&desc, 2.0f, 2.0f, 2.0f, &box) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        box_before = henka_authoring_mesh_get_counts(box);
        if (henka_authoring_mesh_extrude_vertex(
                box, 1U, 0.5f, &rejected_vertex_id, &rejected_report) == HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(box);
            goto cleanup;
        }
        box_after = henka_authoring_mesh_get_counts(box);
        if (rejected_vertex_id != HENKA_AUTHORING_INVALID_ID || rejected_report.changed ||
            box_after.vertices != box_before.vertices || box_after.edges != box_before.edges ||
            box_after.faces != box_before.faces || !henka_authoring_mesh_validate(box))
        {
            henka_authoring_mesh_destroy(box);
            goto cleanup;
        }
        henka_authoring_mesh_destroy(box);
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("transactional vertex extrude");
}

static int test_loose_vertex_extrude_operation(void)
{
    const henka_authoring_mesh_desc desc = {8U, 8U, 4U, 4U};
    const henka_vec3 direction = {0.0f, 2.0f, 0.0f};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id source_vertex_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id new_vertex_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id new_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_modeling_report report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    const henka_authoring_vertex* source_vertex;
    const henka_authoring_vertex* new_vertex;
    const henka_authoring_edge* new_edge;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){1.0f, 2.0f, 3.0f}, (henka_vec2){0.25f, 0.75f}, 4U,
            &source_vertex_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_extrude_loose_vertex(
            mesh, source_vertex_id, direction, 0.5f,
            &new_vertex_id, &new_edge_id, &report) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    source_vertex = henka_authoring_mesh_get_vertex(mesh, source_vertex_id);
    new_vertex = henka_authoring_mesh_get_vertex(mesh, new_vertex_id);
    new_edge = henka_authoring_mesh_get_edge(mesh, new_edge_id);
    if (!report.changed || report.created_vertices != 1U ||
        report.created_edges != 1U || report.created_faces != 0U ||
        source_vertex == NULL || new_vertex == NULL || new_edge == NULL ||
        source_vertex_id == new_vertex_id || new_vertex_id == HENKA_AUTHORING_INVALID_ID ||
        new_edge_id == HENKA_AUTHORING_INVALID_ID ||
        after.vertices != before.vertices + 1U || after.edges != before.edges + 1U ||
        after.faces != before.faces ||
        new_vertex->position.x != 1.0f || new_vertex->position.y != 2.5f ||
        new_vertex->position.z != 3.0f || new_vertex->uv.x != 0.25f ||
        new_vertex->uv.y != 0.75f || new_vertex->material_region != 4U ||
        new_edge->face_count != 0U || !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    {
        henka_authoring_mesh* rejected_mesh = NULL;
        henka_authoring_vertex_id rejected_source_id = HENKA_AUTHORING_INVALID_ID;
        henka_authoring_vertex_id rejected_vertex_id = HENKA_AUTHORING_INVALID_ID;
        henka_authoring_edge_id rejected_edge_id = HENKA_AUTHORING_INVALID_ID;
        henka_authoring_modeling_report rejected_report = {0};
        henka_authoring_mesh_counts rejected_before;
        henka_authoring_mesh_counts rejected_after;
        if (henka_authoring_mesh_create(&desc, &rejected_mesh) != HENKA_SUCCESS ||
            henka_authoring_mesh_add_vertex(
                rejected_mesh, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f},
                0U, &rejected_source_id) != HENKA_SUCCESS)
        {
            henka_authoring_mesh_destroy(rejected_mesh);
            goto cleanup;
        }
        rejected_before = henka_authoring_mesh_get_counts(rejected_mesh);
        if (henka_authoring_mesh_extrude_loose_vertex(
                rejected_mesh, rejected_source_id, (henka_vec3){0.0f, 0.0f, 0.0f},
                0.5f, &rejected_vertex_id, &rejected_edge_id, &rejected_report) == HENKA_SUCCESS ||
            rejected_vertex_id != HENKA_AUTHORING_INVALID_ID ||
            rejected_edge_id != HENKA_AUTHORING_INVALID_ID || rejected_report.changed ||
            (rejected_after = henka_authoring_mesh_get_counts(rejected_mesh)).vertices != rejected_before.vertices ||
            rejected_after.edges != rejected_before.edges || rejected_after.faces != rejected_before.faces ||
            !henka_authoring_mesh_validate(rejected_mesh))
        {
            henka_authoring_mesh_destroy(rejected_mesh);
            goto cleanup;
        }
        henka_authoring_mesh_destroy(rejected_mesh);
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("transactional loose vertex extrude");
}

static int test_vertex_extrude_boundary_fan_operation(void)
{
    const henka_authoring_mesh_desc desc = {16U, 32U, 8U, 4U};
    const henka_vec3 positions[6] = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}};
    const henka_vec2 uvs[6] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
        {0.0f, 1.0f}, {-1.0f, 1.0f}, {-1.0f, 0.0f}};
    const henka_authoring_vertex_id face_vertices[2][4] = {
        {1U, 2U, 3U, 4U}, {1U, 4U, 5U, 6U}};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertices[6];
    henka_authoring_vertex_id new_vertex_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_modeling_report report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    const henka_authoring_vertex* new_vertex;
    size_t index;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < 6U; ++index)
    {
        if (henka_authoring_mesh_add_vertex(
                mesh, positions[index], uvs[index], 0U, &vertices[index]) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    for (index = 0U; index < 2U; ++index)
    {
        if (henka_authoring_mesh_add_face(
                mesh, face_vertices[index], 4U, 0U, true, &(henka_authoring_face_id){0U}) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (before.vertices != 6U || before.edges != 7U || before.faces != 2U ||
        henka_authoring_mesh_extrude_vertex(
            mesh, vertices[0], 0.5f, &new_vertex_id, &report) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    new_vertex = henka_authoring_mesh_get_vertex(mesh, new_vertex_id);
    if (!report.changed || report.created_vertices != 1U || report.created_faces != 2U ||
        new_vertex_id == HENKA_AUTHORING_INVALID_ID || new_vertex == NULL ||
        fabsf(new_vertex->position.z - 0.5f) > 0.0001f ||
        after.vertices != before.vertices + 1U || after.faces != before.faces + 2U ||
        after.edges != before.edges + 3U || !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("transactional vertex boundary fan extrude");
}

static int test_loose_edge_extrude_operation(void)
{
    const henka_authoring_mesh_desc desc = {16U, 16U, 8U, 4U};
    const henka_vec3 direction = {0.0f, 2.0f, 0.0f};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertices[2] = {
        HENKA_AUTHORING_INVALID_ID, HENKA_AUTHORING_INVALID_ID};
    henka_authoring_edge_id source_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id new_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_modeling_report report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    const henka_authoring_vertex* first_new_vertex;
    const henka_authoring_vertex* second_new_vertex;
    const henka_authoring_edge* source_edge;
    const henka_authoring_edge* new_edge;
    const henka_authoring_face* new_face;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 5U,
            &vertices[0]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){1.0f, 0.0f, 0.0f}, (henka_vec2){1.0f, 0.0f}, 5U,
            &vertices[1]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_edge(
            mesh, vertices[0], vertices[1], false, &source_edge_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_extrude_loose_edge(
            mesh, source_edge_id, direction, 0.5f, &new_edge_id, &new_face_id,
            &report) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    first_new_vertex = henka_authoring_mesh_get_vertex(mesh, 3U);
    second_new_vertex = henka_authoring_mesh_get_vertex(mesh, 4U);
    source_edge = henka_authoring_mesh_get_edge(mesh, source_edge_id);
    new_edge = henka_authoring_mesh_get_edge(mesh, new_edge_id);
    new_face = henka_authoring_mesh_get_face(mesh, new_face_id);
    if (!report.changed || report.created_vertices != 2U ||
        report.created_edges != 3U || report.created_faces != 1U ||
        new_edge_id == HENKA_AUTHORING_INVALID_ID ||
        new_face_id == HENKA_AUTHORING_INVALID_ID || first_new_vertex == NULL ||
        second_new_vertex == NULL || source_edge == NULL || new_edge == NULL ||
        new_face == NULL || after.vertices != before.vertices + 2U ||
        after.edges != before.edges + 3U || after.faces != before.faces + 1U ||
        first_new_vertex->position.x != 0.0f || first_new_vertex->position.y != 0.5f ||
        first_new_vertex->position.z != 0.0f || second_new_vertex->position.x != 1.0f ||
        second_new_vertex->position.y != 0.5f || second_new_vertex->position.z != 0.0f ||
        first_new_vertex->material_region != 5U || second_new_vertex->material_region != 5U ||
        source_edge->face_count != 1U || new_edge->face_count != 1U ||
        new_face->corner_count != 4U || !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("transactional loose edge extrude");
}

static int test_boundary_edge_extrude_operation(void)
{
    const henka_authoring_mesh_desc desc = {16U, 32U, 8U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_mesh* rejected_mesh = NULL;
    henka_authoring_face_id source_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id source_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id new_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id new_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_modeling_report report = {0};
    henka_authoring_modeling_report rejected_report = {0};
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    const henka_authoring_edge* source_edge;
    const henka_authoring_edge* new_edge;
    const henka_authoring_face* new_face;
    const henka_authoring_vertex* new_first;
    const henka_authoring_vertex* new_second;
    size_t face_slot;
    int result = 0;

    if (henka_authoring_mesh_create_plane(&desc, 2.0f, 2.0f, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (face_slot = 0U; face_slot < desc.max_faces; ++face_slot)
    {
        if (henka_authoring_mesh_get_face_id_at(mesh, face_slot, &source_face_id) == HENKA_SUCCESS)
        {
            break;
        }
    }
    if (source_face_id == HENKA_AUTHORING_INVALID_ID)
    {
        goto cleanup;
    }
    {
        const henka_authoring_face* source_face =
            henka_authoring_mesh_get_face(mesh, source_face_id);
        if (source_face == NULL || source_face->corner_count != 4U)
        {
            goto cleanup;
        }
        source_edge_id = source_face->edges[0];
    }
    if (henka_authoring_mesh_set_face_material_region(mesh, source_face_id, 7U) != HENKA_SUCCESS ||
        henka_authoring_mesh_set_edge_hard(mesh, source_edge_id, true) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    source_edge = henka_authoring_mesh_get_edge(mesh, source_edge_id);
    if (source_edge == NULL || source_edge->face_count != 1U)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_extrude_edge(
            mesh, source_edge_id, 0.5f, &new_edge_id, &new_face_id, &report) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    after = henka_authoring_mesh_get_counts(mesh);
    source_edge = henka_authoring_mesh_get_edge(mesh, source_edge_id);
    new_edge = henka_authoring_mesh_get_edge(mesh, new_edge_id);
    new_face = henka_authoring_mesh_get_face(mesh, new_face_id);
    new_first = new_edge == NULL ? NULL : henka_authoring_mesh_get_vertex(
        mesh, new_edge->vertices[0]);
    new_second = new_edge == NULL ? NULL : henka_authoring_mesh_get_vertex(
        mesh, new_edge->vertices[1]);
    if (!report.changed || report.created_vertices != 2U || report.created_edges != 3U ||
        report.created_faces != 1U || new_edge_id == HENKA_AUTHORING_INVALID_ID ||
        new_face_id == HENKA_AUTHORING_INVALID_ID || source_edge == NULL || new_edge == NULL ||
        new_face == NULL || new_first == NULL || new_second == NULL ||
        after.vertices != before.vertices + 2U || after.edges != before.edges + 3U ||
        after.faces != before.faces + 1U || source_edge->face_count != 1U ||
        new_edge->face_count != 2U || !new_edge->hard || new_face->corner_count != 4U ||
        new_face->material_region != 7U ||
        fabsf(new_first->position.y + 0.5f) > 0.0001f ||
        fabsf(new_second->position.y + 0.5f) > 0.0001f ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;

    if (henka_authoring_mesh_create_box(&desc, 2.0f, 2.0f, 2.0f, &rejected_mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before = henka_authoring_mesh_get_counts(rejected_mesh);
    if (henka_authoring_mesh_extrude_edge(
            rejected_mesh, 1U, 0.5f, &new_edge_id, &new_face_id, &rejected_report) == HENKA_SUCCESS ||
        rejected_report.changed ||
        (after = henka_authoring_mesh_get_counts(rejected_mesh), memcmp(&before, &after, sizeof(before)) != 0) ||
        !henka_authoring_mesh_validate(rejected_mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(rejected_mesh);
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("transactional boundary edge extrude");
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

static int test_persistence_versions_and_malformed(void)
{
    const char* path = "build/test_tmp/authoring_nested/authoring_mesh_versions.bin";
    const henka_authoring_mesh_desc desc = {3U, 3U, 1U, 3U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_mesh* source = NULL;
    henka_authoring_vertex_id current_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id source_id;
    const henka_authoring_vertex* vertex;
    FILE* file;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){9.0f, 0.0f, 0.0f},
            (henka_vec2){0.0f, 0.0f},
            0U,
            &current_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_create(&desc, &source) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            source,
            (henka_vec3){1.0f, 0.0f, 0.0f},
            (henka_vec2){0.0f, 0.0f},
            0U,
            &source_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            source,
            (henka_vec3){2.0f, 0.0f, 0.0f},
            (henka_vec2){1.0f, 0.0f},
            0U,
            &source_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_save_file(source, path) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_authoring_mesh_destroy(source);
    source = NULL;

    file = test_open_file(path, "rb+");
    if (file == NULL)
    {
        goto cleanup;
    }
    if (fseek(file, 76L, SEEK_SET) != 0 || !test_write_u32(file, 1U))
    {
        fclose(file);
        goto cleanup;
    }
    if (fclose(file) != 0)
    {
        goto cleanup;
    }
    {
        const henka_result duplicate_result = henka_authoring_mesh_load_file(mesh, path);
        vertex = henka_authoring_mesh_get_vertex(mesh, current_id);
        if (duplicate_result == HENKA_SUCCESS || henka_authoring_mesh_get_counts(mesh).vertices != 1U ||
            vertex == NULL || fabsf(vertex->position.x - 9.0f) > 0.0001f)
        {
            goto cleanup;
        }
    }

    if (!test_write_legacy_fixture(path, 2U))
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_load_file(mesh, path) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (!henka_authoring_mesh_validate(mesh) ||
        henka_authoring_mesh_get_counts(mesh).vertices != 3U ||
        henka_authoring_mesh_get_counts(mesh).edges != 3U ||
        henka_authoring_mesh_get_counts(mesh).faces != 1U ||
        henka_authoring_mesh_get_vertex(mesh, 3U) == NULL)
    {
        goto cleanup;
    }
    if (!test_write_legacy_fixture(path, 3U))
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_load_file(mesh, path) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }

    if (henka_authoring_mesh_save_file(mesh, path) != HENKA_SUCCESS ||
        !test_append_byte(path, 0xa5U) ||
        henka_authoring_mesh_set_vertex_position(mesh, 1U, (henka_vec3){8.0f, 0.0f, 0.0f}) != HENKA_SUCCESS ||
        henka_authoring_mesh_load_file(mesh, path) == HENKA_SUCCESS ||
        (vertex = henka_authoring_mesh_get_vertex(mesh, 1U)) == NULL ||
        fabsf(vertex->position.x - 8.0f) > 0.0001f)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    remove(path);
    henka_authoring_mesh_destroy(source);
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("persistence versions/malformed transaction");
}

static int test_loose_component_representation_and_persistence(void)
{
    const char* path = "build/test_tmp/authoring_loose_components.hams";
    henka_authoring_mesh_desc desc = henka_authoring_mesh_desc_default();
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_mesh* loaded = NULL;
    henka_authoring_vertex_id first_vertex = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id second_vertex = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id extra_vertex = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id loaded_edge_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id loaded_face_id = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_mesh_counts counts;
    int result = 0;

    desc.max_vertices = 4U;
    desc.max_edges = 4U;
    desc.max_faces = 2U;
    desc.max_face_corners = 4U;
    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_vec2){0.0f, 0.0f},
            0U,
            &first_vertex) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){1.0f, 0.0f, 0.0f},
            (henka_vec2){1.0f, 0.0f},
            0U,
            &second_vertex) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_add_edge(
            mesh, first_vertex, second_vertex, true, &edge_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    counts = henka_authoring_mesh_get_counts(mesh);
    if (!henka_authoring_mesh_validate(mesh) || counts.vertices != 2U ||
        counts.edges != 1U || counts.faces != 0U ||
        henka_authoring_mesh_get_edge(mesh, edge_id) == NULL ||
        !henka_authoring_mesh_get_edge(mesh, edge_id)->hard ||
        henka_authoring_mesh_get_edge_face_count(mesh, edge_id) != 0U ||
        henka_authoring_mesh_edge_is_boundary(mesh, edge_id) ||
        henka_authoring_mesh_remove_vertex(mesh, first_vertex) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        henka_authoring_mesh_add_edge(
            mesh, first_vertex, second_vertex, false, &loaded_edge_id) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        henka_authoring_mesh_add_edge(
            mesh, first_vertex, first_vertex, false, &loaded_edge_id) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        henka_authoring_mesh_add_edge(
            mesh, first_vertex, 999U, false, &loaded_edge_id) !=
            HENKA_ERROR_INVALID_ARGUMENT)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_save_file(mesh, path) != HENKA_SUCCESS ||
        henka_authoring_mesh_load_file_new(path, &loaded) != HENKA_SUCCESS ||
        loaded == NULL || !henka_authoring_mesh_validate(loaded) ||
        henka_authoring_mesh_get_counts(loaded).vertices != 2U ||
        henka_authoring_mesh_get_counts(loaded).edges != 1U ||
        henka_authoring_mesh_get_counts(loaded).faces != 0U ||
        test_find_edge_between_vertices(
            loaded, first_vertex, second_vertex, &loaded_edge_id) !=
            HENKA_SUCCESS ||
        henka_authoring_mesh_get_edge_face_count(loaded, loaded_edge_id) != 0U ||
        henka_authoring_mesh_add_vertex(
            loaded,
            (henka_vec3){2.0f, 0.0f, 0.0f},
            (henka_vec2){0.0f, 0.0f},
            0U,
            &extra_vertex) != HENKA_SUCCESS ||
        extra_vertex <= second_vertex ||
        henka_authoring_mesh_add_face(
            loaded,
            (const henka_authoring_vertex_id[]){
                first_vertex, second_vertex, extra_vertex},
            3U,
            0U,
            false,
            &loaded_face_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_get_edge_face_count(loaded, loaded_edge_id) != 1U ||
        !henka_authoring_mesh_validate(loaded) ||
        henka_authoring_mesh_remove_face(loaded, loaded_face_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_get_edge(loaded, loaded_edge_id) != NULL ||
        !henka_authoring_mesh_validate(loaded))
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_remove_edge(mesh, edge_id) != HENKA_SUCCESS ||
        henka_authoring_mesh_get_edge(mesh, edge_id) != NULL ||
        !henka_authoring_mesh_validate(mesh) ||
        henka_authoring_mesh_remove_vertex(mesh, first_vertex) != HENKA_SUCCESS ||
        henka_authoring_mesh_remove_vertex(mesh, second_vertex) != HENKA_SUCCESS ||
        henka_authoring_mesh_get_counts(mesh).vertices != 0U ||
        henka_authoring_mesh_get_counts(mesh).edges != 0U ||
        henka_authoring_mesh_get_counts(mesh).faces != 0U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    remove(path);
    henka_authoring_mesh_destroy(loaded);
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("loose component representation/persistence");
}

static int test_hams_loose_topology_versioning(void)
{
    const char* path = "build/test_tmp/authoring_hams_loose_versioning.hams";
    const henka_authoring_mesh_desc desc = {4U, 4U, 1U, 4U};
    henka_authoring_mesh* destination = NULL;
    henka_authoring_mesh* surface = NULL;
    henka_authoring_mesh* loose = NULL;
    henka_authoring_mesh* loaded = NULL;
    henka_authoring_vertex_id destination_vertex = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id surface_vertices[4] = {0U, 0U, 0U, 0U};
    henka_authoring_vertex_id loose_first = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id loose_second = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_vertex_id loose_isolated = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_edge_id loose_edge = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id surface_face = HENKA_AUTHORING_INVALID_ID;
    FILE* file = NULL;
    unsigned char header[8] = {0U};
    const henka_authoring_vertex* vertex;
    const char* stage = "create";
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &destination) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            destination,
            (henka_vec3){42.0f, 0.0f, 0.0f},
            (henka_vec2){0.0f, 0.0f},
            0U,
            &destination_vertex) != HENKA_SUCCESS ||
        henka_authoring_mesh_create(&desc, &surface) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    stage = "surface construction and v4 load";
    for (size_t index = 0U; index < 4U; ++index)
    {
        const henka_vec3 positions[4] = {
            {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
            {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
        const henka_vec2 uvs[4] = {
            {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
        if (henka_authoring_mesh_add_vertex(
                surface, positions[index], uvs[index], 0U,
                &surface_vertices[index]) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (henka_authoring_mesh_add_face(
            surface, surface_vertices, 4U, 0U, true, &surface_face) != HENKA_SUCCESS)
    {
        stage = "surface face";
        goto cleanup;
    }
    if (henka_authoring_mesh_save_file(surface, path) != HENKA_SUCCESS)
    {
        stage = "surface save";
        goto cleanup;
    }
    if (!test_patch_hams_version(path, 4U))
    {
        stage = "surface v4 patch";
        goto cleanup;
    }
    if (henka_authoring_mesh_load_file(destination, path) != HENKA_SUCCESS)
    {
        stage = "surface v4 load result";
        goto cleanup;
    }
    if (henka_authoring_mesh_get_counts(destination).faces != 1U ||
        !henka_authoring_mesh_validate(destination))
    {
        stage = "surface v4 load state";
        goto cleanup;
    }

    stage = "loose construction";
    if (henka_authoring_mesh_create(&desc, &loose) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            loose,
            (henka_vec3){2.0f, 0.0f, 0.0f},
            (henka_vec2){0.0f, 0.0f},
            7U,
            &loose_first) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            loose,
            (henka_vec3){3.0f, 0.0f, 0.0f},
            (henka_vec2){1.0f, 0.0f},
            7U,
            &loose_second) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            loose,
            (henka_vec3){8.0f, 0.0f, 0.0f},
            (henka_vec2){0.5f, 0.5f},
            9U,
            &loose_isolated) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_edge(
            loose, loose_first, loose_second, true, &loose_edge) != HENKA_SUCCESS ||
        henka_authoring_mesh_save_file(loose, path) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    stage = "v5 header";
    file = test_open_file(path, "rb");
    if (file == NULL || fread(header, sizeof(header), 1U, file) != 1U ||
        memcmp(header, "HAMS\x05\0\0\0", sizeof(header)) != 0)
    {
        if (file != NULL) fclose(file);
        file = NULL;
        goto cleanup;
    }
    if (fclose(file) != 0)
    {
        file = NULL;
        goto cleanup;
    }
    file = NULL;
    stage = "v5 loose load";
    if (henka_authoring_mesh_load_file_new(path, &loaded) != HENKA_SUCCESS ||
        loaded == NULL ||
        henka_authoring_mesh_get_edge_face_count(loaded, loose_edge) != 0U ||
        henka_authoring_mesh_get_vertex(loaded, loose_first) == NULL ||
        henka_authoring_mesh_get_vertex(loaded, loose_isolated) == NULL ||
        fabsf(henka_authoring_mesh_get_vertex(loaded, loose_isolated)->position.x - 8.0f) > 0.0001f ||
        !henka_authoring_mesh_validate(loaded) ||
        henka_authoring_mesh_add_vertex(
            loaded,
            (henka_vec3){4.0f, 0.0f, 0.0f},
            (henka_vec2){2.0f, 0.0f},
            7U,
            &destination_vertex) != HENKA_SUCCESS ||
        destination_vertex <= loose_second)
    {
        goto cleanup;
    }

    stage = "v4 loose rejection";
    if (!test_patch_hams_version(path, 4U) ||
        henka_authoring_mesh_load_file(destination, path) == HENKA_SUCCESS ||
        henka_authoring_mesh_get_counts(destination).faces != 1U ||
        (vertex = henka_authoring_mesh_get_vertex(destination, 1U)) == NULL ||
        fabsf(vertex->position.x - 0.0f) > 0.0001f ||
        !henka_authoring_mesh_validate(destination))
    {
        goto cleanup;
    }

    stage = "malformed v5 construction";
    if (henka_authoring_mesh_save_file(loose, path) != HENKA_SUCCESS ||
        !test_patch_hams_version(path, 5U) ||
        !test_patch_hams_u32_at(path, 48L + (3L * 28L) + 20L, 1U))
    {
        goto cleanup;
    }
    stage = "malformed v5 rejection";
    if (henka_authoring_mesh_load_file(destination, path) == HENKA_SUCCESS ||
        henka_authoring_mesh_get_counts(destination).faces != 1U ||
        (vertex = henka_authoring_mesh_get_vertex(destination, 1U)) == NULL ||
        fabsf(vertex->position.x - 0.0f) > 0.0001f ||
        !henka_authoring_mesh_validate(destination))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    if (file != NULL) fclose(file);
    remove(path);
    henka_authoring_mesh_destroy(loaded);
    henka_authoring_mesh_destroy(loose);
    henka_authoring_mesh_destroy(surface);
    henka_authoring_mesh_destroy(destination);
    return result ? 1 : fail(stage);
}

int main(void)
{
    return test_topology_and_evaluation() && test_evaluation_failure_clears_output_counts() &&
        test_face_operation_outputs_fail_closed() &&
        test_primitive_constructor_outputs_fail_closed() &&
        test_mesh_create_output_fails_closed() &&
        test_topology_add_outputs_fail_closed() &&
        test_query_outputs_fail_closed() &&
        test_rejection_and_tombstones() &&
        test_history_and_persistence() && test_modeling_operations() && test_face_flip_operation() &&
        test_vertex_merge_operations() &&
        test_vertex_topology_operations() && test_vertex_bevel_operations() &&
        test_boundary_edge_bevel_operation() && test_boundary_edge_batch_bevel_operation() &&
        test_same_face_boundary_edge_batch_bevel_operation() &&
        test_single_quad_face_cut_operation() &&
        test_multi_cut_single_quad_operation() &&
        test_interior_edge_bevel_operation() && test_quad_strip_loop_cut_operation() &&
        test_closed_quad_ring_loop_cut_operation() &&
        test_edge_loop_slide_operation() &&
        test_uv_authoring() &&
        test_modeling_material_region_and_uv_continuity() &&
        test_bounded_primitive_constructors() &&
        test_edge_dissolve_operation() &&
        test_edge_delete_operation() &&
        test_vertex_extrude_operation() &&
        test_loose_vertex_extrude_operation() &&
        test_vertex_extrude_boundary_fan_operation() &&
        test_loose_edge_extrude_operation() &&
        test_boundary_edge_extrude_operation() &&
        test_logical_identity_reuse_and_history() &&
        test_persistence_versions_and_malformed() &&
        test_loose_component_representation_and_persistence() &&
        test_hams_loose_topology_versioning() ? 0 : 1;
}
