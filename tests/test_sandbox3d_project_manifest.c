#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include <henka/core.h>
#include <henka/authoring_modeling.h>
#include <henka/camera.h>
#include <henka/engine.h>
#include <henka/mesh.h>
#include <henka/scene.h>
#include <henka/assets.h>

#include "../examples/sandbox3d/game_authoring.h"

static int test_float_close(float left, float right)
{
    const float difference = left - right;
    return difference < 0.0001f && difference > -0.0001f;
}

static int test_write_file(const char* path, const char* contents)
{
    FILE* file = NULL;
    int close_result;
    if (fopen_s(&file, path, "wb") != 0 || file == NULL)
    {
        return 0;
    }
    if (fputs(contents, file) == EOF)
    {
        fclose(file);
        return 0;
    }
    close_result = fclose(file);
    if (close_result != 0)
    {
        return 0;
    }
    return 1;
}

static int test_ensure_directory(const char* path)
{
    int result;
    if (path == NULL || path[0] == '\0')
    {
        return 0;
    }
#if defined(_WIN32)
    result = _mkdir(path);
#else
    result = mkdir(path, 0700);
#endif
    return result == 0 || errno == EEXIST;
}

static int test_project_reopen_materializes_asset(void)
{
    const char* project_root = "build/test_tmp/project_asset_reopen_root";
    const char* scene_path = "asset_scene.hscene";
    const char* asset_path = "build/test_tmp/project_asset_reopen_root/triangle.obj";
    const char* manifest_path =
        "build/test_tmp/project_asset_reopen_root/henka.project";
    const char* scene_file_path =
        "build/test_tmp/project_asset_reopen_root/asset_scene.hscene";
    const char* obj_source =
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "f 1 2 3\n";
    henka_scene* source_scene = NULL;
    henka_scene* reopened_scene = NULL;
    sandbox3d_game_authoring* source_authoring = NULL;
    sandbox3d_game_authoring* reopened_authoring = NULL;
    henka_engine* engine = NULL;
    henka_asset_manager* assets = NULL;
    henka_engine_config config = {0};
    henka_camera camera;
    henka_entity source_entity = HENKA_INVALID_ENTITY;
    henka_entity reopened_entity = HENKA_INVALID_ENTITY;
    henka_scene_document_id document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id reopened_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_object object;
    henka_mesh* reopened_mesh = NULL;
    henka_result open_result = HENKA_ERROR_UNKNOWN;
    henka_result entity_result = HENKA_ERROR_UNKNOWN;
    henka_result object_result = HENKA_ERROR_UNKNOWN;
    henka_result mesh_result = HENKA_ERROR_UNKNOWN;
    int success = 0;

    if (!test_ensure_directory("build/test_tmp")) goto cleanup;
    if (!test_ensure_directory(project_root)) goto cleanup;
    if (!test_write_file(asset_path, obj_source)) goto cleanup;
    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD,
        1.0f,
        0.1f,
        100.0f);
    if (henka_scene_create(&source_scene) != HENKA_SUCCESS ||
        henka_scene_set_camera(source_scene, &camera) != HENKA_SUCCESS) goto cleanup;
    source_entity = henka_scene_create_entity_named(
        source_scene, "Imported Project Triangle");
    if (source_entity == HENKA_INVALID_ENTITY) goto cleanup;
    if (sandbox3d_game_authoring_create(
            source_scene, scene_path, &source_authoring) != HENKA_SUCCESS) goto cleanup;
    if (sandbox3d_game_authoring_register_entity(
            source_authoring, source_entity, &document_id) != HENKA_SUCCESS) goto cleanup;
    if (sandbox3d_game_authoring_get_object_for_entity(
            source_authoring, source_entity, &document_id, &object) != HENKA_SUCCESS) goto cleanup;
    {
        object.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_ASSET;
        object.source.asset_kind = HENKA_SCENE_DOCUMENT_ASSET_MESH;
        (void)snprintf(
            object.source.path,
            sizeof(object.source.path),
            "%s",
            "triangle.obj");
        if (sandbox3d_game_authoring_update_object_for_entity(
                source_authoring, source_entity, &object) != HENKA_SUCCESS) goto cleanup;
    }
    if (sandbox3d_game_authoring_save(source_authoring, project_root) !=
        HENKA_SUCCESS) goto cleanup;
    sandbox3d_game_authoring_destroy(source_authoring);
    source_authoring = NULL;
    henka_scene_destroy(source_scene);
    source_scene = NULL;

    if (sandbox3d_game_authoring_open_project(
            project_root,
            &reopened_scene,
            &reopened_authoring) == HENKA_SUCCESS ||
        reopened_scene != NULL || reopened_authoring != NULL)
    {
        goto cleanup;
    }

    config.application_name = "Henka Project Asset Reopen Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    config.asset_base_path = project_root;
    if (henka_engine_create(&config, &engine) != HENKA_SUCCESS) goto cleanup;
    assets = henka_engine_get_asset_manager(engine);
    if (assets == NULL) goto cleanup;
    open_result = sandbox3d_game_authoring_open_project_with_assets(
            project_root,
            assets,
            &reopened_scene,
            &reopened_authoring);
    if (open_result == HENKA_SUCCESS && reopened_scene != NULL &&
        reopened_authoring != NULL &&
        henka_scene_get_entity_count(reopened_scene) == 1U)
    {
        entity_result = sandbox3d_game_authoring_get_entity_for_document_id(
            reopened_authoring, document_id, &reopened_entity);
    }
    if (entity_result == HENKA_SUCCESS)
    {
        object_result = sandbox3d_game_authoring_get_object_for_entity(
            reopened_authoring, reopened_entity, &reopened_id, &object);
    }
    if (object_result == HENKA_SUCCESS)
    {
        mesh_result = henka_scene_get_entity_mesh(
            reopened_scene, reopened_entity, &reopened_mesh);
    }
    if (open_result != HENKA_SUCCESS || reopened_scene == NULL ||
        reopened_authoring == NULL ||
        henka_scene_get_entity_count(reopened_scene) != 1U ||
        entity_result != HENKA_SUCCESS || object_result != HENKA_SUCCESS ||
        reopened_id != document_id ||
        strcmp(object.source.path, "triangle.obj") != 0 ||
        mesh_result != HENKA_SUCCESS || reopened_mesh == NULL)
    {
        goto cleanup;
    }
    success = 1;

cleanup:
    sandbox3d_game_authoring_destroy(reopened_authoring);
    sandbox3d_game_authoring_destroy(source_authoring);
    henka_scene_destroy(reopened_scene);
    henka_scene_destroy(source_scene);
    henka_engine_destroy(engine);
    (void)remove(asset_path);
    (void)remove(manifest_path);
    (void)remove(scene_file_path);
    (void)remove(project_root);
    return success;
}

static int test_project_reopen_materializes_primitives(void)
{
    const char* project_root = "build/test_tmp/project_primitive_reopen_root";
    const char* scene_path = "primitive_scene.hscene";
    const char* manifest_path =
        "build/test_tmp/project_primitive_reopen_root/henka.project";
    const char* scene_file_path =
        "build/test_tmp/project_primitive_reopen_root/primitive_scene.hscene";
    const char* names[3] = {"Project Box", "Project Sphere", "Project Plane"};
    const henka_scene_document_source_kind source_kind[3] = {
        HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE,
        HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE,
        HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE};
    const henka_scene_document_primitive_kind primitive_kind[3] = {
        HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX,
        HENKA_SCENE_DOCUMENT_PRIMITIVE_SPHERE,
        HENKA_SCENE_DOCUMENT_PRIMITIVE_PLANE};
    const henka_vec3 dimensions[3] = {
        {2.0f, 3.0f, 4.0f},
        {2.0f, 2.0f, 2.0f},
        {8.0f, 1.0f, 6.0f}};
    henka_scene* source_scene = NULL;
    henka_scene* reopened_scene = NULL;
    sandbox3d_game_authoring* source_authoring = NULL;
    sandbox3d_game_authoring* reopened_authoring = NULL;
    henka_engine* engine = NULL;
    henka_engine_config config = {0};
    henka_camera camera;
    henka_entity source_entities[3] = {
        HENKA_INVALID_ENTITY, HENKA_INVALID_ENTITY, HENKA_INVALID_ENTITY};
    henka_entity reopened_entity = HENKA_INVALID_ENTITY;
    henka_scene_document_id document_ids[3] = {
        HENKA_INVALID_SCENE_DOCUMENT_ID,
        HENKA_INVALID_SCENE_DOCUMENT_ID,
        HENKA_INVALID_SCENE_DOCUMENT_ID};
    henka_scene_document_id reopened_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_object object;
    henka_mesh* mesh = NULL;
    size_t index;
    int success = 0;

    if (!test_ensure_directory("build/test_tmp") ||
        !test_ensure_directory(project_root)) goto cleanup;
    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD,
        1.0f,
        0.1f,
        100.0f);
    if (henka_scene_create(&source_scene) != HENKA_SUCCESS ||
        henka_scene_set_camera(source_scene, &camera) != HENKA_SUCCESS) goto cleanup;
    if (sandbox3d_game_authoring_create(
            source_scene, scene_path, &source_authoring) != HENKA_SUCCESS) goto cleanup;
    for (index = 0U; index < 3U; ++index)
    {
        source_entities[index] = henka_scene_create_entity_named(
            source_scene, names[index]);
        if (source_entities[index] == HENKA_INVALID_ENTITY ||
            sandbox3d_game_authoring_register_entity(
                source_authoring,
                source_entities[index],
                &document_ids[index]) != HENKA_SUCCESS ||
            sandbox3d_game_authoring_get_object_for_entity(
                source_authoring,
                source_entities[index],
                &document_ids[index],
                &object) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        object.source.kind = source_kind[index];
        object.source.primitive = primitive_kind[index];
        object.source.primitive_dimensions = dimensions[index];
        if (sandbox3d_game_authoring_update_object_for_entity(
                source_authoring,
                source_entities[index],
                &object) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (sandbox3d_game_authoring_save(source_authoring, project_root) !=
        HENKA_SUCCESS) goto cleanup;
    sandbox3d_game_authoring_destroy(source_authoring);
    source_authoring = NULL;
    henka_scene_destroy(source_scene);
    source_scene = NULL;

    config.application_name = "Henka Project Primitive Reopen Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    config.asset_base_path = project_root;
    if (henka_engine_create(&config, &engine) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_open_project_with_engine(
            project_root,
            engine,
            &reopened_scene,
            &reopened_authoring) != HENKA_SUCCESS ||
        reopened_scene == NULL || reopened_authoring == NULL ||
        henka_scene_get_entity_count(reopened_scene) != 3U)
    {
        goto cleanup;
    }
    for (index = 0U; index < 3U; ++index)
    {
        if (sandbox3d_game_authoring_get_entity_for_document_id(
                reopened_authoring,
                document_ids[index],
                &reopened_entity) != HENKA_SUCCESS ||
            sandbox3d_game_authoring_get_object_for_entity(
                reopened_authoring,
                reopened_entity,
                &reopened_id,
                &object) != HENKA_SUCCESS ||
            reopened_id != document_ids[index] ||
            object.source.kind != source_kind[index] ||
            object.source.primitive != primitive_kind[index] ||
            object.source.primitive_dimensions.x != dimensions[index].x ||
            object.source.primitive_dimensions.y != dimensions[index].y ||
            object.source.primitive_dimensions.z != dimensions[index].z ||
            henka_scene_get_entity_mesh(
                reopened_scene,
                reopened_entity,
                &mesh) != HENKA_SUCCESS ||
            mesh == NULL)
        {
            goto cleanup;
        }
    }
    success = 1;

cleanup:
    sandbox3d_game_authoring_destroy(reopened_authoring);
    sandbox3d_game_authoring_destroy(source_authoring);
    henka_scene_destroy(reopened_scene);
    henka_scene_destroy(source_scene);
    henka_engine_destroy(engine);
    (void)remove(manifest_path);
    (void)remove(scene_file_path);
    (void)remove(project_root);
    return success;
}

static int test_project_reopen_materializes_authoring_mesh(void)
{
    const char* project_root = "build/test_tmp/project_authoring_mesh_reopen_root";
    const char* scene_path = "authoring_mesh_scene.hscene";
    const char* source_path =
        "build/test_tmp/project_authoring_mesh_reopen_root/authored_source.hams";
    const char* manifest_path =
        "build/test_tmp/project_authoring_mesh_reopen_root/henka.project";
    const char* scene_file_path =
        "build/test_tmp/project_authoring_mesh_reopen_root/authoring_mesh_scene.hscene";
    henka_authoring_mesh* source_mesh = NULL;
    henka_scene* source_scene = NULL;
    henka_scene* reopened_scene = NULL;
    sandbox3d_game_authoring* source_authoring = NULL;
    sandbox3d_game_authoring* reopened_authoring = NULL;
    henka_engine* engine = NULL;
    henka_engine_config config = {0};
    henka_authoring_mesh_desc description = henka_authoring_mesh_desc_default();
    henka_camera camera;
    henka_entity source_entity = HENKA_INVALID_ENTITY;
    henka_entity reopened_entity = HENKA_INVALID_ENTITY;
    henka_scene_document_id document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id reopened_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_object object;
    henka_mesh* reopened_mesh = NULL;
    henka_authoring_mesh_counts counts;
    int success = 0;

    if (!test_ensure_directory("build/test_tmp") ||
        !test_ensure_directory(project_root)) goto cleanup;
    if (henka_authoring_mesh_create_box(
            &description,
            2.0f,
            3.0f,
            4.0f,
            &source_mesh) != HENKA_SUCCESS ||
        source_mesh == NULL ||
        henka_authoring_mesh_save_file(source_mesh, source_path) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    counts = henka_authoring_mesh_get_counts(source_mesh);
    if (counts.vertices == 0U || counts.faces == 0U) goto cleanup;
    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD,
        1.0f,
        0.1f,
        100.0f);
    if (henka_scene_create(&source_scene) != HENKA_SUCCESS ||
        henka_scene_set_camera(source_scene, &camera) != HENKA_SUCCESS) goto cleanup;
    source_entity = henka_scene_create_entity_named(
        source_scene, "Persisted Authoring Mesh");
    if (source_entity == HENKA_INVALID_ENTITY ||
        sandbox3d_game_authoring_create(
            source_scene, scene_path, &source_authoring) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_register_entity(
            source_authoring, source_entity, &document_id) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_get_object_for_entity(
            source_authoring, source_entity, &document_id, &object) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    object.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_AUTHORING_MESH;
    object.source.path[0] = '\0';
    (void)snprintf(
        object.source.path,
        sizeof(object.source.path),
        "%s",
        "authored_source.hams");
    if (sandbox3d_game_authoring_update_object_for_entity(
            source_authoring, source_entity, &object) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_save(source_authoring, project_root) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    sandbox3d_game_authoring_destroy(source_authoring);
    source_authoring = NULL;
    henka_scene_destroy(source_scene);
    source_scene = NULL;
    henka_authoring_mesh_destroy(source_mesh);
    source_mesh = NULL;

    config.application_name = "Henka Project Authoring Mesh Reopen Test";
    config.window_width = 320;
    config.window_height = 240;
    config.enable_vsync = false;
    config.asset_base_path = ".";
    if (henka_engine_create(&config, &engine) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_open_project_with_engine(
            project_root,
            engine,
            &reopened_scene,
            &reopened_authoring) != HENKA_SUCCESS ||
        reopened_scene == NULL || reopened_authoring == NULL ||
        henka_scene_get_entity_count(reopened_scene) != 1U ||
        sandbox3d_game_authoring_get_entity_for_document_id(
            reopened_authoring, document_id, &reopened_entity) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_get_object_for_entity(
            reopened_authoring, reopened_entity, &reopened_id, &object) != HENKA_SUCCESS ||
        reopened_id != document_id ||
        object.source.kind != HENKA_SCENE_DOCUMENT_SOURCE_AUTHORING_MESH ||
        strcmp(object.source.path, "authored_source.hams") != 0 ||
        henka_scene_get_entity_mesh(
            reopened_scene, reopened_entity, &reopened_mesh) != HENKA_SUCCESS ||
        reopened_mesh == NULL)
    {
        goto cleanup;
    }
    success = 1;

cleanup:
    sandbox3d_game_authoring_destroy(reopened_authoring);
    sandbox3d_game_authoring_destroy(source_authoring);
    henka_scene_destroy(reopened_scene);
    henka_scene_destroy(source_scene);
    henka_authoring_mesh_destroy(source_mesh);
    henka_engine_destroy(engine);
    (void)remove(source_path);
    (void)remove(manifest_path);
    (void)remove(scene_file_path);
    (void)remove(project_root);
    return success;
}

static int test_project_reopen_cycle(void)
{
    const char* project_root = "build/test_tmp/project_reopen_root";
    const char* relative_path = "authoritative.hscene";
    henka_scene* source_scene = NULL;
    henka_scene* reopened_scene = NULL;
    henka_scene* continued_scene = NULL;
    sandbox3d_game_authoring* source_authoring = NULL;
    sandbox3d_game_authoring* reopened_authoring = NULL;
    sandbox3d_game_authoring* continued_authoring = NULL;
    henka_entity source_entity = HENKA_INVALID_ENTITY;
    henka_entity reopened_entity = HENKA_INVALID_ENTITY;
    henka_entity continued_entity = HENKA_INVALID_ENTITY;
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id reopened_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id continued_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_object object;
    henka_scene_document_object continued_object;
    henka_transform transform = henka_transform_identity();
    henka_camera camera;
    henka_result open_result;
    int success = 0;

    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD,
        1.0f,
        0.1f,
        100.0f);
    transform.position.x = 3.0f;
    if (henka_scene_create(&source_scene) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    source_entity = henka_scene_create_entity_named(
        source_scene, "Project Reopen Object");
    if (source_entity == HENKA_INVALID_ENTITY ||
        henka_scene_set_camera(source_scene, &camera) != HENKA_SUCCESS ||
        henka_scene_set_entity_transform(
            source_scene, source_entity, transform) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_create(
            source_scene, relative_path, &source_authoring) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_register_entity(
            source_authoring, source_entity, &object_id) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_save(source_authoring, project_root) !=
            HENKA_SUCCESS)
    {
        goto cleanup;
    }
    sandbox3d_game_authoring_destroy(source_authoring);
    source_authoring = NULL;
    henka_scene_destroy(source_scene);
    source_scene = NULL;

    open_result = sandbox3d_game_authoring_open_project(
        project_root, &reopened_scene, &reopened_authoring);
    if (open_result != HENKA_SUCCESS ||
        reopened_scene == NULL ||
        reopened_authoring == NULL ||
        henka_scene_get_entity_count(reopened_scene) != 1U ||
        sandbox3d_game_authoring_get_entity_for_document_id(
            reopened_authoring, object_id, &reopened_entity) != HENKA_SUCCESS ||
        !henka_scene_is_entity_valid(reopened_scene, reopened_entity) ||
        sandbox3d_game_authoring_get_object_for_entity(
            reopened_authoring, reopened_entity, &reopened_id, &object) !=
            HENKA_SUCCESS ||
        reopened_id != object_id ||
        strcmp(object.name, "Project Reopen Object") != 0 ||
        !test_float_close(object.transform.position.x, 3.0f))
    {
        goto cleanup;
    }

    object.transform.position.x = 9.0f;
    (void)snprintf(
        object.name,
        sizeof(object.name),
        "%s",
        "Continued Project Edit");
    if (sandbox3d_game_authoring_update_object_for_entity(
            reopened_authoring, reopened_entity, &object) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (sandbox3d_game_authoring_save(reopened_authoring, project_root) !=
        HENKA_SUCCESS)
    {
        goto cleanup;
    }
    sandbox3d_game_authoring_destroy(reopened_authoring);
    reopened_authoring = NULL;
    henka_scene_destroy(reopened_scene);
    reopened_scene = NULL;

    open_result = sandbox3d_game_authoring_open_project(
        project_root, &continued_scene, &continued_authoring);
    if (open_result != HENKA_SUCCESS ||
        sandbox3d_game_authoring_get_entity_for_document_id(
            continued_authoring, object_id, &continued_entity) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_get_object_for_entity(
            continued_authoring, continued_entity, &continued_id, &continued_object) !=
            HENKA_SUCCESS ||
        continued_id != object_id ||
        strcmp(continued_object.name, "Continued Project Edit") != 0 ||
        !test_float_close(continued_object.transform.position.x, 9.0f))
    {
        goto cleanup;
    }
    success = 1;

cleanup:
    sandbox3d_game_authoring_destroy(continued_authoring);
    sandbox3d_game_authoring_destroy(reopened_authoring);
    sandbox3d_game_authoring_destroy(source_authoring);
    henka_scene_destroy(continued_scene);
    henka_scene_destroy(reopened_scene);
    henka_scene_destroy(source_scene);
    (void)remove("build/test_tmp/project_reopen_root/henka.project");
    (void)remove("build/test_tmp/project_reopen_root/authoritative.hscene");
    return success;
}

int main(void)
{
    const char* project_root = "build/test_tmp/project_manifest_root";
    const char* scene_path = "main.hscene";
    const char* wrong_scene_path = "wrong.hscene";
    const char* manifest_path =
        "build/test_tmp/project_manifest_root/henka.project";
    const char* saved_scene_path =
        "build/test_tmp/project_manifest_root/main.hscene";
    henka_scene* scene = NULL;
    henka_camera camera;
    sandbox3d_game_authoring* authoring = NULL;
    sandbox3d_game_authoring* reloader = NULL;
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_scene_document_id document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id reloader_document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_transform authored_transform = henka_transform_identity();
    henka_transform loaded_transform = henka_transform_identity();
    henka_transform changed_transform = henka_transform_identity();
    FILE* manifest = NULL;
    henka_result load_result = HENKA_ERROR_UNKNOWN;
    henka_result transform_result = HENKA_ERROR_UNKNOWN;
    henka_result changed_result = HENKA_ERROR_UNKNOWN;
    henka_result reloader_create_result = HENKA_ERROR_UNKNOWN;
    henka_result reloader_register_result = HENKA_ERROR_UNKNOWN;
    int result = 1;

    authored_transform.position.x = 4.0f;
    changed_transform.position.x = 11.0f;
    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD,
        1.0f,
        0.1f,
        100.0f);
    if (henka_scene_create(&scene) != HENKA_SUCCESS ||
        henka_scene_set_camera(scene, &camera) != HENKA_SUCCESS ||
        (entity = henka_scene_create_entity_named(scene, "Manifest Object")) ==
            HENKA_INVALID_ENTITY ||
        henka_scene_set_entity_transform(scene, entity, authored_transform) !=
            HENKA_SUCCESS ||
        sandbox3d_game_authoring_create(
            scene, scene_path, &authoring) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_register_entity(
            authoring, entity, &document_id) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_save(authoring, project_root) != HENKA_SUCCESS)
    {
        fprintf(stderr, "project manifest test failed during save setup\n");
        goto cleanup;
    }

    if (fopen_s(&manifest, manifest_path, "rb") != 0)
    {
        manifest = NULL;
    }
    if (manifest == NULL)
    {
        fprintf(stderr, "project manifest was not written\n");
        goto cleanup;
    }
    fclose(manifest);
    manifest = NULL;

    if ((changed_result = henka_scene_set_entity_transform(
            scene, entity, changed_transform)) != HENKA_SUCCESS ||
        (reloader_create_result = sandbox3d_game_authoring_create(
            scene, wrong_scene_path, &reloader)) != HENKA_SUCCESS ||
        (reloader_register_result = sandbox3d_game_authoring_register_entity(
            reloader, entity, &reloader_document_id)) != HENKA_SUCCESS ||
        reloader_document_id != document_id ||
        (load_result = sandbox3d_game_authoring_load(reloader, project_root)) !=
            HENKA_SUCCESS ||
        (transform_result = henka_scene_get_entity_transform(
            scene, entity, &loaded_transform)) != HENKA_SUCCESS ||
        !test_float_close(loaded_transform.position.x, authored_transform.position.x))
    {
        fprintf(
            stderr,
            "project manifest was not used to select the scene (changed=%d create=%d register=%d load=%d transform=%d x=%f)\n",
            (int)changed_result,
            (int)reloader_create_result,
            (int)reloader_register_result,
            (int)load_result,
            (int)transform_result,
            (double)loaded_transform.position.x);
        goto cleanup;
    }

    if (remove(manifest_path) != 0 ||
        henka_scene_set_entity_transform(scene, entity, changed_transform) !=
            HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(reloader, project_root) != HENKA_SUCCESS ||
        henka_scene_get_entity_transform(scene, entity, &loaded_transform) !=
            HENKA_SUCCESS ||
        !test_float_close(loaded_transform.position.x, authored_transform.position.x))
    {
        fprintf(stderr, "missing project manifest did not use legacy scene path\n");
        goto cleanup;
    }

    if (!test_write_file(
            manifest_path,
            "schema_version=1\nstartup_scene=missing.hscene\n") ||
        henka_scene_set_entity_transform(scene, entity, changed_transform) !=
            HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(reloader, project_root) == HENKA_SUCCESS ||
        henka_scene_get_entity_transform(scene, entity, &loaded_transform) !=
            HENKA_SUCCESS ||
        !test_float_close(loaded_transform.position.x, changed_transform.position.x))
    {
        fprintf(stderr, "missing selected scene changed live state\n");
        goto cleanup;
    }

    if (!test_write_file(
            manifest_path,
            "schema_version=1\nstartup_scene=../outside.hscene\n") ||
        henka_scene_set_entity_transform(scene, entity, authored_transform) !=
            HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(reloader, project_root) == HENKA_SUCCESS ||
        henka_scene_get_entity_transform(scene, entity, &loaded_transform) !=
            HENKA_SUCCESS ||
        !test_float_close(loaded_transform.position.x, authored_transform.position.x))
    {
        fprintf(stderr, "traversal manifest changed live state\n");
        goto cleanup;
    }

    if (!test_write_file(manifest_path, "schema_version=1\n") ||
        henka_scene_set_entity_transform(scene, entity, changed_transform) !=
            HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(reloader, project_root) == HENKA_SUCCESS ||
        henka_scene_get_entity_transform(scene, entity, &loaded_transform) !=
            HENKA_SUCCESS ||
        !test_float_close(loaded_transform.position.x, changed_transform.position.x))
    {
        fprintf(stderr, "malformed project manifest changed live state\n");
        goto cleanup;
    }

    if (!test_project_reopen_cycle())
    {
        fprintf(stderr, "project reopen cycle failed\n");
        goto cleanup;
    }
    if (!test_project_reopen_materializes_asset())
    {
        fprintf(stderr, "project asset reopen did not materialize the source\n");
        goto cleanup;
    }
    if (!test_project_reopen_materializes_primitives())
    {
        fprintf(stderr, "project primitive reopen did not materialize the sources\n");
        goto cleanup;
    }
    if (!test_project_reopen_materializes_authoring_mesh())
    {
        fprintf(stderr, "project authoring-mesh reopen did not materialize the source\n");
        goto cleanup;
    }

    result = 0;

cleanup:
    if (manifest != NULL)
    {
        fclose(manifest);
    }
    remove(manifest_path);
    remove(saved_scene_path);
    sandbox3d_game_authoring_destroy(reloader);
    sandbox3d_game_authoring_destroy(authoring);
    henka_scene_destroy(scene);
    return result;
}
