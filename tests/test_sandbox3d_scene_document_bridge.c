#include <stdio.h>
#include <string.h>

#include <henka/core.h>
#include <henka/scene.h>
#include <henka/scene_document.h>

#include "../examples/sandbox3d/scene_document_bridge.h"

int main(void)
{
    henka_scene_document* document = NULL;
    sandbox3d_scene_document_bridge* bridge = NULL;
    henka_scene* scene = NULL;
    henka_scene_document_object object = henka_scene_document_object_default();
    henka_scene_document_object child_object = henka_scene_document_object_default();
    henka_physics_body_desc body_desc;
    henka_entity entity = HENKA_INVALID_ENTITY;
    henka_entity child_entity = HENKA_INVALID_ENTITY;
    henka_entity helper = HENKA_INVALID_ENTITY;
    henka_entity parent_entity = HENKA_INVALID_ENTITY;
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id child_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_camera authored_camera;
    henka_camera loaded_camera;
    henka_camera unchanged_camera;
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
    (void)snprintf(child_object.name, sizeof(child_object.name), "%s", "Bridge Child");
    child_object.transform.position = (henka_vec3){8.0f, 3.0f, 4.0f};
    if (henka_scene_document_add_object(document, &object, &object_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    child_object.parent_id = object_id;
    authored_camera = henka_camera_create_perspective(
        55.0f * HENKA_DEG_TO_RAD,
        4.0f / 3.0f,
        0.2f,
        500.0f);
    authored_camera.position = (henka_vec3){4.0f, 5.0f, 6.0f};
    authored_camera.yaw_radians = -0.4f;
    authored_camera.pitch_radians = 0.15f;
    if (henka_scene_document_add_object(document, &child_object, &child_id) != HENKA_SUCCESS ||
        henka_scene_document_set_camera(document, &authored_camera) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_create(document, scene, &bridge) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    entity = henka_scene_create_entity_named(scene, "Runtime Object");
    child_entity = henka_scene_create_entity_named(scene, "Runtime Child");
    helper = henka_scene_create_entity(scene);
    if (entity == HENKA_INVALID_ENTITY || child_entity == HENKA_INVALID_ENTITY ||
        helper == HENKA_INVALID_ENTITY ||
        henka_scene_set_entity_flags(scene, helper, HENKA_SCENE_ENTITY_FLAG_HELPER) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_bind(bridge, object_id, helper) == HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_bind(bridge, object_id, entity) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_bind(bridge, child_id, child_entity) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_get_binding_count(bridge) != 2U ||
        sandbox3d_scene_document_bridge_bind(bridge, object_id, entity) == HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_apply_object(bridge, object_id) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_apply_object(bridge, child_id) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_apply_hierarchy(bridge) != HENKA_SUCCESS ||
        strcmp(henka_scene_get_entity_name(scene, entity), "Bridge Object") != 0 ||
        henka_scene_get_entity_transform(scene, entity, &object.transform) != HENKA_SUCCESS ||
        object.transform.position.x != 2.0f ||
        henka_scene_get_entity_parent(scene, child_entity, &parent_entity) != HENKA_SUCCESS ||
        parent_entity != entity ||
        henka_scene_get_entity_local_transform(scene, child_entity, &object.transform) != HENKA_SUCCESS ||
        object.transform.position.x != 6.0f ||
        sandbox3d_scene_document_bridge_make_physics_body_desc(
            bridge, object_id, &body_desc) != HENKA_SUCCESS ||
        body_desc.linked_entity != entity ||
        body_desc.collider.shape != HENKA_PHYSICS_SHAPE_BOX ||
        body_desc.collider.is_trigger != true ||
        body_desc.collider.data.box.half_extents.y != 2.0f)
    {
        goto cleanup;
    }
    if (sandbox3d_scene_document_bridge_apply_camera(bridge) != HENKA_SUCCESS ||
        henka_scene_get_camera(scene, &loaded_camera) != HENKA_SUCCESS ||
        loaded_camera.position.x != authored_camera.position.x ||
        loaded_camera.position.y != authored_camera.position.y ||
        loaded_camera.yaw_radians != authored_camera.yaw_radians ||
        loaded_camera.pitch_radians != authored_camera.pitch_radians ||
        loaded_camera.projection_mode != authored_camera.projection_mode)
    {
        goto cleanup;
    }
    authored_camera.position = (henka_vec3){-7.0f, 8.0f, 9.0f};
    authored_camera.roll_radians = 0.2f;
    if (henka_scene_set_camera(scene, &authored_camera) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_sync_camera(bridge) != HENKA_SUCCESS ||
        henka_scene_document_get_camera(document, &loaded_camera) != HENKA_SUCCESS ||
        loaded_camera.position.z != authored_camera.position.z ||
        loaded_camera.roll_radians != authored_camera.roll_radians)
    {
        goto cleanup;
    }
    if (henka_scene_document_clear_camera(document) != HENKA_SUCCESS ||
        henka_scene_get_camera(scene, &unchanged_camera) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_apply_camera(bridge) != HENKA_SUCCESS ||
        henka_scene_get_camera(scene, &loaded_camera) != HENKA_SUCCESS ||
        loaded_camera.position.x != unchanged_camera.position.x ||
        loaded_camera.position.y != unchanged_camera.position.y ||
        loaded_camera.position.z != unchanged_camera.position.z)
    {
        goto cleanup;
    }
    if (sandbox3d_scene_document_bridge_begin_play(bridge) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_apply_camera(bridge) != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_scene_document_bridge_sync_camera(bridge) != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_scene_document_bridge_end_play(bridge) != HENKA_SUCCESS)
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
    if (sandbox3d_scene_document_bridge_unbind(bridge, object_id) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_apply_hierarchy(bridge) == HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_bind(bridge, object_id, entity) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_apply_hierarchy(bridge) != HENKA_SUCCESS ||
        sandbox3d_scene_document_bridge_unbind(bridge, child_id) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_scene_destroy_entity(scene, child_entity);
    child_entity = HENKA_INVALID_ENTITY;
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
