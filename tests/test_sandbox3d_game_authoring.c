#include <stdio.h>
#include <string.h>

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
    henka_scene_document* replacement_document = NULL;
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id restored_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_behavior_id behavior_id =
        HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID;
    henka_scene_document_behavior behavior;
    henka_scene_document_behavior loaded_behavior;
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_interaction_desc interaction;
    henka_transform transform;
    henka_scene* play_scene;
    henka_script_state_store* state_store;
    henka_script_state_value state_value;
    bool state_present;
    int exit_code = 1;

    play_scene = NULL;

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
        fprintf(stderr, "game authoring test failed during setup\n");
        goto cleanup;
    }

    behavior = henka_scene_document_behavior_default();
    behavior.language = HENKA_SCRIPT_LANGUAGE_HENKASCRIPT;
    (void)snprintf(
        behavior.asset_path,
        sizeof(behavior.asset_path),
        "%s",
        "tests/fixtures/scripts/mixed.hks");
    if (sandbox3d_game_authoring_add_behavior_for_entity(
            authoring, entity, &behavior, &behavior_id) != HENKA_SUCCESS ||
        behavior_id == HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID ||
        sandbox3d_game_authoring_get_behavior_count_for_entity(authoring, entity) != 1U ||
        sandbox3d_game_authoring_get_behavior_for_entity(
            authoring, entity, behavior_id, &loaded_behavior) != HENKA_SUCCESS ||
        loaded_behavior.language != HENKA_SCRIPT_LANGUAGE_HENKASCRIPT ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &restored_id, &object) != HENKA_SUCCESS)
    {
        fprintf(stderr, "game authoring test failed during behavior authoring\n");
        goto cleanup;
    }

    object.physics.enabled = true;
    object.physics.body_type = HENKA_PHYSICS_BODY_DYNAMIC;
    object.physics.shape = HENKA_PHYSICS_SHAPE_SPHERE;
    object.physics.sphere_radius = 0.5f;
    object.physics.mass = 1.0f;
    if (sandbox3d_game_authoring_update_object_for_entity(authoring, entity, &object) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_save(authoring, ".") != HENKA_SUCCESS ||
        (state_store = sandbox3d_game_authoring_get_script_state_store(authoring)) == NULL ||
        henka_script_state_store_set(
            state_store,
            (henka_script_state_identity){object_id, 10U},
            3U,
            (henka_script_state_value){
                HENKA_SCRIPT_STATE_VALUE_I32, {.i32 = 77}}) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_save_play_state(authoring, ".") != HENKA_SUCCESS ||
        henka_script_state_store_set(
            state_store,
            (henka_script_state_identity){object_id, 10U},
            3U,
            (henka_script_state_value){
                HENKA_SCRIPT_STATE_VALUE_I32, {.i32 = 99}}) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_load_play_state(authoring, ".") != HENKA_SUCCESS ||
        henka_script_state_store_get(
            state_store,
            (henka_script_state_identity){object_id, 10U},
            3U,
            &state_value,
            &state_present) != HENKA_SUCCESS ||
        !state_present || state_value.type != HENKA_SCRIPT_STATE_VALUE_I32 ||
        state_value.as.i32 != 77 ||
        henka_scene_set_entity_interaction(
            scene,
            entity,
            &(henka_interaction_desc){false, 0.0f, "Changed"}) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(authoring, ".") != HENKA_SUCCESS ||
        henka_scene_get_entity_interaction(scene, entity, &interaction) != HENKA_SUCCESS ||
        !interaction.enabled || interaction.max_distance != 4.0f)
    {
        fprintf(stderr, "game authoring test failed during save/load\n");
        goto cleanup;
    }
    if (sandbox3d_game_authoring_get_behavior_count_for_entity(authoring, entity) != 1U ||
        sandbox3d_game_authoring_get_behavior_for_entity(
            authoring, entity, behavior_id, &loaded_behavior) != HENKA_SUCCESS ||
        strcmp(loaded_behavior.asset_path, behavior.asset_path) != 0)
    {
        fprintf(stderr, "game authoring test failed during behavior persistence\n");
        goto cleanup;
    }
    if (sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &restored_id, &restored) != HENKA_SUCCESS ||
        restored_id != object_id || !restored.physics.enabled ||
        restored.physics.body_type != HENKA_PHYSICS_BODY_DYNAMIC ||
        !restored.interaction.enabled)
    {
        fprintf(stderr, "game authoring test failed during authored-object verification\n");
        goto cleanup;
    }

    if (henka_scene_document_create(&replacement_document) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    restored.id = UINT64_C(5000);
    if (henka_scene_document_add_object(
            replacement_document,
            &restored,
            &restored_id) != HENKA_SUCCESS ||
        restored_id != UINT64_C(5000) ||
        henka_scene_document_save_file(
            replacement_document,
            ".",
            relative_path) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_load(authoring, ".") != HENKA_SUCCESS ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &restored_id, &restored) != HENKA_SUCCESS ||
            restored_id != UINT64_C(5000))
    {
        fprintf(stderr, "game authoring test failed during persistent-ID remap\n");
        goto cleanup;
    }
    henka_scene_document_destroy(replacement_document);
    replacement_document = NULL;

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
            restored_id != UINT64_C(5000) || !restored.interaction.enabled)
        {
            if (invalid_file != NULL)
            {
                fclose(invalid_file);
            }
            fprintf(stderr, "game authoring test failed during malformed-load retention\n");
            goto cleanup;
        }
    }

    transform = restored.transform;
    if (sandbox3d_game_authoring_get_play_scene(authoring) != NULL ||
        sandbox3d_game_authoring_start_play(authoring) != HENKA_SUCCESS ||
        (play_scene = sandbox3d_game_authoring_get_play_scene(authoring)) == NULL ||
        play_scene == scene ||
        !sandbox3d_game_authoring_is_play_locked(authoring) ||
        sandbox3d_game_authoring_pause_play(authoring) != HENKA_SUCCESS ||
        !sandbox3d_game_authoring_is_play_locked(authoring) ||
        sandbox3d_game_authoring_save(authoring, ".") != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_load(authoring, ".") != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_save_play_state(authoring, ".") != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_load_play_state(authoring, ".") != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_update_object_for_entity(authoring, entity, &restored) != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_resume_play(authoring) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_tick_play(authoring) != HENKA_SUCCESS ||
        henka_scene_get_entity_transform(play_scene, entity, &restored.transform) != HENKA_SUCCESS ||
        restored.transform.position.y >= transform.position.y ||
        henka_scene_get_entity_transform(scene, entity, &restored.transform) != HENKA_SUCCESS ||
        restored.transform.position.y != transform.position.y ||
        henka_scene_set_entity_visible(play_scene, entity, false) != HENKA_SUCCESS ||
        henka_scene_is_entity_visible(scene, entity) == false ||
        sandbox3d_game_authoring_stop_play(authoring) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        sandbox3d_game_authoring_get_play_scene(authoring) != NULL ||
        henka_scene_get_entity_transform(scene, entity, &restored.transform) != HENKA_SUCCESS ||
        restored.transform.position.y != transform.position.y)
    {
        fprintf(stderr, "game authoring test failed during Play lifecycle\n");
        goto cleanup;
    }

    {
        char template_path[512];
        FILE* template_file = NULL;
        int template_path_length;
        (void)remove(
            "build/test_tmp/game_authoring_scripts/scripts/behavior_5000_2.lua");
        if (sandbox3d_game_authoring_attach_script_template(
                authoring,
                "build/test_tmp/game_authoring_scripts",
                entity,
                HENKA_SCRIPT_LANGUAGE_LUA) != HENKA_SUCCESS ||
            sandbox3d_game_authoring_get_behavior_count_for_entity(authoring, entity) != 2U ||
            sandbox3d_game_authoring_get_behavior_at_for_entity(
                authoring, entity, 1U, &loaded_behavior) != HENKA_SUCCESS ||
            loaded_behavior.language != HENKA_SCRIPT_LANGUAGE_LUA ||
            loaded_behavior.asset_path[0] == '\0')
        {
            fprintf(stderr, "game authoring test failed during script template attachment\n");
            goto cleanup;
        }
        template_path_length = snprintf(
            template_path,
            sizeof(template_path),
            "%s/%s",
            "build/test_tmp/game_authoring_scripts",
            loaded_behavior.asset_path);
        if (template_path_length < 0 ||
            (size_t)template_path_length >= sizeof(template_path) ||
#if defined(_MSC_VER)
            fopen_s(&template_file, template_path, "rb") != 0 ||
#else
            (template_file = fopen(template_path, "rb")) == NULL ||
#endif
            template_file == NULL)
        {
            fprintf(stderr, "game authoring test failed opening generated script template\n");
            goto cleanup;
        }
        if (fclose(template_file) != 0 || remove(template_path) != 0)
        {
            fprintf(stderr, "game authoring test failed during script template cleanup\n");
            goto cleanup;
        }
    }
    exit_code = 0;

cleanup:
    henka_scene_document_destroy(replacement_document);
    sandbox3d_game_authoring_destroy(authoring);
    henka_scene_destroy(scene);
    return exit_code;
}
