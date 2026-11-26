#include "editor_layout.h"

const char* const sandbox3d_modeling_toolbar_tool_labels[6] = {
    "Select", "Move", "Rotate", "Scale", "Snap", "X-Ray"};
const char* const sandbox3d_modeling_toolbar_orientation_labels[3] = {
    "World", "Local", "Norm."};
const char* const sandbox3d_modeling_toolbar_pivot_labels[3] = {
    "Median", "Active", "Indiv."};

#include <henka/workspace.h>

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS 64U
#define SANDBOX3D_EDITOR_LAYOUT_MAX_FRAMEBUFFER_DIMENSION 32768
#define SANDBOX3D_EDITOR_LAYOUT_DEFAULT_PANEL_HEIGHT 360.0f
#define SANDBOX3D_EDITOR_LAYOUT_DEFAULT_CONTROLS_WIDTH 320.0f
#define SANDBOX3D_EDITOR_LAYOUT_DEFAULT_SCENE_OBJECTS_WIDTH 260.0f
#define SANDBOX3D_EDITOR_LAYOUT_DEFAULT_DETAILS_WIDTH 352.0f
#define SANDBOX3D_EDITOR_LAYOUT_DEFAULT_UTILITY_HEIGHT 228.0f
#define SANDBOX3D_EDITOR_LAYOUT_DEBUG_STRIP_HEIGHT 58.0f
#define SANDBOX3D_EDITOR_LAYOUT_MODELING_TOOLBAR_MIN_FRAME_PIXELS 448.0

static bool sandbox3d_editor_layout_float_is_valid(float value)
{
    return isfinite((double)value) != 0;
}

static bool sandbox3d_editor_layout_point_in_nonempty_rect(
    henka_ui_rect rect,
    henka_vec2 point)
{
    return sandbox3d_editor_layout_float_is_valid(rect.x) &&
        sandbox3d_editor_layout_float_is_valid(rect.y) &&
        sandbox3d_editor_layout_float_is_valid(rect.width) &&
        sandbox3d_editor_layout_float_is_valid(rect.height) &&
        rect.width > 0.0f &&
        rect.height > 0.0f &&
        henka_ui_rect_contains(rect, point);
}

static double sandbox3d_editor_layout_clamp_framebuffer_coordinate(
    double coordinate,
    int framebuffer_extent)
{
    if (coordinate <= 0.0)
    {
        return 0.0;
    }
    if (coordinate >= (double)framebuffer_extent)
    {
        return (double)framebuffer_extent;
    }
    return coordinate;
}

henka_result sandbox3d_editor_layout_content_viewport_to_framebuffer(
    henka_viewport content_viewport,
    uint16_t app_scale_percent,
    uint32_t display_scale_numerator,
    uint32_t display_scale_denominator,
    int framebuffer_width,
    int framebuffer_height,
    henka_viewport* out_framebuffer_viewport)
{
    double scale;
    double left;
    double top;
    double right;
    double bottom;
    long rounded_left;
    long rounded_top;
    long rounded_right;
    long rounded_bottom;
    double app_scale;

    if (out_framebuffer_viewport != NULL)
    {
        *out_framebuffer_viewport = (henka_viewport){0, 0, 0, 0};
    }
    if (out_framebuffer_viewport == NULL ||
        content_viewport.x < 0 || content_viewport.y < 0 ||
        content_viewport.width <= 0 || content_viewport.height <= 0 ||
        framebuffer_width <= 0 || framebuffer_height <= 0 ||
        (app_scale_percent != 100U && app_scale_percent != 125U &&
         app_scale_percent != 150U && app_scale_percent != 200U) ||
        display_scale_numerator == 0U || display_scale_denominator == 0U ||
        display_scale_numerator > 16U || display_scale_denominator > 16U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (framebuffer_width > SANDBOX3D_EDITOR_LAYOUT_MAX_FRAMEBUFFER_DIMENSION ||
        framebuffer_height > SANDBOX3D_EDITOR_LAYOUT_MAX_FRAMEBUFFER_DIMENSION)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    app_scale = (double)app_scale_percent / 100.0;
    scale = (double)display_scale_numerator /
        (double)display_scale_denominator * app_scale;
    if (!isfinite(scale) || scale < 0.25 || scale > 8.0)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    left = sandbox3d_editor_layout_clamp_framebuffer_coordinate(
        (double)content_viewport.x * scale,
        framebuffer_width);
    top = sandbox3d_editor_layout_clamp_framebuffer_coordinate(
        (double)content_viewport.y * scale,
        framebuffer_height);
    right = sandbox3d_editor_layout_clamp_framebuffer_coordinate(
        ((double)content_viewport.x + (double)content_viewport.width) * scale,
        framebuffer_width);
    bottom = sandbox3d_editor_layout_clamp_framebuffer_coordinate(
        ((double)content_viewport.y + (double)content_viewport.height) * scale,
        framebuffer_height);

    rounded_left = lround(left);
    rounded_top = lround(top);
    rounded_right = lround(right);
    rounded_bottom = lround(bottom);
    if (rounded_right <= rounded_left || rounded_bottom <= rounded_top)
    {
        return HENKA_SUCCESS;
    }

    out_framebuffer_viewport->x = (int)rounded_left;
    out_framebuffer_viewport->y = (int)rounded_top;
    out_framebuffer_viewport->width = (int)(rounded_right - rounded_left);
    out_framebuffer_viewport->height = (int)(rounded_bottom - rounded_top);
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_content_size_for_app_scale(
    int content_width,
    int content_height,
    uint16_t app_scale_percent,
    int* out_logical_width,
    int* out_logical_height)
{
    double logical_width;
    double logical_height;
    double app_scale;

    if (out_logical_width != NULL)
    {
        *out_logical_width = 0;
    }
    if (out_logical_height != NULL)
    {
        *out_logical_height = 0;
    }
    if (out_logical_width == NULL || out_logical_height == NULL ||
        content_width <= 0 || content_height <= 0 ||
        (app_scale_percent != 100U && app_scale_percent != 125U &&
         app_scale_percent != 150U && app_scale_percent != 200U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    app_scale = (double)app_scale_percent / 100.0;
    logical_width = ceil((double)content_width / app_scale);
    logical_height = ceil((double)content_height / app_scale);
    if (!isfinite(logical_width) || !isfinite(logical_height) ||
        logical_width < 1.0 || logical_height < 1.0 ||
        logical_width > (double)INT_MAX ||
        logical_height > (double)INT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    *out_logical_width = (int)logical_width;
    *out_logical_height = (int)logical_height;
    return HENKA_SUCCESS;
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
            metrics.sidebar_width = 260.0f;
            metrics.utility_width = 292.0f;
            metrics.stack_sidebars = false;
            break;
        case SANDBOX3D_EDITOR_LAYOUT_WIDE:
            metrics.outer_margin = 20.0f;
            metrics.panel_gap = 16.0f;
            metrics.toolbar_height = 48.0f;
            metrics.sidebar_width = 304.0f;
            metrics.utility_width = 344.0f;
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
    const henka_ui_context* ui_context,
    const sandbox3d_workspace_model* workspace,
    const sandbox3d_editor_layout_visibility* visibility,
    int framebuffer_width,
    int framebuffer_height,
    sandbox3d_editor_frame_layout* out_layout)
{
    sandbox3d_editor_layout_metrics metrics;
    sandbox3d_editor_scene_view_header_layout scene_header;
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
    int minimum_scene_width;

    if (out_layout == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(out_layout, 0, sizeof(*out_layout));
    if (ui_context == NULL || workspace == NULL || visibility == NULL ||
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
    minimum_scene_width = metrics.breakpoint ==
        SANDBOX3D_EDITOR_LAYOUT_NARROW ? 260 :
        metrics.breakpoint == SANDBOX3D_EDITOR_LAYOUT_MEDIUM ? 520 : 620;
    if (metrics.breakpoint == SANDBOX3D_EDITOR_LAYOUT_NARROW)
    {
        minimum_scene_width = (int)lround(fmin(
            260.0,
            fmax(120.0, (double)framebuffer_width / 3.0)));
    }
    workspace_desc.min_scene_width = minimum_scene_width;
    workspace_desc.min_scene_height = framebuffer_height >= 720 ? 404 :
        framebuffer_height >= 640 ? 344 : 244;
    workspace_desc.left_dock_visible = left_visible;
    workspace_desc.right_dock_visible = right_visible;
    workspace_desc.bottom_dock_visible = false;
    workspace_desc.left_dock_width = workspace->left_dock_width;
    workspace_desc.right_dock_width = workspace->right_dock_width;

    if (henka_workspace_layout_docked(&workspace_desc, &docked_layout) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    if (sandbox3d_editor_layout_scene_view_header_for_context(
            ui_context,
            docked_layout.scene_frame,
            &scene_header) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    if (fabsf(scene_header.height - workspace_desc.scene_header_height) >
        0.01f)
    {
        workspace_desc.scene_header_height = scene_header.height;
        if (henka_workspace_layout_docked(
                &workspace_desc, &docked_layout) != HENKA_SUCCESS ||
            sandbox3d_editor_layout_scene_view_header_for_context(
                ui_context,
                docked_layout.scene_frame,
                &scene_header) != HENKA_SUCCESS ||
            fabsf(scene_header.height - workspace_desc.scene_header_height) >
                0.01f)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
    }
    out_layout->left_dock = docked_layout.left_dock;
    out_layout->scene_frame = docked_layout.scene_frame;
    out_layout->scene_header = scene_header;
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
            memset(&out_layout->scene_header, 0, sizeof(out_layout->scene_header));
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

static henka_result sandbox3d_editor_layout_measure_max_text_width(
    const henka_ui_context* ui_context,
    const char* const* labels,
    size_t label_count,
    float scale,
    float* out_width)
{
    size_t index;
    float maximum_width = 0.0f;

    if (ui_context == NULL || labels == NULL || label_count == 0U ||
        out_width == NULL ||
        !sandbox3d_editor_layout_float_is_valid(scale) || scale <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < label_count; ++index)
    {
        int measured_width = 0;
        int measured_height = 0;
        henka_result result;

        if (labels[index] == NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        result = henka_ui_measure_text_for_context(
            ui_context,
            labels[index],
            scale,
            &measured_width,
            &measured_height);
        if (result != HENKA_SUCCESS || measured_width < 0)
        {
            return result == HENKA_SUCCESS ? HENKA_ERROR_NUMERIC_RANGE : result;
        }
        if ((float)measured_width > maximum_width)
        {
            maximum_width = (float)measured_width;
        }
    }

    *out_width = maximum_width;
    return HENKA_SUCCESS;
}

static size_t sandbox3d_editor_layout_toolbar_grid_columns(
    float available_width,
    float minimum_button_width,
    size_t tool_count,
    const size_t* candidates,
    size_t candidate_count)
{
    const float gap = 4.0f;
    size_t index;

    if (candidates == NULL || candidate_count == 0U)
    {
        return 0U;
    }
    for (index = 0U; index < candidate_count; ++index)
    {
        const size_t columns = candidates[index];
        const float cell_width = columns == 0U || columns > tool_count
            ? 0.0f
            : (available_width - gap * (float)(columns - 1U)) /
                (float)columns;

        if (cell_width >= 44.0f && cell_width >= minimum_button_width)
        {
            return columns;
        }
    }
    return 0U;
}

static float sandbox3d_editor_layout_toolbar_grid_end_y(
    float tool_y,
    size_t tool_rows)
{
    const float button_height = 28.0f;
    const float row_gap = 4.0f;

    return tool_rows == 0U
        ? tool_y
        : tool_y + (float)tool_rows * button_height +
            (float)(tool_rows - 1U) * row_gap;
}

henka_ui_rect sandbox3d_editor_layout_modeling_toolbar_bounds(
    const henka_ui_context* ui_context,
    henka_ui_rect scene_frame,
    uint16_t app_scale_percent,
    bool authoring_available,
    bool options_expanded)
{
    sandbox3d_modeling_toolbar_layout toolbar_layout;

    if (sandbox3d_editor_layout_modeling_toolbar_compute(
            ui_context,
            scene_frame,
            app_scale_percent,
            authoring_available,
            options_expanded,
            &toolbar_layout) != HENKA_SUCCESS)
    {
        return (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    }
    return toolbar_layout.bounds;
}

henka_result sandbox3d_editor_layout_modeling_toolbar_compute(
    const henka_ui_context* ui_context,
    henka_ui_rect scene_frame,
    uint16_t app_scale_percent,
    bool authoring_available,
    bool options_expanded,
    sandbox3d_modeling_toolbar_layout* out_layout)
{
    static const size_t compact_primary_columns[] = {4U, 2U};
    static const size_t compact_options_columns[] = {6U, 3U, 2U};
    static const size_t wide_columns[] = {6U, 3U, 2U};
    sandbox3d_modeling_toolbar_layout layout;
    sandbox3d_editor_scene_view_header_layout scene_header;
    const bool compact_toolbar = scene_frame.width < 760.0f;
    const size_t tool_count =
        compact_toolbar && authoring_available && !options_expanded ? 4U :
        authoring_available ? 6U : 6U;
    const size_t* column_candidates;
    size_t candidate_count;
    double effective_frame_width;
    float y;
    float width;
    float maximum_tool_label_width;
    float minimum_tool_button_width;
    float tool_y;
    float tool_bottom;
    float summary_bottom;
    float toolbar_height;
    float minimum_toolbar_y;
    size_t index;
    henka_result result;

    if (out_layout == NULL || ui_context == NULL ||
        !sandbox3d_editor_layout_float_is_valid(scene_frame.x) ||
        !sandbox3d_editor_layout_float_is_valid(scene_frame.y) ||
        !sandbox3d_editor_layout_float_is_valid(scene_frame.width) ||
        !sandbox3d_editor_layout_float_is_valid(scene_frame.height) ||
        (app_scale_percent != 100U && app_scale_percent != 125U &&
         app_scale_percent != 150U && app_scale_percent != 200U) ||
        scene_frame.height < 150.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    effective_frame_width = (double)scene_frame.width *
        (double)app_scale_percent / 100.0;
    if (!isfinite(effective_frame_width) ||
        effective_frame_width < SANDBOX3D_EDITOR_LAYOUT_MODELING_TOOLBAR_MIN_FRAME_PIXELS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(&layout, 0, sizeof(layout));
    layout.compact = compact_toolbar;
    layout.authoring_available = authoring_available;
    layout.options_expanded = compact_toolbar && authoring_available && options_expanded;
    y = authoring_available
        ? scene_frame.y + 82.0f
        : compact_toolbar ? scene_frame.y + 76.0f : scene_frame.y + 34.0f;
    result = sandbox3d_editor_layout_scene_view_header_for_context(
        ui_context, scene_frame, &scene_header);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    minimum_toolbar_y = scene_frame.y + scene_header.height +
        (compact_toolbar ? 8.0f : 4.0f);
    y = fmaxf(y, minimum_toolbar_y);
    width = scene_frame.width - 20.0f;
    layout.bounds = (henka_ui_rect){scene_frame.x + 10.0f, y, width, 0.0f};

    if (!authoring_available && compact_toolbar)
    {
        layout.bounds.height = 52.0f;
        *out_layout = layout;
        return HENKA_SUCCESS;
    }

    layout.tool_count = tool_count;
    if (sandbox3d_editor_layout_measure_max_text_width(
            ui_context,
            sandbox3d_modeling_toolbar_tool_labels,
            tool_count,
            1.0f,
            &maximum_tool_label_width) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    minimum_tool_button_width = maximum_tool_label_width + 36.0f;
    if (minimum_tool_button_width < 44.0f)
    {
        minimum_tool_button_width = 44.0f;
    }
    if (compact_toolbar)
    {
        column_candidates = layout.options_expanded
            ? compact_options_columns
            : compact_primary_columns;
        candidate_count = layout.options_expanded
            ? sizeof(compact_options_columns) / sizeof(compact_options_columns[0])
            : sizeof(compact_primary_columns) / sizeof(compact_primary_columns[0]);
    }
    else
    {
        column_candidates = wide_columns;
        candidate_count = sizeof(wide_columns) / sizeof(wide_columns[0]);
    }
    layout.tool_columns = sandbox3d_editor_layout_toolbar_grid_columns(
        width,
        minimum_tool_button_width,
        tool_count,
        column_candidates,
        candidate_count);
    if (layout.tool_columns == 0U)
    {
        return HENKA_ERROR_LIMIT;
    }
    layout.tool_rows = (tool_count + layout.tool_columns - 1U) /
        layout.tool_columns;

    if (compact_toolbar)
    {
        const float inner_x = layout.bounds.x + 8.0f;
        const float inner_width = width - 16.0f;
        const bool narrow_compact_selection = width < 270.0f;

        layout.options_toggle = (henka_ui_rect){
            layout.bounds.x + width - 112.0f,
            y + 4.0f,
            104.0f,
            32.0f};
        layout.selection_mode = (henka_ui_rect){
            layout.bounds.x + (narrow_compact_selection ? 8.0f : 74.0f),
            y + 40.0f,
            narrow_compact_selection ? width - 16.0f : 196.0f,
            22.0f};

        if (layout.options_expanded)
        {
            float orientation_title_width = 0.0f;
            float pivot_title_width = 0.0f;
            float orientation_option_width = 0.0f;
            float pivot_option_width = 0.0f;
            float orientation_label_height = 14.0f;
            float pivot_label_height = 14.0f;
            float orientation_row_y;
            float pivot_row_y;
            float orientation_row_height;
            float pivot_row_height;
            float orientation_select_width;
            float pivot_select_width;
            bool orientation_inline;
            bool pivot_inline;

            if (sandbox3d_editor_layout_measure_max_text_width(
                    ui_context,
                    (const char* const[]) {"Orientation"},
                    1U,
                    0.82f,
                    &orientation_title_width) != HENKA_SUCCESS ||
                sandbox3d_editor_layout_measure_max_text_width(
                    ui_context,
                    (const char* const[]) {"Pivot"},
                    1U,
                    0.82f,
                    &pivot_title_width) != HENKA_SUCCESS ||
                sandbox3d_editor_layout_measure_max_text_width(
                    ui_context,
                    sandbox3d_modeling_toolbar_orientation_labels,
                    sizeof(sandbox3d_modeling_toolbar_orientation_labels) /
                        sizeof(sandbox3d_modeling_toolbar_orientation_labels[0]),
                    1.0f,
                    &orientation_option_width) != HENKA_SUCCESS ||
                sandbox3d_editor_layout_measure_max_text_width(
                    ui_context,
                    sandbox3d_modeling_toolbar_pivot_labels,
                    sizeof(sandbox3d_modeling_toolbar_pivot_labels) /
                        sizeof(sandbox3d_modeling_toolbar_pivot_labels[0]),
                    1.0f,
                    &pivot_option_width) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }

            orientation_title_width += 8.0f;
            pivot_title_width += 8.0f;
            orientation_select_width = inner_width - orientation_title_width - 4.0f;
            pivot_select_width = inner_width - pivot_title_width - 4.0f;
            orientation_inline = orientation_select_width / 3.0f >=
                orientation_option_width + 4.0f;
            pivot_inline = pivot_select_width / 3.0f >= pivot_option_width + 4.0f;

            orientation_row_y = y + 64.0f;
            if (orientation_inline)
            {
                layout.orientation_label = (henka_ui_rect){
                    inner_x,
                    orientation_row_y + 3.0f,
                    orientation_title_width - 8.0f,
                    orientation_label_height};
                layout.orientation = (henka_ui_rect){
                    inner_x + orientation_title_width,
                    orientation_row_y,
                    orientation_select_width,
                    22.0f};
                orientation_row_height = 22.0f;
            }
            else
            {
                layout.orientation_label = (henka_ui_rect){
                    inner_x,
                    orientation_row_y,
                    inner_width,
                    orientation_label_height};
                layout.orientation = (henka_ui_rect){
                    inner_x,
                    orientation_row_y + orientation_label_height + 2.0f,
                    inner_width,
                    22.0f};
                orientation_row_height = orientation_label_height + 24.0f;
            }

            pivot_row_y = orientation_row_y + orientation_row_height + 2.0f;
            if (pivot_inline)
            {
                layout.pivot_label = (henka_ui_rect){
                    inner_x,
                    pivot_row_y + 3.0f,
                    pivot_title_width - 8.0f,
                    pivot_label_height};
                layout.pivot = (henka_ui_rect){
                    inner_x + pivot_title_width,
                    pivot_row_y,
                    pivot_select_width,
                    22.0f};
                pivot_row_height = 22.0f;
            }
            else
            {
                layout.pivot_label = (henka_ui_rect){
                    inner_x,
                    pivot_row_y,
                    inner_width,
                    pivot_label_height};
                layout.pivot = (henka_ui_rect){
                    inner_x,
                    pivot_row_y + pivot_label_height + 2.0f,
                    inner_width,
                    22.0f};
                pivot_row_height = pivot_label_height + 24.0f;
            }
            tool_y = pivot_row_y + pivot_row_height + 8.0f;
        }
        else
        {
            tool_y = layout.selection_mode.y +
                layout.selection_mode.height + 8.0f;
        }
    }
    else
    {
        float orientation_title_width = 0.0f;
        float pivot_title_width = 0.0f;

        if (sandbox3d_editor_layout_measure_max_text_width(
                ui_context,
                (const char* const[]) {"Orientation"},
                1U,
                0.82f,
                &orientation_title_width) != HENKA_SUCCESS ||
            sandbox3d_editor_layout_measure_max_text_width(
                ui_context,
                (const char* const[]) {"Pivot"},
                1U,
                0.82f,
                &pivot_title_width) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        layout.selection_mode = (henka_ui_rect){
            layout.bounds.x + 74.0f, y + 20.0f, 196.0f, 22.0f};
        layout.orientation_label = (henka_ui_rect){
            layout.bounds.x + 278.0f, y + 24.0f,
            orientation_title_width, 14.0f};
        layout.orientation = (henka_ui_rect){
            layout.bounds.x + 352.0f, y + 20.0f, 142.0f, 22.0f};
        layout.pivot_label = (henka_ui_rect){
            layout.bounds.x + 8.0f, y + 54.0f, pivot_title_width, 14.0f};
        layout.pivot = (henka_ui_rect){
            layout.bounds.x + 74.0f, y + 50.0f, 196.0f, 22.0f};
        tool_y = y + 80.0f;
    }

    {
        const float gap = 4.0f;
        const float button_width =
            (width - gap * (float)(layout.tool_columns - 1U)) /
            (float)layout.tool_columns;

        for (index = 0U; index < tool_count; ++index)
        {
            const size_t column = index % layout.tool_columns;
            const size_t row = index / layout.tool_columns;

            layout.tool_buttons[index] = (henka_ui_rect){
                layout.bounds.x + (button_width + gap) * (float)column,
                tool_y + 32.0f * (float)row,
                button_width,
                28.0f};
        }
    }

    tool_bottom = sandbox3d_editor_layout_toolbar_grid_end_y(
        tool_y, layout.tool_rows);
    if (compact_toolbar)
    {
        layout.state_summary = (henka_ui_rect){
            layout.bounds.x + 8.0f,
            tool_bottom + 8.0f,
            width - 16.0f,
            18.0f};
    }
    else
    {
        layout.state_summary = (henka_ui_rect){
            layout.bounds.x + 8.0f,
            fmaxf(y + 112.0f, tool_bottom + 4.0f),
            width - 16.0f,
            18.0f};
    }
    summary_bottom = layout.state_summary.y + layout.state_summary.height;
    toolbar_height = fmaxf(
        compact_toolbar ? 0.0f : 136.0f,
        summary_bottom - y + (compact_toolbar ? 8.0f : 6.0f));
    layout.bounds.height = toolbar_height;

    *out_layout = layout;
    return HENKA_SUCCESS;
}

bool sandbox3d_editor_layout_modeling_toolbar_contains_interactive_point(
    const sandbox3d_modeling_toolbar_layout* layout,
    henka_vec2 point)
{
    size_t index;

    if (layout == NULL ||
        !sandbox3d_editor_layout_float_is_valid(point.x) ||
        !sandbox3d_editor_layout_float_is_valid(point.y) ||
        !sandbox3d_editor_layout_point_in_nonempty_rect(layout->bounds, point))
    {
        return false;
    }

    if (sandbox3d_editor_layout_point_in_nonempty_rect(layout->selection_mode, point) ||
        sandbox3d_editor_layout_point_in_nonempty_rect(layout->options_toggle, point) ||
        sandbox3d_editor_layout_point_in_nonempty_rect(layout->orientation, point) ||
        sandbox3d_editor_layout_point_in_nonempty_rect(layout->pivot, point))
    {
        return true;
    }

    for (index = 0U; index < sizeof(layout->tool_buttons) / sizeof(layout->tool_buttons[0]); ++index)
    {
        if (sandbox3d_editor_layout_point_in_nonempty_rect(
                layout->tool_buttons[index],
                point))
        {
            return true;
        }
    }
    return false;
}

henka_viewport sandbox3d_editor_frame_layout_navigation_viewport(
    const henka_ui_context* ui_context,
    const sandbox3d_editor_frame_layout* layout,
    bool authoring_available,
    bool options_expanded,
    uint16_t app_scale_percent)
{
    henka_viewport navigation_viewport;
    const henka_ui_rect toolbar = layout == NULL
        ? (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f}
        : sandbox3d_editor_layout_modeling_toolbar_bounds(
            ui_context,
            layout->scene_frame,
            app_scale_percent,
            authoring_available,
            options_expanded);
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

static henka_result sandbox3d_editor_layout_text_control_row_impl(
    const henka_ui_context* ui_context,
    henka_ui_rect bounds,
    const char* const* labels,
    size_t item_count,
    float scale,
    float minimum_item_width,
    float horizontal_padding,
    float gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count)
{
    double total_gap;
    double required_width;
    double available_width;
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
        !sandbox3d_editor_layout_float_is_valid(minimum_item_width) ||
        minimum_item_width < 0.0f ||
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

        if (labels[item_index] == NULL || ui_context == NULL ||
            henka_ui_measure_text_for_context(
                ui_context,
                labels[item_index],
                scale,
                &measured_width,
                &measured_height) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        (void)measured_height;
        required_item_width =
            (double)measured_width + (double)horizontal_padding * 2.0;
        if (required_item_width < (double)minimum_item_width)
        {
            required_item_width = (double)minimum_item_width;
        }
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

    item_x = (double)bounds.x;
    for (item_index = 0U; item_index < item_count; ++item_index)
    {
        const double item_width = (double)required_items[item_index];
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

henka_result sandbox3d_editor_layout_text_control_row_for_context(
    const henka_ui_context* ui_context,
    henka_ui_rect bounds,
    const char* const* labels,
    size_t item_count,
    float scale,
    float minimum_item_width,
    float horizontal_padding,
    float gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_item_count)
{
    if (out_item_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_item_count = 0U;
    if (ui_context == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_editor_layout_text_control_row_impl(
        ui_context,
        bounds,
        labels,
        item_count,
        scale,
        minimum_item_width,
        horizontal_padding,
        gap,
        out_items,
        item_capacity,
        out_item_count);
}

henka_result sandbox3d_editor_layout_text_control_grid_for_context(
    const henka_ui_context* ui_context,
    henka_ui_rect bounds,
    const char* const* labels,
    size_t item_count,
    float scale,
    float minimum_item_width,
    float horizontal_padding,
    float horizontal_gap,
    float row_height,
    float row_gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_row_count)
{
    float required_widths[SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS];
    henka_ui_rect candidates[SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS];
    double bounds_right;
    double bounds_bottom;
    double row_step;
    double used_width = 0.0;
    size_t row_count = 0U;
    size_t item_index;

    if (out_row_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_row_count = 0U;
    if (ui_context == NULL ||
        !sandbox3d_editor_layout_float_is_valid(bounds.x) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.y) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.width) ||
        !sandbox3d_editor_layout_float_is_valid(bounds.height) ||
        bounds.width <= 0.0f || bounds.height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(scale) || scale <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(minimum_item_width) ||
        minimum_item_width < 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(horizontal_padding) ||
        horizontal_padding < 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(horizontal_gap) ||
        horizontal_gap < 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(row_height) ||
        row_height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(row_gap) ||
        row_gap < 0.0f ||
        item_count > SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS ||
        (item_count > 0U &&
            (labels == NULL || out_items == NULL ||
             item_capacity < item_count)))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (item_count == 0U)
    {
        return HENKA_SUCCESS;
    }

    bounds_right = (double)bounds.x + (double)bounds.width;
    bounds_bottom = (double)bounds.y + (double)bounds.height;
    row_step = (double)row_height + (double)row_gap;
    if (!isfinite(bounds_right) || !isfinite(bounds_bottom) ||
        !isfinite(row_step) || row_step <= 0.0)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    for (item_index = 0U; item_index < item_count; ++item_index)
    {
        int measured_width;
        int measured_height;
        double width;

        if (labels[item_index] == NULL ||
            henka_ui_measure_text_for_context(
                ui_context,
                labels[item_index],
                scale,
                &measured_width,
                &measured_height) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        (void)measured_height;
        width = (double)measured_width +
            (double)horizontal_padding * 2.0;
        if (width < (double)minimum_item_width)
        {
            width = (double)minimum_item_width;
        }
        if (!isfinite(width) || width <= 0.0 ||
            width > (double)bounds.width || width > (double)FLT_MAX)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        required_widths[item_index] = (float)width;
    }

    for (item_index = 0U; item_index < item_count; ++item_index)
    {
        const double item_width = (double)required_widths[item_index];
        double item_x;
        double item_y;
        double item_right;
        double item_bottom;

        if (row_count == 0U)
        {
            row_count = 1U;
            used_width = 0.0;
        }
        else
        {
            const double next_width =
                used_width + (used_width > 0.0 ?
                    (double)horizontal_gap : 0.0) + item_width;
            if (next_width > (double)bounds.width)
            {
                ++row_count;
                used_width = 0.0;
            }
        }

        item_x = (double)bounds.x + used_width +
            (used_width > 0.0 ? (double)horizontal_gap : 0.0);
        item_y = (double)bounds.y +
            (double)(row_count - 1U) * row_step;
        item_right = item_x + item_width;
        item_bottom = item_y + (double)row_height;
        if (!isfinite(item_x) || !isfinite(item_y) ||
            !isfinite(item_right) || !isfinite(item_bottom) ||
            item_x < -(double)FLT_MAX || item_x > (double)FLT_MAX ||
            item_y < -(double)FLT_MAX || item_y > (double)FLT_MAX ||
            item_right > bounds_right || item_bottom > bounds_bottom)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        candidates[item_index] = (henka_ui_rect){
            (float)item_x,
            (float)item_y,
            required_widths[item_index],
            row_height};
        used_width = item_right - (double)bounds.x;
    }

    for (item_index = 0U; item_index < item_count; ++item_index)
    {
        out_items[item_index] = candidates[item_index];
    }
    *out_row_count = row_count;
    return HENKA_SUCCESS;
}

static bool sandbox3d_editor_layout_rect_contains_rect(
    henka_ui_rect outer,
    henka_ui_rect inner)
{
    const double outer_right = (double)outer.x + (double)outer.width;
    const double outer_bottom = (double)outer.y + (double)outer.height;
    const double inner_right = (double)inner.x + (double)inner.width;
    const double inner_bottom = (double)inner.y + (double)inner.height;

    return
        sandbox3d_editor_layout_float_is_valid(outer.x) &&
        sandbox3d_editor_layout_float_is_valid(outer.y) &&
        sandbox3d_editor_layout_float_is_valid(outer.width) &&
        sandbox3d_editor_layout_float_is_valid(outer.height) &&
        sandbox3d_editor_layout_float_is_valid(inner.x) &&
        sandbox3d_editor_layout_float_is_valid(inner.y) &&
        sandbox3d_editor_layout_float_is_valid(inner.width) &&
        sandbox3d_editor_layout_float_is_valid(inner.height) &&
        outer.width > 0.0f && outer.height > 0.0f &&
        inner.width > 0.0f && inner.height > 0.0f &&
        (double)inner.x >= (double)outer.x &&
        (double)inner.y >= (double)outer.y &&
        inner_right <= outer_right + 0.01 &&
        inner_bottom <= outer_bottom + 0.01;
}

bool sandbox3d_editor_layout_flow_text_control_grid_for_context(
    henka_ui_context* ui_context,
    henka_ui_rect viewport,
    float flow_row_spacing,
    const char* const* labels,
    size_t item_count,
    float scale,
    float minimum_item_width,
    float horizontal_padding,
    float horizontal_gap,
    float row_height,
    float row_gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_row_count,
    bool* out_all_visible)
{
    henka_ui_rect candidate_items[SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS];
    henka_ui_rect first_flow_row;
    double effective_row_gap;
    double maximum_grid_height;
    double flow_row_height;
    size_t row_count = 0U;
    size_t row_index;
    bool visible = true;
    bool row_intersects_viewport = false;

    if (out_row_count != NULL)
    {
        *out_row_count = 0U;
    }
    if (out_all_visible != NULL)
    {
        *out_all_visible = false;
    }
    if (ui_context == NULL || labels == NULL || item_count == 0U ||
        item_count > SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS ||
        out_items == NULL || item_capacity < item_count ||
        out_row_count == NULL || out_all_visible == NULL ||
        !sandbox3d_editor_layout_float_is_valid(viewport.x) ||
        !sandbox3d_editor_layout_float_is_valid(viewport.y) ||
        !sandbox3d_editor_layout_float_is_valid(viewport.width) ||
        !sandbox3d_editor_layout_float_is_valid(viewport.height) ||
        viewport.width <= 0.0f || viewport.height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(flow_row_spacing) ||
        flow_row_spacing < 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(row_height) ||
        row_height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(row_gap) || row_gap < 0.0f)
    {
        return false;
    }

    effective_row_gap = (double)row_gap + (double)flow_row_spacing;
    flow_row_height = (double)row_height + (double)row_gap;
    maximum_grid_height = (double)item_count * (double)row_height +
        (double)(item_count - 1U) * effective_row_gap;
    if (!isfinite(effective_row_gap) ||
        !isfinite(flow_row_height) ||
        !isfinite(maximum_grid_height) ||
        effective_row_gap > (double)FLT_MAX ||
        flow_row_height > (double)FLT_MAX ||
        maximum_grid_height <= 0.0 ||
        maximum_grid_height > (double)FLT_MAX)
    {
        return false;
    }

    /* Preflight measured geometry before advancing the active flow. Its
     * height is the maximum bounded row count, not the remaining viewport;
     * scroll position must not change whether later rows are reserved. */
    if (sandbox3d_editor_layout_text_control_grid_for_context(
            ui_context,
            (henka_ui_rect){
                viewport.x,
                viewport.y,
                viewport.width,
                (float)maximum_grid_height},
            labels,
            item_count,
            scale,
            minimum_item_width,
            horizontal_padding,
            horizontal_gap,
            row_height,
            (float)effective_row_gap,
            candidate_items,
            SANDBOX3D_EDITOR_LAYOUT_MAX_TOOL_ITEMS,
            &row_count) != HENKA_SUCCESS ||
        row_count == 0U)
    {
        return false;
    }

    if (henka_ui_flow_next_row(
            ui_context,
            (float)flow_row_height,
            0U,
            &first_flow_row,
            &row_intersects_viewport) != HENKA_SUCCESS)
    {
        return false;
    }
    (void)row_intersects_viewport;

    for (row_index = 1U; row_index < row_count; ++row_index)
    {
        henka_ui_rect reserved_row;
        const double expected_y = (double)first_flow_row.y +
            (double)row_index *
                ((double)row_height + (double)row_gap +
                 (double)flow_row_spacing);
        if (!isfinite(expected_y) || expected_y < -(double)FLT_MAX ||
            expected_y > (double)FLT_MAX ||
            henka_ui_flow_next_row(
                ui_context,
                (float)flow_row_height,
                0U,
                &reserved_row,
                &row_intersects_viewport) != HENKA_SUCCESS ||
            fabs((double)reserved_row.y - expected_y) > 0.5 ||
            fabs((double)reserved_row.x - (double)first_flow_row.x) > 0.5 ||
            fabs((double)reserved_row.width -
                (double)first_flow_row.width) > 0.5)
        {
            return false;
        }
        (void)row_intersects_viewport;
    }

    {
        const double y_offset =
            (double)first_flow_row.y - (double)viewport.y;
        size_t item_index;
        for (item_index = 0U; item_index < item_count; ++item_index)
        {
            const double item_y =
                (double)candidate_items[item_index].y + y_offset;
            if (!isfinite(item_y) || item_y < -(double)FLT_MAX ||
                item_y > (double)FLT_MAX)
            {
                return false;
            }
            candidate_items[item_index].y = (float)item_y;
            if (!sandbox3d_editor_layout_rect_contains_rect(
                    viewport,
                    candidate_items[item_index]))
            {
                visible = false;
            }
        }
    }

    memcpy(out_items, candidate_items, item_count * sizeof(*out_items));
    *out_row_count = row_count;
    *out_all_visible = visible;
    return true;
}

bool sandbox3d_editor_layout_flow_button_grid_for_context(
    henka_ui_context* ui_context,
    henka_ui_rect viewport,
    float flow_row_spacing,
    const char* const* labels,
    size_t item_count,
    float scale,
    float minimum_item_width,
    float horizontal_gap,
    float row_height,
    float row_gap,
    henka_ui_rect* out_items,
    size_t item_capacity,
    size_t* out_row_count,
    bool* out_all_visible)
{
    return sandbox3d_editor_layout_flow_text_control_grid_for_context(
        ui_context,
        viewport,
        flow_row_spacing,
        labels,
        item_count,
        scale,
        minimum_item_width,
        HENKA_UI_BUTTON_TEXT_HORIZONTAL_PADDING,
        horizontal_gap,
        row_height,
        row_gap,
        out_items,
        item_capacity,
        out_row_count,
        out_all_visible);
}

static const char* const g_sandbox3d_editor_utility_tab_labels[
    SANDBOX3D_EDITOR_UTILITY_TAB_COUNT] = {
        "Help",
        "Scene Legend",
        "Object Info",
        "Terrain",
        "Assets",
        "Paths",
        "Settings",
        "Diagnostics",
        "Transform QA",
        "Physics QA"};

const char* sandbox3d_editor_utility_tab_label(size_t index)
{
    return index < SANDBOX3D_EDITOR_UTILITY_TAB_COUNT
        ? g_sandbox3d_editor_utility_tab_labels[index]
        : NULL;
}

henka_result sandbox3d_editor_layout_utility_tabs_for_context(
    const henka_ui_context* ui_context,
    henka_ui_rect panel_bounds,
    float app_ui_scale,
    henka_ui_rect* out_tab_rects,
    size_t tab_rect_capacity,
    size_t* out_row_count,
    float* out_row_height)
{
    const henka_ui_rect navigation_bounds = {
        panel_bounds.x + 14.0f,
        panel_bounds.y + 38.0f,
        panel_bounds.width - 28.0f,
        panel_bounds.height - 38.0f};
    henka_ui_rect candidate_tabs[SANDBOX3D_EDITOR_UTILITY_TAB_COUNT];
    size_t candidate_rows = 0U;
    float candidate_row_height = 24.0f;
    henka_result result;

    if (out_row_count != NULL)
    {
        *out_row_count = 0U;
    }
    if (out_row_height != NULL)
    {
        *out_row_height = 0.0f;
    }
    if (ui_context == NULL || out_tab_rects == NULL ||
        out_row_count == NULL || out_row_height == NULL ||
        tab_rect_capacity < SANDBOX3D_EDITOR_UTILITY_TAB_COUNT ||
        !sandbox3d_editor_layout_float_is_valid(app_ui_scale) ||
        app_ui_scale < 1.0f || app_ui_scale > 2.0f ||
        !sandbox3d_editor_layout_float_is_valid(panel_bounds.x) ||
        !sandbox3d_editor_layout_float_is_valid(panel_bounds.y) ||
        !sandbox3d_editor_layout_float_is_valid(panel_bounds.width) ||
        !sandbox3d_editor_layout_float_is_valid(panel_bounds.height) ||
        panel_bounds.width <= 28.0f || panel_bounds.height <= 38.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = sandbox3d_editor_layout_text_control_grid_for_context(
        ui_context,
        navigation_bounds,
        g_sandbox3d_editor_utility_tab_labels,
        SANDBOX3D_EDITOR_UTILITY_TAB_COUNT,
        1.0f,
        0.0f,
        12.0f,
        8.0f,
        candidate_row_height,
        0.0f,
        candidate_tabs,
        SANDBOX3D_EDITOR_UTILITY_TAB_COUNT,
        &candidate_rows);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    if (panel_bounds.height - 38.0f -
            (float)candidate_rows * candidate_row_height - 14.0f < 44.0f)
    {
        const float available_for_rows = panel_bounds.height - 38.0f - 14.0f - 44.0f;
        const float minimum_hit_height = 32.0f / app_ui_scale;

        if (candidate_rows == 0U || available_for_rows <= 0.0f)
        {
            return HENKA_ERROR_LIMIT;
        }
        candidate_row_height = available_for_rows / (float)candidate_rows;
        if (candidate_row_height > 24.0f)
        {
            candidate_row_height = 24.0f;
        }
        if (!isfinite(candidate_row_height) ||
            candidate_row_height < minimum_hit_height)
        {
            return HENKA_ERROR_LIMIT;
        }

        result = sandbox3d_editor_layout_text_control_grid_for_context(
            ui_context,
            navigation_bounds,
            g_sandbox3d_editor_utility_tab_labels,
            SANDBOX3D_EDITOR_UTILITY_TAB_COUNT,
            1.0f,
            0.0f,
            12.0f,
            8.0f,
            candidate_row_height,
            0.0f,
            candidate_tabs,
            SANDBOX3D_EDITOR_UTILITY_TAB_COUNT,
            &candidate_rows);
        if (result != HENKA_SUCCESS || candidate_rows == 0U ||
            panel_bounds.height - 38.0f -
                (float)candidate_rows * candidate_row_height - 14.0f < 44.0f)
        {
            return result != HENKA_SUCCESS ? result : HENKA_ERROR_LIMIT;
        }
    }

    memcpy(
        out_tab_rects,
        candidate_tabs,
        sizeof(candidate_tabs));
    *out_row_count = candidate_rows;
    *out_row_height = candidate_row_height;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_details_footer_for_context(
    const henka_ui_context* ui_context,
    henka_ui_rect panel_bounds,
    sandbox3d_editor_details_footer_layout* out_layout)
{
    static const char* const labels[] = {"Save Project", "Reload Project"};
    henka_ui_rect measured[2];
    sandbox3d_editor_details_footer_layout candidate;
    const henka_ui_rect inner = {
        panel_bounds.x + 14.0f,
        panel_bounds.y + 14.0f,
        panel_bounds.width - 40.0f,
        panel_bounds.height - 28.0f};
    size_t measured_rows = 0U;
    float common_width;
    float content_height;
    float top;
    double panel_right;
    double panel_bottom;

    if (out_layout == NULL || ui_context == NULL ||
        !sandbox3d_editor_layout_float_is_valid(panel_bounds.x) ||
        !sandbox3d_editor_layout_float_is_valid(panel_bounds.y) ||
        !sandbox3d_editor_layout_float_is_valid(panel_bounds.width) ||
        !sandbox3d_editor_layout_float_is_valid(panel_bounds.height) ||
        panel_bounds.width <= 40.0f || panel_bounds.height <= 28.0f ||
        inner.width <= 0.0f || inner.height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (sandbox3d_editor_layout_text_control_grid_for_context(
            ui_context,
            inner,
            labels,
            sizeof(labels) / sizeof(labels[0]),
            1.0f,
            140.0f,
            12.0f,
            8.0f,
            24.0f,
            4.0f,
            measured,
            sizeof(measured) / sizeof(measured[0]),
            &measured_rows) != HENKA_SUCCESS ||
        measured_rows == 0U || measured_rows > 2U)
    {
        return HENKA_ERROR_LIMIT;
    }

    /* Keep both buttons the same size. If the measured one-row placement
     * cannot accommodate that common width, use the stacked presentation. */
    common_width = fmaxf(measured[0].width, measured[1].width);
    candidate.row_count = measured_rows;
    if (candidate.row_count == 1U &&
        common_width * 2.0f + 8.0f > inner.width)
    {
        candidate.row_count = 2U;
    }
    if (!sandbox3d_editor_layout_float_is_valid(common_width) ||
        common_width < 140.0f || common_width > inner.width)
    {
        return HENKA_ERROR_LIMIT;
    }

    content_height = (float)candidate.row_count * 24.0f +
        (float)(candidate.row_count - 1U) * 4.0f;
    if (content_height > inner.height)
    {
        return HENKA_ERROR_LIMIT;
    }
    top = panel_bounds.y + panel_bounds.height - 14.0f - content_height;
    candidate.save_project = (henka_ui_rect){
        inner.x,
        top,
        common_width,
        24.0f};
    candidate.reload_project = (henka_ui_rect){
        candidate.row_count == 1U
            ? inner.x + common_width + 8.0f
            : inner.x,
        top + (candidate.row_count == 1U ? 0.0f : 28.0f),
        common_width,
        24.0f};
    candidate.scroll_reserve = content_height + 10.0f;

    if (!sandbox3d_editor_layout_float_is_valid(candidate.scroll_reserve) ||
        !isfinite((double)panel_bounds.x + (double)panel_bounds.width) ||
        !isfinite((double)panel_bounds.y + (double)panel_bounds.height))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    panel_right = (double)panel_bounds.x + (double)panel_bounds.width;
    panel_bottom = (double)panel_bounds.y + (double)panel_bounds.height;
    if ((double)candidate.save_project.x < (double)panel_bounds.x ||
        (double)candidate.reload_project.x < (double)panel_bounds.x ||
        (double)candidate.save_project.y < (double)panel_bounds.y ||
        (double)candidate.reload_project.y < (double)panel_bounds.y ||
        (double)candidate.save_project.x + (double)candidate.save_project.width > panel_right ||
        (double)candidate.reload_project.x + (double)candidate.reload_project.width > panel_right ||
        (double)candidate.save_project.y + (double)candidate.save_project.height > panel_bottom ||
        (double)candidate.reload_project.y + (double)candidate.reload_project.height > panel_bottom)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    *out_layout = candidate;
    return HENKA_SUCCESS;
}

static bool sandbox3d_editor_layout_rect_has_area(henka_ui_rect rect)
{
    return sandbox3d_editor_layout_float_is_valid(rect.x) &&
        sandbox3d_editor_layout_float_is_valid(rect.y) &&
        sandbox3d_editor_layout_float_is_valid(rect.width) &&
        sandbox3d_editor_layout_float_is_valid(rect.height) &&
        rect.width > 0.0f && rect.height > 0.0f;
}

float sandbox3d_editor_layout_details_empty_selection_text_y(
    henka_ui_rect panel_bounds,
    const sandbox3d_editor_details_scene_actions_layout* scene_actions_layout)
{
    if (scene_actions_layout != NULL && scene_actions_layout->visible)
    {
        return scene_actions_layout->scroll_viewport.y + 2.0f;
    }
    return panel_bounds.y + 38.0f;
}

henka_result sandbox3d_editor_layout_details_scene_actions(
    henka_ui_rect panel_bounds,
    henka_ui_rect base_scroll_viewport,
    bool controls_available,
    bool path_editor_open,
    sandbox3d_editor_details_scene_actions_layout* out_layout)
{
    sandbox3d_editor_details_scene_actions_layout candidate = {0};
    const double panel_bottom =
        (double)panel_bounds.y + (double)panel_bounds.height;
    const double row_y = (double)panel_bounds.y + 38.0;
    const double compact_gap = 4.0;
    const double button_min_width = 56.0;
    const double two_column_min_width =
        button_min_width * 2.0 + compact_gap;
    const double three_column_min_width =
        button_min_width * 3.0 + compact_gap * 2.0;
    const double one_row_min_width =
        button_min_width * 5.0 + compact_gap * 4.0;
    const double form_path_label_width = 52.0;
    const double form_path_min_width = 80.0;
    const double form_button_min_width = 64.0;
    const bool panel_values_valid =
        sandbox3d_editor_layout_float_is_valid(panel_bounds.x) &&
        sandbox3d_editor_layout_float_is_valid(panel_bounds.y) &&
        sandbox3d_editor_layout_float_is_valid(panel_bounds.width) &&
        sandbox3d_editor_layout_float_is_valid(panel_bounds.height) &&
        sandbox3d_editor_layout_float_is_valid(base_scroll_viewport.x) &&
        sandbox3d_editor_layout_float_is_valid(base_scroll_viewport.y) &&
        sandbox3d_editor_layout_float_is_valid(base_scroll_viewport.width) &&
        sandbox3d_editor_layout_float_is_valid(base_scroll_viewport.height);

    if (out_layout == NULL || !panel_values_valid ||
        panel_bounds.width < 0.0f || panel_bounds.height < 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    candidate.scroll_viewport = base_scroll_viewport;
    if (candidate.scroll_viewport.width < 0.0f)
    {
        candidate.scroll_viewport.width = 0.0f;
    }
    if (candidate.scroll_viewport.height < 0.0f)
    {
        candidate.scroll_viewport.height = 0.0f;
    }
    if (controls_available && panel_bounds.width > 28.0f &&
        row_y <= (double)FLT_MAX)
    {
        const double row_width = (double)panel_bounds.width - 28.0;
        const bool use_compact_grid = path_editor_open ||
            row_width < one_row_min_width;
        const size_t button_columns = row_width >= one_row_min_width
            ? 5U
            : (row_width >= three_column_min_width ? 3U : 2U);
        const size_t button_rows =
            (5U + button_columns - 1U) / button_columns;
        const double action_row_height = path_editor_open
            ? 56.0
            : (double)button_rows * 26.0 +
                (double)(button_rows - 1U) * compact_gap;
        const double row_bottom = row_y + action_row_height;
        const double required_scroll_top = row_bottom + 6.0;

        candidate.path_editor_open = path_editor_open;
        if (row_width >= two_column_min_width &&
            row_bottom <= panel_bottom - 8.0 &&
            row_bottom <= (double)FLT_MAX)
        {
            const float row_x = panel_bounds.x + 14.0f;
            const float row_height = (float)action_row_height;
            candidate.action_row = (henka_ui_rect){
                row_x,
                (float)row_y,
                (float)row_width,
                row_height};
            candidate.visible = true;
            candidate.compact = use_compact_grid;

            if (path_editor_open)
            {
                const double form_button_width = fmin(
                    96.0,
                    (row_width - compact_gap) * 0.5);
                const double form_buttons_width =
                    form_button_width * 2.0 + compact_gap;
                if (row_width >= form_path_label_width + compact_gap +
                        form_path_min_width &&
                    form_button_width >= form_button_min_width &&
                    form_buttons_width <= row_width)
                {
                    const double form_button_x = row_x +
                        (row_width - form_buttons_width) * 0.5;
                    const double second_row_y = row_y + 30.0;
                    candidate.path_label = (henka_ui_rect){
                        row_x,
                        (float)row_y,
                        (float)form_path_label_width,
                        26.0f};
                    candidate.path_field = (henka_ui_rect){
                        row_x + (float)(form_path_label_width + compact_gap),
                        (float)row_y,
                        (float)(row_width - form_path_label_width -
                            compact_gap),
                        26.0f};
                    candidate.path_confirm_button = (henka_ui_rect){
                        (float)form_button_x,
                        (float)second_row_y,
                        (float)form_button_width,
                        26.0f};
                    candidate.path_cancel_button = (henka_ui_rect){
                        (float)(form_button_x + form_button_width +
                            compact_gap),
                        (float)second_row_y,
                        (float)form_button_width,
                        26.0f};
                    candidate.path_editor_available = true;
                }
            }
            else
            {
                henka_ui_rect* buttons[5] = {
                    &candidate.new_scene_button,
                    &candidate.open_scene_button,
                    &candidate.save_scene_button,
                    &candidate.save_as_button,
                    &candidate.reload_scene_button};
                const double button_width = (row_width -
                    compact_gap * (double)(button_columns - 1U)) /
                    (double)button_columns;
                size_t button_index;

                for (button_index = 0U; button_index < 5U; ++button_index)
                {
                    const size_t column = button_index % button_columns;
                    const size_t row = button_index / button_columns;
                    *buttons[button_index] = (henka_ui_rect){
                        row_x + (float)((button_width + compact_gap) *
                            (double)column),
                        (float)(row_y + (26.0 + compact_gap) *
                            (double)row),
                        (float)button_width,
                        26.0f};
                }
            }

            if (required_scroll_top >
                (double)candidate.scroll_viewport.y)
            {
                const double shift = required_scroll_top -
                    (double)candidate.scroll_viewport.y;
                const double remaining_height =
                    (double)candidate.scroll_viewport.height - shift;
                if (!isfinite(shift) || !isfinite(remaining_height) ||
                    required_scroll_top > (double)FLT_MAX)
                {
                    return HENKA_ERROR_NUMERIC_RANGE;
                }
                candidate.scroll_viewport.y = (float)required_scroll_top;
                candidate.scroll_viewport.height = remaining_height > 0.0
                    ? (float)remaining_height
                    : 0.0f;
            }
        }
    }

    if (!sandbox3d_editor_layout_float_is_valid(
            candidate.scroll_viewport.x) ||
        !sandbox3d_editor_layout_float_is_valid(
            candidate.scroll_viewport.y) ||
        !sandbox3d_editor_layout_float_is_valid(
            candidate.scroll_viewport.width) ||
        !sandbox3d_editor_layout_float_is_valid(
            candidate.scroll_viewport.height))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    if (candidate.visible)
    {
        if (!sandbox3d_editor_layout_rect_has_area(candidate.action_row) ||
            !sandbox3d_editor_layout_rect_has_area(candidate.scroll_viewport))
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        if (candidate.path_editor_open)
        {
            if (candidate.path_editor_available &&
                (!sandbox3d_editor_layout_rect_has_area(
                    candidate.path_label) ||
                 !sandbox3d_editor_layout_rect_has_area(
                    candidate.path_field) ||
                 !sandbox3d_editor_layout_rect_has_area(
                    candidate.path_confirm_button) ||
                 !sandbox3d_editor_layout_rect_has_area(
                    candidate.path_cancel_button)))
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
        }
        else if (!sandbox3d_editor_layout_rect_has_area(
                     candidate.new_scene_button) ||
                 !sandbox3d_editor_layout_rect_has_area(
                     candidate.open_scene_button) ||
                 !sandbox3d_editor_layout_rect_has_area(
                     candidate.save_scene_button) ||
                 !sandbox3d_editor_layout_rect_has_area(
                     candidate.save_as_button) ||
                 !sandbox3d_editor_layout_rect_has_area(
                     candidate.reload_scene_button))
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
    }

    *out_layout = candidate;
    return HENKA_SUCCESS;
}

static bool sandbox3d_editor_layout_is_wrap_space(char value)
{
    return value == ' ' || value == '\t';
}

#ifdef HENKA_SANDBOX3D_TEST_SEAM
/* Regression-only coverage for the retired fixed-column display helpers. */
static henka_result sandbox3d_editor_layout_process_wrapped_text(
    const char* text,
    size_t max_columns,
    char* out_text,
    size_t out_capacity,
    size_t* out_required_bytes,
    size_t* out_line_count)
{
    const size_t text_length = strlen(text);
    size_t position = 0U;
    size_t written = 0U;
    size_t lines = 1U;

    if (out_text == NULL && out_capacity != 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    while (position < text_length)
    {
        size_t end = position;
        size_t columns = 0U;
        size_t segment_end;
        size_t next_position;
        bool line_break;

        if (text[position] == '\n')
        {
            segment_end = position;
            next_position = position + 1U;
            line_break = true;
        }
        else
        {
            while (end < text_length && text[end] != '\n' && columns < max_columns)
            {
                ++end;
                ++columns;
            }

            if (end < text_length && text[end] == '\n')
            {
                segment_end = end;
                next_position = end + 1U;
                line_break = true;
            }
            else if (end < text_length)
            {
                size_t split = end;
                while (split > position && !sandbox3d_editor_layout_is_wrap_space(text[split - 1U]))
                {
                    --split;
                }
                if (split > position)
                {
                    segment_end = split - 1U;
                    next_position = split;
                }
                else
                {
                    segment_end = end;
                    next_position = end;
                }
                while (next_position < text_length &&
                       sandbox3d_editor_layout_is_wrap_space(text[next_position]))
                {
                    ++next_position;
                }
                line_break = next_position < text_length;
            }
            else
            {
                segment_end = end;
                next_position = end;
                line_break = false;
            }
        }

        if (segment_end < position ||
            segment_end - position > SIZE_MAX - written)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        if (out_text != NULL && segment_end > position)
        {
            memcpy(out_text + written, text + position, segment_end - position);
        }
        written += segment_end - position;

        if (line_break)
        {
            if (written == SIZE_MAX || lines == SIZE_MAX)
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            if (out_text != NULL)
            {
                out_text[written] = '\n';
            }
            ++written;
            ++lines;
        }

        if (next_position <= position && next_position < text_length)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        position = next_position;
    }

    if (out_required_bytes != NULL)
    {
        *out_required_bytes = written;
    }
    if (out_line_count != NULL)
    {
        *out_line_count = lines;
    }
    if (out_text != NULL)
    {
        out_text[written] = '\0';
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_measure_wrapped_text(
    const char* text,
    size_t max_columns,
    size_t* out_line_count)
{
    if (out_line_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_line_count = 0U;
    if (text == NULL || max_columns == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_editor_layout_process_wrapped_text(
        text,
        max_columns,
        NULL,
        0U,
        NULL,
        out_line_count);
}

henka_result sandbox3d_editor_layout_wrap_text(
    const char* text,
    size_t max_columns,
    char* out_text,
    size_t out_capacity,
    size_t* out_line_count)
{
    henka_result result;
    size_t required_bytes = 0U;
    size_t line_count = 0U;

    if (out_line_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_line_count = 0U;
    if (text == NULL || max_columns == 0U || out_text == NULL || out_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = sandbox3d_editor_layout_process_wrapped_text(
        text,
        max_columns,
        NULL,
        0U,
        &required_bytes,
        &line_count);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (required_bytes >= out_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }

    result = sandbox3d_editor_layout_process_wrapped_text(
        text,
        max_columns,
        out_text,
        out_capacity,
        NULL,
        &line_count);
    if (result == HENKA_SUCCESS)
    {
        *out_line_count = line_count;
    }
    return result;
}

henka_result sandbox3d_editor_layout_limit_wrapped_text(
    const char* wrapped_text,
    size_t maximum_line_count,
    size_t maximum_columns,
    char* out_text,
    size_t out_capacity,
    size_t* out_line_count,
    bool* out_truncated)
{
    size_t text_length;
    size_t line_count = 1U;
    size_t index;
    size_t prefix_line_count;
    size_t prefix_bytes = 0U;
    size_t cursor = 0U;
    size_t required_bytes;
    size_t ellipsis_length;

    if (out_line_count == NULL || out_truncated == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_line_count = 0U;
    *out_truncated = false;
    if (wrapped_text == NULL || maximum_line_count == 0U || maximum_columns == 0U ||
        out_text == NULL || out_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    text_length = strlen(wrapped_text);
    for (index = 0U; index < text_length; ++index)
    {
        if (wrapped_text[index] == '\n')
        {
            if (line_count == SIZE_MAX)
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            ++line_count;
        }
    }

    if (line_count <= maximum_line_count)
    {
        if (text_length == SIZE_MAX || text_length + 1U > out_capacity)
        {
            return HENKA_ERROR_LIMIT;
        }
        memmove(out_text, wrapped_text, text_length + 1U);
        *out_line_count = line_count;
        return HENKA_SUCCESS;
    }

    prefix_line_count = maximum_line_count - 1U;
    ellipsis_length = maximum_columns < 3U ? maximum_columns : 3U;
    for (index = 0U; index < prefix_line_count; ++index)
    {
        const char* newline = strchr(wrapped_text + cursor, '\n');
        if (newline == NULL)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        prefix_bytes = (size_t)(newline - wrapped_text);
        cursor = prefix_bytes + 1U;
    }
    if (prefix_bytes > SIZE_MAX -
        (prefix_line_count > 0U ? ellipsis_length + 2U : ellipsis_length + 1U))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    required_bytes = prefix_bytes +
        (prefix_line_count > 0U ? ellipsis_length + 2U : ellipsis_length + 1U);
    if (required_bytes > out_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }

    memmove(out_text, wrapped_text, prefix_bytes);
    cursor = prefix_bytes;
    if (prefix_line_count > 0U)
    {
        out_text[cursor++] = '\n';
    }
    memcpy(out_text + cursor, "...", ellipsis_length);
    out_text[cursor + ellipsis_length] = '\0';
    *out_line_count = maximum_line_count;
    *out_truncated = true;
    return HENKA_SUCCESS;
}

static size_t sandbox3d_editor_layout_normalized_name_length(
    const char* object_name,
    size_t object_name_length)
{
    size_t index;
    size_t normalized_length = 0U;
    bool previous_was_line_break = false;

    for (index = 0U; index < object_name_length; ++index)
    {
        const char character = object_name[index];
        if (character == '\r' || character == '\n')
        {
            if (!previous_was_line_break)
            {
                ++normalized_length;
            }
            previous_was_line_break = true;
        }
        else
        {
            ++normalized_length;
            previous_was_line_break = false;
        }
    }

    return normalized_length;
}

static void sandbox3d_editor_layout_copy_normalized_name_range(
    const char* object_name,
    size_t object_name_length,
    size_t range_start,
    size_t range_length,
    char* out_text)
{
    size_t source_index = 0U;
    size_t normalized_index = 0U;
    size_t output_index = 0U;
    bool previous_was_line_break = false;

    while (source_index < object_name_length && output_index < range_length)
    {
        char character = object_name[source_index++];
        if (character == '\r' || character == '\n')
        {
            if (previous_was_line_break)
            {
                continue;
            }
            character = ' ';
            previous_was_line_break = true;
        }
        else
        {
            previous_was_line_break = false;
        }

        if (normalized_index >= range_start)
        {
            out_text[output_index++] = character;
        }
        ++normalized_index;
    }
}

henka_result sandbox3d_editor_layout_format_hidden_row_label(
    const char* object_name,
    size_t maximum_columns,
    char* out_text,
    size_t out_capacity,
    bool* out_name_truncated)
{
    static const char hidden_suffix[] = " [Hidden]";
    static const char unnamed_name[] = "(unnamed)";
    static const char compact_unnamed_name[] = "?";
    const size_t suffix_length = sizeof(hidden_suffix) - 1U;
    size_t source_name_length;
    size_t name_length;
    size_t name_columns;
    size_t prefix_length;
    size_t suffix_name_length;
    size_t output_length;
    bool object_name_was_empty = false;
    bool unnamed_name_compacted = false;

    if (object_name == NULL || out_text == NULL || out_name_truncated == NULL ||
        out_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    source_name_length = strlen(object_name);
    if (source_name_length == 0U)
    {
        object_name = unnamed_name;
        source_name_length = sizeof(unnamed_name) - 1U;
        object_name_was_empty = true;
    }
    if (maximum_columns <= suffix_length)
    {
        return HENKA_ERROR_LIMIT;
    }

    name_columns = maximum_columns - suffix_length;
    if (object_name_was_empty &&
        name_columns < sizeof(unnamed_name) - 1U)
    {
        object_name = compact_unnamed_name;
        source_name_length = sizeof(compact_unnamed_name) - 1U;
        unnamed_name_compacted = true;
    }
    name_length = sandbox3d_editor_layout_normalized_name_length(
        object_name,
        source_name_length);
    if (name_length <= name_columns)
    {
        output_length = name_length + suffix_length;
        if (output_length >= out_capacity)
        {
            return HENKA_ERROR_LIMIT;
        }
        sandbox3d_editor_layout_copy_normalized_name_range(
            object_name,
            source_name_length,
            0U,
            name_length,
            out_text);
        memcpy(out_text + name_length, hidden_suffix, sizeof(hidden_suffix));
        *out_name_truncated = unnamed_name_compacted;
        return HENKA_SUCCESS;
    }

    output_length = name_columns + suffix_length;
    if (output_length >= out_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }

    /* Keep the marker intact and use the remaining row budget for a bounded
     * middle ellipsis. A four-column name budget yields "A..."; larger
     * budgets retain both the beginning and end of the canonical name. */
    if (name_columns < 4U)
    {
        prefix_length = name_columns > 1U ? name_columns - 1U : name_columns;
        sandbox3d_editor_layout_copy_normalized_name_range(
            object_name,
            source_name_length,
            0U,
            prefix_length,
            out_text);
        if (name_columns > prefix_length)
        {
            out_text[prefix_length] = '.';
        }
        memcpy(out_text + name_columns, hidden_suffix, sizeof(hidden_suffix));
        *out_name_truncated = true;
        return HENKA_SUCCESS;
    }
    prefix_length = (name_columns - 3U + 1U) / 2U;
    suffix_name_length = name_columns - 3U - prefix_length;

    sandbox3d_editor_layout_copy_normalized_name_range(
        object_name,
        source_name_length,
        0U,
        prefix_length,
        out_text);
    memcpy(out_text + prefix_length, "...", 3U);
    if (suffix_name_length > 0U)
    {
        sandbox3d_editor_layout_copy_normalized_name_range(
            object_name,
            source_name_length,
            name_length - suffix_name_length,
            suffix_name_length,
            out_text + prefix_length + 3U);
    }
    memcpy(out_text + name_columns, hidden_suffix, sizeof(hidden_suffix));
    *out_name_truncated = true;
    return HENKA_SUCCESS;
}
#endif /* HENKA_SANDBOX3D_TEST_SEAM */

henka_result sandbox3d_editor_layout_text_row_height(
    size_t line_count,
    float line_height,
    float vertical_padding,
    float minimum_height,
    float* out_height)
{
    double content_height;
    double row_height;

    if (out_height == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_height = 0.0f;
    if (line_count == 0U ||
        !sandbox3d_editor_layout_float_is_valid(line_height) || line_height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(vertical_padding) || vertical_padding < 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(minimum_height) || minimum_height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    content_height = (double)line_count * (double)line_height + (double)vertical_padding;
    if (!isfinite(content_height) || content_height > (double)FLT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    row_height = fmax(content_height, (double)minimum_height);
    if (!isfinite(row_height) || row_height > (double)FLT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    *out_height = (float)row_height;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_clamp_row_height(
    float natural_row_height,
    float available_height,
    float minimum_row_height,
    float* out_visible_height)
{
    if (out_visible_height == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_visible_height = 0.0f;
    if (!sandbox3d_editor_layout_float_is_valid(natural_row_height) ||
        natural_row_height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(available_height) ||
        available_height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(minimum_row_height) ||
        minimum_row_height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (minimum_row_height > available_height)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    *out_visible_height = fminf(natural_row_height, available_height);
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_page_variable_rows(
    const size_t* row_line_counts,
    size_t row_count,
    float available_height,
    float line_height,
    float vertical_padding,
    float minimum_row_height,
    size_t requested_page,
    size_t* out_page_index,
    size_t* out_page_count,
    size_t* out_first_row,
    size_t* out_visible_row_count)
{
    size_t page_count = 0U;
    size_t selected_page;
    size_t row_index;
    size_t visible_first = 0U;
    size_t visible_count = 0U;
    size_t current_page = 0U;
    double used_height = 0.0;
    bool current_page_has_rows = false;

    if (out_page_index == NULL || out_page_count == NULL ||
        out_first_row == NULL || out_visible_row_count == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_page_index = 0U;
    *out_page_count = 0U;
    *out_first_row = 0U;
    *out_visible_row_count = 0U;

    if ((row_count > 0U && row_line_counts == NULL) ||
        !sandbox3d_editor_layout_float_is_valid(available_height) || available_height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(line_height) || line_height <= 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(vertical_padding) || vertical_padding < 0.0f ||
        !sandbox3d_editor_layout_float_is_valid(minimum_row_height) || minimum_row_height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (row_index = 0U; row_index < row_count; ++row_index)
    {
        float row_height;
        float visible_row_height;
        henka_result result = sandbox3d_editor_layout_text_row_height(
            row_line_counts[row_index],
            line_height,
            vertical_padding,
            minimum_row_height,
            &row_height);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        result = sandbox3d_editor_layout_clamp_row_height(
            row_height,
            available_height,
            minimum_row_height,
            &visible_row_height);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (visible_row_height < row_height)
        {
            current_page_has_rows = false;
            used_height = 0.0;
            if (page_count == SIZE_MAX)
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            ++page_count;
            continue;
        }
        if (current_page_has_rows &&
            used_height + (double)row_height > (double)available_height)
        {
            current_page_has_rows = false;
            used_height = 0.0;
        }
        if (!current_page_has_rows)
        {
            if (page_count == SIZE_MAX)
            {
                return HENKA_ERROR_NUMERIC_RANGE;
            }
            ++page_count;
            current_page_has_rows = true;
        }
        used_height += (double)row_height;
    }

    if (page_count == 0U)
    {
        page_count = 1U;
    }
    selected_page = requested_page < page_count ? requested_page : page_count - 1U;

    current_page = 0U;
    used_height = 0.0;
    current_page_has_rows = false;
    for (row_index = 0U; row_index < row_count; ++row_index)
    {
        float row_height;
        float visible_row_height;
        henka_result result = sandbox3d_editor_layout_text_row_height(
            row_line_counts[row_index],
            line_height,
            vertical_padding,
            minimum_row_height,
            &row_height);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        result = sandbox3d_editor_layout_clamp_row_height(
            row_height,
            available_height,
            minimum_row_height,
            &visible_row_height);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (visible_row_height < row_height)
        {
            if (current_page_has_rows)
            {
                if (current_page == selected_page)
                {
                    break;
                }
                ++current_page;
                current_page_has_rows = false;
                used_height = 0.0;
            }
            if (current_page == selected_page)
            {
                visible_first = row_index;
                visible_count = 1U;
            }
            ++current_page;
            current_page_has_rows = false;
            used_height = 0.0;
            continue;
        }
        if (current_page_has_rows &&
            used_height + (double)row_height > (double)available_height)
        {
            if (current_page == selected_page)
            {
                break;
            }
            ++current_page;
            current_page_has_rows = false;
            used_height = 0.0;
        }
        if (!current_page_has_rows)
        {
            if (current_page == selected_page)
            {
                visible_first = row_index;
            }
            current_page_has_rows = true;
        }
        if (current_page == selected_page)
        {
            ++visible_count;
        }
        used_height += (double)row_height;
    }

    *out_page_index = selected_page;
    *out_page_count = page_count;
    *out_first_row = visible_first;
    *out_visible_row_count = visible_count;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_controls_status_page(
    henka_ui_rect panel_bounds,
    sandbox3d_editor_controls_status_layout* out_layout)
{
    sandbox3d_editor_controls_status_layout candidate = {0};
    float half_button_width;
    float left;
    float middle;
    float primary_y;
    float secondary_y;

    if (out_layout == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_layout = candidate;

    if (!isfinite(panel_bounds.x) ||
        !isfinite(panel_bounds.y) ||
        !isfinite(panel_bounds.width) ||
        !isfinite(panel_bounds.height) ||
        panel_bounds.width <= 40.0f ||
        panel_bounds.height <= 188.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    left = panel_bounds.x + 14.0f;
    half_button_width = (panel_bounds.width - 38.0f) * 0.5f;
    middle = left + half_button_width + 10.0f;
    primary_y = panel_bounds.y + panel_bounds.height - 106.0f;
    secondary_y = primary_y + 34.0f;
    candidate.scroll_viewport = (henka_ui_rect){
        left,
        panel_bounds.y + 74.0f,
        panel_bounds.width - 40.0f,
        panel_bounds.height - 188.0f};
    candidate.footer_save_settings = (henka_ui_rect){
        left, primary_y, half_button_width, 28.0f};
    candidate.footer_reset_layout = (henka_ui_rect){
        middle, primary_y, half_button_width, 28.0f};
    candidate.footer_reset_settings = (henka_ui_rect){
        left, secondary_y, half_button_width, 28.0f};
    candidate.footer_open_legend = (henka_ui_rect){
        middle, secondary_y, half_button_width, 28.0f};

    if (!isfinite(candidate.scroll_viewport.x) ||
        !isfinite(candidate.scroll_viewport.y) ||
        !isfinite(candidate.scroll_viewport.width) ||
        !isfinite(candidate.scroll_viewport.height) ||
        !isfinite(candidate.footer_open_legend.y +
            candidate.footer_open_legend.height) ||
        candidate.scroll_viewport.width <= 0.0f ||
        candidate.scroll_viewport.height <= 0.0f ||
        candidate.scroll_viewport.y + candidate.scroll_viewport.height >
            candidate.footer_save_settings.y - 8.0f ||
        candidate.footer_open_legend.y +
            candidate.footer_open_legend.height >
                panel_bounds.y + panel_bounds.height)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_layout = candidate;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_scene_objects_regions(
    henka_ui_rect panel_bounds,
    float measured_line_height,
    float measured_scene_name_label_width,
    float desired_actions_content_height,
    sandbox3d_editor_scene_objects_layout* out_layout)
{
    sandbox3d_editor_scene_objects_layout candidate = {0};
    const float name_left = panel_bounds.x + 14.0f;
    const float name_width = panel_bounds.width - 28.0f;
    const float name_top = panel_bounds.y + 60.0f;
    const float footer_y = panel_bounds.y + panel_bounds.height - 34.0f;
    const float content_bottom = footer_y - 6.0f;
    const float name_label_height = fmaxf(measured_line_height, 16.0f);
    const float name_control_height = fmaxf(measured_line_height + 10.0f, 28.0f);
    const float control_gap = 6.0f;
    const float row_gap = 4.0f;
    const float content_gap = 8.0f;
    const float apply_width = fminf(
        fmaxf(52.0f, measured_line_height * 3.2f),
        name_width * 0.42f);
    const float minimum_field_width = fmaxf(64.0f, measured_line_height * 3.4f);
    const float one_row_minimum_width = measured_scene_name_label_width +
        control_gap + minimum_field_width + control_gap + apply_width;
    const float field_and_apply_minimum_width =
        minimum_field_width + control_gap + apply_width;
    float content_top;
    float available_height;
    float minimum_hierarchy_height;
    float minimum_actions_height;
    float maximum_actions_height;
    float desired_actions_viewport_height;
    float actions_height;
    float hierarchy_height;
    float name_header_height;

    if (out_layout == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_layout = candidate;
    if (!sandbox3d_editor_layout_float_is_valid(panel_bounds.x) ||
        !sandbox3d_editor_layout_float_is_valid(panel_bounds.y) ||
        !sandbox3d_editor_layout_float_is_valid(panel_bounds.width) ||
        !sandbox3d_editor_layout_float_is_valid(panel_bounds.height) ||
        !sandbox3d_editor_layout_float_is_valid(measured_line_height) ||
        !sandbox3d_editor_layout_float_is_valid(measured_scene_name_label_width) ||
        !sandbox3d_editor_layout_float_is_valid(desired_actions_content_height) ||
        panel_bounds.width <= 40.0f || panel_bounds.height <= 0.0f ||
        measured_line_height <= 0.0f || measured_scene_name_label_width <= 0.0f ||
        desired_actions_content_height < 0.0f || name_width < 90.0f)
    {
        return HENKA_ERROR_LIMIT;
    }

    if (name_width >= 196.0f && name_width >= one_row_minimum_width)
    {
        candidate.scene_name_label = (henka_ui_rect){
            name_left,
            name_top,
            measured_scene_name_label_width,
            name_control_height};
        candidate.scene_name_field = (henka_ui_rect){
            name_left + measured_scene_name_label_width + control_gap,
            name_top,
            name_width - measured_scene_name_label_width -
                control_gap - control_gap - apply_width,
            name_control_height};
        candidate.scene_name_apply = (henka_ui_rect){
            name_left + name_width - apply_width,
            name_top,
            apply_width,
            name_control_height};
        name_header_height = name_control_height;
    }
    else if (name_width >= 136.0f &&
        name_width >= field_and_apply_minimum_width)
    {
        candidate.scene_name_label = (henka_ui_rect){
            name_left,
            name_top,
            name_width,
            name_label_height};
        candidate.scene_name_field = (henka_ui_rect){
            name_left,
            name_top + name_label_height + row_gap,
            name_width - control_gap - apply_width,
            name_control_height};
        candidate.scene_name_apply = (henka_ui_rect){
            name_left + name_width - apply_width,
            candidate.scene_name_field.y,
            apply_width,
            name_control_height};
        name_header_height = name_label_height + row_gap + name_control_height;
    }
    else
    {
        candidate.scene_name_label = (henka_ui_rect){
            name_left,
            name_top,
            name_width,
            name_label_height};
        candidate.scene_name_field = (henka_ui_rect){
            name_left,
            name_top + name_label_height + row_gap,
            name_width,
            name_control_height};
        candidate.scene_name_apply = (henka_ui_rect){
            name_left,
            candidate.scene_name_field.y + name_control_height + row_gap,
            name_width,
            name_control_height};
        name_header_height = name_label_height + row_gap +
            name_control_height + row_gap + name_control_height;
    }

    content_top = name_top + name_header_height + content_gap;
    available_height = content_bottom - content_top;
    minimum_hierarchy_height = fmaxf(40.0f, measured_line_height + 8.0f);
    minimum_actions_height = fmaxf(48.0f, measured_line_height + 16.0f);
    maximum_actions_height =
        available_height - 6.0f - minimum_hierarchy_height;
    if (available_height <= 0.0f ||
        maximum_actions_height < minimum_actions_height)
    {
        return HENKA_ERROR_LIMIT;
    }

    /* The previous frame's measured action content determines the viewport.
     * On the first frame, reserve a font-scaled estimate; give the remaining
     * dock height to the hierarchy instead of leaving unused action space. */
    desired_actions_viewport_height = desired_actions_content_height > 0.0f
        ? desired_actions_content_height + 12.0f
        : measured_line_height * 14.0f + 16.0f;
    actions_height = fminf(
        fmaxf(minimum_actions_height, desired_actions_viewport_height),
        maximum_actions_height);
    hierarchy_height = available_height - actions_height - 6.0f;
    candidate.actions_viewport = (henka_ui_rect){
        name_left,
        content_top,
        panel_bounds.width - 40.0f,
        actions_height};
    candidate.hierarchy_viewport = (henka_ui_rect){
        panel_bounds.x + 14.0f,
        content_top + actions_height + 6.0f,
        panel_bounds.width - 28.0f,
        hierarchy_height};
    candidate.page_footer = (henka_ui_rect){
        panel_bounds.x + 14.0f,
        footer_y,
        panel_bounds.width - 28.0f,
        28.0f};

    if (!sandbox3d_editor_layout_float_is_valid(candidate.actions_viewport.x) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.actions_viewport.y) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.actions_viewport.width) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.actions_viewport.height) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.scene_name_label.x) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.scene_name_label.y) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.scene_name_label.width) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.scene_name_label.height) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.scene_name_field.x) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.scene_name_field.y) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.scene_name_field.width) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.scene_name_field.height) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.scene_name_apply.x) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.scene_name_apply.y) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.scene_name_apply.width) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.scene_name_apply.height) ||
        !sandbox3d_editor_layout_float_is_valid(candidate.hierarchy_viewport.y) ||
        candidate.scene_name_label.width <= 0.0f ||
        candidate.scene_name_field.width <= 0.0f ||
        candidate.scene_name_apply.width <= 0.0f ||
        candidate.scene_name_label.x < panel_bounds.x ||
        candidate.scene_name_field.x < panel_bounds.x ||
        candidate.scene_name_apply.x < panel_bounds.x ||
        candidate.scene_name_label.x + candidate.scene_name_label.width >
            panel_bounds.x + panel_bounds.width ||
        candidate.scene_name_field.x + candidate.scene_name_field.width >
            panel_bounds.x + panel_bounds.width ||
        candidate.scene_name_apply.x + candidate.scene_name_apply.width >
            panel_bounds.x + panel_bounds.width ||
        candidate.actions_viewport.width <= 0.0f ||
        candidate.actions_viewport.height < minimum_actions_height ||
        candidate.hierarchy_viewport.height < minimum_hierarchy_height ||
        candidate.page_footer.y + candidate.page_footer.height >
            panel_bounds.y + panel_bounds.height)
    {
        return HENKA_ERROR_LIMIT;
    }

    *out_layout = candidate;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_editor_layout_scene_view_header_for_context(
    const henka_ui_context* ui_context,
    henka_ui_rect bounds,
    sandbox3d_editor_scene_view_header_layout* out_layout)
{
    static const char* const context_labels[] = {"Build", "Game", "World"};
    static const char* const shading_labels[] = {
        "Wire", "Solid", "Material", "Rendered"};
    sandbox3d_editor_scene_view_header_layout candidate = {0};
    size_t row_count = 0U;
    size_t index;
    henka_result result;

    if (out_layout == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_layout = candidate;
    if (ui_context == NULL ||
        !isfinite(bounds.x) || !isfinite(bounds.y) ||
        !isfinite(bounds.width) || !isfinite(bounds.height) ||
        bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (bounds.width >= 800.0f)
    {
        const henka_ui_rect shading_row = {
            bounds.x + bounds.width -
                (bounds.width >= 850.0f ? 360.0f : 292.0f) - 8.0f,
            bounds.y + 4.0f,
            bounds.width >= 850.0f ? 360.0f : 292.0f,
            22.0f};

        candidate.context_selector = (henka_ui_rect){
            bounds.x + 112.0f, bounds.y + 4.0f, 210.0f, 22.0f};
        candidate.tools_button = (henka_ui_rect){
            bounds.x + 326.0f, bounds.y + 4.0f, 62.0f, 22.0f};
        candidate.focus_button = (henka_ui_rect){
            bounds.x + 396.0f, bounds.y + 4.0f, 82.0f, 22.0f};
        candidate.height = 30.0f;
        result = sandbox3d_editor_layout_text_control_row_for_context(
            ui_context,
            shading_row,
            shading_labels,
            sizeof(shading_labels) / sizeof(shading_labels[0]),
            1.0f,
            0.0f,
            HENKA_UI_TAB_TEXT_HORIZONTAL_PADDING,
            3.0f,
            candidate.shading_buttons,
            sizeof(candidate.shading_buttons) /
                sizeof(candidate.shading_buttons[0]),
            &candidate.shading_button_count);
        if (result != HENKA_SUCCESS || candidate.shading_button_count != 4U)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
    }
    else if (bounds.width >= 430.0f)
    {
        const float context_width = 176.0f;
        const henka_ui_rect shading_row = {
            bounds.x + 8.0f,
            bounds.y + 42.0f,
            fminf(292.0f, bounds.width - 16.0f),
            22.0f};

        candidate.context_selector = (henka_ui_rect){
            bounds.x + 104.0f, bounds.y + 4.0f, context_width, 22.0f};
        candidate.tools_button = (henka_ui_rect){
            bounds.x + 284.0f, bounds.y + 4.0f, 58.0f, 22.0f};
        candidate.compact = true;
        candidate.height = 68.0f;
        result = sandbox3d_editor_layout_text_control_row_for_context(
            ui_context,
            shading_row,
            shading_labels,
            sizeof(shading_labels) / sizeof(shading_labels[0]),
            1.0f,
            0.0f,
            HENKA_UI_TAB_TEXT_HORIZONTAL_PADDING,
            3.0f,
            candidate.shading_buttons,
            sizeof(candidate.shading_buttons) /
                sizeof(candidate.shading_buttons[0]),
            &candidate.shading_button_count);
        if (result != HENKA_SUCCESS || candidate.shading_button_count != 4U)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
    }
    else
    {
        const bool tools_fit_title_row = bounds.width >= 190.0f;
        const float content_x = bounds.x + 8.0f;
        const float content_width = bounds.width - 16.0f;
        const float context_y = bounds.y + (tools_fit_title_row ? 30.0f : 55.0f);
        float context_bottom = context_y;
        float shading_bottom = context_y;

        candidate.narrow = true;
        candidate.context_uses_buttons = true;
        candidate.tools_button = tools_fit_title_row
            ? (henka_ui_rect){
                bounds.x + bounds.width - 66.0f,
                bounds.y + 4.0f,
                58.0f,
                22.0f}
            : (henka_ui_rect){content_x, bounds.y + 30.0f, 58.0f, 22.0f};
        result = sandbox3d_editor_layout_text_control_grid_for_context(
            ui_context,
            (henka_ui_rect){
                content_x,
                context_y,
                content_width,
                fmaxf(0.0f, bounds.y + bounds.height - context_y)},
            context_labels,
            sizeof(context_labels) / sizeof(context_labels[0]),
            1.0f,
            0.0f,
            HENKA_UI_TAB_TEXT_HORIZONTAL_PADDING,
            3.0f,
            22.0f,
            3.0f,
            candidate.context_buttons,
            sizeof(candidate.context_buttons) /
                sizeof(candidate.context_buttons[0]),
            &row_count);
        if (result != HENKA_SUCCESS || row_count == 0U)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        candidate.context_button_count =
            sizeof(candidate.context_buttons) /
            sizeof(candidate.context_buttons[0]);
        for (index = 0U; index < candidate.context_button_count; ++index)
        {
            context_bottom = fmaxf(
                context_bottom,
                candidate.context_buttons[index].y +
                    candidate.context_buttons[index].height);
        }

        result = sandbox3d_editor_layout_text_control_grid_for_context(
            ui_context,
            (henka_ui_rect){
                content_x,
                context_bottom + 3.0f,
                content_width,
                fmaxf(0.0f, bounds.y + bounds.height - context_bottom - 3.0f)},
            shading_labels,
            sizeof(shading_labels) / sizeof(shading_labels[0]),
            1.0f,
            0.0f,
            HENKA_UI_TAB_TEXT_HORIZONTAL_PADDING,
            3.0f,
            22.0f,
            3.0f,
            candidate.shading_buttons,
            sizeof(candidate.shading_buttons) /
                sizeof(candidate.shading_buttons[0]),
            &row_count);
        if (result != HENKA_SUCCESS || row_count == 0U)
        {
            return HENKA_ERROR_NUMERIC_RANGE;
        }
        candidate.shading_button_count =
            sizeof(candidate.shading_buttons) /
            sizeof(candidate.shading_buttons[0]);
        for (index = 0U; index < candidate.shading_button_count; ++index)
        {
            shading_bottom = fmaxf(
                shading_bottom,
                candidate.shading_buttons[index].y +
                    candidate.shading_buttons[index].height);
        }
        candidate.height = shading_bottom - bounds.y + 5.0f;
    }

    if (!isfinite(candidate.height) || candidate.height <= 0.0f ||
        candidate.height + 9.0f > bounds.height)
    {
        return HENKA_ERROR_LIMIT;
    }
    *out_layout = candidate;
    return HENKA_SUCCESS;
}
