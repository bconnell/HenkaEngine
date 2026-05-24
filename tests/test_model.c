#include "test_suite.h"

#include <henka/model.h>

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
    static const char* unsupported_polygon_obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 1.0 1.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "v -1.0 0.5 0.0\n"
        "f 1 2 3 4 5\n";
    static const char* unsupported_negative_index_obj =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "f -3 -2 -1\n";
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
    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(unsupported_polygon_obj, "unsupported_polygon_obj", &model) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices == NULL);
    HENKA_TEST_ASSERT(model.indices == NULL);

    model.vertices = NULL;
    model.indices = NULL;
    model.vertex_count = 0U;
    model.index_count = 0U;
    HENKA_TEST_ASSERT(henka_model_data_load_obj_from_memory(unsupported_negative_index_obj, "unsupported_negative_index_obj", &model) != HENKA_SUCCESS);
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
}
