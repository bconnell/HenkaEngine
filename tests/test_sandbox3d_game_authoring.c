#include <stdio.h>

#include <henka/physics.h>
#include <henka/scene.h>

#include "../examples/sandbox3d/game_authoring.h"

int main(void)
{
    const char* relative_path = "build/test_tmp/game_authoring_slice.hscene";
    henka_scene* scene = NULL;
    sandbox3d_game_authoring* authoring = NULL;
    henka_scene_document_object object;
    henka_scene_document_object restored;
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id restored_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_interaction_desc interaction;
    henka_transform transform;
    int exit_code = 1;

    if (henka_scene_create(&scene) != HENKA_SUCCESS ||
        (entity = henka_scene_create_entity_named(scene, "Game Authoring Object")) == HENKA_INVALID_ENTITY ||
        henka_scene_set_entity_interaction(
            scene,
            entity,
            &(henka_interaction_desc){true, 4.0f, "Use object"}) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_create(scene, relative_path, &authoring) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_register_entity(authoring, entity, &object_id) != HENKA_SUCCESS ||
        object_id == HENKA_INVALID_SCENE_DOCUMENT_ID ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &restored_id, &object) != HENKA_SUCCESS ||
        restored_id != object_id || !object.interaction.enabled ||
        object.interaction.max_distance != 4.0f)
    {
        goto cleanup;
    }

    object.physics.enabled = true;
    object.physics.body_type = HENKA_PHYSICS_BODY_DYNAMIC;
    object.physics.shape = HENKA_PHYSICS_SHAPE_SPHERE;
    object.physics.sphere_radius = 0.5f;
    object.physics.mass = 1.0f;
    if (sandbox3d_game_authoring_update_object_for_entity(authoring, entity, &object) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_save(authoring, ".") != HENKA_SUCCESS ||
        henka_scene_set_entity_interaction(
            scene,
            entity,
            &(henka_interaction_desc){false, 0.0f, "Changed"}) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(authoring, ".") != HENKA_SUCCESS ||
        henka_scene_get_entity_interaction(scene, entity, &interaction) != HENKA_SUCCESS ||
        !interaction.enabled || interaction.max_distance != 4.0f)
    {
        goto cleanup;
    }
    if (sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &restored_id, &restored) != HENKA_SUCCESS ||
        restored_id != object_id || !restored.physics.enabled ||
        restored.physics.body_type != HENKA_PHYSICS_BODY_DYNAMIC ||
        !restored.interaction.enabled)
    {
        goto cleanup;
    }

    {
        FILE* invalid_file = fopen(relative_path, "wb");
        int invalid_write_ok = invalid_file != NULL;
        if (invalid_write_ok && fputs("not an HSCN document", invalid_file) == EOF)
        {
            invalid_write_ok = 0;
        }
        if (invalid_file != NULL)
        {
            if (fclose(invalid_file) != 0)
            {
                invalid_write_ok = 0;
            }
            invalid_file = NULL;
        }
        if (!invalid_write_ok ||
            sandbox3d_game_authoring_load(authoring, ".") == HENKA_SUCCESS ||
            henka_scene_get_entity_interaction(scene, entity, &interaction) != HENKA_SUCCESS ||
            !interaction.enabled || interaction.max_distance != 4.0f ||
            sandbox3d_game_authoring_get_object_for_entity(
                authoring, entity, &restored_id, &restored) != HENKA_SUCCESS ||
            restored_id != object_id || !restored.interaction.enabled)
        {
            if (invalid_file != NULL)
            {
                fclose(invalid_file);
            }
            goto cleanup;
        }
    }

    transform = restored.transform;
    if (sandbox3d_game_authoring_start_play(authoring) != HENKA_SUCCESS ||
        !sandbox3d_game_authoring_is_play_locked(authoring) ||
        sandbox3d_game_authoring_update_object_for_entity(authoring, entity, &restored) != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_step_play(authoring) != HENKA_SUCCESS ||
        henka_scene_get_entity_transform(scene, entity, &restored.transform) != HENKA_SUCCESS ||
        restored.transform.position.y >= transform.position.y ||
        sandbox3d_game_authoring_stop_play(authoring) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        henka_scene_get_entity_transform(scene, entity, &restored.transform) != HENKA_SUCCESS ||
        restored.transform.position.y != transform.position.y)
    {
        goto cleanup;
    }
    exit_code = 0;

cleanup:
    sandbox3d_game_authoring_destroy(authoring);
    henka_scene_destroy(scene);
    return exit_code;
}
