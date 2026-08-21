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
    int exit_code = 1;

    if (henka_scene_document_create(&document) != HENKA_SUCCESS ||
        henka_scene_create(&scene) != HENKA_SUCCESS ||
        henka_physics_world_create(&physics_world) != HENKA_SUCCESS)
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
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_STOPPED ||
        sandbox3d_play_session_start(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_RUNNING ||
        !sandbox3d_scene_document_bridge_is_play_locked(bridge) ||
        sandbox3d_scene_document_bridge_apply_object(bridge, object_id) != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_play_session_pause(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_step_fixed(session) != HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_play_session_resume(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_step_fixed(session) != HENKA_SUCCESS ||
        henka_scene_get_entity_transform(scene, entity, &transform) != HENKA_SUCCESS ||
        transform.position.y >= 5.0f ||
        henka_scene_set_entity_visible(scene, entity, false) != HENKA_SUCCESS ||
        sandbox3d_play_session_stop(session) != HENKA_SUCCESS ||
        sandbox3d_play_session_get_state(session) != SANDBOX3D_PLAY_SESSION_STOPPED ||
        sandbox3d_scene_document_bridge_is_play_locked(bridge) ||
        henka_scene_get_entity_transform(scene, entity, &transform) != HENKA_SUCCESS ||
        transform.position.y != 5.0f ||
        !henka_scene_is_entity_visible(scene, entity))
    {
        goto cleanup;
    }
    exit_code = 0;

cleanup:
    sandbox3d_play_session_destroy(session);
    sandbox3d_scene_document_bridge_destroy(bridge);
    henka_physics_world_destroy(physics_world);
    henka_scene_destroy(scene);
    henka_scene_document_destroy(document);
    return exit_code;
}
