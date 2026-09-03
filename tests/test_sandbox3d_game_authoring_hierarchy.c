#include <stdio.h>

#include <henka/core.h>
#include <henka/scene.h>

#include "../examples/sandbox3d/game_authoring.h"

int main(void)
{
    const char* relative_path = "build/test_tmp/game_authoring_hierarchy.hscene";
    henka_scene* scene = NULL;
    sandbox3d_game_authoring* authoring = NULL;
    henka_entity parent = HENKA_INVALID_ENTITY;
    henka_entity child = HENKA_INVALID_ENTITY;
    henka_scene_document_id parent_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id child_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id duplicate_parent_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_id duplicate_child_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_object child_object;
    henka_transform parent_transform = henka_transform_identity();
    henka_transform child_transform = henka_transform_identity();
    int result = 1;

    parent_transform.position = (henka_vec3){8.0f, 1.0f, -4.0f};
    child_transform.position = (henka_vec3){1.0f, 2.0f, 3.0f};
    if (henka_scene_create(&scene) != HENKA_SUCCESS ||
        (child = henka_scene_create_entity_named(scene, "Child Registered First")) ==
            HENKA_INVALID_ENTITY ||
        (parent = henka_scene_create_entity_named(scene, "Parent Registered Recursively")) ==
            HENKA_INVALID_ENTITY ||
        henka_scene_set_entity_transform(scene, parent, parent_transform) != HENKA_SUCCESS ||
        henka_scene_set_entity_transform(scene, child, child_transform) != HENKA_SUCCESS ||
        henka_scene_set_entity_parent(
            scene,
            child,
            parent,
            HENKA_SCENE_PARENT_KEEP_LOCAL) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_create(scene, relative_path, &authoring) != HENKA_SUCCESS)
    {
        fprintf(stderr, "game authoring hierarchy test failed during setup\n");
        goto cleanup;
    }

    if (sandbox3d_game_authoring_register_entity(
            authoring, child, &child_id) != HENKA_SUCCESS ||
        child_id == HENKA_INVALID_SCENE_DOCUMENT_ID ||
        sandbox3d_game_authoring_register_entity(
            authoring, parent, &parent_id) != HENKA_SUCCESS ||
        parent_id == HENKA_INVALID_SCENE_DOCUMENT_ID ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, child, &duplicate_child_id, &child_object) != HENKA_SUCCESS ||
        duplicate_child_id != child_id ||
        child_object.parent_id != parent_id)
    {
        fprintf(stderr, "game authoring hierarchy test failed during child-first registration\n");
        goto cleanup;
    }

    if (sandbox3d_game_authoring_register_entity(
            authoring, parent, &duplicate_parent_id) != HENKA_SUCCESS ||
        duplicate_parent_id != parent_id ||
        sandbox3d_game_authoring_register_entity(
            authoring, child, &duplicate_child_id) != HENKA_SUCCESS ||
        duplicate_child_id != child_id)
    {
        fprintf(stderr, "game authoring hierarchy test failed during idempotent registration\n");
        goto cleanup;
    }

    result = 0;

cleanup:
    remove(relative_path);
    sandbox3d_game_authoring_destroy(authoring);
    henka_scene_destroy(scene);
    return result;
}
