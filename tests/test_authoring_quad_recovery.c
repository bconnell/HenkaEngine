#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <henka/authoring_mesh.h>

static henka_result make_two_triangle_quad(
    bool uv_seam,
    henka_authoring_mesh** out_mesh)
{
    henka_authoring_mesh_desc desc = {16U, 32U, 16U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id v[4];
    henka_authoring_face_id first_face;
    henka_authoring_face_id second_face = 0U;
    henka_authoring_vertex_id first[3];
    henka_authoring_vertex_id second[3];
    henka_result result;

    if (out_mesh == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_mesh = NULL;

    result = henka_authoring_mesh_create(&desc, &mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_authoring_mesh_add_vertex(
        mesh,
        (henka_vec3){-1.0f, 0.0f, -1.0f},
        (henka_vec2){0.0f, 0.0f},
        0U,
        &v[0]);
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){1.0f, 0.0f, -1.0f},
            (henka_vec2){1.0f, 0.0f},
            0U,
            &v[1]);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){1.0f, 0.0f, 1.0f},
            (henka_vec2){1.0f, 1.0f},
            0U,
            &v[2]);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){-1.0f, 0.0f, 1.0f},
            (henka_vec2){0.0f, 1.0f},
            0U,
            &v[3]);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_authoring_mesh_destroy(mesh);
        return result;
    }

    first[0] = v[0];
    first[1] = v[1];
    first[2] = v[2];

    second[0] = v[0];
    second[1] = v[2];
    second[2] = v[3];

    result = henka_authoring_mesh_add_face(
        mesh,
        first,
        3U,
        0U,
        true,
        &first_face);

    if (result == HENKA_SUCCESS)
    {
        result = henka_authoring_mesh_add_face(
            mesh,
            second,
            3U,
            0U,
            true,
            &second_face);
    }

    if (result == HENKA_SUCCESS && uv_seam)
    {
        result = henka_authoring_mesh_set_face_corner_uv(
            mesh,
            second_face,
            0U,
            (henka_vec2){0.25f, 0.0f});
    }

    if (result != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh))
    {
        henka_authoring_mesh_destroy(mesh);
        return result == HENKA_SUCCESS
            ? HENKA_ERROR_UNKNOWN
            : result;
    }

    *out_mesh = mesh;
    return HENKA_SUCCESS;
}

static void test_recovers_square_quad(void)
{
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_mesh_counts counts;
    size_t merged = 0U;
    uint32_t face_id;
    const henka_authoring_face* recovered = NULL;
    henka_authoring_render_vertex render_vertices[8];
    uint32_t render_indices[12];
    henka_authoring_render_data render_data;

    assert(make_two_triangle_quad(false, &mesh) == HENKA_SUCCESS);

    assert(henka_authoring_mesh_recover_quads(
        mesh,
        0.94f,
        1.10f,
        0.0001f,
        &merged) == HENKA_SUCCESS);

    assert(merged == 1U);

    counts = henka_authoring_mesh_get_counts(mesh);
    assert(counts.vertices == 4U);
    assert(counts.faces == 1U);
    assert(counts.edges == 4U);

    for (face_id = 1U;
         face_id <= HENKA_AUTHORING_MESH_HARD_MAX_FACES;
         ++face_id)
    {
        recovered = henka_authoring_mesh_get_face(mesh, face_id);
        if (recovered != NULL)
        {
            break;
        }
    }

    assert(recovered != NULL);
    assert(recovered->corner_count == 4U);

    render_data.vertices = render_vertices;
    render_data.vertex_capacity =
        sizeof(render_vertices) / sizeof(render_vertices[0]);
    render_data.vertex_count = 0U;
    render_data.indices = render_indices;
    render_data.index_capacity =
        sizeof(render_indices) / sizeof(render_indices[0]);
    render_data.index_count = 0U;

    assert(henka_authoring_mesh_evaluate(
        mesh,
        &render_data) == HENKA_SUCCESS);

    assert(render_data.index_count == 6U);

    henka_authoring_mesh_destroy(mesh);
}

static void test_preserves_uv_seam(void)
{
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_mesh_counts counts;
    size_t merged = 0U;

    assert(make_two_triangle_quad(true, &mesh) == HENKA_SUCCESS);

    assert(henka_authoring_mesh_recover_quads(
        mesh,
        0.94f,
        1.10f,
        0.0001f,
        &merged) == HENKA_SUCCESS);

    counts = henka_authoring_mesh_get_counts(mesh);

    assert(merged == 0U);
    assert(counts.faces == 2U);

    henka_authoring_mesh_destroy(mesh);
}

static int run_probe(const char* path)
{
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_mesh_counts before;
    henka_authoring_mesh_counts after;
    size_t merged = 0U;
    size_t triangles_before = 0U;
    size_t quads_before = 0U;
    size_t triangles_after = 0U;
    size_t quads_after = 0U;
    uint32_t face_id;

    if (henka_authoring_mesh_load_file_new(
            path,
            &mesh) != HENKA_SUCCESS)
    {
        fprintf(stderr, "probe could not load %s\n", path);
        return 2;
    }

    before = henka_authoring_mesh_get_counts(mesh);

    for (face_id = 1U;
         face_id <= HENKA_AUTHORING_MESH_HARD_MAX_FACES;
         ++face_id)
    {
        const henka_authoring_face* face =
            henka_authoring_mesh_get_face(mesh, face_id);

        if (face == NULL)
        {
            continue;
        }

        triangles_before += face->corner_count == 3U ? 1U : 0U;
        quads_before += face->corner_count == 4U ? 1U : 0U;
    }

    if (henka_authoring_mesh_recover_quads(
            mesh,
            0.94f,
            1.10f,
            0.0001f,
            &merged) != HENKA_SUCCESS)
    {
        fprintf(stderr, "probe quad recovery failed\n");
        henka_authoring_mesh_destroy(mesh);
        return 3;
    }

    after = henka_authoring_mesh_get_counts(mesh);

    for (face_id = 1U;
         face_id <= HENKA_AUTHORING_MESH_HARD_MAX_FACES;
         ++face_id)
    {
        const henka_authoring_face* face =
            henka_authoring_mesh_get_face(mesh, face_id);

        if (face == NULL)
        {
            continue;
        }

        triangles_after += face->corner_count == 3U ? 1U : 0U;
        quads_after += face->corner_count == 4U ? 1U : 0U;
    }

    printf(
        "QUAD_RECOVERY_PROBE "
        "before_vertices=%zu "
        "before_edges=%zu "
        "before_faces=%zu "
        "before_triangles=%zu "
        "before_quads=%zu "
        "merged_pairs=%zu "
        "after_vertices=%zu "
        "after_edges=%zu "
        "after_faces=%zu "
        "after_triangles=%zu "
        "after_quads=%zu "
        "after_quad_ratio=%.4f\n",
        before.vertices,
        before.edges,
        before.faces,
        triangles_before,
        quads_before,
        merged,
        after.vertices,
        after.edges,
        after.faces,
        triangles_after,
        quads_after,
        after.faces > 0U
            ? (double)quads_after / (double)after.faces
            : 0.0);

    henka_authoring_mesh_destroy(mesh);

    return merged > 0U ? 0 : 4;
}

int main(int argc, char** argv)
{
    if (argc == 2)
    {
        return run_probe(argv[1]);
    }

    test_recovers_square_quad();
    test_preserves_uv_seam();

    puts("authoring quad recovery tests passed");
    return 0;
}
