#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/audio.h>
#include <henka/assets.h>
#include <henka/character_controller.h>
#include <henka/input.h>
#include <henka/memory.h>
#include <henka/physics.h>
#include <henka/scene.h>
#include <henka/scene_document.h>

#include "../examples/sandbox3d/play_session.h"
#include "../engine/src/henka_internal.h"
#include "../engine/src/core/memory_internal.h"

typedef struct test_character_controller_input
{
    bool forward;
    bool jump;
    size_t query_count;
} test_character_controller_input;

static bool test_character_controller_input_query(
    void* user_data,
    uint32_t action_id)
{
    test_character_controller_input* input =
        (test_character_controller_input*)user_data;
    if (input == NULL)
    {
        return false;
    }
    ++input->query_count;
    if (action_id == HENKA_INPUT_ACTION_MOVE_FORWARD)
    {
        return input->forward;
    }
    if (action_id == HENKA_INPUT_ACTION_MOVE_UP)
    {
        return input->jump;
    }
    return false;
}

static bool test_character_controller_play_integration(void)
{
    henka_scene_document* document = NULL;
    sandbox3d_scene_document_bridge* bridge = NULL;
    sandbox3d_play_session* session = NULL;
    henka_scene* scene = NULL;
    henka_physics_world* physics_world = NULL;
    henka_scene_document_object ground = henka_scene_document_object_default();
    henka_scene_document_object player = henka_scene_document_object_default();
    henka_scene_document_id ground_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id player_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_entity ground_entity = HENKA_INVALID_ENTITY;
    henka_entity player_entity = HENKA_INVALID_ENTITY;
    henka_transform initial_transform;
    henka_transform moved_transform;
    henka_transform jump_transform = {0};
    sandbox3d_play_character_controller_config controller_config;
    test_character_controller_input input = {false, false, 0U};
    size_t tick_index;
    bool success = false;

    ground.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE;
    ground.source.primitive = HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX;
    ground.source.primitive_dimensions = (henka_vec3){40.0f, 1.0f, 40.0f};
    ground.physics.enabled = true;
    ground.physics.body_type = HENKA_PHYSICS_BODY_STATIC;
    ground.physics.shape = HENKA_PHYSICS_SHAPE_BOX;
    ground.physics.box_half_extents = (henka_vec3){20.0f, 0.5f, 20.0f};
    ground.physics.mass = 0.0f;
    ground.transform.position = (henka_vec3){0.0f, -0.5f, 0.0f};
    player.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE;
    player.source.primitive = HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX;
    player.source.primitive_dimensions = (henka_vec3){1.0f, 2.0f, 1.0f};
    player.transform.position = (henka_vec3){0.0f, 2.0f, 0.0f};
    (void)snprintf(ground.name, sizeof(ground.name), "%s", "Controller Ground");
    (void)snprintf(player.name, sizeof(player.name), "%s", "Controller Player");

    if (henka_scene_document_create(&document) != HENKA_SUCCESS ||
        henka_scene_create(&scene) != HENKA_SUCCESS ||
        henka_physics_world_create(&physics_world) != HENKA_SUCCESS ||
        henka_scene_document_add_object(document, &ground, &ground_id) != HENKA_SUCCESS ||
        henka_scene_document_add_object(document, &player, &player_id) != HENKA_SUCCESS ||
        (ground_entity = henka_scene_create_entity_named(scene, ground.name)) == HENKA_INVALID_ENTITY ||
        (player_entity = henka_scene_create_entity_named(scene, player.name)) == HENKA_INVALID_ENTITY ||
        sandbox3d_scene_document_bridge_create(document, scene, &bridge) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_bind(bridge, ground_id, ground_entity) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_bind(bridge, player_id, player_entity) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_apply_object(bridge, ground_id) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_apply_object(bridge, player_id) != HENKA_SUCCESS ||
        sandbox3d_play_session_create(bridge, physics_world, &session) != HENKA_SUCCESS ||
        sandbox3d_play_session_set_input_context(
            session,
            test_character_controller_input_query,
            &input,
            (henka_vec3){0.0f, 1.0f, 4.0f}) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    controller_config = (sandbox3d_play_character_controller_config){
        player_id,
        0.45f,
        0.5f,
        2.0f,
        4.0f,
        0.0f,
        0.0f,
        1.0f,
        45.0f,
        1U,
        HENKA_PHYSICS_ALL_LAYERS};
    if (sandbox3d_play_session_set_character_controller(
            session, &controller_config) != HENKA_SUCCESS ||
        henka_scene_get_entity_transform(scene, player_entity, &initial_transform) != HENKA_SUCCESS ||
        sandbox3d_play_session_start(session) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    input.forward = true;
    for (tick_index = 0U; tick_index < 180U; ++tick_index)
    {
        if (sandbox3d_play_session_tick(session) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    input.forward = false;
    if (henka_scene_get_entity_transform(scene, player_entity, &moved_transform) != HENKA_SUCCESS ||
        moved_transform.position.z >= initial_transform.position.z - 0.5f ||
        moved_transform.position.y < 0.8f ||
        input.query_count == 0U)
    {
        goto cleanup;
    }

    input.jump = true;
    if (sandbox3d_play_session_tick(session) != HENKA_SUCCESS ||
        henka_scene_get_entity_transform(scene, player_entity, &jump_transform) != HENKA_SUCCESS ||
        jump_transform.position.y <= moved_transform.position.y + 0.01f)
    {
        goto cleanup;
    }
    input.jump = false;
    if (sandbox3d_play_session_stop(session) != HENKA_SUCCESS ||
        henka_physics_world_get_body_count(physics_world) != 0U ||
        henka_scene_get_entity_transform(scene, player_entity, &jump_transform) != HENKA_SUCCESS ||
        jump_transform.position.x != initial_transform.position.x ||
        jump_transform.position.y != initial_transform.position.y ||
        jump_transform.position.z != initial_transform.position.z)
    {
        goto cleanup;
    }
    success = true;

cleanup:
    sandbox3d_play_session_destroy(session);
    sandbox3d_scene_document_bridge_destroy(bridge);
    henka_physics_world_destroy(physics_world);
    henka_scene_destroy(scene);
    henka_scene_document_destroy(document);
    return success;
}

static bool test_play_session_stop_retry_after_body_failure(void)
{
    enum { TEST_OBJECT_COUNT = 9 };
    static const henka_vec3 static_positions[TEST_OBJECT_COUNT - 1U] = {
        {2.4f, 0.0f, 0.0f},
        {-2.4f, 0.0f, 0.0f},
        {0.0f, 0.0f, 2.4f},
        {0.0f, 0.0f, -2.4f},
        {1.7f, 0.0f, 1.7f},
        {-1.7f, 0.0f, 1.7f},
        {1.7f, 0.0f, -1.7f},
        {-1.7f, 0.0f, -1.7f}};
    henka_scene_document* document = NULL;
    sandbox3d_scene_document_bridge* bridge = NULL;
    sandbox3d_play_session* session = NULL;
    henka_scene* scene = NULL;
    henka_physics_world* physics_world = NULL;
    henka_scene_document_id object_ids[TEST_OBJECT_COUNT] = {0};
    henka_entity entities[TEST_OBJECT_COUNT] = {0};
    henka_transform initial_transform;
    henka_transform restored_transform;
    henka_result first_stop_result;
    size_t event_count = 0U;
    size_t index;
    bool success = false;

    if (henka_scene_document_create(&document) != HENKA_SUCCESS ||
        henka_scene_create(&scene) != HENKA_SUCCESS ||
        henka_physics_world_create(&physics_world) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < TEST_OBJECT_COUNT; ++index)
    {
        henka_scene_document_object object = henka_scene_document_object_default();
        object.physics.enabled = true;
        object.physics.body_type = index == 0U
            ? HENKA_PHYSICS_BODY_DYNAMIC
            : HENKA_PHYSICS_BODY_STATIC;
        object.physics.shape = HENKA_PHYSICS_SHAPE_SPHERE;
        object.physics.sphere_radius = index == 0U ? 2.0f : 0.5f;
        object.physics.mass = index == 0U ? 1.0f : 0.0f;
        object.transform.position = index == 0U
            ? (henka_vec3){0.0f, 0.0f, 0.0f}
            : static_positions[index - 1U];
        if (snprintf(
                object.name,
                sizeof(object.name),
                "Stop Failure Object %zu",
                index) < 0 ||
            henka_scene_document_add_object(
                document, &object, &object_ids[index]) != HENKA_SUCCESS ||
            (entities[index] = henka_scene_create_entity_named(scene, object.name)) ==
                HENKA_INVALID_ENTITY)
        {
            goto cleanup;
        }
    }
    if (sandbox3d_scene_document_bridge_create(document, scene, &bridge) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < TEST_OBJECT_COUNT; ++index)
    {
        if (sandbox3d_scene_document_bridge_bind(
                bridge, object_ids[index], entities[index]) != HENKA_SUCCESS ||
            sandbox3d_scene_document_bridge_apply_object(
                bridge, object_ids[index]) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (henka_scene_get_entity_transform(
            scene, entities[0], &initial_transform) != HENKA_SUCCESS ||
        sandbox3d_play_session_create(bridge, physics_world, &session) != HENKA_SUCCESS ||
        sandbox3d_play_session_start(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_tick(session) != HENKA_SUCCESS ||
        henka_physics_world_get_events(physics_world, &event_count) == NULL ||
        event_count != TEST_OBJECT_COUNT - 1U ||
        henka_physics_world_get_body_count(physics_world) != TEST_OBJECT_COUNT)
    {
        goto cleanup;
    }

    henka_memory_test_fail_after(0U);
    first_stop_result = sandbox3d_play_session_stop(session);
    if (first_stop_result != HENKA_ERROR_OUT_OF_MEMORY)
    {
        henka_memory_test_disable_failures();
        goto cleanup;
    }
    henka_memory_test_disable_failures();
    if (sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_FAILED ||
        henka_physics_world_get_body_count(physics_world) != TEST_OBJECT_COUNT ||
        !sandbox3d_scene_document_bridge_is_play_locked(bridge) ||
        henka_scene_get_entity_transform(
            scene, entities[0], &restored_transform) != HENKA_SUCCESS ||
        memcmp(&restored_transform, &initial_transform, sizeof(initial_transform)) != 0 ||
        sandbox3d_play_session_stop(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_STOPPED ||
        henka_physics_world_get_body_count(physics_world) != 0U ||
        sandbox3d_scene_document_bridge_is_play_locked(bridge))
    {
        goto cleanup;
    }
    success = true;

cleanup:
    henka_memory_test_disable_failures();
    sandbox3d_play_session_destroy(session);
    sandbox3d_scene_document_bridge_destroy(bridge);
    henka_physics_world_destroy(physics_world);
    henka_scene_destroy(scene);
    henka_scene_document_destroy(document);
    return success;
}

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
    test_write_u32(bytes, 24U, HENKA_AUDIO_DEFAULT_SAMPLE_RATE);
    test_write_u32(bytes, 28U, HENKA_AUDIO_DEFAULT_SAMPLE_RATE * 2U);
    test_write_u16(bytes, 32U, 2U);
    test_write_u16(bytes, 34U, 16U);
    memcpy(bytes + 36U, "data", 4U);
    test_write_u32(bytes, 40U, (uint32_t)data_size);
    for (frame_index = 0U; frame_index < frame_count; ++frame_index)
    {
        test_write_u16(bytes, 44U + frame_index * 2U, 8192U);
    }
#if defined(_WIN32)
    if (fopen_s(&file, path, "wb") != 0)
    {
        file = NULL;
    }
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
    const char* audio_path = "build/test_tmp/play_session_audio.wav";
    henka_scene_document* document = NULL;
    sandbox3d_scene_document_bridge* bridge = NULL;
    sandbox3d_play_session* session = NULL;
    henka_scene* scene = NULL;
    henka_physics_world* physics_world = NULL;
    henka_audio_system* audio_system = NULL;
    henka_engine asset_engine;
    henka_asset_manager* asset_manager = NULL;
    henka_scene_document_object object = henka_scene_document_object_default();
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_audio_listener authored_listener = henka_audio_listener_default();
    henka_audio_listener runtime_listener;
    henka_transform transform;
    henka_transform stepped_transform;
    henka_script_host* script_host;
    henka_script_state_store* script_state_store = NULL;
    henka_script_state_value state_value = {0};
    henka_script_api_value arguments[2];
    henka_script_api_value output;
    henka_script_event script_event;
    henka_script_source_diagnostic reload_diagnostic;
    bool state_present = false;
    henka_result tick_result = HENKA_SUCCESS;
    size_t tick_count;
    int exit_code = 1;

    if (!test_character_controller_play_integration())
    {
        fprintf(stderr, "character controller Play integration test failed\n");
        return 1;
    }
    if (!test_play_session_stop_retry_after_body_failure())
    {
        fprintf(stderr, "Play stop retry after body failure test failed\n");
        return 1;
    }

    object.behavior_count = 2U;
    object.behaviors[0] = henka_scene_document_behavior_default();
    object.behaviors[0].id = 10U;
    object.behaviors[0].language = HENKA_SCRIPT_LANGUAGE_HENKASCRIPT;
    (void)snprintf(
        object.behaviors[0].asset_path,
        sizeof(object.behaviors[0].asset_path),
        "%s",
        "tests/fixtures/scripts/publisher.hks");
    object.behaviors[1] = henka_scene_document_behavior_default();
    object.behaviors[1].id = 11U;
    object.behaviors[1].language = HENKA_SCRIPT_LANGUAGE_LUA;
    (void)snprintf(
        object.behaviors[1].asset_path,
        sizeof(object.behaviors[1].asset_path),
        "%s",
        "tests/fixtures/scripts/subscriber.lua");

    object.audio.enabled = true;
    object.audio.looping = true;
    object.audio.spatial = true;
    object.audio.streaming = true;
    (void)snprintf(object.audio.clip_path, sizeof(object.audio.clip_path), "%s", audio_path);

    memset(&asset_engine, 0, sizeof(asset_engine));
    asset_engine.asset_base_path = ".";
    asset_manager = (henka_asset_manager*)henka_malloc(sizeof(*asset_manager));
    if (asset_manager != NULL)
    {
        memset(asset_manager, 0, sizeof(*asset_manager));
        asset_manager->engine = &asset_engine;
    }
    if (henka_scene_document_create(&document) != HENKA_SUCCESS ||
        henka_scene_create(&scene) != HENKA_SUCCESS ||
        henka_physics_world_create(&physics_world) != HENKA_SUCCESS ||
        henka_script_state_store_create(&script_state_store) != HENKA_SUCCESS ||
        !test_write_audio_fixture(audio_path) ||
        henka_audio_system_create(&(henka_audio_system_config){0U}, &audio_system) != HENKA_SUCCESS ||
        asset_manager == NULL)
    {
        goto cleanup;
    }
    authored_listener.position = (henka_vec3){3.0f, 4.0f, -5.0f};
    authored_listener.forward = (henka_vec3){0.0f, 0.0f, -1.0f};
    authored_listener.up = (henka_vec3){0.0f, 1.0f, 0.0f};
    if (henka_scene_document_set_audio_listener(document, authored_listener) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    object.transform.position = (henka_vec3){0.0f, 5.0f, 0.0f};
    object.physics.enabled = true;
    object.physics.body_type = HENKA_PHYSICS_BODY_DYNAMIC;
    object.physics.shape = HENKA_PHYSICS_SHAPE_SPHERE;
    object.physics.sphere_radius = 0.5f;
    object.physics.mass = 1.0f;
    if (henka_scene_document_add_object(document, &object, &object_id) != HENKA_SUCCESS ||
        (entity = henka_scene_create_entity_named(scene, "Play Object")) == HENKA_INVALID_ENTITY ||
        sandbox3d_scene_document_bridge_create(document, scene, &bridge) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_bind(bridge, object_id, entity) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_apply_object(bridge, object_id) != HENKA_SUCCESS ||
        sandbox3d_play_session_create(bridge, physics_world, &session) != HENKA_SUCCESS ||
        sandbox3d_play_session_set_script_state_store(session, script_state_store) != HENKA_SUCCESS ||
        sandbox3d_play_session_set_audio_system(session, audio_system) != HENKA_SUCCESS ||
        sandbox3d_play_session_set_audio_asset_manager(session, asset_manager) != HENKA_SUCCESS ||
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_STOPPED ||
        sandbox3d_play_session_start(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_RUNNING ||
        henka_audio_system_get_active_voice_count(audio_system) != 1U ||
        henka_audio_system_get_listener(audio_system, &runtime_listener) != HENKA_SUCCESS ||
        runtime_listener.position.x != authored_listener.position.x ||
        runtime_listener.position.y != authored_listener.position.y ||
        runtime_listener.position.z != authored_listener.position.z ||
        runtime_listener.forward.y != authored_listener.forward.y ||
        runtime_listener.up.z != authored_listener.up.z ||
        (script_host = sandbox3d_play_session_get_script_host(session)) == NULL)
    {
        goto cleanup;
    }
    {
        float audio_samples[HENKA_AUDIO_OUTPUT_CHANNELS];
        if (henka_audio_system_mix(audio_system, audio_samples, 1U) != HENKA_SUCCESS ||
            fabsf(audio_samples[0]) <= 0.0001f || fabsf(audio_samples[1]) <= 0.0001f ||
            sandbox3d_play_session_pause(session) != HENKA_SUCCESS ||
            henka_audio_system_mix(audio_system, audio_samples, 1U) != HENKA_SUCCESS ||
            fabsf(audio_samples[0]) > 0.0001f || fabsf(audio_samples[1]) > 0.0001f ||
            sandbox3d_play_session_resume(session) != HENKA_SUCCESS ||
            henka_audio_system_mix(audio_system, audio_samples, 1U) != HENKA_SUCCESS ||
            fabsf(audio_samples[0]) <= 0.0001f || fabsf(audio_samples[1]) <= 0.0001f)
        {
            goto cleanup;
        }
    }
    arguments[0] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = object_id}};
    if (henka_script_host_invoke(
            script_host,
            HENKA_SCRIPT_API_AUDIO_STOP,
            arguments,
            1U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT ||
        output.as.result != HENKA_SUCCESS ||
        henka_audio_system_get_active_voice_count(audio_system) != 0U ||
        henka_script_host_invoke(
            script_host,
            HENKA_SCRIPT_API_AUDIO_IS_PLAYING,
            arguments,
            1U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_BOOL || output.as.boolean ||
        henka_script_host_invoke(
            script_host,
            HENKA_SCRIPT_API_AUDIO_RESTART,
            arguments,
            1U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT ||
        output.as.result != HENKA_SUCCESS ||
        henka_audio_system_get_active_voice_count(audio_system) != 1U ||
        henka_script_host_invoke(
            script_host,
            HENKA_SCRIPT_API_AUDIO_IS_PLAYING,
            arguments,
            1U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_BOOL || !output.as.boolean)
    {
        goto cleanup;
    }
    if (sandbox3d_play_session_reload_behavior(
            session, object_id, 10U, &reload_diagnostic) != HENKA_SUCCESS ||
        reload_diagnostic.result != HENKA_SUCCESS ||
        reload_diagnostic.message[0] != '\0' ||
        sandbox3d_play_session_reload_behavior(
            session, object_id, 999U, &reload_diagnostic) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        reload_diagnostic.result != HENKA_ERROR_INVALID_ARGUMENT ||
        reload_diagnostic.message[0] == '\0' ||
        sandbox3d_play_session_reload_behavior(
            session, object_id, 11U, &reload_diagnostic) != HENKA_SUCCESS ||
        reload_diagnostic.result != HENKA_SUCCESS ||
        henka_script_state_store_get(
            script_state_store,
            (henka_script_state_identity){object_id, 11U},
            90U,
            &state_value,
            &state_present) != HENKA_SUCCESS ||
        !state_present || state_value.type != HENKA_SCRIPT_STATE_VALUE_I32 ||
        state_value.as.i32 != 7)
    {
        goto cleanup;
    }
    if (henka_script_state_store_get(
            script_state_store,
            (henka_script_state_identity){object_id, 11U},
            90U,
            &state_value,
            &state_present) != HENKA_SUCCESS ||
        !state_present || state_value.type != HENKA_SCRIPT_STATE_VALUE_I32 ||
        state_value.as.i32 != 7)
    {
        goto cleanup;
    }
    arguments[0] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = object_id}};
    arguments[1] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_VEC3, {.vec3 = {1.0f, 4.0f, 0.0f}}};
    if (henka_script_host_invoke(
            script_host,
            HENKA_SCRIPT_API_TRANSFORM_SET_POSITION,
            arguments,
            2U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT ||
        output.as.result != HENKA_SUCCESS ||
        henka_script_host_invoke(
            script_host,
            HENKA_SCRIPT_API_TRANSFORM_GET_POSITION,
            arguments,
            1U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_VEC3 ||
        output.as.vec3.x != 1.0f || output.as.vec3.y != 4.0f ||
        output.as.vec3.z != 0.0f)
    {
        goto cleanup;
    }
    if (henka_script_host_invoke(
            script_host,
            HENKA_SCRIPT_API_ENTITY_IS_VALID,
            arguments,
            1U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_BOOL || !output.as.boolean)
    {
        goto cleanup;
    }
    arguments[1].as.vec3 = (henka_vec3){0.0f, -1.0f, 0.0f};
    if (henka_script_host_invoke(
            script_host,
            HENKA_SCRIPT_API_PHYSICS_APPLY_IMPULSE,
            arguments,
            2U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT ||
        output.as.result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    arguments[0].type = HENKA_SCRIPT_API_VALUE_EVENT_ID;
    arguments[0].as.event_id = 7U;
    arguments[1].type = HENKA_SCRIPT_API_VALUE_ENTITY;
    arguments[1].as.entity = object_id;
    if (henka_script_host_invoke(
            script_host,
            HENKA_SCRIPT_API_EVENTS_EMIT,
            arguments,
            2U,
            &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT ||
        output.as.result != HENKA_SUCCESS ||
        henka_script_host_poll_event(
            script_host, &script_event) != HENKA_SUCCESS ||
        script_event.event_id != 7U || script_event.source_entity != object_id ||
        henka_script_host_get_pending_event_count(script_host) != 0U)
    {
        goto cleanup;
    }
    if (sandbox3d_scene_document_bridge_apply_object(bridge, object_id) != HENKA_ERROR_INVALID_ARGUMENT ||
        !sandbox3d_scene_document_bridge_is_play_locked(bridge) ||
        sandbox3d_play_session_pause(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_step_fixed(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_PAUSED ||
        henka_scene_get_entity_transform(scene, entity, &stepped_transform) != HENKA_SUCCESS ||
        stepped_transform.position.y >= 5.0f ||
        sandbox3d_play_session_reload_behavior(
            session, object_id, 10U, &reload_diagnostic) != HENKA_SUCCESS ||
        reload_diagnostic.result != HENKA_SUCCESS ||
        sandbox3d_play_session_resume(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_tick(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_RUNNING ||
        henka_scene_get_entity_transform(scene, entity, &transform) != HENKA_SUCCESS ||
        transform.position.y >= stepped_transform.position.y ||
        henka_scene_set_entity_visible(scene, entity, false) != HENKA_SUCCESS ||
        sandbox3d_play_session_stop(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_STOPPED ||
        sandbox3d_scene_document_bridge_is_play_locked(bridge) ||
        henka_scene_get_entity_transform(scene, entity, &transform) != HENKA_SUCCESS ||
        transform.position.y != 5.0f ||
        !henka_scene_is_entity_visible(scene, entity) ||
        henka_audio_system_get_active_voice_count(audio_system) != 0U ||
        sandbox3d_play_session_set_audio_system(session, NULL) != HENKA_SUCCESS ||
        sandbox3d_play_session_start(session) != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_STOPPED ||
        henka_audio_system_get_active_voice_count(audio_system) != 0U ||
        sandbox3d_play_session_set_audio_system(session, audio_system) != HENKA_SUCCESS ||
        henka_physics_world_set_gravity(
            physics_world,
            (henka_vec3){0.0f, FLT_MAX, 0.0f}) != HENKA_SUCCESS ||
        sandbox3d_play_session_start(session) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (tick_count = 0U;
         tick_count < 4096U &&
             sandbox3d_play_session_get_state(session) == SANDBOX3D_PLAY_SESSION_RUNNING;
         ++tick_count)
    {
        tick_result = sandbox3d_play_session_tick(session);
    }
    if (tick_result == HENKA_SUCCESS ||
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_PAUSED_ERROR ||
        sandbox3d_play_session_get_last_error(session) == HENKA_SUCCESS ||
        sandbox3d_play_session_stop(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_STOPPED ||
        sandbox3d_scene_document_bridge_is_play_locked(bridge))
    {
        goto cleanup;
    }
    henka_scene_destroy_entity(scene, entity);
    {
        const henka_result failed_start = sandbox3d_play_session_start(session);
        const sandbox3d_play_session_state failed_state =
            sandbox3d_play_session_get_state(session);
        const henka_result failed_error = sandbox3d_play_session_get_last_error(session);
        const bool failed_locked = sandbox3d_scene_document_bridge_is_play_locked(bridge);
        if (failed_start != HENKA_ERROR_INVALID_ARGUMENT ||
            failed_state != SANDBOX3D_PLAY_SESSION_STOPPED ||
            failed_error != HENKA_ERROR_INVALID_ARGUMENT ||
            failed_locked)
        {
            goto cleanup;
        }
    }
    exit_code = 0;

cleanup:
    sandbox3d_play_session_destroy(session);
    sandbox3d_scene_document_bridge_destroy(bridge);
    henka_physics_world_destroy(physics_world);
    henka_script_state_store_destroy(script_state_store);
    henka_audio_system_destroy(audio_system);
    henka_asset_manager_destroy(asset_manager);
    henka_scene_destroy(scene);
    henka_scene_document_destroy(document);
    remove(audio_path);
    return exit_code;
}
