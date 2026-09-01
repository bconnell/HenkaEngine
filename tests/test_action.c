#include "test_suite.h"

#include <string.h>

#include <henka/action.h>
#include <henka/core.h>

#include "../engine/src/core/checked.h"

static void henka_test_action_default_transform_growth(void)
{
    enum { ENTITY_COUNT = 20 };
    henka_action_context* actions;
    henka_entity entity;
    henka_scene* scene;
    int index;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_create(&actions) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_set_scene(actions, scene) == HENKA_SUCCESS);

    for (index = 0; index < ENTITY_COUNT; ++index)
    {
        entity = henka_scene_create_entity(scene);
        HENKA_TEST_ASSERT(entity != HENKA_INVALID_ENTITY);
        HENKA_TEST_ASSERT(
            henka_action_context_register_default_transform(actions, entity, henka_transform_identity()) ==
            HENKA_SUCCESS);
    }

    henka_action_context_destroy(actions);
    henka_scene_destroy(scene);
}

static void henka_test_action_parenting(void)
{
    henka_action_context* actions;
    henka_action_request request;
    henka_action_result result;
    henka_scene* scene;
    henka_entity root;
    henka_entity child;
    henka_entity helper;
    henka_entity stale_parent;
    henka_transform transform;
    henka_transform child_world;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_create(&actions) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_set_scene(actions, scene) == HENKA_SUCCESS);
    root = henka_scene_create_entity_named(scene, "Action Parent");
    child = henka_scene_create_entity_named(scene, "Action Child");
    helper = henka_scene_create_entity_named(scene, "Action Helper");
    HENKA_TEST_ASSERT(root != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(child != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(helper != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(
        scene, helper, HENKA_SCENE_ENTITY_FLAG_HELPER) == HENKA_SUCCESS);

    transform = henka_transform_identity();
    transform.position = (henka_vec3){10.0f, 2.0f, -3.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, root, transform) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){1.0f, 2.0f, 3.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_transform(scene, child, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(scene, child, &child_world) == HENKA_SUCCESS);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SET_PARENT;
    request.params.set_parent.entity = child;
    request.params.set_parent.parent = root;
    request.params.set_parent.mode = HENKA_SCENE_PARENT_KEEP_LOCAL;
    HENKA_TEST_ASSERT(henka_action_validate(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(result.has_before_parent);
    HENKA_TEST_ASSERT(result.before_parent == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(scene, child, &stale_parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(stale_parent == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(result.has_after_parent);
    HENKA_TEST_ASSERT(result.after_parent == root);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(scene, child, &stale_parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(stale_parent == root);
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_transform(scene, child, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.y, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.z, 3.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(scene, child, &child_world) == HENKA_SUCCESS);

    request.dry_run = true;
    request.params.set_parent.parent = HENKA_INVALID_ENTITY;
    request.params.set_parent.mode = HENKA_SCENE_PARENT_KEEP_WORLD;
    HENKA_TEST_ASSERT(henka_action_validate(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(result.dry_run);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(scene, child, &stale_parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(stale_parent == root);
    request.dry_run = false;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(scene, child, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, child_world.position.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.y, child_world.position.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.z, child_world.position.z, 0.0001f);

    request.params.set_parent.parent = root;
    request.params.set_parent.mode = (henka_scene_parenting_mode)99;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_INVALID_PARENTING);
    HENKA_TEST_ASSERT(strcmp(henka_action_status_to_string(result.status), "invalid parenting") == 0);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(scene, child, &stale_parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(stale_parent == HENKA_INVALID_ENTITY);

    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(
        scene, child, HENKA_SCENE_ENTITY_FLAG_TRANSFORM_LOCKED) == HENKA_SUCCESS);
    request.params.set_parent.mode = HENKA_SCENE_PARENT_KEEP_LOCAL;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_TRANSFORM_LOCKED);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(scene, child, &stale_parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(stale_parent == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(scene, child, HENKA_SCENE_ENTITY_FLAG_NONE) == HENKA_SUCCESS);

    request.params.set_parent.parent = helper;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_HELPER_ENTITY);

    request.params.set_parent.parent = child;
    request.params.set_parent.mode = HENKA_SCENE_PARENT_KEEP_LOCAL;
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, child, HENKA_INVALID_ENTITY, HENKA_SCENE_PARENT_KEEP_WORLD) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_INVALID_PARENTING);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(scene, child, &stale_parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(stale_parent == HENKA_INVALID_ENTITY);

    henka_scene_destroy_entity(scene, root);
    request.params.set_parent.parent = root;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_INVALID_ENTITY);

    henka_action_context_destroy(actions);
    henka_scene_destroy(scene);
}

static void henka_test_action_duplicate_subtree(void)
{
    henka_action_context* actions;
    henka_action_request request;
    henka_action_result result;
    henka_scene* scene;
    henka_entity source_root;
    henka_entity source_child;
    henka_entity duplicate_root;
    henka_entity duplicate_child;
    henka_entity parent;
    henka_transform transform;
    henka_bounds bounds;
    henka_interaction_desc interaction;
    size_t entity_count;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_create(&actions) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_set_scene(actions, scene) == HENKA_SUCCESS);
    source_root = henka_scene_create_entity_named(scene, "Duplicate Root");
    source_child = henka_scene_create_entity_named(scene, "Duplicate Child");
    HENKA_TEST_ASSERT(source_root != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(source_child != HENKA_INVALID_ENTITY);

    transform = henka_transform_identity();
    transform.position = (henka_vec3){12.0f, 4.0f, -6.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_transform(scene, source_root, transform) == HENKA_SUCCESS);
    transform = henka_transform_identity();
    transform.position = (henka_vec3){1.0f, 2.0f, 3.0f};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_transform(scene, source_child, transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_parent(
        scene, source_child, source_root, HENKA_SCENE_PARENT_KEEP_LOCAL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_tag(scene, source_child, "duplicate-child") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_scene_set_entity_visible(scene, source_child, false) == HENKA_SUCCESS);
    bounds = (henka_bounds){{-1.0f, -2.0f, -3.0f}, {1.0f, 2.0f, 3.0f}};
    HENKA_TEST_ASSERT(henka_scene_set_entity_local_bounds(scene, source_child, bounds) == HENKA_SUCCESS);
    interaction = (henka_interaction_desc){true, 4.0f, "Inspect duplicate"};
    HENKA_TEST_ASSERT(henka_scene_set_entity_interaction(scene, source_child, &interaction) == HENKA_SUCCESS);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_DUPLICATE_SUBTREE;
    request.params.duplicate_subtree.entity = source_root;
    request.dry_run = true;
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 2U);
    HENKA_TEST_ASSERT(henka_action_validate(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(result.dry_run);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 2U);

    request.dry_run = false;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    duplicate_root = result.affected_entity;
    HENKA_TEST_ASSERT(duplicate_root != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(duplicate_root != source_root);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 4U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_world_transform(scene, duplicate_root, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, 12.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.y, 4.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.z, -6.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_count(scene, duplicate_root, &entity_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(entity_count == 1U);
    HENKA_TEST_ASSERT(henka_scene_get_entity_child_at_index(
        scene, duplicate_root, 0U, &duplicate_child) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(duplicate_child != source_child);
    HENKA_TEST_ASSERT(henka_scene_get_entity_parent(scene, duplicate_child, &parent) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(parent == duplicate_root);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_name(scene, duplicate_child), "Duplicate Child") == 0);
    HENKA_TEST_ASSERT(strcmp(henka_scene_get_entity_tag(scene, duplicate_child), "duplicate-child") == 0);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_visible(scene, duplicate_child));
    HENKA_TEST_ASSERT(henka_scene_get_entity_local_bounds(scene, duplicate_child, &bounds) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(bounds.extents.y, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_interaction(scene, duplicate_child, &interaction) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(interaction.enabled && strcmp(interaction.prompt, "Inspect duplicate") == 0);

    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(
        scene, source_root, HENKA_SCENE_ENTITY_FLAG_HELPER) == HENKA_SUCCESS);
    request.params.duplicate_subtree.entity = source_root;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_HELPER_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 4U);

    henka_scene_destroy_entity(scene, source_root);
    request.params.duplicate_subtree.entity = source_root;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 3U);

    henka_action_context_destroy(actions);
    henka_scene_destroy(scene);
}

void henka_test_action(void)
{
    henka_test_action_default_transform_growth();
    henka_test_action_parenting();
    henka_test_action_duplicate_subtree();
    henka_action_context* actions;
    henka_action_object_details details[4];
    henka_action_request request;
    henka_action_result result;
    henka_action_scene_summary summary;
    char overlong_name[HENKA_MAX_SCENE_TEXT_BYTES + 2U];
    henka_camera camera;
    henka_scene* scene;
    henka_transform transform;
    henka_entity cube;
    henka_entity helper;
    henka_entity movable;
    size_t object_count;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_create(&actions) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_set_scene(actions, scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_get_selected_entity(actions) == HENKA_INVALID_ENTITY);
    camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, 16.0f / 9.0f, 0.1f, 100.0f);
    HENKA_TEST_ASSERT(henka_action_context_set_camera(actions, &camera) == HENKA_SUCCESS);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_ADD_PRIMITIVE_OBJECT;
    request.params.add_primitive.primitive = (henka_action_primitive)999;
    request.params.add_primitive.name = "Invalid Primitive";
    request.params.add_primitive.transform = henka_transform_identity();
    request.params.add_primitive.visible = true;
    HENKA_TEST_ASSERT(henka_action_validate(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_INVALID_COMMAND);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 0U);

    memset(overlong_name, 'N', sizeof(overlong_name));
    overlong_name[sizeof(overlong_name) - 1U] = '\0';
    request.params.add_primitive.primitive = HENKA_ACTION_PRIMITIVE_CUBE;
    request.params.add_primitive.name = overlong_name;
    HENKA_TEST_ASSERT(henka_action_validate(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_INVALID_NAME);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 0U);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_ADD_PRIMITIVE_OBJECT;
    request.params.add_primitive.primitive = HENKA_ACTION_PRIMITIVE_CUBE;
    request.params.add_primitive.name = "Action Cube";
    request.params.add_primitive.transform = henka_transform_identity();
    request.params.add_primitive.visible = true;
    HENKA_TEST_ASSERT(henka_action_validate(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 0U);
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    cube = result.affected_entity;
    HENKA_TEST_ASSERT(cube != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 1U);

    HENKA_TEST_ASSERT(henka_action_list_objects(actions, details, 4U, &object_count) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(object_count == 1U);
    HENKA_TEST_ASSERT(details[0].object.entity == cube);
    HENKA_TEST_ASSERT(strcmp(details[0].object.name, "Action Cube") == 0);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_ADD_PRIMITIVE_OBJECT;
    request.params.add_primitive.primitive = HENKA_ACTION_PRIMITIVE_CYLINDER;
    request.params.add_primitive.name = "Action Cylinder";
    request.params.add_primitive.transform = henka_transform_identity();
    request.params.add_primitive.visible = true;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(result.has_object_details);
    HENKA_TEST_ASSERT(strcmp(result.object_details.object.tag, "primitive_cylinder") == 0);
    HENKA_TEST_ASSERT(result.object_details.object.has_bounds);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(result.object_details.object.local_bounds.extents.x, 0.5f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(result.object_details.object.local_bounds.extents.y, 0.5f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(result.object_details.object.local_bounds.extents.z, 0.5f, 0.0001f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SELECT_OBJECT;
    request.params.entity.entity = cube;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_action_context_get_selected_entity(actions) == cube);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_CLEAR_SELECTION;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_action_context_get_selected_entity(actions) == HENKA_INVALID_ENTITY);
    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SELECT_OBJECT;
    request.params.entity.entity = cube;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_action_context_get_selected_entity(actions) == cube);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_MOVE_BY_DELTA;
    request.params.move_by_delta.entity = cube;
    request.params.move_by_delta.delta = (henka_vec3){2.0f, 0.0f, -1.0f};
    HENKA_TEST_ASSERT(henka_action_validate(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(result.has_after_transform);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(result.after_transform.position.x, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.z, -1.0f, 0.0001f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_MOVE_BY_DELTA;
    request.params.move_by_delta.entity = cube;
    request.params.move_by_delta.delta = (henka_vec3){0.0f, 0.25f, 0.0f};
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.y, 0.25f, 0.0001f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_ROTATE_BY_DELTA;
    request.params.rotate_by_delta.entity = cube;
    request.params.rotate_by_delta.delta_rotation = henka_quat_from_axis_angle((henka_vec3){0.0f, 1.0f, 0.0f}, 30.0f * HENKA_DEG_TO_RAD);
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(result.has_after_transform);
    HENKA_TEST_ASSERT(result.after_transform.rotation.w != 1.0f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_ROTATE_BY_DELTA;
    request.params.rotate_by_delta.entity = cube;
    request.params.rotate_by_delta.delta_rotation = henka_quat_from_axis_angle((henka_vec3){1.0f, 0.0f, 0.0f}, 15.0f * HENKA_DEG_TO_RAD);
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(result.after_transform.rotation.x != 0.0f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_ROTATE_BY_DELTA;
    request.params.rotate_by_delta.entity = cube;
    request.params.rotate_by_delta.delta_rotation = henka_quat_from_axis_angle((henka_vec3){0.0f, 0.0f, 1.0f}, -15.0f * HENKA_DEG_TO_RAD);
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(result.after_transform.rotation.z != 0.0f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER;
    request.params.scale_by_multiplier.entity = cube;
    request.params.scale_by_multiplier.scale_multiplier = (henka_vec3){-1.5f, 2.0f, 1.0f};
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.x, -1.5f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.y, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.z, 1.0f, 0.0001f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER;
    request.params.scale_by_multiplier.entity = cube;
    request.params.scale_by_multiplier.scale_multiplier = (henka_vec3){1.0f, 0.0f, 1.0f};
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_INVALID_TRANSFORM);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.x, -1.5f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.y, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.z, 1.0f, 0.0001f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER;
    request.params.scale_by_multiplier.entity = cube;
    request.params.scale_by_multiplier.scale_multiplier = (henka_vec3){1.1f, 1.1f, 1.1f};
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.x, -1.65f, 0.0001f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER;
    request.params.scale_by_multiplier.entity = cube;
    request.params.scale_by_multiplier.scale_multiplier = (henka_vec3){0.9f, 0.9f, 0.9f};
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.x, -1.485f, 0.0001f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_GET_OBJECT_DETAILS;
    request.params.entity.entity = cube;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(result.has_object_details);
    HENKA_TEST_ASSERT(result.object_details.selected);
    HENKA_TEST_ASSERT(result.object_details.has_default_transform);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_HIDE_OBJECT;
    request.params.entity.entity = cube;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(!henka_scene_is_entity_visible(scene, cube));
    HENKA_TEST_ASSERT(henka_action_context_get_selected_entity(actions) == HENKA_INVALID_ENTITY);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_MOVE_BY_DELTA;
    request.params.move_by_delta.entity = cube;
    request.params.move_by_delta.delta = (henka_vec3){1.0f, 0.0f, 0.0f};
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_TARGET_HIDDEN);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SHOW_OBJECT;
    request.params.entity.entity = cube;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_RESET_TRANSFORM;
    request.params.entity.entity = cube;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.x, 1.0f, 0.0001f);

    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(
        scene,
        cube,
        HENKA_SCENE_ENTITY_FLAG_TRANSFORM_LOCKED) == HENKA_SUCCESS);
    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_MOVE_BY_DELTA;
    request.params.move_by_delta.entity = cube;
    request.params.move_by_delta.delta = (henka_vec3){1.0f, 0.0f, 0.0f};
    HENKA_TEST_ASSERT(henka_action_validate(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_TRANSFORM_LOCKED);
    HENKA_TEST_ASSERT(strcmp(henka_action_status_to_string(result.status), "transform locked") == 0);
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, 0.0f, 0.0001f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_ADD_PRIMITIVE_OBJECT;
    request.params.add_primitive.primitive = HENKA_ACTION_PRIMITIVE_CUBE;
    request.params.add_primitive.name = "Movable Box";
    request.params.add_primitive.transform = henka_transform_identity();
    request.params.add_primitive.visible = true;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    movable = result.affected_entity;
    HENKA_TEST_ASSERT(movable != HENKA_INVALID_ENTITY);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SELECT_OBJECT;
    request.params.entity.entity = movable;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_MOVE_BY_DELTA;
    request.params.move_by_delta.entity = movable;
    request.params.move_by_delta.delta = (henka_vec3){0.5f, 0.0f, 0.0f};
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_CLEAR_SELECTION;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_action_context_get_selected_entity(actions) == HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_is_entity_transform_locked(scene, cube));

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_MOVE_BY_DELTA;
    request.params.move_by_delta.entity = cube;
    request.params.move_by_delta.delta = (henka_vec3){1.0f, 0.0f, 0.0f};
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_TRANSFORM_LOCKED);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.position.x, 0.0f, 0.0001f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_RESET_TRANSFORM;
    request.params.entity.entity = cube;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(
        scene,
        cube,
        HENKA_SCENE_ENTITY_FLAG_NONE) == HENKA_SUCCESS);

    helper = henka_scene_create_entity_named(scene, "Helper");
    HENKA_TEST_ASSERT(helper != HENKA_INVALID_ENTITY);
    HENKA_TEST_ASSERT(henka_scene_set_entity_flags(scene, helper, HENKA_SCENE_ENTITY_FLAG_HELPER) == HENKA_SUCCESS);
    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SELECT_OBJECT;
    request.params.entity.entity = helper;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_HELPER_ENTITY);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SET_POSITION;
    request.params.set_position.entity = cube;
    request.params.set_position.position = (henka_vec3){NAN, 0.0f, 0.0f};
    HENKA_TEST_ASSERT(henka_action_validate(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!result.success);
    HENKA_TEST_ASSERT(result.status == HENKA_ACTION_STATUS_INVALID_TRANSFORM);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_GET_SCENE_SUMMARY;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(result.has_scene_summary);
    HENKA_TEST_ASSERT(result.scene_summary.user_entity_count == 3U);
    HENKA_TEST_ASSERT(result.scene_summary.helper_entity_count == 1U);

    HENKA_TEST_ASSERT(henka_action_get_scene_summary(actions, &summary) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(summary.user_entity_count == 3U);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_CLEAR_SCENE;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 0U);
    HENKA_TEST_ASSERT(henka_action_context_get_selected_entity(actions) == HENKA_INVALID_ENTITY);

    henka_action_context_destroy(actions);
    henka_scene_destroy(scene);
}
