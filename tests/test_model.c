#include "test_suite.h"

#include <string.h>

#include <henka/memory.h>
#include <henka/model.h>

#include "../engine/src/core/checked.h"

static void henka_test_model_rejects_unsafe_bounds(void)
{
    static const char* non_finite_obj =
        "v nan 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "f 1 2 3\n";
    static const char* overflow_index_obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "f 999999999999999999999 2 3\n";
    size_t oversized_length;
    char* oversized_source;
    henka_model_data model;

    memset(&model, 0, sizeof(model));
    HENKA_TEST_ASSERT(
        henka_model_data_load_obj_from_memory(non_finite_obj, "non_finite_obj", &model) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices == NULL);
    HENKA_TEST_ASSERT(model.indices == NULL);

    memset(&model, 0, sizeof(model));
    HENKA_TEST_ASSERT(
        henka_model_data_load_obj_from_memory(overflow_index_obj, "overflow_index_obj", &model) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices == NULL);
    HENKA_TEST_ASSERT(model.indices == NULL);

    oversized_length = HENKA_MAX_OBJ_SOURCE_BYTES + 1U;
    oversized_source = henka_malloc(oversized_length + 1U);
    HENKA_TEST_ASSERT(oversized_source != NULL);
    memset(oversized_source, '#', oversized_length);
    oversized_source[oversized_length] = '\0';

    memset(&model, 0, sizeof(model));
    HENKA_TEST_ASSERT(
        henka_model_data_load_obj_from_memory(oversized_source, "oversized_obj", &model) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices == NULL);
    HENKA_TEST_ASSERT(model.indices == NULL);
    henka_free(oversized_source);
}

void henka_test_model(void)
{
    static const char* valid_obj =
        "# simple quad\n"
        "v -0.5 0.0 -0.5\n"
        "v 0.5 0.0 -0.5\n"
        "v 0.5 0.0 0.5\n"
        "v -0.5 0.0 0.5\n"
        "vt 0.0 0.0\n"
        "vt 1.0 0.0\n"
        "vt 1.0 1.0\n"
        "vt 0.0 1.0\n"
        "f 1/1 2/2 3/3 4/4\n";
    static const char* valid_obj_with_whitespace =
        "\r\n"
        "   # comment with leading whitespace\r\n"
        "v 0.0 0.0 0.0   \r\n"
        "v 1.0 0.0 0.0\r\n"
        "v 0.0 1.0 0.0\r\n"
        "   f   1   2   3   \r\n";
    static const char* valid_obj_without_uvs_or_normals =
        "v 0.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "f 1 2 3\n";
    static const char* invalid_obj =
        "v 0.0 0.0 0.0\n"
        "f 1 2\n";
    static const char* invalid_index_obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "f 1 2 4\n";
    static const char* valid_ngon_obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 1.0 1.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "v -1.0 0.5 0.0\n"
        "f 1 2 3 4 5\n";
    static const char* valid_negative_index_obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "f -3 -2 -1\n";
    static const char* valid_negative_optional_index_obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "vt 0.125 0.250\n"
        "vt 0.500 0.750\n"
        "vt 0.900 0.100\n"
        "vn 0.0 1.0 0.0\n"
        "f 1/-3/-1 2/-2/-1 3/-1/-1\n";
    static const char* degenerate_face_obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 2.0 0.0 0.0\n"
        "f 1 2 3\n";
    static const char* vertices_without_faces_obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n";
    static const char* empty_obj =
        "   \n"
        "\t# comment only\n";
    henka_model_data model;

    model.vertices = NULL;
    model.indices = NULL;
    model.vertex_count = 0U;
    model.index_count = 0U;

    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(valid_obj, "valid_obj", &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices != NULL);
    HENKA_TEST_ASSERT(model.indices != NULL);
    HENKA_TEST_ASSERT(model.vertex_count == 6U);
    HENKA_TEST_ASSERT(model.index_count == 6U);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[0].normal.y, -1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[1].uv.x, 1.0f, 0.0001);
    henka_model_data_destroy(&model);

    model.vertices = NULL;
    model.indices = NULL;
    model.vertex_count = 0U;
    model.index_count = 0U;
    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(invalid_obj, "invalid_obj", &model) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices == NULL);
    HENKA_TEST_ASSERT(model.indices == NULL);

    model.vertices = NULL;
    model.indices = NULL;
    model.vertex_count = 0U;
    model.index_count = 0U;
    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(valid_obj_with_whitespace, "valid_obj_with_whitespace", &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertex_count == 3U);
    HENKA_TEST_ASSERT(model.index_count == 3U);
    henka_model_data_destroy(&model);

    model.vertices = NULL;
    model.indices = NULL;
    model.vertex_count = 0U;
    model.index_count = 0U;
    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(valid_obj_without_uvs_or_normals, "valid_obj_without_uvs_or_normals", &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertex_count == 3U);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[0].uv.x, 0.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[0].normal.z, -1.0f, 0.0001);
    henka_model_data_destroy(&model);

    model.vertices = NULL;
    model.indices = NULL;
    model.vertex_count = 0U;
    model.index_count = 0U;
    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(invalid_index_obj, "invalid_index_obj", &model) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices == NULL);
    HENKA_TEST_ASSERT(model.indices == NULL);

    model.vertices = NULL;
    model.indices = NULL;
    model.vertex_count = 0U;
    model.index_count = 0U;
    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(valid_ngon_obj, "valid_ngon_obj", &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices != NULL);
    HENKA_TEST_ASSERT(model.indices != NULL);
    HENKA_TEST_ASSERT(model.vertex_count == 9U);
    HENKA_TEST_ASSERT(model.index_count == 9U);
    henka_model_data_destroy(&model);

    model.vertices = NULL;
    model.indices = NULL;
    model.vertex_count = 0U;
    model.index_count = 0U;
    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(valid_negative_index_obj, "valid_negative_index_obj", &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices != NULL);
    HENKA_TEST_ASSERT(model.indices != NULL);
    HENKA_TEST_ASSERT(model.vertex_count == 3U);
    HENKA_TEST_ASSERT(model.index_count == 3U);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[0].position.x, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[1].position.x, 1.0f, 0.0001f);
    henka_model_data_destroy(&model);

    model.vertices = NULL;
    model.indices = NULL;
    model.vertex_count = 0U;
    model.index_count = 0U;
    HENKA_TEST_ASSERT(
        henka_model_data_load_obj_from_memory(
            valid_negative_optional_index_obj,
            "valid_negative_optional_index_obj",
            &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices != NULL);
    HENKA_TEST_ASSERT(model.indices != NULL);
    HENKA_TEST_ASSERT(model.vertex_count == 3U);
    HENKA_TEST_ASSERT(model.index_count == 3U);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[0].uv.x, 0.125f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[1].uv.y, 0.750f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[2].uv.x, 0.900f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[2].uv.y, 0.100f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[0].normal.y, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[1].normal.y, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[2].normal.y, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[2].normal.z, 0.0f, 0.0001f);
    henka_model_data_destroy(&model);

    model.vertices = NULL;
    model.indices = NULL;
    model.vertex_count = 0U;
    model.index_count = 0U;
    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(degenerate_face_obj, "degenerate_face_obj", &model) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices == NULL);
    HENKA_TEST_ASSERT(model.indices == NULL);

    model.vertices = NULL;
    model.indices = NULL;
    model.vertex_count = 0U;
    model.index_count = 0U;
    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(vertices_without_faces_obj, "vertices_without_faces_obj", &model) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices == NULL);
    HENKA_TEST_ASSERT(model.indices == NULL);

    model.vertices = NULL;
    model.indices = NULL;
    model.vertex_count = 0U;
    model.index_count = 0U;
    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(empty_obj, "empty_obj", &model) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices == NULL);
    HENKA_TEST_ASSERT(model.indices == NULL);

    henka_test_model_rejects_unsafe_bounds();
}
