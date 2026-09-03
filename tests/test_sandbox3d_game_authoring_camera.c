#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <henka/camera.h>
#include <henka/core.h>
#include <henka/scene.h>
#include <henka/scene_document.h>

#include "../examples/sandbox3d/game_authoring.h"

static bool test_camera_matches(
    const henka_camera* left,
    const henka_camera* right)
{
    const float epsilon = 0.0001f;
    return left != NULL && right != NULL &&
        fabsf(left->position.x - right->position.x) <= epsilon &&
        fabsf(left->position.y - right->position.y) <= epsilon &&
        fabsf(left->position.z - right->position.z) <= epsilon &&
        fabsf(left->yaw_radians - right->yaw_radians) <= epsilon &&
        fabsf(left->pitch_radians - right->pitch_radians) <= epsilon &&
        fabsf(left->roll_radians - right->roll_radians) <= epsilon &&
        left->projection_mode == right->projection_mode &&
        fabsf(left->field_of_view_radians - right->field_of_view_radians) <= epsilon &&
        fabsf(left->orthographic_height - right->orthographic_height) <= epsilon &&
        fabsf(left->near_plane - right->near_plane) <= epsilon &&
        fabsf(left->far_plane - right->far_plane) <= epsilon &&
        fabsf(left->aspect_ratio - right->aspect_ratio) <= epsilon &&
        fabsf(left->movement_speed - right->movement_speed) <= epsilon &&
        fabsf(left->fast_movement_multiplier - right->fast_movement_multiplier) <= epsilon;
}

static bool test_write_invalid_document(const char* path)
{
    FILE* file = NULL;
    bool success;
#if defined(_MSC_VER)
    success = fopen_s(&file, path, "wb") == 0 && file != NULL;
#else
    file = fopen(path, "wb");
    success = file != NULL;
#endif
    if (!success)
    {
        return false;
    }
    success = fputs("not an HSCN document", file) >= 0 && fclose(file) == 0;
    return success;
}

int main(void)
{
    const char* relative_path = "build/test_tmp/game_authoring_camera.hscene";
    henka_scene* scene = NULL;
    sandbox3d_game_authoring* authoring = NULL;
    henka_scene_document* replacement = NULL;
    henka_scene_document_object object;
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id replacement_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_camera authored_camera;
    henka_camera runtime_camera;
    henka_camera loaded_camera;
    int result = 1;

    if (henka_scene_create(&scene) != HENKA_SUCCESS ||
        (entity = henka_scene_create_entity_named(scene, "Camera Persistence Object")) == HENKA_INVALID_ENTITY ||
        sandbox3d_game_authoring_create(scene, relative_path, &authoring) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_register_entity(authoring, entity, &object_id) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &object_id, &object) != HENKA_SUCCESS)
    {
        fprintf(stderr, "game authoring camera test failed during setup\n");
        goto cleanup;
    }

    authored_camera = henka_camera_create_perspective(
        58.0f * HENKA_DEG_TO_RAD,
        16.0f / 10.0f,
        0.25f,
        500.0f);
    authored_camera.position = (henka_vec3){4.0f, 3.0f, 8.0f};
    authored_camera.yaw_radians = -0.8f;
    authored_camera.pitch_radians = 0.35f;
    authored_camera.roll_radians = 0.1f;
    if (henka_scene_set_camera(scene, &authored_camera) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_save(authoring, ".") != HENKA_SUCCESS)
    {
        fprintf(stderr, "game authoring camera test failed during save\n");
        goto cleanup;
    }

    runtime_camera = authored_camera;
    runtime_camera.position = (henka_vec3){-6.0f, 2.0f, 1.0f};
    runtime_camera.yaw_radians = 0.2f;
    if (henka_scene_set_camera(scene, &runtime_camera) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(authoring, ".") != HENKA_SUCCESS ||
        henka_scene_get_camera(scene, &loaded_camera) != HENKA_SUCCESS ||
        !test_camera_matches(&loaded_camera, &authored_camera))
    {
        fprintf(stderr, "game authoring camera test failed during authored camera load\n");
        goto cleanup;
    }

    if (henka_scene_document_create(&replacement) != HENKA_SUCCESS ||
        henka_scene_document_add_object(replacement, &object, &replacement_id) != HENKA_SUCCESS ||
        henka_scene_document_save_file(replacement, ".", relative_path) != HENKA_SUCCESS)
    {
        fprintf(stderr, "game authoring camera test failed creating legacy document\n");
        goto cleanup;
    }
    runtime_camera.position = (henka_vec3){-3.0f, 7.0f, 2.0f};
    runtime_camera.yaw_radians = 1.1f;
    runtime_camera.pitch_radians = -0.2f;
    if (henka_scene_set_camera(scene, &runtime_camera) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(authoring, ".") != HENKA_SUCCESS ||
        henka_scene_get_camera(scene, &loaded_camera) != HENKA_SUCCESS ||
        !test_camera_matches(&loaded_camera, &runtime_camera))
    {
        fprintf(stderr, "game authoring camera test failed during legacy camera retention\n");
        goto cleanup;
    }
    henka_scene_document_destroy(replacement);
    replacement = NULL;

    if (!test_write_invalid_document(relative_path))
    {
        fprintf(stderr, "game authoring camera test failed creating invalid document\n");
        goto cleanup;
    }
    if (henka_scene_get_camera(scene, &runtime_camera) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(authoring, ".") == HENKA_SUCCESS ||
        henka_scene_get_camera(scene, &loaded_camera) != HENKA_SUCCESS ||
        !test_camera_matches(&loaded_camera, &runtime_camera))
    {
        fprintf(stderr, "game authoring camera test failed during invalid-load retention\n");
        goto cleanup;
    }
    result = 0;

cleanup:
    remove(relative_path);
    henka_scene_document_destroy(replacement);
    sandbox3d_game_authoring_destroy(authoring);
    henka_scene_destroy(scene);
    return result;
}
