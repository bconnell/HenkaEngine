#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/audio.h>
#include <henka/assets.h>
#include <henka/memory.h>
#include <henka/physics.h>
#include <henka/scene.h>
#include <henka/scene_document.h>

#include "../examples/sandbox3d/play_session.h"
#include "../engine/src/henka_internal.h"

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
