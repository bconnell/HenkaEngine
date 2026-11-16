#include "test_suite.h"

#include <float.h>
#include <math.h>
#include <string.h>

#include <henka/persistence.h>

#include "../examples/sandbox3d/editor_ui_state.h"
#include "../examples/sandbox3d/editor_scale_control.h"
#include "../examples/sandbox3d/editor_utility_settings_layout.h"
#include "../examples/sandbox3d/modeling_toolbar.h"

void henka_test_sandbox3d_editor_ui(void)
{
    sandbox3d_modeling_toolbar_state toolbar;
    char toolbar_summary[96];
    char toolbar_options_summary[96];
    henka_settings* settings;
    sandbox3d_editor_ui_state invalid_state;
    sandbox3d_editor_ui_state legacy_state;
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
    HENKA_TEST_ASSERT(!toolbar.options_expanded);
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
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_toolbar_format_options_summary(
            &toolbar,
            toolbar_options_summary,
            sizeof(toolbar_options_summary)) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        strcmp(
            toolbar_options_summary,
            "World | Median | Snap off | X-Ray off") == 0);
    toolbar.orientation_mode = SANDBOX3D_AUTHORING_ORIENTATION_LOCAL;
    toolbar.pivot_mode = SANDBOX3D_AUTHORING_PIVOT_INDIVIDUAL;
    toolbar.snap_enabled = true;
    toolbar.xray_enabled = true;
    HENKA_TEST_ASSERT(
        sandbox3d_modeling_toolbar_format_options_summary(
            &toolbar,
            toolbar_options_summary,
            sizeof(toolbar_options_summary)) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(
        strcmp(
            toolbar_options_summary,
            "Local | Individual | Snap on | X-Ray on") == 0);

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

    HENKA_TEST_ASSERT(!state.details_overview_expanded);
    HENKA_TEST_ASSERT(state.details_transform_expanded);
    HENKA_TEST_ASSERT(!state.details_materials_expanded);
    HENKA_TEST_ASSERT(!state.details_authoring_expanded);
    HENKA_TEST_ASSERT(!state.details_audio_expanded);
    HENKA_TEST_ASSERT(!state.details_physics_expanded);
    HENKA_TEST_ASSERT(!state.details_interaction_expanded);
    HENKA_TEST_ASSERT(!state.details_actions_expanded);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_details_group_order_is_valid(
            state.details_group_order));
    HENKA_TEST_ASSERT(
        state.details_group_order[0] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_TRANSFORM);
    HENKA_TEST_ASSERT(
        state.details_group_order[1] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_AUTHORING);
    HENKA_TEST_ASSERT(
        state.details_group_order[2] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_OVERVIEW);
    HENKA_TEST_ASSERT(
        state.details_group_order[
            SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT - 1U] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_HIERARCHY);

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.controls_scroll_offset, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.controls_content_height, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.details_scroll_offset, 0.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.scene_objects_actions_scroll_offset, 0.0f, 0.0001f);
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
    HENKA_TEST_ASSERT(!state.details_overview_expanded);
    HENKA_TEST_ASSERT(state.details_transform_expanded);
    HENKA_TEST_ASSERT(!state.details_materials_expanded);
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
    stored.scene_objects_actions_scroll_offset = 192.0f;
    stored.details_scroll_offset = 144.0f;
    {
        size_t group_position;
        for (group_position = 0U;
             group_position < SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT;
             ++group_position)
        {
            if (stored.details_group_order[group_position] ==
                SANDBOX3D_EDITOR_DETAILS_GROUP_MATERIALS)
            {
                break;
            }
        }
        HENKA_TEST_ASSERT(
            group_position < SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_ui_reorder_details_group(
                &stored,
                group_position,
                0U));
    }
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
            "ui.object_details.group_order.8",
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
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.scene_objects_actions_scroll_offset,
        192.0f,
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
    HENKA_TEST_ASSERT(
        henka_settings_set_string(
            settings,
            "ui.scene_objects.actions.scroll.offset",
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
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.scene_objects_actions_scroll_offset,
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
            SANDBOX3D_EDITOR_DETAILS_GROUP_TRANSFORM);

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

    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_controls_scroll_delta(48.0f, 31.84f),
        3.84f,
        0.001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_controls_scroll_delta(-48.0f, 31.84f),
        -3.84f,
        0.001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_controls_scroll_delta(48.0f, 100.0f),
        48.0f,
        0.001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_controls_scroll_delta(200.0f, 100.0f),
        200.0f,
        0.001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_controls_scroll_delta(48.0f, 60.0f),
        32.0f,
        0.001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_scene_objects_scroll_delta(48.0f, 60.0f),
        36.0f,
        0.001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_editor_ui_scene_objects_scroll_delta(200.0f, 100.0f),
        200.0f,
        0.001f);

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

    state.scene_objects_actions_scroll_offset = 0.0f;
    state.scene_objects_actions_content_height = 300.0f;
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_scroll_scene_objects_actions_by(
            &state,
            100.0f,
            48.0f));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.scene_objects_actions_scroll_offset,
        48.0f,
        0.0001f);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_scroll_scene_objects_actions_by(
            &state,
            100.0f,
            500.0f));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.scene_objects_actions_scroll_offset,
        200.0f,
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

    state.utility_settings_scroll_offset = 0.0f;
    state.utility_settings_content_height = 640.0f;
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_scroll_utility_settings(
            &state,
            240.0f,
            1));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.utility_settings_scroll_offset,
        48.0f,
        0.0001f);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_scroll_utility_settings(
            &state,
            240.0f,
            20));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.utility_settings_scroll_offset,
        400.0f,
        0.0001f);
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_scroll_utility_settings(
            &state,
            240.0f,
            -1));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.utility_settings_scroll_offset,
        352.0f,
        0.0001f);
    state.controls_scroll_offset = 96.0f;
    state.details_scroll_offset = 72.0f;
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_state_store(settings, &state) == HENKA_SUCCESS);
    sandbox3d_editor_ui_state_reset(&stored);
    sandbox3d_editor_ui_state_load(settings, &stored);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        stored.utility_settings_scroll_offset,
        352.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        stored.controls_scroll_offset,
        96.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        stored.details_scroll_offset,
        72.0f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        stored.utility_settings_content_height,
        0.0f,
        0.0001f);

    state.utility_settings_scroll_offset = 24.0f;
    state.utility_settings_content_height = 120.0f;
    HENKA_TEST_ASSERT(
        !sandbox3d_editor_ui_scroll_utility_settings(
            &state,
            240.0f,
            1));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        state.utility_settings_scroll_offset,
        0.0f,
        0.0001f);
    HENKA_TEST_ASSERT(
        !sandbox3d_editor_ui_scroll_utility_settings(
            NULL,
            240.0f,
            1));

    {
        sandbox3d_utility_settings_layout settings_layout;
        sandbox3d_editor_scale_control_layout_desc scale_layout;
        henka_ui_rect settings_viewport = {10.0f, 20.0f, 400.0f, 200.0f};
        henka_ui_rect partial_rect;
        const float boundary_widths[] = {
            112.0f, 127.0f, 128.0f, 183.0f, 184.0f, 231.0f, 232.0f,
            273.0f, 274.0f, 327.0f, 328.0f, 400.0f};
        size_t width_index;
        size_t control_index;

        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_layout_build(
                settings_viewport,
                false,
                0.0f,
                &settings_layout) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(settings_layout.adjustment_row_count == 1U);
        HENKA_TEST_ASSERT(settings_layout.environment_row_count == 1U);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            settings_layout.content_height,
            570.0f,
            0.0001f);
        HENKA_TEST_ASSERT(
            !sandbox3d_utility_settings_rect_is_fully_visible(
                settings_viewport,
                settings_layout.controls[
                    SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_SMOOTH]));
        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_rect_is_fully_visible(
                settings_viewport,
                settings_layout.scale_bounds));

        /* At 200% UI scale the real 1280x720 package leaves a compact
         * Settings viewport. The scale control must remain fully reachable
         * there so the user can return to a smaller scale. */
        settings_viewport = (henka_ui_rect){409.0f, 302.0f, 193.0f, 32.0f};
        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_layout_build(
                settings_viewport,
                false,
                0.0f,
                &settings_layout) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_rect_is_fully_visible(
                settings_viewport,
                settings_layout.scale_bounds));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            settings_layout.scale_bounds.height,
            24.0f,
            0.0001f);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_scale_control_layout(
                settings_layout.scale_bounds,
                &scale_layout) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(scale_layout.compact);
        HENKA_TEST_ASSERT(!scale_layout.stacked);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scale_layout.choices.x,
            settings_viewport.x + SANDBOX3D_EDITOR_SCALE_COMPACT_LABEL_WIDTH,
            0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            scale_layout.choices.width / 4.0f,
            (settings_viewport.width - SANDBOX3D_EDITOR_SCALE_COMPACT_LABEL_WIDTH) / 4.0f,
            0.0001f);
        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_rect_is_fully_visible(
                settings_viewport,
                scale_layout.choices));

        settings_viewport = (henka_ui_rect){10.0f, 20.0f, 191.0f, 44.0f};
        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_layout_build(
                settings_viewport,
                false,
                0.0f,
                &settings_layout) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            sandbox3d_editor_scale_control_layout(
                settings_layout.scale_bounds,
                &scale_layout) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(!scale_layout.compact);
        HENKA_TEST_ASSERT(scale_layout.stacked);
        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_rect_is_fully_visible(
                settings_viewport,
                scale_layout.choices));

        settings_viewport.width = 260.0f;
        settings_viewport.height = 220.0f;
        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_layout_build(
                settings_viewport,
                true,
                0.0f,
                &settings_layout) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(settings_layout.adjustment_row_count == 2U);
        HENKA_TEST_ASSERT(settings_layout.environment_row_count == 2U);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            settings_layout.controls[
                SANDBOX3D_UTILITY_SETTINGS_CONTROL_SPEED_LESS].y -
                settings_layout.controls[
                    SANDBOX3D_UTILITY_SETTINGS_CONTROL_SENSITIVITY_LESS].y,
            30.0f,
            0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            settings_layout.controls[
                SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_PRESET].y -
                settings_layout.controls[
                    SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_CYCLE].y,
            30.0f,
            0.0001f);
        HENKA_TEST_ASSERT(
            settings_layout.controls[
                SANDBOX3D_UTILITY_SETTINGS_CONTROL_SENSITIVITY_MORE].x +
                    settings_layout.controls[
                        SANDBOX3D_UTILITY_SETTINGS_CONTROL_SENSITIVITY_MORE].width <=
                settings_viewport.x + settings_viewport.width);
        HENKA_TEST_ASSERT(
            !sandbox3d_utility_settings_rect_is_fully_visible(
                settings_viewport,
                settings_layout.controls[
                    SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_SMOOTH]));
        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_layout_build(
                settings_viewport,
                true,
                settings_layout.content_height - settings_viewport.height,
                &settings_layout) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_rect_is_fully_visible(
                settings_viewport,
                settings_layout.controls[
                    SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_SMOOTH]));
        HENKA_TEST_ASSERT(
            !sandbox3d_utility_settings_rect_is_fully_visible(
                settings_viewport,
                settings_layout.scale_bounds));

        settings_viewport.width = 128.0f;
        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_layout_build(
                settings_viewport,
                false,
                0.0f,
                &settings_layout) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(settings_layout.adjustment_row_count == 2U);
        HENKA_TEST_ASSERT(settings_layout.environment_row_count == 3U);
        HENKA_TEST_ASSERT(
            settings_layout.controls[
                SANDBOX3D_UTILITY_SETTINGS_CONTROL_SPEED_MORE].x +
                    settings_layout.controls[
                        SANDBOX3D_UTILITY_SETTINGS_CONTROL_SPEED_MORE].width <=
                settings_viewport.x + settings_viewport.width);
        partial_rect = settings_layout.controls[
            SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_CYCLE];
        partial_rect.y = settings_viewport.y - 1.0f;
        HENKA_TEST_ASSERT(
            !sandbox3d_utility_settings_rect_is_fully_visible(
                settings_viewport,
                partial_rect));
        settings_viewport.width = 96.0f;
        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_layout_build(
                settings_viewport,
                false,
                0.0f,
                &settings_layout) == HENKA_ERROR_INVALID_ARGUMENT);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(settings_layout.content_height, 0.0f, 0.0001f);

        settings_viewport.x = 0.0f;
        settings_viewport.y = 0.0f;
        settings_viewport.height = 240.0f;
        for (width_index = 0U;
             width_index < sizeof(boundary_widths) / sizeof(boundary_widths[0]);
             ++width_index)
        {
            settings_viewport.width = boundary_widths[width_index];
            HENKA_TEST_ASSERT(
                sandbox3d_utility_settings_layout_build(
                    settings_viewport,
                    true,
                    0.0f,
                    &settings_layout) == HENKA_SUCCESS);
            HENKA_TEST_ASSERT(
                settings_layout.adjustment_row_count ==
                (settings_viewport.width >= 274.0f ? 1U :
                    settings_viewport.width >= 128.0f ? 2U : 4U));
            HENKA_TEST_ASSERT(
                settings_layout.environment_row_count ==
                (settings_viewport.width >= 328.0f ? 1U :
                    settings_viewport.width >= 232.0f ? 2U : 3U));
            HENKA_TEST_ASSERT(
                settings_layout.exposure_stacked ==
                (settings_viewport.width < 184.0f));
            HENKA_TEST_ASSERT_FLOAT_CLOSE(
                settings_layout.controls[
                    SANDBOX3D_UTILITY_SETTINGS_CONTROL_EXPOSURE_MORE].x,
                settings_layout.controls[
                    SANDBOX3D_UTILITY_SETTINGS_CONTROL_EXPOSURE_LESS].x +
                    (settings_layout.exposure_stacked ? 0.0f : 96.0f),
                0.0001f);
            HENKA_TEST_ASSERT_FLOAT_CLOSE(
                settings_layout.controls[
                    SANDBOX3D_UTILITY_SETTINGS_CONTROL_EXPOSURE_MORE].y,
                settings_layout.controls[
                    SANDBOX3D_UTILITY_SETTINGS_CONTROL_EXPOSURE_LESS].y +
                    (settings_layout.exposure_stacked ? 30.0f : 0.0f),
                0.0001f);
            HENKA_TEST_ASSERT_FLOAT_CLOSE(
                settings_layout.rows[
                    SANDBOX3D_UTILITY_SETTINGS_ROW_SENSITIVITY_SPEED].y,
                settings_layout.controls[
                    SANDBOX3D_UTILITY_SETTINGS_CONTROL_EXPOSURE_MORE].y +
                    30.0f,
                0.0001f);
            for (control_index = 0U;
                 control_index < SANDBOX3D_UTILITY_SETTINGS_CONTROL_COUNT;
                 ++control_index)
            {
                const henka_ui_rect control = settings_layout.controls[control_index];
                HENKA_TEST_ASSERT(control.x >= settings_viewport.x);
                HENKA_TEST_ASSERT(control.width > 0.0f);
                HENKA_TEST_ASSERT(
                    control.x + control.width <=
                    settings_viewport.x + settings_viewport.width);
            }
        }

        settings_viewport.width = FLT_MAX;
        settings_viewport.x = FLT_MAX;
        HENKA_TEST_ASSERT(
            sandbox3d_utility_settings_layout_build(
                settings_viewport,
                false,
                0.0f,
                &settings_layout) == HENKA_ERROR_NUMERIC_RANGE);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(settings_layout.content_height, 0.0f, 0.0001f);
    }
    /* Existing installations persisted the former expanded, Overview-first
     * defaults. Migrate only that exact untouched default; retain custom
     * expansion and section-order choices. */
    sandbox3d_editor_ui_state_reset(&legacy_state);
    legacy_state.details_overview_expanded = true;
    legacy_state.details_transform_expanded = true;
    legacy_state.details_materials_expanded = true;
    legacy_state.details_authoring_expanded = true;
    {
        size_t group_index;
        for (group_index = 0U;
             group_index < SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT;
             ++group_index)
        {
            legacy_state.details_group_order[group_index] =
                (unsigned char)group_index;
        }
    }
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_state_store(settings, &legacy_state) ==
            HENKA_SUCCESS);
    sandbox3d_editor_ui_state_load(settings, &state);
    HENKA_TEST_ASSERT(!state.details_overview_expanded);
    HENKA_TEST_ASSERT(state.details_transform_expanded);
    HENKA_TEST_ASSERT(!state.details_materials_expanded);
    HENKA_TEST_ASSERT(!state.details_authoring_expanded);
    HENKA_TEST_ASSERT(
        state.details_group_order[0] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_TRANSFORM);
    HENKA_TEST_ASSERT(
        state.details_group_order[1] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_AUTHORING);
    HENKA_TEST_ASSERT(
        state.details_group_order[2] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_OVERVIEW);

    /* A changed expansion choice prevents the legacy-default migration even
     * when the saved section order still matches the old default. */
    sandbox3d_editor_ui_state_reset(&legacy_state);
    legacy_state.details_overview_expanded = true;
    legacy_state.details_transform_expanded = true;
    legacy_state.details_materials_expanded = true;
    legacy_state.details_authoring_expanded = true;
    legacy_state.details_physics_expanded = true;
    {
        size_t group_index;
        for (group_index = 0U;
             group_index < SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT;
             ++group_index)
        {
            legacy_state.details_group_order[group_index] =
                (unsigned char)group_index;
        }
    }
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_state_store(settings, &legacy_state) ==
            HENKA_SUCCESS);
    sandbox3d_editor_ui_state_load(settings, &state);
    HENKA_TEST_ASSERT(state.details_overview_expanded);
    HENKA_TEST_ASSERT(state.details_transform_expanded);
    HENKA_TEST_ASSERT(state.details_materials_expanded);
    HENKA_TEST_ASSERT(state.details_authoring_expanded);
    HENKA_TEST_ASSERT(state.details_physics_expanded);
    HENKA_TEST_ASSERT(
        state.details_group_order[0] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_OVERVIEW);

    /* A user-reordered legacy layout also retains its old section state. */
    sandbox3d_editor_ui_state_reset(&legacy_state);
    legacy_state.details_overview_expanded = true;
    legacy_state.details_transform_expanded = true;
    legacy_state.details_materials_expanded = true;
    legacy_state.details_authoring_expanded = true;
    {
        size_t group_index;
        for (group_index = 0U;
             group_index < SANDBOX3D_EDITOR_DETAILS_GROUP_COUNT;
             ++group_index)
        {
            legacy_state.details_group_order[group_index] =
                (unsigned char)group_index;
        }
    }
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_reorder_details_group(&legacy_state, 0U, 2U));
    HENKA_TEST_ASSERT(
        sandbox3d_editor_ui_state_store(settings, &legacy_state) ==
            HENKA_SUCCESS);
    sandbox3d_editor_ui_state_load(settings, &state);
    HENKA_TEST_ASSERT(state.details_overview_expanded);
    HENKA_TEST_ASSERT(state.details_transform_expanded);
    HENKA_TEST_ASSERT(state.details_materials_expanded);
    HENKA_TEST_ASSERT(state.details_authoring_expanded);
    HENKA_TEST_ASSERT(
        state.details_group_order[0] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_TRANSFORM);
    HENKA_TEST_ASSERT(
        state.details_group_order[1] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_MATERIALS);
    HENKA_TEST_ASSERT(
        state.details_group_order[2] ==
            SANDBOX3D_EDITOR_DETAILS_GROUP_OVERVIEW);

    henka_settings_destroy(settings);
}
