#include "test_suite.h"

#include <string.h>

#include <henka/action.h>
#include <henka/core.h>
#include <henka/gizmo.h>

void henka_test_gizmo(void)
{
    henka_action_context* actions;
    henka_action_request request;
    henka_action_result action_result;
    henka_camera camera;
    float distance;
    henka_gizmo_drag_state drag;
    henka_gizmo_handle_hit hit;
    henka_gizmo_model model;
    henka_gizmo_snap_settings snap;
    henka_scene* scene;
    float snapped;
    henka_transform transform;
    henka_transform transformed;
    henka_entity cube;
    float value;
    henka_gizmo_axis axis;
    size_t handle_index;
    henka_vec2 ring_points[5];
    henka_vec2 screen_point;
    henka_viewport viewport;

    HENKA_TEST_ASSERT(henka_gizmo_get_axis_direction(HENKA_GIZMO_AXIS_X, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_gizmo_snap_move(1.14f, 0.25f, &snapped) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(snapped, 1.25f, 0.0001f);
    HENKA_TEST_ASSERT(henka_gizmo_snap_rotate(20.0f * HENKA_DEG_TO_RAD, 15.0f * HENKA_DEG_TO_RAD, &snapped) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(snapped, 15.0f * HENKA_DEG_TO_RAD, 0.0001f);
    HENKA_TEST_ASSERT(henka_gizmo_snap_scale(0.02f, 0.1f, 0.05f, &snapped) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(snapped, 0.1f, 0.0001f);

    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_axis(
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_ray){{0.5f, 0.0f, 3.0f}, {0.0f, 0.0f, -1.0f}},
            1.5f,
            0.25f,
            &axis,
            &distance) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(axis == HENKA_GIZMO_AXIS_X);

    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_axis(
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_ray){{0.0f, 2.0f, 3.0f}, {0.0f, 0.0f, -1.0f}},
            1.5f,
            0.2f,
            &axis,
            &distance) != HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_rotation_rings(
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_ray){{1.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
            1.0f,
            0.2f,
            &axis,
            &distance) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(axis == HENKA_GIZMO_AXIS_Y);

    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_uniform_handle(
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_ray){{0.0f, 0.0f, 3.0f}, {0.0f, 0.0f, -1.0f}},
            0.5f,
            &distance) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_gizmo_project_axis_delta(
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_vec3){1.0f, 0.0f, 0.0f},
            (henka_vec3){0.0f, 0.0f, -1.0f},
            (henka_ray){{1.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}},
            (henka_ray){{2.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}},
            &value) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(value, 1.0f, 0.0001f);

    HENKA_TEST_ASSERT(
        henka_gizmo_project_rotation_delta(
            (henka_vec3){0.0f, 0.0f, 0.0f},
            (henka_vec3){0.0f, 1.0f, 0.0f},
            (henka_ray){{1.0f, 2.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
            (henka_ray){{0.0f, 2.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            &value) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(fabsf(value), 90.0f * HENKA_DEG_TO_RAD, 0.0001f);

    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_segment_2d(
            (henka_vec2){110.0f, 101.0f},
            (henka_vec2){100.0f, 100.0f},
            (henka_vec2){160.0f, 100.0f},
            8.0f,
            &distance) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_rect_2d(
            (henka_vec2){206.0f, 206.0f},
            (henka_vec2){200.0f, 200.0f},
            (henka_vec2){10.0f, 10.0f},
            4.0f) == true);
    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_rect_2d(
            (henka_vec2){220.5f, 200.0f},
            (henka_vec2){200.0f, 200.0f},
            (henka_vec2){10.0f, 10.0f},
            4.0f) == false);

    ring_points[0] = (henka_vec2){150.0f, 100.0f};
    ring_points[1] = (henka_vec2){100.0f, 150.0f};
    ring_points[2] = (henka_vec2){50.0f, 100.0f};
    ring_points[3] = (henka_vec2){100.0f, 50.0f};
    ring_points[4] = (henka_vec2){150.0f, 100.0f};
    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_polyline_2d(
            (henka_vec2){148.0f, 102.0f},
            ring_points,
            5U,
            10.0f,
            &distance) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_gizmo_hit_test_polyline_2d(
            (henka_vec2){100.0f, 100.0f},
            ring_points,
            5U,
            6.0f,
            &distance) != HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_scene_create(&scene) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_create(&actions) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_action_context_set_scene(actions, scene) == HENKA_SUCCESS);
    camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, 16.0f / 9.0f, 0.1f, 100.0f);
    camera.position = (henka_vec3){0.0f, 1.5f, 4.5f};
    HENKA_TEST_ASSERT(henka_action_context_set_camera(actions, &camera) == HENKA_SUCCESS);

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_ADD_PRIMITIVE_OBJECT;
    request.params.add_primitive.primitive = HENKA_ACTION_PRIMITIVE_CUBE;
    request.params.add_primitive.name = "Gizmo Cube";
    request.params.add_primitive.transform = henka_transform_identity();
    request.params.add_primitive.transform.position = (henka_vec3){0.0f, 0.5f, 0.0f};
    request.params.add_primitive.visible = true;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &action_result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(action_result.success);
    cube = action_result.affected_entity;

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SELECT_OBJECT;
    request.params.entity.entity = cube;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &action_result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(action_result.success);

    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    viewport = (henka_viewport){100, 80, 960, 540};
    snap.enabled = true;
    snap.move_snap_increment = 0.25f;
    snap.rotate_snap_increment = 15.0f * HENKA_DEG_TO_RAD;
    snap.scale_snap_increment = 0.1f;
    snap.minimum_scale = 0.01f;

    HENKA_TEST_ASSERT(henka_camera_world_to_screen(&camera, viewport.width, viewport.height, transform.position, &screen_point, NULL) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_gizmo_build_model(
        &camera,
        viewport,
        cube,
        transform,
        HENKA_GIZMO_MODE_MOVE,
        (henka_vec2){(float)viewport.x + screen_point.x + 15.0f, (float)viewport.y + screen_point.y + 15.0f},
        1.0f,
        &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_gizmo_hit_test_model(&model, &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!hit.hit);
    HENKA_TEST_ASSERT(hit.in_dead_zone);
    HENKA_TEST_ASSERT(henka_gizmo_build_model(
        &camera,
        viewport,
        cube,
        transform,
        HENKA_GIZMO_MODE_MOVE,
        (henka_vec2){(float)viewport.x + screen_point.x + 24.0f, (float)viewport.y + screen_point.y + 24.0f},
        1.0f,
        &model) == HENKA_SUCCESS);
    for (handle_index = 0U; handle_index < model.handle_count; ++handle_index)
    {
        if (model.handles[handle_index].type == HENKA_GIZMO_HANDLE_MOVE_BOX &&
            model.handles[handle_index].axis == HENKA_GIZMO_AXIS_X)
        {
            screen_point = model.handles[handle_index].screen_center;
            break;
        }
    }
    HENKA_TEST_ASSERT(handle_index < model.handle_count);
    HENKA_TEST_ASSERT(henka_gizmo_build_model(
        &camera,
        viewport,
        cube,
        transform,
        HENKA_GIZMO_MODE_MOVE,
        (henka_vec2){10.0f, 10.0f},
        1.0f,
        &model) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_gizmo_build_model(
        &camera,
        viewport,
        cube,
        transform,
        HENKA_GIZMO_MODE_MOVE,
        (henka_vec2){(float)viewport.x + screen_point.x, (float)viewport.y + screen_point.y},
        1.0f,
        &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_gizmo_hit_test_model(&model, &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(hit.hit);
    HENKA_TEST_ASSERT(hit.axis == HENKA_GIZMO_AXIS_X);
    HENKA_TEST_ASSERT(henka_gizmo_begin_drag(&model, &hit, &drag) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(drag.target_entity == cube);
    HENKA_TEST_ASSERT(henka_gizmo_apply_drag_to_transform(
        &drag,
        (henka_vec2){
            drag.drag_start_mouse_local.x + drag.drag_axis_screen_direction.x * 80.0f,
            drag.drag_start_mouse_local.y + drag.drag_axis_screen_direction.y * 80.0f},
        &snap,
        &transformed) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(transformed.position.x != transform.position.x);
    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SET_POSITION;
    request.params.set_position.entity = cube;
    request.params.set_position.position = transformed.position;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &action_result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(action_result.success);

    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_gizmo_build_model(
        &camera,
        viewport,
        cube,
        transform,
        HENKA_GIZMO_MODE_SCALE,
        (henka_vec2){(float)viewport.x + screen_point.x, (float)viewport.y + screen_point.y},
        1.0f,
        &model) == HENKA_SUCCESS);
    for (handle_index = 0U; handle_index < model.handle_count; ++handle_index)
    {
        if (model.handles[handle_index].type == HENKA_GIZMO_HANDLE_SCALE_UNIFORM)
        {
            screen_point = model.handles[handle_index].screen_center;
            break;
        }
    }
    HENKA_TEST_ASSERT(handle_index < model.handle_count);
    HENKA_TEST_ASSERT(henka_gizmo_build_model(
        &camera,
        viewport,
        cube,
        transform,
        HENKA_GIZMO_MODE_SCALE,
        (henka_vec2){(float)viewport.x + screen_point.x, (float)viewport.y + screen_point.y},
        1.0f,
        &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_gizmo_hit_test_model(&model, &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(hit.hit);
    HENKA_TEST_ASSERT(hit.axis == HENKA_GIZMO_AXIS_UNIFORM);
    HENKA_TEST_ASSERT(henka_gizmo_hit_test_rect_2d(
        model.mouse_local,
        model.handles[handle_index].screen_center,
        model.handles[handle_index].screen_half_extents,
        model.handles[handle_index].hit_tolerance));
    HENKA_TEST_ASSERT(henka_gizmo_begin_drag(&model, &hit, &drag) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_gizmo_apply_drag_to_transform(
        &drag,
        (henka_vec2){
            drag.drag_start_mouse_local.x + drag.drag_axis_screen_direction.x * 32.0f,
            drag.drag_start_mouse_local.y + drag.drag_axis_screen_direction.y * 32.0f},
        &snap,
        &transformed) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(transformed.scale.x != transform.scale.x);
    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SET_SCALE;
    request.params.set_scale.entity = cube;
    request.params.set_scale.scale = transformed.scale;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &action_result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(action_result.success);

    HENKA_TEST_ASSERT(henka_scene_get_entity_transform(scene, cube, &transform) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_gizmo_build_model(
        &camera,
        viewport,
        cube,
        transform,
        HENKA_GIZMO_MODE_ROTATE,
        (henka_vec2){(float)viewport.x + screen_point.x, (float)viewport.y + screen_point.y},
        1.0f,
        &model) == HENKA_SUCCESS);
    for (handle_index = 0U; handle_index < model.handle_count; ++handle_index)
    {
        if (model.handles[handle_index].type == HENKA_GIZMO_HANDLE_ROTATE_RING &&
            model.handles[handle_index].axis == HENKA_GIZMO_AXIS_Y)
        {
            screen_point = model.handles[handle_index].points[0];
            break;
        }
    }
    HENKA_TEST_ASSERT(handle_index < model.handle_count);
    HENKA_TEST_ASSERT(henka_gizmo_build_model(
        &camera,
        viewport,
        cube,
        transform,
        HENKA_GIZMO_MODE_ROTATE,
        (henka_vec2){(float)viewport.x + screen_point.x, (float)viewport.y + screen_point.y},
        1.0f,
        &model) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_gizmo_hit_test_model(&model, &hit) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(hit.hit);
    HENKA_TEST_ASSERT(hit.type == HENKA_GIZMO_HANDLE_ROTATE_RING);
    HENKA_TEST_ASSERT(henka_gizmo_begin_drag(&model, &hit, &drag) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_gizmo_apply_drag_to_transform(
        &drag,
        (henka_vec2){
            drag.drag_center_screen.x + 20.0f,
            drag.drag_center_screen.y - 60.0f},
        &snap,
        &transformed) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(transformed.rotation.w != transform.rotation.w);
    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SET_ROTATION;
    request.params.set_rotation.entity = cube;
    request.params.set_rotation.rotation = transformed.rotation;
    HENKA_TEST_ASSERT(henka_action_execute(actions, &request, &action_result) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(action_result.success);

    henka_action_context_destroy(actions);
    henka_scene_destroy(scene);
}
