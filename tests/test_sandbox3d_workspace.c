#include "test_suite.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <henka/persistence.h>

#include "../examples/sandbox3d/interaction_tools.h"
#include "../examples/sandbox3d/studio_environment.h"
#include "../examples/sandbox3d/workspace_persistence.h"
#include "../examples/sandbox3d/workspace_tools.h"

/* HENKA_WORKSPACE_TEST_DOCK_MEMBERSHIP_V1 */
static bool henka_test_workspace_dock_contains_section(
    const sandbox3d_workspace_model* model,
    sandbox3d_workspace_dock_zone dock,
    sandbox3d_workspace_panel_id panel)
{
    size_t index;
    const size_t count =
        sandbox3d_workspace_get_topology_dock_section_count(model, dock);

    for (index = 0U; index < count; ++index)
    {
        if (sandbox3d_workspace_get_topology_dock_section_at(
                model,
                dock,
                index) == panel)
        {
            return true;
        }
    }

    return false;
}

static void henka_test_sandbox3d_workspace_persistence(void)
{
    henka_settings* settings = NULL;
    sandbox3d_workspace_model saved;
    sandbox3d_workspace_model loaded;
    sandbox3d_workspace_model before_rejected_load;

    HENKA_TEST_ASSERT(henka_settings_create(&settings) == HENKA_SUCCESS);
    sandbox3d_workspace_model_reset(&saved);
    HENKA_TEST_ASSERT(sandbox3d_workspace_apply_named_layout(
        &saved,
        SANDBOX3D_WORKSPACE_LAYOUT_SCENE_ASSEMBLY));
    saved.left_dock_width = 416.0f;
    saved.right_dock_width = 544.0f;
    HENKA_TEST_ASSERT(sandbox3d_workspace_save_custom_layout(&saved, "Studio"));
    HENKA_TEST_ASSERT(sandbox3d_workspace_save_custom_layout_slot(
        &saved, 1U, "Assembly"));
    sandbox3d_workspace_persistence_save_panels(&saved, settings);
    sandbox3d_workspace_persistence_save_topology(&saved, settings);
    sandbox3d_workspace_persistence_save_custom_layout(&saved, settings);
    sandbox3d_workspace_persistence_save_custom_layout_slots(&saved, settings);

    sandbox3d_workspace_model_reset(&loaded);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_persistence_load_panels(&loaded, settings));
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_persistence_load_topology(&loaded, settings));
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_persistence_load_custom_layout(&loaded, settings));
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_persistence_load_custom_layout_slots(&loaded, settings));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(loaded.left_dock_width, 416.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(loaded.right_dock_width, 544.0f, 0.0001f);
    HENKA_TEST_ASSERT(loaded.named_layout == saved.named_layout);
    HENKA_TEST_ASSERT(sandbox3d_workspace_has_custom_layout(&loaded));
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_custom_layout_name(&loaded), "Studio") == 0);
    HENKA_TEST_ASSERT(sandbox3d_workspace_has_custom_layout_slot(&loaded, 1U));
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_custom_layout_slot_name(&loaded, 1U),
        "Assembly") == 0);

    before_rejected_load = loaded;
    HENKA_TEST_ASSERT(henka_settings_set_float(
        settings,
        "ui.workspace.left_dock_width",
        1000.0f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(!sandbox3d_workspace_persistence_load_panels(
        &loaded, settings));
    HENKA_TEST_ASSERT(memcmp(
        &loaded,
        &before_rejected_load,
        sizeof(loaded)) == 0);

    henka_settings_destroy(settings);
}

void henka_test_sandbox3d_workspace(void)
{
    volatile size_t context_count = SANDBOX3D_WORK_CONTEXT_COUNT;
    float studio_environment[SANDBOX3D_STUDIO_ENVIRONMENT_PIXEL_COUNT];
    unsigned char ground_texture[SANDBOX3D_GROUND_TEXTURE_PIXEL_COUNT];
    henka_vec4 ground_color;
    const sandbox3d_workspace_panel* panel;
    henka_ui_rect ownership[2];
    henka_ui_rect rect;
    henka_ui_rect controls_panel_bounds;
    henka_ui_rect controls_rect;
    henka_ui_rect floating_rect;
    henka_ui_rect sanitized_floating_rect;
    size_t controls_index;
    size_t panel_index;
    sandbox3d_workspace_topology_layout topology_layout;
    sandbox3d_workspace_topology_layout dock_topology_layout;
    const sandbox3d_workspace_topology_node* topology_root;
    sandbox3d_workspace_context_state context_state;
    sandbox3d_workspace_model context_model_before;
    sandbox3d_workspace_model model;
    sandbox3d_workspace_model tab_model;
    uint32_t stress_seed;
    size_t stress_iteration;
    sandbox3d_workspace_panel_id stress_closed_panel;
    float initial_ratio;
    float history_ratio;
    henka_ui_rect left_splitter;
    henka_ui_rect right_splitter;
    henka_ui_rect visual_splitter;

    henka_test_sandbox3d_workspace_persistence();
    ground_color = sandbox3d_ground_surface_color();
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ground_color.x, 0.035f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ground_color.y, 0.050f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ground_color.z, 0.075f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ground_color.w, 1.0f, 0.0001f);
    ground_color = sandbox3d_debug_grid_color();
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ground_color.x, 0.055f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ground_color.y, 0.075f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ground_color.z, 0.100f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(ground_color.w, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT(sandbox3d_ground_surface_uses_texture());

    memset(ground_texture, 0, sizeof(ground_texture));
    sandbox3d_generate_ground_surface_texture(
        ground_texture,
        SANDBOX3D_GROUND_TEXTURE_PIXEL_COUNT);
    HENKA_TEST_ASSERT(sandbox3d_ground_surface_texture_is_valid(
        ground_texture,
        SANDBOX3D_GROUND_TEXTURE_PIXEL_COUNT));
    HENKA_TEST_ASSERT(ground_texture[0U] !=
        ground_texture[SANDBOX3D_GROUND_TEXTURE_CHANNELS]);
    HENKA_TEST_ASSERT(!sandbox3d_ground_surface_texture_is_valid(
        ground_texture,
        SANDBOX3D_GROUND_TEXTURE_PIXEL_COUNT - 1U));

    sandbox3d_generate_studio_environment(
        studio_environment,
        SANDBOX3D_STUDIO_ENVIRONMENT_PIXEL_COUNT);
    HENKA_TEST_ASSERT(sandbox3d_studio_environment_is_valid(
        studio_environment,
        SANDBOX3D_STUDIO_ENVIRONMENT_PIXEL_COUNT));
    {
        const size_t key_pixel = (SANDBOX3D_STUDIO_ENVIRONMENT_HEIGHT * 7U / 16U) *
            SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH *
            SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS +
            (SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH * 11U / 16U) *
                SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS;
        const size_t neutral_pixel = (SANDBOX3D_STUDIO_ENVIRONMENT_HEIGHT * 7U / 16U) *
            SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH *
            SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS +
            (SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH / 6U) *
                SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS;
        HENKA_TEST_ASSERT(studio_environment[key_pixel + 0U] >
            studio_environment[neutral_pixel + 0U]);
        HENKA_TEST_ASSERT(studio_environment[key_pixel + 0U] >
            studio_environment[key_pixel + 2U]);
    }
    HENKA_TEST_ASSERT(!sandbox3d_studio_environment_is_valid(
        studio_environment,
        SANDBOX3D_STUDIO_ENVIRONMENT_PIXEL_COUNT - 1U));

    left_splitter = sandbox3d_workspace_left_splitter_rect(
        (henka_ui_rect){0.0f, 0.0f, 200.0f, 720.0f},
        (henka_ui_rect){220.0f, 0.0f, 760.0f, 720.0f});
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        left_splitter.width,
        SANDBOX3D_WORKSPACE_DIVIDER_HIT_WIDTH,
        0.0001f);
    visual_splitter = sandbox3d_workspace_splitter_visual_rect(left_splitter);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(visual_splitter.width, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        visual_splitter.x,
        left_splitter.x + (left_splitter.width - visual_splitter.width) * 0.5f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(visual_splitter.y, left_splitter.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(visual_splitter.height, left_splitter.height, 0.0001f);

    right_splitter = sandbox3d_workspace_right_splitter_rect(
        (henka_ui_rect){220.0f, 0.0f, 760.0f, 720.0f},
        (henka_ui_rect){1000.0f, 0.0f, 240.0f, 720.0f});
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        right_splitter.width,
        SANDBOX3D_WORKSPACE_DIVIDER_HIT_WIDTH,
        0.0001f);
    visual_splitter = sandbox3d_workspace_splitter_visual_rect(right_splitter);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(visual_splitter.width, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        visual_splitter.x,
        right_splitter.x + (right_splitter.width - visual_splitter.width) * 0.5f,
        0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(visual_splitter.y, right_splitter.y, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(visual_splitter.height, right_splitter.height, 0.0001f);

    visual_splitter = sandbox3d_workspace_splitter_visual_rect(
        (henka_ui_rect){100.0f, 40.0f, 320.0f, 10.0f});
    HENKA_TEST_ASSERT_FLOAT_CLOSE(visual_splitter.x, 100.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(visual_splitter.width, 320.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(visual_splitter.y, 44.5f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(visual_splitter.height, 1.0f, 0.0001f);

    visual_splitter = sandbox3d_workspace_splitter_visual_rect(
        (henka_ui_rect){NAN, 0.0f, 10.0f, 100.0f});
    HENKA_TEST_ASSERT(
        visual_splitter.x == 0.0f && visual_splitter.y == 0.0f &&
        visual_splitter.width == 0.0f && visual_splitter.height == 0.0f);
    visual_splitter = sandbox3d_workspace_splitter_visual_rect(
        (henka_ui_rect){0.0f, 0.0f, 0.5f, 100.0f});
    HENKA_TEST_ASSERT(
        visual_splitter.x == 0.0f && visual_splitter.y == 0.0f &&
        visual_splitter.width == 0.0f && visual_splitter.height == 0.0f);
    visual_splitter = sandbox3d_workspace_splitter_visual_rect(
        (henka_ui_rect){0.0f, 0.0f, 100.0f, 0.5f});
    HENKA_TEST_ASSERT(
        visual_splitter.x == 0.0f && visual_splitter.y == 0.0f &&
        visual_splitter.width == 0.0f && visual_splitter.height == 0.0f);

    HENKA_TEST_ASSERT(
        sandbox3d_workspace_splitter_border_mask(false, false) ==
        HENKA_UI_BORDER_ALL);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_splitter_border_mask(true, false) ==
        (HENKA_UI_BORDER_ALL & ~HENKA_UI_BORDER_LEFT));
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_splitter_border_mask(false, true) ==
        (HENKA_UI_BORDER_ALL & ~HENKA_UI_BORDER_RIGHT));
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_splitter_border_mask(true, true) ==
        (HENKA_UI_BORDER_ALL &
         ~HENKA_UI_BORDER_LEFT &
         ~HENKA_UI_BORDER_RIGHT));

    /* HENKA_WORK_CONTEXT_TEST_V1 */
    sandbox3d_workspace_context_state_reset(&context_state);
    HENKA_TEST_ASSERT(
        context_state.active == SANDBOX3D_WORK_CONTEXT_BUILD);
    HENKA_TEST_ASSERT(!context_state.debug_hud_visible);
    HENKA_TEST_ASSERT(context_count == 3U);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_panel_name(
            SANDBOX3D_WORKSPACE_PANEL_CONTROLS),
        "Tools") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_context_label(
            SANDBOX3D_WORK_CONTEXT_BUILD),
        "Build") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_context_label(
            SANDBOX3D_WORK_CONTEXT_GAME),
        "Game") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_context_label(
            SANDBOX3D_WORK_CONTEXT_WORLD),
        "World") == 0);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_parse_work_context("build") ==
        SANDBOX3D_WORK_CONTEXT_BUILD);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_parse_work_context("game") ==
        SANDBOX3D_WORK_CONTEXT_GAME);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_parse_work_context("world") ==
        SANDBOX3D_WORK_CONTEXT_WORLD);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_parse_work_context("debug") ==
        SANDBOX3D_WORK_CONTEXT_BUILD);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_parse_work_context("animate") ==
        SANDBOX3D_WORK_CONTEXT_BUILD);
    HENKA_TEST_ASSERT(
        !sandbox3d_workspace_context_is_valid(
            (sandbox3d_workspace_work_context)-1));
    HENKA_TEST_ASSERT(
        !sandbox3d_workspace_context_is_valid(
            SANDBOX3D_WORK_CONTEXT_COUNT));

    sandbox3d_workspace_model_reset(&model);
    context_model_before = model;
    context_state.debug_hud_visible = true;
    HENKA_TEST_ASSERT(sandbox3d_workspace_context_set(
        &context_state,
        SANDBOX3D_WORK_CONTEXT_GAME));
    HENKA_TEST_ASSERT(
        context_state.active == SANDBOX3D_WORK_CONTEXT_GAME);
    HENKA_TEST_ASSERT(context_state.debug_hud_visible);
    HENKA_TEST_ASSERT(
        memcmp(
            &model,
            &context_model_before,
            sizeof(model)) == 0);
    HENKA_TEST_ASSERT(sandbox3d_workspace_context_set(
        &context_state,
        SANDBOX3D_WORK_CONTEXT_WORLD));
    HENKA_TEST_ASSERT(
        context_state.active == SANDBOX3D_WORK_CONTEXT_WORLD);
    HENKA_TEST_ASSERT(context_state.debug_hud_visible);
    HENKA_TEST_ASSERT(
        !sandbox3d_workspace_context_set(
            &context_state,
            SANDBOX3D_WORK_CONTEXT_COUNT));
    HENKA_TEST_ASSERT(
        context_state.active == SANDBOX3D_WORK_CONTEXT_WORLD);
    sandbox3d_workspace_model_reset(&model);
    for (panel_index = 0U;
         panel_index < SANDBOX3D_WORKSPACE_PANEL_COUNT;
         ++panel_index)
    {
        panel = sandbox3d_workspace_get_panel_const(
            &model,
            (sandbox3d_workspace_panel_id)panel_index);
        HENKA_TEST_ASSERT(panel != NULL);
        HENKA_TEST_ASSERT(
            panel->floating_rect.width >= panel->minimum_width);
        HENKA_TEST_ASSERT(
            panel->floating_rect.height >= panel->minimum_height);
        HENKA_TEST_ASSERT(
            sandbox3d_workspace_sanitize_floating_rect(
                panel,
                panel->floating_rect,
                &sanitized_floating_rect));
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            sanitized_floating_rect.width,
            panel->floating_rect.width,
            0.0001f);
        HENKA_TEST_ASSERT_FLOAT_CLOSE(
            sanitized_floating_rect.height,
            panel->floating_rect.height,
            0.0001f);
    }

    panel = sandbox3d_workspace_get_panel_const(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_UTILITY);
    HENKA_TEST_ASSERT(panel != NULL);
    floating_rect = panel->floating_rect;
    floating_rect.height = 560.0f;
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_sanitize_floating_rect(
            panel,
            floating_rect,
            &sanitized_floating_rect));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sanitized_floating_rect.height,
        panel->minimum_height,
        0.0001f);
    floating_rect = panel->floating_rect;
    floating_rect.width = NAN;
    HENKA_TEST_ASSERT(
        !sandbox3d_workspace_sanitize_floating_rect(
            panel,
            floating_rect,
            &sanitized_floating_rect));
    floating_rect = panel->floating_rect;
    floating_rect.height = 5000.0f;
    HENKA_TEST_ASSERT(
        !sandbox3d_workspace_sanitize_floating_rect(
            panel,
            floating_rect,
            &sanitized_floating_rect));

    HENKA_TEST_ASSERT(
        !sandbox3d_workspace_should_draw_section_tabs(0U));
    HENKA_TEST_ASSERT(
        !sandbox3d_workspace_should_draw_section_tabs(1U));
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_should_draw_section_tabs(2U));
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_should_draw_section_tabs(
            SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS));
    HENKA_TEST_ASSERT(
        !sandbox3d_workspace_should_draw_section_tabs(
            SANDBOX3D_WORKSPACE_TOPOLOGY_MAX_TABS + 1U));

    HENKA_TEST_ASSERT(
        sandbox3d_workspace_clamp_controls_page(-1) ==
        SANDBOX3D_CONTROLS_PAGE_MAIN);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_clamp_controls_page(
            SANDBOX3D_CONTROLS_PAGE_MAIN) ==
        SANDBOX3D_CONTROLS_PAGE_MAIN);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_clamp_controls_page(
            SANDBOX3D_CONTROLS_PAGE_CAMERA_STATUS) ==
        SANDBOX3D_CONTROLS_PAGE_CAMERA_STATUS);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_clamp_controls_page(
            SANDBOX3D_CONTROLS_PAGE_QA) ==
        SANDBOX3D_CONTROLS_PAGE_QA);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_clamp_controls_page(99) ==
        SANDBOX3D_CONTROLS_PAGE_QA);

    controls_panel_bounds =
        (henka_ui_rect){16.0f, 16.0f, 320.0f, 180.0f};
    for (controls_index = 0U;
         controls_index < SANDBOX3D_WORKSPACE_CONTROLS_PAGE_COUNT;
         ++controls_index)
    {
        controls_rect =
            sandbox3d_workspace_controls_page_tab_rect(
                controls_panel_bounds,
                controls_index);
        HENKA_TEST_ASSERT(
            sandbox3d_workspace_rect_contains_rect(
                controls_panel_bounds,
                controls_rect));
    }
    for (controls_index = 0U;
         controls_index <
            SANDBOX3D_WORKSPACE_CONTROLS_QA_ACTION_COUNT;
         ++controls_index)
    {
        controls_rect =
            sandbox3d_workspace_controls_qa_action_rect(
                controls_panel_bounds,
                controls_index);
        HENKA_TEST_ASSERT(
            sandbox3d_workspace_rect_contains_rect(
                controls_panel_bounds,
                controls_rect));
    }
    HENKA_TEST_ASSERT(
        !sandbox3d_workspace_rect_contains_rect(
            controls_panel_bounds,
            (henka_ui_rect){30.0f, 196.0f, 292.0f, 28.0f}));

    HENKA_TEST_ASSERT(sandbox3d_workspace_get_named_layout(&model) == SANDBOX3D_WORKSPACE_LAYOUT_DEFAULT);
    HENKA_TEST_ASSERT(!sandbox3d_workspace_has_custom_layout(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_closed_section_count(&model) == 0U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_closed_section_at(&model, 0U) == SANDBOX3D_WORKSPACE_PANEL_NONE);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_named_layout_label(SANDBOX3D_WORKSPACE_LAYOUT_MODELING),
        "Modeling") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_named_layout_label(
            SANDBOX3D_WORKSPACE_LAYOUT_MINIMAL_VIEWPORT),
        "Focus Viewport") == 0);
    HENKA_TEST_ASSERT(sandbox3d_workspace_parse_named_layout("materials") == SANDBOX3D_WORKSPACE_LAYOUT_MATERIALS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_parse_named_layout("scene_assembly") == SANDBOX3D_WORKSPACE_LAYOUT_SCENE_ASSEMBLY);
    HENKA_TEST_ASSERT(sandbox3d_workspace_parse_named_layout("minimal_viewport") == SANDBOX3D_WORKSPACE_LAYOUT_MINIMAL_VIEWPORT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_apply_named_layout(
        &model, SANDBOX3D_WORKSPACE_LAYOUT_MODELING));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_named_layout(&model) == SANDBOX3D_WORKSPACE_LAYOUT_MODELING);
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_apply_named_layout(
        &model, SANDBOX3D_WORKSPACE_LAYOUT_SCENE_ASSEMBLY));
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_apply_named_layout(
        &model, SANDBOX3D_WORKSPACE_LAYOUT_MINIMAL_VIEWPORT));
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_save_custom_layout(&model, "Studio"));
    HENKA_TEST_ASSERT(sandbox3d_workspace_has_custom_layout(&model));
    HENKA_TEST_ASSERT(strcmp(sandbox3d_workspace_custom_layout_name(&model), "Studio") == 0);
    HENKA_TEST_ASSERT(sandbox3d_workspace_save_custom_layout_slot(&model, 1U, "Assembly"));
    HENKA_TEST_ASSERT(sandbox3d_workspace_save_custom_layout_slot(&model, 2U, "Review"));
    HENKA_TEST_ASSERT(sandbox3d_workspace_has_custom_layout_slot(&model, 1U));
    HENKA_TEST_ASSERT(sandbox3d_workspace_has_custom_layout_slot(&model, 2U));
    HENKA_TEST_ASSERT(strcmp(sandbox3d_workspace_custom_layout_slot_name(&model, 1U), "Assembly") == 0);
    HENKA_TEST_ASSERT(strcmp(sandbox3d_workspace_custom_layout_slot_name(&model, 2U), "Review") == 0);
    HENKA_TEST_ASSERT(!sandbox3d_workspace_save_custom_layout(&model, "bad\nname"));
    HENKA_TEST_ASSERT(sandbox3d_workspace_apply_named_layout(&model, SANDBOX3D_WORKSPACE_LAYOUT_MATERIALS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_apply_custom_layout(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_named_layout(&model) == SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM);
    HENKA_TEST_ASSERT(sandbox3d_workspace_apply_named_layout(&model, SANDBOX3D_WORKSPACE_LAYOUT_MODELING));
    HENKA_TEST_ASSERT(sandbox3d_workspace_can_undo(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_undo(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_named_layout(&model) == SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM);
    HENKA_TEST_ASSERT(sandbox3d_workspace_redo(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_named_layout(&model) == SANDBOX3D_WORKSPACE_LAYOUT_MODELING);
    HENKA_TEST_ASSERT(sandbox3d_workspace_apply_custom_layout_slot(&model, 1U));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_named_layout(&model) == SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM);
    sandbox3d_workspace_reset_layout(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_has_custom_layout_slot(&model, 1U));
    HENKA_TEST_ASSERT(sandbox3d_workspace_apply_custom_layout_slot(&model, 2U));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_named_layout(&model) == SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM);
    HENKA_TEST_ASSERT(sandbox3d_workspace_apply_named_layout(&model, SANDBOX3D_WORKSPACE_LAYOUT_MODELING));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(
        &model, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS)->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS)->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(
        &model, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS)->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    sandbox3d_workspace_begin_topology_transaction(&model);
    sandbox3d_workspace_rollback_topology_transaction(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_named_layout(&model) == SANDBOX3D_WORKSPACE_LAYOUT_MODELING);
    sandbox3d_workspace_begin_topology_transaction(&model);
    sandbox3d_workspace_commit_topology_transaction(&model);
    history_ratio = sandbox3d_workspace_topology_get_node_const(
        &model,
        model.topology_root)->data.split.ratio;
    HENKA_TEST_ASSERT(history_ratio > 0.50f);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_named_layout(&model) == SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM);
    HENKA_TEST_ASSERT(sandbox3d_workspace_undo(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_named_layout(&model) == SANDBOX3D_WORKSPACE_LAYOUT_MODELING);
    HENKA_TEST_ASSERT(sandbox3d_workspace_redo(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_named_layout(&model) == SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM);
    sandbox3d_workspace_reset_layout(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_has_custom_layout(&model));
    HENKA_TEST_ASSERT(strcmp(sandbox3d_workspace_custom_layout_name(&model), "Studio") == 0);
    initial_ratio = sandbox3d_workspace_topology_get_node_const(
        &model, model.topology_root)->data.split.ratio;
    sandbox3d_workspace_begin_divider_drag(&model, model.topology_root, (henka_vec2){640.0f, 360.0f});
    sandbox3d_workspace_update_divider_drag(
        &model,
        (henka_vec2){760.0f, 360.0f},
        (henka_ui_rect){0.0f, 0.0f, 1280.0f, 720.0f});
    sandbox3d_workspace_commit_topology_transaction(&model);
    history_ratio = sandbox3d_workspace_topology_get_node_const(
        &model, model.topology_root)->data.split.ratio;
    HENKA_TEST_ASSERT(sandbox3d_workspace_can_undo(&model));
    HENKA_TEST_ASSERT(!sandbox3d_workspace_can_redo(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_undo(&model));
    HENKA_TEST_ASSERT(!sandbox3d_workspace_can_undo(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_can_redo(&model));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_workspace_topology_get_node_const(&model, model.topology_root)->data.split.ratio,
        initial_ratio,
        0.0001f);
    HENKA_TEST_ASSERT(sandbox3d_workspace_redo(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_can_undo(&model));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(
        sandbox3d_workspace_topology_get_node_const(&model, model.topology_root)->data.split.ratio,
        history_ratio,
        0.0001f);
    sandbox3d_workspace_model_reset(&model);
    topology_root = sandbox3d_workspace_topology_get_node_const(&model, model.topology_root);
    HENKA_TEST_ASSERT(topology_root != NULL);
    HENKA_TEST_ASSERT(topology_root->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT);
    HENKA_TEST_ASSERT(topology_root->data.split.first_child != topology_root->data.split.second_child);
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_section_has_tab(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, SANDBOX3D_WORKSPACE_PANEL_CONTROLS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_dock_section_count(
        &model, SANDBOX3D_WORKSPACE_DOCK_LEFT) == 2U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_dock_section_at(
        &model, SANDBOX3D_WORKSPACE_DOCK_LEFT, 0U) == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_dock_section_at(
        &model, SANDBOX3D_WORKSPACE_DOCK_LEFT, 1U) == SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_dock_section_count(
        &model, SANDBOX3D_WORKSPACE_DOCK_RIGHT) == 2U);
    sandbox3d_workspace_build_topology_layout(
        &model,
        (henka_ui_rect){0.0f, 0.0f, 1280.0f, 720.0f},
        &topology_layout);
    HENKA_TEST_ASSERT(topology_layout.divider_count == 3U);
    HENKA_TEST_ASSERT(topology_layout.section_rects[SANDBOX3D_WORKSPACE_PANEL_CONTROLS].width >= 180.0f);
    HENKA_TEST_ASSERT(topology_layout.section_rects[SANDBOX3D_WORKSPACE_PANEL_UTILITY].height >= 180.0f);
    HENKA_TEST_ASSERT(topology_layout.divider_hit_rects[0].width == 10.0f);
    sandbox3d_workspace_set_ui_scale(&model, 2.0f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(sandbox3d_workspace_get_ui_scale(&model), 2.0f, 0.0001f);
    sandbox3d_workspace_build_topology_layout(
        &model,
        (henka_ui_rect){0.0f, 0.0f, 1280.0f, 720.0f},
        &topology_layout);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(topology_layout.divider_hit_rects[0].width, 20.0f, 0.0001f);
    sandbox3d_workspace_set_ui_scale(&model, 1.0f);
    {
        const float dpi_scales[] = {0.5f, 0.75f, 1.25f, 1.5f, 3.0f, 5.0f};
        size_t dpi_index;
        for (dpi_index = 0U; dpi_index < sizeof(dpi_scales) / sizeof(dpi_scales[0]); ++dpi_index)
        {
            const float expected_scale = dpi_scales[dpi_index] < SANDBOX3D_WORKSPACE_UI_SCALE_MIN
                ? SANDBOX3D_WORKSPACE_UI_SCALE_MIN
                : (dpi_scales[dpi_index] > SANDBOX3D_WORKSPACE_UI_SCALE_MAX
                    ? SANDBOX3D_WORKSPACE_UI_SCALE_MAX
                    : dpi_scales[dpi_index]);
            sandbox3d_workspace_set_ui_scale(&model, dpi_scales[dpi_index]);
            sandbox3d_workspace_build_topology_layout(
                &model,
                (henka_ui_rect){0.0f, 0.0f, 1280.0f, 720.0f},
                &topology_layout);
            HENKA_TEST_ASSERT_FLOAT_CLOSE(
                sandbox3d_workspace_get_ui_scale(&model),
                expected_scale,
                0.0001f);
            HENKA_TEST_ASSERT(topology_layout.divider_count == 3U);
            HENKA_TEST_ASSERT(topology_layout.divider_hit_rects[0].width >= 10.0f * expected_scale);
            HENKA_TEST_ASSERT(topology_layout.section_rects[SANDBOX3D_WORKSPACE_PANEL_CONTROLS].width >= 180.0f);
            HENKA_TEST_ASSERT(topology_layout.section_rects[SANDBOX3D_WORKSPACE_PANEL_UTILITY].height >= 180.0f);
        }
    }
    sandbox3d_workspace_set_ui_scale(&model, 1.0f);
    sandbox3d_workspace_build_dock_topology_layout(
        &model,
        SANDBOX3D_WORKSPACE_DOCK_LEFT,
        (henka_ui_rect){0.0f, 0.0f, 320.0f, 680.0f},
        &dock_topology_layout);
    HENKA_TEST_ASSERT(dock_topology_layout.divider_count == 1U);
    HENKA_TEST_ASSERT(dock_topology_layout.section_rects[SANDBOX3D_WORKSPACE_PANEL_CONTROLS].height >= 180.0f);
    HENKA_TEST_ASSERT(dock_topology_layout.section_rects[SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS].height >= 180.0f);
    HENKA_TEST_ASSERT(dock_topology_layout.divider_node_indices[0] == topology_root->data.split.first_child);
    HENKA_TEST_ASSERT(dock_topology_layout.divider_hit_rects[0].height == 10.0f);
    sandbox3d_workspace_begin_divider_drag(&model, model.topology_root, (henka_vec2){640.0f, 360.0f});
    sandbox3d_workspace_update_divider_drag(&model, (henka_vec2){760.0f, 360.0f}, (henka_ui_rect){0.0f, 0.0f, 1280.0f, 720.0f});
    HENKA_TEST_ASSERT(topology_root->data.split.ratio > 0.5f);
    sandbox3d_workspace_rollback_topology_transaction(&model);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(topology_root->data.split.ratio, 0.5f, 0.0001f);
    sandbox3d_workspace_begin_divider_drag(&model, model.topology_root, (henka_vec2){640.0f, 360.0f});
    sandbox3d_workspace_update_divider_drag(
        &model,
        (henka_vec2){760.0f, 360.0f},
        (henka_ui_rect){0.0f, 0.0f, 1280.0f, 720.0f});
    sandbox3d_workspace_equalize_divider(&model, model.topology_root);
    sandbox3d_workspace_commit_topology_transaction(&model);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(topology_root->data.split.ratio, 0.5f, 0.0001f);
    {
        const sandbox3d_workspace_topology_node* left_split = sandbox3d_workspace_topology_get_node_const(
            &model,
            topology_root->data.split.first_child);
        const sandbox3d_workspace_panel_id collapsing_section =
            sandbox3d_workspace_get_topology_dock_section_at(
                &model,
                SANDBOX3D_WORKSPACE_DOCK_LEFT,
                0U);
        HENKA_TEST_ASSERT(left_split != NULL);
        HENKA_TEST_ASSERT(collapsing_section != SANDBOX3D_WORKSPACE_PANEL_NONE);
        HENKA_TEST_ASSERT(left_split->type == SANDBOX3D_WORKSPACE_TOPOLOGY_NODE_SPLIT);
        sandbox3d_workspace_begin_divider_drag(
            &model,
            topology_root->data.split.first_child,
            (henka_vec2){320.0f, 360.0f});
        sandbox3d_workspace_update_divider_drag(
            &model,
            (henka_vec2){320.0f, 0.0f},
            (henka_ui_rect){0.0f, 0.0f, 1280.0f, 720.0f});
        HENKA_TEST_ASSERT(sandbox3d_workspace_divider_close_preview(&model));
        HENKA_TEST_ASSERT(
            sandbox3d_workspace_divider_close_section(&model) == collapsing_section);
        sandbox3d_workspace_end_interaction(&model);
        HENKA_TEST_ASSERT(
            sandbox3d_workspace_section_is_closed(&model, collapsing_section));
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_restore_last_closed_section(&model));
    }
    sandbox3d_workspace_model_reset(&tab_model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_merge_sections(
        &tab_model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_tab_count(
        &tab_model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) == 2U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_cycle_topology_section_tab(
        &tab_model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, 1));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_active_tab(
        &tab_model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_cycle_topology_section_tab(
        &tab_model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, 1));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_active_tab(
        &tab_model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) == SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_close_active_tab(
        &tab_model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_tab_count(
        &tab_model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_restore_last_closed_section(&tab_model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_tab_count(
        &tab_model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) == 2U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_close_section(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_closed_section_count(&model) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_closed_section_at(&model, 0U) == SANDBOX3D_WORKSPACE_PANEL_UTILITY);
    model.section_chooser_selected_index = 0U;
    HENKA_TEST_ASSERT(sandbox3d_workspace_cycle_section_chooser_selection(&model, 1));
    HENKA_TEST_ASSERT(model.section_chooser_selected_index == 0U);
    HENKA_TEST_ASSERT(!sandbox3d_workspace_cycle_section_chooser_selection(&model, 0));
    sandbox3d_workspace_model_reset(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_close_section(&model, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_close_section(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_closed_section_count(&model) == 2U);
    model.section_chooser_selected_index = 0U;
    HENKA_TEST_ASSERT(sandbox3d_workspace_cycle_section_chooser_selection(&model, 1));
    HENKA_TEST_ASSERT(model.section_chooser_selected_index == 1U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_cycle_section_chooser_selection(&model, 1));
    HENKA_TEST_ASSERT(model.section_chooser_selected_index == 0U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_closed_section_at(&model, 1U) == SANDBOX3D_WORKSPACE_PANEL_UTILITY);
    HENKA_TEST_ASSERT(sandbox3d_workspace_split_section(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_SPLIT_VERTICAL,
        SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(!sandbox3d_workspace_section_is_closed(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_dock_section_count(
        &model, SANDBOX3D_WORKSPACE_DOCK_LEFT) == 3U);
    HENKA_TEST_ASSERT(henka_test_workspace_dock_contains_section(
        &model,
        SANDBOX3D_WORKSPACE_DOCK_LEFT,
        SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(!sandbox3d_workspace_split_section(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL,
        SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_context_command_label(SANDBOX3D_WORKSPACE_CONTEXT_OPEN_HORIZONTAL),
        "Open a horizontal window") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_context_command_label(SANDBOX3D_WORKSPACE_CONTEXT_OPEN_VERTICAL),
        "Open a vertical window") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_context_command_label(SANDBOX3D_WORKSPACE_CONTEXT_CLOSE_SECTION),
        "Close this section") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_context_command_label(SANDBOX3D_WORKSPACE_CONTEXT_MERGE_ADJACENT),
        "Merge with adjacent section") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_context_command_label(SANDBOX3D_WORKSPACE_CONTEXT_EQUALIZE),
        "Equalize sections") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_context_command_label(SANDBOX3D_WORKSPACE_CONTEXT_MAXIMIZE),
        "Maximize / Restore section") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_context_command_label(SANDBOX3D_WORKSPACE_CONTEXT_DETACH),
        "Detach section") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_context_command_label(SANDBOX3D_WORKSPACE_CONTEXT_MOVE_TO_TAB_GROUP),
        "Move to tab group") == 0);
    HENKA_TEST_ASSERT(strcmp(
        sandbox3d_workspace_context_command_label(SANDBOX3D_WORKSPACE_CONTEXT_RESTORE_LAST_CLOSED),
        "Restore last closed section") == 0);
    HENKA_TEST_ASSERT(sandbox3d_workspace_close_section(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(sandbox3d_workspace_section_is_closed(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_restore_last_closed_section(&model));
    HENKA_TEST_ASSERT(!sandbox3d_workspace_section_is_closed(&model, SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(sandbox3d_workspace_merge_sections(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_section_is_closed(&model, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_section_has_tab(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_for_tab(
        &model, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS) == SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_tab_count(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) == 2U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_tab_at(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, 1U) == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_active_tab(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) == SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_close_section(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_section_is_closed(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_section_is_closed(
        &model, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_for_tab(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) == SANDBOX3D_WORKSPACE_PANEL_NONE);
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_restore_last_closed_section(&model));
    HENKA_TEST_ASSERT(!sandbox3d_workspace_section_is_closed(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_section_is_closed(
        &model, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_tab_count(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) == 2U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_set_topology_section_active_tab(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_active_tab(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(!sandbox3d_workspace_set_topology_section_active_tab(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, SANDBOX3D_WORKSPACE_PANEL_UTILITY));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_dock_section_count(
        &model, SANDBOX3D_WORKSPACE_DOCK_LEFT) == 2U);
    HENKA_TEST_ASSERT(henka_test_workspace_dock_contains_section(
        &model,
        SANDBOX3D_WORKSPACE_DOCK_LEFT,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_dock_section_at(
        &model, SANDBOX3D_WORKSPACE_DOCK_LEFT, 2U) == SANDBOX3D_WORKSPACE_PANEL_NONE);
    HENKA_TEST_ASSERT(sandbox3d_workspace_reorder_tab(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS,
        0U));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_tab_at(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, 0U) == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_active_tab(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_begin_tab_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    sandbox3d_workspace_update_tab_drag(&model, 1U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_commit_tab_drag(&model));
    sandbox3d_workspace_end_interaction(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_tab_at(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, 1U) == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_begin_tab_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    sandbox3d_workspace_update_tab_drag(&model, 0U);
    sandbox3d_workspace_cancel_tab_drag(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_tab_at(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS, 1U) == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    sandbox3d_workspace_model_reset(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_merge_sections(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_move_tab(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_for_tab(
        &model, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS) == SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_tab_count(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) == 1U);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_topology_section_active_tab(
        &model, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS) == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_move_tab(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_section_is_closed(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_restore_last_closed_section(&model));
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));

    sandbox3d_workspace_model_reset(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_merge_sections(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_begin_tab_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_extract_tab_for_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS,
        (henka_ui_rect){16.0f, 16.0f, 320.0f, 560.0f},
        (henka_vec2){420.0f, 120.0f},
        1280,
        720));
    HENKA_TEST_ASSERT(
        model.active_drag_panel == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_topology_section_for_tab(
            &model,
            SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS) ==
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    sandbox3d_workspace_rollback_topology_transaction(&model);
    sandbox3d_workspace_end_interaction(&model);
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_topology_section_for_tab(
            &model,
            SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS) ==
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));

    sandbox3d_workspace_model_reset(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_merge_sections(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_set_topology_section_active_tab(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
    HENKA_TEST_ASSERT(sandbox3d_workspace_split_active_tab(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_SPLIT_VERTICAL));
    HENKA_TEST_ASSERT(
        sandbox3d_workspace_get_topology_section_for_tab(
            &model,
            SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS) ==
        SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));

    sandbox3d_workspace_set_maximized_section(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    sandbox3d_workspace_build_topology_layout(
        &model,
        (henka_ui_rect){0.0f, 0.0f, 640.0f, 360.0f},
        &topology_layout);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(topology_layout.section_rects[SANDBOX3D_WORKSPACE_PANEL_CONTROLS].width, 640.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(topology_layout.section_rects[SANDBOX3D_WORKSPACE_PANEL_CONTROLS].height, 360.0f, 0.0001f);
    sandbox3d_workspace_restore_maximized_section(&model);
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
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.right_dock_width, 540.0f, 0.0001f);

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

    sandbox3d_workspace_dock_panel(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        SANDBOX3D_WORKSPACE_DOCK_LEFT);
    sandbox3d_workspace_begin_docked_panel_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        rect,
        (henka_vec2){rect.x + 12.0f, rect.y + 10.0f},
        1280,
        720);
    sandbox3d_workspace_cancel_panel_drag(&model);
    sandbox3d_workspace_end_interaction(&model);
    HENKA_TEST_ASSERT(sandbox3d_workspace_get_panel_const(
        &model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS)->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT);

    sandbox3d_workspace_begin_docked_panel_drag(
        &model,
        SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
        rect,
        (henka_vec2){rect.x + 12.0f, rect.y + 10.0f},
        1280,
        720);
    sandbox3d_workspace_end_interaction(&model);

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

    /*
     * Resize clamp contract: use an explicit 356px opposing dock so the
     * 1280px frame leaves a 334px maximum for the left dock:
     * 1280 - 520 scene - 356 opposing dock - 70 chrome = 334.
     */
    model.right_dock_width = 356.0f;
    sandbox3d_workspace_begin_dock_resize(
        &model,
        SANDBOX3D_WORKSPACE_RESIZE_LEFT_DOCK,
        (henka_vec2){320.0f, 200.0f});
    sandbox3d_workspace_update_dock_resize(&model, (henka_vec2){370.0f, 200.0f}, 1280, 520.0f, 300.0f, model.right_dock_width);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.left_dock_width, 334.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(model.right_dock_width, 356.0f, 0.0001f);
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

    stress_seed = UINT32_C(0x4E4B4155);
    for (stress_iteration = 0U; stress_iteration < 32U; ++stress_iteration)
    {
        sandbox3d_workspace_model_reset(&model);
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
        HENKA_TEST_ASSERT(model.keyboard_focus_panel == SANDBOX3D_WORKSPACE_PANEL_NONE);

        stress_seed = stress_seed * UINT32_C(1664525) + UINT32_C(1013904223);
        stress_closed_panel =
            (sandbox3d_workspace_panel_id)(stress_seed % SANDBOX3D_WORKSPACE_PANEL_COUNT);
        HENKA_TEST_ASSERT(sandbox3d_workspace_close_section(
            &model,
            stress_closed_panel));
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
        HENKA_TEST_ASSERT(sandbox3d_workspace_restore_last_closed_section(&model));
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
        HENKA_TEST_ASSERT(sandbox3d_workspace_close_section(
            &model,
            stress_closed_panel));
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));

        stress_seed = stress_seed * UINT32_C(1664525) + UINT32_C(1013904223);
        {
            const sandbox3d_workspace_panel_id split_source =
                stress_closed_panel == SANDBOX3D_WORKSPACE_PANEL_CONTROLS
                    ? SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS
                    : SANDBOX3D_WORKSPACE_PANEL_CONTROLS;
            HENKA_TEST_ASSERT(sandbox3d_workspace_split_section(
                &model,
                split_source,
                stress_seed & 1U
                    ? SANDBOX3D_WORKSPACE_SPLIT_HORIZONTAL
                    : SANDBOX3D_WORKSPACE_SPLIT_VERTICAL,
                stress_closed_panel));
            HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
        }

        sandbox3d_workspace_begin_topology_transaction(&model);
        sandbox3d_workspace_begin_divider_drag(&model, model.topology_root, (henka_vec2){640.0f, 360.0f});
        sandbox3d_workspace_update_divider_drag(
            &model,
            (henka_vec2){480.0f + (float)(stress_iteration % 7U) * 24.0f, 360.0f},
            (henka_ui_rect){0.0f, 0.0f, 1280.0f, 720.0f});
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
        sandbox3d_workspace_rollback_topology_transaction(&model);
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));

        HENKA_TEST_ASSERT(sandbox3d_workspace_merge_sections(
            &model,
            SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
            SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
        HENKA_TEST_ASSERT(sandbox3d_workspace_cycle_topology_section_tab(
            &model,
            SANDBOX3D_WORKSPACE_PANEL_CONTROLS,
            stress_iteration & 1U ? -1 : 1));
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));

        sandbox3d_workspace_set_maximized_section(&model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
        sandbox3d_workspace_restore_maximized_section(&model);
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
        HENKA_TEST_ASSERT(sandbox3d_workspace_apply_named_layout(
            &model,
            (sandbox3d_workspace_named_layout)(stress_iteration % SANDBOX3D_WORKSPACE_LAYOUT_CUSTOM)));
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
        HENKA_TEST_ASSERT(sandbox3d_workspace_save_custom_layout(&model, "Stress"));
        HENKA_TEST_ASSERT(sandbox3d_workspace_apply_custom_layout(&model));
        HENKA_TEST_ASSERT(sandbox3d_workspace_topology_is_valid(&model));
    }
}
