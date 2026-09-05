#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/audio.h>
#include <henka/camera.h>
#include <henka/core.h>
#include <henka/physics.h>
#include <henka/scene.h>

#include "../examples/sandbox3d/game_authoring.h"

static void test_write_u16(unsigned char* bytes, size_t offset, uint16_t value)
{
    bytes[offset] = (unsigned char)(value & 0xffU);
    bytes[offset + 1U] = (unsigned char)((value >> 8U) & 0xffU);
}

static void test_write_u32(unsigned char* bytes, size_t offset, uint32_t value)
{
    bytes[offset] = (unsigned char)(value & 0xffU);
    bytes[offset + 1U] = (unsigned char)((value >> 8U) & 0xffU);
    bytes[offset + 2U] = (unsigned char)((value >> 16U) & 0xffU);
    bytes[offset + 3U] = (unsigned char)((value >> 24U) & 0xffU);
}

static bool test_write_audio_fixture(const char* path)
{
    const size_t frame_count = 128U;
    const size_t data_size = frame_count * sizeof(int16_t);
    const size_t file_size = 44U + data_size;
    unsigned char* bytes = (unsigned char*)calloc(1U, file_size);
    FILE* file = NULL;
    size_t frame_index;
    bool success;
    if (bytes == NULL)
    {
        return false;
    }
    memcpy(bytes, "RIFF", 4U);
    test_write_u32(bytes, 4U, (uint32_t)(file_size - 8U));
    memcpy(bytes + 8U, "WAVEfmt ", 8U);
    test_write_u32(bytes, 16U, 16U);
    test_write_u16(bytes, 20U, 1U);
    test_write_u16(bytes, 22U, 1U);
    test_write_u32(bytes, 24U, 48000U);
    test_write_u32(bytes, 28U, 48000U * sizeof(int16_t));
    test_write_u16(bytes, 32U, sizeof(int16_t));
    test_write_u16(bytes, 34U, 16U);
    memcpy(bytes + 36U, "data", 4U);
    test_write_u32(bytes, 40U, (uint32_t)data_size);
    for (frame_index = 0U; frame_index < frame_count; ++frame_index)
    {
        const int16_t sample = (frame_index % 2U) == 0U ? INT16_MAX / 4 : INT16_MIN / 4;
        test_write_u16(bytes, 44U + frame_index * sizeof(int16_t), (uint16_t)sample);
    }
#if defined(_MSC_VER)
    (void)fopen_s(&file, path, "wb");
#else
    file = fopen(path, "wb");
#endif
    success = file != NULL && fwrite(bytes, 1U, file_size, file) == file_size;
    if (file != NULL && fclose(file) != 0)
    {
        success = false;
    }
    free(bytes);
    return success;
}

int main(void)
{
    const char* relative_path = "build/test_tmp/game_authoring_slice.hscene";
    const char* audio_path = "build/test_tmp/game_authoring_audio.wav";
    henka_scene* scene = NULL;
    sandbox3d_game_authoring* authoring = NULL;
    henka_audio_system* audio_system = NULL;
    henka_scene_document_object object;
    henka_scene_document_object restored;
    henka_scene_document* replacement_document = NULL;
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id child_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id restored_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id duplicate_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_behavior_id behavior_id =
        HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID;
    henka_scene_document_behavior_id state_behavior_id =
        HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID;
    henka_scene_document_behavior behavior;
    henka_scene_document_behavior loaded_behavior;
    henka_camera camera;
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_entity child_entity = HENKA_INVALID_ENTITY;
    henka_entity restored_parent = HENKA_INVALID_ENTITY;
    henka_interaction_desc interaction;
    henka_transform transform;
    henka_scene* play_scene;
    henka_script_state_value state_value;
    henka_script_source_diagnostic reload_diagnostic;
    bool state_present;
    henka_result save_result;
    henka_result detach_result;
    henka_result load_result;
    henka_result parent_result;
    int exit_code = 1;

    play_scene = NULL;

    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD,
        16.0f / 9.0f,
        0.1f,
        100.0f);
    if (henka_scene_create(&scene) != HENKA_SUCCESS ||
        (entity = henka_scene_create_entity_named(scene, "Game Authoring Object")) == HENKA_INVALID_ENTITY ||
        henka_scene_set_camera(scene, &camera) != HENKA_SUCCESS ||
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

    {
        henka_scene_document_behavior state_behavior =
            henka_scene_document_behavior_default();
        state_behavior.language = HENKA_SCRIPT_LANGUAGE_HENKASCRIPT;
        (void)snprintf(
            state_behavior.asset_path,
            sizeof(state_behavior.asset_path),
            "%s",
            "tests/fixtures/scripts/state_mutator.hks");
        if (sandbox3d_game_authoring_add_behavior_for_entity(
                authoring,
                entity,
                &state_behavior,
                &state_behavior_id) != HENKA_SUCCESS ||
            state_behavior_id == HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID ||
            sandbox3d_game_authoring_get_behavior_count_for_entity(authoring, entity) != 2U)
        {
            fprintf(stderr, "game authoring test failed during state behavior authoring\n");
            goto cleanup;
        }
        if (sandbox3d_game_authoring_get_object_for_entity(
                authoring, entity, &restored_id, &object) != HENKA_SUCCESS)
        {
            fprintf(stderr, "game authoring test failed refreshing authored object\n");
            goto cleanup;
        }
    }

    object.physics.enabled = true;
    object.physics.body_type = HENKA_PHYSICS_BODY_DYNAMIC;
    object.physics.shape = HENKA_PHYSICS_SHAPE_SPHERE;
    object.physics.sphere_radius = 0.5f;
    object.physics.mass = 1.0f;
    object.audio.enabled = true;
    object.audio.looping = true;
    object.audio.spatial = true;
    (void)snprintf(object.audio.clip_path, sizeof(object.audio.clip_path), "%s", audio_path);
    if (sandbox3d_game_authoring_update_object_for_entity(authoring, entity, &object) != HENKA_SUCCESS ||
        !test_write_audio_fixture(audio_path) ||
        sandbox3d_game_authoring_save(authoring, ".") != HENKA_SUCCESS ||
        sandbox3d_game_authoring_set_script_state_value(
            authoring,
            (henka_script_state_identity){object_id, 10U},
            3U,
            (henka_script_state_value){
                HENKA_SCRIPT_STATE_VALUE_I32, {.i32 = 77}}) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_save_play_state(authoring, ".") != HENKA_SUCCESS ||
        sandbox3d_game_authoring_set_script_state_value(
            authoring,
            (henka_script_state_identity){object_id, 10U},
            3U,
            (henka_script_state_value){
                HENKA_SCRIPT_STATE_VALUE_I32, {.i32 = 99}}) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_load_play_state(authoring, ".") != HENKA_SUCCESS ||
        sandbox3d_game_authoring_get_script_state_value(
            authoring,
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
    if (sandbox3d_game_authoring_get_behavior_count_for_entity(authoring, entity) != 2U ||
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
        sandbox3d_game_authoring_load(authoring, ".") != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &restored_id, &restored) != HENKA_SUCCESS ||
            restored_id != object_id ||
            strcmp(restored.name, "Game Authoring Object") != 0)
    {
        fprintf(stderr, "game authoring test failed during persistent-ID remap\n");
        goto cleanup;
    }
    henka_scene_document_destroy(replacement_document);
    replacement_document = NULL;

    if (henka_scene_document_create(&replacement_document) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    {
        henka_scene_document_object unrelated =
            henka_scene_document_object_default();
        unrelated.id = UINT64_C(7000);
        (void)snprintf(
            unrelated.name,
            sizeof(unrelated.name),
            "%s",
            "Unrelated persisted object");
        if (henka_scene_document_add_object(
                replacement_document,
                &unrelated,
                &duplicate_id) != HENKA_SUCCESS ||
            duplicate_id != unrelated.id ||
            henka_scene_document_save_file(
                replacement_document,
                ".",
                relative_path) != HENKA_SUCCESS ||
            sandbox3d_game_authoring_load(authoring, ".") != HENKA_ERROR_INVALID_ARGUMENT ||
            sandbox3d_game_authoring_get_object_for_entity(
                authoring, entity, &restored_id, &restored) != HENKA_SUCCESS ||
            restored_id != object_id ||
            strcmp(restored.name, "Game Authoring Object") != 0)
        {
            fprintf(
                stderr,
                "game authoring test failed during missing-object retention\n");
            goto cleanup;
        }
    }
    henka_scene_document_destroy(replacement_document);
    replacement_document = NULL;

    if (henka_scene_document_create(&replacement_document) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    {
        henka_scene_document_object duplicate = restored;
        restored.id = UINT64_C(6000);
        duplicate.id = UINT64_C(6001);
        duplicate.behavior_count = 0U;
        if (henka_scene_document_add_object(
                replacement_document,
                &restored,
                &restored_id) != HENKA_SUCCESS ||
            henka_scene_document_add_object(
                replacement_document,
                &duplicate,
                &duplicate_id) != HENKA_SUCCESS ||
            henka_scene_document_save_file(
                replacement_document,
                ".",
                relative_path) != HENKA_SUCCESS ||
            sandbox3d_game_authoring_load(authoring, ".") == HENKA_SUCCESS ||
            sandbox3d_game_authoring_get_object_for_entity(
                authoring, entity, &restored_id, &restored) != HENKA_SUCCESS ||
            restored_id != object_id)
        {
            fprintf(stderr, "game authoring test failed during ambiguous-name rejection\n");
            goto cleanup;
        }
    }
    henka_scene_document_remove_object(replacement_document, duplicate_id);
    if (henka_scene_document_save_file(
            replacement_document,
            ".",
            relative_path) != HENKA_SUCCESS)
    {
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
            restored_id != object_id || !restored.interaction.enabled)
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
    if (sandbox3d_game_authoring_start_play(authoring) != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_get_play_scene(authoring) != NULL ||
        henka_audio_system_create(&(henka_audio_system_config){0U}, &audio_system) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_set_audio_system(authoring, audio_system) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_get_play_scene(authoring) != NULL ||
        sandbox3d_game_authoring_start_play(authoring) != HENKA_SUCCESS ||
        (play_scene = sandbox3d_game_authoring_get_play_scene(authoring)) == NULL ||
        play_scene == scene ||
        henka_audio_system_get_active_voice_count(audio_system) != 1U ||
        !sandbox3d_game_authoring_is_play_locked(authoring) ||
        sandbox3d_game_authoring_set_audio_system(authoring, NULL) != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_pause_play(authoring) != HENKA_SUCCESS ||
        !sandbox3d_game_authoring_is_play_locked(authoring) ||
        sandbox3d_game_authoring_save(authoring, ".") != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_load(authoring, ".") != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_save_play_state(authoring, ".") != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_load_play_state(authoring, ".") != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_set_script_state_value(
            authoring,
            (henka_script_state_identity){object_id, 10U},
            3U,
            (henka_script_state_value){
                HENKA_SCRIPT_STATE_VALUE_I32, {.i32 = 101}}) != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_get_script_state_value(
            authoring,
            (henka_script_state_identity){object_id, 10U},
            3U,
            &state_value,
            &state_present) != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_game_authoring_reload_behavior_for_entity(
            authoring, entity, behavior_id, &reload_diagnostic) != HENKA_SUCCESS ||
        reload_diagnostic.result != HENKA_SUCCESS ||
        reload_diagnostic.message[0] != '\0' ||
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
        henka_audio_system_get_active_voice_count(audio_system) != 0U ||
        sandbox3d_game_authoring_set_audio_system(authoring, NULL) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        sandbox3d_game_authoring_get_play_scene(authoring) != NULL ||
        henka_scene_get_entity_transform(scene, entity, &restored.transform) != HENKA_SUCCESS ||
        restored.transform.position.y != transform.position.y)
    {
        fprintf(stderr, "game authoring test failed during Play lifecycle\n");
        goto cleanup;
    }
    if (sandbox3d_game_authoring_get_script_state_value(
            authoring,
            (henka_script_state_identity){restored_id, state_behavior_id},
            80U,
            &state_value,
            &state_present) != HENKA_SUCCESS ||
        state_present)
    {
        fprintf(
            stderr,
            "game authoring test failed: Play state leaked (present=%d type=%d value=%d)\n",
            state_present ? 1 : 0,
            (int)state_value.type,
            state_value.type == HENKA_SCRIPT_STATE_VALUE_I32 ? state_value.as.i32 : 0);
        goto cleanup;
    }
    if (sandbox3d_game_authoring_save_play_state(authoring, ".") != HENKA_SUCCESS ||
        sandbox3d_game_authoring_load_play_state(authoring, ".") != HENKA_SUCCESS ||
        sandbox3d_game_authoring_get_script_state_value(
            authoring,
            (henka_script_state_identity){restored_id, state_behavior_id},
            80U,
            &state_value,
            &state_present) != HENKA_SUCCESS ||
        !state_present || state_value.type != HENKA_SCRIPT_STATE_VALUE_I32 ||
        state_value.as.i32 != 7)
    {
        fprintf(
            stderr,
            "game authoring test failed: explicit Play state save/load (present=%d type=%d value=%d)\n",
            state_present ? 1 : 0,
            (int)state_value.type,
            state_value.type == HENKA_SCRIPT_STATE_VALUE_I32 ? state_value.as.i32 : 0);
        goto cleanup;
    }
    if (sandbox3d_game_authoring_reload_behavior_for_entity(
            authoring, entity, behavior_id, &reload_diagnostic) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        reload_diagnostic.result != HENKA_ERROR_INVALID_ARGUMENT ||
        reload_diagnostic.message[0] == '\0')
    {
        fprintf(stderr, "game authoring test failed stopped reload rejection\n");
        goto cleanup;
    }

    {
        char template_path[512];
        FILE* template_file = NULL;
        int template_path_length;
        (void)remove(
            "build/test_tmp/game_authoring_scripts/scripts/behavior_5000_3.lua");
        if (sandbox3d_game_authoring_attach_script_template(
                authoring,
                "build/test_tmp/game_authoring_scripts",
                entity,
                HENKA_SCRIPT_LANGUAGE_LUA) != HENKA_SUCCESS ||
            sandbox3d_game_authoring_get_behavior_count_for_entity(authoring, entity) != 3U ||
            sandbox3d_game_authoring_get_behavior_at_for_entity(
                authoring, entity, 2U, &loaded_behavior) != HENKA_SUCCESS ||
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
    if (sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &object_id, &object) != HENKA_SUCCESS)
    {
        fprintf(stderr, "game authoring test failed refreshing parent identity\n");
        goto cleanup;
    }
    child_entity = henka_scene_create_entity_named(scene, "Game Authoring Child");
    if (child_entity == HENKA_INVALID_ENTITY ||
        henka_scene_set_entity_transform(
            scene,
            child_entity,
            (henka_transform){{8.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}}) != HENKA_SUCCESS ||
        henka_scene_set_entity_parent(
            scene,
            child_entity,
            entity,
            HENKA_SCENE_PARENT_KEEP_LOCAL) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_register_entity(
            authoring, child_entity, &child_id) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, child_entity, &restored_id, &restored) != HENKA_SUCCESS ||
        restored_id != child_id ||
        restored.parent_id != object_id)
    {
        fprintf(stderr, "game authoring test failed during hierarchy setup\n");
        goto cleanup;
    }
    restored.parent_id = object_id;
    if (sandbox3d_game_authoring_update_object_for_entity(
            authoring, child_entity, &restored) != HENKA_SUCCESS ||
        henka_scene_get_entity_parent(
            scene, child_entity, &restored_parent) != HENKA_SUCCESS ||
        restored_parent != entity)
    {
        fprintf(stderr, "game authoring test failed during hierarchy update\n");
        goto cleanup;
    }
    save_result = sandbox3d_game_authoring_save(authoring, ".");
    detach_result = henka_scene_set_entity_parent(
        scene,
        child_entity,
        HENKA_INVALID_ENTITY,
        HENKA_SCENE_PARENT_KEEP_WORLD);
    load_result = sandbox3d_game_authoring_load(authoring, ".");
    parent_result = henka_scene_get_entity_parent(
        scene, child_entity, &restored_parent);
    if (save_result != HENKA_SUCCESS ||
        detach_result != HENKA_SUCCESS ||
        load_result != HENKA_SUCCESS ||
        parent_result != HENKA_SUCCESS ||
        restored_parent != entity)
    {
        fprintf(
            stderr,
            "game authoring test failed during hierarchy reload (save=%d detach=%d load=%d parent=%d restored=%llu expected=%llu)\n",
            (int)save_result,
            (int)detach_result,
            (int)load_result,
            (int)parent_result,
            (unsigned long long)restored_parent,
            (unsigned long long)entity);
        goto cleanup;
    }
    exit_code = 0;

cleanup:
    henka_scene_document_destroy(replacement_document);
    sandbox3d_game_authoring_destroy(authoring);
    henka_audio_system_destroy(audio_system);
    henka_scene_destroy(scene);
    (void)remove(audio_path);
    return exit_code;
}
