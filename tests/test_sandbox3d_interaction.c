#include "test_suite.h"

#include <string.h>

#include "../examples/sandbox3d/interaction_tools.h"

void henka_test_sandbox3d_interaction(void)
{
    henka_camera camera;
    henka_quat rotation_delta;
    henka_bounds selection_bounds;
    sandbox3d_selection_highlight_model highlight_model;
    sandbox3d_selection_highlight_model ground_highlight_model;
    henka_vec3 focus_target;
    henka_vec3 move_delta;
    henka_vec3 scale_multiplier;
    henka_vec2 clip_start;
    henka_vec2 clip_end;
    sandbox3d_interaction_gate gate;
    henka_ui_rect panels[4];
    henka_ui_rect viewport_clip;

    HENKA_TEST_ASSERT(sandbox3d_viewport_tool_mode_to_gizmo_mode(SANDBOX3D_VIEWPORT_TOOL_MOVE) == HENKA_GIZMO_MODE_MOVE);
    HENKA_TEST_ASSERT(sandbox3d_viewport_tool_mode_to_gizmo_mode(SANDBOX3D_VIEWPORT_TOOL_ROTATE) == HENKA_GIZMO_MODE_ROTATE);
    HENKA_TEST_ASSERT(sandbox3d_viewport_tool_mode_to_gizmo_mode(SANDBOX3D_VIEWPORT_TOOL_SCALE) == HENKA_GIZMO_MODE_SCALE);
    HENKA_TEST_ASSERT(sandbox3d_viewport_tool_mode_to_gizmo_mode(SANDBOX3D_VIEWPORT_TOOL_ORBIT) == HENKA_GIZMO_MODE_SELECT);
    HENKA_TEST_ASSERT(sandbox3d_viewport_tool_mode_uses_gizmo(SANDBOX3D_VIEWPORT_TOOL_MOVE));
    HENKA_TEST_ASSERT(!sandbox3d_viewport_tool_mode_uses_gizmo(SANDBOX3D_VIEWPORT_TOOL_PAN));
    HENKA_TEST_ASSERT(sandbox3d_viewport_tool_mode_is_navigation(SANDBOX3D_VIEWPORT_TOOL_ORBIT));
    HENKA_TEST_ASSERT(!sandbox3d_viewport_tool_mode_is_navigation(SANDBOX3D_VIEWPORT_TOOL_SCALE));

    panels[0] = (henka_ui_rect){16.0f, 16.0f, 320.0f, 470.0f};
    panels[1] = (henka_ui_rect){16.0f, 498.0f, 320.0f, 190.0f};
    panels[2] = (henka_ui_rect){924.0f, 16.0f, 340.0f, 400.0f};
    panels[3] = (henka_ui_rect){924.0f, 428.0f, 340.0f, 260.0f};
    HENKA_TEST_ASSERT(sandbox3d_point_is_owned_by_panels((henka_vec2){120.0f, 100.0f}, panels, 4U));
    HENKA_TEST_ASSERT(sandbox3d_point_is_owned_by_panels((henka_vec2){120.0f, 520.0f}, panels, 4U));
    HENKA_TEST_ASSERT(sandbox3d_point_is_owned_by_panels((henka_vec2){1020.0f, 120.0f}, panels, 4U));
    HENKA_TEST_ASSERT(sandbox3d_point_is_owned_by_panels((henka_vec2){1020.0f, 500.0f}, panels, 4U));
    HENKA_TEST_ASSERT(!sandbox3d_point_is_owned_by_panels((henka_vec2){600.0f, 240.0f}, panels, 4U));

    memset(&gate, 0, sizeof(gate));
    gate.supported_mouse_button = true;
    gate.cursor_in_viewport = true;
    HENKA_TEST_ASSERT(sandbox3d_evaluate_select_reject_reason(&gate) == SANDBOX3D_INTERACTION_REJECT_NONE);

    gate.mouse_capture_active = true;
    HENKA_TEST_ASSERT(sandbox3d_evaluate_select_reject_reason(&gate) == SANDBOX3D_INTERACTION_REJECT_MOUSE_CAPTURE_ACTIVE);
    gate.mouse_capture_active = false;
    gate.ui_wants_mouse = true;
    HENKA_TEST_ASSERT(sandbox3d_evaluate_select_reject_reason(&gate) == SANDBOX3D_INTERACTION_REJECT_UI_OWNS_MOUSE);
    gate.ui_wants_mouse = false;
    gate.cursor_in_viewport = false;
    HENKA_TEST_ASSERT(sandbox3d_evaluate_select_reject_reason(&gate) == SANDBOX3D_INTERACTION_REJECT_CURSOR_OUTSIDE_VIEWPORT);

    memset(&gate, 0, sizeof(gate));
    gate.supported_mouse_button = true;
    gate.cursor_in_viewport = true;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_navigation_reject_reason(SANDBOX3D_VIEWPORT_TOOL_SELECT, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_NAVIGATION_MODE_INACTIVE);
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_navigation_reject_reason(SANDBOX3D_VIEWPORT_TOOL_ORBIT, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_NONE);
    gate.ui_wants_mouse = true;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_navigation_reject_reason(SANDBOX3D_VIEWPORT_TOOL_PAN, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_UI_OWNS_MOUSE);
    gate.ui_wants_mouse = false;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_navigation_reject_reason(SANDBOX3D_VIEWPORT_TOOL_PAN, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_NONE);

    memset(&gate, 0, sizeof(gate));
    gate.supported_mouse_button = true;
    gate.cursor_in_viewport = true;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_gizmo_reject_reason(SANDBOX3D_VIEWPORT_TOOL_MOVE, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_NO_SELECTED_OBJECT);
    gate.selected_object_present = true;
    gate.selected_object_valid = true;
    gate.selected_object_visible = true;
    gate.selected_object_selectable = true;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_gizmo_reject_reason(SANDBOX3D_VIEWPORT_TOOL_MOVE, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_GIZMO_MODE_INACTIVE);
    gate.gizmo_mode_active = true;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_gizmo_reject_reason(SANDBOX3D_VIEWPORT_TOOL_MOVE, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_GIZMO_MODEL_INVALID);
    gate.gizmo_model_valid = true;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_gizmo_reject_reason(SANDBOX3D_VIEWPORT_TOOL_MOVE, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_OVERLAY_HAS_NO_PRIMITIVES);
    gate.overlay_has_primitives = true;
    gate.ui_wants_mouse = true;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_gizmo_reject_reason(SANDBOX3D_VIEWPORT_TOOL_MOVE, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_UI_OWNS_MOUSE);
    gate.ui_wants_mouse = false;
    gate.mouse_capture_active = true;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_gizmo_reject_reason(SANDBOX3D_VIEWPORT_TOOL_MOVE, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_MOUSE_CAPTURE_ACTIVE);
    gate.mouse_capture_active = false;
    gate.cursor_in_viewport = false;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_gizmo_reject_reason(SANDBOX3D_VIEWPORT_TOOL_MOVE, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_CURSOR_OUTSIDE_VIEWPORT);
    gate.cursor_in_viewport = true;
    gate.cursor_in_gizmo_dead_zone = true;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_gizmo_reject_reason(SANDBOX3D_VIEWPORT_TOOL_MOVE, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_CURSOR_IN_GIZMO_DEAD_ZONE);
    gate.cursor_in_gizmo_dead_zone = false;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_gizmo_reject_reason(SANDBOX3D_VIEWPORT_TOOL_MOVE, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_NO_HANDLE_UNDER_CURSOR);
    gate.handle_under_cursor = true;
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_gizmo_reject_reason(SANDBOX3D_VIEWPORT_TOOL_MOVE, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_NONE);

    move_delta = sandbox3d_make_move_delta(HENKA_GIZMO_AXIS_X, 0.25f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(move_delta.x, 0.25f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(move_delta.y, 0.0f, 0.0001f);
    move_delta = sandbox3d_make_move_delta(HENKA_GIZMO_AXIS_Z, -0.5f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(move_delta.z, -0.5f, 0.0001f);

    rotation_delta = sandbox3d_make_rotation_delta(HENKA_GIZMO_AXIS_Y, 15.0f * HENKA_DEG_TO_RAD);
    HENKA_TEST_ASSERT(rotation_delta.w != henka_quat_identity().w || rotation_delta.y != henka_quat_identity().y);
    scale_multiplier = sandbox3d_make_uniform_scale_multiplier(0.10f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(scale_multiplier.x, 1.10f, 0.0001f);
    scale_multiplier = sandbox3d_make_uniform_scale_multiplier(-1.5f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(scale_multiplier.x, 0.10f, 0.0001f);

    selection_bounds = (henka_bounds){{1.0f, 2.0f, 3.0f}, {0.5f, 0.75f, 1.0f}};
    HENKA_TEST_ASSERT(sandbox3d_build_selection_highlight_model(selection_bounds, &highlight_model));
    HENKA_TEST_ASSERT(highlight_model.valid);
    HENKA_TEST_ASSERT(highlight_model.edge_count == 12U);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(highlight_model.edge_starts[0].x, 0.5f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(highlight_model.edge_ends[0].x, 1.5f, 0.0001f);
    selection_bounds.extents.x = 0.0f;
    HENKA_TEST_ASSERT(!sandbox3d_build_selection_highlight_model(selection_bounds, &highlight_model));

    HENKA_TEST_ASSERT(sandbox3d_build_ground_selection_highlight_model(
        (henka_vec3){0.0f, -0.02f, 0.0f},
        6.0f,
        0.04f,
        &ground_highlight_model));
    HENKA_TEST_ASSERT(ground_highlight_model.valid);
    HENKA_TEST_ASSERT(ground_highlight_model.edge_count == 4U);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ground_highlight_model.edge_starts[0].x, -6.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ground_highlight_model.edge_starts[0].y, 0.02f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ground_highlight_model.edge_ends[1].z, 6.0f, 0.0001f);
    HENKA_TEST_ASSERT(!sandbox3d_build_ground_selection_highlight_model(
        (henka_vec3){0.0f, 0.0f, 0.0f},
        5000.0f,
        0.0f,
        &ground_highlight_model));

    viewport_clip = (henka_ui_rect){0.0f, 0.0f, 640.0f, 360.0f};
    clip_start = (henka_vec2){-100.0f, 120.0f};
    clip_end = (henka_vec2){700.0f, 120.0f};
    HENKA_TEST_ASSERT(sandbox3d_clip_line_to_rect(&clip_start, &clip_end, viewport_clip));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(clip_start.x, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(clip_end.x, 640.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(clip_start.y, 120.0f, 0.0001f);
    clip_start = (henka_vec2){-50.0f, -50.0f};
    clip_end = (henka_vec2){-10.0f, -10.0f};
    HENKA_TEST_ASSERT(!sandbox3d_clip_line_to_rect(&clip_start, &clip_end, viewport_clip));

    camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, 16.0f / 9.0f, 0.1f, 100.0f);
    camera.position = (henka_vec3){0.0f, 2.4f, 8.6f};
    camera.yaw_radians = -HENKA_PI * 0.5f;
    camera.pitch_radians = -0.22f;
    focus_target = (henka_vec3){0.0f, 0.6f, 0.0f};
    HENKA_TEST_ASSERT(henka_camera_orbit_target(&camera, focus_target, 0.25f, 0.15f));
    HENKA_TEST_ASSERT(camera.position.x != 0.0f || camera.position.y != 2.4f || camera.position.z != 8.6f);
    HENKA_TEST_ASSERT(henka_camera_pan_target(&camera, &focus_target, 0.35f, 0.20f));
    HENKA_TEST_ASSERT(focus_target.x != 0.0f || focus_target.y != 0.6f || focus_target.z != 0.0f);
}
