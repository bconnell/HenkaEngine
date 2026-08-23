#include "test_suite.h"

#include <math.h>
#include <string.h>

#include "../examples/sandbox3d/interaction_tools.h"
#include "../examples/sandbox3d/terrain_autosave.h"

static void henka_test_sandbox3d_authoring_cage_overlay(void)
{
    const char* test_marker =
        "HENKA_T1B_AUTHORING_CAGE_TEST_V1";
    henka_authoring_mesh_desc desc;
    henka_authoring_mesh* mesh = NULL;
    henka_authoring_vertex_id vertices[4];
    henka_authoring_face_id face_id = 0U;
    sandbox3d_authoring_cage_edge cage_edges[8];
    size_t cage_edge_count;
    size_t index;
    bool diagonal_02 = false;
    bool diagonal_13 = false;

    HENKA_TEST_ASSERT(
        strcmp(
            test_marker,
            "HENKA_T1B_AUTHORING_CAGE_TEST_V1") == 0);

    HENKA_TEST_ASSERT(
        SANDBOX3D_AUTHORING_RENDER_TRIANGLES_DEFAULT);

    desc = henka_authoring_mesh_desc_default();
    desc.max_vertices = 8U;
    desc.max_edges = 8U;
    desc.max_faces = 2U;
    desc.max_face_corners = 4U;

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_create(
            &desc,
            &mesh) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){-1.0f, -1.0f, 0.0f},
            (henka_vec2){0.0f, 0.0f},
            0U,
            &vertices[0]) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){1.0f, -1.0f, 0.0f},
            (henka_vec2){1.0f, 0.0f},
            0U,
            &vertices[1]) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){1.0f, 1.0f, 0.0f},
            (henka_vec2){1.0f, 1.0f},
            0U,
            &vertices[2]) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_add_vertex(
            mesh,
            (henka_vec3){-1.0f, 1.0f, 0.0f},
            (henka_vec2){0.0f, 1.0f},
            0U,
            &vertices[3]) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_authoring_mesh_add_face(
            mesh,
            vertices,
            4U,
            0U,
            false,
            &face_id) == HENKA_SUCCESS);

    cage_edge_count = 99U;

    HENKA_TEST_ASSERT(
        sandbox3d_build_authoring_cage(
            mesh,
            cage_edges,
            sizeof(cage_edges) / sizeof(cage_edges[0]),
            &cage_edge_count) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(cage_edge_count == 4U);

    for (index = 0U;
         index < cage_edge_count;
         ++index)
    {
        const henka_authoring_vertex_id first =
            cage_edges[index].vertices[0];

        const henka_authoring_vertex_id second =
            cage_edges[index].vertices[1];

        if ((first == vertices[0] &&
             second == vertices[2]) ||
            (first == vertices[2] &&
             second == vertices[0]))
        {
            diagonal_02 = true;
        }

        if ((first == vertices[1] &&
             second == vertices[3]) ||
            (first == vertices[3] &&
             second == vertices[1]))
        {
            diagonal_13 = true;
        }
    }

    HENKA_TEST_ASSERT(!diagonal_02);
    HENKA_TEST_ASSERT(!diagonal_13);

    cage_edge_count = 99U;

    HENKA_TEST_ASSERT(
        sandbox3d_build_authoring_cage(
            mesh,
            cage_edges,
            3U,
            &cage_edge_count) ==
        HENKA_ERROR_INVALID_ARGUMENT);

    HENKA_TEST_ASSERT(cage_edge_count == 0U);

    henka_authoring_mesh_destroy(mesh);
}
void henka_test_sandbox3d_interaction(void)
{
    henka_test_sandbox3d_authoring_cage_overlay();
    double autosave_elapsed_seconds = 0.0;
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
    sandbox3d_projected_triangle topology_triangles[6];
    sandbox3d_silhouette_segment topology_segments[32];
    size_t topology_segment_count;
    {
        static sandbox3d_projected_triangle dense_triangles[5000];
        static sandbox3d_silhouette_segment dense_segments[15000];
        size_t dense_index;

        memset(dense_triangles, 0, sizeof(dense_triangles));
        for (dense_index = 0U; dense_index < 5000U; ++dense_index)
        {
            const float x = dense_index < 2U ? 0.0f : (float)dense_index * 4.0f;
            sandbox3d_projected_triangle* triangle = &dense_triangles[dense_index];
            triangle->points[0] = (henka_vec2){x, 0.0f};
            triangle->points[1] = (henka_vec2){x + 1.0f, 0.0f};
            triangle->points[2] = (henka_vec2){x, 1.0f};
            triangle->depths[0] = dense_index == 0U ? 0.8f : dense_index == 1U ? 0.2f : 0.5f;
            triangle->depths[1] = triangle->depths[0];
            triangle->depths[2] = triangle->depths[0];
            triangle->vertex_ids[0] = ((uint64_t)dense_index * 3U) + 1U;
            triangle->vertex_ids[1] = ((uint64_t)dense_index * 3U) + 2U;
            triangle->vertex_ids[2] = ((uint64_t)dense_index * 3U) + 3U;
        }
        HENKA_TEST_ASSERT(
            sandbox3d_build_topology_silhouette(
                dense_triangles, 5000U, dense_segments, 15000U) == 14997U);
    }

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
    HENKA_TEST_ASSERT(!sandbox3d_should_start_empty_viewport_pan(
        SANDBOX3D_VIEWPORT_TOOL_SELECT,
        &gate,
        true,
        false,
        false,
        SANDBOX3D_EMPTY_VIEWPORT_PAN_DRAG_THRESHOLD_PIXELS - 0.01f));
    HENKA_TEST_ASSERT(sandbox3d_should_start_empty_viewport_pan(
        SANDBOX3D_VIEWPORT_TOOL_SELECT,
        &gate,
        true,
        false,
        false,
        SANDBOX3D_EMPTY_VIEWPORT_PAN_DRAG_THRESHOLD_PIXELS));
    HENKA_TEST_ASSERT(!sandbox3d_should_start_empty_viewport_pan(
        SANDBOX3D_VIEWPORT_TOOL_SELECT,
        &gate,
        true,
        true,
        false,
        20.0f));
    HENKA_TEST_ASSERT(!sandbox3d_should_start_empty_viewport_pan(
        SANDBOX3D_VIEWPORT_TOOL_SELECT,
        &gate,
        true,
        false,
        true,
        20.0f));
    HENKA_TEST_ASSERT(!sandbox3d_should_start_right_mouse_pan(
        &gate,
        true,
        false,
        false,
        SANDBOX3D_RIGHT_MOUSE_PAN_DRAG_THRESHOLD_PIXELS - 0.01f));
    HENKA_TEST_ASSERT(sandbox3d_should_start_right_mouse_pan(
        &gate,
        true,
        false,
        false,
        SANDBOX3D_RIGHT_MOUSE_PAN_DRAG_THRESHOLD_PIXELS));
    HENKA_TEST_ASSERT(!sandbox3d_should_start_right_mouse_pan(
        &gate,
        true,
        true,
        false,
        SANDBOX3D_RIGHT_MOUSE_PAN_DRAG_THRESHOLD_PIXELS));
    HENKA_TEST_ASSERT(!sandbox3d_should_start_right_mouse_pan(
        &gate,
        true,
        false,
        true,
        SANDBOX3D_RIGHT_MOUSE_PAN_DRAG_THRESHOLD_PIXELS));
    HENKA_TEST_ASSERT(!sandbox3d_should_start_empty_viewport_pan(
        SANDBOX3D_VIEWPORT_TOOL_MOVE,
        &gate,
        true,
        false,
        false,
        20.0f));
    gate.ui_wants_mouse = true;
    HENKA_TEST_ASSERT(!sandbox3d_should_start_empty_viewport_pan(
        SANDBOX3D_VIEWPORT_TOOL_SELECT,
        &gate,
        true,
        false,
        false,
        20.0f));
    gate.ui_wants_mouse = false;

    HENKA_TEST_ASSERT(sandbox3d_should_prefer_terrain_hit(true, 3.0f, false, 0.0f));
    HENKA_TEST_ASSERT(sandbox3d_should_prefer_terrain_hit(true, 3.0f, true, 4.0f));
    HENKA_TEST_ASSERT(!sandbox3d_should_prefer_terrain_hit(true, 5.0f, true, 4.0f));
    HENKA_TEST_ASSERT(!sandbox3d_should_prefer_terrain_hit(false, 3.0f, true, 4.0f));
    HENKA_TEST_ASSERT(!sandbox3d_should_prefer_terrain_hit(true, -1.0f, false, 0.0f));
    HENKA_TEST_ASSERT(!sandbox3d_should_prefer_terrain_hit(true, NAN, false, 0.0f));

    HENKA_TEST_ASSERT(!sandbox3d_terrain_autosave_is_due(
        &autosave_elapsed_seconds,
        9.0,
        0U,
        SANDBOX3D_TERRAIN_AUTOSAVE_INTERVAL_SECONDS));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(autosave_elapsed_seconds, 0.0, 0.0001);
    HENKA_TEST_ASSERT(!sandbox3d_terrain_autosave_is_due(
        &autosave_elapsed_seconds,
        4.0,
        1U,
        SANDBOX3D_TERRAIN_AUTOSAVE_INTERVAL_SECONDS));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(autosave_elapsed_seconds, 4.0, 0.0001);
    HENKA_TEST_ASSERT(sandbox3d_terrain_autosave_is_due(
        &autosave_elapsed_seconds,
        6.0,
        1U,
        SANDBOX3D_TERRAIN_AUTOSAVE_INTERVAL_SECONDS));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(autosave_elapsed_seconds, 0.0, 0.0001);
    HENKA_TEST_ASSERT(!sandbox3d_terrain_autosave_is_due(
        &autosave_elapsed_seconds,
        10.0,
        0U,
        SANDBOX3D_TERRAIN_AUTOSAVE_INTERVAL_SECONDS));

    autosave_elapsed_seconds = -1.0;
    HENKA_TEST_ASSERT(!sandbox3d_terrain_autosave_is_due(
        &autosave_elapsed_seconds,
        1.0,
        1U,
        SANDBOX3D_TERRAIN_AUTOSAVE_INTERVAL_SECONDS));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(autosave_elapsed_seconds, 1.0, 0.0001);

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
    gate.selected_bounds_valid = true;
    HENKA_TEST_ASSERT(sandbox3d_selection_highlight_is_allowed(&gate));
    gate.selected_object_transform_locked = true;
    HENKA_TEST_ASSERT(!sandbox3d_selection_highlight_is_allowed(&gate));
    HENKA_TEST_ASSERT(
        sandbox3d_evaluate_gizmo_reject_reason(SANDBOX3D_VIEWPORT_TOOL_MOVE, &gate) ==
        SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_TRANSFORM_LOCKED);
    HENKA_TEST_ASSERT(
        strcmp(
            sandbox3d_interaction_reject_reason_to_string(
                SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_TRANSFORM_LOCKED),
            "Selected object transform locked") == 0);
    gate.selected_object_transform_locked = false;
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

    memset(topology_triangles, 0, sizeof(topology_triangles));
    topology_triangles[0].points[0] = (henka_vec2){0.0f, 0.0f};
    topology_triangles[0].points[1] = (henka_vec2){2.0f, 0.0f};
    topology_triangles[0].points[2] = (henka_vec2){2.0f, 2.0f};
    topology_triangles[0].vertex_ids[0] = 1U;
    topology_triangles[0].vertex_ids[1] = 2U;
    topology_triangles[0].vertex_ids[2] = 3U;
    topology_triangles[1].points[0] = (henka_vec2){0.0f, 0.0f};
    topology_triangles[1].points[1] = (henka_vec2){2.0f, 2.0f};
    topology_triangles[1].points[2] = (henka_vec2){0.0f, 2.0f};
    topology_triangles[1].vertex_ids[0] = 1U;
    topology_triangles[1].vertex_ids[1] = 3U;
    topology_triangles[1].vertex_ids[2] = 4U;
    topology_segment_count = sandbox3d_build_topology_silhouette(
        topology_triangles, 2U, topology_segments, 32U);
    HENKA_TEST_ASSERT(topology_segment_count == 4U);
    topology_triangles[2] = topology_triangles[0];
    topology_triangles[3] = topology_triangles[1];
    topology_triangles[2].points[0].x += 4.0f;
    topology_triangles[2].points[1].x += 4.0f;
    topology_triangles[2].points[2].x += 4.0f;
    topology_triangles[3].points[0].x += 4.0f;
    topology_triangles[3].points[1].x += 4.0f;
    topology_triangles[3].points[2].x += 4.0f;
    /* A logical owner can aggregate separate children whose mesh-local vertex
     * indices intentionally start at the same values.  The silhouette
     * builder receives source-scoped IDs and must not merge those unrelated
     * edges. */
    topology_triangles[2].vertex_ids[0] |= UINT64_C(2) << 32U;
    topology_triangles[2].vertex_ids[1] |= UINT64_C(2) << 32U;
    topology_triangles[2].vertex_ids[2] |= UINT64_C(2) << 32U;
    topology_triangles[3].vertex_ids[0] |= UINT64_C(2) << 32U;
    topology_triangles[3].vertex_ids[1] |= UINT64_C(2) << 32U;
    topology_triangles[3].vertex_ids[2] |= UINT64_C(2) << 32U;
    topology_segment_count = sandbox3d_build_topology_silhouette(
        topology_triangles, 4U, topology_segments, 32U);
    HENKA_TEST_ASSERT(topology_segment_count == 8U);
    {
        size_t segment_index;
        for (segment_index = 0U; segment_index < topology_segment_count; ++segment_index)
        {
            const sandbox3d_silhouette_segment* segment = &topology_segments[segment_index];
            HENKA_TEST_ASSERT(!(
                (segment->start.x < 2.1f && segment->end.x > 3.9f) ||
                (segment->end.x < 2.1f && segment->start.x > 3.9f)));
        }
    }
    topology_triangles[4].points[0] = (henka_vec2){0.0f, 0.0f};
    topology_triangles[4].points[1] = (henka_vec2){2.0f, 0.0f};
    topology_triangles[4].points[2] = (henka_vec2){1.0f, 2.0f};
    topology_triangles[4].depths[0] = 0.8f;
    topology_triangles[4].depths[1] = 0.8f;
    topology_triangles[4].depths[2] = 0.8f;
    topology_triangles[4].vertex_ids[0] = 20U;
    topology_triangles[4].vertex_ids[1] = 21U;
    topology_triangles[4].vertex_ids[2] = 22U;
    topology_triangles[5] = topology_triangles[4];
    topology_triangles[5].depths[0] = 0.2f;
    topology_triangles[5].depths[1] = 0.2f;
    topology_triangles[5].depths[2] = 0.2f;
    topology_triangles[5].vertex_ids[0] = 30U;
    topology_triangles[5].vertex_ids[1] = 31U;
    topology_triangles[5].vertex_ids[2] = 32U;
    topology_segment_count = sandbox3d_build_topology_silhouette(
        topology_triangles + 4U, 2U, topology_segments, 32U);
    HENKA_TEST_ASSERT(topology_segment_count == 3U);

    memset(topology_triangles, 0, sizeof(topology_triangles));
    topology_triangles[0].points[0] = (henka_vec2){0.0f, 0.0f};
    topology_triangles[0].points[1] = (henka_vec2){10.0f, 0.0f};
    topology_triangles[0].points[2] = (henka_vec2){10.0f, 10.0f};
    topology_triangles[0].depths[0] = 0.8f;
    topology_triangles[0].depths[1] = 0.8f;
    topology_triangles[0].depths[2] = 0.8f;
    topology_triangles[0].vertex_ids[0] = 40U;
    topology_triangles[0].vertex_ids[1] = 41U;
    topology_triangles[0].vertex_ids[2] = 42U;
    topology_triangles[1].points[0] = (henka_vec2){4.9f, 0.0f};
    topology_triangles[1].points[1] = (henka_vec2){5.1f, 0.0f};
    topology_triangles[1].points[2] = (henka_vec2){5.0f, 0.2f};
    topology_triangles[1].depths[0] = 0.2f;
    topology_triangles[1].depths[1] = 0.2f;
    topology_triangles[1].depths[2] = 0.2f;
    topology_triangles[1].vertex_ids[0] = 50U;
    topology_triangles[1].vertex_ids[1] = 51U;
    topology_triangles[1].vertex_ids[2] = 52U;
    topology_segment_count = sandbox3d_build_topology_silhouette(
        topology_triangles, 2U, topology_segments, 32U);
    {
        size_t segment_index;
        bool full_candidate_edge = false;
        bool candidate_left_visible = false;
        bool candidate_right_visible = false;
        for (segment_index = 0U; segment_index < topology_segment_count; ++segment_index)
        {
            const sandbox3d_silhouette_segment* segment = &topology_segments[segment_index];
            const float minimum_x = fminf(segment->start.x, segment->end.x);
            const float maximum_x = fmaxf(segment->start.x, segment->end.x);
            if (fabsf(segment->start.y) < 0.001f && fabsf(segment->end.y) < 0.001f)
            {
                full_candidate_edge = full_candidate_edge ||
                    (minimum_x < 0.001f && maximum_x > 9.999f);
                candidate_left_visible = candidate_left_visible ||
                    (minimum_x < 4.89f && maximum_x > 0.01f);
                candidate_right_visible = candidate_right_visible ||
                    (minimum_x < 9.99f && maximum_x > 5.11f);
            }
        }
        HENKA_TEST_ASSERT(!full_candidate_edge);
        HENKA_TEST_ASSERT(candidate_left_visible);
        HENKA_TEST_ASSERT(candidate_right_visible);
    }

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
