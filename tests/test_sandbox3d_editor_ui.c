#include "test_suite.h"

#include <math.h>
#include <string.h>

#include <henka/persistence.h>

#include "../examples/sandbox3d/editor_ui_state.h"
#include "../examples/sandbox3d/modeling_toolbar.h"

void henka_test_sandbox3d_editor_ui(void)
{
    sandbox3d_modeling_toolbar_state toolbar;
    char toolbar_summary[96];
    henka_settings* settings;
    sandbox3d_editor_ui_state invalid_state;
    sandbox3d_editor_ui_state state;
    sandbox3d_editor_ui_state stored;

    settings = NULL;
    HENKA_TEST_ASSERT(
        henka_settings_create(&settings) == HENKA_SUCCESS);

    sandbox3d_modeling_toolbar_state_reset(&toolbar);
    HENKA_TEST_ASSERT(
        toolbar.selection_mode == SANDBOX3D_AUTHORING_SELECTION_FACE);
    HENKA_TEST_ASSERT(
        toolbar.transform_tool == SANDBOX3D_TRANSFORM_TOOL_NONE);
    HENKA_TEST_ASSERT(
        toolbar.orientation_mode == SANDBOX3D_AUTHORING_ORIENTATION_WORLD);
    HENKA_TEST_ASSERT(
        toolbar.pivot_mode == SANDBOX3D_AUTHORING_PIVOT_MEDIAN);
    HENKA_TEST_ASSERT(!toolbar.snap_enabled);
    HENKA_TEST_ASSERT(!toolbar.xray_enabled);
    HENKA_TEST_ASSERT(!toolbar.authoring_available);
    HENKA_TEST_ASSERT(
        strcmp(
            sandbox3d_modeling_toolbar_selection_label(
                SANDBOX3D_AUTHORING_SELECTION_VERTEX),
            "Vertex") == 0);
    HENKA_TEST_ASSERT(
        strcmp(
            sandbox3d_modeling_toolbar_disabled_reason(
                SANDBOX3D_MODELING_TOOLBAR_ACTION_MOVE,
                &toolbar),
            "Make the selected asset editable first.") == 0);
    HENKA_TEST_ASSERT(
        strcmp(
            sandbox3d_modeling_toolbar_disabled_reason(
                SANDBOX3D_MODELING_TOOLBAR_ACTION_XRAY,
                &toolbar),
            "Make the selected asset editable first.") == 0);
    toolbar.authoring_available = true;
    HENKA_TEST_ASSERT(
        strcmp(
            sandbox3d_modeling_toolbar_action_tooltip(
                SANDBOX3D_MODELING_TOOLBAR_ACTION_XRAY),
            "Select front-facing components through occluding mesh surfaces.") == 0);
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_toolbar_format_summary(
            &toolbar,
            toolbar_summary,
            sizeof(toolbar_summary)) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        strcmp(toolbar_summary, "Face | Select | World | Median | 0 selected") == 0);

    sandbox3d_editor_ui_state_reset(&state);

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_details_footer_reserve(false),
        0.0f,
        0.0001f);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_details_footer_reserve(true) > 24.0f);

    HENKA_TEST_ASSERT(!state.controls_workspace_expanded);
    HENKA_TEST_ASSERT(state.controls_viewer_expanded);
    HENKA_TEST_ASSERT(state.controls_viewport_expanded);
    HENKA_TEST_ASSERT(!state.controls_viewport_tool_expanded);

    HENKA_TEST_ASSERT(state.details_overview_expanded);
    HENKA_TEST_ASSERT(state.details_transform_expanded);
    HENKA_TEST_ASSERT(state.details_materials_expanded);
    HENKA_TEST_ASSERT(!state.details_audio_expanded);
    HENKA_TEST_ASSERT(!state.details_physics_expanded);
    HENKA_TEST_ASSERT(!state.details_interaction_expanded);
    HENKA_TEST_ASSERT(!state.details_actions_expanded);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_details_group_order_is_valid(
            state.details_group_order));
    HENKA_TEST_ASSERT(
        state.details_group_order[0] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_OVERVIEW);
    HENKA_TEST_ASSERT(
        state.details_group_order[
            SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT - 1U] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_AUDIO);

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
    stored.details_audio_expanded = true;
    stored.details_physics_expanded = true;
    stored.details_interaction_expanded = true;
    stored.details_actions_expanded = true;
    stored.controls_scroll_offset = 96.0f;
    stored.details_scroll_offset = 144.0f;
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_reorder_details_group(
            &stored,
            SANDBOX3D_EDITOR_DETAILS_GROUP_MATERIALS,
            0U));
    HENKA_TEST_ASSERT(
        stored.details_group_order[0] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_MATERIALS);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_reorder_details_group(
            &stored,
            0U,
            SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT - 1U));
    HENKA_TEST_ASSERT(
        stored.details_group_order[
            SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT - 1U] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_MATERIALS);
    HENKA_TEST_ASSERT(
        !sandbox3d_editor_ui_reorder_details_group(
            &stored,
            SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT,
            0U));
    invalid_state = stored;
    invalid_state.details_group_order[1] = invalid_state.details_group_order[0];
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_state_store(
            settings,
            &invalid_state) == HENKA_ERROR_INVALID_ARGUMENT);

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
            "ui.object_details.audio.expanded",
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
        henka_settings_get_float(
            settings,
            "ui.controls.scroll.offset",
            0.0f),
        96.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        henka_settings_get_float(
            settings,
            "ui.object_details.scroll.offset",
            0.0f),
        144.0f,
        0.0001f);
    HENKA_TEST_ASSERT(
        henka_settings_get_int(
            settings,
            "ui.object_details.group_order.7",
            -1) == SANDBOX3D_EDITOR_DETAILS_GROUP_MATERIALS);

    sandbox3d_editor_ui_state_load(settings, &state);
    HENKA_TEST_ASSERT(state.details_audio_expanded);
    HENKA_TEST_ASSERT(
        state.details_group_order[
            SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT - 1U] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_MATERIALS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.controls_scroll_offset,
        96.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.details_scroll_offset,
        144.0f,
        0.0001f);
    HENKA_TEST_ASSERT(
        henka_settings_set_string(
            settings,
            "ui.controls.scroll.offset",
            "-4") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        henka_settings_set_string(
            settings,
            "ui.object_details.scroll.offset",
            "nan") == HENKA_SUCCESS);
    sandbox3d_editor_ui_state_load(settings, &state);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.controls_scroll_offset,
        0.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.details_scroll_offset,
        0.0f,
        0.0001f);
    HENKA_TEST_ASSERT(
        henka_settings_set_int(
            settings,
            "ui.object_details.group_order.0",
            99) == HENKA_SUCCESS);
    sandbox3d_editor_ui_state_load(settings, &state);
    HENKA_TEST_ASSERT(
        state.details_group_order[0] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_OVERVIEW);

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

    {
        sandbox3d_editor_scroll_state scroll = {0.0f, 0.0f, 0.0f};
        const float thumb_height =
            sandbox3d_editor_ui_scrollbar_thumb_height(
                400.0f,
                100.0f,
                200.0f);

        sandbox3d_editor_ui_scroll_state_set_content(
            &scroll,
            400.0f,
            100.0f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_ui_scroll_state_apply_delta(
                &scroll,
                12.5f,
                100.0f));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(scroll.offset, 12.5f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(thumb_height, 50.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            sandbox3d_editor_ui_scrollbar_thumb_offset(
                scroll.offset,
                scroll.content_height,
                scroll.viewport_height,
                200.0f,
                thumb_height),
            6.25f,
            0.0001f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_ui_scroll_state_set_from_scrollbar(
                &scroll,
                150.0f,
                10.0f,
                200.0f,
                thumb_height,
                25.0f));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(scroll.offset, 230.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_ui_scroll_state_apply_delta(
                &scroll,
                1000.0f,
                100.0f));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(scroll.offset, 300.0f, 0.0001f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_ui_scroll_state_apply_delta(
                &scroll,
                -1000.0f,
                100.0f));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(scroll.offset, 0.0f, 0.0001f);
        scroll.content_height = 80.0f;
        HENKA_TEST_ASSERT(
            !sandbox3d_editor_ui_scroll_state_apply_delta(
                &scroll,
                24.0f,
                100.0f));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(scroll.offset, 0.0f, 0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            sandbox3d_editor_ui_scrollbar_thumb_height(
                80.0f,
                100.0f,
                200.0f),
            0.0f,
            0.0001f);
    }

    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_state_store(
            NULL,
            &stored) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_state_store(
            settings,
            NULL) == HENKA_ERROR_INVALID_ARGUMENT);


    state.controls_scroll_offset = 0.0f;
    state.controls_content_height = 300.0f;
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_scroll_controls(
            &state,
            100.0f,
            1));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.controls_scroll_offset,
        48.0f,
        0.0001f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_scroll_controls(
            &state,
            100.0f,
            10));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.controls_scroll_offset,
        200.0f,
        0.0001f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_scroll_controls(
            &state,
            100.0f,
            -1));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.controls_scroll_offset,
        152.0f,
        0.0001f);

    state.controls_scroll_offset = 0.0f;
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_scroll_controls(
            &state,
            100.0f,
            -1));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.controls_scroll_offset,
        0.0f,
        0.0001f);

    state.controls_scroll_offset = 20.0f;
    state.controls_content_height = 80.0f;
    HENKA_TEST_ASSERT(
        !sandbox3d_editor_ui_scroll_controls(
            &state,
            100.0f,
            1));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.controls_scroll_offset,
        0.0f,
        0.0001f);

    state.controls_scroll_offset = 20.0f;
    state.controls_content_height = 300.0f;
    HENKA_TEST_ASSERT(
        !sandbox3d_editor_ui_scroll_controls(
            &state,
            NAN,
            1));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.controls_scroll_offset,
        0.0f,
        0.0001f);

    HENKA_TEST_ASSERT(
        !sandbox3d_editor_ui_scroll_controls(
            NULL,
            100.0f,
            1));

    state.controls_workspace_expanded = true;
    state.controls_viewer_expanded = false;
    state.details_actions_expanded = true;
    state.controls_scroll_offset = 120.0f;
    state.controls_content_height = 300.0f;
    state.details_scroll_offset = 80.0f;
    state.details_content_height = 240.0f;
    sandbox3d_editor_ui_state_reset(&state);
    HENKA_TEST_ASSERT(!state.controls_workspace_expanded);
    HENKA_TEST_ASSERT(state.controls_viewer_expanded);
    HENKA_TEST_ASSERT(!state.details_actions_expanded);
    HENKA_TEST_ASSERT(!state.details_audio_expanded);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.controls_scroll_offset, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.controls_content_height, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.details_scroll_offset, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(state.details_content_height, 0.0f, 0.0001f);

    HENKA_TEST_ASSERT(
        !sandbox3d_editor_ui_scroll_controls(
            &state,
            100.0f,
            0));

    state.details_scroll_offset = 0.0f;
    state.details_content_height = 260.0f;
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_scroll_details(
            &state,
            100.0f,
            1));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.details_scroll_offset,
        48.0f,
        0.0001f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_scroll_details(
            &state,
            100.0f,
            10));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.details_scroll_offset,
        160.0f,
        0.0001f);

    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_scroll_details(
            &state,
            100.0f,
            -1));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.details_scroll_offset,
        112.0f,
        0.0001f);

    state.details_scroll_offset = 20.0f;
    state.details_content_height = 80.0f;
    HENKA_TEST_ASSERT(
        !sandbox3d_editor_ui_scroll_details(
            &state,
            100.0f,
            1));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.details_scroll_offset,
        0.0f,
        0.0001f);

    state.details_scroll_offset = 20.0f;
    state.details_content_height = 260.0f;
    HENKA_TEST_ASSERT(
        !sandbox3d_editor_ui_scroll_details(
            &state,
            NAN,
            1));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.details_scroll_offset,
        0.0f,
        0.0001f);

    HENKA_TEST_ASSERT(
        !sandbox3d_editor_ui_scroll_details(
            NULL,
            100.0f,
            1));
    henka_settings_destroy(settings);
}
