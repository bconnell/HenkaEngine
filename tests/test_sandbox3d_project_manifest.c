#include <stdio.h>
#include <string.h>

#include <henka/core.h>
#include <henka/camera.h>
#include <henka/scene.h>

#include "../examples/sandbox3d/game_authoring.h"

static int test_float_close(float left, float right)
{
    const float difference = left - right;
    return difference < 0.0001f && difference > -0.0001f;
}

static int test_write_manifest(const char* path, const char* contents)
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

    if (!test_write_manifest(
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

    if (!test_write_manifest(
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

    if (!test_write_manifest(manifest_path, "schema_version=1\n") ||
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
