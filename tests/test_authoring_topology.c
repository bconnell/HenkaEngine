#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <henka/authoring_mesh.h>
#include <henka/authoring_topology.h>
#include <henka/persistence.h>

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

static int topology_test_write_u32(FILE* file, uint32_t value)
{
    const unsigned char bytes[4] = {
        (unsigned char)(value & 0xffU),
        (unsigned char)((value >> 8U) & 0xffU),
        (unsigned char)((value >> 16U) & 0xffU),
        (unsigned char)((value >> 24U) & 0xffU)};
    return fwrite(bytes, sizeof(bytes), 1U, file) == 1U;
}

static int topology_test_write_f32(FILE* file, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return topology_test_write_u32(file, bits);
}

static int topology_test_write_hams_v7_repeated_vertex_fixture(const char* path)
{
    static const henka_vec3 positions[6] = {
        {1.0f, 0.0f, 0.0f},
        {0.5f, 0.8660254f, 0.0f},
        {-0.5f, 0.8660254f, 0.0f},
        {-1.0f, 0.0f, 0.0f},
        {-0.5f, -0.8660254f, 0.0f},
        {0.5f, -0.8660254f, 0.0f}};
    static const struct topology_test_edge_record
    {
        uint32_t vertices[2];
        uint32_t faces[2];
        uint32_t face_count;
    } edges[8] = {
        {{1U, 2U}, {1U, 2U}, 2U},
        {{2U, 3U}, {1U, 2U}, 2U},
        {{3U, 4U}, {1U, 2U}, 2U},
        {{4U, 5U}, {2U, HENKA_AUTHORING_INVALID_ID}, 1U},
        {{5U, 6U}, {2U, HENKA_AUTHORING_INVALID_ID}, 1U},
        {{1U, 6U}, {1U, 2U}, 2U},
        {{2U, 4U}, {1U, HENKA_AUTHORING_INVALID_ID}, 1U},
        {{2U, 6U}, {1U, HENKA_AUTHORING_INVALID_ID}, 1U}};
    static const uint32_t face_vertices[2][6] = {
        /* The first face repeats vertex 2 and omits vertex 5. */
        {1U, 2U, 3U, 4U, 2U, 6U},
        {1U, 6U, 5U, 4U, 3U, 2U}};
    static const uint32_t face_edges[2][6] = {
        {1U, 2U, 3U, 7U, 8U, 6U},
        {6U, 5U, 4U, 3U, 2U, 1U}};
    FILE* file = fopen(path, "wb");
    size_t index;
    int ok = file != NULL;

    if (!ok)
    {
        return 0;
    }
    ok = fwrite("HAMS", 4U, 1U, file) == 1U &&
        topology_test_write_u32(file, 7U) &&
        topology_test_write_u32(file, 6U) &&
        topology_test_write_u32(file, 8U) &&
        topology_test_write_u32(file, 2U) &&
        topology_test_write_u32(file, 6U) &&
        topology_test_write_u32(file, 6U) &&
        topology_test_write_u32(file, 8U) &&
        topology_test_write_u32(file, 2U) &&
        topology_test_write_u32(file, 7U) &&
        topology_test_write_u32(file, 9U) &&
        topology_test_write_u32(file, 3U);
    for (index = 0U; ok && index < 6U; ++index)
    {
        ok = topology_test_write_u32(file, (uint32_t)(index + 1U)) &&
            topology_test_write_f32(file, positions[index].x) &&
            topology_test_write_f32(file, positions[index].y) &&
            topology_test_write_f32(file, positions[index].z) &&
            topology_test_write_f32(file, positions[index].x) &&
            topology_test_write_f32(file, positions[index].y) &&
            topology_test_write_u32(file, 0U);
    }
    for (index = 0U; ok && index < 8U; ++index)
    {
        ok = topology_test_write_u32(file, (uint32_t)(index + 1U)) &&
            topology_test_write_u32(file, edges[index].vertices[0]) &&
            topology_test_write_u32(file, edges[index].vertices[1]) &&
            topology_test_write_u32(file, edges[index].faces[0]) &&
            topology_test_write_u32(file, edges[index].faces[1]) &&
            topology_test_write_u32(file, edges[index].face_count) &&
            fputc(0, file) != EOF && fputc(0, file) != EOF;
    }
    for (index = 0U; ok && index < 2U; ++index)
    {
        size_t corner;
        ok = topology_test_write_u32(file, (uint32_t)(index + 1U)) &&
            topology_test_write_u32(file, 6U) &&
            topology_test_write_u32(file, 0U) && fputc(1, file) != EOF;
        for (corner = 0U; ok && corner < 6U; ++corner)
        {
            ok = topology_test_write_u32(file, face_vertices[index][corner]);
        }
        for (corner = 0U; ok && corner < 6U; ++corner)
        {
            ok = topology_test_write_f32(file, 0.0f) &&
                topology_test_write_f32(file, 0.0f);
        }
        for (corner = 0U; ok && corner < 6U; ++corner)
        {
            ok = topology_test_write_u32(file, face_edges[index][corner]);
        }
    }
    if (fclose(file) != 0)
    {
        ok = 0;
    }
    return ok;
}

static int test_duplicate_face_matching_preserves_vertex_multiplicity(void)
{
    const char* path = "build/test_tmp/authoring_topology_vertex_multiplicity.hams";
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_topology_options analysis = henka_authoring_topology_options_default();
    henka_authoring_topology_report analysis_report;
    henka_authoring_topology_repair_options repair =
        henka_authoring_topology_repair_options_default();
    henka_authoring_topology_repair_report repair_report;
    const char* failure = "fixture setup";
    int result = 0;

    repair.remove_duplicate_faces = true;
    if (henka_path_ensure_parent_directory(path) != HENKA_SUCCESS ||
        !topology_test_write_hams_v7_repeated_vertex_fixture(path))
    {
        goto cleanup;
    }
    failure = "fixture loads as a structurally valid two-face mesh";
    if (henka_authoring_mesh_load_file_new(path, &mesh) != HENKA_SUCCESS ||
        !henka_authoring_mesh_validate(mesh) ||
        henka_authoring_mesh_get_counts(mesh).faces != 2U)
    {
        goto cleanup;
    }
    failure = "analysis distinguishes unequal vertex multiplicities";
    if (henka_authoring_topology_analyze(mesh, &analysis, &analysis_report) != HENKA_SUCCESS ||
        analysis_report.duplicate_face_count != 0U)
    {
        goto cleanup;
    }
    failure = "repair leaves unequal vertex multisets unchanged";
    if (henka_authoring_mesh_repair_topology(mesh, &repair, &repair_report) != HENKA_SUCCESS ||
        repair_report.changed || repair_report.removed_duplicate_faces != 0U ||
        repair_report.before.duplicate_face_count != 0U ||
        repair_report.after.duplicate_face_count != 0U ||
        henka_authoring_mesh_get_counts(mesh).faces != 2U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    remove(path);
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail(failure);
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

static int test_transactional_safe_repair(void)
{
    const henka_authoring_mesh_desc desc = {16U, 32U, 16U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertices[5];
    henka_authoring_vertex_id face_vertices[4];
    henka_authoring_face_id first_face = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id duplicate_face = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_topology_repair_options options =
        henka_authoring_topology_repair_options_default();
    henka_authoring_topology_repair_report report;
    henka_authoring_mesh_counts counts;
    henka_authoring_mesh_counts before_unsafe;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        return fail("repair create");
    }
    if (henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U, &vertices[0]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){1.0f, 0.0f, 0.0f}, (henka_vec2){1.0f, 0.0f}, 0U, &vertices[1]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){1.0f, 1.0f, 0.0f}, (henka_vec2){1.0f, 1.0f}, 0U, &vertices[2]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){0.0f, 1.0f, 0.0f}, (henka_vec2){0.0f, 1.0f}, 0U, &vertices[3]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U, &vertices[4]) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    memcpy(face_vertices, vertices, 4U * sizeof(*face_vertices));
    if (henka_authoring_mesh_add_face(
            mesh, face_vertices, 4U, 0U, true, &first_face) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_face(
            mesh, face_vertices, 4U, 0U, true, &duplicate_face) != HENKA_SUCCESS ||
        henka_authoring_mesh_set_edge_hard(mesh,
            henka_authoring_mesh_get_face(mesh, first_face)->edges[0], true) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    options.max_passes = 4U;
    options.remove_isolated_vertices = true;
    options.remove_duplicate_faces = true;
    options.remove_degenerate_faces = true;
    if (henka_authoring_mesh_get_counts(mesh).vertices != 5U ||
        henka_authoring_mesh_get_counts(mesh).faces != 2U ||
        henka_authoring_mesh_repair_topology(mesh, &options, &report) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    counts = henka_authoring_mesh_get_counts(mesh);
    if (!report.changed || report.removed_duplicate_faces != 1U ||
        report.removed_isolated_vertices != 1U || counts.vertices != 4U ||
        counts.faces != 1U || !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }

    if (henka_authoring_mesh_repair_topology(mesh, &options, &report) != HENKA_SUCCESS ||
        report.changed || report.passes != 1U ||
        henka_authoring_mesh_get_counts(mesh).vertices != 4U ||
        henka_authoring_mesh_get_counts(mesh).faces != 1U)
    {
        goto cleanup;
    }

    henka_authoring_mesh_destroy(mesh);
    mesh = NULL;
    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U, &vertices[0]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){1.0f, 0.0f, 0.0f}, (henka_vec2){1.0f, 0.0f}, 0U, &vertices[1]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){1.0f, 1.0f, 0.0f}, (henka_vec2){1.0f, 1.0f}, 0U, &vertices[2]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){0.0f, 1.0f, 0.0f}, (henka_vec2){0.0f, 1.0f}, 0U, &vertices[3]) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_add_face(
            mesh, vertices, 4U, 0U, true, &first_face) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_face(
            mesh, vertices, 4U, 7U, true, &duplicate_face) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    before_unsafe = henka_authoring_mesh_get_counts(mesh);
    if (henka_authoring_mesh_repair_topology(mesh, &options, &report) != HENKA_ERROR_INVALID_ARGUMENT ||
        henka_authoring_mesh_get_counts(mesh).vertices != before_unsafe.vertices ||
        henka_authoring_mesh_get_counts(mesh).faces != before_unsafe.faces ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }

    options.policy_version += 1U;
    if (henka_authoring_mesh_repair_topology(mesh, &options, &report) != HENKA_ERROR_INVALID_ARGUMENT ||
        henka_authoring_mesh_get_counts(mesh).faces != before_unsafe.faces)
    {
        goto cleanup;
    }
    options.policy_version = HENKA_AUTHORING_TOPOLOGY_POLICY_VERSION;
    options.max_passes = 0U;
    if (henka_authoring_mesh_repair_topology(mesh, &options, &report) != HENKA_ERROR_INVALID_ARGUMENT ||
        henka_authoring_mesh_get_counts(mesh).faces != before_unsafe.faces)
    {
        goto cleanup;
    }

    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("transactional safe repair");
}

static int test_quad_strip_walk(void)
{
    const henka_authoring_mesh_desc desc = {16U, 32U, 8U, 8U};
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
    henka_authoring_quad_strip_step steps[3];
    henka_authoring_face_id face_id;
    henka_authoring_edge_id edge_id;
    henka_authoring_edge_id start_edge_id = HENKA_AUTHORING_INVALID_ID;
    size_t count = 0U;
    size_t index;
    bool closed = true;
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
    if (start_edge_id == HENKA_AUTHORING_INVALID_ID ||
        henka_authoring_topology_walk_quad_strip(
            mesh, start_edge_id, NULL, 0U, &count, &closed) != HENKA_SUCCESS ||
        count != 3U || closed ||
        henka_authoring_topology_walk_quad_strip(
            mesh, start_edge_id, steps, 2U, &count, &closed) != HENKA_ERROR_LIMIT ||
        count != 0U ||
        henka_authoring_topology_walk_quad_strip(
            mesh, start_edge_id, steps, 3U, &count, &closed) != HENKA_SUCCESS ||
        count != 3U || closed || steps[0].face_id != 1U ||
        steps[1].face_id != 2U || steps[2].face_id != 3U ||
        steps[0].entry_edge_id != start_edge_id ||
        steps[1].entry_edge_id != steps[0].exit_edge_id ||
        steps[2].entry_edge_id != steps[1].exit_edge_id)
    {
        goto cleanup;
    }
    if (henka_authoring_mesh_set_edge_hard(mesh, steps[0].exit_edge_id, true) != HENKA_SUCCESS ||
        henka_authoring_topology_walk_quad_strip(
            mesh, start_edge_id, steps, 3U, &count, &closed) != HENKA_ERROR_INVALID_ARGUMENT ||
        count != 0U)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("quad strip walk");
}

static int test_degenerate_face_repair(void)
{
    const henka_authoring_mesh_desc desc = {8U, 16U, 8U, 8U};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertices[3];
    henka_authoring_face_id face_id;
    henka_authoring_topology_repair_options options =
        henka_authoring_topology_repair_options_default();
    henka_authoring_topology_repair_report report;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec2){0.0f, 0.0f}, 0U, &vertices[0]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){1.0f, 0.0f, 0.0f}, (henka_vec2){1.0f, 0.0f}, 0U, &vertices[1]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_vertex(
            mesh, (henka_vec3){2.0f, 0.0f, 0.0f}, (henka_vec2){2.0f, 0.0f}, 0U, &vertices[2]) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_face(mesh, vertices, 3U, 0U, true, &face_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    options.max_passes = 2U;
    options.remove_degenerate_faces = true;
    if (henka_authoring_mesh_repair_topology(mesh, &options, &report) != HENKA_SUCCESS ||
        !report.changed || report.removed_degenerate_faces != 1U ||
        henka_authoring_mesh_get_counts(mesh).faces != 0U ||
        !henka_authoring_mesh_validate(mesh))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("degenerate face repair");
}

static int test_large_face_duplicate_analysis(void)
{
    enum { CORNER_COUNT = 40 };
    const henka_authoring_mesh_desc desc = {64U, 128U, 4U, CORNER_COUNT};
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertices[CORNER_COUNT];
    henka_authoring_face_id first_face = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_face_id duplicate_face = HENKA_AUTHORING_INVALID_ID;
    henka_authoring_topology_options options = henka_authoring_topology_options_default();
    henka_authoring_topology_report report;
    henka_authoring_topology_repair_options repair_options =
        henka_authoring_topology_repair_options_default();
    henka_authoring_topology_repair_report repair_report;
    size_t index;
    int result = 0;

    if (henka_authoring_mesh_create(&desc, &mesh) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < CORNER_COUNT; ++index)
    {
        const float angle = 6.2831853071795864769f * (float)index / CORNER_COUNT;
        const henka_vec3 position = {cosf(angle), sinf(angle), 0.0f};
        if (henka_authoring_mesh_add_vertex(
                mesh, position, (henka_vec2){position.x, position.y}, 0U,
                &vertices[index]) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (henka_authoring_mesh_add_face(
            mesh, vertices, CORNER_COUNT, 0U, true, &first_face) != HENKA_SUCCESS ||
        henka_authoring_mesh_add_face(
            mesh, vertices, CORNER_COUNT, 0U, true, &duplicate_face) != HENKA_SUCCESS ||
        first_face == duplicate_face || !henka_authoring_mesh_validate(mesh))
    {
        (void)fprintf(stderr, "could not construct two valid 40-corner duplicate faces\n");
        goto cleanup;
    }
    if (henka_authoring_topology_analyze(mesh, &options, &report) != HENKA_SUCCESS)
    {
        (void)fprintf(stderr, "topology analysis rejected valid 40-corner duplicate faces\n");
        goto cleanup;
    }
    if (report.face_count != 2U || report.ngon_count != 2U ||
        report.max_face_corners != CORNER_COUNT || report.duplicate_face_count != 1U)
    {
        (void)fprintf(stderr,
            "large-face analysis: faces=%zu ngons=%zu max-corners=%zu duplicates=%zu\n",
            report.face_count,
            report.ngon_count,
            report.max_face_corners,
            report.duplicate_face_count);
        goto cleanup;
    }
    repair_options.analysis = options;
    repair_options.max_passes = 2U;
    repair_options.remove_duplicate_faces = true;
    if (henka_authoring_mesh_repair_topology(mesh, &repair_options, &repair_report) !=
            HENKA_SUCCESS ||
        !repair_report.changed || repair_report.removed_duplicate_faces != 1U ||
        henka_authoring_mesh_get_counts(mesh).faces != 1U ||
        !henka_authoring_mesh_validate(mesh))
    {
        (void)fprintf(stderr, "large-face duplicate repair did not remove exactly one face\n");
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_authoring_mesh_destroy(mesh);
    return result ? 1 : fail("large face duplicate analysis");
}

int main(void)
{
    if (!test_quad_cage_and_quality() ||
        !test_winding_and_uv_seam() ||
        !test_profiles() ||
        !test_transactional_safe_repair() ||
        !test_quad_strip_walk() ||
        !test_degenerate_face_repair() ||
        !test_duplicate_face_matching_preserves_vertex_multiplicity() ||
        !test_large_face_duplicate_analysis())
    {
        return 1;
    }

    printf("authoring topology tests passed\n");
    return 0;
}
