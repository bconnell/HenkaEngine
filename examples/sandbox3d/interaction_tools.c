#include "interaction_tools.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum
{
    SANDBOX3D_SILHOUETTE_GRID_DIMENSION = 64,
    SANDBOX3D_SILHOUETTE_GRID_CELL_COUNT =
        SANDBOX3D_SILHOUETTE_GRID_DIMENSION * SANDBOX3D_SILHOUETTE_GRID_DIMENSION,
    SANDBOX3D_SILHOUETTE_GRID_CELL_CAPACITY = 96,
    SANDBOX3D_SILHOUETTE_MAX_TRIANGLES = 16384
};

static uint16_t sandbox3d_silhouette_grid_indices[
    SANDBOX3D_SILHOUETTE_GRID_CELL_COUNT * SANDBOX3D_SILHOUETTE_GRID_CELL_CAPACITY];
static uint16_t sandbox3d_silhouette_grid_counts[SANDBOX3D_SILHOUETTE_GRID_CELL_COUNT];
static uint32_t sandbox3d_silhouette_candidate_marks[SANDBOX3D_SILHOUETTE_MAX_TRIANGLES];
static uint16_t sandbox3d_silhouette_candidates[SANDBOX3D_SILHOUETTE_MAX_TRIANGLES];
static uint32_t sandbox3d_silhouette_candidate_stamp;

static int sandbox3d_silhouette_grid_coordinate(float value, float minimum, float maximum)
{
    float normalized;

    if (!isfinite(value) || !isfinite(minimum) || !isfinite(maximum) || maximum <= minimum)
    {
        return 0;
    }
    normalized = (value - minimum) / (maximum - minimum);
    if (normalized <= 0.0f)
    {
        return 0;
    }
    if (normalized >= 1.0f)
    {
        return SANDBOX3D_SILHOUETTE_GRID_DIMENSION - 1;
    }
    return (int)(normalized * (float)SANDBOX3D_SILHOUETTE_GRID_DIMENSION);
}

static bool sandbox3d_build_silhouette_visibility_grid(
    const sandbox3d_projected_triangle* triangles,
    size_t triangle_count,
    float minimum_x,
    float maximum_x,
    float minimum_y,
    float maximum_y)
{
    size_t triangle_index;

    if (triangles == NULL || triangle_count == 0U || triangle_count > SANDBOX3D_SILHOUETTE_MAX_TRIANGLES ||
        !isfinite(minimum_x) || !isfinite(maximum_x) || !isfinite(minimum_y) || !isfinite(maximum_y) ||
        maximum_x <= minimum_x || maximum_y <= minimum_y)
    {
        return false;
    }
    memset(sandbox3d_silhouette_grid_counts, 0, sizeof(sandbox3d_silhouette_grid_counts));
    for (triangle_index = 0U; triangle_index < triangle_count; ++triangle_index)
    {
        const sandbox3d_projected_triangle* triangle = &triangles[triangle_index];
        const float triangle_min_x = fminf(triangle->points[0].x, fminf(triangle->points[1].x, triangle->points[2].x));
        const float triangle_max_x = fmaxf(triangle->points[0].x, fmaxf(triangle->points[1].x, triangle->points[2].x));
        const float triangle_min_y = fminf(triangle->points[0].y, fminf(triangle->points[1].y, triangle->points[2].y));
        const float triangle_max_y = fmaxf(triangle->points[0].y, fmaxf(triangle->points[1].y, triangle->points[2].y));
        const int first_x = sandbox3d_silhouette_grid_coordinate(triangle_min_x, minimum_x, maximum_x);
        const int last_x = sandbox3d_silhouette_grid_coordinate(triangle_max_x, minimum_x, maximum_x);
        const int first_y = sandbox3d_silhouette_grid_coordinate(triangle_min_y, minimum_y, maximum_y);
        const int last_y = sandbox3d_silhouette_grid_coordinate(triangle_max_y, minimum_y, maximum_y);
        int grid_y;
        int grid_x;

        if (!isfinite(triangle_min_x) || !isfinite(triangle_max_x) ||
            !isfinite(triangle_min_y) || !isfinite(triangle_max_y))
        {
            continue;
        }
        for (grid_y = first_y; grid_y <= last_y; ++grid_y)
        {
            for (grid_x = first_x; grid_x <= last_x; ++grid_x)
            {
                const size_t cell = (size_t)grid_y * SANDBOX3D_SILHOUETTE_GRID_DIMENSION + (size_t)grid_x;
                const uint16_t count = sandbox3d_silhouette_grid_counts[cell];
                if (count >= SANDBOX3D_SILHOUETTE_GRID_CELL_CAPACITY)
                {
                    return false;
                }
                sandbox3d_silhouette_grid_indices[cell * SANDBOX3D_SILHOUETTE_GRID_CELL_CAPACITY + count] =
                    (uint16_t)triangle_index;
                sandbox3d_silhouette_grid_counts[cell] = (uint16_t)(count + 1U);
            }
        }
    }
    return true;
}

static size_t sandbox3d_collect_silhouette_visibility_candidates(
    henka_vec2 start,
    henka_vec2 end,
    float minimum_x,
    float maximum_x,
    float minimum_y,
    float maximum_y,
    size_t triangle_count)
{
    const float edge_min_x = fminf(start.x, end.x) - 0.001f;
    const float edge_max_x = fmaxf(start.x, end.x) + 0.001f;
    const float edge_min_y = fminf(start.y, end.y) - 0.001f;
    const float edge_max_y = fmaxf(start.y, end.y) + 0.001f;
    const int first_x = sandbox3d_silhouette_grid_coordinate(edge_min_x, minimum_x, maximum_x);
    const int last_x = sandbox3d_silhouette_grid_coordinate(edge_max_x, minimum_x, maximum_x);
    const int first_y = sandbox3d_silhouette_grid_coordinate(edge_min_y, minimum_y, maximum_y);
    const int last_y = sandbox3d_silhouette_grid_coordinate(edge_max_y, minimum_y, maximum_y);
    size_t candidate_count = 0U;
    int grid_y;
    int grid_x;

    ++sandbox3d_silhouette_candidate_stamp;
    if (sandbox3d_silhouette_candidate_stamp == 0U)
    {
        memset(sandbox3d_silhouette_candidate_marks, 0, sizeof(sandbox3d_silhouette_candidate_marks));
        sandbox3d_silhouette_candidate_stamp = 1U;
    }
    for (grid_y = first_y; grid_y <= last_y; ++grid_y)
    {
        for (grid_x = first_x; grid_x <= last_x; ++grid_x)
        {
            const size_t cell = (size_t)grid_y * SANDBOX3D_SILHOUETTE_GRID_DIMENSION + (size_t)grid_x;
            uint16_t cell_index;
            for (cell_index = 0U; cell_index < sandbox3d_silhouette_grid_counts[cell]; ++cell_index)
            {
                const uint16_t triangle_index = sandbox3d_silhouette_grid_indices[
                    cell * SANDBOX3D_SILHOUETTE_GRID_CELL_CAPACITY + cell_index];
                if ((size_t)triangle_index >= triangle_count ||
                    sandbox3d_silhouette_candidate_marks[triangle_index] == sandbox3d_silhouette_candidate_stamp)
                {
                    continue;
                }
                sandbox3d_silhouette_candidate_marks[triangle_index] = sandbox3d_silhouette_candidate_stamp;
                sandbox3d_silhouette_candidates[candidate_count++] = triangle_index;
            }
        }
    }
    return candidate_count;
}

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
        case SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_TRANSFORM_LOCKED:
            return "Selected object transform locked";
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
    if (gate->selected_object_transform_locked)
    {
        return SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_TRANSFORM_LOCKED;
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

bool sandbox3d_should_start_empty_viewport_pan(
    sandbox3d_viewport_tool_mode tool_mode,
    const sandbox3d_interaction_gate* gate,
    bool empty_viewport_drag_candidate,
    bool gizmo_drag_active,
    bool alt_orbit_active,
    float drag_distance_pixels)
{
    return tool_mode == SANDBOX3D_VIEWPORT_TOOL_SELECT &&
        sandbox3d_evaluate_select_reject_reason(gate) ==
            SANDBOX3D_INTERACTION_REJECT_NONE &&
        empty_viewport_drag_candidate &&
        !gizmo_drag_active &&
        !alt_orbit_active &&
        isfinite(drag_distance_pixels) &&
        drag_distance_pixels >= SANDBOX3D_EMPTY_VIEWPORT_PAN_DRAG_THRESHOLD_PIXELS;
}

bool sandbox3d_should_start_right_mouse_pan(
    const sandbox3d_interaction_gate* gate,
    bool right_mouse_drag_candidate,
    bool competing_navigation_active,
    bool gizmo_drag_active,
    float drag_distance_pixels)
{
    return sandbox3d_evaluate_select_reject_reason(gate) ==
            SANDBOX3D_INTERACTION_REJECT_NONE &&
        right_mouse_drag_candidate &&
        !competing_navigation_active &&
        !gizmo_drag_active &&
        isfinite(drag_distance_pixels) &&
        drag_distance_pixels >= SANDBOX3D_RIGHT_MOUSE_PAN_DRAG_THRESHOLD_PIXELS;
}

bool sandbox3d_should_prefer_terrain_hit(
    bool terrain_hit,
    float terrain_distance,
    bool object_hit,
    float object_distance)
{
    if (!terrain_hit || !isfinite(terrain_distance) || terrain_distance < 0.0f)
    {
        return false;
    }

    if (!object_hit || !isfinite(object_distance) || object_distance < 0.0f)
    {
        return true;
    }

    return terrain_distance < object_distance;
}

henka_result sandbox3d_build_authoring_cage(
    const henka_authoring_mesh* mesh,
    sandbox3d_authoring_cage_edge* out_edges,
    size_t edge_capacity,
    size_t* out_edge_count)
{
    henka_authoring_mesh_counts counts;
    size_t edge_id;
    size_t output_count;

    if (out_edge_count != NULL)
    {
        *out_edge_count = 0U;
    }

    if (mesh == NULL ||
        out_edges == NULL ||
        out_edge_count == NULL ||
        edge_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    counts = henka_authoring_mesh_get_counts(mesh);

    if (counts.edges > edge_capacity)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    output_count = 0U;

    for (edge_id = 1U;
         edge_id <= HENKA_AUTHORING_MESH_HARD_MAX_EDGES &&
         output_count < counts.edges;
         ++edge_id)
    {
        const henka_authoring_edge* edge =
            henka_authoring_mesh_get_edge(
                mesh,
                (henka_authoring_edge_id)edge_id);

        if (edge == NULL)
        {
            continue;
        }

        out_edges[output_count].id = edge->id;
        out_edges[output_count].vertices[0] =
            edge->vertices[0];

        out_edges[output_count].vertices[1] =
            edge->vertices[1];

        ++output_count;
    }

    if (output_count != counts.edges)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    *out_edge_count = output_count;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_build_authoring_vertex_points(
    const henka_authoring_mesh* mesh,
    sandbox3d_authoring_vertex_point* out_points,
    size_t point_capacity,
    size_t* out_point_count)
{
    henka_authoring_mesh_desc desc;
    henka_authoring_mesh_counts counts;
    size_t physical_slot;
    size_t output_count = 0U;

    if (out_point_count != NULL)
    {
        *out_point_count = 0U;
    }

    if (mesh == NULL ||
        out_points == NULL ||
        out_point_count == NULL ||
        point_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    desc = henka_authoring_mesh_get_desc(mesh);
    counts = henka_authoring_mesh_get_counts(mesh);
    if (counts.vertices > point_capacity)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (physical_slot = 0U;
         physical_slot < desc.max_vertices && output_count < counts.vertices;
         ++physical_slot)
    {
        henka_authoring_vertex_id vertex_id;
        const henka_authoring_vertex* vertex;

        if (henka_authoring_mesh_get_vertex_id_at(
                mesh,
                physical_slot,
                &vertex_id) != HENKA_SUCCESS)
        {
            continue;
        }

        vertex = henka_authoring_mesh_get_vertex(mesh, vertex_id);
        if (vertex == NULL)
        {
            *out_point_count = 0U;
            return HENKA_ERROR_UNKNOWN;
        }

        out_points[output_count].id = vertex_id;
        out_points[output_count].loose =
            henka_authoring_mesh_get_vertex_edge_count(mesh, vertex_id) == 0U;
        ++output_count;
    }

    if (output_count != counts.vertices)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    *out_point_count = output_count;
    return HENKA_SUCCESS;
}
bool sandbox3d_selection_highlight_is_allowed(const sandbox3d_interaction_gate* gate)
{
    return gate != NULL &&
        gate->selected_object_present &&
        gate->selected_object_valid &&
        gate->selected_object_visible &&
        gate->selected_object_selectable &&
        !gate->selected_object_transform_locked &&
        gate->selected_bounds_valid;
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

static bool sandbox3d_projected_triangle_barycentric_at(
    const sandbox3d_projected_triangle* triangle,
    henka_vec2 point,
    float* out_first_weight,
    float* out_second_weight,
    float* out_third_weight)
{
    float denominator;
    float first_weight;
    float second_weight;
    float third_weight;

    if (triangle == NULL || out_first_weight == NULL || out_second_weight == NULL ||
        out_third_weight == NULL)
    {
        return false;
    }
    denominator =
        (triangle->points[1].y - triangle->points[2].y) *
            (triangle->points[0].x - triangle->points[2].x) +
        (triangle->points[2].x - triangle->points[1].x) *
            (triangle->points[0].y - triangle->points[2].y);
    if (!isfinite(denominator) || fabsf(denominator) <= 0.000001f)
    {
        return false;
    }
    first_weight = (
        (triangle->points[1].y - triangle->points[2].y) *
            (point.x - triangle->points[2].x) +
        (triangle->points[2].x - triangle->points[1].x) *
            (point.y - triangle->points[2].y)) / denominator;
    second_weight = (
        (triangle->points[2].y - triangle->points[0].y) *
            (point.x - triangle->points[2].x) +
        (triangle->points[0].x - triangle->points[2].x) *
            (point.y - triangle->points[2].y)) / denominator;
    third_weight = 1.0f - first_weight - second_weight;
    *out_first_weight = first_weight;
    *out_second_weight = second_weight;
    *out_third_weight = third_weight;
    return isfinite(first_weight) && isfinite(second_weight) && isfinite(third_weight);
}

static bool sandbox3d_projected_triangle_depth_at(
    const sandbox3d_projected_triangle* triangle,
    henka_vec2 point,
    float* out_depth)
{
    float first_weight;
    float second_weight;
    float third_weight;

    if (out_depth == NULL ||
        !sandbox3d_projected_triangle_barycentric_at(
            triangle,
            point,
            &first_weight,
            &second_weight,
            &third_weight) ||
        first_weight < -0.0005f || second_weight < -0.0005f || third_weight < -0.0005f)
    {
        return false;
    }
    *out_depth = first_weight * triangle->depths[0] +
        second_weight * triangle->depths[1] + third_weight * triangle->depths[2];
    return isfinite(*out_depth);
}

static bool sandbox3d_projected_triangle_contains_strict(
    const sandbox3d_projected_triangle* triangle,
    henka_vec2 point)
{
    float first_weight;
    float second_weight;
    float third_weight;

    if (!sandbox3d_projected_triangle_barycentric_at(
            triangle,
            point,
            &first_weight,
            &second_weight,
            &third_weight))
    {
        return false;
    }
    return first_weight > 0.0005f &&
        second_weight > 0.0005f &&
        third_weight > 0.0005f;
}

static bool sandbox3d_segment_is_interior_to_projected_union(
    const sandbox3d_projected_triangle* triangles,
    size_t triangle_count,
    henka_vec2 start,
    henka_vec2 end)
{
    static const float sample_parameters[3] = {0.25f, 0.5f, 0.75f};
    size_t sample_index;

    if (triangles == NULL || triangle_count == 0U ||
        !isfinite(start.x) || !isfinite(start.y) ||
        !isfinite(end.x) || !isfinite(end.y))
    {
        return false;
    }

    for (sample_index = 0U; sample_index < 3U; ++sample_index)
    {
        const float parameter = sample_parameters[sample_index];
        const henka_vec2 sample = {
            start.x + (end.x - start.x) * parameter,
            start.y + (end.y - start.y) * parameter};
        size_t triangle_index;
        bool covered = false;

        for (triangle_index = 0U; triangle_index < triangle_count; ++triangle_index)
        {
            if (sandbox3d_projected_triangle_contains_strict(
                    &triangles[triangle_index], sample))
            {
                covered = true;
                break;
            }
        }
        if (!covered)
        {
            return false;
        }
    }
    return true;
}

static bool sandbox3d_append_exposed_silhouette_segment(
    const sandbox3d_projected_triangle* triangles,
    size_t triangle_count,
    henka_vec2 start,
    henka_vec2 end,
    sandbox3d_silhouette_segment* out_segments,
    size_t segment_capacity,
    size_t* inout_output_count)
{
    if (out_segments == NULL || inout_output_count == NULL)
    {
        return false;
    }
    /* Object-mode selection is a union boundary. A child-mesh boundary that
     * is covered through the full segment by another projected surface is an
     * interior seam, not part of the logical object's silhouette. */
    if (sandbox3d_segment_is_interior_to_projected_union(
            triangles, triangle_count, start, end))
    {
        return true;
    }
    if (*inout_output_count >= segment_capacity)
    {
        return false;
    }
    out_segments[*inout_output_count] = (sandbox3d_silhouette_segment){start, end};
    ++*inout_output_count;
    return true;
}

static bool sandbox3d_clip_affine_interval(
    float base,
    float slope,
    float minimum,
    float* inout_start,
    float* inout_end)
{
    float crossing;

    if (inout_start == NULL || inout_end == NULL ||
        !isfinite(base) || !isfinite(slope) || !isfinite(minimum))
    {
        return false;
    }
    if (fabsf(slope) <= 0.000001f)
    {
        return base >= minimum;
    }
    crossing = (minimum - base) / slope;
    if (slope > 0.0f)
    {
        if (crossing > *inout_start)
        {
            *inout_start = crossing;
        }
    }
    else if (crossing < *inout_end)
    {
        *inout_end = crossing;
    }
    return *inout_start <= *inout_end + 0.000001f;
}

static int sandbox3d_compare_float(const void* left, const void* right)
{
    const float left_value = *(const float*)left;
    const float right_value = *(const float*)right;
    return left_value < right_value ? -1 : left_value > right_value ? 1 : 0;
}

size_t sandbox3d_build_topology_silhouette(
    const sandbox3d_projected_triangle* triangles,
    size_t triangle_count,
    sandbox3d_silhouette_segment* out_segments,
    size_t segment_capacity)
{
    /* Keep the edge table load bounded for imported showcase meshes.  A
     * 4,096-slot table silently truncated the dense Giraffe outline before
     * the renderer could classify its real silhouette. */
    enum { edge_slot_count = 65536 };
    typedef struct silhouette_edge
    {
        bool used;
        uint64_t first_id;
        uint64_t second_id;
        uint8_t total_count;
        uint8_t positive_count;
        uint8_t negative_count;
        henka_vec2 start;
        henka_vec2 end;
        float start_depth;
        float end_depth;
    } silhouette_edge;
    /* This bounded outline pass is render-thread owned. Static scratch keeps
     * dense showcase meshes from consuming the application stack while the
     * caller switches from component selection to the full-object outline. */
    static silhouette_edge edges[edge_slot_count];
    size_t triangle_index;
    size_t output_count = 0U;
    float minimum_x = FLT_MAX;
    float maximum_x = -FLT_MAX;
    float minimum_y = FLT_MAX;
    float maximum_y = -FLT_MAX;
    bool visibility_grid_valid = false;

    if (triangles == NULL || out_segments == NULL || triangle_count == 0U || segment_capacity == 0U)
    {
        return 0U;
    }
    if (triangle_count > 4096U)
    {
        for (triangle_index = 0U; triangle_index < triangle_count; ++triangle_index)
        {
            int point_index;
            for (point_index = 0; point_index < 3; ++point_index)
            {
                minimum_x = fminf(minimum_x, triangles[triangle_index].points[point_index].x);
                maximum_x = fmaxf(maximum_x, triangles[triangle_index].points[point_index].x);
                minimum_y = fminf(minimum_y, triangles[triangle_index].points[point_index].y);
                maximum_y = fmaxf(maximum_y, triangles[triangle_index].points[point_index].y);
            }
        }
        visibility_grid_valid = sandbox3d_build_silhouette_visibility_grid(
            triangles,
            triangle_count,
            minimum_x,
            maximum_x,
            minimum_y,
            maximum_y);
    }
    memset(edges, 0, sizeof(edges));
    for (triangle_index = 0U; triangle_index < triangle_count; ++triangle_index)
    {
        const sandbox3d_projected_triangle* triangle = &triangles[triangle_index];
        const float area =
            (triangle->points[1].x - triangle->points[0].x) *
                (triangle->points[2].y - triangle->points[0].y) -
            (triangle->points[1].y - triangle->points[0].y) *
                (triangle->points[2].x - triangle->points[0].x);
        int edge_index;

        if (!isfinite(area) || fabsf(area) <= 0.0001f)
        {
            continue;
        }
        for (edge_index = 0; edge_index < 3; ++edge_index)
        {
            const int next_index = (edge_index + 1) % 3;
            uint64_t first_id = triangle->vertex_ids[edge_index];
            uint64_t second_id = triangle->vertex_ids[next_index];
            size_t slot;
            size_t probe;
            silhouette_edge* edge = NULL;
            const float start_depth = triangle->depths[edge_index];
            const float end_depth = triangle->depths[next_index];

            if (first_id == second_id)
            {
                continue;
            }
            if (first_id > second_id)
            {
                const uint64_t swap = first_id;
                first_id = second_id;
                second_id = swap;
            }
            slot = (size_t)((first_id * UINT64_C(11400714819323198485) ^
                second_id * UINT64_C(14029467366897019727)) % edge_slot_count);
            for (probe = 0U; probe < edge_slot_count; ++probe)
            {
                silhouette_edge* candidate = &edges[(slot + probe) % edge_slot_count];
                if (!candidate->used)
                {
                    candidate->used = true;
                    candidate->first_id = first_id;
                    candidate->second_id = second_id;
                    candidate->start = triangle->points[edge_index];
                    candidate->end = triangle->points[next_index];
                    candidate->start_depth = start_depth;
                    candidate->end_depth = end_depth;
                    edge = candidate;
                    break;
                }
                if (candidate->first_id == first_id && candidate->second_id == second_id)
                {
                    edge = candidate;
                    break;
                }
            }
            if (edge == NULL)
            {
                return 0U;
            }
            if (area > 0.0f)
            {
                if (edge->positive_count < UINT8_MAX) ++edge->positive_count;
            }
            else if (edge->negative_count < UINT8_MAX)
            {
                ++edge->negative_count;
            }
            if (edge->total_count < UINT8_MAX) ++edge->total_count;
            if (edge->total_count > 1U &&
                isfinite(start_depth) && isfinite(end_depth) &&
                start_depth + end_depth <
                    (edge->start_depth + edge->end_depth))
            {
                edge->start = triangle->points[edge_index];
                edge->end = triangle->points[next_index];
                edge->start_depth = start_depth;
                edge->end_depth = end_depth;
            }
        }
    }

    for (triangle_index = 0U; triangle_index < edge_slot_count; ++triangle_index)
    {
        const silhouette_edge* edge = &edges[triangle_index];
        const bool boundary = edge->total_count == 1U;
        const bool front_back = edge->total_count == 2U &&
            edge->positive_count > 0U && edge->negative_count > 0U;
        if (!edge->used || (!boundary && !front_back))
        {
            continue;
        }
        if (triangle_count > 4096U && !visibility_grid_valid)
        {
            if (!sandbox3d_append_exposed_silhouette_segment(
                    triangles,
                    triangle_count,
                    edge->start,
                    edge->end,
                    out_segments,
                    segment_capacity,
                    &output_count))
            {
                return 0U;
            }
            continue;
        }
        {
            enum { max_visibility_events = 65536 };
            static float visibility_events[max_visibility_events];
            size_t visibility_event_count = 2U;
            size_t occluder_index;
            size_t candidate_index;
            size_t candidate_count = triangle_count;
            size_t event_index;

            visibility_events[0] = 0.0f;
            visibility_events[1] = 1.0f;
            if (visibility_grid_valid)
            {
                candidate_count = sandbox3d_collect_silhouette_visibility_candidates(
                    edge->start,
                    edge->end,
                    minimum_x,
                    maximum_x,
                    minimum_y,
                    maximum_y,
                    triangle_count);
            }
            if (isfinite(edge->start_depth) && isfinite(edge->end_depth))
            {
                for (candidate_index = 0U; candidate_index < candidate_count; ++candidate_index)
                {
                    occluder_index = visibility_grid_valid
                        ? sandbox3d_silhouette_candidates[candidate_index]
                        : candidate_index;
                    const sandbox3d_projected_triangle* occluder = &triangles[occluder_index];
                    float start_first;
                    float start_second;
                    float start_third;
                    float end_first;
                    float end_second;
                    float end_third;
                    float interval_start = 0.0f;
                    float interval_end = 1.0f;
                    float occluder_start_depth;
                    float occluder_end_depth;
                    float depth_base;
                    float depth_slope;
                    float depth_crossing;
                    const float edge_min_x = fminf(edge->start.x, edge->end.x) - 0.001f;
                    const float edge_max_x = fmaxf(edge->start.x, edge->end.x) + 0.001f;
                    const float edge_min_y = fminf(edge->start.y, edge->end.y) - 0.001f;
                    const float edge_max_y = fmaxf(edge->start.y, edge->end.y) + 0.001f;

                    if (fmaxf(occluder->points[0].x, fmaxf(occluder->points[1].x, occluder->points[2].x)) < edge_min_x ||
                        fminf(occluder->points[0].x, fminf(occluder->points[1].x, occluder->points[2].x)) > edge_max_x ||
                        fmaxf(occluder->points[0].y, fmaxf(occluder->points[1].y, occluder->points[2].y)) < edge_min_y ||
                        fminf(occluder->points[0].y, fminf(occluder->points[1].y, occluder->points[2].y)) > edge_max_y)
                    {
                        continue;
                    }
                    if (!sandbox3d_projected_triangle_barycentric_at(
                            occluder,
                            edge->start,
                            &start_first,
                            &start_second,
                            &start_third) ||
                        !sandbox3d_projected_triangle_barycentric_at(
                            occluder,
                            edge->end,
                            &end_first,
                            &end_second,
                            &end_third) ||
                        !sandbox3d_clip_affine_interval(
                            start_first,
                            end_first - start_first,
                            -0.0005f,
                            &interval_start,
                            &interval_end) ||
                        !sandbox3d_clip_affine_interval(
                            start_second,
                            end_second - start_second,
                            -0.0005f,
                            &interval_start,
                            &interval_end) ||
                        !sandbox3d_clip_affine_interval(
                            start_third,
                            end_third - start_third,
                            -0.0005f,
                            &interval_start,
                            &interval_end) ||
                        interval_end < -0.000001f || interval_start > 1.000001f)
                    {
                        continue;
                    }
                    interval_start = fmaxf(interval_start, 0.0f);
                    interval_end = fminf(interval_end, 1.0f);
                    if (interval_end < interval_start)
                    {
                        continue;
                    }
                    if (visibility_event_count + 2U >= max_visibility_events)
                    {
                        return 0U;
                    }
                    visibility_events[visibility_event_count++] = interval_start;
                    visibility_events[visibility_event_count++] = interval_end;
                    occluder_start_depth = start_first * occluder->depths[0] +
                        start_second * occluder->depths[1] + start_third * occluder->depths[2];
                    occluder_end_depth = end_first * occluder->depths[0] +
                        end_second * occluder->depths[1] + end_third * occluder->depths[2];
                    depth_base = edge->start_depth - occluder_start_depth;
                    depth_slope = (edge->end_depth - occluder_end_depth) - depth_base;
                    if (fabsf(depth_slope) > 0.000001f)
                    {
                        depth_crossing = (0.0015f - depth_base) / depth_slope;
                        if (depth_crossing > interval_start + 0.000001f &&
                            depth_crossing < interval_end - 0.000001f &&
                            visibility_event_count + 1U < max_visibility_events)
                        {
                            visibility_events[visibility_event_count++] = depth_crossing;
                        }
                    }
                }
            }
            qsort(
                visibility_events,
                visibility_event_count,
                sizeof(visibility_events[0]),
                sandbox3d_compare_float);
            {
                bool visible_run = false;
                float visible_start = 0.0f;
                float visible_end = 0.0f;
            for (event_index = 0U; event_index + 1U < visibility_event_count; ++event_index)
            {
                const float interval_start = visibility_events[event_index];
                const float interval_end = visibility_events[event_index + 1U];
                const float sample_t = (interval_start + interval_end) * 0.5f;
                const henka_vec2 sample_point = {
                    edge->start.x + (edge->end.x - edge->start.x) * sample_t,
                    edge->start.y + (edge->end.y - edge->start.y) * sample_t};
                const float sample_depth = edge->start_depth +
                    (edge->end_depth - edge->start_depth) * sample_t;
                bool visible = interval_end > interval_start + 0.0001f;

                if (visible && isfinite(sample_depth))
                {
                    for (candidate_index = 0U; candidate_index < candidate_count; ++candidate_index)
                    {
                        occluder_index = visibility_grid_valid
                            ? sandbox3d_silhouette_candidates[candidate_index]
                            : candidate_index;
                        float occluder_depth;
                        if (sandbox3d_projected_triangle_depth_at(
                                &triangles[occluder_index], sample_point, &occluder_depth) &&
                            occluder_depth + 0.0015f < sample_depth)
                        {
                            visible = false;
                            break;
                        }
                    }
                }
                if (visible)
                {
                    if (!visible_run)
                    {
                        visible_start = interval_start;
                        visible_run = true;
                    }
                    visible_end = interval_end;
                }
                else if (visible_run)
                {
                    if (!sandbox3d_append_exposed_silhouette_segment(
                            triangles,
                            triangle_count,
                            (henka_vec2){
                                edge->start.x + (edge->end.x - edge->start.x) * visible_start,
                                edge->start.y + (edge->end.y - edge->start.y) * visible_start},
                            (henka_vec2){
                                edge->start.x + (edge->end.x - edge->start.x) * visible_end,
                                edge->start.y + (edge->end.y - edge->start.y) * visible_end},
                            out_segments,
                            segment_capacity,
                            &output_count))
                    {
                        return 0U;
                    }
                    visible_run = false;
                }
            }
            if (visible_run)
            {
                if (!sandbox3d_append_exposed_silhouette_segment(
                        triangles,
                        triangle_count,
                        (henka_vec2){
                            edge->start.x + (edge->end.x - edge->start.x) * visible_start,
                            edge->start.y + (edge->end.y - edge->start.y) * visible_start},
                        (henka_vec2){
                            edge->start.x + (edge->end.x - edge->start.x) * visible_end,
                            edge->start.y + (edge->end.y - edge->start.y) * visible_end},
                        out_segments,
                        segment_capacity,
                        &output_count))
                {
                    return 0U;
                }
            }
            }
        }
    }
    return output_count;
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
