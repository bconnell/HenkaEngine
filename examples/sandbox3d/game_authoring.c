#include "game_authoring.h"

#include <stdio.h>
#include <string.h>

#include <henka/memory.h>

#define SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS HENKA_SCENE_DOCUMENT_MAX_OBJECTS
#define SANDBOX3D_GAME_AUTHORING_MAX_RELATIVE_PATH_BYTES HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES

typedef struct sandbox3d_game_authoring_binding
{
    henka_scene_document_id document_id;
    henka_entity entity;
} sandbox3d_game_authoring_binding;

struct sandbox3d_game_authoring
{
    henka_scene* scene;
    henka_scene_document* document;
    sandbox3d_scene_document_bridge* bridge;
    henka_physics_world* play_world;
    henka_scene* play_scene;
    sandbox3d_scene_document_bridge* play_bridge;
    sandbox3d_play_session* play_session;
    char relative_path[SANDBOX3D_GAME_AUTHORING_MAX_RELATIVE_PATH_BYTES];
    sandbox3d_game_authoring_binding bindings[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    size_t binding_count;
};

static size_t sandbox3d_game_authoring_find_binding(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity)
{
    size_t index;
    if (authoring == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return SIZE_MAX;
    }
    for (index = 0U; index < authoring->binding_count; ++index)
    {
        if (authoring->bindings[index].entity == entity)
        {
            return index;
        }
    }
    return SIZE_MAX;
}

static henka_result sandbox3d_game_authoring_find_document_id_by_name(
    const henka_scene_document* document,
    const char* name,
    henka_scene_document_id* out_id)
{
    size_t index;
    size_t count;
    if (document == NULL || name == NULL || out_id == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    count = henka_scene_document_get_object_count(document);
    for (index = 0U; index < count; ++index)
    {
        henka_scene_document_object object;
        if (henka_scene_document_get_object_at(document, index, &object) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (strcmp(object.name, name) == 0)
        {
            *out_id = object.id;
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

static henka_result sandbox3d_game_authoring_build_object(
    const henka_scene* scene,
    henka_entity entity,
    henka_scene_document_object* out_object)
{
    henka_scene_object_info info;
    henka_interaction_desc interaction;
    int written;
    if (scene == NULL || out_object == NULL ||
        !henka_scene_is_entity_valid(scene, entity) ||
        henka_scene_get_entity_info(scene, entity, &info) != HENKA_SUCCESS ||
        henka_scene_get_entity_interaction(scene, entity, &interaction) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_object = henka_scene_document_object_default();
    written = snprintf(
        out_object->name,
        sizeof(out_object->name),
        "%s",
        info.name == NULL ? "Object" : info.name);
    if (written < 0 || (size_t)written >= sizeof(out_object->name))
    {
        return HENKA_ERROR_LIMIT;
    }
    out_object->visible = info.visible;
    out_object->transform = info.transform;
    out_object->interaction.enabled = interaction.enabled;
    out_object->interaction.max_distance = interaction.max_distance;
    written = snprintf(
        out_object->interaction.prompt,
        sizeof(out_object->interaction.prompt),
        "%s",
        interaction.prompt == NULL ? "" : interaction.prompt);
    if (written < 0 || (size_t)written >= sizeof(out_object->interaction.prompt))
    {
        return HENKA_ERROR_LIMIT;
    }
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_game_authoring_copy_document(
    const henka_scene_document* source,
    henka_scene_document** out_copy)
{
    henka_scene_document* copy = NULL;
    size_t index;
    size_t count;
    henka_result result;

    if (source == NULL || out_copy == NULL ||
        henka_scene_document_validate(source) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_copy = NULL;
    result = henka_scene_document_create(&copy);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    count = henka_scene_document_get_object_count(source);
    for (index = 0U; index < count; ++index)
    {
        henka_scene_document_object object;
        henka_scene_document_id ignored_id;
        result = henka_scene_document_get_object_at(source, index, &object);
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_document_add_object(copy, &object, &ignored_id);
        }
        if (result != HENKA_SUCCESS)
        {
            henka_scene_document_destroy(copy);
            return result;
        }
    }
    *out_copy = copy;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_game_authoring_restore_document(
    henka_scene_document* destination,
    const henka_scene_document* source)
{
    size_t index;
    size_t count;
    henka_result result;

    if (destination == NULL || source == NULL ||
        henka_scene_document_validate(source) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_scene_document_clear(destination);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    count = henka_scene_document_get_object_count(source);
    for (index = 0U; index < count; ++index)
    {
        henka_scene_document_object object;
        henka_scene_document_id ignored_id;
        result = henka_scene_document_get_object_at(source, index, &object);
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_document_add_object(destination, &object, &ignored_id);
        }
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_create(
    henka_scene* scene,
    const char* relative_path,
    sandbox3d_game_authoring** out_authoring)
{
    sandbox3d_game_authoring* authoring;
    int written;
    henka_result result;
    if (out_authoring == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_authoring = NULL;
    if (scene == NULL || relative_path == NULL || relative_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    authoring = (sandbox3d_game_authoring*)henka_calloc(1U, sizeof(*authoring));
    if (authoring == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    written = snprintf(authoring->relative_path, sizeof(authoring->relative_path), "%s", relative_path);
    if (written < 0 || (size_t)written >= sizeof(authoring->relative_path))
    {
        henka_free(authoring);
        return HENKA_ERROR_LIMIT;
    }
    authoring->scene = scene;
    result = henka_scene_document_create(&authoring->document);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_create(
            authoring->document,
            scene,
            &authoring->bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_physics_world_create(&authoring->play_world);
    }
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_play_session_destroy(authoring->play_session);
        henka_physics_world_destroy(authoring->play_world);
        sandbox3d_scene_document_bridge_destroy(authoring->bridge);
        henka_scene_document_destroy(authoring->document);
        henka_free(authoring);
        return result;
    }
    *out_authoring = authoring;
    return HENKA_SUCCESS;
}

void sandbox3d_game_authoring_destroy(
    sandbox3d_game_authoring* authoring)
{
    if (authoring == NULL)
    {
        return;
    }
    if (authoring->play_session != NULL)
    {
        (void)sandbox3d_play_session_stop(authoring->play_session);
    }
    sandbox3d_play_session_destroy(authoring->play_session);
    sandbox3d_scene_document_bridge_destroy(authoring->play_bridge);
    henka_scene_destroy(authoring->play_scene);
    henka_physics_world_destroy(authoring->play_world);
    sandbox3d_scene_document_bridge_destroy(authoring->bridge);
    henka_scene_document_destroy(authoring->document);
    henka_free(authoring);
}

henka_result sandbox3d_game_authoring_register_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_id* out_document_id)
{
    henka_scene_document_object object;
    henka_scene_document_id document_id;
    henka_result result;
    if (out_document_id != NULL)
    {
        *out_document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    }
    if (authoring == NULL || out_document_id == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        authoring->binding_count >= SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS ||
        sandbox3d_game_authoring_find_binding(authoring, entity) != SIZE_MAX ||
        sandbox3d_game_authoring_build_object(authoring->scene, entity, &object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_scene_document_add_object(authoring->document, &object, &document_id);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_scene_document_bridge_bind(authoring->bridge, document_id, entity);
    if (result != HENKA_SUCCESS)
    {
        (void)henka_scene_document_remove_object(authoring->document, document_id);
        return result;
    }
    authoring->bindings[authoring->binding_count++] =
        (sandbox3d_game_authoring_binding){document_id, entity};
    *out_document_id = document_id;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_unregister_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity)
{
    size_t index;
    if (authoring == NULL || sandbox3d_game_authoring_is_play_locked(authoring) ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (sandbox3d_scene_document_bridge_unbind(
            authoring->bridge,
            authoring->bindings[index].document_id) != HENKA_SUCCESS ||
        henka_scene_document_remove_object(
            authoring->document,
            authoring->bindings[index].document_id) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (index + 1U < authoring->binding_count)
    {
        memmove(
            &authoring->bindings[index],
            &authoring->bindings[index + 1U],
            (authoring->binding_count - index - 1U) * sizeof(authoring->bindings[0]));
    }
    --authoring->binding_count;
    authoring->bindings[authoring->binding_count] =
        (sandbox3d_game_authoring_binding){
            HENKA_INVALID_SCENE_DOCUMENT_ID,
            HENKA_INVALID_ENTITY};
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_get_object_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_id* out_document_id,
    henka_scene_document_object* out_object)
{
    size_t index;
    if (out_document_id != NULL)
    {
        *out_document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    }
    if (authoring == NULL || out_document_id == NULL || out_object == NULL ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_scene_document_get_object(
            authoring->document,
            authoring->bindings[index].document_id,
            out_object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_document_id = authoring->bindings[index].document_id;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_update_object_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    const henka_scene_document_object* object)
{
    henka_scene_document_object previous;
    henka_scene_document_id document_id;
    henka_result result;
    if (authoring == NULL || object == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &document_id, &previous) != HENKA_SUCCESS ||
        object->id != document_id)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_scene_document_set_object(authoring->document, object);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_scene_document_bridge_apply_object(authoring->bridge, document_id);
    if (result != HENKA_SUCCESS)
    {
        (void)henka_scene_document_set_object(authoring->document, &previous);
        (void)sandbox3d_scene_document_bridge_apply_object(authoring->bridge, document_id);
    }
    return result;
}

henka_result sandbox3d_game_authoring_save(
    const sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    if (authoring == NULL || project_root == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_save_file(
        authoring->document,
        project_root,
        authoring->relative_path);
}

henka_result sandbox3d_game_authoring_load(
    sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    henka_scene_document* candidate = NULL;
    henka_scene_document* previous = NULL;
    henka_scene_document_id ids[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    henka_scene_document_id previous_ids[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    size_t index;
    size_t rebound_count = 0U;
    henka_result result;
    if (authoring == NULL || project_root == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_scene_document_create(&candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_scene_document_load_file(candidate, project_root, authoring->relative_path);
    if (result != HENKA_SUCCESS)
    {
        henka_scene_document_destroy(candidate);
        return result;
    }
    for (index = 0U; index < authoring->binding_count; ++index)
    {
        previous_ids[index] = authoring->bindings[index].document_id;
        const char* name = henka_scene_get_entity_name(
            authoring->scene,
            authoring->bindings[index].entity);
        if (name == NULL || sandbox3d_game_authoring_find_document_id_by_name(
                candidate,
                name,
                &ids[index]) != HENKA_SUCCESS)
        {
            henka_scene_document_destroy(candidate);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    result = sandbox3d_game_authoring_copy_document(authoring->document, &previous);
    if (result != HENKA_SUCCESS)
    {
        henka_scene_document_destroy(candidate);
        return result;
    }
    result = henka_scene_document_load_file(authoring->document, project_root, authoring->relative_path);
    if (result != HENKA_SUCCESS)
    {
        goto load_cleanup;
    }
    for (index = 0U; index < authoring->binding_count; ++index)
    {
        result = sandbox3d_scene_document_bridge_unbind(
            authoring->bridge,
            previous_ids[index]);
        if (result != HENKA_SUCCESS)
        {
            goto load_rollback;
        }
    }
    for (index = 0U; index < authoring->binding_count; ++index)
    {
        result = sandbox3d_scene_document_bridge_bind(
            authoring->bridge,
            ids[index],
            authoring->bindings[index].entity);
        if (result != HENKA_SUCCESS)
        {
            goto load_rollback;
        }
        authoring->bindings[index].document_id = ids[index];
        ++rebound_count;
    }
    for (index = 0U; index < authoring->binding_count; ++index)
    {
        result = sandbox3d_scene_document_bridge_apply_object(
            authoring->bridge,
            authoring->bindings[index].document_id);
        if (result != HENKA_SUCCESS)
        {
            goto load_rollback;
        }
    }
    henka_scene_document_destroy(candidate);
    henka_scene_document_destroy(previous);
    return result;

load_rollback:
    for (index = 0U; index < rebound_count; ++index)
    {
        (void)sandbox3d_scene_document_bridge_unbind(authoring->bridge, ids[index]);
    }
    (void)sandbox3d_game_authoring_restore_document(authoring->document, previous);
    for (index = 0U; index < authoring->binding_count; ++index)
    {
        authoring->bindings[index].document_id = previous_ids[index];
        (void)sandbox3d_scene_document_bridge_bind(
            authoring->bridge,
            previous_ids[index],
            authoring->bindings[index].entity);
        (void)sandbox3d_scene_document_bridge_apply_object(
            authoring->bridge,
            previous_ids[index]);
    }
load_cleanup:
    henka_scene_document_destroy(candidate);
    henka_scene_document_destroy(previous);
    return result;
}

const char* sandbox3d_game_authoring_get_relative_path(
    const sandbox3d_game_authoring* authoring)
{
    return authoring == NULL ? NULL : authoring->relative_path;
}

henka_scene* sandbox3d_game_authoring_get_authoring_scene(
    const sandbox3d_game_authoring* authoring)
{
    return authoring == NULL ? NULL : authoring->scene;
}

henka_scene* sandbox3d_game_authoring_get_play_scene(
    const sandbox3d_game_authoring* authoring)
{
    return authoring == NULL ? NULL : authoring->play_scene;
}

sandbox3d_play_session_state sandbox3d_game_authoring_get_play_state(
    const sandbox3d_game_authoring* authoring)
{
    return authoring == NULL
        ? SANDBOX3D_PLAY_SESSION_FAILED
        : authoring->play_session == NULL
            ? SANDBOX3D_PLAY_SESSION_STOPPED
            : sandbox3d_play_session_get_state(authoring->play_session);
}

henka_result sandbox3d_game_authoring_start_play(
    sandbox3d_game_authoring* authoring)
{
    sandbox3d_play_session* play_session = NULL;
    henka_result result;
    size_t index;

    if (authoring == NULL || sandbox3d_game_authoring_get_play_state(authoring) !=
        SANDBOX3D_PLAY_SESSION_STOPPED || authoring->play_scene != NULL ||
        authoring->play_bridge != NULL || authoring->play_session != NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_scene_clone(authoring->scene, &authoring->play_scene);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_create(
            authoring->document,
            authoring->play_scene,
            &authoring->play_bridge);
    }
    for (index = 0U; result == HENKA_SUCCESS && index < authoring->binding_count; ++index)
    {
        result = sandbox3d_scene_document_bridge_bind(
            authoring->play_bridge,
            authoring->bindings[index].document_id,
            authoring->bindings[index].entity);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_play_session_create(
            authoring->play_bridge,
            authoring->play_world,
            &play_session);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_play_session_start(play_session);
    }
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_play_session_destroy(play_session);
        sandbox3d_scene_document_bridge_destroy(authoring->play_bridge);
        henka_scene_destroy(authoring->play_scene);
        authoring->play_bridge = NULL;
        authoring->play_scene = NULL;
        return result;
    }
    authoring->play_session = play_session;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_pause_play(
    sandbox3d_game_authoring* authoring)
{
    return authoring == NULL
        ? HENKA_ERROR_INVALID_ARGUMENT
        : authoring->play_session == NULL
            ? HENKA_ERROR_INVALID_ARGUMENT
            : sandbox3d_play_session_pause(authoring->play_session);
}

henka_result sandbox3d_game_authoring_resume_play(
    sandbox3d_game_authoring* authoring)
{
    return authoring == NULL
        ? HENKA_ERROR_INVALID_ARGUMENT
        : authoring->play_session == NULL
            ? HENKA_ERROR_INVALID_ARGUMENT
            : sandbox3d_play_session_resume(authoring->play_session);
}

henka_result sandbox3d_game_authoring_tick_play(
    sandbox3d_game_authoring* authoring)
{
    return authoring == NULL
        ? HENKA_ERROR_INVALID_ARGUMENT
        : authoring->play_session == NULL
            ? HENKA_ERROR_INVALID_ARGUMENT
            : sandbox3d_play_session_tick(authoring->play_session);
}

henka_result sandbox3d_game_authoring_step_play(
    sandbox3d_game_authoring* authoring)
{
    return authoring == NULL
        ? HENKA_ERROR_INVALID_ARGUMENT
        : authoring->play_session == NULL
            ? HENKA_ERROR_INVALID_ARGUMENT
            : sandbox3d_play_session_step_fixed(authoring->play_session);
}

henka_result sandbox3d_game_authoring_stop_play(
    sandbox3d_game_authoring* authoring)
{
    henka_result result;

    if (authoring == NULL || authoring->play_session == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_play_session_stop(authoring->play_session);
    sandbox3d_play_session_destroy(authoring->play_session);
    authoring->play_session = NULL;
    sandbox3d_scene_document_bridge_destroy(authoring->play_bridge);
    authoring->play_bridge = NULL;
    henka_scene_destroy(authoring->play_scene);
    authoring->play_scene = NULL;
    return HENKA_SUCCESS;
}

bool sandbox3d_game_authoring_is_play_locked(
    const sandbox3d_game_authoring* authoring)
{
    const sandbox3d_play_session_state state =
        sandbox3d_game_authoring_get_play_state(authoring);
    return authoring != NULL &&
        (state != SANDBOX3D_PLAY_SESSION_STOPPED ||
            sandbox3d_scene_document_bridge_is_play_locked(authoring->bridge));
}
