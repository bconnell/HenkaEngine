#include <stdio.h>
#include <string.h>

#include <henka/scene.h>
#include <henka/scene_document.h>

#include "../examples/sandbox3d/scene_document_bridge.h"

int main(void)
{
    henka_scene_document* document = NULL;
    sandbox3d_scene_document_bridge* bridge = NULL;
    henka_scene* scene = NULL;
    henka_scene_document_object object = henka_scene_document_object_default();
    henka_physics_body_desc body_desc;
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_entity helper = HENKA_INVALID_ENTITY;
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    int exit_code = 1;

    if (henka_scene_document_create(&document) != HENKA_SUCCESS ||
        henka_scene_create(&scene) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    (void)snprintf(object.name, sizeof(object.name), "%s", "Bridge Object");
    object.transform.position = (henka_vec3){2.0f, 3.0f, 4.0f};
    object.interaction.enabled = true;
    object.interaction.max_distance = 5.0f;
    (void)snprintf(object.interaction.prompt, sizeof(object.interaction.prompt), "%s", "Use bridge");
    object.physics.enabled = true;
    object.physics.body_type = HENKA_PHYSICS_BODY_DYNAMIC;
    object.physics.shape = HENKA_PHYSICS_SHAPE_BOX;
    object.physics.box_half_extents = (henka_vec3){1.0f, 2.0f, 3.0f};
    object.physics.mass = 4.0f;
    object.physics.is_trigger = true;
    if (henka_scene_document_add_object(document, &object, &object_id) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_create(document, scene, &bridge) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    entity = henka_scene_create_entity_named(scene, "Runtime Object");
    helper = henka_scene_create_entity(scene);
    if (entity == HENKA_INVALID_ENTITY || helper == HENKA_INVALID_ENTITY ||
        henka_scene_set_entity_flags(scene, helper, HENKA_SCENE_ENTITY_FLAG_HELPER) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_bind(bridge, object_id, helper) == HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_bind(bridge, object_id, entity) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_get_binding_count(bridge) != 1U ||
        sandbox3d_scene_document_bridge_bind(bridge, object_id, entity) == HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_apply_object(bridge, object_id) != HENKA_SUCCESS ||
        strcmp(henka_scene_get_entity_name(scene, entity), "Bridge Object") != 0 ||
        henka_scene_get_entity_transform(scene, entity, &object.transform) != HENKA_SUCCESS ||
        object.transform.position.x != 2.0f ||
        sandbox3d_scene_document_bridge_make_physics_body_desc(
            bridge, object_id, &body_desc) != HENKA_SUCCESS ||
        body_desc.linked_entity != entity ||
        body_desc.collider.shape != HENKA_PHYSICS_SHAPE_BOX ||
        body_desc.collider.is_trigger != true ||
        body_desc.collider.data.box.half_extents.y != 2.0f)
    {
        goto cleanup;
    }
    if (henka_scene_set_entity_transform(
            scene,
            entity,
            (henka_transform){{9.0f, 8.0f, 7.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}}) != HENKA_SUCCESS ||
        henka_scene_set_entity_visible(scene, entity, false) != HENKA_SUCCESS ||
        henka_scene_set_entity_interaction(
            scene,
            entity,
            &(henka_interaction_desc){false, 1.0f, "Changed"}) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_sync_object(bridge, object_id) != HENKA_SUCCESS ||
        henka_scene_document_get_object(document, object_id, &object) != HENKA_SUCCESS ||
        object.transform.position.x != 9.0f || object.visible ||
        object.interaction.enabled || object.interaction.max_distance != 1.0f ||
        strcmp(object.interaction.prompt, "Changed") != 0)
    {
        goto cleanup;
    }
    henka_scene_destroy_entity(scene, entity);
    entity = HENKA_INVALID_ENTITY;
    if (sandbox3d_scene_document_bridge_get_entity(bridge, object_id, &entity) == HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_unbind(bridge, object_id) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_get_binding_count(bridge) != 0U)
    {
        goto cleanup;
    }
    exit_code = 0;

cleanup:
    sandbox3d_scene_document_bridge_destroy(bridge);
    henka_scene_destroy(scene);
    henka_scene_document_destroy(document);
    return exit_code;
}
