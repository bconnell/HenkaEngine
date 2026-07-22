#include "test_suite.h"

#include <float.h>
#include <string.h>

#include <henka/core.h>
#include <henka/scene.h>

#include "../engine/src/core/checked.h"

static void henka_test_scene_capacity_growth(void)
{
    enum
    {
        ENTITY_COUNT = 40
    };
    henka_entity entities[ENTITY_COUNT];
    henka_entity replacement;
    henka_scene* scene;
    int index;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    for (index = 0; index < ENTITY_COUNT; ++index)
    {
        entities[index] = henka_scene_create_entity(scene);
        HENKA_TEST_ASSERT(entities[index] != HENKA_INVALID_ENTITY);
    }

    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == (size_t)ENTITY_COUNT);
    henka_scene_destroy_entity(scene, entities[7]);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == (size_t)ENTITY_COUNT - 1U);

    replacement = henka_scene_create_entity_named(scene, "Replacement");
    HENKA_TEST_ASSERT(replacement == entities[7]);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == (size_t)ENTITY_COUNT);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, replacement), "Replacement") == 0);

    henka_scene_destroy(scene);
}

void henka_test_scene(void)
{
    henka_bounds bounds;
    uint32_t flags;
    henka_scene* scene;
    henka_entity found;
    henka_entity first;
    henka_entity helper;
    henka_entity listed;
    henka_ray ray;
    henka_scene_object_info info;
    char interaction_prompt[] = "Inspect sample";
    henka_material material;
    char material_name[] = "Mutable Material";
    char overlong_text[HENKA_MAX_SCENE_TEXT_BYTES + 2U];
    henka_entity second;
    henka_interaction_desc interaction;
    henka_interaction_desc read_interaction;
    henka_material read_material;
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
    HENKA_TEST_ASSERT(henka_scene_translate_entity(scene, first, (henka_vec3){-2.0f, 1.0f, 0.5f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, first, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.position.x, 3.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.position.y, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_rotate_entity(scene, first, henka_quat_from_axis_angle((henka_vec3){0.0f, 1.0f, 0.0f}, 90.0f * HENKA_DEG_TO_RAD)) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_scale_entity(scene, first, (henka_vec3){2.0f, -1.0f, 0.5f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, first, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.scale.x, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.scale.y, -1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.scale.z, 0.5f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_scale_entity(scene, first, (henka_vec3){1.0f, 0.0f, 1.0f}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, first, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.scale.y, -1.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, first, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_visible(scene, first));
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, first, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_is_entity_visible(scene, first));
    HENKA_TEST_ASSERT(henka_scene_set_entity_name(scene, second, "Marker") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, second), "Marker") == 0);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(scene, "Marker", &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    HENKA_TEST_ASSERT(henka_scene_set_entity_tag(scene, second, "marker") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_tag(scene, second), "marker") == 0);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_tag(scene, "marker", &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    helper = henka_scene_create_entity_named(scene, "Transform Gizmo");
    HENKA_TEST_ASSERT(helper != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(scene, helper, HENKA_SCENE_ENTITY_FLAG_HELPER) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_is_entity_helper(scene, helper));
    HENKA_TEST_ASSERT(henka_scene_get_entity_flags(scene, helper, &flags) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT((flags & HENKA_SCENE_ENTITY_FLAG_HELPER) != 0U);
    bounds = (henka_bounds){{0.0f, 0.5f, 0.0f}, {0.5f, 0.5f, 0.5f}};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(scene, second, bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(scene, second, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.extents.x, 0.5f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(
        scene,
        second,
        (henka_bounds){{0.0f, 0.0f, 0.0f}, {-1.0f, 1.0f, 1.0f}}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(
        scene,
        second,
        (henka_bounds){{NAN, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}) == HENKA_ERROR_INVALID_ARGUMENT);
    bounds = (henka_bounds){{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(scene, helper, bounds) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){10.0f, 0.0f, 0.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, helper, transform) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){1.0f, 0.0f, 0.0f};
    transform.rotation = henka_quat_from_axis_angle((henka_vec3){0.0f, 1.0f, 0.0f}, 45.0f * HENKA_DEG_TO_RAD);
    transform.scale = (henka_vec3){-2.0f, 1.0f, 0.5f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, second, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, second, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.scale.x, -2.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_bounds(scene, second, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(bounds.extents.x > 0.5f);
    HENKA_TEST_ASSERT(bounds.extents.z > 0.25f);
    material = henka_material_default();
    material.name = material_name;
    material.shader = (henka_shader*)(uintptr_t)1U;
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(scene, second, material) == HENKA_SUCCESS);
    material_name[0] = 'X';
    HENKA_TEST_ASSERT(henka_scene_get_entity_material(scene, second, &read_material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(read_material.name, "Mutable Material") == 0);
    material.base_color.x = NAN;
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(scene, second, material) == HENKA_ERROR_INVALID_ARGUMENT);
    material.base_color.x = 1.0f;
    material.use_texture = true;
    material.base_color_texture = NULL;
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(scene, second, material) == HENKA_ERROR_INVALID_ARGUMENT);

    interaction = (henka_interaction_desc){true, 3.5f, interaction_prompt};
    HENKA_TEST_ASSERT(henka_scene_set_entity_interaction(scene, second, &interaction) == HENKA_SUCCESS);
    strcpy_s(interaction_prompt, sizeof(interaction_prompt), "Changed prompt");
    HENKA_TEST_ASSERT(henka_scene_get_entity_interaction(scene, second, &read_interaction) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_interaction.enabled);
    HENKA_TEST_ASSERT(strcmp(read_interaction.prompt, "Inspect sample") == 0);
    interaction.max_distance = -1.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_interaction(scene, second, &interaction) == HENKA_ERROR_INVALID_ARGUMENT);
    interaction.max_distance = NAN;
    HENKA_TEST_ASSERT(henka_scene_set_entity_interaction(scene, second, &interaction) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_can_interact(scene, second, (henka_vec3){NAN, 0.0f, 0.0f}) == HENKA_INTERACTION_RESULT_UNAVAILABLE);
    HENKA_TEST_ASSERT(henka_scene_can_interact(scene, second, (henka_vec3){0.0f, 0.0f, 0.0f}) == HENKA_INTERACTION_RESULT_AVAILABLE);
    HENKA_TEST_ASSERT(henka_scene_get_entity_info(scene, second, &info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(info.entity == second);
    HENKA_TEST_ASSERT(strcmp(info.tag, "marker") == 0);
    ray.origin = (henka_vec3){1.0f, 0.5f, 3.0f};
    ray.direction = henka_vec3_normalize((henka_vec3){0.0f, 0.0f, -1.0f});
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    HENKA_TEST_ASSERT(henka_scene_translate_entity(scene, second, (henka_vec3){2.0f, 0.0f, -1.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_bounds(scene, second, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.x, 3.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.z, -1.0f, 0.0001f);
    ray.origin = (henka_vec3){3.0f, 0.5f, 2.0f};
    ray.direction = henka_vec3_normalize((henka_vec3){0.0f, 0.0f, -1.0f});
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    HENKA_TEST_ASSERT(henka_scene_scale_entity(scene, second, (henka_vec3){1.5f, 2.0f, 1.0f}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_bounds(scene, second, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(bounds.extents.x > 0.75f);
    HENKA_TEST_ASSERT(bounds.extents.y >= 1.0f);
    ray.origin = (henka_vec3){10.0f, 0.0f, 3.0f};
    ray.direction = henka_vec3_normalize((henka_vec3){0.0f, 0.0f, -1.0f});
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, second, false) == HENKA_SUCCESS);
    ray.origin = (henka_vec3){3.0f, 0.5f, 2.0f};
    ray.direction = henka_vec3_normalize((henka_vec3){0.0f, 0.0f, -1.0f});
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, second, true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_name(scene, second, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_name(scene, second) == NULL);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, HENKA_INVALID_ENTITY, &read_back) == HENKA_ERROR_INVALID_ARGUMENT);
    transform = henka_transform_identity();
    transform.position.x = NAN;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, second, transform) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_translate_entity(scene, HENKA_INVALID_ENTITY, (henka_vec3){1.0f, 0.0f, 0.0f}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_rotate_entity(scene, HENKA_INVALID_ENTITY, henka_quat_identity()) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_scale_entity(scene, second, (henka_vec3){INFINITY, 1.0f, 1.0f}) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(scene, NULL, &found) == HENKA_ERROR_INVALID_ARGUMENT);
    memset(overlong_text, 'n', sizeof(overlong_text));
    overlong_text[sizeof(overlong_text) - 1U] = '\0';
    HENKA_TEST_ASSERT(henka_scene_set_entity_name(scene, second, overlong_text) == HENKA_ERROR_INVALID_ARGUMENT);

    henka_scene_destroy_entity(scene, first);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, first));
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 2U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 0U) == second);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 1U) == helper);

    henka_scene_destroy(scene);

    henka_test_scene_capacity_growth();
}
