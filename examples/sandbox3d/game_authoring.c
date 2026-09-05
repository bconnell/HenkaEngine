#include "game_authoring.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/script_asset.h>

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
    henka_script_state_store* script_state_store;
    /* Edit-owned state is never borrowed by a live Play host.  The last
     * stopped Play store is retained only for an explicit save operation. */
    henka_script_state_store* play_script_state_store;
    henka_scene* play_scene;
    sandbox3d_scene_document_bridge* play_bridge;
    sandbox3d_play_session* play_session;
    henka_audio_system* audio_system;
    henka_asset_manager* audio_asset_manager;
    sandbox3d_game_authoring_input_query play_input_query;
    void* play_input_user_data;
    henka_vec3 play_observer_position;
    bool play_observer_position_valid;
    char relative_path[SANDBOX3D_GAME_AUTHORING_MAX_RELATIVE_PATH_BYTES];
    char project_root[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
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

static henka_result sandbox3d_game_authoring_build_object(
    const henka_scene* scene,
    henka_entity entity,
    henka_scene_document_object* out_object)
{
    henka_scene_object_info info;
    henka_interaction_desc interaction;
    henka_material material;
    const henka_material_asset* material_asset = NULL;
    uint64_t material_asset_revision = 0U;
    bool material_asset_overridden = false;
    int written;
    if (scene == NULL || out_object == NULL ||
        !henka_scene_is_entity_valid(scene, entity) ||
        henka_scene_get_entity_info(scene, entity, &info) != HENKA_SUCCESS ||
        henka_scene_get_entity_interaction(scene, entity, &interaction) != HENKA_SUCCESS ||
        henka_scene_get_entity_material(scene, entity, &material) != HENKA_SUCCESS ||
        henka_scene_get_entity_material_asset(scene, entity, &material_asset) != HENKA_SUCCESS ||
        henka_scene_get_entity_material_asset_state(
            scene,
            entity,
            &material_asset_revision,
            &material_asset_overridden) != HENKA_SUCCESS)
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
    out_object->renderer.base_color = material.base_color;
    out_object->renderer.metallic = material.metallic;
    out_object->renderer.roughness = material.roughness;
    out_object->renderer.emissive = material.emissive_color;
    out_object->renderer.emissive_strength = material.emissive_strength;
    if (material_asset != NULL && material_asset_overridden)
    {
        /* This bridge has no material-asset path/authority with which to
         * reconstruct an explicit override. Do not collapse that state into
         * an apparently asset-owned document. */
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    /* Inline scalar state is document-owned only when no manager-owned
     * material definition is attached. */
    out_object->renderer.material_override =
        material_asset == NULL && material.shader != NULL;
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

static henka_result sandbox3d_game_authoring_set_project_root(
    sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    int written;
    if (authoring == NULL || project_root == NULL || project_root[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    written = snprintf(
        authoring->project_root,
        sizeof(authoring->project_root),
        "%s",
        project_root);
    return written < 0
        ? HENKA_ERROR_INVALID_ARGUMENT
        : (size_t)written >= sizeof(authoring->project_root)
            ? HENKA_ERROR_LIMIT
            : HENKA_SUCCESS;
}

static henka_result sandbox3d_game_authoring_get_state_relative_path(
    const sandbox3d_game_authoring* authoring,
    char* out_path,
    size_t out_path_capacity)
{
    int written;
    if (authoring == NULL || out_path == NULL || out_path_capacity == 0U ||
        authoring->relative_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    written = snprintf(
        out_path,
        out_path_capacity,
        "%s.hstate",
        authoring->relative_path);
    if (written < 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return (size_t)written >= out_path_capacity
        ? HENKA_ERROR_LIMIT
        : HENKA_SUCCESS;
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
    (void)snprintf(
        authoring->project_root,
        sizeof(authoring->project_root),
        "%s",
        ".");
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
    if (result == HENKA_SUCCESS)
    {
        result = henka_script_state_store_create(&authoring->script_state_store);
    }
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_play_session_destroy(authoring->play_session);
        henka_physics_world_destroy(authoring->play_world);
        henka_script_state_store_destroy(authoring->script_state_store);
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
    henka_script_state_store_destroy(authoring->play_script_state_store);
    henka_script_state_store_destroy(authoring->script_state_store);
    sandbox3d_scene_document_bridge_destroy(authoring->bridge);
    henka_scene_document_destroy(authoring->document);
    henka_free(authoring);
}

static henka_result sandbox3d_game_authoring_register_entity_once(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_id* out_document_id)
{
    henka_scene_document_object object;
    henka_scene_document_id document_id;
    henka_entity parent_entity = HENKA_INVALID_ENTITY;
    size_t parent_index;
    henka_result result;

    if (authoring == NULL || out_document_id == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (authoring->binding_count >= SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
    {
        return HENKA_ERROR_LIMIT;
    }
    if (sandbox3d_game_authoring_find_binding(authoring, entity) != SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_game_authoring_build_object(
        authoring->scene, entity, &object);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (henka_scene_get_entity_parent(
            authoring->scene, entity, &parent_entity) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (parent_entity != HENKA_INVALID_ENTITY)
    {
        parent_index = sandbox3d_game_authoring_find_binding(
            authoring, parent_entity);
        if (parent_index == SIZE_MAX)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        object.parent_id = authoring->bindings[parent_index].document_id;
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

henka_result sandbox3d_game_authoring_register_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_id* out_document_id)
{
    henka_entity chain[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    henka_entity current_entity;
    henka_scene_document_object target_object;
    henka_scene_document_id document_id;
    henka_result result;
    size_t chain_count = 0U;
    size_t initial_binding_count;
    size_t index;

    if (out_document_id != NULL)
    {
        *out_document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    }
    if (authoring == NULL || out_document_id == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* Validate the target before honoring an existing binding so stale scene
     * entities cannot be mistaken for an idempotent registration. */
    result = sandbox3d_game_authoring_build_object(
        authoring->scene, entity, &target_object);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    index = sandbox3d_game_authoring_find_binding(authoring, entity);
    if (index != SIZE_MAX)
    {
        *out_document_id = authoring->bindings[index].document_id;
        return HENKA_SUCCESS;
    }

    /* Preflight the complete live parent chain before mutating the document
     * or bridge.  This makes registration independent of scene iteration
     * order and keeps failures before the first visible authoring mutation. */
    current_entity = entity;
    while (current_entity != HENKA_INVALID_ENTITY &&
           sandbox3d_game_authoring_find_binding(authoring, current_entity) == SIZE_MAX)
    {
        henka_scene_document_object preflight_object;
        henka_entity parent_entity = HENKA_INVALID_ENTITY;

        if (chain_count >= SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
        {
            return HENKA_ERROR_LIMIT;
        }
        for (index = 0U; index < chain_count; ++index)
        {
            if (chain[index] == current_entity)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
        result = sandbox3d_game_authoring_build_object(
            authoring->scene, current_entity, &preflight_object);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (henka_scene_get_entity_parent(
                authoring->scene, current_entity, &parent_entity) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        chain[chain_count++] = current_entity;
        current_entity = parent_entity;
    }
    if (chain_count > SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS - authoring->binding_count)
    {
        return HENKA_ERROR_LIMIT;
    }

    initial_binding_count = authoring->binding_count;
    for (index = chain_count; index > 0U; --index)
    {
        result = sandbox3d_game_authoring_register_entity_once(
            authoring, chain[index - 1U], &document_id);
        if (result != HENKA_SUCCESS)
        {
            while (authoring->binding_count > initial_binding_count)
            {
                henka_entity rollback_entity =
                    authoring->bindings[authoring->binding_count - 1U].entity;
                if (sandbox3d_game_authoring_unregister_entity(
                        authoring, rollback_entity) != HENKA_SUCCESS)
                {
                    return HENKA_ERROR_UNKNOWN;
                }
            }
            return result;
        }
    }

    index = sandbox3d_game_authoring_find_binding(authoring, entity);
    if (index == SIZE_MAX)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    *out_document_id = authoring->bindings[index].document_id;
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
    henka_scene* candidate_scene = NULL;
    sandbox3d_scene_document_bridge* candidate_bridge = NULL;
    henka_scene_document_id document_id;
    size_t binding_index;
    size_t binding_count;
    henka_scene_document_id binding_document_id;
    henka_entity binding_entity;
    henka_result rollback_result;
    henka_result result;
    if (authoring == NULL || object == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &document_id, &previous) != HENKA_SUCCESS ||
        object->id != document_id)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_scene_clone(authoring->scene, &candidate_scene);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_scene_document_bridge_create(
        authoring->document,
        candidate_scene,
        &candidate_bridge);
    if (result == HENKA_SUCCESS)
    {
        binding_count = sandbox3d_scene_document_bridge_get_binding_count(
            authoring->bridge);
        for (binding_index = 0U;
            binding_index < binding_count && result == HENKA_SUCCESS;
            ++binding_index)
        {
            result = sandbox3d_scene_document_bridge_get_binding_at(
                authoring->bridge,
                binding_index,
                &binding_document_id,
                &binding_entity);
            if (result == HENKA_SUCCESS)
            {
                result = sandbox3d_scene_document_bridge_bind(
                    candidate_bridge,
                    binding_document_id,
                    binding_entity);
            }
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_object_candidate(
            candidate_bridge,
            document_id,
            object);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_hierarchy_candidate(
            candidate_bridge,
            document_id,
            object);
    }
    sandbox3d_scene_document_bridge_destroy(candidate_bridge);
    henka_scene_destroy(candidate_scene);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_scene_document_set_object(authoring->document, object);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_scene_document_bridge_apply_object(authoring->bridge, document_id);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_hierarchy(authoring->bridge);
    }
    if (result != HENKA_SUCCESS)
    {
        rollback_result = henka_scene_document_set_object(
            authoring->document,
            &previous);
        if (rollback_result == HENKA_SUCCESS)
        {
            rollback_result = sandbox3d_scene_document_bridge_apply_object(
                authoring->bridge,
                document_id);
        }
        if (rollback_result == HENKA_SUCCESS)
        {
            rollback_result = sandbox3d_scene_document_bridge_apply_hierarchy(
                authoring->bridge);
        }
        if (rollback_result != HENKA_SUCCESS)
        {
            return HENKA_ERROR_UNKNOWN;
        }
    }
    return result;
}

size_t sandbox3d_game_authoring_get_behavior_count_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity)
{
    size_t index;
    if (authoring == NULL ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return 0U;
    }
    return henka_scene_document_get_behavior_count(
        authoring->document,
        authoring->bindings[index].document_id);
}

henka_result sandbox3d_game_authoring_get_behavior_at_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity,
    size_t behavior_index,
    henka_scene_document_behavior* out_behavior)
{
    size_t index;
    if (authoring == NULL || out_behavior == NULL ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_get_behavior_at(
        authoring->document,
        authoring->bindings[index].document_id,
        behavior_index,
        out_behavior);
}

henka_result sandbox3d_game_authoring_get_behavior_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_behavior_id behavior_id,
    henka_scene_document_behavior* out_behavior)
{
    size_t index;
    if (authoring == NULL || out_behavior == NULL ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_get_behavior(
        authoring->document,
        authoring->bindings[index].document_id,
        behavior_id,
        out_behavior);
}

henka_result sandbox3d_game_authoring_add_behavior_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    const henka_scene_document_behavior* behavior,
    henka_scene_document_behavior_id* out_behavior_id)
{
    size_t index;
    if (out_behavior_id != NULL)
    {
        *out_behavior_id = HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID;
    }
    if (authoring == NULL || behavior == NULL || out_behavior_id == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_add_behavior(
        authoring->document,
        authoring->bindings[index].document_id,
        behavior,
        out_behavior_id);
}

henka_result sandbox3d_game_authoring_update_behavior_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    const henka_scene_document_behavior* behavior)
{
    size_t index;
    if (authoring == NULL || behavior == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_set_behavior(
        authoring->document,
        authoring->bindings[index].document_id,
        behavior);
}

henka_result sandbox3d_game_authoring_remove_behavior_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_behavior_id behavior_id)
{
    size_t index;
    if (authoring == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_remove_behavior(
        authoring->document,
        authoring->bindings[index].document_id,
        behavior_id);
}

henka_result sandbox3d_game_authoring_reload_behavior_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_behavior_id behavior_id,
    henka_script_source_diagnostic* out_diagnostic)
{
    size_t index;
    if (out_diagnostic != NULL)
    {
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
        out_diagnostic->result = HENKA_ERROR_INVALID_ARGUMENT;
        (void)snprintf(
            out_diagnostic->message,
            sizeof(out_diagnostic->message),
            "Behavior reload rejected");
    }
    if (authoring == NULL || behavior_id == HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_play_session_reload_behavior(
        authoring->play_session,
        authoring->bindings[index].document_id,
        behavior_id,
        out_diagnostic);
}

henka_result sandbox3d_game_authoring_attach_script_template(
    sandbox3d_game_authoring* authoring,
    const char* project_root,
    henka_entity entity,
    henka_script_language language)
{
    henka_scene_document_behavior behavior;
    henka_scene_document_behavior_id behavior_id;
    henka_scene_document_id document_id;
    henka_scene_document_object object;
    char relative_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    const char* extension;
    int written;
    size_t behavior_count;
    henka_result result;

    if (authoring == NULL || project_root == NULL || project_root[0] == '\0' ||
        strlen(project_root) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES ||
        entity == HENKA_INVALID_ENTITY ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        (language != HENKA_SCRIPT_LANGUAGE_LUA &&
         language != HENKA_SCRIPT_LANGUAGE_HENKASCRIPT) ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &document_id, &object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    behavior_count = object.behavior_count;
    if (behavior_count >= HENKA_SCENE_DOCUMENT_MAX_BEHAVIORS_PER_OBJECT)
    {
        return HENKA_ERROR_LIMIT;
    }
    extension = language == HENKA_SCRIPT_LANGUAGE_LUA ? "lua" : "hks";
    written = snprintf(
        relative_path,
        sizeof(relative_path),
        "scripts/behavior_%llu_%zu.%s",
        (unsigned long long)document_id,
        behavior_count + 1U,
        extension);
    if (written < 0 || (size_t)written >= sizeof(relative_path))
    {
        return HENKA_ERROR_LIMIT;
    }
    behavior = henka_scene_document_behavior_default();
    behavior.language = language;
    written = snprintf(
        behavior.asset_path,
        sizeof(behavior.asset_path),
        "%s",
        relative_path);
    if (written < 0 || (size_t)written >= sizeof(behavior.asset_path))
    {
        return HENKA_ERROR_LIMIT;
    }
    result = sandbox3d_game_authoring_add_behavior_for_entity(
        authoring,
        entity,
        &behavior,
        &behavior_id);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_script_asset_create_template(
        project_root,
        relative_path,
        language);
    if (result != HENKA_SUCCESS)
    {
        (void)sandbox3d_game_authoring_remove_behavior_for_entity(
            authoring,
            entity,
            behavior_id);
        return result;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_save(
    sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    henka_camera previous_camera;
    const bool had_authored_camera =
        authoring != NULL && authoring->document != NULL &&
        henka_scene_document_has_camera(authoring->document);
    henka_result result;
    if (authoring == NULL || project_root == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        strlen(project_root) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (had_authored_camera &&
        henka_scene_document_get_camera(authoring->document, &previous_camera) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_scene_document_bridge_sync_camera(authoring->bridge);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_scene_document_save_file(
        authoring->document,
        project_root,
        authoring->relative_path);
    if (result != HENKA_SUCCESS)
    {
        if (had_authored_camera)
        {
            (void)henka_scene_document_set_camera(authoring->document, &previous_camera);
        }
        else
        {
            (void)henka_scene_document_clear_camera(authoring->document);
        }
        return result;
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_game_authoring_set_project_root(authoring, project_root);
    }
    return result;
}

henka_result sandbox3d_game_authoring_load(
    sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    henka_scene_document* candidate = NULL;
    henka_scene* candidate_scene = NULL;
    sandbox3d_scene_document_bridge* candidate_bridge = NULL;
    size_t index;
    henka_result result;
    if (authoring == NULL || project_root == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        strlen(project_root) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES)
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
    if (henka_scene_document_get_object_count(candidate) != authoring->binding_count)
    {
        henka_scene_document_destroy(candidate);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < authoring->binding_count; ++index)
    {
        if (henka_scene_document_get_object(
                candidate,
                authoring->bindings[index].document_id,
                &(henka_scene_document_object){0}) == HENKA_SUCCESS)
        {
            continue;
        }
        henka_scene_document_destroy(candidate);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_scene_clone(authoring->scene, &candidate_scene);
    if (result != HENKA_SUCCESS)
    {
        goto load_cleanup;
    }
    result = sandbox3d_scene_document_bridge_create(
        candidate,
        candidate_scene,
        &candidate_bridge);
    for (index = 0U;
        index < authoring->binding_count && result == HENKA_SUCCESS;
        ++index)
    {
        result = sandbox3d_scene_document_bridge_bind(
            candidate_bridge,
            authoring->bindings[index].document_id,
            authoring->bindings[index].entity);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_objects(candidate_bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_hierarchy(
            candidate_bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_camera(
            candidate_bridge);
    }
    sandbox3d_scene_document_bridge_destroy(candidate_bridge);
    candidate_bridge = NULL;
    if (result != HENKA_SUCCESS)
    {
        goto load_cleanup;
    }
    /* Publish only a candidate whose persistent object IDs exactly match the
     * live binding set. Runtime entities remain generation-checked derived
     * state; names are not identity and cannot silently remap a loaded file. */
    result = henka_scene_replace_contents(authoring->scene, candidate_scene);
    if (result != HENKA_SUCCESS)
    {
        goto load_cleanup;
    }
    result = henka_scene_document_swap_contents(
        authoring->document,
        candidate);
    if (result != HENKA_SUCCESS)
    {
        /* Both swaps are allocation-free. Restore the first swap if the
         * validated document invariant is violated rather than reconstructing
         * state through fallible setters. */
        if (henka_scene_replace_contents(authoring->scene, candidate_scene) !=
            HENKA_SUCCESS)
        {
            result = HENKA_ERROR_UNKNOWN;
        }
        goto load_cleanup;
    }
    henka_scene_document_destroy(candidate);
    candidate = NULL;
    henka_scene_destroy(candidate_scene);
    candidate_scene = NULL;
    (void)sandbox3d_game_authoring_set_project_root(authoring, project_root);
    return result;

load_cleanup:
    sandbox3d_scene_document_bridge_destroy(candidate_bridge);
    henka_scene_destroy(candidate_scene);
    henka_scene_document_destroy(candidate);
    return result;
}

henka_result sandbox3d_game_authoring_save_play_state(
    sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    char state_relative_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    henka_result result;
    if (authoring == NULL || project_root == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        strlen(project_root) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES ||
        sandbox3d_game_authoring_get_state_relative_path(
            authoring,
            state_relative_path,
            sizeof(state_relative_path)) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_state_store_save_file(
        authoring->play_script_state_store != NULL
            ? authoring->play_script_state_store
            : authoring->script_state_store,
        project_root,
        state_relative_path);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_game_authoring_set_project_root(authoring, project_root);
    }
    return result;
}

henka_result sandbox3d_game_authoring_load_play_state(
    sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    char state_relative_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    henka_result result;
    if (authoring == NULL || project_root == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        strlen(project_root) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES ||
        sandbox3d_game_authoring_get_state_relative_path(
            authoring,
            state_relative_path,
            sizeof(state_relative_path)) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_state_store_load_file(
        authoring->script_state_store,
        project_root,
        state_relative_path);
    if (result == HENKA_SUCCESS)
    {
        henka_script_state_store_destroy(authoring->play_script_state_store);
        authoring->play_script_state_store = NULL;
        result = sandbox3d_game_authoring_set_project_root(authoring, project_root);
    }
    return result;
}

henka_result sandbox3d_game_authoring_set_script_state_value(
    sandbox3d_game_authoring* authoring,
    henka_script_state_identity identity,
    uint32_t key,
    henka_script_state_value value)
{
    if (authoring == NULL || sandbox3d_game_authoring_is_play_locked(authoring))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_script_state_store_set(
        authoring->script_state_store, identity, key, value);
}

henka_result sandbox3d_game_authoring_get_script_state_value(
    const sandbox3d_game_authoring* authoring,
    henka_script_state_identity identity,
    uint32_t key,
    henka_script_state_value* out_value,
    bool* out_present)
{
    if (authoring == NULL || sandbox3d_game_authoring_is_play_locked(authoring))
    {
        if (out_value != NULL)
        {
            *out_value = (henka_script_state_value){HENKA_SCRIPT_STATE_VALUE_NONE};
        }
        if (out_present != NULL)
        {
            *out_present = false;
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_script_state_store_get(
        authoring->script_state_store,
        identity,
        key,
        out_value,
        out_present);
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
    henka_script_state_store* play_script_state_store = NULL;
    henka_result result;
    size_t index;

    if (authoring == NULL || sandbox3d_game_authoring_get_play_state(authoring) !=
        SANDBOX3D_PLAY_SESSION_STOPPED || authoring->play_scene != NULL ||
        authoring->play_bridge != NULL || authoring->play_session != NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_script_state_store_clone(
        authoring->script_state_store,
        &play_script_state_store);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_clone(authoring->scene, &authoring->play_scene);
    }
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
        result = sandbox3d_play_session_create_with_project_root(
            authoring->play_bridge,
            authoring->play_world,
            authoring->project_root,
            &play_session);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_play_session_set_script_state_store(
            play_session,
            play_script_state_store);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_play_session_set_audio_system(
            play_session,
            authoring->audio_system);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_play_session_set_audio_asset_manager(
            play_session,
            authoring->audio_asset_manager);
    }
    if (result == HENKA_SUCCESS && authoring->play_observer_position_valid)
    {
        result = sandbox3d_play_session_set_input_context(
            play_session,
            authoring->play_input_query,
            authoring->play_input_user_data,
            authoring->play_observer_position);
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
        henka_script_state_store_destroy(play_script_state_store);
        authoring->play_bridge = NULL;
        authoring->play_scene = NULL;
        return result;
    }
    henka_script_state_store_destroy(authoring->play_script_state_store);
    authoring->play_script_state_store = play_script_state_store;
    authoring->play_session = play_session;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_set_audio_system(
    sandbox3d_game_authoring* authoring,
    henka_audio_system* audio_system)
{
    if (authoring == NULL ||
        sandbox3d_game_authoring_get_play_state(authoring) !=
            SANDBOX3D_PLAY_SESSION_STOPPED)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    authoring->audio_system = audio_system;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_set_audio_asset_manager(
    sandbox3d_game_authoring* authoring,
    henka_asset_manager* asset_manager)
{
    if (authoring == NULL ||
        sandbox3d_game_authoring_get_play_state(authoring) !=
            SANDBOX3D_PLAY_SESSION_STOPPED)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    authoring->audio_asset_manager = asset_manager;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_set_play_input_context(
    sandbox3d_game_authoring* authoring,
    sandbox3d_game_authoring_input_query input_query,
    void* input_user_data,
    henka_vec3 observer_position)
{
    if (authoring == NULL ||
        !isfinite(observer_position.x) ||
        !isfinite(observer_position.y) ||
        !isfinite(observer_position.z))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    authoring->play_input_query = input_query;
    authoring->play_input_user_data = input_user_data;
    authoring->play_observer_position = observer_position;
    authoring->play_observer_position_valid = true;
    if (authoring->play_session != NULL)
    {
        return sandbox3d_play_session_set_input_context(
            authoring->play_session,
            input_query,
            input_user_data,
            observer_position);
    }
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
    return result;
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
