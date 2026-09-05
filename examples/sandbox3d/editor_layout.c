#include "editor_layout.h"

#include <henka/workspace.h>

#include <float.h>
#include <math.h>
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
    workspace_desc.left_dock_width = workspace->left_dock_width;
    workspace_desc.right_dock_width = workspace->right_dock_width;

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

        if (labels[item_index] == NULL ||
            henka_ui_measure_text(
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
