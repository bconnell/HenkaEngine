#include <float.h>
#include <stdio.h>

#include <henka/physics.h>
#include <henka/scene.h>
#include <henka/scene_document.h>

#include "../examples/sandbox3d/play_session.h"

int main(void)
{
    henka_scene_document* document = NULL;
    sandbox3d_scene_document_bridge* bridge = NULL;
    sandbox3d_play_session* session = NULL;
    henka_scene* scene = NULL;
    henka_physics_world* physics_world = NULL;
    henka_scene_document_object object = henka_scene_document_object_default();
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_transform transform;
    henka_transform stepped_transform;
    henka_script_host* script_host;
    henka_script_state_store* script_state_store = NULL;
    henka_script_state_value state_value = {0};
    henka_script_api_value arguments[2];
    henka_script_api_value output;
    henka_script_event script_event;
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

    if (henka_scene_document_create(&document) != HENKA_SUCCESS ||
        henka_scene_create(&scene) != HENKA_SUCCESS ||
        henka_physics_world_create(&physics_world) != HENKA_SUCCESS ||
        henka_script_state_store_create(&script_state_store) != HENKA_SUCCESS)
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
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_STOPPED ||
        sandbox3d_play_session_start(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_RUNNING ||
        (script_host = sandbox3d_play_session_get_script_host(session)) == NULL)
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
    henka_scene_destroy(scene);
    henka_scene_document_destroy(document);
    return exit_code;
}
