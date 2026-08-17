#ifndef SANDBOX3D_INTERACTION_TOOLS_H
#define SANDBOX3D_INTERACTION_TOOLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/gizmo.h>
#include <henka/math.h>
#include <henka/ui.h>

typedef enum sandbox3d_viewport_tool_mode
{
    SANDBOX3D_VIEWPORT_TOOL_SELECT = 0,
    SANDBOX3D_VIEWPORT_TOOL_ORBIT,
    SANDBOX3D_VIEWPORT_TOOL_PAN,
    SANDBOX3D_VIEWPORT_TOOL_MOVE,
    SANDBOX3D_VIEWPORT_TOOL_ROTATE,
    SANDBOX3D_VIEWPORT_TOOL_SCALE,
    SANDBOX3D_VIEWPORT_TOOL_COUNT
} sandbox3d_viewport_tool_mode;

typedef enum sandbox3d_interaction_reject_reason
{
    SANDBOX3D_INTERACTION_REJECT_NONE = 0,
    SANDBOX3D_INTERACTION_REJECT_MOUSE_CAPTURE_ACTIVE,
    SANDBOX3D_INTERACTION_REJECT_UI_OWNS_MOUSE,
    SANDBOX3D_INTERACTION_REJECT_CURSOR_OUTSIDE_VIEWPORT,
    SANDBOX3D_INTERACTION_REJECT_NO_SELECTED_OBJECT,
    SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_INVALID,
    SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_HIDDEN,
    SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_NOT_SELECTABLE,
    SANDBOX3D_INTERACTION_REJECT_SELECTED_OBJECT_TRANSFORM_LOCKED,
    SANDBOX3D_INTERACTION_REJECT_SELECTED_BOUNDS_INVALID,
    SANDBOX3D_INTERACTION_REJECT_GIZMO_MODE_INACTIVE,
    SANDBOX3D_INTERACTION_REJECT_GIZMO_MODEL_INVALID,
    SANDBOX3D_INTERACTION_REJECT_OVERLAY_HAS_NO_PRIMITIVES,
    SANDBOX3D_INTERACTION_REJECT_NO_HANDLE_UNDER_CURSOR,
    SANDBOX3D_INTERACTION_REJECT_CURSOR_IN_GIZMO_DEAD_ZONE,
    SANDBOX3D_INTERACTION_REJECT_VIEWPORT_CHANGED_DURING_DRAG,
    SANDBOX3D_INTERACTION_REJECT_DRAG_TARGET_INVALID,
    SANDBOX3D_INTERACTION_REJECT_ACTION_COMMAND_FAILED,
    SANDBOX3D_INTERACTION_REJECT_NAVIGATION_MODE_INACTIVE,
    SANDBOX3D_INTERACTION_REJECT_PANEL_CLICK_REJECTED,
    SANDBOX3D_INTERACTION_REJECT_INPUT_NOT_SUPPORTED_ON_CURRENT_MOUSE_BUTTON
} sandbox3d_interaction_reject_reason;

typedef struct sandbox3d_interaction_gate
{
    bool mouse_capture_active;
    bool ui_wants_mouse;
    bool cursor_in_viewport;
    bool panel_click_rejected;
    bool supported_mouse_button;
    bool selected_object_present;
    bool selected_object_valid;
    bool selected_object_visible;
    bool selected_object_selectable;
    bool selected_object_transform_locked;
    bool selected_bounds_valid;
    bool gizmo_mode_active;
    bool gizmo_model_valid;
    bool overlay_has_primitives;
    bool handle_under_cursor;
    bool cursor_in_gizmo_dead_zone;
    bool navigation_mode_active;
} sandbox3d_interaction_gate;

typedef struct sandbox3d_selection_highlight_model
{
    bool valid;
    size_t edge_count;
    henka_vec3 edge_starts[12];
    henka_vec3 edge_ends[12];
} sandbox3d_selection_highlight_model;

typedef struct sandbox3d_projected_triangle
{
    henka_vec2 points[3];
    float depths[3];
    /* IDs are source-scoped, not mesh-local.  This lets one logical scene
     * object aggregate multiple children without inventing shared edges. */
    uint64_t vertex_ids[3];
} sandbox3d_projected_triangle;

typedef struct sandbox3d_silhouette_segment
{
    henka_vec2 start;
    henka_vec2 end;
} sandbox3d_silhouette_segment;

/* Keep ordinary clicks as selection while allowing laptop touchpads to pan
 * only after an intentional empty-viewport drag. */
#define SANDBOX3D_EMPTY_VIEWPORT_PAN_DRAG_THRESHOLD_PIXELS 6.0f

const char* sandbox3d_viewport_tool_mode_to_string(sandbox3d_viewport_tool_mode tool_mode);
const char* sandbox3d_interaction_reject_reason_to_string(sandbox3d_interaction_reject_reason reason);
henka_gizmo_mode sandbox3d_viewport_tool_mode_to_gizmo_mode(sandbox3d_viewport_tool_mode tool_mode);
bool sandbox3d_viewport_tool_mode_uses_gizmo(sandbox3d_viewport_tool_mode tool_mode);
bool sandbox3d_viewport_tool_mode_is_navigation(sandbox3d_viewport_tool_mode tool_mode);
bool sandbox3d_point_is_owned_by_panels(
    henka_vec2 framebuffer_point,
    const henka_ui_rect* panel_bounds,
    size_t panel_count);
sandbox3d_interaction_reject_reason sandbox3d_evaluate_navigation_reject_reason(
    sandbox3d_viewport_tool_mode tool_mode,
    const sandbox3d_interaction_gate* gate);
sandbox3d_interaction_reject_reason sandbox3d_evaluate_gizmo_reject_reason(
    sandbox3d_viewport_tool_mode tool_mode,
    const sandbox3d_interaction_gate* gate);
sandbox3d_interaction_reject_reason sandbox3d_evaluate_select_reject_reason(
    const sandbox3d_interaction_gate* gate);
bool sandbox3d_should_start_empty_viewport_pan(
    sandbox3d_viewport_tool_mode tool_mode,
    const sandbox3d_interaction_gate* gate,
    bool empty_viewport_drag_candidate,
    bool gizmo_drag_active,
    bool alt_orbit_active,
    float drag_distance_pixels);
bool sandbox3d_should_prefer_terrain_hit(
    bool terrain_hit,
    float terrain_distance,
    bool object_hit,
    float object_distance);
bool sandbox3d_selection_highlight_is_allowed(const sandbox3d_interaction_gate* gate);
henka_vec3 sandbox3d_make_move_delta(henka_gizmo_axis axis, float magnitude);
henka_quat sandbox3d_make_rotation_delta(henka_gizmo_axis axis, float radians);
henka_vec3 sandbox3d_make_uniform_scale_multiplier(float delta_scale);
bool sandbox3d_build_selection_highlight_model(
    henka_bounds bounds,
    sandbox3d_selection_highlight_model* out_model);
bool sandbox3d_build_ground_selection_highlight_model(
    henka_vec3 center,
    float half_extent,
    float y_offset,
    sandbox3d_selection_highlight_model* out_model);
/* Builds boundary/front-back silhouette segments from projected indexed
 * triangles. Internal diagonals and invented convex-hull chords are omitted.
 * The bounded implementation also suppresses samples hidden by nearer
 * projected triangles; exact renderer-owned stencil/mask outlines remain a
 * separate presentation path. The caller owns the bounded output storage. */
size_t sandbox3d_build_topology_silhouette(
    const sandbox3d_projected_triangle* triangles,
    size_t triangle_count,
    sandbox3d_silhouette_segment* out_segments,
    size_t segment_capacity);
bool sandbox3d_clip_line_to_rect(
    henka_vec2* start,
    henka_vec2* end,
    henka_ui_rect rect);

#endif
