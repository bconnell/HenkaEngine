#include "test_suite.h"

#include <float.h>
#include <stdint.h>
#include <string.h>

#include <henka/core.h>
#include <henka/prefab.h>
#include <henka/scene.h>

#include "../engine/src/core/checked.h"
#include "../engine/src/henka_internal.h"

static void henka_test_scene_capacity_growth(void)
{
    enum
    {
        ENTITY_COUNT = 40
    };
    henka_entity entities[ENTITY_COUNT];
    henka_entity replacement;
    henka_scene* scene;
    henka_transform replacement_transform;
    int index;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_render_revision(scene) != 0U);
    for (index = 0; index < ENTITY_COUNT; ++index)
    {
        entities[index] = henka_scene_create_entity(scene);
        HENKA_TEST_ASSERT(entities[index] != HENKA_INVALID_ENTITY);
    }

    {
        const uint64_t initial_revision = henka_scene_get_render_revision(scene);
        HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
            scene,
            entities[0],
            henka_transform_identity()) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_scene_get_render_revision(scene) > initial_revision);
    }

    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == (size_t)ENTITY_COUNT);
    henka_scene_destroy_entity(scene, entities[7]);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == (size_t)ENTITY_COUNT - 1U);

    replacement = henka_scene_create_entity_named(scene, "Replacement");
    HENKA_TEST_ASSERT(replacement != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(replacement != entities[7]);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, entities[7]));
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(scene, replacement));
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == (size_t)ENTITY_COUNT);
    HENKA_TEST_ASSERT(
        henka_scene_get_entity_at_storage_index(scene, 7U) == replacement);
    HENKA_TEST_ASSERT(
        henka_scene_get_entity_at_storage_index(scene, 0U) == entities[0]);
    HENKA_TEST_ASSERT(
        henka_scene_get_entity_at_storage_index(scene, ENTITY_COUNT) == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, replacement), "Replacement") == 0);

    replacement_transform = henka_transform_identity();
    replacement_transform.position.x = 8.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
        scene,
        replacement,
        replacement_transform) == HENKA_SUCCESS);
    replacement_transform.position.x = -12.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
        scene,
        entities[7],
        replacement_transform) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(
        scene,
        replacement,
        &replacement_transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        replacement_transform.position.x,
        8.0f,
        0.0001f);

    henka_scene_destroy(scene);
}

static void henka_test_scene_revision_exhaustion(void)
{
    henka_scene* scene;
    henka_entity entity;
    henka_entity rejected_entity;
    henka_camera camera;
    henka_material material;
    henka_transform before;
    henka_transform attempted;
    const float previous_light_intensity = 3.0f;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    entity = henka_scene_create_entity_named(scene, "Revision Exhaustion");
    HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(
        scene, entity, &before) == HENKA_SUCCESS);

    scene->render_revision = UINT64_MAX;
    scene->content_revision = UINT64_MAX;
    attempted = before;
    attempted.position.x = 4.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
        scene, entity, attempted) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(
        scene, entity, &attempted) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        attempted.position.x, before.position.x, 0.0001f);
    HENKA_TEST_ASSERT(scene->render_revision == UINT64_MAX);
    HENKA_TEST_ASSERT(scene->content_revision == UINT64_MAX);

    material = henka_material_default();
    material.shader = (henka_shader*)(uintptr_t)1U;
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(
        scene, entity, material) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(henka_scene_set_entity_interaction(
        scene,
        entity,
        &(henka_interaction_desc){true, 5.0f, "Interact"}) == HENKA_ERROR_LIMIT);

    rejected_entity = henka_scene_create_entity_named(scene, "Rejected");
    HENKA_TEST_ASSERT(rejected_entity == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 1U);
    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD, 1.0f, 0.1f, 100.0f);
    HENKA_TEST_ASSERT(henka_scene_set_camera(scene, &camera) == HENKA_ERROR_LIMIT);
    HENKA_TEST_ASSERT(!scene->has_camera);
    henka_scene_set_light_intensity(scene, 7.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        scene->light_intensity, previous_light_intensity, 0.0001f);

    henka_scene_destroy(scene);
}

static void henka_test_scene_hierarchy(void)
{
    henka_scene* scene;
    henka_scene* clone;
    henka_entity root;
    henka_entity child;
    henka_entity grandchild;
    henka_entity alternate_root;
    henka_entity nonuniform_root;
    henka_entity parent;
    henka_transform transform;
    henka_transform world_before;
    henka_transform child_world_before_destroy;
    henka_transform local;
    henka_transform expected_world;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    root = henka_scene_create_entity_named(scene, "Hierarchy Root");
    child = henka_scene_create_entity_named(scene, "Hierarchy Child");
    grandchild = henka_scene_create_entity_named(scene, "Hierarchy Grandchild");
    alternate_root = henka_scene_create_entity_named(scene, "Alternate Root");
    nonuniform_root = henka_scene_create_entity_named(scene, "Nonuniform Root");
    HENKA_TEST_ASSERT(root != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(child != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(grandchild != HENKA_INVALID_ENTITY);

    transform = henka_transform_identity();
    transform.position = (henka_vec3){10.0f, 2.0f, -3.0f};
    transform.scale = (henka_vec3){2.0f, 2.0f, 2.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, root, transform) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){1.0f, 2.0f, 3.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_transform(scene, child, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, child, root, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(scene, child, &parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(parent == root);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(scene, child, &world_before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(world_before.position.x, 12.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(world_before.position.y, 6.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(world_before.position.z, 3.0f, 0.0001f);

    transform = henka_transform_identity();
    transform.position = (henka_vec3){1.0f, 2.0f, 3.0f};
    transform.rotation = henka_quat_from_axis_angle(
        (henka_vec3){0.0f, 1.0f, 0.0f}, 45.0f * HENKA_DEG_TO_RAD);
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_transform(scene, child, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(scene, child, &world_before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(world_before.rotation.y, transform.rotation.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(world_before.rotation.w, transform.rotation.w, 0.0001f);

    transform = henka_transform_identity();
    transform.position = (henka_vec3){10.0f, 2.0f, -3.0f};
    transform.rotation = henka_quat_from_axis_angle(
        (henka_vec3){0.0f, 1.0f, 0.0f}, 90.0f * HENKA_DEG_TO_RAD);
    transform.scale = (henka_vec3){2.0f, 2.0f, 2.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, root, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(scene, child, &world_before) == HENKA_SUCCESS);
    expected_world = henka_transform_identity();
    expected_world.position = henka_vec3_add(
        transform.position,
        henka_quat_rotate_vec3(
            transform.rotation,
            (henka_vec3){2.0f, 4.0f, 6.0f}));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(world_before.position.x, expected_world.position.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(world_before.position.y, expected_world.position.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(world_before.position.z, expected_world.position.z, 0.0001f);

    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, grandchild, child, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, root, grandchild, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(scene, root, &parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(parent == HENKA_INVALID_ENTITY);

    transform = henka_transform_identity();
    transform.position = (henka_vec3){-5.0f, 0.0f, 4.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, alternate_root, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, child, alternate_root, HENKA_SCENE_PARENT_KEEP_WORLD) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(scene, child, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, world_before.position.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.y, world_before.position.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.z, world_before.position.z, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_transform(scene, child, &local) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        local.position.x,
        world_before.position.x - (-5.0f),
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        local.position.y,
        world_before.position.y,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        local.position.z,
        world_before.position.z - 4.0f,
        0.0001f);

    transform = henka_transform_identity();
    transform.position = (henka_vec3){-7.0f, 1.0f, 4.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, child, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(scene, child, &world_before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(world_before.position.x, -7.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(world_before.position.y, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(world_before.position.z, 4.0f, 0.0001f);

    transform = henka_transform_identity();
    transform.scale = (henka_vec3){2.0f, 1.0f, 2.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, nonuniform_root, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, grandchild, nonuniform_root, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_ERROR_INVALID_ARGUMENT);

    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, child, HENKA_INVALID_ENTITY, HENKA_SCENE_PARENT_KEEP_WORLD) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(scene, child, &parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(parent == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(scene, child, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, -7.0f, 0.0001f);

    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, child, HENKA_INVALID_ENTITY, (henka_scene_parenting_mode)99) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, child, (henka_entity)0x1234U, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(scene, HENKA_INVALID_ENTITY, &parent) == HENKA_ERROR_INVALID_ARGUMENT);

    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, child, alternate_root, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(scene, child, &world_before) == HENKA_SUCCESS);
    clone = NULL;
    HENKA_TEST_ASSERT(henka_scene_clone(scene, &clone) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(clone, child, &parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(parent == alternate_root);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(clone, child, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, world_before.position.x, 0.0001f);
    henka_scene_destroy(clone);

    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(
        scene, child, &child_world_before_destroy) == HENKA_SUCCESS);
    henka_scene_destroy_entity(scene, alternate_root);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(scene, child, &parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(parent == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(scene, child, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, child_world_before_destroy.position.x, 0.0001f);
    henka_scene_destroy(scene);
}

static void henka_test_prefab_snapshot_and_transaction(void)
{
    henka_scene* source;
    henka_scene* target;
    henka_prefab* prefab;
    henka_entity source_root;
    henka_entity source_child;
    henka_entity source_visual;
    henka_entity source_external_owner;
    henka_entity source_external_visual;
    henka_entity instance_root;
    henka_entity instance_child;
    henka_entity instance_visual;
    henka_entity instance_external_visual;
    henka_entity holder;
    henka_entity stale_holder;
    henka_entity nested_root;
    henka_entity nested_child;
    henka_entity parent;
    henka_entity selection_owner;
    size_t target_count;
    henka_transform transform;
    henka_bounds bounds;
    henka_interaction_desc interaction;

    HENKA_TEST_ASSERT(henka_scene_create(&source) == HENKA_SUCCESS);
    source_child = henka_scene_create_entity_named(source, "Prefab Child");
    source_root = henka_scene_create_entity_named(source, "Prefab Root");
    source_visual = henka_scene_create_entity_named(source, "Prefab Visual");
    source_external_owner = henka_scene_create_entity_named(source, "External Owner");
    source_external_visual = henka_scene_create_entity_named(source, "Prefab External Visual");
    HENKA_TEST_ASSERT(source_root != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(source_child != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(source_visual != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(source_external_owner != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(source_external_visual != HENKA_INVALID_ENTITY);

    transform = henka_transform_identity();
    transform.position = (henka_vec3){10.0f, 2.0f, -4.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(source, source_root, transform) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){2.0f, 3.0f, 4.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_transform(source, source_child, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        source, source_child, source_root, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_tag(source, source_child, "prefab-part") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(source, source_child, false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(
        source, source_child, HENKA_SCENE_ENTITY_FLAG_HELPER) == HENKA_SUCCESS);
    bounds = (henka_bounds){{-1.0f, -2.0f, -3.0f}, {1.0f, 2.0f, 3.0f}};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(source, source_child, bounds) == HENKA_SUCCESS);
    interaction = (henka_interaction_desc){true, 6.0f, "Inspect prefab part"};
    HENKA_TEST_ASSERT(henka_scene_set_entity_interaction(source, source_child, &interaction) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){-3.0f, 1.0f, 2.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_transform(source, source_visual, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        source, source_visual, source_root, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_selection_owner(
        source, source_visual, source_root) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){4.0f, -1.0f, -2.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_transform(source, source_external_visual, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        source, source_external_visual, source_root, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_selection_owner(
        source, source_external_visual, source_external_owner) == HENKA_SUCCESS);

    prefab = NULL;
    HENKA_TEST_ASSERT(henka_prefab_create_from_scene(source, source_root, &prefab) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_prefab_get_entity_count(prefab) == 4U);
    henka_scene_destroy(source);

    HENKA_TEST_ASSERT(henka_scene_create(&target) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){30.0f, 5.0f, 7.0f};
    transform.scale.x = 0.0f;
    HENKA_TEST_ASSERT(henka_prefab_instantiate(prefab, target, transform, &instance_root) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(target) == 0U);

    transform = henka_transform_identity();
    transform.position = (henka_vec3){30.0f, 5.0f, 7.0f};
    HENKA_TEST_ASSERT(henka_prefab_instantiate(prefab, target, transform, &instance_root) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(instance_root != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(target) == 4U);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(
        target, "Prefab Child", &instance_child) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(target, instance_root), "Prefab Root") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(target, instance_child), "Prefab Child") == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(target, instance_child, &parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(parent == instance_root);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(target, instance_child, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, 32.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.y, 8.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.z, 11.0f, 0.0001f);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_visible(target, instance_child));
    HENKA_TEST_ASSERT(henka_scene_is_entity_helper(target, instance_child));
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(target, instance_child, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.extents.x, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_interaction(target, instance_child, &interaction) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(interaction.enabled && strcmp(interaction.prompt, "Inspect prefab part") == 0);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(
        target, "Prefab Visual", &instance_visual) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(target, instance_visual, &parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(parent == instance_root);
    HENKA_TEST_ASSERT(henka_scene_get_entity_selection_owner(
        target, instance_visual, &selection_owner) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(selection_owner == instance_root);
    HENKA_TEST_ASSERT(henka_scene_find_entity_by_name(
        target, "Prefab External Visual", &instance_external_visual) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(
        target, instance_external_visual, &parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(parent == instance_root);
    HENKA_TEST_ASSERT(henka_scene_get_entity_selection_owner(
        target, instance_external_visual, &selection_owner) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(selection_owner == instance_external_visual);

    holder = henka_scene_create_entity_named(target, "Prefab Holder");
    HENKA_TEST_ASSERT(holder != HENKA_INVALID_ENTITY);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){100.0f, 10.0f, -20.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(target, holder, transform) == HENKA_SUCCESS);
    stale_holder = holder;
    henka_scene_destroy_entity(target, stale_holder);
    target_count = henka_scene_get_entity_count(target);
    nested_root = HENKA_INVALID_ENTITY;
    HENKA_TEST_ASSERT(henka_prefab_instantiate_under_parent(
        prefab,
        target,
        stale_holder,
        henka_transform_identity(),
        &nested_root) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(nested_root == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(target) == target_count);

    holder = henka_scene_create_entity_named(target, "Prefab Holder Reused");
    HENKA_TEST_ASSERT(holder != HENKA_INVALID_ENTITY);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){100.0f, 10.0f, -20.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(target, holder, transform) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){1.0f, 2.0f, 3.0f};
    HENKA_TEST_ASSERT(henka_prefab_instantiate_under_parent(
        prefab, target, holder, transform, &nested_root) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(nested_root != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(target, nested_root, &parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(parent == holder);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(target, nested_root, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, 101.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.y, 12.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.z, -17.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_count(target, nested_root, &target_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(target_count == 3U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_at_index(
        target, nested_root, 0U, &nested_child) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(target, nested_child, &parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(parent == nested_root);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(target, nested_child, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, 103.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.y, 15.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.z, -13.0f, 0.0001f);

    henka_prefab_destroy(prefab);
    henka_scene_destroy(target);
}

static void henka_test_scene_child_enumeration(void)
{
    henka_scene* scene;
    henka_entity root;
    henka_entity first_child;
    henka_entity stale_child;
    henka_entity third_child;
    henka_entity replacement_child;
    henka_entity listed;
    size_t count;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    root = henka_scene_create_entity_named(scene, "Enumeration Root");
    first_child = henka_scene_create_entity_named(scene, "Enumeration First");
    stale_child = henka_scene_create_entity_named(scene, "Enumeration Stale");
    third_child = henka_scene_create_entity_named(scene, "Enumeration Third");
    replacement_child = henka_scene_create_entity_named(scene, "Enumeration Replacement");
    HENKA_TEST_ASSERT(root != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(first_child != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(stale_child != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(third_child != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(replacement_child != HENKA_INVALID_ENTITY);

    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, first_child, root, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, stale_child, root, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, third_child, root, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, replacement_child, root, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_scene_get_entity_child_count(scene, root, &count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(count == 4U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_at_index(
        scene, root, 0U, &listed) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(listed == first_child);
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_at_index(
        scene, root, 3U, &listed) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(listed == replacement_child);
    listed = HENKA_INVALID_ENTITY;
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_at_index(
        scene, root, 4U, &listed) == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(listed == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_count(
        scene, HENKA_INVALID_ENTITY, &count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(count == 1U);

    henka_scene_destroy_entity(scene, stale_child);
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_count(
        scene, stale_child, &count) == HENKA_ERROR_INVALID_ARGUMENT);
    replacement_child = henka_scene_create_entity_named(scene, "Enumeration Reused");
    HENKA_TEST_ASSERT(replacement_child != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, replacement_child, root, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_count(scene, root, &count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(count == 4U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_at_index(
        scene, root, 1U, &listed) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(listed == replacement_child);

    count = 42U;
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_count(
        scene, (henka_entity)0x1234U, &count) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(count == 0U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_count(scene, root, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_at_index(
        scene, (henka_entity)0x1234U, 0U, &listed) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(listed == HENKA_INVALID_ENTITY);

    henka_scene_destroy(scene);
}

void henka_test_scene(void)
{
    henka_bounds bounds;
    henka_camera camera;
    uint32_t flags;
    henka_scene* scene;
    henka_scene* cloned_scene;
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
    henka_entity selection_root;
    henka_entity selection_child;
    henka_interaction_desc interaction;
    henka_interaction_desc read_interaction;
    henka_material read_material;
    const henka_material_asset* material_asset;
    const henka_material_asset* read_material_asset;
    henka_transform transform;
    henka_transform read_back;
    henka_scene_environment_desc environment;
    henka_scene_environment_desc read_environment;
    henka_scene_environment_desc invalid_environment;
    henka_scene_fog_desc fog;
    henka_scene_fog_desc read_fog;
    henka_mesh* read_mesh;
    henka_scene_light_desc light;
    henka_scene_light_desc read_light;
    henka_scene_lod_desc lod;
    henka_scene_lod_desc read_lod;
    henka_scene_reflection_probe_desc reflection_probe;
    henka_scene_reflection_probe_desc read_reflection_probe;
    uint32_t reflection_probe_indices[HENKA_SCENE_MAX_REFLECTION_PROBES];
    uint32_t reflection_probe_index;
    uint32_t light_indices[HENKA_SCENE_MAX_LOCAL_LIGHTS];
    uint32_t light_index;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(scene != NULL);
    HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_environment.intensity, 1.5f, 0.0001f);
    HENKA_TEST_ASSERT(read_environment.moon.enabled);
    HENKA_TEST_ASSERT(read_environment.stars.enabled);
    environment = (henka_scene_environment_desc){
        (henka_vec3){0.02f, 0.03f, 0.05f},
        (henka_vec3){0.14f, 0.18f, 0.24f},
        (henka_vec3){0.06f, 0.09f, 0.16f},
        2.0f};
    HENKA_TEST_ASSERT(henka_scene_set_environment(scene, environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_environment.horizon_color.y, 0.18f, 0.0001f);
    environment.intensity = 17.0f;
    HENKA_TEST_ASSERT(henka_scene_set_environment(scene, environment) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_environment.intensity, 2.0f, 0.0001f);
    environment = henka_scene_environment_default();
    environment.mode = HENKA_SCENE_ENVIRONMENT_PROCEDURAL;
    environment.sun.manual_direction = false;
    environment.time_of_day_enabled = true;
    environment.time_of_day_hours = 6.0f;
    HENKA_TEST_ASSERT(henka_scene_set_environment(scene, environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_advance_environment_time(scene, 60.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_environment.time_of_day_hours, 8.4f, 0.0001f);
    {
        henka_scene_environment_desc orbital_environment = henka_scene_environment_default();
        orbital_environment.mode = HENKA_SCENE_ENVIRONMENT_PROCEDURAL;
        orbital_environment.sun.manual_direction = true;
        orbital_environment.sun.direction = henka_vec3_normalize((henka_vec3){0.25f, -0.8f, 0.5f});
        orbital_environment.moon.enabled = true;
        orbital_environment.moon.manual_direction = false;
        HENKA_TEST_ASSERT(henka_scene_set_environment(scene, orbital_environment) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            henka_vec3_dot(read_environment.sun.direction, read_environment.moon.direction),
            -1.0f,
            0.0001f);
    }
    environment.time_of_day_enabled = true;
    environment.time_of_day_hours = 8.4f;
    HENKA_TEST_ASSERT(henka_scene_set_environment(scene, environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
    invalid_environment = read_environment;
    invalid_environment.atmosphere.rayleigh_scattering = -1.0f;
    HENKA_TEST_ASSERT(henka_scene_set_environment(scene, invalid_environment) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_environment.time_of_day_hours, 8.4f, 0.0001f);
    invalid_environment = read_environment;
    invalid_environment.moon.intensity = -1.0f;
    HENKA_TEST_ASSERT(henka_scene_set_environment(scene, invalid_environment) == HENKA_ERROR_INVALID_ARGUMENT);
    {
        henka_scene_environment_preset preset;
        const char* preset_label;
        for (preset = HENKA_SCENE_ENVIRONMENT_PRESET_CLEAR_MIDDAY;
             preset < HENKA_SCENE_ENVIRONMENT_PRESET_COUNT;
             ++preset)
        {
            preset_label = henka_scene_environment_preset_get_label(preset);
            HENKA_TEST_ASSERT(preset_label != NULL);
            HENKA_TEST_ASSERT(strcmp(preset_label, "Unknown") != 0);
            HENKA_TEST_ASSERT(henka_scene_set_environment_preset(scene, preset) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(read_environment.mode == HENKA_SCENE_ENVIRONMENT_PROCEDURAL);
            HENKA_TEST_ASSERT(!read_environment.time_of_day_enabled);
        }
        invalid_environment = read_environment;
        HENKA_TEST_ASSERT(henka_scene_set_environment_preset(
            scene,
            (henka_scene_environment_preset)HENKA_SCENE_ENVIRONMENT_PRESET_COUNT) == HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT(henka_scene_get_environment(scene, &read_environment) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(read_environment.intensity, invalid_environment.intensity, 0.0001f);
    }
    light = (henka_scene_light_desc){
        HENKA_SCENE_LIGHT_POINT,
        (henka_vec3){1.0f, 2.0f, 3.0f},
        (henka_vec3){0.0f, -1.0f, 0.0f},
        (henka_vec3){1.0f, 0.8f, 0.6f},
        20.0f,
        12.0f,
        1.0f,
        0.5f,
        true};
    HENKA_TEST_ASSERT(henka_scene_add_light(scene, light, &light_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(light_index < HENKA_SCENE_MAX_LOCAL_LIGHTS);
    HENKA_TEST_ASSERT(henka_scene_get_light(scene, light_index, &read_light) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_light.position.x, 1.0f, 0.0001f);
    light.type = HENKA_SCENE_LIGHT_SPOT;
    light.inner_cone_cosine = 0.8f;
    light.outer_cone_cosine = 0.9f;
    HENKA_TEST_ASSERT(henka_scene_update_light(scene, light_index, light) == HENKA_ERROR_INVALID_ARGUMENT);
    light.outer_cone_cosine = 0.5f;
    HENKA_TEST_ASSERT(henka_scene_update_light(scene, light_index, light) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_remove_light(scene, light_index) == HENKA_SUCCESS);
    light.enabled = true;
    light.type = HENKA_SCENE_LIGHT_POINT;
    for (light_index = 0U; light_index < HENKA_SCENE_MAX_LOCAL_LIGHTS; ++light_index)
    {
        HENKA_TEST_ASSERT(henka_scene_add_light(scene, light, &light_indices[light_index]) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_scene_add_light(scene, light, &light_index) == HENKA_ERROR_LIMIT);
    for (light_index = 0U; light_index < HENKA_SCENE_MAX_LOCAL_LIGHTS; ++light_index)
    {
        HENKA_TEST_ASSERT(henka_scene_remove_light(scene, light_indices[light_index]) == HENKA_SUCCESS);
    }
    reflection_probe = (henka_scene_reflection_probe_desc){
        (henka_vec3){0.0f, 1.0f, -2.0f},
        (henka_vec3){4.0f, 2.0f, 5.0f},
        1.0f,
        true,
        true};
    HENKA_TEST_ASSERT(henka_scene_add_reflection_probe(scene, reflection_probe, &reflection_probe_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(reflection_probe_index == 0U);
    HENKA_TEST_ASSERT(henka_scene_get_reflection_probe(scene, reflection_probe_index, &read_reflection_probe) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_reflection_probe.extents.z, 5.0f, 0.0001f);
    reflection_probe.position.x = 2.0f;
    HENKA_TEST_ASSERT(henka_scene_update_reflection_probe(scene, reflection_probe_index, reflection_probe) == HENKA_SUCCESS);
    reflection_probe.extents.x = 0.0f;
    HENKA_TEST_ASSERT(henka_scene_update_reflection_probe(scene, reflection_probe_index, reflection_probe) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_remove_reflection_probe(scene, reflection_probe_index) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_reflection_probe(scene, reflection_probe_index, &read_reflection_probe) == HENKA_ERROR_INVALID_ARGUMENT);
    reflection_probe.extents.x = 4.0f;
    for (reflection_probe_index = 0U; reflection_probe_index < HENKA_SCENE_MAX_REFLECTION_PROBES; ++reflection_probe_index)
    {
        HENKA_TEST_ASSERT(henka_scene_add_reflection_probe(scene, reflection_probe, &reflection_probe_indices[reflection_probe_index]) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_scene_add_reflection_probe(scene, reflection_probe, &reflection_probe_index) == HENKA_ERROR_LIMIT);
    for (reflection_probe_index = 0U; reflection_probe_index < HENKA_SCENE_MAX_REFLECTION_PROBES; ++reflection_probe_index)
    {
        HENKA_TEST_ASSERT(henka_scene_remove_reflection_probe(scene, reflection_probe_indices[reflection_probe_index]) == HENKA_SUCCESS);
    }
    HENKA_TEST_ASSERT(henka_scene_get_fog(scene, &read_fog) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!read_fog.enabled);
    fog = (henka_scene_fog_desc){
        true,
        HENKA_SCENE_FOG_EXPONENTIAL,
        (henka_vec3){0.18f, 0.21f, 0.26f},
        6.0f,
        64.0f,
        0.02f};
    HENKA_TEST_ASSERT(henka_scene_set_fog(scene, fog) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_fog(scene, &read_fog) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_fog.enabled);
    HENKA_TEST_ASSERT(read_fog.mode == HENKA_SCENE_FOG_EXPONENTIAL);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_fog.end_distance, 64.0f, 0.0001f);
    fog.end_distance = 5.0f;
    HENKA_TEST_ASSERT(henka_scene_set_fog(scene, fog) == HENKA_ERROR_INVALID_ARGUMENT);
    fog.end_distance = 64.0f;
    fog.mode = (henka_scene_fog_mode)99;
    HENKA_TEST_ASSERT(henka_scene_set_fog(scene, fog) == HENKA_ERROR_INVALID_ARGUMENT);
    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD,
        16.0f / 9.0f,
        0.1f,
        100.0f);
    HENKA_TEST_ASSERT(henka_scene_set_camera(scene, &camera) == HENKA_SUCCESS);
    camera.aspect_ratio = 0.0f;
    HENKA_TEST_ASSERT(henka_scene_set_camera(scene, &camera) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 0U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 0U) == HENKA_INVALID_ENTITY);

    first = henka_scene_create_entity_named(scene, "Ground");
    second = henka_scene_create_entity(scene);
    HENKA_TEST_ASSERT(first != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(second != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_selection_owner(scene, first, &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == first);
    HENKA_TEST_ASSERT(henka_scene_set_entity_selection_owner(scene, second, first) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_selection_owner(scene, second, &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == first);
    HENKA_TEST_ASSERT(henka_scene_set_entity_selection_owner(scene, first, second) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_set_entity_selection_owner(scene, second, HENKA_INVALID_ENTITY) == HENKA_ERROR_INVALID_ARGUMENT);
    selection_root = henka_scene_create_entity_named(scene, "Selection Root");
    selection_child = henka_scene_create_entity_named(scene, "Selection Child");
    HENKA_TEST_ASSERT(selection_root != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(selection_child != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_selection_owner(
        scene, selection_child, selection_root) == HENKA_SUCCESS);
    henka_scene_destroy_entity(scene, selection_root);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, selection_root));
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(scene, selection_child));
    HENKA_TEST_ASSERT(henka_scene_get_entity_selection_owner(
        scene, selection_child, &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == selection_child);
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(scene, first));
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(scene, second));
    HENKA_TEST_ASSERT(henka_scene_clear_entity_mesh(scene, first) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_mesh(scene, first, &read_mesh) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_mesh == NULL);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 3U);
    lod = (henka_scene_lod_desc){0};
    HENKA_TEST_ASSERT(henka_scene_set_entity_lod(scene, first, lod) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_lod(scene, first, &read_lod) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_lod.level_count == 0U);
    lod.level_count = 1U;
    lod.max_distances[0] = 32.0f;
    HENKA_TEST_ASSERT(henka_scene_set_entity_lod(scene, first, lod) == HENKA_ERROR_INVALID_ARGUMENT);
    listed = henka_scene_get_entity_at_index(scene, 0U);
    HENKA_TEST_ASSERT(listed == first);
    listed = henka_scene_get_entity_at_index(scene, 1U);
    HENKA_TEST_ASSERT(listed == second);
    listed = henka_scene_get_entity_at_index(scene, 2U);
    HENKA_TEST_ASSERT(listed == selection_child);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 3U) == HENKA_INVALID_ENTITY);
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
    HENKA_TEST_ASSERT(henka_scene_set_entity_selection_owner(scene, second, helper) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(
        scene,
        first,
        HENKA_SCENE_ENTITY_FLAG_TRANSFORM_LOCKED) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_is_entity_transform_locked(scene, first));
    HENKA_TEST_ASSERT(henka_scene_get_entity_flags(scene, first, &flags) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT((flags & HENKA_SCENE_ENTITY_FLAG_TRANSFORM_LOCKED) != 0U);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_transform_locked(scene, second));
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
    transform.rotation = (henka_quat){FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, helper, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, helper, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.rotation.x, 0.5f, 0.0002f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(read_back.rotation.w, 0.5f, 0.0002f);
    transform = henka_transform_identity();
    transform.position.x = FLT_MAX;
    transform.scale = (henka_vec3){FLT_MAX, 1.0f, 1.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, helper, transform) == HENKA_ERROR_INVALID_ARGUMENT);
    bounds = (henka_bounds){{9.0f, 9.0f, 9.0f}, {8.0f, 8.0f, 8.0f}};
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_bounds(
        scene,
        HENKA_INVALID_ENTITY,
        &bounds) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.center.x, 0.0f, 0.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.extents.x, 0.0f, 0.0f);
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
    material_asset = (const henka_material_asset*)(uintptr_t)0x1234U;
    read_material_asset = NULL;
    HENKA_TEST_ASSERT(henka_scene_get_entity_material_asset(
        scene, second, &read_material_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_material_asset == NULL);
    HENKA_TEST_ASSERT(henka_scene_set_entity_material_asset(
        scene, second, material_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material_asset(
        scene, second, &read_material_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_material_asset == material_asset);
    HENKA_TEST_ASSERT(henka_scene_set_entity_material(scene, second, material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material_asset(
        scene, second, &read_material_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_material_asset == material_asset);
    HENKA_TEST_ASSERT(henka_scene_set_entity_material_asset(
        scene, second, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material_asset(
        scene, second, &read_material_asset) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_material_asset == NULL);
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
    ray.direction = (henka_vec3){0.0f, 0.0f, 0.0f};
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    ray.direction = (henka_vec3){NAN, 0.0f, -1.0f};
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    ray.origin = (henka_vec3){NAN, 0.5f, 3.0f};
    ray.direction = (henka_vec3){0.0f, 0.0f, -1.0f};
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_ERROR_INVALID_ARGUMENT);

    ray.origin = (henka_vec3){1.0f, 0.5f, 3.0f};
    ray.direction = henka_vec3_normalize((henka_vec3){0.0f, 0.0f, -1.0f});
    HENKA_TEST_ASSERT(henka_scene_pick_entity(scene, ray, &found, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    ray.direction = (henka_vec3){0.0f, 0.0f, -FLT_MAX};
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

    cloned_scene = NULL;
    HENKA_TEST_ASSERT(henka_scene_clone(scene, &cloned_scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(cloned_scene != NULL);
    HENKA_TEST_ASSERT(cloned_scene != scene);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(cloned_scene) ==
        henka_scene_get_entity_count(scene));
    HENKA_TEST_ASSERT(henka_scene_is_entity_valid(cloned_scene, second));
    HENKA_TEST_ASSERT(henka_scene_get_entity_info(cloned_scene, second, &info) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(info.entity == second);
    HENKA_TEST_ASSERT(info.tag != NULL && strcmp(info.tag, "marker") == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_material(cloned_scene, second, &read_material) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(read_material.name, material_name) == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_interaction(cloned_scene, second, &read_interaction) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_interaction.prompt != NULL && strcmp(read_interaction.prompt, "Inspect sample") == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_selection_owner(cloned_scene, second, &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == first);
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(
        cloned_scene,
        second,
        (henka_transform){
            (henka_vec3){42.0f, 0.0f, 0.0f},
            henka_quat_identity(),
            (henka_vec3){1.0f, 1.0f, 1.0f}}) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, second, &read_back) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(read_back.position.x != 42.0f);
    henka_scene_destroy(cloned_scene);

    henka_scene_destroy_entity(scene, first);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_valid(scene, first));
    HENKA_TEST_ASSERT(henka_scene_get_entity_selection_owner(scene, second, &found) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(found == second);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 3U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 0U) == second);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 1U) == helper);
    HENKA_TEST_ASSERT(henka_scene_get_entity_at_index(scene, 2U) == selection_child);

    henka_scene_destroy(scene);

    henka_test_scene_capacity_growth();
    henka_test_scene_revision_exhaustion();
    henka_test_scene_hierarchy();
    henka_test_scene_child_enumeration();
    henka_test_prefab_snapshot_and_transaction();
}
