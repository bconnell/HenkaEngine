#include "editor_layout.h"

#include <henka/workspace.h>

#include <float.h>
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

henka_ui_rect sandbox3d_editor_layout_modeling_toolbar_bounds(
    henka_ui_rect scene_frame,
    bool authoring_available,
    bool options_expanded)
{
    sandbox3d_modeling_toolbar_layout toolbar_layout;

    if (sandbox3d_editor_layout_modeling_toolbar_compute(
            scene_frame,
            authoring_available,
            options_expanded,
            &toolbar_layout) != HENKA_SUCCESS)
    {
        return (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    }
    return toolbar_layout.bounds;
}

henka_result sandbox3d_editor_layout_modeling_toolbar_compute(
    henka_ui_rect scene_frame,
    bool authoring_available,
    bool options_expanded,
    sandbox3d_modeling_toolbar_layout* out_layout)
{
    sandbox3d_modeling_toolbar_layout layout;
    const bool compact_toolbar = scene_frame.width < 760.0f;
    float y;
    float width;
    float toolbar_height;
    size_t index;

    if (out_layout == NULL ||
        !sandbox3d_editor_layout_float_is_valid(scene_frame.x) ||
        !sandbox3d_editor_layout_float_is_valid(scene_frame.y) ||
        !sandbox3d_editor_layout_float_is_valid(scene_frame.width) ||
        !sandbox3d_editor_layout_float_is_valid(scene_frame.height) ||
        scene_frame.width < 500.0f || scene_frame.height < 150.0f)
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
    width = scene_frame.width - 20.0f;
    if (!authoring_available && compact_toolbar)
    {
        toolbar_height = 52.0f;
    }
    else if (compact_toolbar)
    {
        toolbar_height = layout.options_expanded ? 166.0f : 104.0f;
    }
    else
    {
        toolbar_height = 136.0f;
    }
    layout.bounds = (henka_ui_rect){scene_frame.x + 10.0f, y, width, toolbar_height};

    if (!authoring_available && compact_toolbar)
    {
        *out_layout = layout;
        return HENKA_SUCCESS;
    }

    layout.selection_mode = (henka_ui_rect){
        layout.bounds.x + 74.0f,
        y + 20.0f,
        196.0f,
        22.0f};
    if (compact_toolbar)
    {
        layout.options_toggle = (henka_ui_rect){
            layout.bounds.x + width - 112.0f,
            y + 4.0f,
            104.0f,
            32.0f};
        if (layout.options_expanded)
        {
            layout.orientation = (henka_ui_rect){
                layout.bounds.x + 82.0f, y + 50.0f, 196.0f, 22.0f};
            layout.pivot = (henka_ui_rect){
                layout.bounds.x + 74.0f, y + 80.0f, 196.0f, 22.0f};
        }
        layout.state_summary = layout.options_expanded
            ? (henka_ui_rect){layout.bounds.x + 8.0f, y + 142.0f, width - 16.0f, 18.0f}
            : (henka_ui_rect){layout.bounds.x + 8.0f, y + 84.0f, width - 16.0f, 18.0f};
    }
    else
    {
        layout.orientation = (henka_ui_rect){
            layout.bounds.x + 352.0f, y + 20.0f, 142.0f, 22.0f};
        layout.pivot = (henka_ui_rect){
            layout.bounds.x + 74.0f, y + 50.0f, 196.0f, 22.0f};
        layout.state_summary = (henka_ui_rect){
            layout.bounds.x + 8.0f, y + 112.0f, width - 16.0f, 18.0f};
    }

    if (compact_toolbar && !layout.options_expanded)
    {
        const float gap = 4.0f;
        const float button_width = (width - gap * 3.0f) / 4.0f;
        for (index = 0U; index < 4U; ++index)
        {
            layout.tool_buttons[index] = (henka_ui_rect){
                layout.bounds.x + (button_width + gap) * (float)index,
                y + 52.0f,
                button_width,
                28.0f};
        }
    }
    else
    {
        const float gap = 4.0f;
        const float button_width = (width - gap * 5.0f) / 6.0f;
        const float tool_y = y + (compact_toolbar ? 110.0f : 80.0f);
        for (index = 0U; index < 6U; ++index)
        {
            layout.tool_buttons[index] = (henka_ui_rect){
                layout.bounds.x + (button_width + gap) * (float)index,
                tool_y,
                button_width,
                28.0f};
        }
    }

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
    const sandbox3d_editor_frame_layout* layout,
    bool authoring_available,
    bool options_expanded)
{
    henka_viewport navigation_viewport;
    const henka_ui_rect toolbar = layout == NULL
        ? (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f}
        : sandbox3d_editor_layout_modeling_toolbar_bounds(
            layout->scene_frame,
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
    bool distribute_extra_width,
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

        if (labels[item_index] == NULL ||
            (ui_context != NULL
                 ? henka_ui_measure_text_for_context(
                       ui_context,
                       labels[item_index],
                       scale,
                       &measured_width,
                       &measured_height)
                 : henka_ui_measure_text(
                       labels[item_index],
                       scale,
                       &measured_width,
                       &measured_height)) != HENKA_SUCCESS)
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

    extra_width = distribute_extra_width
        ? (available_width - required_width) / (double)item_count
        : 0.0;
    if (!isfinite(extra_width) || extra_width < 0.0 ||
        extra_width > (double)FLT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    item_x = (double)bounds.x;
    for (item_index = 0U; item_index < item_count; ++item_index)
    {
        const double item_width = distribute_extra_width && item_index + 1U == item_count
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
    return sandbox3d_editor_layout_text_control_row_impl(
        NULL,
        true,
        bounds,
        labels,
        item_count,
        scale,
        0.0f,
        horizontal_padding,
        gap,
        out_items,
        item_capacity,
        out_item_count);
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
        false,
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

static bool sandbox3d_editor_layout_is_wrap_space(char value)
{
    return value == ' ' || value == '\t';
}

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

henka_result sandbox3d_editor_layout_format_hidden_row_label(
    const char* object_name,
    size_t maximum_columns,
    char* out_text,
    size_t out_capacity,
    bool* out_name_truncated)
{
    static const char hidden_suffix[] = " [Hidden]";
    const size_t suffix_length = sizeof(hidden_suffix) - 1U;
    size_t name_length;
    size_t name_columns;
    size_t prefix_length;
    size_t suffix_name_length;
    size_t output_length;

    if (object_name == NULL || out_text == NULL || out_name_truncated == NULL ||
        out_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    name_length = strlen(object_name);
    if (name_length == 0U || strchr(object_name, '\n') != NULL ||
        strchr(object_name, '\r') != NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (maximum_columns <= suffix_length)
    {
        return HENKA_ERROR_LIMIT;
    }

    name_columns = maximum_columns - suffix_length;
    if (name_length <= name_columns)
    {
        output_length = name_length + suffix_length;
        if (output_length >= out_capacity)
        {
            return HENKA_ERROR_LIMIT;
        }
        memmove(out_text, object_name, name_length);
        memcpy(out_text + name_length, hidden_suffix, sizeof(hidden_suffix));
        *out_name_truncated = false;
        return HENKA_SUCCESS;
    }

    /* Keep the marker intact and use the remaining row budget for a bounded
     * middle ellipsis. A four-column name budget yields "A..."; larger
     * budgets retain both the beginning and end of the canonical name. */
    if (name_columns < 4U)
    {
        return HENKA_ERROR_LIMIT;
    }
    output_length = name_columns + suffix_length;
    if (output_length >= out_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }
    prefix_length = (name_columns - 3U + 1U) / 2U;
    suffix_name_length = name_columns - 3U - prefix_length;

    memmove(out_text, object_name, prefix_length);
    memcpy(out_text + prefix_length, "...", 3U);
    if (suffix_name_length > 0U)
    {
        memcpy(
            out_text + prefix_length + 3U,
            object_name + name_length - suffix_name_length,
            suffix_name_length);
    }
    memcpy(out_text + name_columns, hidden_suffix, sizeof(hidden_suffix));
    *out_name_truncated = true;
    return HENKA_SUCCESS;
}

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
