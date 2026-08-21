#include "play_session.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/scene_behavior_runtime.h>

#define SANDBOX3D_PLAY_SESSION_MAX_OBJECTS HENKA_SCENE_DOCUMENT_MAX_OBJECTS
#define SANDBOX3D_PLAY_SESSION_MAX_PROJECT_ROOT_BYTES HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES
#define SANDBOX3D_PLAY_SESSION_FIXED_DELTA_SECONDS (1.0f / 60.0f)

typedef struct sandbox3d_play_snapshot
{
    henka_scene_document_id document_id;
    henka_entity entity;
    henka_transform transform;
    bool visible;
    henka_physics_body_id body;
} sandbox3d_play_snapshot;

struct sandbox3d_play_session
{
    sandbox3d_scene_document_bridge* bridge;
    henka_physics_world* physics_world;
    henka_script_host* script_host;
    henka_script_state_store* script_state_store;
    henka_scene_behavior_runtime* behavior_runtime;
    sandbox3d_play_input_query input_query;
    void* input_user_data;
    henka_vec3 observer_position;
    bool observer_position_valid;
    sandbox3d_play_session_state state;
    henka_result last_error;
    uint64_t frame_index;
    char project_root[SANDBOX3D_PLAY_SESSION_MAX_PROJECT_ROOT_BYTES];
    size_t snapshot_count;
    sandbox3d_play_snapshot snapshots[SANDBOX3D_PLAY_SESSION_MAX_OBJECTS];
};

static size_t sandbox3d_play_session_find_snapshot(
    const sandbox3d_play_session* session,
    henka_scene_document_id document_id)
{
    size_t index;
    if (session == NULL || document_id == HENKA_INVALID_SCENE_DOCUMENT_ID)
    {
        return SIZE_MAX;
    }
    for (index = 0U; index < session->snapshot_count; ++index)
    {
        if (session->snapshots[index].document_id == document_id)
        {
            return index;
        }
    }
    return SIZE_MAX;
}

static size_t sandbox3d_play_session_find_snapshot_by_body(
    const sandbox3d_play_session* session,
    henka_physics_body_id body)
{
    size_t index;
    if (session == NULL || body == HENKA_INVALID_PHYSICS_BODY_ID)
    {
        return SIZE_MAX;
    }
    for (index = 0U; index < session->snapshot_count; ++index)
    {
        if (session->snapshots[index].body == body)
        {
            return index;
        }
    }
    return SIZE_MAX;
}

static henka_script_lifecycle_event sandbox3d_play_session_event_to_lifecycle(
    henka_physics_event_type event_type)
{
    switch (event_type)
    {
        case HENKA_PHYSICS_EVENT_COLLISION_ENTER:
            return HENKA_SCRIPT_LIFECYCLE_COLLISION_ENTER;
        case HENKA_PHYSICS_EVENT_COLLISION_STAY:
            return HENKA_SCRIPT_LIFECYCLE_COLLISION_STAY;
        case HENKA_PHYSICS_EVENT_COLLISION_EXIT:
            return HENKA_SCRIPT_LIFECYCLE_COLLISION_EXIT;
        case HENKA_PHYSICS_EVENT_TRIGGER_ENTER:
            return HENKA_SCRIPT_LIFECYCLE_TRIGGER_ENTER;
        case HENKA_PHYSICS_EVENT_TRIGGER_STAY:
            return HENKA_SCRIPT_LIFECYCLE_TRIGGER_STAY;
        case HENKA_PHYSICS_EVENT_TRIGGER_EXIT:
            return HENKA_SCRIPT_LIFECYCLE_TRIGGER_EXIT;
        default:
            return HENKA_SCRIPT_LIFECYCLE_EVENT;
    }
}

static henka_result sandbox3d_play_session_dispatch_physics_events(
    sandbox3d_play_session* session)
{
    const henka_physics_event* events;
    size_t event_count = 0U;
    size_t event_index;
    if (session == NULL || session->behavior_runtime == NULL ||
        session->physics_world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    events = henka_physics_world_get_events(session->physics_world, &event_count);
    if (events == NULL && event_count != 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (event_index = 0U; event_index < event_count; ++event_index)
    {
        const henka_physics_event* event = &events[event_index];
        const henka_script_lifecycle_event lifecycle =
            sandbox3d_play_session_event_to_lifecycle(event->type);
        const size_t first_index = sandbox3d_play_session_find_snapshot_by_body(
            session, event->contact.body_a);
        const size_t second_index = sandbox3d_play_session_find_snapshot_by_body(
            session, event->contact.body_b);
        henka_script_behavior_batch_report report;
        henka_result result;
        if (lifecycle == HENKA_SCRIPT_LIFECYCLE_EVENT ||
            first_index == SIZE_MAX || second_index == SIZE_MAX)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        result = henka_scene_behavior_runtime_dispatch_signal_for_entity(
            session->behavior_runtime,
            session->snapshots[first_index].document_id,
            lifecycle,
            SANDBOX3D_PLAY_SESSION_FIXED_DELTA_SECONDS,
            session->frame_index,
            0U,
            session->snapshots[second_index].document_id,
            session->snapshots[second_index].document_id,
            (uint32_t)event->type,
            &report);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (second_index != first_index)
        {
            result = henka_scene_behavior_runtime_dispatch_signal_for_entity(
                session->behavior_runtime,
                session->snapshots[second_index].document_id,
                lifecycle,
                SANDBOX3D_PLAY_SESSION_FIXED_DELTA_SECONDS,
                session->frame_index,
                0U,
                session->snapshots[first_index].document_id,
                session->snapshots[first_index].document_id,
                (uint32_t)event->type,
                &report);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
        }
    }
    return HENKA_SUCCESS;
}

static void sandbox3d_play_session_set_result_value(
    henka_script_api_value* out_value,
    henka_result result)
{
    if (out_value != NULL)
    {
        out_value->type = HENKA_SCRIPT_API_VALUE_RESULT;
        out_value->as.result = result;
    }
}

static henka_result sandbox3d_play_session_script_dispatch(
    void* user_data,
    uint32_t api_id,
    const henka_script_api_value* arguments,
    size_t argument_count,
    henka_script_api_value* out_value)
{
    sandbox3d_play_session* session =
        (sandbox3d_play_session*)user_data;
    size_t snapshot_index;
    henka_entity entity;
    henka_transform transform;
    henka_result result;
    if (session == NULL || arguments == NULL || out_value == NULL ||
        session->bridge == NULL || session->physics_world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (argument_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    switch (api_id)
    {
        case HENKA_SCRIPT_API_ENTITY_IS_VALID:
            if (argument_count != 1U ||
                arguments[0].type != HENKA_SCRIPT_API_VALUE_ENTITY)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            snapshot_index = sandbox3d_play_session_find_snapshot(
                session, arguments[0].as.entity);
            out_value->type = HENKA_SCRIPT_API_VALUE_BOOL;
            out_value->as.boolean = snapshot_index != SIZE_MAX &&
                henka_scene_is_entity_valid(
                    sandbox3d_scene_document_bridge_get_scene(session->bridge),
                    session->snapshots[snapshot_index].entity);
            return HENKA_SUCCESS;

        case HENKA_SCRIPT_API_TRANSFORM_GET_POSITION:
            if (argument_count != 1U ||
                arguments[0].type != HENKA_SCRIPT_API_VALUE_ENTITY)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            snapshot_index = sandbox3d_play_session_find_snapshot(
                session, arguments[0].as.entity);
            if (snapshot_index == SIZE_MAX ||
                henka_scene_get_entity_transform(
                    sandbox3d_scene_document_bridge_get_scene(session->bridge),
                    session->snapshots[snapshot_index].entity,
                    &transform) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            out_value->type = HENKA_SCRIPT_API_VALUE_VEC3;
            out_value->as.vec3 = transform.position;
            return HENKA_SUCCESS;

        case HENKA_SCRIPT_API_TRANSFORM_SET_POSITION:
            if (argument_count != 2U ||
                arguments[0].type != HENKA_SCRIPT_API_VALUE_ENTITY ||
                arguments[1].type != HENKA_SCRIPT_API_VALUE_VEC3 ||
                !isfinite(arguments[1].as.vec3.x) ||
                !isfinite(arguments[1].as.vec3.y) ||
                !isfinite(arguments[1].as.vec3.z))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            snapshot_index = sandbox3d_play_session_find_snapshot(
                session, arguments[0].as.entity);
            if (snapshot_index == SIZE_MAX ||
                henka_scene_get_entity_transform(
                    sandbox3d_scene_document_bridge_get_scene(session->bridge),
                    session->snapshots[snapshot_index].entity,
                    &transform) != HENKA_SUCCESS)
            {
                sandbox3d_play_session_set_result_value(
                    out_value, HENKA_ERROR_INVALID_ARGUMENT);
                return HENKA_SUCCESS;
            }
            entity = session->snapshots[snapshot_index].entity;
            {
                const henka_transform previous_transform = transform;
                transform.position = arguments[1].as.vec3;
                result = henka_scene_set_entity_transform(
                    sandbox3d_scene_document_bridge_get_scene(session->bridge),
                    entity,
                    transform);
                if (result == HENKA_SUCCESS &&
                    session->snapshots[snapshot_index].body != HENKA_INVALID_PHYSICS_BODY_ID)
                {
                    result = henka_physics_body_set_transform(
                        session->physics_world,
                        session->snapshots[snapshot_index].body,
                        transform,
                        false);
                    if (result != HENKA_SUCCESS)
                    {
                        (void)henka_scene_set_entity_transform(
                            sandbox3d_scene_document_bridge_get_scene(session->bridge),
                            entity,
                            previous_transform);
                    }
                }
            }
            sandbox3d_play_session_set_result_value(out_value, result);
            return HENKA_SUCCESS;

        case HENKA_SCRIPT_API_PHYSICS_APPLY_IMPULSE:
            if (argument_count != 2U ||
                arguments[0].type != HENKA_SCRIPT_API_VALUE_ENTITY ||
                arguments[1].type != HENKA_SCRIPT_API_VALUE_VEC3 ||
                !isfinite(arguments[1].as.vec3.x) ||
                !isfinite(arguments[1].as.vec3.y) ||
                !isfinite(arguments[1].as.vec3.z))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            snapshot_index = sandbox3d_play_session_find_snapshot(
                session, arguments[0].as.entity);
            result = snapshot_index == SIZE_MAX ||
                session->snapshots[snapshot_index].body == HENKA_INVALID_PHYSICS_BODY_ID
                ? HENKA_ERROR_INVALID_ARGUMENT
                : henka_physics_body_apply_impulse(
                    session->physics_world,
                    session->snapshots[snapshot_index].body,
                    arguments[1].as.vec3);
            sandbox3d_play_session_set_result_value(out_value, result);
            return HENKA_SUCCESS;

        case HENKA_SCRIPT_API_EVENTS_EMIT:
            if (argument_count != 2U ||
                arguments[0].type != HENKA_SCRIPT_API_VALUE_EVENT_ID ||
                arguments[1].type != HENKA_SCRIPT_API_VALUE_ENTITY ||
                arguments[0].as.event_id == 0U ||
                arguments[1].as.entity == HENKA_INVALID_ENTITY)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            result = henka_script_host_emit_event(
                session->script_host,
                arguments[0].as.event_id,
                arguments[1].as.entity,
                session->frame_index);
            sandbox3d_play_session_set_result_value(out_value, result);
            return result;

        case HENKA_SCRIPT_API_INPUT_IS_ACTION_DOWN:
            if (argument_count != 1U ||
                arguments[0].type != HENKA_SCRIPT_API_VALUE_ACTION_ID)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            out_value->type = HENKA_SCRIPT_API_VALUE_BOOL;
            out_value->as.boolean = session->input_query != NULL &&
                arguments[0].as.action_id != 0U &&
                session->input_query(
                    session->input_user_data,
                    arguments[0].as.action_id);
            return HENKA_SUCCESS;

        case HENKA_SCRIPT_API_INTERACTION_TRY:
            if (argument_count != 1U ||
                arguments[0].type != HENKA_SCRIPT_API_VALUE_ENTITY)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            snapshot_index = sandbox3d_play_session_find_snapshot(
                session, arguments[0].as.entity);
            if (snapshot_index == SIZE_MAX || !session->observer_position_valid)
            {
                sandbox3d_play_session_set_result_value(
                    out_value,
                    (henka_result)HENKA_INTERACTION_RESULT_UNAVAILABLE);
                return HENKA_SUCCESS;
            }
            out_value->type = HENKA_SCRIPT_API_VALUE_RESULT;
            out_value->as.result = (henka_result)henka_scene_can_interact(
                sandbox3d_scene_document_bridge_get_scene(session->bridge),
                session->snapshots[snapshot_index].entity,
                session->observer_position);
            return HENKA_SUCCESS;

        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }
}

static henka_result sandbox3d_play_session_bind_script_host(
    sandbox3d_play_session* session)
{
    const henka_script_api_function* functions = NULL;
    size_t function_count = 0U;
    size_t index;
    henka_result result;
    if (session == NULL ||
        henka_script_api_schema_get(&functions, &function_count) != HENKA_SUCCESS ||
        functions == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_host_create(&session->script_host);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    for (index = 0U; index < function_count; ++index)
    {
        result = henka_script_host_bind_api(
            session->script_host,
            functions[index].id,
            &(size_t){0U});
        if (result != HENKA_SUCCESS)
        {
            henka_script_host_destroy(session->script_host);
            session->script_host = NULL;
            return result;
        }
    }
    result = henka_script_host_set_state_store(
        session->script_host,
        session->script_state_store);
    if (result != HENKA_SUCCESS)
    {
        henka_script_host_destroy(session->script_host);
        session->script_host = NULL;
        return result;
    }
    return henka_script_host_set_dispatcher(
        session->script_host,
        sandbox3d_play_session_script_dispatch,
        session);
}

static void sandbox3d_play_session_clear_snapshot(
    sandbox3d_play_snapshot* snapshot)
{
    if (snapshot != NULL)
    {
        *snapshot = (sandbox3d_play_snapshot){
            HENKA_INVALID_SCENE_DOCUMENT_ID,
            HENKA_INVALID_ENTITY,
            henka_transform_identity(),
            false,
            HENKA_INVALID_PHYSICS_BODY_ID};
    }
}

static henka_result sandbox3d_play_session_destroy_bodies(
    sandbox3d_play_session* session)
{
    size_t index;
    henka_result result = HENKA_SUCCESS;
    for (index = 0U; index < session->snapshot_count; ++index)
    {
        if (session->snapshots[index].body != HENKA_INVALID_PHYSICS_BODY_ID)
        {
            const henka_result destroy_result = henka_physics_body_destroy(
                session->physics_world,
                session->snapshots[index].body);
            if (destroy_result != HENKA_SUCCESS && result == HENKA_SUCCESS)
            {
                result = destroy_result;
            }
            if (destroy_result == HENKA_SUCCESS)
            {
                session->snapshots[index].body = HENKA_INVALID_PHYSICS_BODY_ID;
            }
        }
    }
    return result;
}

static void sandbox3d_play_session_abort_start(
    sandbox3d_play_session* session)
{
    if (session == NULL)
    {
        return;
    }
    (void)sandbox3d_play_session_destroy_bodies(session);
    if (session->behavior_runtime != NULL)
    {
        (void)henka_scene_behavior_runtime_dispatch(
            session->behavior_runtime,
            HENKA_SCRIPT_LIFECYCLE_DESTROY,
            0.0f,
            session->frame_index,
            &(henka_script_behavior_batch_report){0});
        (void)henka_scene_behavior_runtime_dispatch(
            session->behavior_runtime,
            HENKA_SCRIPT_LIFECYCLE_STOP,
            0.0f,
            session->frame_index,
            &(henka_script_behavior_batch_report){0});
        henka_scene_behavior_runtime_destroy(session->behavior_runtime);
        session->behavior_runtime = NULL;
    }
    henka_script_host_destroy(session->script_host);
    session->script_host = NULL;
    session->snapshot_count = 0U;
    session->state = SANDBOX3D_PLAY_SESSION_STOPPED;
    (void)sandbox3d_scene_document_bridge_end_play(session->bridge);
}

static henka_result sandbox3d_play_session_abort_start_with_error(
    sandbox3d_play_session* session,
    henka_result error)
{
    sandbox3d_play_session_abort_start(session);
    if (session != NULL)
    {
        session->last_error = error;
    }
    return error;
}

static henka_result sandbox3d_play_session_restore_scene(
    sandbox3d_play_session* session)
{
    size_t index;
    for (index = 0U; index < session->snapshot_count; ++index)
    {
        const sandbox3d_play_snapshot* snapshot = &session->snapshots[index];
        if (!henka_scene_is_entity_valid(
                sandbox3d_scene_document_bridge_get_scene(session->bridge),
                snapshot->entity))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < session->snapshot_count; ++index)
    {
        const sandbox3d_play_snapshot* snapshot = &session->snapshots[index];
        henka_scene* scene = sandbox3d_scene_document_bridge_get_scene(session->bridge);
        if (henka_scene_set_entity_transform(scene, snapshot->entity, snapshot->transform) != HENKA_SUCCESS ||
            henka_scene_set_entity_visible(scene, snapshot->entity, snapshot->visible) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_play_session_create_with_project_root(
    sandbox3d_scene_document_bridge* bridge,
    henka_physics_world* physics_world,
    const char* project_root,
    sandbox3d_play_session** out_session)
{
    sandbox3d_play_session* session;
    int written;
    if (out_session == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_session = NULL;
    if (bridge == NULL || physics_world == NULL || project_root == NULL ||
        project_root[0] == '\0' ||
        sandbox3d_scene_document_bridge_validate(bridge) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session = (sandbox3d_play_session*)henka_calloc(1U, sizeof(*session));
    if (session == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    session->bridge = bridge;
    session->physics_world = physics_world;
    session->state = SANDBOX3D_PLAY_SESSION_STOPPED;
    session->last_error = HENKA_SUCCESS;
    written = snprintf(
        session->project_root,
        sizeof(session->project_root),
        "%s",
        project_root);
    if (written < 0 || (size_t)written >= sizeof(session->project_root))
    {
        henka_free(session);
        return HENKA_ERROR_LIMIT;
    }
    *out_session = session;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_play_session_create(
    sandbox3d_scene_document_bridge* bridge,
    henka_physics_world* physics_world,
    sandbox3d_play_session** out_session)
{
    return sandbox3d_play_session_create_with_project_root(
        bridge, physics_world, ".", out_session);
}

void sandbox3d_play_session_destroy(sandbox3d_play_session* session)
{
    if (session == NULL)
    {
        return;
    }
    if (session->state != SANDBOX3D_PLAY_SESSION_STOPPED)
    {
        (void)sandbox3d_play_session_stop(session);
    }
    henka_free(session);
}

sandbox3d_play_session_state sandbox3d_play_session_get_state(
    const sandbox3d_play_session* session)
{
    return session == NULL
        ? SANDBOX3D_PLAY_SESSION_FAILED
        : session->state;
}

henka_result sandbox3d_play_session_get_last_error(
    const sandbox3d_play_session* session)
{
    return session == NULL ? HENKA_ERROR_INVALID_ARGUMENT : session->last_error;
}

henka_script_host* sandbox3d_play_session_get_script_host(
    const sandbox3d_play_session* session)
{
    return session == NULL ? NULL : session->script_host;
}

henka_result sandbox3d_play_session_set_script_state_store(
    sandbox3d_play_session* session,
    henka_script_state_store* store)
{
    if (session == NULL || session->state != SANDBOX3D_PLAY_SESSION_STOPPED)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->script_state_store = store;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_play_session_set_input_context(
    sandbox3d_play_session* session,
    sandbox3d_play_input_query input_query,
    void* input_user_data,
    henka_vec3 observer_position)
{
    if (session == NULL ||
        !isfinite(observer_position.x) ||
        !isfinite(observer_position.y) ||
        !isfinite(observer_position.z))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->input_query = input_query;
    session->input_user_data = input_user_data;
    session->observer_position = observer_position;
    session->observer_position_valid = true;
    return HENKA_SUCCESS;
}

henka_script_state_store* sandbox3d_play_session_get_script_state_store(
    const sandbox3d_play_session* session)
{
    return session == NULL ? NULL : session->script_state_store;
}

henka_result sandbox3d_play_session_start(sandbox3d_play_session* session)
{
    size_t index;
    henka_result result;
    size_t binding_count;
    if (session == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (session->state != SANDBOX3D_PLAY_SESSION_STOPPED)
    {
        session->last_error = HENKA_ERROR_INVALID_ARGUMENT;
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    binding_count = sandbox3d_scene_document_bridge_get_binding_count(session->bridge);
    if (binding_count > SANDBOX3D_PLAY_SESSION_MAX_OBJECTS ||
        sandbox3d_scene_document_bridge_begin_play(session->bridge) != HENKA_SUCCESS)
    {
        session->last_error = HENKA_ERROR_INVALID_ARGUMENT;
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->snapshot_count = 0U;
    session->last_error = HENKA_SUCCESS;
    session->frame_index = 0U;
    for (index = 0U; index < binding_count; ++index)
    {
        henka_scene_document_id document_id;
        henka_entity entity;
        henka_scene_document_object object;
        henka_transform transform;
        henka_physics_body_desc body_desc;
        henka_physics_body_id body = HENKA_INVALID_PHYSICS_BODY_ID;
        henka_scene* scene;
        if (sandbox3d_scene_document_bridge_get_binding_at(
                session->bridge, index, &document_id, &entity) != HENKA_SUCCESS ||
            sandbox3d_scene_document_bridge_get_scene(session->bridge) == NULL ||
            sandbox3d_scene_document_bridge_get_object(
                session->bridge, document_id, &object) != HENKA_SUCCESS)
        {
            return sandbox3d_play_session_abort_start_with_error(
                session, HENKA_ERROR_INVALID_ARGUMENT);
        }
        scene = sandbox3d_scene_document_bridge_get_scene(session->bridge);
        if (henka_scene_get_entity_transform(scene, entity, &transform) != HENKA_SUCCESS)
        {
            return sandbox3d_play_session_abort_start_with_error(
                session, HENKA_ERROR_INVALID_ARGUMENT);
        }
        session->snapshots[session->snapshot_count] = (sandbox3d_play_snapshot){
            document_id,
            entity,
            transform,
            henka_scene_is_entity_visible(scene, entity),
            HENKA_INVALID_PHYSICS_BODY_ID};
        if (object.physics.enabled)
        {
            const henka_result descriptor_result =
                sandbox3d_scene_document_bridge_make_physics_body_desc(
                    session->bridge, document_id, &body_desc);
            const henka_result body_result = descriptor_result == HENKA_SUCCESS
                ? henka_physics_body_create(
                    session->physics_world, &body_desc, &body)
                : descriptor_result;
            if (body_result != HENKA_SUCCESS)
            {
                return sandbox3d_play_session_abort_start_with_error(
                    session, body_result);
            }
            session->snapshots[session->snapshot_count].body = body;
        }
        ++session->snapshot_count;
    }
    {
        henka_script_behavior_batch_report report;
        result = sandbox3d_play_session_bind_script_host(session);
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_behavior_runtime_create_with_host(
            sandbox3d_scene_document_bridge_get_document(session->bridge),
            session->project_root,
            0U,
            session->script_host,
            &session->behavior_runtime);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_behavior_runtime_dispatch(
                session->behavior_runtime,
                HENKA_SCRIPT_LIFECYCLE_CREATE,
                0.0f,
                session->frame_index,
                &report);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_behavior_runtime_dispatch(
                session->behavior_runtime,
                HENKA_SCRIPT_LIFECYCLE_START,
                0.0f,
                session->frame_index,
                &report);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_behavior_runtime_dispatch_events(
                session->behavior_runtime,
                &report);
        }
        if (result != HENKA_SUCCESS)
        {
            return sandbox3d_play_session_abort_start_with_error(session, result);
        }
    }
    session->state = SANDBOX3D_PLAY_SESSION_RUNNING;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_play_session_pause(sandbox3d_play_session* session)
{
    if (session == NULL || session->state != SANDBOX3D_PLAY_SESSION_RUNNING)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->state = SANDBOX3D_PLAY_SESSION_PAUSED;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_play_session_resume(sandbox3d_play_session* session)
{
    if (session == NULL || session->state != SANDBOX3D_PLAY_SESSION_PAUSED)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->state = SANDBOX3D_PLAY_SESSION_RUNNING;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_play_session_run_fixed_tick(
    sandbox3d_play_session* session)
{
    henka_script_behavior_batch_report report;
    henka_result result = henka_scene_behavior_runtime_dispatch(
        session->behavior_runtime,
        HENKA_SCRIPT_LIFECYCLE_UPDATE,
        SANDBOX3D_PLAY_SESSION_FIXED_DELTA_SECONDS,
        session->frame_index,
        &report);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_behavior_runtime_dispatch_events(
            session->behavior_runtime,
            &report);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_behavior_runtime_dispatch(
            session->behavior_runtime,
            HENKA_SCRIPT_LIFECYCLE_FIXED_UPDATE,
            SANDBOX3D_PLAY_SESSION_FIXED_DELTA_SECONDS,
            session->frame_index,
            &report);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_behavior_runtime_dispatch_events(
            session->behavior_runtime,
            &report);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_physics_world_step_fixed(session->physics_world);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_play_session_dispatch_physics_events(session);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_behavior_runtime_dispatch_events(
            session->behavior_runtime,
            &report);
    }
    if (result != HENKA_SUCCESS)
    {
        session->state = SANDBOX3D_PLAY_SESSION_PAUSED_ERROR;
        session->last_error = result;
    }
    else
    {
        ++session->frame_index;
    }
    return result;
}

henka_result sandbox3d_play_session_tick(sandbox3d_play_session* session)
{
    if (session == NULL || session->state != SANDBOX3D_PLAY_SESSION_RUNNING)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_play_session_run_fixed_tick(session);
}

henka_result sandbox3d_play_session_step_fixed(sandbox3d_play_session* session)
{
    if (session == NULL || session->state != SANDBOX3D_PLAY_SESSION_PAUSED)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_play_session_run_fixed_tick(session);
}

henka_result sandbox3d_play_session_stop(sandbox3d_play_session* session)
{
    henka_result destroy_script_result;
    henka_result script_result;
    henka_result restore_result;
    henka_result body_result;
    henka_result end_result;
    henka_result result;
    if (session == NULL || session->state == SANDBOX3D_PLAY_SESSION_STOPPED)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* Cleanup is deliberately attempted in full even if authored-state
     * restoration fails. Play scenes may be discarded by the coordinator, so
     * physics bodies and the bridge lock must not retain references into a
     * scene that is about to be released. */
    destroy_script_result = henka_scene_behavior_runtime_dispatch(
        session->behavior_runtime,
        HENKA_SCRIPT_LIFECYCLE_DESTROY,
        0.0f,
        session->frame_index,
        &(henka_script_behavior_batch_report){0});
    script_result = henka_scene_behavior_runtime_dispatch(
        session->behavior_runtime,
        HENKA_SCRIPT_LIFECYCLE_STOP,
        0.0f,
        session->frame_index,
        &(henka_script_behavior_batch_report){0});
    henka_scene_behavior_runtime_destroy(session->behavior_runtime);
    session->behavior_runtime = NULL;
    henka_script_host_destroy(session->script_host);
    session->script_host = NULL;
    restore_result = sandbox3d_play_session_restore_scene(session);
    body_result = sandbox3d_play_session_destroy_bodies(session);
    end_result = sandbox3d_scene_document_bridge_end_play(session->bridge);
    result = destroy_script_result != HENKA_SUCCESS ? destroy_script_result
        : script_result != HENKA_SUCCESS ? script_result
        : restore_result != HENKA_SUCCESS ? restore_result
        : (body_result != HENKA_SUCCESS ? body_result : end_result);
    for (size_t index = 0U; index < session->snapshot_count; ++index)
    {
        sandbox3d_play_session_clear_snapshot(&session->snapshots[index]);
    }
    session->snapshot_count = 0U;
    if (result != HENKA_SUCCESS)
    {
        session->last_error = result;
        session->state = SANDBOX3D_PLAY_SESSION_FAILED;
        return result;
    }
    session->last_error = HENKA_SUCCESS;
    session->state = SANDBOX3D_PLAY_SESSION_STOPPED;
    return HENKA_SUCCESS;
}
