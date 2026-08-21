#include "play_session.h"

#include <string.h>

#include <henka/memory.h>

#define SANDBOX3D_PLAY_SESSION_MAX_OBJECTS HENKA_SCENE_DOCUMENT_MAX_OBJECTS

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
    sandbox3d_play_session_state state;
    size_t snapshot_count;
    sandbox3d_play_snapshot snapshots[SANDBOX3D_PLAY_SESSION_MAX_OBJECTS];
};

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

henka_result sandbox3d_play_session_create(
    sandbox3d_scene_document_bridge* bridge,
    henka_physics_world* physics_world,
    sandbox3d_play_session** out_session)
{
    sandbox3d_play_session* session;
    if (out_session == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_session = NULL;
    if (bridge == NULL || physics_world == NULL ||
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
    *out_session = session;
    return HENKA_SUCCESS;
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

henka_result sandbox3d_play_session_start(sandbox3d_play_session* session)
{
    size_t index;
    const size_t binding_count =
        sandbox3d_scene_document_bridge_get_binding_count(session == NULL ? NULL : session->bridge);
    if (session == NULL || session->state != SANDBOX3D_PLAY_SESSION_STOPPED ||
        binding_count > SANDBOX3D_PLAY_SESSION_MAX_OBJECTS ||
        sandbox3d_scene_document_bridge_begin_play(session == NULL ? NULL : session->bridge) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    session->snapshot_count = 0U;
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
            (void)sandbox3d_play_session_destroy_bodies(session);
            session->snapshot_count = 0U;
            session->state = SANDBOX3D_PLAY_SESSION_STOPPED;
            (void)sandbox3d_scene_document_bridge_end_play(session->bridge);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        scene = sandbox3d_scene_document_bridge_get_scene(session->bridge);
        if (henka_scene_get_entity_transform(scene, entity, &transform) != HENKA_SUCCESS)
        {
            (void)sandbox3d_play_session_destroy_bodies(session);
            session->snapshot_count = 0U;
            session->state = SANDBOX3D_PLAY_SESSION_STOPPED;
            (void)sandbox3d_scene_document_bridge_end_play(session->bridge);
            return HENKA_ERROR_INVALID_ARGUMENT;
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
                (void)sandbox3d_play_session_destroy_bodies(session);
                session->snapshot_count = 0U;
                session->state = SANDBOX3D_PLAY_SESSION_STOPPED;
                (void)sandbox3d_scene_document_bridge_end_play(session->bridge);
                return body_result;
            }
            session->snapshots[session->snapshot_count].body = body;
        }
        ++session->snapshot_count;
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

henka_result sandbox3d_play_session_step_fixed(sandbox3d_play_session* session)
{
    henka_result result;
    if (session == NULL || session->state != SANDBOX3D_PLAY_SESSION_RUNNING)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_physics_world_step_fixed(session->physics_world);
    if (result != HENKA_SUCCESS)
    {
        session->state = SANDBOX3D_PLAY_SESSION_FAILED;
    }
    return result;
}

henka_result sandbox3d_play_session_stop(sandbox3d_play_session* session)
{
    henka_result result;
    if (session == NULL || session->state == SANDBOX3D_PLAY_SESSION_STOPPED)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_play_session_restore_scene(session);
    if (result != HENKA_SUCCESS)
    {
        session->state = SANDBOX3D_PLAY_SESSION_FAILED;
        return result;
    }
    result = sandbox3d_play_session_destroy_bodies(session);
    if (result != HENKA_SUCCESS)
    {
        session->state = SANDBOX3D_PLAY_SESSION_FAILED;
        return result;
    }
    result = sandbox3d_scene_document_bridge_end_play(session->bridge);
    if (result != HENKA_SUCCESS)
    {
        session->state = SANDBOX3D_PLAY_SESSION_FAILED;
        return result;
    }
    for (size_t index = 0U; index < session->snapshot_count; ++index)
    {
        sandbox3d_play_session_clear_snapshot(&session->snapshots[index]);
    }
    session->snapshot_count = 0U;
    session->state = SANDBOX3D_PLAY_SESSION_STOPPED;
    return HENKA_SUCCESS;
}
