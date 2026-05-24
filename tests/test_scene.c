#include "test_suite.h"

#include <string.h>

#include <henka/scene.h>

void henka_test_scene(void)
{
    henka_scene* scene;
    henka_entity first;
    henka_entity second;
    henka_transform transform;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(scene != NULL);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 0U);

    first = henka_scene_create_entity_named(scene, "Ground");
    second = henka_scene_create_entity(scene);
    HENKA_TEST_ASSERT(first != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(second != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(scene, first));
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(scene, second));
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 2U);
    HENKA_TEST_ASSERT(henka_scene_is_entity_visible(scene, first));
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, first), "Ground") == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_name(scene, second) == NULL);

    transform = henka_transform_identity();
    transform.position.x = 5.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, first, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, first, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_visible(scene, first));
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, first, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_is_entity_visible(scene, first));
    HENKA_TEST_ASSERT(henka_scene_set_entity_name(scene, second, "Marker") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, second), "Marker") == 0);
    HENKA_TEST_ASSERT(henka_scene_set_entity_name(scene, second, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_name(scene, second) == NULL);

    henka_scene_destroy_entity(scene, first);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, first));
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 1U);

    henka_scene_destroy(scene);
}
