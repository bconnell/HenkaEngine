#include <string.h>
#include <stdint.h>

#include "test_suite.h"

#include "../examples/sandbox3d/object_authoring_tools.h"

static void henka_test_sandbox3d_object_authoring_scene_policy(void)
{
    henka_scene* scene;
    henka_entity first;
    henka_entity second;
    henka_entity helper;
    henka_entity stale;
    henka_entity entities[4];
    size_t count;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    first = henka_scene_create_entity_named(scene, "First");
    second = henka_scene_create_entity_named(scene, "Second");
    helper = henka_scene_create_entity_named(scene, "Editor Helper");
    HENKA_TEST_ASSERT(first != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(second != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(helper != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(scene, helper, HENKA_SCENE_ENTITY_FLAG_HELPER) == HENKA_SUCCESS);

    count = sandbox3d_object_authoring_collect_user_entities(scene, entities, 1U);
    HENKA_TEST_ASSERT(count == 2U);
    HENKA_TEST_ASSERT(entities[0] == first);
    count = sandbox3d_object_authoring_collect_user_entities(scene, entities, 4U);
    HENKA_TEST_ASSERT(count == 2U);
    HENKA_TEST_ASSERT(entities[0] == first);
    HENKA_TEST_ASSERT(entities[1] == second);
    HENKA_TEST_ASSERT(sandbox3d_object_authoring_collect_user_entities(scene, NULL, 0U) == 2U);
    HENKA_TEST_ASSERT(!sandbox3d_object_authoring_can_edit_entity(scene, helper));

    henka_scene_destroy_entity(scene, second);
    stale = second;
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, stale));
    HENKA_TEST_ASSERT(!sandbox3d_object_authoring_can_edit_entity(scene, stale));
    HENKA_TEST_ASSERT(sandbox3d_object_authoring_collect_user_entities(scene, entities, 4U) == 1U);
    HENKA_TEST_ASSERT(entities[0] == first);

    henka_scene_destroy(scene);
}

static void henka_test_sandbox3d_object_authoring_duplicate(void)
{
    henka_scene* scene;
    henka_entity source;
    henka_entity duplicate;
    henka_entity helper;
    henka_transform source_transform;
    henka_transform duplicate_transform;
    henka_bounds bounds;
    henka_interaction_desc interaction;
    henka_material material;
    henka_result result;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    source = henka_scene_create_entity_named(scene, "Source");
    helper = henka_scene_create_entity_named(scene, "Helper");
    HENKA_TEST_ASSERT(source != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(helper != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_tag(scene, source, "authoring") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, source, false) == HENKA_SUCCESS);
    source_transform = henka_transform_identity();
    source_transform.position = (henka_vec3){2.0f, 3.0f, 4.0f};
    source_transform.scale = (henka_vec3){2.0f, 1.5f, 0.5f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, source, source_transform) == HENKA_SUCCESS);
    bounds = (henka_bounds){{1.0f, 2.0f, 3.0f}, {0.5f, 0.75f, 1.0f}};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(scene, source, bounds) == HENKA_SUCCESS);
    interaction = (henka_interaction_desc){true, 7.0f, "Use"};
    HENKA_TEST_ASSERT(henka_scene_set_entity_interaction(scene, source, &interaction) == HENKA_SUCCESS);
    material = henka_material_default();
    material.name = "Source Material";
    material.shader = (henka_shader*)(uintptr_t)1U;
    material.metallic = 0.8f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(scene, source, material) == HENKA_SUCCESS);

    result = sandbox3d_object_authoring_duplicate_entity(scene, source, "Duplicate", &duplicate);
    HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(duplicate != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, duplicate), "Duplicate") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_tag(scene, duplicate), "authoring") == 0);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_visible(scene, duplicate));
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, duplicate, &duplicate_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(duplicate_transform.position.x, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(duplicate_transform.scale.z, 0.5f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(scene, duplicate, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.extents.y, 0.75f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_interaction(scene, duplicate, &interaction) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(interaction.enabled);
    HENKA_TEST_ASSERT(strcmp(interaction.prompt, "Use") == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material(scene, duplicate, &material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(material.metallic, 0.8f, 0.0001f);

    duplicate_transform.position.x = 99.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, duplicate, duplicate_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, source, &source_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(source_transform.position.x, 2.0f, 0.0001f);

    HENKA_TEST_ASSERT(sandbox3d_object_authoring_duplicate_entity(scene, helper, "Invalid", &duplicate) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(duplicate == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(sandbox3d_object_authoring_duplicate_entity(scene, source, NULL, &duplicate) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(duplicate == HENKA_INVALID_ENTITY);

    henka_scene_destroy(scene);
}

void henka_test_sandbox3d_object_authoring(void)
{
    henka_test_sandbox3d_object_authoring_scene_policy();
    henka_test_sandbox3d_object_authoring_duplicate();
}
