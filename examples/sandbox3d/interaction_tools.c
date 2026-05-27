#include "interaction_tools.h"

#include <math.h>
#include <string.h>

const char* sandbox3d_viewport_tool_mode_to_string(sandbox3d_viewport_tool_mode tool_mode)
{
    switch (tool_mode)
    {
        case SANDBOX3D_VIEWPORT_TOOL_ORBIT:
            return "Orbit";
        case SANDBOX3D_VIEWPORT_TOOL_PAN:
            return "Pan";
        case SANDBOX3D_VIEWPORT_TOOL_MOVE:
            return "Move";
        case SANDBOX3D_VIEWPORT_TOOL_ROTATE:
            return "Rotate";
        case SANDBOX3D_VIEWPORT_TOOL_SCALE:
            return "Scale";
        case SANDBOX3D_VIEWPORT_TOOL_SELECT:
        default:
            return "Select";
    }
}

const char* sandbox3d_interaction_reject_reason_to_string(sandbox3d_interaction_reject_reason reason)
{
    switch (reason)
    {
        case SANDBOX3D_INTERACTION_REJECT_MOUSE_CAPTURE_ACTIVE:
            return "Mouse capture active";
        case SANDBOX3D_INTERACTION_REJECT_UI_OWNS_MOUSE:
            return "UI owns mouse";
        case SANDBOX3D_INTERACTION_REJECT_CURSOR_OUTSIDE_VIEWPORT:
            return "Cursor outside viewport";
        case SANDBOX3D_INTERACTION_REJECT_NO_SELECTED_OBJECT:
            return "No selected object";
        case SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_INVALID:
            return "Selected object invalid";
        case SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_HIDDEN:
            return "Selected object hidden";
        case SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_NOT_SELECTABLE:
            return "Selected object not selectable";
        case SANDBOX3D_INTERACTION_REJECT_SELECTED_BOUNDS_INVALID:
            return "Selected bounds invalid";
        case SANDBOX3D_INTERACTION_REJECT_GIZMO_MODE_INACTIVE:
            return "Gizmo mode inactive";
        case SANDBOX3D_INTERACTION_REJECT_GIZMO_MODEL_INVALID:
            return "Gizmo model invalid";
        case SANDBOX3D_INTERACTION_REJECT_OVERLAY_HAS_NO_PRIMITIVES:
            return "Overlay has no primitives";
        case SANDBOX3D_INTERACTION_REJECT_NO_HANDLE_UNDER_CURSOR:
            return "No handle under cursor";
        case SANDBOX3D_INTERACTION_REJECT_CURSOR_IN_GIZMO_DEAD_ZONE:
            return "Cursor in gizmo dead zone";
        case SANDBOX3D_INTERACTION_REJECT_VIEWPORT_CHANGED_DURING_DRAG:
            return "Viewport changed during drag";
        case SANDBOX3D_INTERACTION_REJECT_DRAG_TARGET_INVALID:
            return "Drag target invalid";
        case SANDBOX3D_INTERACTION_REJECT_ACTION_COMMAND_FAILED:
            return "Action command failed";
        case SANDBOX3D_INTERACTION_REJECT_NAVIGATION_MODE_INACTIVE:
            return "Navigation mode inactive";
        case SANDBOX3D_INTERACTION_REJECT_PANEL_CLICK_REJECTED:
            return "Panel click rejected";
        case SANDBOX3D_INTERACTION_REJECT_INPUT_NOT_SUPPORTED_ON_CURRENT_MOUSE_BUTTON:
            return "Input not supported on current mouse button";
        case SANDBOX3D_INTERACTION_REJECT_NONE:
        default:
            return "None";
    }
}

henka_gizmo_mode sandbox3d_viewport_tool_mode_to_gizmo_mode(sandbox3d_viewport_tool_mode tool_mode)
{
    switch (tool_mode)
    {
        case SANDBOX3D_VIEWPORT_TOOL_MOVE:
            return HENKA_GIZMO_MODE_MOVE;
        case SANDBOX3D_VIEWPORT_TOOL_ROTATE:
            return HENKA_GIZMO_MODE_ROTATE;
        case SANDBOX3D_VIEWPORT_TOOL_SCALE:
            return HENKA_GIZMO_MODE_SCALE;
        case SANDBOX3D_VIEWPORT_TOOL_ORBIT:
        case SANDBOX3D_VIEWPORT_TOOL_PAN:
        case SANDBOX3D_VIEWPORT_TOOL_SELECT:
        default:
            return HENKA_GIZMO_MODE_SELECT;
    }
}

bool sandbox3d_viewport_tool_mode_uses_gizmo(sandbox3d_viewport_tool_mode tool_mode)
{
    return tool_mode == SANDBOX3D_VIEWPORT_TOOL_MOVE ||
        tool_mode == SANDBOX3D_VIEWPORT_TOOL_ROTATE ||
        tool_mode == SANDBOX3D_VIEWPORT_TOOL_SCALE;
}

bool sandbox3d_viewport_tool_mode_is_navigation(sandbox3d_viewport_tool_mode tool_mode)
{
    return tool_mode == SANDBOX3D_VIEWPORT_TOOL_ORBIT ||
        tool_mode == SANDBOX3D_VIEWPORT_TOOL_PAN;
}

bool sandbox3d_point_is_owned_by_panels(
    henka_vec2 framebuffer_point,
    const henka_ui_rect* panel_bounds,
    size_t panel_count)
{
    size_t index;

    if (panel_bounds == NULL)
    {
        return false;
    }

    for (index = 0U; index < panel_count; ++index)
    {
        if (panel_bounds[index].width > 0.0f &&
            panel_bounds[index].height > 0.0f &&
            henka_ui_rect_contains(panel_bounds[index], framebuffer_point))
        {
            return true;
        }
    }

    return false;
}

static sandbox3d_interaction_reject_reason sandbox3d_evaluate_common_reject_reason(
    const sandbox3d_interaction_gate* gate)
{
    if (gate == NULL)
    {
        return SANDBOX3D_INTERACTION_REJECT_PANEL_CLICK_REJECTED;
    }

    if (gate->mouse_capture_active)
    {
        return SANDBOX3D_INTERACTION_REJECT_MOUSE_CAPTURE_ACTIVE;
    }
    if (gate->ui_wants_mouse)
    {
        return SANDBOX3D_INTERACTION_REJECT_UI_OWNS_MOUSE;
    }
    if (gate->panel_click_rejected)
    {
        return SANDBOX3D_INTERACTION_REJECT_PANEL_CLICK_REJECTED;
    }
    if (!gate->supported_mouse_button)
    {
        return SANDBOX3D_INTERACTION_REJECT_INPUT_NOT_SUPPORTED_ON_CURRENT_MOUSE_BUTTON;
    }
    if (!gate->cursor_in_viewport)
    {
        return SANDBOX3D_INTERACTION_REJECT_CURSOR_OUTSIDE_VIEWPORT;
    }

    return SANDBOX3D_INTERACTION_REJECT_NONE;
}

sandbox3d_interaction_reject_reason sandbox3d_evaluate_navigation_reject_reason(
    sandbox3d_viewport_tool_mode tool_mode,
    const sandbox3d_interaction_gate* gate)
{
    sandbox3d_interaction_reject_reason common_reason;

    common_reason = sandbox3d_evaluate_common_reject_reason(gate);
    if (common_reason != SANDBOX3D_INTERACTION_REJECT_NONE)
    {
        return common_reason;
    }

    if (!sandbox3d_viewport_tool_mode_is_navigation(tool_mode))
    {
        return SANDBOX3D_INTERACTION_REJECT_NAVIGATION_MODE_INACTIVE;
    }
    return SANDBOX3D_INTERACTION_REJECT_NONE;
}

sandbox3d_interaction_reject_reason sandbox3d_evaluate_gizmo_reject_reason(
    sandbox3d_viewport_tool_mode tool_mode,
    const sandbox3d_interaction_gate* gate)
{
    sandbox3d_interaction_reject_reason common_reason;

    common_reason = sandbox3d_evaluate_common_reject_reason(gate);
    if (common_reason != SANDBOX3D_INTERACTION_REJECT_NONE)
    {
        return common_reason;
    }

    if (!sandbox3d_viewport_tool_mode_uses_gizmo(tool_mode))
    {
        return SANDBOX3D_INTERACTION_REJECT_GIZMO_MODE_INACTIVE;
    }
    if (!gate->selected_object_present)
    {
        return SANDBOX3D_INTERACTION_REJECT_NO_SELECTED_OBJECT;
    }
    if (!gate->selected_object_valid)
    {
        return SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_INVALID;
    }
    if (!gate->selected_object_visible)
    {
        return SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_HIDDEN;
    }
    if (!gate->selected_object_selectable)
    {
        return SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_NOT_SELECTABLE;
    }
    if (!gate->gizmo_mode_active)
    {
        return SANDBOX3D_INTERACTION_REJECT_GIZMO_MODE_INACTIVE;
    }
    if (!gate->gizmo_model_valid)
    {
        return SANDBOX3D_INTERACTION_REJECT_GIZMO_MODEL_INVALID;
    }
    if (!gate->overlay_has_primitives)
    {
        return SANDBOX3D_INTERACTION_REJECT_OVERLAY_HAS_NO_PRIMITIVES;
    }
    if (gate->cursor_in_gizmo_dead_zone)
    {
        return SANDBOX3D_INTERACTION_REJECT_CURSOR_IN_GIZMO_DEAD_ZONE;
    }
    if (!gate->handle_under_cursor)
    {
        return SANDBOX3D_INTERACTION_REJECT_NO_HANDLE_UNDER_CURSOR;
    }

    return SANDBOX3D_INTERACTION_REJECT_NONE;
}

sandbox3d_interaction_reject_reason sandbox3d_evaluate_select_reject_reason(
    const sandbox3d_interaction_gate* gate)
{
    return sandbox3d_evaluate_common_reject_reason(gate);
}

henka_vec3 sandbox3d_make_move_delta(henka_gizmo_axis axis, float magnitude)
{
    switch (axis)
    {
        case HENKA_GIZMO_AXIS_X:
            return (henka_vec3){magnitude, 0.0f, 0.0f};
        case HENKA_GIZMO_AXIS_Y:
            return (henka_vec3){0.0f, magnitude, 0.0f};
        case HENKA_GIZMO_AXIS_Z:
            return (henka_vec3){0.0f, 0.0f, magnitude};
        case HENKA_GIZMO_AXIS_UNIFORM:
        case HENKA_GIZMO_AXIS_NONE:
        default:
            return (henka_vec3){0.0f, 0.0f, 0.0f};
    }
}

henka_quat sandbox3d_make_rotation_delta(henka_gizmo_axis axis, float radians)
{
    henka_vec3 axis_direction;

    if (henka_gizmo_get_axis_direction(axis, &axis_direction) != HENKA_SUCCESS)
    {
        return henka_quat_identity();
    }

    return henka_quat_from_axis_angle(axis_direction, radians);
}

henka_vec3 sandbox3d_make_uniform_scale_multiplier(float delta_scale)
{
    float scale_multiplier;

    scale_multiplier = 1.0f + delta_scale;
    if (scale_multiplier < 0.1f)
    {
        scale_multiplier = 0.1f;
    }

    return (henka_vec3){scale_multiplier, scale_multiplier, scale_multiplier};
}

bool sandbox3d_build_selection_highlight_model(
    henka_bounds bounds,
    sandbox3d_selection_highlight_model* out_model)
{
    static const int edges[12][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0},
        {4, 5}, {5, 7}, {7, 6}, {6, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    henka_vec3 points[8];
    int edge;
    int point;

    if (out_model == NULL ||
        !isfinite(bounds.center.x) ||
        !isfinite(bounds.center.y) ||
        !isfinite(bounds.center.z) ||
        !isfinite(bounds.extents.x) ||
        !isfinite(bounds.extents.y) ||
        !isfinite(bounds.extents.z) ||
        bounds.extents.x <= 0.0f ||
        bounds.extents.y <= 0.0f ||
        bounds.extents.z <= 0.0f)
    {
        return false;
    }

    memset(out_model, 0, sizeof(*out_model));
    for (point = 0; point < 8; ++point)
    {
        points[point] = henka_vec3_add(
            bounds.center,
            (henka_vec3){
                (point & 1) ? bounds.extents.x : -bounds.extents.x,
                (point & 2) ? bounds.extents.y : -bounds.extents.y,
                (point & 4) ? bounds.extents.z : -bounds.extents.z});
    }

    for (edge = 0; edge < 12; ++edge)
    {
        out_model->edge_starts[edge] = points[edges[edge][0]];
        out_model->edge_ends[edge] = points[edges[edge][1]];
    }
    out_model->edge_count = 12U;
    out_model->valid = true;
    return true;
}

bool sandbox3d_build_ground_selection_highlight_model(
    henka_vec3 center,
    float half_extent,
    float y_offset,
    sandbox3d_selection_highlight_model* out_model)
{
    const float y = center.y + y_offset;
    henka_vec3 corners[4];
    int edge;

    if (out_model == NULL ||
        !isfinite(center.x) ||
        !isfinite(center.y) ||
        !isfinite(center.z) ||
        !isfinite(half_extent) ||
        !isfinite(y_offset) ||
        half_extent <= 0.0f ||
        half_extent > 1000.0f)
    {
        return false;
    }

    memset(out_model, 0, sizeof(*out_model));
    corners[0] = (henka_vec3){center.x - half_extent, y, center.z - half_extent};
    corners[1] = (henka_vec3){center.x + half_extent, y, center.z - half_extent};
    corners[2] = (henka_vec3){center.x + half_extent, y, center.z + half_extent};
    corners[3] = (henka_vec3){center.x - half_extent, y, center.z + half_extent};
    for (edge = 0; edge < 4; ++edge)
    {
        out_model->edge_starts[edge] = corners[edge];
        out_model->edge_ends[edge] = corners[(edge + 1) % 4];
    }

    out_model->edge_count = 4U;
    out_model->valid = true;
    return true;
}

static bool sandbox3d_clip_line_parameter(float p, float q, float* t0, float* t1)
{
    float r;

    if (p == 0.0f)
    {
        return q >= 0.0f;
    }

    r = q / p;
    if (p < 0.0f)
    {
        if (r > *t1)
        {
            return false;
        }
        if (r > *t0)
        {
            *t0 = r;
        }
    }
    else
    {
        if (r < *t0)
        {
            return false;
        }
        if (r < *t1)
        {
            *t1 = r;
        }
    }

    return true;
}

bool sandbox3d_clip_line_to_rect(
    henka_vec2* start,
    henka_vec2* end,
    henka_ui_rect rect)
{
    const float max_x = rect.x + rect.width;
    const float max_y = rect.y + rect.height;
    const float dx = end != NULL && start != NULL ? end->x - start->x : 0.0f;
    const float dy = end != NULL && start != NULL ? end->y - start->y : 0.0f;
    float t0 = 0.0f;
    float t1 = 1.0f;
    henka_vec2 original_start;

    if (start == NULL ||
        end == NULL ||
        !isfinite(start->x) ||
        !isfinite(start->y) ||
        !isfinite(end->x) ||
        !isfinite(end->y) ||
        !isfinite(rect.x) ||
        !isfinite(rect.y) ||
        !isfinite(rect.width) ||
        !isfinite(rect.height) ||
        rect.width <= 0.0f ||
        rect.height <= 0.0f)
    {
        return false;
    }

    if (!sandbox3d_clip_line_parameter(-dx, start->x - rect.x, &t0, &t1) ||
        !sandbox3d_clip_line_parameter(dx, max_x - start->x, &t0, &t1) ||
        !sandbox3d_clip_line_parameter(-dy, start->y - rect.y, &t0, &t1) ||
        !sandbox3d_clip_line_parameter(dy, max_y - start->y, &t0, &t1))
    {
        return false;
    }

    original_start = *start;
    if (t1 < 1.0f)
    {
        end->x = original_start.x + t1 * dx;
        end->y = original_start.y + t1 * dy;
    }
    if (t0 > 0.0f)
    {
        start->x = original_start.x + t0 * dx;
        start->y = original_start.y + t0 * dy;
    }

    return true;
}
