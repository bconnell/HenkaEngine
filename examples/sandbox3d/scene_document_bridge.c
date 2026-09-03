#include "scene_document_bridge.h"

#include <stdio.h>
#include <string.h>

#include <henka/memory.h>

#define SANDBOX3D_SCENE_DOCUMENT_BRIDGE_MAX_BINDINGS HENKA_SCENE_DOCUMENT_MAX_OBJECTS

typedef struct sandbox3d_scene_document_binding
{
    henka_scene_document_id document_id;
    henka_entity entity;
} sandbox3d_scene_document_binding;

struct sandbox3d_scene_document_bridge
{
    henka_scene_document* document;
    henka_scene* scene;
    bool play_locked;
    size_t binding_count;
    sandbox3d_scene_document_binding bindings[
        SANDBOX3D_SCENE_DOCUMENT_BRIDGE_MAX_BINDINGS];
};

static size_t sandbox3d_scene_document_bridge_find_document_index(
    const sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id)
{
    size_t index;
    if (bridge == NULL || document_id == HENKA_INVALID_SCENE_DOCUMENT_ID)
    {
        return SIZE_MAX;
    }
    for (index = 0U; index < bridge->binding_count; ++index)
    {
        if (bridge->bindings[index].document_id == document_id)
        {
            return index;
        }
    }
    return SIZE_MAX;
}

static size_t sandbox3d_scene_document_bridge_find_entity_index(
    const sandbox3d_scene_document_bridge* bridge,
    henka_entity entity)
{
    size_t index;
    if (bridge == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return SIZE_MAX;
    }
    for (index = 0U; index < bridge->binding_count; ++index)
    {
        if (bridge->bindings[index].entity == entity)
        {
            return index;
        }
    }
    return SIZE_MAX;
}

static henka_result sandbox3d_scene_document_bridge_get_bound_object(
    const sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id,
    henka_scene_document_object* out_object,
    henka_entity* out_entity)
{
    const size_t binding_index =
        sandbox3d_scene_document_bridge_find_document_index(bridge, document_id);
    if (bridge == NULL || out_object == NULL || out_entity == NULL ||
        binding_index == SIZE_MAX ||
        !henka_scene_is_entity_valid(
            bridge->scene, bridge->bindings[binding_index].entity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_scene_document_get_object(
            bridge->document, document_id, out_object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_entity = bridge->bindings[binding_index].entity;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_document_bridge_create(
    henka_scene_document* document,
    henka_scene* scene,
    sandbox3d_scene_document_bridge** out_bridge)
{
    sandbox3d_scene_document_bridge* bridge;
    if (out_bridge == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_bridge = NULL;
    if (document == NULL || scene == NULL ||
        henka_scene_document_validate(document) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    bridge = (sandbox3d_scene_document_bridge*)henka_calloc(1U, sizeof(*bridge));
    if (bridge == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    bridge->document = document;
    bridge->scene = scene;
    *out_bridge = bridge;
    return HENKA_SUCCESS;
}

void sandbox3d_scene_document_bridge_destroy(
    sandbox3d_scene_document_bridge* bridge)
{
    henka_free(bridge);
}

size_t sandbox3d_scene_document_bridge_get_binding_count(
    const sandbox3d_scene_document_bridge* bridge)
{
    return bridge == NULL ? 0U : bridge->binding_count;
}

henka_result sandbox3d_scene_document_bridge_get_binding_at(
    const sandbox3d_scene_document_bridge* bridge,
    size_t index,
    henka_scene_document_id* out_document_id,
    henka_entity* out_entity)
{
    if (out_document_id != NULL)
    {
        *out_document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    }
    if (out_entity != NULL)
    {
        *out_entity = HENKA_INVALID_ENTITY;
    }
    if (bridge == NULL || out_document_id == NULL || out_entity == NULL ||
        index >= bridge->binding_count ||
        !henka_scene_is_entity_valid(bridge->scene, bridge->bindings[index].entity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_document_id = bridge->bindings[index].document_id;
    *out_entity = bridge->bindings[index].entity;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_document_bridge_validate(
    const sandbox3d_scene_document_bridge* bridge)
{
    size_t document_count;
    size_t index;
    if (bridge == NULL || bridge->document == NULL || bridge->scene == NULL ||
        henka_scene_document_validate(bridge->document) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    document_count = henka_scene_document_get_object_count(bridge->document);
    if (document_count != bridge->binding_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < bridge->binding_count; ++index)
    {
        if (henka_scene_document_get_object(
                bridge->document,
                bridge->bindings[index].document_id,
                &(henka_scene_document_object){0}) != HENKA_SUCCESS ||
            !henka_scene_is_entity_valid(bridge->scene, bridge->bindings[index].entity))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    for (index = 0U; index < document_count; ++index)
    {
        henka_scene_document_object object;
        if (henka_scene_document_get_object_at(
                bridge->document,
                index,
                &object) != HENKA_SUCCESS ||
            sandbox3d_scene_document_bridge_find_document_index(
                bridge,
                object.id) == SIZE_MAX)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_document_bridge_begin_play(
    sandbox3d_scene_document_bridge* bridge)
{
    if (bridge == NULL || bridge->play_locked ||
        sandbox3d_scene_document_bridge_validate(bridge) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    bridge->play_locked = true;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_document_bridge_end_play(
    sandbox3d_scene_document_bridge* bridge)
{
    if (bridge == NULL || !bridge->play_locked)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    bridge->play_locked = false;
    return HENKA_SUCCESS;
}

bool sandbox3d_scene_document_bridge_is_play_locked(
    const sandbox3d_scene_document_bridge* bridge)
{
    return bridge != NULL && bridge->play_locked;
}

henka_scene* sandbox3d_scene_document_bridge_get_scene(
    const sandbox3d_scene_document_bridge* bridge)
{
    return bridge == NULL ? NULL : bridge->scene;
}

const henka_scene_document* sandbox3d_scene_document_bridge_get_document(
    const sandbox3d_scene_document_bridge* bridge)
{
    return bridge == NULL ? NULL : bridge->document;
}

henka_result sandbox3d_scene_document_bridge_apply_camera(
    const sandbox3d_scene_document_bridge* bridge)
{
    henka_camera camera;
    if (bridge == NULL || bridge->play_locked ||
        sandbox3d_scene_document_bridge_validate(bridge) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!henka_scene_document_has_camera(bridge->document))
    {
        return HENKA_SUCCESS;
    }
    if (henka_scene_document_get_camera(bridge->document, &camera) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_set_camera(bridge->scene, &camera);
}

henka_result sandbox3d_scene_document_bridge_sync_camera(
    sandbox3d_scene_document_bridge* bridge)
{
    henka_camera camera;
    if (bridge == NULL || bridge->play_locked ||
        sandbox3d_scene_document_bridge_validate(bridge) != HENKA_SUCCESS ||
        henka_scene_get_camera(bridge->scene, &camera) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_set_camera(bridge->document, &camera);
}

henka_result sandbox3d_scene_document_bridge_get_object(
    const sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id,
    henka_scene_document_object* out_object)
{
    if (bridge == NULL || out_object == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_get_object(
        bridge->document, document_id, out_object);
}

henka_result sandbox3d_scene_document_bridge_bind(
    sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id,
    henka_entity entity)
{
    henka_scene_document_object object;
    if (bridge == NULL || bridge->play_locked ||
        document_id == HENKA_INVALID_SCENE_DOCUMENT_ID ||
        entity == HENKA_INVALID_ENTITY ||
        bridge->binding_count >= SANDBOX3D_SCENE_DOCUMENT_BRIDGE_MAX_BINDINGS ||
        sandbox3d_scene_document_bridge_find_document_index(bridge, document_id) != SIZE_MAX ||
        sandbox3d_scene_document_bridge_find_entity_index(bridge, entity) != SIZE_MAX ||
        !henka_scene_is_entity_valid(bridge->scene, entity) ||
        henka_scene_is_entity_helper(bridge->scene, entity) ||
        henka_scene_document_get_object(bridge->document, document_id, &object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    bridge->bindings[bridge->binding_count++] =
        (sandbox3d_scene_document_binding){document_id, entity};
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_document_bridge_unbind(
    sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id)
{
    const size_t index =
        sandbox3d_scene_document_bridge_find_document_index(bridge, document_id);
    if (index == SIZE_MAX || bridge->play_locked)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (index + 1U < bridge->binding_count)
    {
        memmove(
            &bridge->bindings[index],
            &bridge->bindings[index + 1U],
            (bridge->binding_count - index - 1U) * sizeof(bridge->bindings[0]));
    }
    --bridge->binding_count;
    bridge->bindings[bridge->binding_count] =
        (sandbox3d_scene_document_binding){HENKA_INVALID_SCENE_DOCUMENT_ID, HENKA_INVALID_ENTITY};
    return HENKA_SUCCESS;
}

henka_result sandbox3d_scene_document_bridge_get_entity(
    const sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id,
    henka_entity* out_entity)
{
    const size_t index =
        sandbox3d_scene_document_bridge_find_document_index(bridge, document_id);
    if (out_entity != NULL)
    {
        *out_entity = HENKA_INVALID_ENTITY;
    }
    if (index == SIZE_MAX || out_entity == NULL ||
        !henka_scene_is_entity_valid(bridge->scene, bridge->bindings[index].entity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_entity = bridge->bindings[index].entity;
    return HENKA_SUCCESS;
}

static bool sandbox3d_scene_document_bridge_vec3_equal(
    henka_vec3 left,
    henka_vec3 right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

static bool sandbox3d_scene_document_bridge_vec4_equal(
    henka_vec4 left,
    henka_vec4 right)
{
    return left.x == right.x && left.y == right.y &&
        left.z == right.z && left.w == right.w;
}

static bool sandbox3d_scene_document_bridge_transform_equal(
    henka_transform left,
    henka_transform right)
{
    return sandbox3d_scene_document_bridge_vec3_equal(left.position, right.position) &&
        left.rotation.x == right.rotation.x &&
        left.rotation.y == right.rotation.y &&
        left.rotation.z == right.rotation.z &&
        left.rotation.w == right.rotation.w &&
        sandbox3d_scene_document_bridge_vec3_equal(left.scale, right.scale);
}

static bool sandbox3d_scene_document_bridge_material_scalars_equal(
    const henka_material* left,
    const henka_material* right)
{
    return left != NULL && right != NULL &&
        sandbox3d_scene_document_bridge_vec4_equal(left->base_color, right->base_color) &&
        left->metallic == right->metallic &&
        left->roughness == right->roughness &&
        sandbox3d_scene_document_bridge_vec3_equal(left->emissive_color, right->emissive_color) &&
        left->emissive_strength == right->emissive_strength;
}

henka_result sandbox3d_scene_document_bridge_apply_object(
    const sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id)
{
    henka_scene_object_info previous_info;
    henka_interaction_desc previous_interaction;
    henka_material previous_material;
    henka_material material = henka_material_default();
    const henka_material_asset* material_asset = NULL;
    char previous_name[HENKA_SCENE_DOCUMENT_MAX_NAME_BYTES];
    char previous_prompt[HENKA_SCENE_DOCUMENT_MAX_PROMPT_BYTES];
    henka_scene_document_object object;
    henka_interaction_desc interaction;
    henka_entity entity;
    henka_result result;
    bool name_changed;
    bool transform_changed;
    bool visible_changed;
    bool interaction_changed;
    bool material_changed = false;
    bool material_applied = false;
    uint64_t forward_mutations;
    uint64_t rollback_mutations;
    int written;
    if (bridge == NULL || bridge->play_locked ||
        sandbox3d_scene_document_bridge_get_bound_object(
            bridge, document_id, &object, &entity) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_scene_get_entity_info(bridge->scene, entity, &previous_info) != HENKA_SUCCESS ||
        henka_scene_get_entity_interaction(bridge->scene, entity, &previous_interaction) != HENKA_SUCCESS ||
        henka_scene_get_entity_material(bridge->scene, entity, &previous_material) != HENKA_SUCCESS ||
        henka_scene_get_entity_material_asset(bridge->scene, entity, &material_asset) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (object.renderer.material_override && material_asset != NULL)
    {
        /* Replacing a manager-owned definition requires the asset/material
         * authority, which this bridge does not own. Fail closed instead of
         * silently turning a persisted reference into an inline instance. */
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    name_changed = strcmp(
        previous_info.name == NULL ? "" : previous_info.name,
        object.name) != 0;
    transform_changed = !sandbox3d_scene_document_bridge_transform_equal(
        previous_info.transform,
        object.transform);
    visible_changed = previous_info.visible != object.visible;
    interaction_changed = previous_interaction.enabled != object.interaction.enabled ||
        previous_interaction.max_distance != object.interaction.max_distance ||
        strcmp(
            previous_interaction.prompt == NULL ? "" : previous_interaction.prompt,
            object.interaction.prompt) != 0;
    if (object.renderer.material_override)
    {
        material = previous_material;
        material.base_color = object.renderer.base_color;
        material.metallic = object.renderer.metallic;
        material.roughness = object.renderer.roughness;
        material.emissive_color = object.renderer.emissive;
        material.emissive_strength = object.renderer.emissive_strength;
        material_changed = !sandbox3d_scene_document_bridge_material_scalars_equal(
            &previous_material,
            &material);
    }
    forward_mutations = (transform_changed ? UINT64_C(1) : UINT64_C(0)) +
        (visible_changed ? UINT64_C(1) : UINT64_C(0)) +
        (interaction_changed ? UINT64_C(1) : UINT64_C(0)) +
        (material_changed ? UINT64_C(1) : UINT64_C(0));
    rollback_mutations = (transform_changed ? UINT64_C(1) : UINT64_C(0)) +
        (visible_changed ? UINT64_C(1) : UINT64_C(0)) +
        (interaction_changed ? UINT64_C(1) : UINT64_C(0)) +
        (material_changed ? UINT64_C(1) : UINT64_C(0));
    if (forward_mutations != 0U &&
        !henka_scene_has_render_revision_capacity(
            bridge->scene,
            forward_mutations + rollback_mutations))
    {
        return HENKA_ERROR_LIMIT;
    }
    written = snprintf(
        previous_name,
        sizeof(previous_name),
        "%s",
        previous_info.name == NULL ? "" : previous_info.name);
    if (written < 0 || (size_t)written >= sizeof(previous_name))
    {
        return HENKA_ERROR_LIMIT;
    }
    written = snprintf(
        previous_prompt,
        sizeof(previous_prompt),
        "%s",
        previous_interaction.prompt == NULL ? "" : previous_interaction.prompt);
    if (written < 0 || (size_t)written >= sizeof(previous_prompt))
    {
        return HENKA_ERROR_LIMIT;
    }
    result = name_changed
        ? henka_scene_set_entity_name(bridge->scene, entity, object.name)
        : HENKA_SUCCESS;
    if (result != HENKA_SUCCESS)
    {
        goto rollback;
    }
    result = transform_changed
        ? henka_scene_set_entity_transform(bridge->scene, entity, object.transform)
        : HENKA_SUCCESS;
    if (result != HENKA_SUCCESS)
    {
        goto rollback;
    }
    result = visible_changed
        ? henka_scene_set_entity_visible(bridge->scene, entity, object.visible)
        : HENKA_SUCCESS;
    if (result != HENKA_SUCCESS)
    {
        goto rollback;
    }
    if (interaction_changed)
    {
        interaction = (henka_interaction_desc){
            object.interaction.enabled,
            object.interaction.max_distance,
            object.interaction.prompt};
        result = henka_scene_set_entity_interaction(
            bridge->scene,
            entity,
            &interaction);
    }
    else
    {
        result = HENKA_SUCCESS;
    }
    if (result == HENKA_SUCCESS && material_changed)
    {
        result = henka_scene_set_entity_material(bridge->scene, entity, material);
        material_applied = result == HENKA_SUCCESS;
    }
    if (result == HENKA_SUCCESS)
    {
        return HENKA_SUCCESS;
    }

rollback:
    /* Every value above was validated before publication. Restore the
     * complete runtime presentation if a later setter rejects the update. */
    if (material_applied)
    {
        (void)henka_scene_set_entity_material(
            bridge->scene,
            entity,
            previous_material);
    }
    if (interaction_changed)
    {
        (void)henka_scene_set_entity_interaction(
            bridge->scene,
            entity,
            &(henka_interaction_desc){
                previous_interaction.enabled,
                previous_interaction.max_distance,
                previous_prompt});
    }
    if (visible_changed)
    {
        (void)henka_scene_set_entity_visible(
            bridge->scene,
            entity,
            previous_info.visible);
    }
    if (transform_changed)
    {
        (void)henka_scene_set_entity_transform(
            bridge->scene,
            entity,
            previous_info.transform);
    }
    if (name_changed)
    {
        (void)henka_scene_set_entity_name(
            bridge->scene,
            entity,
            previous_name);
    }
    return result;
}

henka_result sandbox3d_scene_document_bridge_apply_hierarchy(
    const sandbox3d_scene_document_bridge* bridge)
{
    henka_scene_document_id parent_ids[
        SANDBOX3D_SCENE_DOCUMENT_BRIDGE_MAX_BINDINGS];
    henka_entity entities[
        SANDBOX3D_SCENE_DOCUMENT_BRIDGE_MAX_BINDINGS];
    henka_entity parent_entities[
        SANDBOX3D_SCENE_DOCUMENT_BRIDGE_MAX_BINDINGS];
    henka_entity previous_parents[
        SANDBOX3D_SCENE_DOCUMENT_BRIDGE_MAX_BINDINGS];
    size_t applied_order[
        SANDBOX3D_SCENE_DOCUMENT_BRIDGE_MAX_BINDINGS];
    bool applied[
        SANDBOX3D_SCENE_DOCUMENT_BRIDGE_MAX_BINDINGS] = {false};
    size_t applied_count = 0U;
    size_t index;
    henka_result result = HENKA_SUCCESS;

    if (bridge == NULL || bridge->play_locked ||
        sandbox3d_scene_document_bridge_validate(bridge) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < bridge->binding_count; ++index)
    {
        henka_scene_document_object object;
        size_t parent_index;
        entities[index] = bridge->bindings[index].entity;
        if (henka_scene_document_get_object(
                bridge->document,
                bridge->bindings[index].document_id,
                &object) != HENKA_SUCCESS ||
            henka_scene_get_entity_parent(
                bridge->scene,
                entities[index],
                &previous_parents[index]) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        parent_ids[index] = object.parent_id;
        parent_entities[index] = HENKA_INVALID_ENTITY;
        if (parent_ids[index] == HENKA_INVALID_SCENE_DOCUMENT_ID)
        {
            continue;
        }
        parent_index = sandbox3d_scene_document_bridge_find_document_index(
            bridge, parent_ids[index]);
        if (parent_index == SIZE_MAX ||
            !henka_scene_is_entity_valid(
                bridge->scene, bridge->bindings[parent_index].entity))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        parent_entities[index] = bridge->bindings[parent_index].entity;
    }

    while (applied_count < bridge->binding_count)
    {
        bool made_progress = false;
        for (index = 0U; index < bridge->binding_count; ++index)
        {
            size_t parent_index;
            if (applied[index])
            {
                continue;
            }
            parent_index = parent_ids[index] == HENKA_INVALID_SCENE_DOCUMENT_ID
                ? SIZE_MAX
                : sandbox3d_scene_document_bridge_find_document_index(
                    bridge, parent_ids[index]);
            if (parent_index != SIZE_MAX && !applied[parent_index])
            {
                continue;
            }
            result = henka_scene_set_entity_parent(
                bridge->scene,
                entities[index],
                parent_entities[index],
                HENKA_SCENE_PARENT_KEEP_WORLD);
            if (result != HENKA_SUCCESS)
            {
                goto rollback;
            }
            applied[index] = true;
            applied_order[applied_count++] = index;
            made_progress = true;
        }
        if (!made_progress)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto rollback;
        }
    }
    return HENKA_SUCCESS;

rollback:
    while (applied_count > 0U)
    {
        const size_t restore_index = applied_order[--applied_count];
        (void)henka_scene_set_entity_parent(
            bridge->scene,
            entities[restore_index],
            previous_parents[restore_index],
            HENKA_SCENE_PARENT_KEEP_WORLD);
    }
    return result;
}

henka_result sandbox3d_scene_document_bridge_sync_object(
    sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id)
{
    henka_scene_document_object candidate;
    henka_scene_object_info info;
    henka_interaction_desc interaction;
    henka_entity entity;
    int written;

    if (bridge == NULL || bridge->play_locked ||
        sandbox3d_scene_document_bridge_get_bound_object(
            bridge, document_id, &candidate, &entity) != HENKA_SUCCESS ||
        henka_scene_get_entity_info(bridge->scene, entity, &info) != HENKA_SUCCESS ||
        henka_scene_get_entity_interaction(bridge->scene, entity, &interaction) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    written = snprintf(
        candidate.name,
        sizeof(candidate.name),
        "%s",
        info.name == NULL ? "" : info.name);
    if (written < 0 || (size_t)written >= sizeof(candidate.name))
    {
        return HENKA_ERROR_LIMIT;
    }
    written = snprintf(
        candidate.interaction.prompt,
        sizeof(candidate.interaction.prompt),
        "%s",
        interaction.prompt == NULL ? "" : interaction.prompt);
    if (written < 0 || (size_t)written >= sizeof(candidate.interaction.prompt))
    {
        return HENKA_ERROR_LIMIT;
    }
    candidate.transform = info.transform;
    candidate.visible = info.visible;
    candidate.interaction.enabled = interaction.enabled;
    candidate.interaction.max_distance = interaction.max_distance;
    return henka_scene_document_set_object(bridge->document, &candidate);
}

henka_result sandbox3d_scene_document_bridge_make_physics_body_desc(
    const sandbox3d_scene_document_bridge* bridge,
    henka_scene_document_id document_id,
    henka_physics_body_desc* out_desc)
{
    henka_scene_document_object object;
    henka_entity entity;
    if (out_desc == NULL ||
        sandbox3d_scene_document_bridge_get_bound_object(
            bridge, document_id, &object, &entity) != HENKA_SUCCESS ||
        !object.physics.enabled)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(out_desc, 0, sizeof(*out_desc));
    out_desc->type = object.physics.body_type;
    out_desc->transform = object.transform;
    out_desc->mass = object.physics.mass;
    out_desc->material = object.physics.material;
    out_desc->collider = object.physics.shape == HENKA_PHYSICS_SHAPE_SPHERE
        ? henka_physics_collider_sphere(object.physics.sphere_radius)
        : henka_physics_collider_box(object.physics.box_half_extents);
    out_desc->collider.offset = object.physics.collider_offset;
    out_desc->collider.is_trigger = object.physics.is_trigger;
    out_desc->collider.layer = object.physics.layer;
    out_desc->collider.mask = object.physics.mask;
    out_desc->linked_scene = bridge->scene;
    out_desc->linked_entity = entity;
    return HENKA_SUCCESS;
}
