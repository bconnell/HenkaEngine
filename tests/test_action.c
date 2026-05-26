#include "test_suite.h"

#include <string.h>

#include <henka/action.h>
#include <henka/core.h>

void henka_test_action(void)
{
    henka_action_context* actions;
    henka_action_object_details details[4];
    henka_action_request request;
    henka_action_result result;
    henka_action_scene_summary summary;
    henka_camera camera;
    henka_scene* scene;
    henka_transform transform;
    henka_entity cube;
    henka_entity helper;
    size_t object_count;

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_create(&actions) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_set_scene(actions, scene) == HENKA_SUCCESS);
    camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, 16.0f / 9.0f, 0.1f, 100.0f);
    HENKA_TEST_ASSERT(henka_action_context_set_camera(actions, &camera) == HENKA_SUCCESS);

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
    request.command = HENKA_ACTION_COMMAND_ROTATE_BY_DELTA;
    request.params.rotate_by_delta.entity = cube;
    request.params.rotate_by_delta.delta_rotation = henka_quat_from_axis_angle((henka_vec3){0.0f, 1.0f, 0.0f}, 30.0f * HENKA_DEG_TO_RAD);
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(result.has_after_transform);
    HENKA_TEST_ASSERT(result.after_transform.rotation.w != 1.0f);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER;
    request.params.scale_by_multiplier.entity = cube;
    request.params.scale_by_multiplier.scale_multiplier = (henka_vec3){1.5f, 2.0f, 0.0f};
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.x, 1.5f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.y, 2.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(transform.scale.z, 0.01f, 0.0001f);

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
    HENKA_TEST_ASSERT(result.scene_summary.user_entity_count == 1U);
    HENKA_TEST_ASSERT(result.scene_summary.helper_entity_count == 1U);

    HENKA_TEST_ASSERT(henka_action_get_scene_summary(actions, &summary) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(summary.user_entity_count == 1U);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_CLEAR_SCENE;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(result.success);
    HENKA_TEST_ASSERT(henka_scene_get_entity_count(scene) == 0U);
    HENKA_TEST_ASSERT(henka_action_context_get_selected_entity(actions) == HENKA_INVALID_ENTITY);

    henka_action_context_destroy(actions);
    henka_scene_destroy(scene);
}
