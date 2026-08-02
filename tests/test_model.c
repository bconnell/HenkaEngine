#include "test_suite.h"

#include <string.h>

#include <henka/memory.h>
#include <henka/model.h>

#include "../engine/src/core/checked.h"

static void henka_test_model_rejects_unsafe_bounds(void)
{
    static const char* unsupported_gltf_extension =
        "{\"asset\":{\"version\":\"2.0\"},\"extensionsUsed\":[\"KHR_draco_mesh_compression\"]}";
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
        henka_model_data_load_gltf_from_memory(
            unsupported_gltf_extension,
            strlen(unsupported_gltf_extension),
            "unsupported-extension.gltf",
            &model) != HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertices == NULL);
    HENKA_TEST_ASSERT(model.indices == NULL);

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

static void henka_test_gltf_scene_import(void)
{
    static const char* scene_gltf =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,"
        "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA\",\"byteLength\":36}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
        "\"materials\":[{\"name\":\"Scene Material\",\"pbrMetallicRoughness\":{\"metallicFactor\":0.7}}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0},"
        "{\"attributes\":{\"POSITION\":0},\"material\":0}]}],"
        "\"cameras\":[{\"name\":\"Main Camera\",\"type\":\"perspective\",\"perspective\":{\"yfov\":1.0,\"znear\":0.1,\"zfar\":100.0}}],"
        "\"extensions\":{\"KHR_lights_punctual\":{\"lights\":[{\"name\":\"Key\",\"type\":\"point\",\"intensity\":2.0}]}},"
        "\"nodes\":[{\"name\":\"Root\",\"mesh\":0,\"camera\":0,\"children\":[1],\"extensions\":{\"KHR_lights_punctual\":{\"light\":0}}},"
        "{\"name\":\"Child\",\"mesh\":0,\"translation\":[2.0,0.0,0.0]}],"
        "\"scenes\":[{\"nodes\":[0]}],\"scene\":0}";
    henka_model_scene_data scene;

    memset(&scene, 0, sizeof(scene));
    HENKA_TEST_ASSERT(henka_model_scene_data_load_gltf_from_memory(
        scene_gltf, strlen(scene_gltf), "scene.gltf", &scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(scene.primitive_count == 2U);
    HENKA_TEST_ASSERT(scene.primitives[0].material_index == 0);
    HENKA_TEST_ASSERT(scene.primitives[1].material_index == 0);
    HENKA_TEST_ASSERT(scene.material_count == 1U);
    HENKA_TEST_ASSERT(scene.material_present[0]);
    HENKA_TEST_ASSERT(scene.node_count == 2U);
    HENKA_TEST_ASSERT(scene.nodes[1].parent_index == 0);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(scene.nodes[1].world_matrix.m[12], 2.0f, 0.0001f);
    HENKA_TEST_ASSERT(scene.scene_count == 1U);
    HENKA_TEST_ASSERT(scene.active_scene_index == 0U);
    HENKA_TEST_ASSERT(scene.scene_root_counts[0] == 1U);
    HENKA_TEST_ASSERT(scene.camera_count == 1U);
    HENKA_TEST_ASSERT(scene.cameras[0].camera.projection_mode == HENKA_CAMERA_PROJECTION_PERSPECTIVE);
    HENKA_TEST_ASSERT(scene.light_count == 1U);
    HENKA_TEST_ASSERT(scene.lights[0].type == HENKA_MODEL_SCENE_LIGHT_POINT);
    henka_model_scene_data_destroy(&scene);
}

void henka_test_model(void)
{
    static const char* valid_gltf =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,"
        "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA\",\"byteLength\":36}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]}";
    static const char* valid_gltf_material =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,"
        "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA\",\"byteLength\":36}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
        "\"images\":[{\"uri\":\"textures/albedo.png\"}],"
        "\"textures\":[{\"source\":0}],"
        "\"materials\":[{\"name\":\"Imported Gold\",\"pbrMetallicRoughness\":{"
        "\"baseColorFactor\":[0.8,0.6,0.2,1.0],\"metallicFactor\":0.8,\"roughnessFactor\":0.3,"
        "\"baseColorTexture\":{\"index\":0}},\"emissiveFactor\":[0.1,0.2,0.3],"
        "\"alphaMode\":\"MASK\",\"alphaCutoff\":0.4,\"doubleSided\":true}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}]}";
    static const char* valid_gltf_embedded_material =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,"
        "AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA\",\"byteLength\":36}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
        "\"images\":[{\"uri\":\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=\"}],"
        "\"textures\":[{\"source\":0}],"
        "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}]}";
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

    memset(&model, 0, sizeof(model));
    HENKA_TEST_ASSERT(
        henka_model_data_load_gltf_from_memory(
            valid_gltf,
            strlen(valid_gltf),
            "valid.gltf",
            &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.vertex_count == 3U);
    HENKA_TEST_ASSERT(model.index_count == 3U);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[1].position.x, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[0].normal.z, 1.0f, 0.0001f);
    henka_model_data_destroy(&model);

    memset(&model, 0, sizeof(model));
    HENKA_TEST_ASSERT(
        henka_model_data_load_gltf_from_memory(
            valid_gltf_material,
            strlen(valid_gltf_material),
            "material.gltf",
            &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.has_material);
    HENKA_TEST_ASSERT(strcmp(model.material_source.name, "Imported Gold") == 0);
    HENKA_TEST_ASSERT(strcmp(model.material_source.base_color_uri, "textures/albedo.png") == 0);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.material_source.material.base_color.x, 0.8f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.material_source.material.metallic, 0.8f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.material_source.material.roughness, 0.3f, 0.0001f);
    HENKA_TEST_ASSERT(model.material_source.material.alpha_mode == HENKA_MATERIAL_ALPHA_MASKED);
    HENKA_TEST_ASSERT(model.material_source.material.double_sided);
    henka_model_data_destroy(&model);

    memset(&model, 0, sizeof(model));
    HENKA_TEST_ASSERT(
        henka_model_data_load_gltf_from_memory(
            valid_gltf_embedded_material,
            strlen(valid_gltf_embedded_material),
            "embedded-material.gltf",
            &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(model.has_material);
    HENKA_TEST_ASSERT(model.material_source.base_color_uri == NULL);
    HENKA_TEST_ASSERT(model.material_source.base_color_embedded_data != NULL);
    HENKA_TEST_ASSERT(model.material_source.base_color_embedded_size > 0U);
    henka_model_data_destroy(&model);

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
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[0].color.x, 1.0f, 0.0001);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.vertices[0].color.w, 1.0f, 0.0001);
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
    henka_test_gltf_scene_import();
}
