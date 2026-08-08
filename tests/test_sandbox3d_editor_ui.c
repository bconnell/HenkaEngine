#include "test_suite.h"

#include <math.h>

#include <henka/persistence.h>

#include "../examples/sandbox3d/editor_ui_state.h"

void henka_test_sandbox3d_editor_ui(void)
{
    henka_settings* settings;
    sandbox3d_editor_ui_state state;
    sandbox3d_editor_ui_state stored;

    settings = NULL;
    HENKA_TEST_ASSERT(
        henka_settings_create(&settings) == HENKA_SUCCESS);

    sandbox3d_editor_ui_state_reset(&state);

    HENKA_TEST_ASSERT(!state.controls_workspace_expanded);
    HENKA_TEST_ASSERT(state.controls_viewer_expanded);
    HENKA_TEST_ASSERT(state.controls_viewport_expanded);
    HENKA_TEST_ASSERT(!state.controls_viewport_tool_expanded);

    HENKA_TEST_ASSERT(state.details_overview_expanded);
    HENKA_TEST_ASSERT(state.details_transform_expanded);
    HENKA_TEST_ASSERT(state.details_materials_expanded);
    HENKA_TEST_ASSERT(!state.details_physics_expanded);
    HENKA_TEST_ASSERT(!state.details_interaction_expanded);
    HENKA_TEST_ASSERT(!state.details_actions_expanded);

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.controls_scroll_offset, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.controls_content_height, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.details_scroll_offset, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.details_content_height, 0.0f, 0.0001f);

    HENKA_TEST_ASSERT(
        henka_settings_set_bool(
            settings,
            "ui.controls.main.workspace.expanded",
            true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_settings_set_bool(
            settings,
            "ui.controls.main.viewer.expanded",
            false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_settings_set_bool(
            settings,
            "ui.object_details.actions.expanded",
            true) == HENKA_SUCCESS);

    sandbox3d_editor_ui_state_load(settings, &state);

    HENKA_TEST_ASSERT(state.controls_workspace_expanded);
    HENKA_TEST_ASSERT(!state.controls_viewer_expanded);
    HENKA_TEST_ASSERT(state.controls_viewport_expanded);
    HENKA_TEST_ASSERT(!state.controls_viewport_tool_expanded);
    HENKA_TEST_ASSERT(state.details_overview_expanded);
    HENKA_TEST_ASSERT(state.details_transform_expanded);
    HENKA_TEST_ASSERT(state.details_materials_expanded);
    HENKA_TEST_ASSERT(!state.details_physics_expanded);
    HENKA_TEST_ASSERT(!state.details_interaction_expanded);
    HENKA_TEST_ASSERT(state.details_actions_expanded);

    HENKA_TEST_ASSERT(
        henka_settings_set_string(
            settings,
            "ui.controls.main.viewport.expanded",
            "not-a-bool") == HENKA_SUCCESS);
    sandbox3d_editor_ui_state_load(settings, &state);
    HENKA_TEST_ASSERT(state.controls_viewport_expanded);

    sandbox3d_editor_ui_state_reset(&stored);
    stored.controls_workspace_expanded = true;
    stored.controls_viewer_expanded = false;
    stored.controls_viewport_expanded = false;
    stored.controls_viewport_tool_expanded = true;
    stored.details_overview_expanded = false;
    stored.details_transform_expanded = false;
    stored.details_materials_expanded = true;
    stored.details_physics_expanded = true;
    stored.details_interaction_expanded = true;
    stored.details_actions_expanded = true;

    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_state_store(
            settings,
            &stored) == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(
        henka_settings_get_bool(
            settings,
            "ui.controls.main.workspace.expanded",
            false));
    HENKA_TEST_ASSERT(
        !henka_settings_get_bool(
            settings,
            "ui.controls.main.viewer.expanded",
            true));
    HENKA_TEST_ASSERT(
        !henka_settings_get_bool(
            settings,
            "ui.controls.main.viewport.expanded",
            true));
    HENKA_TEST_ASSERT(
        henka_settings_get_bool(
            settings,
            "ui.controls.main.viewport_tool.expanded",
            false));

    HENKA_TEST_ASSERT(
        !henka_settings_get_bool(
            settings,
            "ui.object_details.overview.expanded",
            true));
    HENKA_TEST_ASSERT(
        !henka_settings_get_bool(
            settings,
            "ui.object_details.transform.expanded",
            true));
    HENKA_TEST_ASSERT(
        henka_settings_get_bool(
            settings,
            "ui.object_details.materials.expanded",
            false));
    HENKA_TEST_ASSERT(
        henka_settings_get_bool(
            settings,
            "ui.object_details.physics.expanded",
            false));
    HENKA_TEST_ASSERT(
        henka_settings_get_bool(
            settings,
            "ui.object_details.interaction.expanded",
            false));
    HENKA_TEST_ASSERT(
        henka_settings_get_bool(
            settings,
            "ui.object_details.actions.expanded",
            false));

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_clamp_scroll(
            40.0f, 200.0f, 100.0f),
        40.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_clamp_scroll(
            140.0f, 200.0f, 100.0f),
        100.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_clamp_scroll(
            20.0f, 80.0f, 100.0f),
        0.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_clamp_scroll(
            -1.0f, 200.0f, 100.0f),
        0.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_clamp_scroll(
            NAN, 200.0f, 100.0f),
        0.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_clamp_scroll(
            20.0f, NAN, 100.0f),
        0.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_clamp_scroll(
            20.0f, 200.0f, 0.0f),
        0.0f,
        0.0001f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_state_store(
            NULL,
            &stored) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_state_store(
            settings,
            NULL) == HENKA_ERROR_INVALID_ARGUMENT);

    henka_settings_destroy(settings);
}