#include <stdio.h>

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
