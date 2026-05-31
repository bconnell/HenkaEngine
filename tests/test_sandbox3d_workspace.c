#include "test_suite.h"

#include "../examples/sandbox3d/interaction_tools.h"
#include "../examples/sandbox3d/workspace_tools.h"

void henka_test_sandbox3d_workspace(void)
{
    const sandbox3d_workspace_panel* panel;
    henka_ui_rect ownership[2];
    henka_ui_rect rect;
    sandbox3d_workspace_model model;

    sandbox3d_workspace_model_reset(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_should_start_panels_visible(false));
    HENKA_TEST_ASSERT(sandbox3d_workspace_should_start_panels_visible(true));
    panel = sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT(panel != NULL);
    HENKA_TEST_ASSERT(panel->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT);
    HENKA_TEST_ASSERT(panel->last_docked_zone == SANDBOX3D_WORKSPACE_DOCK_LEFT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_LEFT) == 2U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT) == 2U);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_dock_panel_at(&model, SANDBOX3D_WORKSPACE_DOCK_LEFT, 0U) ==
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_dock_panel_at(&model, SANDBOX3D_WORKSPACE_DOCK_LEFT, 1U) ==
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_dock_panel_at(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT, 0U) ==
        SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_dock_panel_at(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT, 1U) ==
        SANDBOX3D_WORKSPACE_PANEL_UTILITY);
    HENKA_TEST_ASSERT(sandbox3d_workspace_panel_allows_dock(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, SANDBOX3D_WORKSPACE_DOCK_RIGHT));
    HENKA_TEST_ASSERT(!sandbox3d_workspace_panel_allows_dock(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, SANDBOX3D_WORKSPACE_DOCK_FLOATING));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.left_dock_width, 320.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.right_dock_width, 356.0f, 0.0001f);

    rect = (henka_ui_rect){16.0f, 16.0f, 312.0f, 470.0f};
    HENKA_TEST_ASSERT(henka_ui_rect_contains(
        sandbox3d_workspace_docked_title_drag_rect(rect),
        (henka_vec2){rect.x + 12.0f, rect.y + 10.0f}));
    HENKA_TEST_ASSERT(henka_ui_rect_contains(
        sandbox3d_workspace_docked_title_drag_rect(rect),
        (henka_vec2){rect.x + rect.width - 12.0f, rect.y + 10.0f}));
    HENKA_TEST_ASSERT(!henka_ui_rect_contains(
        sandbox3d_workspace_docked_title_drag_rect(rect),
        (henka_vec2){rect.x + 12.0f, rect.y + 34.0f}));
    sandbox3d_workspace_begin_docked_panel_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        rect,
        (henka_vec2){rect.x + 12.0f, rect.y + 10.0f},
        1280,
        720);
    panel = sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT(panel->dock == SANDBOX3D_WORKSPACE_DOCK_FLOATING);
    HENKA_TEST_ASSERT(model.active_drag_panel == SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(panel->floating_rect.x, rect.x, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(panel->floating_rect.y, rect.y, 0.0001f);
    HENKA_TEST_ASSERT(panel->floating_rect.height >= panel->minimum_height);
    HENKA_TEST_ASSERT(henka_ui_rect_contains(panel->floating_rect, (henka_vec2){panel->floating_rect.x + 20.0f, panel->floating_rect.y + 20.0f}));
    HENKA_TEST_ASSERT(henka_ui_rect_contains(
        sandbox3d_workspace_title_drag_rect(panel->floating_rect),
        (henka_vec2){panel->floating_rect.x + 12.0f, panel->floating_rect.y + 10.0f}));
    HENKA_TEST_ASSERT(!henka_ui_rect_contains(
        sandbox3d_workspace_title_drag_rect(panel->floating_rect),
        (henka_vec2){panel->floating_rect.x + panel->floating_rect.width - 12.0f, panel->floating_rect.y + 10.0f}));
    HENKA_TEST_ASSERT(henka_ui_rect_contains(
        sandbox3d_workspace_resize_rect(panel->floating_rect),
        (henka_vec2){panel->floating_rect.x + panel->floating_rect.width - 4.0f, panel->floating_rect.y + panel->floating_rect.height - 4.0f}));

    sandbox3d_workspace_update_panel_drag(&model, (henka_vec2){630.0f, 240.0f}, 1280, 720);
    panel = sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(panel->floating_rect.x, 618.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(panel->floating_rect.y, 230.0f, 0.0001f);
    sandbox3d_workspace_update_panel_drag(&model, (henka_vec2){-80.0f, -40.0f}, 1280, 720);
    panel = sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT(panel->floating_rect.x < 0.0f);
    HENKA_TEST_ASSERT(panel->floating_rect.y < 0.0f);
    sandbox3d_workspace_update_panel_drag(&model, (henka_vec2){1500.0f, 920.0f}, 1280, 720);
    panel = sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT(panel->floating_rect.x + panel->floating_rect.width > 1280.0f);
    HENKA_TEST_ASSERT(panel->floating_rect.y + panel->floating_rect.height > 720.0f);
    HENKA_TEST_ASSERT(panel->floating_rect.width >= panel->minimum_width);
    HENKA_TEST_ASSERT(panel->floating_rect.height >= panel->minimum_height);
    ownership[0] = panel->floating_rect;
    ownership[1] = (henka_ui_rect){350.0f, 16.0f, 500.0f, 600.0f};
    HENKA_TEST_ASSERT(sandbox3d_point_is_owned_by_panels((henka_vec2){1505.0f, 925.0f}, ownership, 2U));
    sandbox3d_workspace_end_interaction(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_panel_is_floating(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS));
    HENKA_TEST_ASSERT(model.active_drag_panel == SANDBOX3D_WORKSPACE_PANEL_NONE);

    sandbox3d_workspace_begin_panel_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        (henka_vec2){panel->floating_rect.x + 10.0f, panel->floating_rect.y + 10.0f});
    HENKA_TEST_ASSERT(model.active_drag_panel == SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    sandbox3d_workspace_end_interaction(&model);
    sandbox3d_workspace_begin_panel_resize(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        (henka_vec2){panel->floating_rect.x + panel->floating_rect.width, panel->floating_rect.y + panel->floating_rect.height});
    sandbox3d_workspace_update_panel_resize(&model, (henka_vec2){panel->floating_rect.x + 40.0f, panel->floating_rect.y + 40.0f}, 1280, 720);
    panel = sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT(panel->floating_rect.width >= panel->minimum_width);
    HENKA_TEST_ASSERT(panel->floating_rect.height >= panel->minimum_height);
    sandbox3d_workspace_end_interaction(&model);

    HENKA_TEST_ASSERT(
        sandbox3d_workspace_evaluate_dock_zone(
            (henka_vec2){24.0f, 80.0f},
            (henka_ui_rect){16.0f, 16.0f, 320.0f, 620.0f},
            (henka_ui_rect){350.0f, 16.0f, 500.0f, 620.0f},
            (henka_ui_rect){900.0f, 16.0f, 356.0f, 620.0f},
            48.0f) == SANDBOX3D_WORKSPACE_DOCK_LEFT);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_evaluate_dock_zone(
            (henka_vec2){1230.0f, 80.0f},
            (henka_ui_rect){16.0f, 16.0f, 320.0f, 620.0f},
            (henka_ui_rect){350.0f, 16.0f, 500.0f, 620.0f},
            (henka_ui_rect){900.0f, 16.0f, 356.0f, 620.0f},
            48.0f) == SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_evaluate_dock_zone(
            (henka_vec2){620.0f, 80.0f},
            (henka_ui_rect){16.0f, 16.0f, 320.0f, 620.0f},
            (henka_ui_rect){350.0f, 16.0f, 500.0f, 620.0f},
            (henka_ui_rect){900.0f, 16.0f, 356.0f, 620.0f},
            48.0f) == SANDBOX3D_WORKSPACE_DOCK_FLOATING);

    sandbox3d_workspace_dock_panel(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    HENKA_TEST_ASSERT(!sandbox3d_workspace_panel_is_floating(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS)->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS)->last_docked_zone == SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_LEFT) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT) == 3U);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_dock_panel_at(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT, 2U) ==
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    sandbox3d_workspace_dock_panel(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY, SANDBOX3D_WORKSPACE_DOCK_LEFT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY)->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY)->last_docked_zone == SANDBOX3D_WORKSPACE_DOCK_LEFT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_LEFT) == 2U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT) == 2U);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_dock_panel_at(&model, SANDBOX3D_WORKSPACE_DOCK_LEFT, 1U) ==
        SANDBOX3D_WORKSPACE_PANEL_UTILITY);
    sandbox3d_workspace_detach_panel(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY, 42U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_panel_is_detached(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY)->detached_window_id == 42U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY)->last_docked_zone == SANDBOX3D_WORKSPACE_DOCK_LEFT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_LEFT) == 1U);
    sandbox3d_workspace_dock_panel(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY, SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    HENKA_TEST_ASSERT(!sandbox3d_workspace_panel_is_detached(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY)->detached_window_id == 0U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT) == 3U);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_dock_panel_at(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT, 2U) ==
        SANDBOX3D_WORKSPACE_PANEL_UTILITY);
    sandbox3d_workspace_dock_panel(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, SANDBOX3D_WORKSPACE_DOCK_LEFT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS)->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_LEFT) == 2U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT) == 2U);

    rect = (henka_ui_rect){910.0f, 16.0f, 340.0f, 560.0f};
    HENKA_TEST_ASSERT(henka_ui_rect_contains(
        sandbox3d_workspace_docked_title_drag_rect(rect),
        (henka_vec2){rect.x + rect.width - 18.0f, rect.y + 12.0f}));
    sandbox3d_workspace_begin_docked_panel_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_UTILITY,
        rect,
        (henka_vec2){rect.x + rect.width - 18.0f, rect.y + 12.0f},
        1280,
        720);
    panel = sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY);
    HENKA_TEST_ASSERT(panel->dock == SANDBOX3D_WORKSPACE_DOCK_FLOATING);
    HENKA_TEST_ASSERT(model.active_drag_panel == SANDBOX3D_WORKSPACE_PANEL_UTILITY);
    sandbox3d_workspace_update_panel_drag(&model, (henka_vec2){860.0f, 120.0f}, 1280, 720);
    HENKA_TEST_ASSERT(sandbox3d_workspace_panel_is_floating(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(sandbox3d_point_is_owned_by_panels(
        (henka_vec2){panel->floating_rect.x + 8.0f, panel->floating_rect.y + 8.0f},
        &panel->floating_rect,
        1U));
    sandbox3d_workspace_end_interaction(&model);
    sandbox3d_workspace_dock_panel(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY, SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY)->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT) == 2U);

    sandbox3d_workspace_dock_panel(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    sandbox3d_workspace_dock_panel(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, SANDBOX3D_WORKSPACE_DOCK_LEFT);
    sandbox3d_workspace_dock_panel(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_LEFT) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_dock_panel_count(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT) == 3U);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_dock_panel_at(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT, 2U) ==
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_dock_panel_at(&model, SANDBOX3D_WORKSPACE_DOCK_RIGHT, 1U) !=
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    sandbox3d_workspace_dock_panel(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, SANDBOX3D_WORKSPACE_DOCK_LEFT);

    sandbox3d_workspace_begin_dock_resize(
        &model,
        SANDBOX3D_WORKSPACE_RESIZE_LEFT_DOCK,
        (henka_vec2){320.0f, 200.0f});
    sandbox3d_workspace_update_dock_resize(&model, (henka_vec2){370.0f, 200.0f}, 1280, 520.0f, 300.0f, model.right_dock_width);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.left_dock_width, 334.0f, 0.0001f);
    sandbox3d_workspace_begin_dock_resize(
        &model,
        SANDBOX3D_WORKSPACE_RESIZE_RIGHT_DOCK,
        (henka_vec2){948.0f, 200.0f});
    sandbox3d_workspace_update_dock_resize(&model, (henka_vec2){898.0f, 200.0f}, 1280, 520.0f, 332.0f, model.left_dock_width);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.right_dock_width, 356.0f, 0.0001f);

    sandbox3d_workspace_begin_docked_panel_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS,
        (henka_ui_rect){920.0f, 500.0f, 356.0f, 400.0f},
        (henka_vec2){940.0f, 510.0f},
        1280,
        720);
    panel = sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(panel->floating_rect.x, 920.0f, 0.0001f);
    HENKA_TEST_ASSERT(panel->floating_rect.y + panel->floating_rect.height > 720.0f);
    sandbox3d_workspace_model_reset(&model);
    sandbox3d_workspace_begin_docked_panel_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        (henka_ui_rect){16.0f, 16.0f, 312.0f, 470.0f},
        (henka_vec2){26.0f, 26.0f},
        1280,
        720);
    sandbox3d_workspace_end_interaction(&model);
    sandbox3d_workspace_begin_docked_panel_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS,
        (henka_ui_rect){900.0f, 16.0f, 356.0f, 400.0f},
        (henka_vec2){910.0f, 26.0f},
        1280,
        720);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS)->z_order >
        sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS)->z_order);
    sandbox3d_workspace_end_interaction(&model);
    panel = sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    sandbox3d_workspace_begin_panel_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        (henka_vec2){panel->floating_rect.x + 10.0f, panel->floating_rect.y + 10.0f});
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS)->z_order >
        sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS)->z_order);
    sandbox3d_workspace_model_reset(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(&model, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS)->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    HENKA_TEST_ASSERT(model.active_drag_panel == SANDBOX3D_WORKSPACE_PANEL_NONE);
    HENKA_TEST_ASSERT(model.resize_target == SANDBOX3D_WORKSPACE_RESIZE_NONE);
}
