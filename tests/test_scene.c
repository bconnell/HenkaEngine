#include "test_suite.h"

#include <string.h>

#include <henka/scene.h>

void henka_test_scene(void)
{
    henka_scene* scene;
    henka_entity found;
    henka_entity first;
    henka_entity listed;
    henka_entity second;
    henka_transform transform;
    henka_transform read_back;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(scene != NULL);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 0U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 0U) == HENKA_INVALID_ENTITY);

    first = henka_scene_create_entity_named(scene, "Ground");
    second = henka_scene_create_entity(scene);
    HENKA_TEST_ASSERT(first != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(second != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(scene, first));
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(scene, second));
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 2U);
    listed = henka_scene_get_entity_at_index(scene, 0U);
    HENKA_TEST_ASSERT(listed == first);
    listed = henka_scene_get_entity_at_index(scene, 1U);
    HENKA_TEST_ASSERT(listed == second);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 2U) == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_is_entity_visible(scene, first));
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, first), "Ground") == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_name(scene, second) == NULL);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(scene, "Ground", &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == first);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(scene, "Missing", &found) == HENKA_ERROR_UNKNOWN);

    transform = henka_transform_identity();
    transform.position.x = 5.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, first, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, first, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.position.x, 5.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, first, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_visible(scene, first));
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, first, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_is_entity_visible(scene, first));
    HENKA_TEST_ASSERT(henka_scene_set_entity_name(scene, second, "Marker") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, second), "Marker") == 0);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(scene, "Marker", &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    HENKA_TEST_ASSERT(henka_scene_set_entity_name(scene, second, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_name(scene, second) == NULL);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, HENKA_INVALID_ENTITY, &read_back) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(scene, NULL, &found) == HENKA_ERROR_INVALID_ARGUMENT);

    henka_scene_destroy_entity(scene, first);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, first));
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 1U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 0U) == second);

    henka_scene_destroy(scene);
}
