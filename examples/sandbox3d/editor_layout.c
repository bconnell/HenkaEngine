#include "editor_layout.h"

#include <henka/workspace.h>

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS 64U
#define SANDBOX3D_EDITOR_LAYOUT_MAX_FRAMEBUFFER_DIMENSION 32768
#define SANDBOX3D_EDITOR_LAYOUT_DEFAULT_PANEL_HEIGHT 360.0f
#define SANDBOX3D_EDITOR_LAYOUT_DEFAULT_CONTROLS_WIDTH 320.0f
#define SANDBOX3D_EDITOR_LAYOUT_DEFAULT_SCENE_OBJECTS_WIDTH 260.0f
#define SANDBOX3D_EDITOR_LAYOUT_DEFAULT_DETAILS_WIDTH 352.0f
#define SANDBOX3D_EDITOR_LAYOUT_DEFAULT_UTILITY_HEIGHT 228.0f
#define SANDBOX3D_EDITOR_LAYOUT_DEBUG_STRIP_HEIGHT 58.0f

static bool sandbox3d_editor_layout_float_is_valid(float value)
{
    return isfinite((double)value) != 0;
}

sandbox3d_editor_layout_breakpoint sandbox3d_editor_layout_breakpoint_for_width(
    int framebuffer_width)
{
    if (framebuffer_width < 1200)
    {
        return SANDBOX3D_EDITOR_LAYOUT_NARROW;
    }
    if (framebuffer_width < 1600)
    {
        return SANDBOX3D_EDITOR_LAYOUT_MEDIUM;
    }
    return SANDBOX3D_EDITOR_LAYOUT_WIDE;
}

henka_result sandbox3d_editor_layout_metrics_for_framebuffer(
    int framebuffer_width,
    int framebuffer_height,
    sandbox3d_editor_layout_metrics* out_metrics)
{
    sandbox3d_editor_layout_breakpoint breakpoint;
    sandbox3d_editor_layout_metrics metrics;

    if (out_metrics == NULL || framebuffer_width <= 0 || framebuffer_height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (framebuffer_width > SANDBOX3D_EDITOR_LAYOUT_MAX_FRAMEBUFFER_DIMENSION ||
        framebuffer_height > SANDBOX3D_EDITOR_LAYOUT_MAX_FRAMEBUFFER_DIMENSION)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    breakpoint = sandbox3d_editor_layout_breakpoint_for_width(framebuffer_width);
    memset(&metrics, 0, sizeof(metrics));
    metrics.breakpoint = breakpoint;
    metrics.minimum_hit_target = 32.0f;

    switch (breakpoint)
    {
        case SANDBOX3D_EDITOR_LAYOUT_NARROW:
            metrics.outer_margin = 12.0f;
            metrics.panel_gap = 8.0f;
            metrics.toolbar_height = 40.0f;
            metrics.sidebar_width = 220.0f;
            metrics.utility_width = 244.0f;
            metrics.stack_sidebars = true;
            break;
        case SANDBOX3D_EDITOR_LAYOUT_MEDIUM:
            metrics.outer_margin = 16.0f;
            metrics.panel_gap = 12.0f;
            metrics.toolbar_height = 44.0f;
            metrics.sidebar_width = 300.0f;
            metrics.utility_width = 344.0f;
            metrics.stack_sidebars = false;
            break;
        case SANDBOX3D_EDITOR_LAYOUT_WIDE:
            metrics.outer_margin = 20.0f;
            metrics.panel_gap = 16.0f;
            metrics.toolbar_height = 48.0f;
            metrics.sidebar_width = fminf(
                fmaxf((float)framebuffer_width * 0.15f, 304.0f),
                480.0f);
            metrics.utility_width = fminf(
                fmaxf((float)framebuffer_width * 0.19f, 344.0f),
                560.0f);
            metrics.stack_sidebars = false;
            break;
        case SANDBOX3D_EDITOR_LAYOUT_BREAKPOINT_COUNT:
        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_metrics = metrics;
    return HENKA_SUCCESS;
}

static bool sandbox3d_editor_layout_panel_visible(
    const sandbox3d_workspace_model* workspace,
    const sandbox3d_editor_layout_visibility* visibility,
    sandbox3d_workspace_panel_id panel_id)
{
    if (workspace == NULL || visibility == NULL ||
        sandbox3d_workspace_section_is_closed(workspace, panel_id))
    {
        return false;
    }

    switch (panel_id)
    {
        case SANDBOX3D_WORKSPACE_PANEL_CONTROLS:
            return visibility->tools_panel_visible ||
                sandbox3d_workspace_panel_is_floating(
                    workspace,
                    SANDBOX3D_WORKSPACE_PANEL_CONTROLS) ||
                sandbox3d_workspace_panel_is_detached(
                    workspace,
                    SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
        case SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS:
            return visibility->scene_objects_panel_visible &&
                (visibility->docked_content_visible ||
                 sandbox3d_workspace_panel_is_floating(
                     workspace,
                     SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS));
        case SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS:
            return visibility->object_details_panel_visible &&
                (visibility->docked_content_visible ||
                 sandbox3d_workspace_panel_is_floating(
                     workspace,
                     SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS));
        case SANDBOX3D_WORKSPACE_PANEL_UTILITY:
            return visibility->utility_panel_visible;
        case SANDBOX3D_WORKSPACE_PANEL_NONE:
        default:
            return false;
    }
}

static henka_ui_rect* sandbox3d_editor_layout_panel_rect_slot(
    sandbox3d_editor_frame_layout* layout,
    sandbox3d_workspace_panel_id panel_id)
{
    if (layout == NULL)
    {
        return NULL;
    }

    switch (panel_id)
    {
        case SANDBOX3D_WORKSPACE_PANEL_CONTROLS:
            return &layout->controls_panel;
        case SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS:
            return &layout->scene_objects_panel;
        case SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS:
            return &layout->object_details_panel;
        case SANDBOX3D_WORKSPACE_PANEL_UTILITY:
            return &layout->utility_panel;
        case SANDBOX3D_WORKSPACE_PANEL_NONE:
        default:
            return NULL;
    }
}

static void sandbox3d_editor_layout_reserve_debug_strip(
    sandbox3d_editor_frame_layout* layout)
{
    const float gap = 6.0f;

    if (layout == NULL || !henka_viewport_is_valid(layout->scene_viewport))
    {
        return;
    }

    if (layout->scene_viewport.height >
        (int)(SANDBOX3D_EDITOR_LAYOUT_DEBUG_STRIP_HEIGHT + gap + 80.0f))
    {
        layout->scene_viewport.height -=
            (int)(SANDBOX3D_EDITOR_LAYOUT_DEBUG_STRIP_HEIGHT + gap);
    }
    layout->debug_strip = (henka_ui_rect){
        (float)layout->scene_viewport.x,
        (float)(layout->scene_viewport.y + layout->scene_viewport.height) + gap,
        (float)layout->scene_viewport.width,
        SANDBOX3D_EDITOR_LAYOUT_DEBUG_STRIP_HEIGHT};
}

static size_t sandbox3d_editor_layout_count_visible_sections(
    const sandbox3d_workspace_model* workspace,
    const sandbox3d_editor_layout_visibility* visibility,
    sandbox3d_workspace_dock_zone dock_zone)
{
    const size_t section_count = sandbox3d_workspace_topology_is_valid(workspace)
        ? sandbox3d_workspace_get_topology_dock_section_count(workspace, dock_zone)
        : sandbox3d_workspace_get_dock_panel_count(workspace, dock_zone);
    size_t visible_count = 0U;
    size_t section_index;

    for (section_index = 0U; section_index < section_count; ++section_index)
    {
        const sandbox3d_workspace_panel_id section_id =
            sandbox3d_workspace_topology_is_valid(workspace)
                ? sandbox3d_workspace_get_topology_dock_section_at(
                    workspace, dock_zone, section_index)
                : sandbox3d_workspace_get_dock_panel_at(
                    workspace, dock_zone, section_index);
        const sandbox3d_workspace_panel_id display_panel_id =
            sandbox3d_workspace_topology_is_valid(workspace)
                ? sandbox3d_workspace_get_topology_section_active_tab(
                    workspace, section_id)
                : section_id;

        if (sandbox3d_editor_layout_panel_visible(
                workspace, visibility, display_panel_id))
        {
            ++visible_count;
        }
    }
    return visible_count;
}

static void sandbox3d_editor_layout_assign_dock_stack(
    const sandbox3d_workspace_model* workspace,
    const sandbox3d_editor_layout_visibility* visibility,
    sandbox3d_workspace_dock_zone dock_zone,
    henka_ui_rect dock_bounds,
    sandbox3d_editor_frame_layout* layout)
{
    const bool topology_valid = sandbox3d_workspace_topology_is_valid(workspace);
    const size_t item_count = topology_valid
        ? sandbox3d_workspace_get_topology_dock_section_count(workspace, dock_zone)
        : sandbox3d_workspace_get_dock_panel_count(workspace, dock_zone);
    const float panel_gap = layout != NULL && layout->panel_gap > 0.0f
        ? layout->panel_gap
        : 12.0f;
    size_t visible_count = 0U;
    size_t index;
    float panel_height;
    float y;

    if (workspace == NULL || visibility == NULL || layout == NULL ||
        dock_bounds.width <= 0.0f || dock_bounds.height <= 0.0f)
    {
        return;
    }

    for (index = 0U; index < item_count; ++index)
    {
        const sandbox3d_workspace_panel_id section_id = topology_valid
            ? sandbox3d_workspace_get_topology_dock_section_at(workspace, dock_zone, index)
            : sandbox3d_workspace_get_dock_panel_at(workspace, dock_zone, index);
        const sandbox3d_workspace_panel_id display_panel_id = topology_valid
            ? sandbox3d_workspace_get_topology_section_active_tab(workspace, section_id)
            : section_id;
        const sandbox3d_workspace_panel* panel =
            sandbox3d_workspace_get_panel_const(workspace, section_id);

        if (panel != NULL && panel->dock == dock_zone &&
            sandbox3d_editor_layout_panel_visible(
                workspace, visibility, display_panel_id))
        {
            ++visible_count;
        }
    }

    if (visible_count == 0U)
    {
        return;
    }

    panel_height =
        (dock_bounds.height - panel_gap * (float)(visible_count - 1U)) /
        (float)visible_count;
    y = dock_bounds.y;
    for (index = 0U; index < item_count; ++index)
    {
        const sandbox3d_workspace_panel_id section_id = topology_valid
            ? sandbox3d_workspace_get_topology_dock_section_at(workspace, dock_zone, index)
            : sandbox3d_workspace_get_dock_panel_at(workspace, dock_zone, index);
        const sandbox3d_workspace_panel_id display_panel_id = topology_valid
            ? sandbox3d_workspace_get_topology_section_active_tab(workspace, section_id)
            : section_id;
        const sandbox3d_workspace_panel* panel =
            sandbox3d_workspace_get_panel_const(workspace, section_id);
        henka_ui_rect* panel_rect;

        if (panel == NULL || panel->dock != dock_zone ||
            !sandbox3d_editor_layout_panel_visible(
                workspace, visibility, display_panel_id))
        {
            continue;
        }
        panel_rect = sandbox3d_editor_layout_panel_rect_slot(layout, section_id);
        if (panel_rect != NULL)
        {
            *panel_rect = (henka_ui_rect){
                dock_bounds.x, y, dock_bounds.width, panel_height};
        }
        y += panel_height + panel_gap;
    }
}

henka_result sandbox3d_editor_frame_layout_build(
    const sandbox3d_workspace_model* workspace,
    const sandbox3d_editor_layout_visibility* visibility,
    int framebuffer_width,
    int framebuffer_height,
    sandbox3d_editor_frame_layout* out_layout)
{
    sandbox3d_editor_layout_metrics metrics;
    henka_workspace_desc workspace_desc;
    henka_workspace_layout docked_layout;
    bool controls_left;
    bool controls_right;
    bool scene_left;
    bool scene_right;
    bool details_left;
    bool details_right;
    bool utility_left;
    bool utility_right;
    bool left_visible;
    bool right_visible;
    const sandbox3d_workspace_panel* panel;
    size_t left_topology_count;
    size_t right_topology_count;
    size_t left_visible_topology_count;
    size_t right_visible_topology_count;
    size_t panel_index;

    if (out_layout == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(out_layout, 0, sizeof(*out_layout));
    if (workspace == NULL || visibility == NULL ||
        framebuffer_width <= 0 || framebuffer_height <= 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (sandbox3d_editor_layout_metrics_for_framebuffer(
            framebuffer_width, framebuffer_height, &metrics) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    out_layout->outer_margin = metrics.outer_margin;
    out_layout->panel_gap = metrics.panel_gap;
    out_layout->controls_panel = (henka_ui_rect){
        metrics.outer_margin,
        metrics.outer_margin,
        SANDBOX3D_EDITOR_LAYOUT_DEFAULT_CONTROLS_WIDTH,
        SANDBOX3D_EDITOR_LAYOUT_DEFAULT_PANEL_HEIGHT};
    out_layout->scene_objects_panel = (henka_ui_rect){
        metrics.outer_margin,
        metrics.outer_margin,
        SANDBOX3D_EDITOR_LAYOUT_DEFAULT_SCENE_OBJECTS_WIDTH,
        SANDBOX3D_EDITOR_LAYOUT_DEFAULT_PANEL_HEIGHT};
    out_layout->object_details_panel = (henka_ui_rect){
        metrics.outer_margin,
        metrics.outer_margin,
        SANDBOX3D_EDITOR_LAYOUT_DEFAULT_DETAILS_WIDTH,
        SANDBOX3D_EDITOR_LAYOUT_DEFAULT_PANEL_HEIGHT};
    out_layout->utility_panel = (henka_ui_rect){
        metrics.outer_margin,
        metrics.outer_margin,
        SANDBOX3D_EDITOR_LAYOUT_DEFAULT_DETAILS_WIDTH,
        SANDBOX3D_EDITOR_LAYOUT_DEFAULT_UTILITY_HEIGHT};
    out_layout->scene_viewport = (henka_viewport){
        0, 0, framebuffer_width, framebuffer_height};

    panel = sandbox3d_workspace_get_panel_const(
        workspace, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    controls_left = sandbox3d_editor_layout_panel_visible(
        workspace, visibility, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) &&
        panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT;
    controls_right = sandbox3d_editor_layout_panel_visible(
        workspace, visibility, SANDBOX3D_WORKSPACE_PANEL_CONTROLS) &&
        panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT;
    panel = sandbox3d_workspace_get_panel_const(
        workspace, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    scene_left = sandbox3d_editor_layout_panel_visible(
        workspace, visibility, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS) &&
        panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT;
    scene_right = sandbox3d_editor_layout_panel_visible(
        workspace, visibility, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS) &&
        panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT;
    panel = sandbox3d_workspace_get_panel_const(
        workspace, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS);
    details_left = sandbox3d_editor_layout_panel_visible(
        workspace, visibility, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS) &&
        panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT;
    details_right = sandbox3d_editor_layout_panel_visible(
        workspace, visibility, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS) &&
        panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT;
    panel = sandbox3d_workspace_get_panel_const(
        workspace, SANDBOX3D_WORKSPACE_PANEL_UTILITY);
    utility_left = sandbox3d_editor_layout_panel_visible(
        workspace, visibility, SANDBOX3D_WORKSPACE_PANEL_UTILITY) &&
        panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT;
    utility_right = sandbox3d_editor_layout_panel_visible(
        workspace, visibility, SANDBOX3D_WORKSPACE_PANEL_UTILITY) &&
        panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT;
    left_visible = controls_left || scene_left || details_left || utility_left;
    right_visible = controls_right || scene_right || details_right || utility_right;

    memset(&workspace_desc, 0, sizeof(workspace_desc));
    workspace_desc.framebuffer_width = framebuffer_width;
    workspace_desc.framebuffer_height = framebuffer_height;
    workspace_desc.margin = metrics.outer_margin;
    workspace_desc.gap = metrics.panel_gap;
    workspace_desc.scene_header_height = 30.0f;
    workspace_desc.scene_padding = 8.0f;
    workspace_desc.min_scene_width = metrics.breakpoint ==
        SANDBOX3D_EDITOR_LAYOUT_NARROW ? 260 :
        metrics.breakpoint == SANDBOX3D_EDITOR_LAYOUT_MEDIUM ? 520 : 620;
    workspace_desc.min_scene_height = framebuffer_height >= 720 ? 404 :
        framebuffer_height >= 640 ? 344 : 244;
    workspace_desc.left_dock_visible = left_visible;
    workspace_desc.right_dock_visible = right_visible;
    workspace_desc.bottom_dock_visible = false;
    /* The reset widths mark the adaptive default policy. Any other value is
     * an explicit user/layout preference and remains authoritative. Below the
     * 1280px desktop acceptance width, retain the existing compact fallback. */
    workspace_desc.left_dock_width =
        metrics.breakpoint != SANDBOX3D_EDITOR_LAYOUT_NARROW &&
        workspace->left_dock_width == SANDBOX3D_WORKSPACE_RESET_LEFT_DOCK_WIDTH
            ? metrics.sidebar_width
            : workspace->left_dock_width;
    workspace_desc.right_dock_width =
        metrics.breakpoint != SANDBOX3D_EDITOR_LAYOUT_NARROW &&
        workspace->right_dock_width == SANDBOX3D_WORKSPACE_RESET_RIGHT_DOCK_WIDTH
            ? metrics.utility_width
            : workspace->right_dock_width;

    if (henka_workspace_layout_docked(&workspace_desc, &docked_layout) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    out_layout->left_dock = docked_layout.left_dock;
    out_layout->scene_frame = docked_layout.scene_frame;
    out_layout->right_dock = docked_layout.right_dock;
    out_layout->scene_viewport = docked_layout.scene_viewport;
    if (visibility->debug_strip_visible)
    {
        sandbox3d_editor_layout_reserve_debug_strip(out_layout);
    }

    out_layout->controls_panel = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    out_layout->scene_objects_panel = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    out_layout->object_details_panel = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    out_layout->utility_panel = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};

    if (workspace->maximized_section != SANDBOX3D_WORKSPACE_PANEL_NONE &&
        sandbox3d_editor_layout_panel_visible(
            workspace, visibility, workspace->maximized_section))
    {
        const sandbox3d_workspace_panel* maximized_panel =
            sandbox3d_workspace_get_panel_const(workspace, workspace->maximized_section);
        if (maximized_panel != NULL &&
            (maximized_panel->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT ||
             maximized_panel->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT))
        {
            const henka_ui_rect maximized_bounds = {
                metrics.outer_margin,
                metrics.outer_margin,
                fmaxf(1.0f, (float)framebuffer_width - metrics.outer_margin * 2.0f),
                fmaxf(1.0f, (float)framebuffer_height - metrics.outer_margin * 2.0f)};
            henka_ui_rect* maximized_slot;
            out_layout->left_dock = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
            out_layout->right_dock = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
            out_layout->left_splitter = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
            out_layout->right_splitter = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
            out_layout->scene_frame = maximized_bounds;
            out_layout->scene_viewport = (henka_viewport){
                0, 0, framebuffer_width, framebuffer_height};
            out_layout->debug_strip = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
            maximized_slot = sandbox3d_editor_layout_panel_rect_slot(
                out_layout, workspace->maximized_section);
            if (maximized_slot != NULL)
            {
                *maximized_slot = maximized_bounds;
            }
            return HENKA_SUCCESS;
        }
    }

    sandbox3d_editor_layout_assign_dock_stack(
        workspace,
        visibility,
        SANDBOX3D_WORKSPACE_DOCK_LEFT,
        out_layout->left_dock,
        out_layout);
    sandbox3d_editor_layout_assign_dock_stack(
        workspace,
        visibility,
        SANDBOX3D_WORKSPACE_DOCK_RIGHT,
        out_layout->right_dock,
        out_layout);
    sandbox3d_workspace_build_dock_topology_layout(
        workspace,
        SANDBOX3D_WORKSPACE_DOCK_LEFT,
        out_layout->left_dock,
        &out_layout->left_topology);
    sandbox3d_workspace_build_dock_topology_layout(
        workspace,
        SANDBOX3D_WORKSPACE_DOCK_RIGHT,
        out_layout->right_dock,
        &out_layout->right_topology);

    left_topology_count = sandbox3d_workspace_get_topology_dock_section_count(
        workspace, SANDBOX3D_WORKSPACE_DOCK_LEFT);
    right_topology_count = sandbox3d_workspace_get_topology_dock_section_count(
        workspace, SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    left_visible_topology_count = sandbox3d_editor_layout_count_visible_sections(
        workspace, visibility, SANDBOX3D_WORKSPACE_DOCK_LEFT);
    right_visible_topology_count = sandbox3d_editor_layout_count_visible_sections(
        workspace, visibility, SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    if (left_visible_topology_count < left_topology_count)
    {
        memset(&out_layout->left_topology, 0, sizeof(out_layout->left_topology));
    }
    if (right_visible_topology_count < right_topology_count)
    {
        memset(&out_layout->right_topology, 0, sizeof(out_layout->right_topology));
    }

    for (panel_index = 0U; panel_index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++panel_index)
    {
        henka_ui_rect* panel_slot = sandbox3d_editor_layout_panel_rect_slot(
            out_layout, (sandbox3d_workspace_panel_id)panel_index);
        const henka_ui_rect left_rect =
            out_layout->left_topology.section_rects[panel_index];
        const henka_ui_rect right_rect =
            out_layout->right_topology.section_rects[panel_index];
        if (panel_slot == NULL)
        {
            continue;
        }
        if (left_rect.width > 0.0f && left_rect.height > 0.0f)
        {
            *panel_slot = left_rect;
        }
        else if (right_rect.width > 0.0f && right_rect.height > 0.0f)
        {
            *panel_slot = right_rect;
        }
    }

    if (sandbox3d_workspace_panel_is_floating(
            workspace, SANDBOX3D_WORKSPACE_PANEL_CONTROLS))
    {
        const sandbox3d_workspace_panel* controls = sandbox3d_workspace_get_panel_const(
            workspace, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
        if (controls != NULL)
        {
            out_layout->controls_panel = controls->floating_rect;
        }
    }
    if (sandbox3d_editor_layout_panel_visible(
            workspace, visibility, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS) &&
        sandbox3d_workspace_panel_is_floating(
            workspace, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS))
    {
        const sandbox3d_workspace_panel* scene_objects = sandbox3d_workspace_get_panel_const(
            workspace, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
        if (scene_objects != NULL)
        {
            out_layout->scene_objects_panel = scene_objects->floating_rect;
        }
    }
    if (sandbox3d_editor_layout_panel_visible(
            workspace, visibility, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS) &&
        sandbox3d_workspace_panel_is_floating(
            workspace, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS))
    {
        const sandbox3d_workspace_panel* details = sandbox3d_workspace_get_panel_const(
            workspace, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS);
        if (details != NULL)
        {
            out_layout->object_details_panel = details->floating_rect;
        }
    }
    if (sandbox3d_editor_layout_panel_visible(
            workspace, visibility, SANDBOX3D_WORKSPACE_PANEL_UTILITY) &&
        sandbox3d_workspace_panel_is_floating(
            workspace, SANDBOX3D_WORKSPACE_PANEL_UTILITY))
    {
        const sandbox3d_workspace_panel* utility = sandbox3d_workspace_get_panel_const(
            workspace, SANDBOX3D_WORKSPACE_PANEL_UTILITY);
        if (utility != NULL)
        {
            out_layout->utility_panel = utility->floating_rect;
        }
    }

    if (left_visible)
    {
        out_layout->left_splitter = sandbox3d_workspace_left_splitter_rect(
            out_layout->left_dock, out_layout->scene_frame);
    }
    if (right_visible)
    {
        out_layout->right_splitter = sandbox3d_workspace_right_splitter_rect(
            out_layout->scene_frame, out_layout->right_dock);
    }
    return HENKA_SUCCESS;
}

bool sandbox3d_editor_frame_layout_is_valid(
    const sandbox3d_editor_frame_layout* layout)
{
    return layout != NULL && henka_viewport_is_valid(layout->scene_viewport);
}

henka_ui_rect sandbox3d_editor_layout_modeling_toolbar_bounds(
    const sandbox3d_editor_frame_layout* layout,
    bool authoring_available)
{
    const henka_ui_rect scene_frame = layout == NULL
        ? (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f}
        : layout->scene_frame;
    const bool compact_toolbar = scene_frame.width < 760.0f;
    const float toolbar_width = scene_frame.width - 20.0f;
    const bool compact_horizontal_toolbar = compact_toolbar &&
        toolbar_width >=
            SANDBOX3D_EDITOR_MODELING_TOOLBAR_COMPACT_ROW_WIDTH;
    float y = compact_toolbar ? scene_frame.y + 76.0f : scene_frame.y + 34.0f;
    const float height = authoring_available
        ? compact_toolbar && !compact_horizontal_toolbar ? 166.0f : 136.0f
        : compact_toolbar ? 52.0f : 136.0f;

    if (layout == NULL || scene_frame.width < 500.0f || scene_frame.height < 150.0f)
    {
        return (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    }

    if (authoring_available)
    {
        const henka_ui_rect scene_header = {
            scene_frame.x,
            scene_frame.y,
            scene_frame.width,
            scene_frame.width >= 430.0f && scene_frame.width < 760.0f
                ? 68.0f
                : 30.0f};
        henka_ui_rect selection_status;
        henka_ui_rect topology_toggle;

        if (sandbox3d_editor_layout_authoring_selection_status_bounds(
                layout->scene_viewport,
                scene_header,
                &selection_status) != HENKA_SUCCESS ||
            sandbox3d_editor_layout_authoring_topology_toggle_bounds(
                layout->scene_viewport,
                scene_header,
                selection_status,
                &topology_toggle) != HENKA_SUCCESS)
        {
            return (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
        }

        y = topology_toggle.y + topology_toggle.height + 8.0f;
    }

    if (y + height > scene_frame.y + scene_frame.height)
    {
        return (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    }
    return (henka_ui_rect){
        scene_frame.x + 10.0f,
        y,
        toolbar_width,
        height};
}

henka_viewport sandbox3d_editor_frame_layout_navigation_viewport(
    const sandbox3d_editor_frame_layout* layout,
    bool authoring_available)
{
    henka_viewport navigation_viewport;
    const henka_ui_rect toolbar = layout == NULL
        ? (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f}
        : sandbox3d_editor_layout_modeling_toolbar_bounds(
            layout,
            authoring_available);
    float safe_top;
    int bottom;

    if (layout == NULL || !henka_viewport_is_valid(layout->scene_viewport))
    {
        return (henka_viewport){0, 0, 0, 0};
    }
    navigation_viewport = layout->scene_viewport;
    if (toolbar.width <= 0.0f || toolbar.height <= 0.0f)
    {
        return navigation_viewport;
    }

    safe_top = toolbar.y + toolbar.height + 8.0f;
    bottom = navigation_viewport.y + navigation_viewport.height;
    if (safe_top > (float)navigation_viewport.y)
    {
        navigation_viewport.y = (int)ceilf(safe_top);
        navigation_viewport.height = bottom - navigation_viewport.y;
    }
    if (!henka_viewport_is_valid(navigation_viewport))
    {
        return (henka_viewport){0, 0, 0, 0};
    }
    return navigation_viewport;
}

henka_ui_rect* sandbox3d_editor_frame_layout_panel_rect_slot(
    sandbox3d_editor_frame_layout* layout,
    sandbox3d_workspace_panel_id panel_id)
{
    return sandbox3d_editor_layout_panel_rect_slot(layout, panel_id);
}

henka_ui_rect sandbox3d_editor_frame_layout_panel_rect(
    const sandbox3d_editor_frame_layout* layout,
    sandbox3d_workspace_panel_id panel_id)
{
    if (layout == NULL)
    {
        return (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    }
    switch (panel_id)
    {
        case SANDBOX3D_WORKSPACE_PANEL_CONTROLS:
            return layout->controls_panel;
        case SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS:
            return layout->scene_objects_panel;
        case SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS:
            return layout->object_details_panel;
        case SANDBOX3D_WORKSPACE_PANEL_UTILITY:
            return layout->utility_panel;
        case SANDBOX3D_WORKSPACE_PANEL_NONE:
        default:
            return (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    }
}

henka_result sandbox3d_editor_layout_tool_row(
    henka_ui_rect bounds,
    size_t item_count,
    float minimum_item_width,
    float gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count)
{
    double total_gap;
    double available_width;
    double item_width;
    size_t item_index;

    if (out_item_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_item_count = 0U;

    if (!sandbox3d_editor_layout_float_is_valid(bounds.x) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.y) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.width) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.height) ||
        bounds.width <= 0.0f || bounds.height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(minimum_item_width) ||
        minimum_item_width <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(gap) || gap < 0.0f ||
        item_count > SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS ||
        (item_count > 0U && (out_items == NULL || item_capacity < item_count)))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (item_count == 0U)
    {
        return HENKA_SUCCESS;
    }

    total_gap = (double)gap * (double)(item_count - 1U);
    available_width = (double)bounds.width - total_gap;
    if (!isfinite(total_gap) || !isfinite(available_width) || available_width <= 0.0)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    item_width = available_width / (double)item_count;
    if (!isfinite(item_width) || item_width < (double)minimum_item_width ||
        item_width > (double)FLT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    for (item_index = 0U; item_index < item_count; ++item_index)
    {
        const double item_x =
            (double)bounds.x + ((double)item_index * (item_width + (double)gap));
        if (!isfinite(item_x) || item_x < -(double)FLT_MAX || item_x > (double)FLT_MAX)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
    }

    for (item_index = 0U; item_index < item_count; ++item_index)
    {
        const double item_x =
            (double)bounds.x + ((double)item_index * (item_width + (double)gap));
        out_items[item_index] = (henka_ui_rect){
            (float)item_x,
            bounds.y,
            (float)item_width,
            bounds.height};
    }
    *out_item_count = item_count;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_editor_layout_text_control_row_internal(
    const henka_ui_context* text_context,
    henka_ui_rect bounds,
    const char* const* labels,
    size_t item_count,
    float scale,
    float horizontal_padding,
    float gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count)
{
    double total_gap;
    double required_width;
    double available_width;
    double extra_width;
    double item_x;
    size_t item_index;
    int measured_width;
    int measured_height;
    float required_items[SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS];

    if (out_item_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_item_count = 0U;

    if (!sandbox3d_editor_layout_float_is_valid(bounds.x) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.y) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.width) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.height) ||
        bounds.width <= 0.0f || bounds.height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(scale) || scale <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(horizontal_padding) ||
        horizontal_padding < 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(gap) || gap < 0.0f ||
        item_count > SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS ||
        (item_count > 0U &&
            (labels == NULL || out_items == NULL || item_capacity < item_count)))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (item_count == 0U)
    {
        return HENKA_SUCCESS;
    }

    required_width = 0.0;
    for (item_index = 0U; item_index < item_count; ++item_index)
    {
        double required_item_width;
        henka_result measure_result;

        if (labels[item_index] == NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        measure_result = text_context != NULL
            ? henka_ui_measure_text_for_context(
                text_context,
                labels[item_index],
                scale,
                &measured_width,
                &measured_height)
            : henka_ui_measure_text(
                labels[item_index],
                scale,
                &measured_width,
                &measured_height);
        if (measure_result != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        (void)measured_height;
        required_item_width =
            (double)measured_width + (double)horizontal_padding * 2.0;
        if (!isfinite(required_item_width) ||
            required_item_width <= 0.0 ||
            required_item_width > (double)FLT_MAX)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        required_items[item_index] = (float)required_item_width;
        required_width += required_item_width;
    }

    total_gap = (double)gap * (double)(item_count - 1U);
    available_width = (double)bounds.width - total_gap;
    if (!isfinite(total_gap) || !isfinite(required_width) ||
        !isfinite(available_width) || available_width < required_width)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    extra_width =
        (available_width - required_width) / (double)item_count;
    if (!isfinite(extra_width) || extra_width < 0.0 ||
        extra_width > (double)FLT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    item_x = (double)bounds.x;
    for (item_index = 0U; item_index < item_count; ++item_index)
    {
        const double item_width = item_index + 1U == item_count
            ? ((double)bounds.x + (double)bounds.width) - item_x
            : (double)required_items[item_index] + extra_width;
        const double item_right = item_x + item_width;
        const double bounds_right = (double)bounds.x + (double)bounds.width;

        if (!isfinite(item_x) || !isfinite(item_width) ||
            !isfinite(item_right) || item_x < -(double)FLT_MAX ||
            item_x > (double)FLT_MAX || item_width <= 0.0 ||
            item_width > (double)FLT_MAX ||
            item_width < (double)required_items[item_index] ||
            item_right > bounds_right)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        out_items[item_index] = (henka_ui_rect){
            (float)item_x,
            bounds.y,
            (float)item_width,
            bounds.height};
        item_x = item_right + (double)gap;
    }

    *out_item_count = item_count;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_nonoverlapping_horizontal_offset(
    float available_width,
    float preferred_offset,
    float leading_text_offset,
    float leading_text_width,
    float gap,
    float item_width,
    float* out_offset)
{
    double leading_right;
    double selected_offset;

    if (out_offset == NULL ||
        !sandbox3d_editor_layout_float_is_valid(available_width) ||
        !sandbox3d_editor_layout_float_is_valid(preferred_offset) ||
        !sandbox3d_editor_layout_float_is_valid(leading_text_offset) ||
        !sandbox3d_editor_layout_float_is_valid(leading_text_width) ||
        !sandbox3d_editor_layout_float_is_valid(gap) ||
        !sandbox3d_editor_layout_float_is_valid(item_width) ||
        available_width <= 0.0f || preferred_offset < 0.0f ||
        leading_text_offset < 0.0f || leading_text_width < 0.0f ||
        gap < 0.0f || item_width <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    leading_right =
        (double)leading_text_offset + (double)leading_text_width;
    selected_offset = fmax((double)preferred_offset, leading_right + (double)gap);
    if (!isfinite(leading_right) || !isfinite(selected_offset) ||
        selected_offset > (double)FLT_MAX ||
        selected_offset + (double)item_width > (double)available_width)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    *out_offset = (float)selected_offset;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_text_control_row(
    henka_ui_rect bounds,
    const char* const* labels,
    size_t item_count,
    float scale,
    float horizontal_padding,
    float gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count)
{
    return sandbox3d_editor_layout_text_control_row_internal(
        NULL,
        bounds,
        labels,
        item_count,
        scale,
        horizontal_padding,
        gap,
        out_items,
        item_capacity,
        out_item_count);
}

henka_result sandbox3d_editor_layout_text_control_row_for_context(
    const henka_ui_context* text_context,
    henka_ui_rect bounds,
    const char* const* labels,
    size_t item_count,
    float scale,
    float horizontal_padding,
    float gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count)
{
    if (text_context == NULL)
    {
        if (out_item_count != NULL)
        {
            *out_item_count = 0U;
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_editor_layout_text_control_row_internal(
        text_context,
        bounds,
        labels,
        item_count,
        scale,
        horizontal_padding,
        gap,
        out_items,
        item_capacity,
        out_item_count);
}

henka_result sandbox3d_editor_layout_text_control_row_minimum_width_for_context(
    const henka_ui_context* text_context,
    const char* const* labels,
    size_t item_count,
    float scale,
    float horizontal_padding,
    float gap,
    float* out_minimum_width)
{
    double required_width;
    size_t item_index;

    if (out_minimum_width == NULL || text_context == NULL ||
        !sandbox3d_editor_layout_float_is_valid(scale) || scale <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(horizontal_padding) ||
        horizontal_padding < 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(gap) || gap < 0.0f ||
        item_count > SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS ||
        (item_count > 0U && labels == NULL))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    required_width = item_count > 1U
        ? (double)gap * (double)(item_count - 1U)
        : 0.0;
    for (item_index = 0U; item_index < item_count; ++item_index)
    {
        int measured_width = 0;
        int measured_height = 0;

        if (labels[item_index] == NULL ||
            henka_ui_measure_text_for_context(
                text_context,
                labels[item_index],
                scale,
                &measured_width,
                &measured_height) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        (void)measured_height;
        required_width +=
            (double)measured_width + (double)horizontal_padding * 2.0;
        if (!isfinite(required_width) || required_width < 0.0 ||
            required_width > (double)FLT_MAX)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
    }

    *out_minimum_width = (float)required_width;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_text_control_grid(
    henka_ui_rect bounds,
    const char* const* labels,
    size_t item_count,
    size_t column_count,
    float control_height,
    float scale,
    float horizontal_padding,
    float column_gap,
    float row_gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count)
{
    henka_ui_rect candidate_items[SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS];
    henka_ui_rect row_items[SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS];
    size_t row_count;
    size_t row_index;
    size_t item_index;

    if (out_item_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_item_count = 0U;
    if (!sandbox3d_editor_layout_float_is_valid(bounds.x) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.y) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.width) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.height) ||
        bounds.width <= 0.0f || bounds.height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(control_height) ||
        control_height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(scale) || scale <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(horizontal_padding) ||
        horizontal_padding < 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(column_gap) || column_gap < 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(row_gap) || row_gap < 0.0f ||
        column_count == 0U ||
        item_count > SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS ||
        (item_count > 0U &&
            (labels == NULL || out_items == NULL || item_capacity < item_count)))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (item_count == 0U)
    {
        return HENKA_SUCCESS;
    }
    if (column_count > item_count)
    {
        column_count = item_count;
    }
    row_count = (item_count + column_count - 1U) / column_count;
    if ((double)row_count * (double)control_height +
            (double)(row_count - 1U) * (double)row_gap >
        (double)bounds.height)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    for (row_index = 0U; row_index < row_count; ++row_index)
    {
        const size_t first_item = row_index * column_count;
        const size_t remaining_items = item_count - first_item;
        const size_t items_in_row =
            remaining_items < column_count ? remaining_items : column_count;
        const double row_y = (double)bounds.y +
            (double)row_index * ((double)control_height + (double)row_gap);
        const double row_bottom = row_y + (double)control_height;
        const double bounds_bottom = (double)bounds.y + (double)bounds.height;
        henka_result result;

        if (!isfinite(row_y) || !isfinite(row_bottom) ||
            row_y < -(double)FLT_MAX || row_y > (double)FLT_MAX ||
            row_bottom > bounds_bottom)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        result = sandbox3d_editor_layout_text_control_row(
            (henka_ui_rect){
                bounds.x,
                (float)row_y,
                bounds.width,
                control_height},
            &labels[first_item],
            items_in_row,
            scale,
            horizontal_padding,
            column_gap,
            row_items,
            SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS,
            &item_index);
        if (result != HENKA_SUCCESS || item_index != items_in_row)
        {
            return result != HENKA_SUCCESS ? result : HENKA_ERROR_UNKNOWN;
        }
        memcpy(
            &candidate_items[first_item],
            row_items,
            items_in_row * sizeof(row_items[0]));
    }

    memcpy(out_items, candidate_items, item_count * sizeof(candidate_items[0]));
    *out_item_count = item_count;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_scene_object_label(
    const henka_ui_context* ui,
    const char* name,
    size_t hierarchy_depth,
    bool hidden,
    float available_width,
    char* out_text,
    size_t text_capacity,
    bool* out_requires_wrapping)
{
    static const char hidden_suffix[] = "  - Hidden";
    char candidate[512];
    char indentation[25];
    const char* suffix = hidden ? hidden_suffix : "";
    const size_t name_length = name != NULL ? strlen(name) : 0U;
    const size_t suffix_length = strlen(suffix);
    const size_t indentation_length = hierarchy_depth > 12U
        ? 24U
        : hierarchy_depth * 2U;
    int measured_width = 0;
    int measured_height = 0;
    size_t candidate_length;
    henka_result result;

    if (ui == NULL || name == NULL || name_length == 0U ||
        out_text == NULL || text_capacity == 0U || out_requires_wrapping == NULL ||
        !sandbox3d_editor_layout_float_is_valid(available_width) ||
        available_width <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (name_length > sizeof(candidate) - 40U)
    {
        return HENKA_ERROR_LIMIT;
    }

    memset(indentation, ' ', indentation_length);
    indentation[indentation_length] = '\0';
    candidate_length = indentation_length + name_length + suffix_length;
    if (candidate_length + 1U > sizeof(candidate))
    {
        return HENKA_ERROR_LIMIT;
    }
    memcpy(candidate, indentation, indentation_length);
    memcpy(candidate + indentation_length, name, name_length);
    memcpy(candidate + indentation_length + name_length, suffix, suffix_length);
    candidate[candidate_length] = '\0';

    result = henka_ui_measure_text_for_context(
        ui,
        candidate,
        1.0f,
        &measured_width,
        &measured_height);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (candidate_length + 1U > text_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }

    memcpy(out_text, candidate, candidate_length + 1U);
    *out_requires_wrapping = (float)measured_width > available_width;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_variable_row_pages(
    const float* row_heights,
    size_t row_count,
    float available_height,
    float row_gap,
    size_t* out_page_starts,
    size_t page_start_capacity,
    size_t* out_page_count)
{
    size_t required_page_count = 1U;
    size_t page_index = 0U;
    size_t row_index;
    double used_height = 0.0;

    if ((row_count > 0U && row_heights == NULL) ||
        out_page_starts == NULL || page_start_capacity < 2U ||
        out_page_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!sandbox3d_editor_layout_float_is_valid(available_height) ||
        available_height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(row_gap) || row_gap < 0.0f)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    for (row_index = 0U; row_index < row_count; ++row_index)
    {
        const float height = row_heights[row_index];
        if (!sandbox3d_editor_layout_float_is_valid(height) || height <= 0.0f)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        if (height > available_height)
        {
            return HENKA_ERROR_LIMIT;
        }
        if (used_height > 0.0 &&
            used_height + (double)row_gap + (double)height >
                (double)available_height)
        {
            ++required_page_count;
            used_height = (double)height;
        }
        else
        {
            used_height = used_height > 0.0
                ? used_height + (double)row_gap + (double)height
                : (double)height;
        }
    }
    if (required_page_count >= page_start_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }

    out_page_starts[0] = 0U;
    used_height = 0.0;
    for (row_index = 0U; row_index < row_count; ++row_index)
    {
        const double height = (double)row_heights[row_index];
        if (used_height > 0.0 &&
            used_height + (double)row_gap + height > (double)available_height)
        {
            ++page_index;
            out_page_starts[page_index] = row_index;
            used_height = 0.0;
        }
        used_height = used_height > 0.0
            ? used_height + (double)row_gap + height
            : height;
    }
    out_page_starts[required_page_count] = row_count;
    *out_page_count = required_page_count;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_authoring_topology_toggle_bounds(
    henka_viewport viewport,
    henka_ui_rect scene_header,
    henka_ui_rect selection_status,
    henka_ui_rect* out_bounds)
{
    const double margin = 12.0;
    const double gap = 4.0;
    const double control_height = 28.0;
    const double preferred_width = 214.0;
    double viewport_right;
    double viewport_bottom;
    double header_bottom;
    double selection_bottom;
    double control_y;
    double control_width;
    henka_ui_rect candidate;

    if (out_bounds == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (viewport.width < 240 || viewport.height <= 0 ||
        !sandbox3d_editor_layout_float_is_valid(scene_header.x) ||
        !sandbox3d_editor_layout_float_is_valid(scene_header.y) ||
        !sandbox3d_editor_layout_float_is_valid(scene_header.width) ||
        !sandbox3d_editor_layout_float_is_valid(scene_header.height) ||
        scene_header.width <= 0.0f || scene_header.height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(selection_status.x) ||
        !sandbox3d_editor_layout_float_is_valid(selection_status.y) ||
        !sandbox3d_editor_layout_float_is_valid(selection_status.width) ||
        !sandbox3d_editor_layout_float_is_valid(selection_status.height) ||
        selection_status.width <= 0.0f || selection_status.height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    viewport_right = (double)viewport.x + (double)viewport.width;
    viewport_bottom = (double)viewport.y + (double)viewport.height;
    header_bottom = (double)scene_header.y + (double)scene_header.height;
    selection_bottom =
        (double)selection_status.y + (double)selection_status.height;
    control_y = fmax(header_bottom, selection_bottom) + gap;
    control_width = fmin(preferred_width, (double)viewport.width - margin * 2.0);
    candidate = (henka_ui_rect){
        (float)((double)viewport.x + margin),
        (float)control_y,
        (float)control_width,
        (float)control_height};

    if (!isfinite(viewport_right) || !isfinite(viewport_bottom) ||
        !isfinite(header_bottom) || !isfinite(selection_bottom) ||
        !isfinite(control_y) || !isfinite(control_width) ||
        control_width <= 0.0 ||
        (double)candidate.x + (double)candidate.width > viewport_right ||
        (double)candidate.y + (double)candidate.height > viewport_bottom)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    *out_bounds = candidate;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_authoring_selection_status_bounds(
    henka_viewport viewport,
    henka_ui_rect scene_header,
    henka_ui_rect* out_bounds)
{
    const double margin = 12.0;
    const double header_gap = 8.0;
    const double preferred_status_width = 320.0;
    const double status_height = 28.0;
    const double viewport_right = (double)viewport.x + (double)viewport.width;
    const double viewport_bottom = (double)viewport.y + (double)viewport.height;
    const double header_bottom = (double)scene_header.y + (double)scene_header.height;
    const double preferred_y = (double)viewport.y + margin;
    const double header_y = scene_header.height > 0.0f
        ? header_bottom + header_gap
        : preferred_y;
    const double status_y = fmax(preferred_y, header_y);
    henka_ui_rect candidate;

    if (out_bounds == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (viewport.width < 240 || viewport.height <= 0 ||
        !sandbox3d_editor_layout_float_is_valid(scene_header.x) ||
        !sandbox3d_editor_layout_float_is_valid(scene_header.y) ||
        !sandbox3d_editor_layout_float_is_valid(scene_header.width) ||
        !sandbox3d_editor_layout_float_is_valid(scene_header.height) ||
        scene_header.width < 0.0f || scene_header.height < 0.0f ||
        (scene_header.height > 0.0f && scene_header.width <= 0.0f))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    candidate = (henka_ui_rect){
        (float)((double)viewport.x + margin),
        (float)status_y,
        (float)fmin(preferred_status_width, (double)viewport.width - margin * 2.0),
        (float)status_height};
    if (!isfinite(viewport_right) || !isfinite(viewport_bottom) ||
        !isfinite(header_bottom) || !isfinite(preferred_y) ||
        !isfinite(status_y) ||
        (double)candidate.x + (double)candidate.width > viewport_right ||
        (double)candidate.y + (double)candidate.height > viewport_bottom)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    *out_bounds = candidate;
    return HENKA_SUCCESS;
}
