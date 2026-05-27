#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>

#include <henka/henka.h>

#include "interaction_tools.h"
#include "workspace_tools.h"

typedef enum sandbox3d_object_kind
{
    SANDBOX3D_OBJECT_GROUND = 0,
    SANDBOX3D_OBJECT_TEXTURED_CUBE,
    SANDBOX3D_OBJECT_COLORED_CUBE,
    SANDBOX3D_OBJECT_OBJ_MARKER,
    SANDBOX3D_OBJECT_MISSING_TEXTURE,
    SANDBOX3D_OBJECT_MISSING_MODEL,
    SANDBOX3D_OBJECT_DEBUG_GRID,
    SANDBOX3D_OBJECT_COUNT
} sandbox3d_object_kind;

typedef struct sandbox3d_object_descriptor
{
    sandbox3d_object_kind kind;
    henka_entity entity;
    const char* display_name;
    const char* location_hint;
    const char* short_explanation;
    const char* developer_detail;
    const char* mesh_summary;
    const char* material_summary;
    const char* texture_summary;
    bool can_hide;
    bool can_reset;
    henka_transform default_transform;
} sandbox3d_object_descriptor;

typedef enum sandbox3d_layout_mode
{
    SANDBOX3D_LAYOUT_VIEW = 0,
    SANDBOX3D_LAYOUT_INSPECT,
    SANDBOX3D_LAYOUT_FULL,
    SANDBOX3D_LAYOUT_COUNT
} sandbox3d_layout_mode;

typedef enum sandbox3d_utility_view
{
    SANDBOX3D_UTILITY_NONE = 0,
    SANDBOX3D_UTILITY_HELP,
    SANDBOX3D_UTILITY_SCENE_LEGEND,
    SANDBOX3D_UTILITY_OBJECT_INFO,
    SANDBOX3D_UTILITY_PATHS,
    SANDBOX3D_UTILITY_SETTINGS,
    SANDBOX3D_UTILITY_DIAGNOSTICS,
    SANDBOX3D_UTILITY_TRANSFORM_QA,
    SANDBOX3D_UTILITY_PHYSICS_QA,
    SANDBOX3D_UTILITY_COUNT
} sandbox3d_utility_view;

typedef enum sandbox3d_gizmo_mode
{
    SANDBOX3D_GIZMO_MODE_SELECT = 0,
    SANDBOX3D_GIZMO_MODE_MOVE,
    SANDBOX3D_GIZMO_MODE_ROTATE,
    SANDBOX3D_GIZMO_MODE_SCALE,
    SANDBOX3D_GIZMO_MODE_COUNT
} sandbox3d_gizmo_mode;

typedef enum sandbox3d_gizmo_axis
{
    SANDBOX3D_GIZMO_AXIS_NONE = 0,
    SANDBOX3D_GIZMO_AXIS_X,
    SANDBOX3D_GIZMO_AXIS_Y,
    SANDBOX3D_GIZMO_AXIS_Z,
    SANDBOX3D_GIZMO_AXIS_UNIFORM
} sandbox3d_gizmo_axis;

typedef struct sandbox3d_gizmo_drag_state
{
    bool dragging;
    henka_entity target_entity;
    henka_vec2 drag_start_mouse_framebuffer;
    henka_vec2 drag_start_mouse_local;
    henka_viewport drag_start_viewport;
    henka_transform drag_start_transform;
    henka_vec2 drag_axis_screen_direction;
    henka_vec2 drag_center_screen;
    float drag_axis_screen_length;
    float drag_start_angle;
    float drag_start_projection;
    sandbox3d_gizmo_axis active_axis;
    sandbox3d_gizmo_mode active_mode;
} sandbox3d_gizmo_drag_state;

typedef struct sandbox3d_gizmo_snap_state
{
    bool enabled;
    float move_snap_increment;
    float rotate_snap_increment;
    float scale_snap_increment;
} sandbox3d_gizmo_snap_state;

typedef enum sandbox3d_gizmo_handle_type
{
    SANDBOX3D_GIZMO_HANDLE_NONE = 0,
    SANDBOX3D_GIZMO_HANDLE_MOVE_AXIS,
    SANDBOX3D_GIZMO_HANDLE_MOVE_BOX,
    SANDBOX3D_GIZMO_HANDLE_ROTATE_RING,
    SANDBOX3D_GIZMO_HANDLE_SCALE_UNIFORM
} sandbox3d_gizmo_handle_type;

typedef struct sandbox3d_gizmo_state
{
    sandbox3d_gizmo_mode mode;
    sandbox3d_gizmo_axis hover_axis;
    sandbox3d_gizmo_handle_type hover_handle_type;
    sandbox3d_gizmo_handle_type active_handle_type;
    henka_gizmo_drag_state drag;
    sandbox3d_gizmo_snap_state snap;
} sandbox3d_gizmo_state;

typedef struct sandbox3d_workspace_state
{
    sandbox3d_layout_mode layout_mode;
    bool scene_objects_panel_visible;
    bool object_details_panel_visible;
    sandbox3d_utility_view active_utility;
    sandbox3d_workspace_model model;
} sandbox3d_workspace_state;

typedef struct sandbox3d_workspace_layout
{
    henka_ui_rect left_dock;
    henka_ui_rect scene_frame;
    henka_ui_rect right_dock;
    henka_ui_rect controls_panel;
    henka_ui_rect scene_objects_panel;
    henka_ui_rect object_details_panel;
    henka_ui_rect utility_panel;
    henka_viewport scene_viewport;
    henka_ui_rect debug_strip;
    henka_ui_rect left_splitter;
    henka_ui_rect right_splitter;
} sandbox3d_workspace_layout;

typedef enum sandbox3d_panel_scroll_target
{
    SANDBOX3D_PANEL_SCROLL_NONE = 0,
    SANDBOX3D_PANEL_SCROLL_CONTROLS,
    SANDBOX3D_PANEL_SCROLL_SCENE_OBJECTS,
    SANDBOX3D_PANEL_SCROLL_DETAILS,
    SANDBOX3D_PANEL_SCROLL_UTILITY
} sandbox3d_panel_scroll_target;

typedef struct sandbox3d_panel_paging_state
{
    int controls_page;
    int scene_objects_page;
} sandbox3d_panel_paging_state;

typedef struct sandbox3d_view_navigation_state
{
    bool orbiting;
    bool panning;
    henka_vec3 orbit_target;
    bool orbit_target_valid;
} sandbox3d_view_navigation_state;

typedef struct sandbox3d_interaction_diagnostics
{
    bool show_handle_hit_boxes;
    bool ui_wants_mouse;
    bool cursor_in_viewport;
    bool mouse_framebuffer_valid;
    bool viewport_local_valid;
    bool selected_entity_valid;
    bool selected_entity_visible;
    bool selected_entity_selectable;
    bool selected_bounds_valid;
    bool selected_highlight_active;
    bool gizmo_model_valid;
    bool dragging;
    henka_vec2 window_mouse;
    henka_vec2 framebuffer_mouse;
    henka_vec2 viewport_local;
    henka_entity selected_entity;
    henka_entity drag_target_entity;
    size_t overlay_primitive_count;
    sandbox3d_viewport_tool_mode viewport_tool;
    sandbox3d_interaction_reject_reason last_reject_reason;
    char last_drag_result[160];
    char last_action_command[64];
    char last_action_result[160];
    char last_success_result[160];
    char last_workspace_action[128];
    char last_selection_action[128];
    sandbox3d_workspace_panel_id hovered_panel;
    bool cursor_in_panel_header;
    sandbox3d_workspace_panel_id active_panel_drag;
    sandbox3d_workspace_panel_id active_panel_resize;
    sandbox3d_workspace_resize_target active_workspace_resize;
} sandbox3d_interaction_diagnostics;

typedef struct sandbox3d_gizmo_render_state
{
    henka_mesh* axis_mesh;
    henka_mesh* ring_mesh;
    henka_entity axis_entities[3];
    henka_entity axis_handle_entities[3];
    henka_entity ring_entities[3];
    henka_entity center_entity;
} sandbox3d_gizmo_render_state;

#define SANDBOX3D_GIZMO_RING_SAMPLES 49
#define SANDBOX3D_GIZMO_MAX_HANDLES 10

typedef struct sandbox3d_gizmo_handle_model
{
    bool visible;
    sandbox3d_gizmo_mode mode;
    sandbox3d_gizmo_axis axis;
    sandbox3d_gizmo_handle_type type;
    henka_vec3 world_start;
    henka_vec3 world_end;
    henka_vec3 world_center;
    henka_vec2 screen_start;
    henka_vec2 screen_end;
    henka_vec2 screen_center;
    henka_vec2 screen_half_extents;
    float hit_tolerance;
    size_t point_count;
    henka_vec2 points[SANDBOX3D_GIZMO_RING_SAMPLES];
} sandbox3d_gizmo_handle_model;

typedef struct sandbox3d_gizmo_model
{
    bool valid;
    henka_entity target_entity;
    sandbox3d_gizmo_mode mode;
    henka_transform target_transform;
    henka_viewport viewport;
    henka_vec2 mouse_framebuffer;
    henka_vec2 mouse_local;
    henka_vec2 screen_center;
    float gizmo_size;
    float dead_zone_radius;
    size_t handle_count;
    sandbox3d_gizmo_handle_model handles[SANDBOX3D_GIZMO_MAX_HANDLES];
} sandbox3d_gizmo_model;

typedef struct sandbox3d_physics_state
{
    henka_physics_world* world;
    henka_physics_body_id bodies[SANDBOX3D_OBJECT_COUNT];
    bool enabled;
    bool paused;
    bool gravity_enabled;
    bool debug_colliders;
    bool debug_contacts;
    henka_physics_raycast_hit last_raycast;
    char last_action[128];
} sandbox3d_physics_state;

typedef struct sandbox3d_state
{
    henka_scene* scene;
    henka_action_context* actions;
    henka_settings* settings;
    henka_ui_context* ui;
    henka_ui_context* native_panel_ui;
    henka_window_id native_panel_window_id;
    henka_camera camera;
    henka_mesh* cube_mesh;
    henka_mesh* ground_mesh;
    henka_mesh* grid_mesh;
    henka_mesh* marker_mesh;
    henka_mesh* missing_model_mesh;
    henka_shader* basic_shader;
    henka_shader* grid_shader;
    henka_texture* cube_texture;
    henka_texture* ground_texture;
    henka_texture* missing_texture;
    henka_entity cube_entity;
    henka_entity ground_entity;
    henka_entity grid_entity;
    henka_entity colored_cube_entity;
    henka_entity fallback_cube_entity;
    henka_entity marker_entity;
    henka_entity fallback_model_entity;
    sandbox3d_object_descriptor descriptors[SANDBOX3D_OBJECT_COUNT];
    sandbox3d_workspace_state workspace;
    sandbox3d_gizmo_state gizmo;
    sandbox3d_gizmo_render_state gizmo_render;
    henka_entity selected_entity;
    bool settings_file_found;
    bool startup_panels_auto_opened;
    bool ui_visibility_report_pending;
    bool ui_visible_last_frame;
    bool status_warning;
    char status_message[160];
    sandbox3d_workspace_layout frame_layout;
    henka_gizmo_model gizmo_model;
    sandbox3d_panel_paging_state paging;
    sandbox3d_view_navigation_state view_navigation;
    sandbox3d_viewport_tool_mode viewport_tool;
    sandbox3d_interaction_diagnostics diagnostics;
    sandbox3d_physics_state physics;
} sandbox3d_state;

static const float g_default_mouse_look_sensitivity = 0.0025f;
static const float g_default_camera_movement_speed = 4.0f;
static const henka_vec3 g_camera_start_position = {0.0f, 2.4f, 8.6f};
static const float g_camera_start_yaw = -HENKA_PI * 0.5f;
static const float g_camera_start_pitch = -0.22f;
static const henka_vec3 g_textured_cube_position = {0.0f, 0.5f, 0.0f};
static const henka_vec3 g_colored_cube_position = {-2.6f, 0.5f, 1.4f};
static const henka_vec3 g_missing_texture_position = {2.6f, 0.5f, 1.4f};
static const henka_vec3 g_marker_position = {-4.4f, 0.0f, -0.9f};
static const henka_vec3 g_missing_model_position = {4.4f, 0.5f, -0.9f};

static const float g_ui_panel_margin = 20.0f;
static const float g_ui_panel_gap = 12.0f;
static const float g_ui_controls_width = 320.0f;
static const float g_ui_scene_width = 260.0f;
static const float g_ui_details_width = 352.0f;
static const float g_ui_panel_height = 360.0f;
static const float g_ui_debug_strip_height = 58.0f;

static const char* g_setting_key_grid_visible = "grid_visible";
static const char* g_setting_key_wireframe_enabled = "wireframe_enabled";
static const char* g_setting_key_mouse_sensitivity = "mouse_sensitivity";
static const char* g_setting_key_camera_speed = "camera_movement_speed";
static const char* g_setting_key_camera_position_x = "camera_position_x";
static const char* g_setting_key_camera_position_y = "camera_position_y";
static const char* g_setting_key_camera_position_z = "camera_position_z";
static const char* g_setting_key_camera_yaw = "camera_yaw_radians";
static const char* g_setting_key_camera_pitch = "camera_pitch_radians";
static const char* g_setting_key_selected_object_name = "ui.selected_object_name";
static const char* g_setting_key_scene_panel_visible = "ui.scene_objects_panel_visible";
static const char* g_setting_key_details_panel_visible = "ui.object_details_panel_visible";
static const char* g_setting_key_layout_mode = "ui.layout_mode";
static const char* g_setting_key_active_utility = "ui.active_utility";

static float sandbox3d_get_mouse_sensitivity(const sandbox3d_state* state);
static void sandbox3d_set_status(sandbox3d_state* state, bool warning, const char* message);
static void sandbox3d_set_statusf(sandbox3d_state* state, bool warning, bool print_console, const char* format, ...);
static void sandbox3d_record_reject_reason(
    sandbox3d_state* state,
    sandbox3d_interaction_reject_reason reason,
    bool update_status);
static void sandbox3d_record_drag_result(sandbox3d_state* state, const char* message);
static void sandbox3d_record_action_result(
    sandbox3d_state* state,
    henka_action_command command,
    const henka_action_result* result);
static void sandbox3d_record_success_result(sandbox3d_state* state, const char* format, ...);
static const sandbox3d_object_descriptor* sandbox3d_get_descriptor_by_entity(const sandbox3d_state* state, henka_entity entity);
static bool sandbox3d_is_drag_target_valid(const sandbox3d_state* state, henka_entity entity);
static void sandbox3d_clear_gizmo_drag(sandbox3d_state* state, bool clear_hover_axis);
static void sandbox3d_set_viewport_tool_mode(
    sandbox3d_state* state,
    sandbox3d_viewport_tool_mode tool_mode,
    bool update_status);
static henka_entity sandbox3d_get_real_selected_entity(const sandbox3d_state* state);
static const sandbox3d_object_descriptor* sandbox3d_get_selected_descriptor(const sandbox3d_state* state);
static const char* sandbox3d_safe_entity_name(const sandbox3d_state* state, henka_entity entity, const char* fallback_name);
static bool sandbox3d_is_selectable_entity(const sandbox3d_state* state, henka_entity entity);
static void sandbox3d_select_entity(sandbox3d_state* state, henka_entity entity);
static void sandbox3d_clear_selection(sandbox3d_state* state, const char* reason);
static bool sandbox3d_execute_action(
    sandbox3d_state* state,
    const henka_action_request* request,
    henka_action_result* out_result);
static henka_vec2 sandbox3d_vec2_subtract(henka_vec2 left, henka_vec2 right);
static float sandbox3d_vec2_length(henka_vec2 value);
static float sandbox3d_vec2_dot(henka_vec2 left, henka_vec2 right);
static henka_vec2 sandbox3d_vec2_normalize(henka_vec2 value);
static bool sandbox3d_try_get_mouse_framebuffer_position(henka_engine* engine, henka_vec2* out_mouse_framebuffer);
static bool sandbox3d_try_get_mouse_viewport_local(
    henka_engine* engine,
    henka_viewport viewport,
    henka_vec2* out_mouse_framebuffer,
    henka_vec2* out_mouse_local);
static bool sandbox3d_get_selected_bounds(const sandbox3d_state* state, henka_bounds* out_bounds);
static henka_vec3 sandbox3d_get_default_orbit_target(void);
static henka_vec3 sandbox3d_get_view_navigation_target(const sandbox3d_state* state);
static void sandbox3d_set_view_navigation_target(sandbox3d_state* state, henka_vec3 target);
static void sandbox3d_sync_view_navigation_target_to_selection(sandbox3d_state* state);
static void sandbox3d_zoom_camera_to_target(sandbox3d_state* state, float direction_scale);
static void sandbox3d_frame_selected_object(sandbox3d_state* state, bool print_status);
static bool sandbox3d_collect_gizmo_overlay_state(
    henka_engine* engine,
    sandbox3d_state* state,
    henka_gizmo_model* out_model,
    henka_gizmo_overlay_model* out_overlay,
    sandbox3d_gizmo_axis* out_hover_axis,
    sandbox3d_gizmo_handle_type* out_hover_type,
    bool* out_in_dead_zone);
static void sandbox3d_refresh_interaction_diagnostics(henka_engine* engine, sandbox3d_state* state);
static bool sandbox3d_ui_owns_mouse_at_point(const sandbox3d_state* state, henka_vec2 framebuffer_mouse);
static bool sandbox3d_handle_workspace_input(
    henka_engine* engine,
    sandbox3d_state* state,
    henka_vec2 framebuffer_mouse,
    int framebuffer_width,
    int framebuffer_height);
static void sandbox3d_draw_workspace_affordances(
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout,
    int framebuffer_width,
    int framebuffer_height);
static bool sandbox3d_workspace_can_dock_panel(
    const sandbox3d_state* state,
    sandbox3d_workspace_panel_id panel_id,
    sandbox3d_workspace_dock_zone dock_zone);
static void sandbox3d_dock_workspace_panel(
    sandbox3d_state* state,
    sandbox3d_workspace_panel_id panel_id,
    sandbox3d_workspace_dock_zone dock_zone);
static void sandbox3d_draw_panel_workspace_controls(
    henka_engine* engine,
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout,
    sandbox3d_workspace_panel_id panel_id);
static henka_ui_rect sandbox3d_get_panel_rect(
    const sandbox3d_workspace_layout* layout,
    sandbox3d_workspace_panel_id panel_id);
static henka_ui_rect sandbox3d_get_workspace_dock_target_rect(
    const sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout,
    sandbox3d_workspace_dock_zone dock_zone,
    int framebuffer_width,
    int framebuffer_height);
static bool sandbox3d_workspace_panel_visible(
    const sandbox3d_state* state,
    sandbox3d_workspace_panel_id panel_id);
static bool sandbox3d_apply_transform_action(
    sandbox3d_state* state,
    henka_action_command command,
    henka_entity entity,
    henka_vec3 vector_value,
    henka_quat rotation_value);
static bool sandbox3d_apply_move_step(sandbox3d_state* state, henka_gizmo_axis axis, float amount);
static bool sandbox3d_apply_rotate_step(sandbox3d_state* state, henka_gizmo_axis axis, float radians);
static bool sandbox3d_apply_scale_step(sandbox3d_state* state, float delta_scale);
static sandbox3d_panel_scroll_target sandbox3d_get_panel_scroll_target(const sandbox3d_state* state, henka_vec2 point);
static void sandbox3d_advance_panel_paging(sandbox3d_state* state, sandbox3d_panel_scroll_target target, int delta);
static void sandbox3d_draw_value_row(
    henka_ui_context* ui,
    float x,
    float y,
    float width,
    const char* label,
    const char* value);
static bool sandbox3d_open_native_panel_test(henka_engine* engine, sandbox3d_state* state);
static void sandbox3d_close_native_panel_test(henka_engine* engine, sandbox3d_state* state);
static void sandbox3d_build_native_panel_test_ui(henka_engine* engine, sandbox3d_state* state);
static henka_result sandbox3d_initialize_physics(sandbox3d_state* state);
static void sandbox3d_update_physics(sandbox3d_state* state, double delta_seconds);
static void sandbox3d_draw_physics_overlay(sandbox3d_state* state, henka_viewport viewport);
static void sandbox3d_draw_selection_highlight(sandbox3d_state* state, henka_viewport viewport);
static void sandbox3d_prepare_physics_demo(sandbox3d_state* state);

static const char* sandbox3d_get_build_configuration_label(void)
{
#if defined(_DEBUG)
    return "Debug";
#else
    return "Release";
#endif
}

static const char* sandbox3d_get_layout_mode_label(sandbox3d_layout_mode layout_mode)
{
    switch (layout_mode)
    {
        case SANDBOX3D_LAYOUT_VIEW:
            return "View";
        case SANDBOX3D_LAYOUT_INSPECT:
            return "Inspect";
        case SANDBOX3D_LAYOUT_FULL:
            return "Full Tools";
        default:
            return "View";
    }
}

static const char* sandbox3d_get_layout_mode_setting_value(sandbox3d_layout_mode layout_mode)
{
    switch (layout_mode)
    {
        case SANDBOX3D_LAYOUT_VIEW:
            return "view";
        case SANDBOX3D_LAYOUT_INSPECT:
            return "inspect";
        case SANDBOX3D_LAYOUT_FULL:
            return "full";
        default:
            return "view";
    }
}

static sandbox3d_layout_mode sandbox3d_parse_layout_mode(const char* value)
{
    if (value == NULL || value[0] == '\0')
    {
        return SANDBOX3D_LAYOUT_VIEW;
    }

    if (strcmp(value, "view") == 0)
    {
        return SANDBOX3D_LAYOUT_VIEW;
    }
    if (strcmp(value, "inspect") == 0)
    {
        return SANDBOX3D_LAYOUT_INSPECT;
    }
    if (strcmp(value, "full") == 0)
    {
        return SANDBOX3D_LAYOUT_FULL;
    }

    return SANDBOX3D_LAYOUT_VIEW;
}

static const char* sandbox3d_get_utility_label(sandbox3d_utility_view utility)
{
    switch (utility)
    {
        case SANDBOX3D_UTILITY_HELP:
            return "Help";
        case SANDBOX3D_UTILITY_SCENE_LEGEND:
            return "Scene Legend";
        case SANDBOX3D_UTILITY_OBJECT_INFO:
            return "Object Info";
        case SANDBOX3D_UTILITY_PATHS:
            return "Paths";
        case SANDBOX3D_UTILITY_SETTINGS:
            return "Settings";
        case SANDBOX3D_UTILITY_DIAGNOSTICS:
            return "Diagnostics";
        case SANDBOX3D_UTILITY_TRANSFORM_QA:
            return "Transform QA";
        case SANDBOX3D_UTILITY_PHYSICS_QA:
            return "Physics QA";
        case SANDBOX3D_UTILITY_NONE:
        default:
            return "None";
    }
}

static const char* sandbox3d_get_utility_setting_value(sandbox3d_utility_view utility)
{
    switch (utility)
    {
        case SANDBOX3D_UTILITY_HELP:
            return "help";
        case SANDBOX3D_UTILITY_SCENE_LEGEND:
            return "scene_legend";
        case SANDBOX3D_UTILITY_OBJECT_INFO:
            return "object_info";
        case SANDBOX3D_UTILITY_PATHS:
            return "paths";
        case SANDBOX3D_UTILITY_SETTINGS:
            return "settings";
        case SANDBOX3D_UTILITY_DIAGNOSTICS:
            return "diagnostics";
        case SANDBOX3D_UTILITY_TRANSFORM_QA:
            return "transform_qa";
        case SANDBOX3D_UTILITY_PHYSICS_QA:
            return "physics_qa";
        case SANDBOX3D_UTILITY_NONE:
        default:
            return "none";
    }
}

static sandbox3d_utility_view sandbox3d_parse_utility_view(const char* value)
{
    if (value == NULL || value[0] == '\0')
    {
        return SANDBOX3D_UTILITY_NONE;
    }

    if (strcmp(value, "help") == 0)
    {
        return SANDBOX3D_UTILITY_HELP;
    }
    if (strcmp(value, "scene_legend") == 0)
    {
        return SANDBOX3D_UTILITY_SCENE_LEGEND;
    }
    if (strcmp(value, "object_info") == 0)
    {
        return SANDBOX3D_UTILITY_OBJECT_INFO;
    }
    if (strcmp(value, "paths") == 0)
    {
        return SANDBOX3D_UTILITY_PATHS;
    }
    if (strcmp(value, "settings") == 0)
    {
        return SANDBOX3D_UTILITY_SETTINGS;
    }
    if (strcmp(value, "diagnostics") == 0)
    {
        return SANDBOX3D_UTILITY_DIAGNOSTICS;
    }
    if (strcmp(value, "transform_qa") == 0)
    {
        return SANDBOX3D_UTILITY_TRANSFORM_QA;
    }
    if (strcmp(value, "physics_qa") == 0)
    {
        return SANDBOX3D_UTILITY_PHYSICS_QA;
    }

    return SANDBOX3D_UTILITY_NONE;
}

static sandbox3d_layout_mode sandbox3d_cycle_layout_mode(sandbox3d_layout_mode layout_mode)
{
    switch (layout_mode)
    {
        case SANDBOX3D_LAYOUT_VIEW:
            return SANDBOX3D_LAYOUT_INSPECT;
        case SANDBOX3D_LAYOUT_INSPECT:
            return SANDBOX3D_LAYOUT_FULL;
        case SANDBOX3D_LAYOUT_FULL:
        default:
            return SANDBOX3D_LAYOUT_VIEW;
    }
}

static const char* sandbox3d_get_gizmo_mode_label(sandbox3d_gizmo_mode mode)
{
    switch (mode)
    {
        case SANDBOX3D_GIZMO_MODE_SELECT:
            return "Select";
        case SANDBOX3D_GIZMO_MODE_MOVE:
            return "Move";
        case SANDBOX3D_GIZMO_MODE_ROTATE:
            return "Rotate";
        case SANDBOX3D_GIZMO_MODE_SCALE:
            return "Scale";
        default:
            return "Select";
    }
}

static const char* sandbox3d_get_gizmo_axis_label(sandbox3d_gizmo_axis axis)
{
    switch (axis)
    {
        case SANDBOX3D_GIZMO_AXIS_X:
            return "X";
        case SANDBOX3D_GIZMO_AXIS_Y:
            return "Y";
        case SANDBOX3D_GIZMO_AXIS_Z:
            return "Z";
        case SANDBOX3D_GIZMO_AXIS_UNIFORM:
            return "Uniform";
        case SANDBOX3D_GIZMO_AXIS_NONE:
        default:
            return "None";
    }
}

static void sandbox3d_gizmo_init_defaults(sandbox3d_gizmo_state* gizmo)
{
    if (gizmo == NULL)
    {
        return;
    }

    gizmo->mode = SANDBOX3D_GIZMO_MODE_SELECT;
    gizmo->hover_axis = SANDBOX3D_GIZMO_AXIS_NONE;
    gizmo->hover_handle_type = SANDBOX3D_GIZMO_HANDLE_NONE;
    gizmo->active_handle_type = SANDBOX3D_GIZMO_HANDLE_NONE;
    gizmo->drag.dragging = false;
    gizmo->drag.target_entity = HENKA_INVALID_ENTITY;
    gizmo->drag.drag_start_viewport = (henka_viewport){0, 0, 0, 0};
    gizmo->drag.active_axis = (henka_gizmo_axis)SANDBOX3D_GIZMO_AXIS_NONE;
    gizmo->drag.active_mode = (henka_gizmo_mode)SANDBOX3D_GIZMO_MODE_SELECT;
    gizmo->drag.gizmo_size = 0.0f;
    gizmo->snap.enabled = true;
    gizmo->snap.move_snap_increment = 0.25f;
    gizmo->snap.rotate_snap_increment = 15.0f * HENKA_DEG_TO_RAD;
    gizmo->snap.scale_snap_increment = 0.1f;
}

static bool sandbox3d_is_finite_float(float value)
{
    return isfinite(value) != 0;
}

static bool sandbox3d_transform_is_finite(henka_transform transform)
{
    return sandbox3d_is_finite_float(transform.position.x) &&
        sandbox3d_is_finite_float(transform.position.y) &&
        sandbox3d_is_finite_float(transform.position.z) &&
        sandbox3d_is_finite_float(transform.rotation.x) &&
        sandbox3d_is_finite_float(transform.rotation.y) &&
        sandbox3d_is_finite_float(transform.rotation.z) &&
        sandbox3d_is_finite_float(transform.rotation.w) &&
        sandbox3d_is_finite_float(transform.scale.x) &&
        sandbox3d_is_finite_float(transform.scale.y) &&
        sandbox3d_is_finite_float(transform.scale.z);
}

static void sandbox3d_gizmo_set_mode(sandbox3d_state* state, sandbox3d_gizmo_mode mode)
{
    sandbox3d_viewport_tool_mode tool_mode;

    switch (mode)
    {
        case SANDBOX3D_GIZMO_MODE_MOVE:
            tool_mode = SANDBOX3D_VIEWPORT_TOOL_MOVE;
            break;
        case SANDBOX3D_GIZMO_MODE_ROTATE:
            tool_mode = SANDBOX3D_VIEWPORT_TOOL_ROTATE;
            break;
        case SANDBOX3D_GIZMO_MODE_SCALE:
            tool_mode = SANDBOX3D_VIEWPORT_TOOL_SCALE;
            break;
        case SANDBOX3D_GIZMO_MODE_SELECT:
        default:
            tool_mode = SANDBOX3D_VIEWPORT_TOOL_SELECT;
            break;
    }

    sandbox3d_set_viewport_tool_mode(state, tool_mode, true);
}

static void sandbox3d_set_viewport_tool_mode(
    sandbox3d_state* state,
    sandbox3d_viewport_tool_mode tool_mode,
    bool update_status)
{
    henka_gizmo_mode gizmo_mode;

    if (state == NULL)
    {
        return;
    }

    gizmo_mode = sandbox3d_viewport_tool_mode_to_gizmo_mode(tool_mode);
    if (state->viewport_tool != tool_mode || state->gizmo.mode != (sandbox3d_gizmo_mode)gizmo_mode)
    {
        state->viewport_tool = tool_mode;
        state->gizmo.mode = (sandbox3d_gizmo_mode)gizmo_mode;
        state->view_navigation.orbiting = false;
        state->view_navigation.panning = false;
        sandbox3d_clear_gizmo_drag(state, true);
        if (update_status)
        {
            sandbox3d_set_statusf(
                state,
                false,
                false,
                "Viewport tool: %s. Gizmo: %s.",
                sandbox3d_viewport_tool_mode_to_string(tool_mode),
                sandbox3d_get_gizmo_mode_label(state->gizmo.mode));
        }
    }
}

static void sandbox3d_gizmo_toggle_snap(sandbox3d_state* state)
{
    if (state == NULL)
    {
        return;
    }

    state->gizmo.snap.enabled = !state->gizmo.snap.enabled;
    sandbox3d_set_statusf(state, false, false, "Snapping %s", state->gizmo.snap.enabled ? "enabled" : "disabled");
}

static henka_result sandbox3d_gizmo_apply_snap_move(float value, float snap_increment, float* out_value)
{
    return henka_gizmo_snap_move(value, snap_increment, out_value);
}

static henka_result sandbox3d_gizmo_apply_snap_rotate(float angle_radians, float snap_increment, float* out_angle)
{
    return henka_gizmo_snap_rotate(angle_radians, snap_increment, out_angle);
}

static henka_result sandbox3d_gizmo_apply_snap_scale(float value, float snap_increment, float* out_value)
{
    return henka_gizmo_snap_scale(value, snap_increment, 0.01f, out_value);
}

static henka_result sandbox3d_gizmo_get_axis_direction(sandbox3d_gizmo_axis axis, henka_vec3* out_direction)
{
    return henka_gizmo_get_axis_direction((henka_gizmo_axis)axis, out_direction);
}

static henka_result sandbox3d_gizmo_get_axis_color(sandbox3d_gizmo_axis axis, henka_vec3* out_color)
{
    if (out_color == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    switch (axis)
    {
        case SANDBOX3D_GIZMO_AXIS_X:
            *out_color = (henka_vec3){1.0f, 0.0f, 0.0f};
            return HENKA_SUCCESS;
        case SANDBOX3D_GIZMO_AXIS_Y:
            *out_color = (henka_vec3){0.0f, 1.0f, 0.0f};
            return HENKA_SUCCESS;
        case SANDBOX3D_GIZMO_AXIS_Z:
            *out_color = (henka_vec3){0.0f, 0.0f, 1.0f};
            return HENKA_SUCCESS;
        case SANDBOX3D_GIZMO_AXIS_UNIFORM:
            *out_color = (henka_vec3){1.0f, 1.0f, 0.0f};
            return HENKA_SUCCESS;
        default:
            *out_color = (henka_vec3){0.75f, 0.75f, 0.75f};
            return HENKA_SUCCESS;
    }
}

static henka_result sandbox3d_entity_get_transform(henka_scene* scene, henka_entity entity, henka_transform* out_transform)
{
    if (scene == NULL || entity == HENKA_INVALID_ENTITY || out_transform == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_scene_get_entity_transform(scene, entity, out_transform);
}

static henka_vec2 sandbox3d_vec2_subtract(henka_vec2 left, henka_vec2 right)
{
    return (henka_vec2){left.x - right.x, left.y - right.y};
}

static float sandbox3d_vec2_length(henka_vec2 value)
{
    return sqrtf(value.x * value.x + value.y * value.y);
}

static float sandbox3d_vec2_dot(henka_vec2 left, henka_vec2 right)
{
    return left.x * right.x + left.y * right.y;
}

static henka_vec2 sandbox3d_vec2_normalize(henka_vec2 value)
{
    const float length = sandbox3d_vec2_length(value);

    if (length <= 0.0001f)
    {
        return (henka_vec2){0.0f, 0.0f};
    }

    return (henka_vec2){value.x / length, value.y / length};
}

static henka_result sandbox3d_entity_set_transform(henka_scene* scene, henka_entity entity, const henka_transform* transform)
{
    if (scene == NULL || entity == HENKA_INVALID_ENTITY || transform == NULL || !sandbox3d_transform_is_finite(*transform))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_scene_set_entity_transform(scene, entity, *transform);
}

static henka_result sandbox3d_entity_reset_transform(henka_scene* scene, henka_entity entity, const henka_transform* default_transform)
{
    if (scene == NULL || entity == HENKA_INVALID_ENTITY || default_transform == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return sandbox3d_entity_set_transform(scene, entity, default_transform);
}

static float sandbox3d_get_gizmo_size(const sandbox3d_state* state, henka_vec3 gizmo_position)
{
    float distance;
    float size;

    if (state == NULL)
    {
        return 1.0f;
    }

    distance = henka_vec3_length(henka_vec3_subtract(gizmo_position, state->camera.position));
    if (distance < 1.5f)
    {
        distance = 1.5f;
    }

    size = distance * 0.18f;
    if (size < 0.45f)
    {
        size = 0.45f;
    }
    if (size > 2.5f)
    {
        size = 2.5f;
    }
    return size;
}

static bool sandbox3d_try_get_mouse_framebuffer_position(henka_engine* engine, henka_vec2* out_mouse_framebuffer)
{
    henka_vec2 mouse_window;
    int framebuffer_height;
    int framebuffer_width;
    int window_height;
    int window_width;

    if (engine == NULL || out_mouse_framebuffer == NULL)
    {
        return false;
    }

    if (henka_engine_get_window_size(engine, &window_width, &window_height) != HENKA_SUCCESS ||
        henka_engine_get_framebuffer_size(engine, &framebuffer_width, &framebuffer_height) != HENKA_SUCCESS)
    {
        return false;
    }

    mouse_window = henka_input_get_mouse_position(engine);
    return henka_window_point_to_framebuffer_point(
               window_width,
               window_height,
               framebuffer_width,
               framebuffer_height,
               mouse_window,
               out_mouse_framebuffer) == HENKA_SUCCESS;
}

static bool sandbox3d_try_get_mouse_viewport_local(
    henka_engine* engine,
    henka_viewport viewport,
    henka_vec2* out_mouse_framebuffer,
    henka_vec2* out_mouse_local)
{
    henka_vec2 mouse_framebuffer;

    if (engine == NULL || out_mouse_framebuffer == NULL || out_mouse_local == NULL)
    {
        return false;
    }

    if (!sandbox3d_try_get_mouse_framebuffer_position(engine, &mouse_framebuffer) ||
        henka_viewport_window_to_local(viewport, mouse_framebuffer, out_mouse_local) != HENKA_SUCCESS)
    {
        return false;
    }

    *out_mouse_framebuffer = mouse_framebuffer;
    return true;
}

static bool sandbox3d_project_handle_point(
    const sandbox3d_state* state,
    henka_viewport viewport,
    henka_vec3 world_point,
    henka_vec2* out_screen_local)
{
    return state != NULL &&
        out_screen_local != NULL &&
        henka_camera_world_to_screen(&state->camera, viewport.width, viewport.height, world_point, out_screen_local, NULL) == HENKA_SUCCESS;
}

static bool sandbox3d_project_handle_box(
    const sandbox3d_state* state,
    henka_viewport viewport,
    henka_vec3 world_center,
    float world_half_size,
    henka_vec2* out_screen_center,
    henka_vec2* out_half_extents)
{
    henka_vec3 camera_forward;
    henka_vec3 camera_right;
    henka_vec3 camera_up;
    henka_vec2 center;
    henka_vec2 x_point;
    henka_vec2 y_point;

    if (state == NULL ||
        out_screen_center == NULL ||
        out_half_extents == NULL ||
        world_half_size <= 0.0f)
    {
        return false;
    }

    camera_forward = henka_camera_get_forward(&state->camera);
    camera_right = henka_camera_get_right(&state->camera);
    camera_up = henka_vec3_normalize(henka_vec3_cross(camera_right, camera_forward));
    if (!sandbox3d_project_handle_point(state, viewport, world_center, &center) ||
        !sandbox3d_project_handle_point(state, viewport, henka_vec3_add(world_center, henka_vec3_scale(camera_right, world_half_size)), &x_point) ||
        !sandbox3d_project_handle_point(state, viewport, henka_vec3_add(world_center, henka_vec3_scale(camera_up, world_half_size)), &y_point))
    {
        return false;
    }

    *out_screen_center = center;
    out_half_extents->x = fmaxf(6.0f, fabsf(x_point.x - center.x));
    out_half_extents->y = fmaxf(6.0f, fabsf(y_point.y - center.y));
    return true;
}

static void sandbox3d_append_gizmo_handle(
    sandbox3d_gizmo_model* model,
    sandbox3d_gizmo_handle_model handle)
{
    if (model == NULL || model->handle_count >= SANDBOX3D_GIZMO_MAX_HANDLES)
    {
        return;
    }

    model->handles[model->handle_count++] = handle;
}

static bool sandbox3d_try_build_gizmo_model(henka_engine* engine, const sandbox3d_state* state, henka_gizmo_model* out_model)
{
    henka_vec2 mouse_framebuffer;
    henka_viewport viewport;
    henka_entity selected_entity;
    henka_transform selected_transform;

    if (engine == NULL || state == NULL || out_model == NULL)
    {
        return false;
    }

    if (henka_engine_get_scene_viewport(engine, &viewport) != HENKA_SUCCESS ||
        !henka_viewport_is_valid(viewport) ||
        !sandbox3d_try_get_mouse_framebuffer_position(engine, &mouse_framebuffer))
    {
        return false;
    }

    selected_entity = sandbox3d_get_real_selected_entity(state);
    if (selected_entity == HENKA_INVALID_ENTITY ||
        !henka_scene_is_entity_visible(state->scene, selected_entity) ||
        sandbox3d_entity_get_transform(state->scene, selected_entity, &selected_transform) != HENKA_SUCCESS)
    {
        return false;
    }

    return henka_gizmo_build_model(
               &state->camera,
               viewport,
               selected_entity,
               selected_transform,
               (henka_gizmo_mode)state->gizmo.mode,
               mouse_framebuffer,
               sandbox3d_get_gizmo_size(state, selected_transform.position),
               (henka_gizmo_model*)out_model) == HENKA_SUCCESS;
}

static bool sandbox3d_gizmo_hit_test_model(
    const henka_gizmo_model* model,
    sandbox3d_gizmo_axis* out_axis,
    sandbox3d_gizmo_handle_type* out_type,
    bool* out_in_dead_zone)
{
    henka_gizmo_handle_hit hit;

    if (out_axis == NULL || out_type == NULL || out_in_dead_zone == NULL)
    {
        return false;
    }

    *out_axis = SANDBOX3D_GIZMO_AXIS_NONE;
    *out_type = SANDBOX3D_GIZMO_HANDLE_NONE;
    *out_in_dead_zone = false;
    if (model == NULL || !model->valid)
    {
        return false;
    }

    memset(&hit, 0, sizeof(hit));
    if (henka_gizmo_hit_test_model((const henka_gizmo_model*)model, &hit) != HENKA_SUCCESS)
    {
        return false;
    }

    *out_in_dead_zone = hit.in_dead_zone;
    if (!hit.hit)
    {
        return false;
    }

    *out_axis = (sandbox3d_gizmo_axis)hit.axis;
    *out_type = (sandbox3d_gizmo_handle_type)hit.type;
    return true;
}

static henka_vec2 sandbox3d_viewport_local_to_framebuffer_point(henka_viewport viewport, henka_vec2 point)
{
    return (henka_vec2){(float)viewport.x + point.x, (float)viewport.y + point.y};
}

static henka_ui_rect sandbox3d_viewport_local_box_to_framebuffer_rect(
    henka_viewport viewport,
    henka_vec2 center,
    henka_vec2 half_extents)
{
    return (henka_ui_rect)
    {
        (float)viewport.x + center.x - half_extents.x,
        (float)viewport.y + center.y - half_extents.y,
        half_extents.x * 2.0f,
        half_extents.y * 2.0f
    };
}

static henka_quat sandbox3d_get_axis_rotation(sandbox3d_gizmo_axis axis)
{
    switch (axis)
    {
        case SANDBOX3D_GIZMO_AXIS_Y:
            return henka_quat_from_axis_angle((henka_vec3){0.0f, 0.0f, 1.0f}, HENKA_PI * 0.5f);
        case SANDBOX3D_GIZMO_AXIS_Z:
            return henka_quat_from_axis_angle((henka_vec3){0.0f, 1.0f, 0.0f}, -HENKA_PI * 0.5f);
        case SANDBOX3D_GIZMO_AXIS_X:
        default:
            return henka_quat_identity();
    }
}

static henka_quat sandbox3d_get_ring_rotation(sandbox3d_gizmo_axis axis)
{
    switch (axis)
    {
        case SANDBOX3D_GIZMO_AXIS_X:
            return henka_quat_from_axis_angle((henka_vec3){0.0f, 1.0f, 0.0f}, HENKA_PI * 0.5f);
        case SANDBOX3D_GIZMO_AXIS_Y:
            return henka_quat_from_axis_angle((henka_vec3){1.0f, 0.0f, 0.0f}, HENKA_PI * 0.5f);
        case SANDBOX3D_GIZMO_AXIS_Z:
        default:
            return henka_quat_identity();
    }
}

static henka_vec4 sandbox3d_get_gizmo_color(
    sandbox3d_gizmo_axis axis,
    sandbox3d_gizmo_axis hover_axis,
    sandbox3d_gizmo_axis active_axis)
{
    henka_vec3 axis_color;
    henka_vec4 color;

    color = (henka_vec4){0.78f, 0.78f, 0.78f, 1.0f};
    if (sandbox3d_gizmo_get_axis_color(axis, &axis_color) == HENKA_SUCCESS)
    {
        color = (henka_vec4){axis_color.x, axis_color.y, axis_color.z, 1.0f};
    }

    if (axis == hover_axis || axis == active_axis)
    {
        color.x = fminf(color.x + 0.35f, 1.0f);
        color.y = fminf(color.y + 0.35f, 1.0f);
        color.z = fminf(color.z + 0.35f, 1.0f);
    }

    return color;
}

static void sandbox3d_hide_gizmo_helpers(sandbox3d_state* state)
{
    size_t index;

    if (state == NULL || state->scene == NULL)
    {
        return;
    }

    for (index = 0U; index < 3U; ++index)
    {
        henka_scene_set_entity_visible(state->scene, state->gizmo_render.axis_entities[index], false);
        henka_scene_set_entity_visible(state->scene, state->gizmo_render.axis_handle_entities[index], false);
        henka_scene_set_entity_visible(state->scene, state->gizmo_render.ring_entities[index], false);
    }

    henka_scene_set_entity_visible(state->scene, state->gizmo_render.center_entity, false);
}

static void sandbox3d_draw_gizmo_overlay(henka_engine* engine, sandbox3d_state* state, henka_viewport viewport)
{
    henka_gizmo_model model;
    henka_gizmo_overlay_model overlay;
    bool in_dead_zone;
    sandbox3d_gizmo_axis active_axis;
    sandbox3d_gizmo_axis hover_axis;
    sandbox3d_gizmo_axis model_hover_axis;
    sandbox3d_gizmo_handle_type model_hover_type;
    size_t index;

    if (engine == NULL || state == NULL || state->ui == NULL || !henka_viewport_is_valid(viewport))
    {
        return;
    }

    if (state->gizmo.mode == SANDBOX3D_GIZMO_MODE_SELECT ||
        !sandbox3d_collect_gizmo_overlay_state(engine, state, &model, &overlay, &model_hover_axis, &model_hover_type, &in_dead_zone))
    {
        return;
    }

    active_axis = state->gizmo.drag.dragging ? (sandbox3d_gizmo_axis)state->gizmo.drag.active_axis : SANDBOX3D_GIZMO_AXIS_NONE;
    hover_axis = state->gizmo.hover_axis != SANDBOX3D_GIZMO_AXIS_NONE ? state->gizmo.hover_axis : model_hover_axis;
    for (index = 0U; index < overlay.primitive_count; ++index)
    {
        const henka_gizmo_overlay_primitive* primitive = &overlay.primitives[index];
        const sandbox3d_gizmo_axis primitive_axis = (sandbox3d_gizmo_axis)primitive->axis;
        const henka_vec4 color = sandbox3d_get_gizmo_color(primitive_axis, hover_axis, active_axis);
        const float thickness = primitive_axis == active_axis ? 5.0f : (primitive_axis == hover_axis ? 4.0f : 3.0f);
        const bool debug_overlay = state->diagnostics.show_handle_hit_boxes;

        if (!primitive->visible)
        {
            continue;
        }

        switch (primitive->type)
        {
            case HENKA_GIZMO_OVERLAY_PRIMITIVE_LINE:
                henka_ui_overlay_line(
                    state->ui,
                    sandbox3d_viewport_local_to_framebuffer_point(viewport, primitive->start),
                    sandbox3d_viewport_local_to_framebuffer_point(viewport, primitive->end),
                    thickness,
                    color);
                break;

            case HENKA_GIZMO_OVERLAY_PRIMITIVE_RECT:
            {
                const henka_ui_rect rect = sandbox3d_viewport_local_box_to_framebuffer_rect(
                    viewport,
                    primitive->center,
                    primitive->half_extents);
                const henka_vec4 fill = (henka_vec4){color.x, color.y, color.z, primitive_axis == active_axis ? 0.95f : 0.82f};
                const henka_vec4 border = debug_overlay
                    ? (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f}
                    : (henka_vec4){0.98f, 0.98f, 0.99f, 0.95f};
                const henka_vec2 top_left = (henka_vec2){rect.x, rect.y};
                const henka_vec2 top_right = (henka_vec2){rect.x + rect.width, rect.y};
                const henka_vec2 bottom_left = (henka_vec2){rect.x, rect.y + rect.height};
                const henka_vec2 bottom_right = (henka_vec2){rect.x + rect.width, rect.y + rect.height};

                henka_ui_overlay_rect(state->ui, rect, fill);
                henka_ui_overlay_line(state->ui, top_left, top_right, 2.0f, border);
                henka_ui_overlay_line(state->ui, top_right, bottom_right, 2.0f, border);
                henka_ui_overlay_line(state->ui, bottom_right, bottom_left, 2.0f, border);
                henka_ui_overlay_line(state->ui, bottom_left, top_left, 2.0f, border);
                break;
            }

            case HENKA_GIZMO_OVERLAY_PRIMITIVE_POLYLINE:
            {
                henka_vec2 framebuffer_points[HENKA_GIZMO_RING_SAMPLES];
                size_t point_index;

                for (point_index = 0U; point_index < primitive->point_count && point_index < HENKA_GIZMO_RING_SAMPLES; ++point_index)
                {
                    framebuffer_points[point_index] = sandbox3d_viewport_local_to_framebuffer_point(viewport, primitive->points[point_index]);
                }

                henka_ui_overlay_polyline(state->ui, framebuffer_points, primitive->point_count, thickness, color);
                if (debug_overlay)
                {
                    henka_ui_overlay_polyline(
                        state->ui,
                        framebuffer_points,
                        primitive->point_count,
                        thickness + model.handles[index].hit_tolerance * 0.18f,
                        (henka_vec4){1.0f, 1.0f, 1.0f, 0.16f});
                }
                break;
            }

            case HENKA_GIZMO_OVERLAY_PRIMITIVE_NONE:
            default:
                break;
        }

        if (debug_overlay && primitive->type == HENKA_GIZMO_OVERLAY_PRIMITIVE_LINE)
        {
            henka_ui_overlay_line(
                state->ui,
                sandbox3d_viewport_local_to_framebuffer_point(viewport, primitive->start),
                sandbox3d_viewport_local_to_framebuffer_point(viewport, primitive->end),
                thickness + model.handles[index].hit_tolerance * 0.20f,
                (henka_vec4){1.0f, 1.0f, 1.0f, 0.14f});
        }
    }

    if (state->diagnostics.show_handle_hit_boxes)
    {
        const henka_ui_rect center_rect = sandbox3d_viewport_local_box_to_framebuffer_rect(
            viewport,
            model.screen_center,
            (henka_vec2){4.0f, 4.0f});
        henka_ui_overlay_rect(state->ui, center_rect, in_dead_zone ? (henka_vec4){1.0f, 0.82f, 0.12f, 0.85f} : (henka_vec4){1.0f, 1.0f, 1.0f, 0.78f});
    }
}

static void sandbox3d_physics_overlay_line(
    const sandbox3d_state* state,
    henka_viewport viewport,
    henka_vec3 start,
    henka_vec3 end,
    float thickness,
    henka_vec4 color)
{
    henka_vec2 screen_start;
    henka_vec2 screen_end;
    if (sandbox3d_project_handle_point(state, viewport, start, &screen_start) &&
        sandbox3d_project_handle_point(state, viewport, end, &screen_end))
    {
        (void)henka_ui_overlay_line(
            state->ui,
            sandbox3d_viewport_local_to_framebuffer_point(viewport, screen_start),
            sandbox3d_viewport_local_to_framebuffer_point(viewport, screen_end),
            thickness,
            color);
    }
}

static void sandbox3d_draw_physics_overlay(sandbox3d_state* state, henka_viewport viewport)
{
    static const int box_edges[12][2] = {
        {0, 1}, {1, 3}, {3, 2}, {2, 0}, {4, 5}, {5, 7}, {7, 6}, {6, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    size_t count;
    size_t index;
    if (state == NULL || state->ui == NULL || state->physics.world == NULL ||
        !state->physics.debug_colliders || !henka_viewport_is_valid(viewport))
    {
        return;
    }
    count = henka_physics_world_get_debug_shape_count(state->physics.world);
    for (index = 0U; index < count; ++index)
    {
        henka_physics_debug_shape shape;
        henka_vec4 color;
        henka_vec3 center;
        if (henka_physics_world_get_debug_shape(state->physics.world, index, &shape) != HENKA_SUCCESS)
        {
            continue;
        }
        color = shape.collider.is_trigger
            ? (henka_vec4){1.0f, 0.63f, 0.15f, 0.95f}
            : (shape.colliding ? (henka_vec4){0.25f, 1.0f, 0.48f, 0.95f} : (henka_vec4){0.22f, 0.88f, 1.0f, 0.85f});
        center = henka_vec3_add(shape.transform.position, shape.collider.offset);
        if (shape.collider.shape == HENKA_PHYSICS_SHAPE_BOX)
        {
            henka_vec3 extent = {
                shape.collider.data.box.half_extents.x * fabsf(shape.transform.scale.x),
                shape.collider.data.box.half_extents.y * fabsf(shape.transform.scale.y),
                shape.collider.data.box.half_extents.z * fabsf(shape.transform.scale.z)};
            henka_vec3 points[8];
            int edge;
            int point;
            for (point = 0; point < 8; ++point)
            {
                points[point] = henka_vec3_add(
                    center,
                    (henka_vec3){
                        (point & 1) ? extent.x : -extent.x,
                        (point & 2) ? extent.y : -extent.y,
                        (point & 4) ? extent.z : -extent.z});
            }
            for (edge = 0; edge < 12; ++edge)
            {
                sandbox3d_physics_overlay_line(state, viewport, points[box_edges[edge][0]], points[box_edges[edge][1]], 2.0f, color);
            }
        }
        else if (shape.collider.shape == HENKA_PHYSICS_SHAPE_SPHERE)
        {
            const int segments = 24;
            float scale = fmaxf(fabsf(shape.transform.scale.x), fmaxf(fabsf(shape.transform.scale.y), fabsf(shape.transform.scale.z)));
            float radius = shape.collider.data.sphere.radius * scale;
            int axis;
            int segment;
            for (axis = 0; axis < 3; ++axis)
            {
                for (segment = 0; segment < segments; ++segment)
                {
                    float first = HENKA_PI * 2.0f * (float)segment / (float)segments;
                    float second = HENKA_PI * 2.0f * (float)(segment + 1) / (float)segments;
                    henka_vec3 a = center;
                    henka_vec3 b = center;
                    if (axis == 0)
                    {
                        a.y += cosf(first) * radius; a.z += sinf(first) * radius;
                        b.y += cosf(second) * radius; b.z += sinf(second) * radius;
                    }
                    else if (axis == 1)
                    {
                        a.x += cosf(first) * radius; a.z += sinf(first) * radius;
                        b.x += cosf(second) * radius; b.z += sinf(second) * radius;
                    }
                    else
                    {
                        a.x += cosf(first) * radius; a.y += sinf(first) * radius;
                        b.x += cosf(second) * radius; b.y += sinf(second) * radius;
                    }
                    sandbox3d_physics_overlay_line(state, viewport, a, b, 2.0f, color);
                }
            }
        }
        else if (shape.collider.shape == HENKA_PHYSICS_SHAPE_PLANE)
        {
            float y = shape.transform.position.y + shape.collider.data.plane.offset;
            henka_vec3 corners[4] = {{-6.0f, y, -6.0f}, {6.0f, y, -6.0f}, {6.0f, y, 6.0f}, {-6.0f, y, 6.0f}};
            int corner;
            for (corner = 0; corner < 4; ++corner)
            {
                sandbox3d_physics_overlay_line(state, viewport, corners[corner], corners[(corner + 1) % 4], 1.5f, color);
            }
        }
    }
    if (state->physics.debug_contacts)
    {
        const henka_physics_contact* contacts = henka_physics_world_get_contacts(state->physics.world, &count);
        for (index = 0U; contacts != NULL && index < count; ++index)
        {
            henka_vec4 color = contacts[index].is_trigger
                ? (henka_vec4){1.0f, 0.68f, 0.18f, 1.0f}
                : (henka_vec4){1.0f, 0.22f, 0.16f, 1.0f};
            sandbox3d_physics_overlay_line(
                state,
                viewport,
                contacts[index].point,
                henka_vec3_add(contacts[index].point, henka_vec3_scale(contacts[index].normal, 0.7f)),
                3.0f,
                color);
        }
    }
}

static void sandbox3d_draw_selection_highlight(sandbox3d_state* state, henka_viewport viewport)
{
    henka_bounds bounds;
    sandbox3d_selection_highlight_model model;
    henka_vec4 outer_color;
    henka_vec4 inner_color;
    size_t edge;

    if (state == NULL || state->ui == NULL || !henka_viewport_is_valid(viewport) ||
        !sandbox3d_get_selected_bounds(state, &bounds) ||
        !sandbox3d_build_selection_highlight_model(bounds, &model))
    {
        return;
    }

    outer_color = (henka_vec4){0.02f, 0.04f, 0.06f, 0.92f};
    inner_color = (henka_vec4){1.0f, 0.86f, 0.22f, 0.98f};
    for (edge = 0U; edge < model.edge_count; ++edge)
    {
        henka_vec2 start;
        henka_vec2 end;
        if (!sandbox3d_project_handle_point(state, viewport, model.edge_starts[edge], &start) ||
            !sandbox3d_project_handle_point(state, viewport, model.edge_ends[edge], &end))
        {
            continue;
        }
        start = sandbox3d_viewport_local_to_framebuffer_point(viewport, start);
        end = sandbox3d_viewport_local_to_framebuffer_point(viewport, end);
        henka_ui_overlay_line(state->ui, start, end, 4.0f, outer_color);
        henka_ui_overlay_line(state->ui, start, end, 2.0f, inner_color);
    }
}

static henka_result sandbox3d_get_settings_path(const henka_engine* engine, char** out_path)
{
    return henka_path_resolve(henka_engine_get_user_data_base_path(engine), "sandbox3d.settings", out_path);
}

static henka_transform sandbox3d_make_transform(henka_vec3 position, henka_vec3 scale)
{
    henka_transform transform;

    transform = henka_transform_identity();
    transform.position = position;
    transform.scale = scale;
    return transform;
}

static henka_bounds sandbox3d_make_bounds(henka_vec3 center, henka_vec3 extents)
{
    henka_bounds bounds;

    bounds.center = center;
    bounds.extents = extents;
    return bounds;
}

static henka_vec3 sandbox3d_get_default_orbit_target(void)
{
    return (henka_vec3){0.0f, 0.6f, 0.0f};
}

static void sandbox3d_reset_workspace_layout(sandbox3d_state* state)
{
    if (state == NULL)
    {
        return;
    }

    state->workspace.layout_mode = SANDBOX3D_LAYOUT_VIEW;
    state->workspace.scene_objects_panel_visible = true;
    state->workspace.object_details_panel_visible = true;
    state->workspace.active_utility = SANDBOX3D_UTILITY_NONE;
    sandbox3d_workspace_model_reset(&state->workspace.model);
    state->viewport_tool = SANDBOX3D_VIEWPORT_TOOL_SELECT;
    state->gizmo.mode = SANDBOX3D_GIZMO_MODE_SELECT;
    sandbox3d_clear_gizmo_drag(state, true);
    if (state->ui != NULL)
    {
        henka_ui_set_visible(state->ui, true);
        state->ui_visibility_report_pending = true;
    }
    sandbox3d_set_status(state, false, "Layout reset. Panels are visible, redocked, and dock sizes restored.");
}

static bool sandbox3d_workspace_shows_scene_panel(const sandbox3d_state* state)
{
    return state != NULL &&
        (state->workspace.layout_mode != SANDBOX3D_LAYOUT_VIEW ||
         sandbox3d_workspace_panel_is_floating(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS)) &&
        state->workspace.scene_objects_panel_visible;
}

static bool sandbox3d_workspace_shows_details_panel(const sandbox3d_state* state)
{
    return state != NULL &&
        (state->workspace.layout_mode != SANDBOX3D_LAYOUT_VIEW ||
         sandbox3d_workspace_panel_is_floating(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS)) &&
        state->workspace.object_details_panel_visible;
}

static bool sandbox3d_workspace_shows_utility_panel(const sandbox3d_state* state)
{
    return state != NULL && state->workspace.active_utility != SANDBOX3D_UTILITY_NONE;
}

static bool sandbox3d_utility_uses_full_dock(const sandbox3d_state* state)
{
    return state != NULL &&
        !sandbox3d_workspace_panel_is_floating(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_UTILITY) &&
        (state->workspace.active_utility == SANDBOX3D_UTILITY_DIAGNOSTICS ||
         state->workspace.active_utility == SANDBOX3D_UTILITY_TRANSFORM_QA ||
         state->workspace.active_utility == SANDBOX3D_UTILITY_PHYSICS_QA);
}

static bool sandbox3d_workspace_panel_visible(
    const sandbox3d_state* state,
    sandbox3d_workspace_panel_id panel_id)
{
    switch (panel_id)
    {
        case SANDBOX3D_WORKSPACE_PANEL_CONTROLS:
            return state != NULL;
        case SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS:
            return sandbox3d_workspace_shows_scene_panel(state);
        case SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS:
            return sandbox3d_workspace_shows_details_panel(state);
        case SANDBOX3D_WORKSPACE_PANEL_UTILITY:
            return sandbox3d_workspace_shows_utility_panel(state);
        case SANDBOX3D_WORKSPACE_PANEL_NONE:
        default:
            return false;
    }
}

static void sandbox3d_set_active_utility(sandbox3d_state* state, sandbox3d_utility_view utility)
{
    if (state == NULL)
    {
        return;
    }

    state->workspace.active_utility = utility;
}

static void sandbox3d_set_status(sandbox3d_state* state, bool warning, const char* message)
{
    if (state == NULL)
    {
        return;
    }

    state->status_warning = warning;
    if (message == NULL || message[0] == '\0')
    {
        snprintf(state->status_message, sizeof(state->status_message), "Ready.");
        return;
    }

    snprintf(state->status_message, sizeof(state->status_message), "%s", message);
}

static void sandbox3d_set_statusf(sandbox3d_state* state, bool warning, bool print_console, const char* format, ...)
{
    char buffer[160];
    va_list args;

    if (format == NULL)
    {
        return;
    }

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    sandbox3d_set_status(state, warning, buffer);
    if (print_console)
    {
        printf("%s\n", buffer);
        fflush(stdout);
    }
}

static void sandbox3d_record_reject_reason(
    sandbox3d_state* state,
    sandbox3d_interaction_reject_reason reason,
    bool update_status)
{
    if (state == NULL)
    {
        return;
    }

    state->diagnostics.last_reject_reason = reason;
    if (update_status && reason != SANDBOX3D_INTERACTION_REJECT_NONE)
    {
        sandbox3d_set_statusf(state, true, false, "Viewport interaction blocked: %s.", sandbox3d_interaction_reject_reason_to_string(reason));
    }
}

static void sandbox3d_record_drag_result(sandbox3d_state* state, const char* message)
{
    if (state == NULL)
    {
        return;
    }

    if (message == NULL || message[0] == '\0')
    {
        snprintf(state->diagnostics.last_drag_result, sizeof(state->diagnostics.last_drag_result), "%s", "(none)");
        return;
    }

    snprintf(state->diagnostics.last_drag_result, sizeof(state->diagnostics.last_drag_result), "%s", message);
}

static void sandbox3d_record_action_result(
    sandbox3d_state* state,
    henka_action_command command,
    const henka_action_result* result)
{
    if (state == NULL)
    {
        return;
    }

    snprintf(
        state->diagnostics.last_action_command,
        sizeof(state->diagnostics.last_action_command),
        "%s",
        henka_action_command_to_string(command));
    if (result == NULL)
    {
        snprintf(state->diagnostics.last_action_result, sizeof(state->diagnostics.last_action_result), "%s", "No result");
        return;
    }

    snprintf(
        state->diagnostics.last_action_result,
        sizeof(state->diagnostics.last_action_result),
        "%s | %s",
        henka_action_status_to_string(result->status),
        result->message[0] != '\0' ? result->message : (result->success ? "Success" : "Failed"));
}

static void sandbox3d_record_success_result(sandbox3d_state* state, const char* format, ...)
{
    va_list args;

    if (state == NULL || format == NULL)
    {
        return;
    }

    va_start(args, format);
    vsnprintf(state->diagnostics.last_success_result, sizeof(state->diagnostics.last_success_result), format, args);
    va_end(args);
}

static void sandbox3d_truncate_text(const char* source, char* buffer, size_t buffer_size, size_t max_visible_characters)
{
    size_t length;

    if (buffer == NULL || buffer_size == 0U)
    {
        return;
    }

    if (source == NULL || source[0] == '\0')
    {
        snprintf(buffer, buffer_size, "(unavailable)");
        return;
    }

    length = strlen(source);
    if (length <= max_visible_characters || max_visible_characters < 4U)
    {
        snprintf(buffer, buffer_size, "%s", source);
        return;
    }

    snprintf(
        buffer,
        buffer_size,
        "%.*s...",
        (int)(max_visible_characters - 3U),
        source);
}

static void sandbox3d_format_display_path(const char* label, const char* path, char* buffer, size_t buffer_size)
{
    char truncated_value[80];
    const char* value;
    size_t path_length;

    if (buffer == NULL || buffer_size == 0U)
    {
        return;
    }

    value = (path != NULL && path[0] != '\0') ? path : "(unavailable)";
    path_length = strlen(value);
    if (path_length > 58U)
    {
        snprintf(truncated_value, sizeof(truncated_value), "...%s", value + path_length - 55U);
        value = truncated_value;
    }

    snprintf(buffer, buffer_size, "%s: %s", label, value);
}

static const sandbox3d_object_descriptor* sandbox3d_get_descriptor_by_entity(
    const sandbox3d_state* state,
    henka_entity entity)
{
    size_t index;

    if (state == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return NULL;
    }

    for (index = 0U; index < SANDBOX3D_OBJECT_COUNT; ++index)
    {
        if (state->descriptors[index].entity == entity)
        {
            return &state->descriptors[index];
        }
    }

    return NULL;
}

static henka_entity sandbox3d_get_real_selected_entity(const sandbox3d_state* state)
{
    henka_entity selected_entity;

    selected_entity = HENKA_INVALID_ENTITY;
    if (state != NULL && state->actions != NULL)
    {
        selected_entity = henka_action_context_get_selected_entity(state->actions);
    }
    else if (state != NULL)
    {
        selected_entity = state->selected_entity;
    }

    if (!sandbox3d_is_selectable_entity(state, selected_entity))
    {
        return HENKA_INVALID_ENTITY;
    }

    return selected_entity;
}

static const sandbox3d_object_descriptor* sandbox3d_get_selected_descriptor(const sandbox3d_state* state)
{
    return sandbox3d_get_descriptor_by_entity(state, sandbox3d_get_real_selected_entity(state));
}

static bool sandbox3d_viewports_match(henka_viewport left, henka_viewport right)
{
    return left.x == right.x &&
        left.y == right.y &&
        left.width == right.width &&
        left.height == right.height;
}

static void sandbox3d_clear_gizmo_drag(sandbox3d_state* state, bool clear_hover_axis)
{
    if (state == NULL)
    {
        return;
    }

    state->gizmo.drag.dragging = false;
    state->gizmo.drag.target_entity = HENKA_INVALID_ENTITY;
    state->gizmo.drag.drag_start_mouse_framebuffer = (henka_vec2){0.0f, 0.0f};
    state->gizmo.drag.drag_start_mouse_local = (henka_vec2){0.0f, 0.0f};
    state->gizmo.drag.active_axis = (henka_gizmo_axis)SANDBOX3D_GIZMO_AXIS_NONE;
    state->gizmo.drag.active_mode = (henka_gizmo_mode)SANDBOX3D_GIZMO_MODE_SELECT;
    state->gizmo.drag.drag_start_viewport = (henka_viewport){0, 0, 0, 0};
    state->gizmo.drag.drag_axis_screen_direction = (henka_vec2){0.0f, 0.0f};
    state->gizmo.drag.drag_center_screen = (henka_vec2){0.0f, 0.0f};
    state->gizmo.drag.drag_axis_screen_length = 0.0f;
    state->gizmo.drag.gizmo_size = 0.0f;
    state->gizmo.drag.drag_start_angle = 0.0f;
    state->gizmo.drag.drag_start_projection = 0.0f;
    state->gizmo.active_handle_type = SANDBOX3D_GIZMO_HANDLE_NONE;
    if (clear_hover_axis)
    {
        state->gizmo.hover_axis = SANDBOX3D_GIZMO_AXIS_NONE;
        state->gizmo.hover_handle_type = SANDBOX3D_GIZMO_HANDLE_NONE;
    }
}

static bool sandbox3d_is_drag_target_valid(const sandbox3d_state* state, henka_entity entity)
{
    return sandbox3d_is_selectable_entity(state, entity) &&
        state != NULL &&
        state->scene != NULL &&
        henka_scene_is_entity_visible(state->scene, entity);
}

static henka_entity sandbox3d_get_first_selectable_entity(const sandbox3d_state* state)
{
    size_t index;

    if (state == NULL)
    {
        return HENKA_INVALID_ENTITY;
    }

    for (index = 0U; index < SANDBOX3D_OBJECT_COUNT; ++index)
    {
        if (state->descriptors[index].entity != HENKA_INVALID_ENTITY)
        {
            return state->descriptors[index].entity;
        }
    }

    return HENKA_INVALID_ENTITY;
}

static bool sandbox3d_is_selectable_entity(const sandbox3d_state* state, henka_entity entity)
{
    if (state == NULL || state->scene == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return false;
    }

    if (!henka_scene_is_entity_valid(state->scene, entity) || henka_scene_is_entity_helper(state->scene, entity))
    {
        return false;
    }

    return sandbox3d_get_descriptor_by_entity(state, entity) != NULL;
}

static const char* sandbox3d_get_descriptor_role_label(const sandbox3d_object_descriptor* descriptor)
{
    if (descriptor == NULL)
    {
        return "Object";
    }

    switch (descriptor->kind)
    {
        case SANDBOX3D_OBJECT_GROUND:
            return "Ground";
        case SANDBOX3D_OBJECT_TEXTURED_CUBE:
            return "Textured";
        case SANDBOX3D_OBJECT_COLORED_CUBE:
            return "Colored";
        case SANDBOX3D_OBJECT_OBJ_MARKER:
            return "OBJ";
        case SANDBOX3D_OBJECT_MISSING_TEXTURE:
            return "Fallback";
        case SANDBOX3D_OBJECT_MISSING_MODEL:
            return "Fallback";
        case SANDBOX3D_OBJECT_DEBUG_GRID:
            return "Grid";
        default:
            return "Object";
    }
}

static const char* sandbox3d_safe_entity_name(const sandbox3d_state* state, henka_entity entity, const char* fallback_name)
{
    const char* name;

    if (state == NULL || state->scene == NULL)
    {
        return fallback_name;
    }

    name = henka_scene_get_entity_name(state->scene, entity);
    return name != NULL ? name : fallback_name;
}

static void sandbox3d_register_object_descriptor(
    sandbox3d_state* state,
    sandbox3d_object_kind kind,
    henka_entity entity,
    const char* display_name,
    const char* location_hint,
    const char* short_explanation,
    const char* developer_detail,
    const char* mesh_summary,
    const char* material_summary,
    const char* texture_summary,
    bool can_hide,
    bool can_reset,
    henka_transform default_transform)
{
    if (state == NULL || kind < 0 || kind >= SANDBOX3D_OBJECT_COUNT)
    {
        return;
    }

    state->descriptors[kind].kind = kind;
    state->descriptors[kind].entity = entity;
    state->descriptors[kind].display_name = display_name;
    state->descriptors[kind].location_hint = location_hint;
    state->descriptors[kind].short_explanation = short_explanation;
    state->descriptors[kind].developer_detail = developer_detail;
    state->descriptors[kind].mesh_summary = mesh_summary;
    state->descriptors[kind].material_summary = material_summary;
    state->descriptors[kind].texture_summary = texture_summary;
    state->descriptors[kind].can_hide = can_hide;
    state->descriptors[kind].can_reset = can_reset;
    state->descriptors[kind].default_transform = default_transform;
    if (state->actions != NULL && can_reset)
    {
        henka_action_context_register_default_transform(state->actions, entity, default_transform);
    }
}

static void sandbox3d_print_scene_legend(const sandbox3d_state* state)
{
    size_t index;

    printf("Scene examples:\n");
    for (index = 0U; index < SANDBOX3D_OBJECT_COUNT; ++index)
    {
        const sandbox3d_object_descriptor* descriptor;

        descriptor = &state->descriptors[index];
        printf(
            "  %s: %s %s\n",
            sandbox3d_safe_entity_name(state, descriptor->entity, descriptor->display_name),
            descriptor->location_hint,
            descriptor->short_explanation);
    }
}

static void sandbox3d_print_capture_state(henka_engine* engine, const char* trigger)
{
    printf("Mouse look: %s", henka_engine_is_mouse_captured(engine) ? "captured" : "released");
    if (trigger != NULL)
    {
        printf(" with %s", trigger);
    }
    printf("\n");
    fflush(stdout);
}

static void sandbox3d_print_ui_state(bool visible)
{
    printf("Sandbox panel: %s\n", visible ? "shown" : "hidden");
    fflush(stdout);
}

static void sandbox3d_print_layout_mode(const sandbox3d_state* state, bool include_hint)
{
    if (state == NULL)
    {
        return;
    }

    printf("Sandbox layout: %s\n", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
    if (include_hint)
    {
        printf("Layout help: F5 cycles View, Inspect, and Full Tools.\n");
    }
    fflush(stdout);
}

static void sandbox3d_print_startup_ui_cue(const sandbox3d_state* state)
{
    if (state == NULL)
    {
        return;
    }

    if (state->startup_panels_auto_opened)
    {
        printf("Startup UI: the docked workspace starts open so Controls and Physics QA are discoverable immediately.\n");
        printf("Startup UI: press F4 to hide or show the panels and press F5 to switch View, Inspect, or Full Tools.\n");
        printf("Startup UI: use the in-window Help, Legend, Paths, Settings, Diagnostics, Transform QA, and Physics QA utilities for inspection.\n");
        printf("Startup UI: recent actions and warnings appear in the Controls panel so normal use does not depend on the console.\n");
    }
    else
    {
        printf("Startup UI: press F4 to show or hide the docked panels and press F5 to cycle layout modes.\n");
        printf("Startup UI: use the in-window utilities for help, scene legend, paths, settings, and diagnostics.\n");
        printf("Startup UI: recent actions and warnings appear in-window while the console stays available for fallback logs.\n");
    }

    fflush(stdout);
}

static void sandbox3d_print_help(const sandbox3d_state* state)
{
    printf("Henka Engine Sandbox 3D\n");
    printf("Build: local %s %s %s\n", sandbox3d_get_build_configuration_label(), __DATE__, __TIME__);
    printf("This scene shows texture rendering, untextured material color, early OBJ loading, visible fallback behavior, and the first developer-facing scene inspection panels.\n");
    printf("Controls:\n");
    printf("  W A S D          Move across the scene\n");
    printf("  Q / E            Move down / up\n");
    printf("  Shift            Move faster\n");
    printf("  Mouse            Look around while mouse capture is active\n");
    printf("  Right Mouse / Tab Toggle mouse capture\n");
    printf("  Left Mouse       Select or manipulate inside the scene viewport when capture is released\n");
    printf("  Alt + Left Mouse Orbit around the selected object or current view target\n");
    printf("  Middle Mouse     Pan the viewport\n");
    printf("  Mouse Wheel      Zoom the viewport when the cursor is over the scene view\n");
    printf("  F1               Toggle wireframe\n");
    printf("  F2               Print the scene legend again\n");
    printf("  F3               Show or hide the debug grid\n");
    printf("  F4               Show or hide the sandbox panels\n");
    printf("  F5               Cycle View, Inspect, and Full Tools layouts\n");
    printf("  F                Frame the selected object\n");
    printf("  H                Print controls and the scene legend again\n");
    printf("  Home             Reset the camera view\n");
    printf("  Escape           Close the panels first. Then release the mouse. Then exit.\n");
    printf("Panel shortcuts:\n");
    printf("  The in-window panels open on startup; F4 hides or shows them.\n");
    printf("  View keeps the largest dedicated scene viewport. Inspect adds object panels. Full Tools shows the complete workspace.\n");
    printf("  Drag any panel header to undock and move it. Floating panels resize at the lower-right grip and redock with L, R, or Home.\n");
    printf("  Drag the narrow bars beside Scene View to resize occupied docks. Reset Layout restores safe defaults.\n");
    printf("  Open Native Panel Test from Controls to validate a separate OS-level tool window without detaching the workspace panels.\n");
    printf("  Use the panels to inspect named scene objects, clear selection, switch gizmo modes, focus the camera, reset object transforms, toggle visibility, and open in-window Help, Scene Legend, Object Info, Paths, Settings, Diagnostics, Transform QA, and Physics QA utilities.\n");
    printf("  Physics QA enables an opt-in fixed-step rigid-body demo with collider/contact debug drawing, impulses, body modes, and camera raycasts.\n");
    printf("  The Controls panel uses readable pages, and Scene Objects supports paging when the dock is tighter than the full list.\n");
    printf("  Select an object from the list or with Left Mouse in the viewport, then use Move, Rotate, or Scale in the Transform section.\n");
    printf("  Common actions also report short in-window status messages. Console output stays available for fallback logs.\n");
    printf("  Mouse look and camera movement pause while the UI is open.\n");
    sandbox3d_print_scene_legend(state);
    printf("Manual QA focus:\n");
    printf("  Confirm each scene example is visible, object selection updates the details panel, and camera focus and reset actions stay predictable.\n");
    printf("Current limitations:\n");
    printf("  OBJ loading is still early and currently limited to a small, documented subset.\n");
    printf("  OBJ material libraries, negative indices, animation, hierarchy tools, scene saving, and broader 2D or 2.5D workflows are not available yet.\n");
    printf("  The UI overlay is intentionally small and is not a full editor.\n");
    printf("  Production workspace panels remain in-window, and Scene View does not detach yet.\n");
    printf("  Rigid-body physics v1 uses sphere, axis-aligned box, and plane colliders; advanced physics features remain future work.\n");
    printf("  Sandbox settings are saved locally beside the executable in the user folder.\n");
    fflush(stdout);
}

static henka_result sandbox3d_configure_entity(
    henka_scene* scene,
    henka_entity entity,
    henka_mesh* mesh,
    henka_material material,
    henka_transform transform)
{
    henka_result result;

    result = henka_scene_set_entity_transform(scene, entity, transform);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_scene_set_entity_mesh(scene, entity, mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    return henka_scene_set_entity_material(scene, entity, material);
}

static void sandbox3d_apply_entity_foundation(
    sandbox3d_state* state,
    henka_entity entity,
    const char* tag,
    henka_bounds bounds,
    bool interactable,
    const char* prompt)
{
    henka_interaction_desc interaction;

    if (state == NULL || state->scene == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return;
    }

    henka_scene_set_entity_tag(state->scene, entity, tag);
    henka_scene_set_entity_local_bounds(state->scene, entity, bounds);

    interaction.enabled = interactable;
    interaction.max_distance = interactable ? 6.0f : 0.0f;
    interaction.prompt = prompt;
    henka_scene_set_entity_interaction(state->scene, entity, &interaction);
}

static henka_material sandbox3d_make_gizmo_material(sandbox3d_state* state, henka_vec4 color)
{
    henka_material material;

    material = henka_material_default();
    material.name = "Transform Gizmo";
    material.type = HENKA_MATERIAL_TYPE_UNLIT;
    material.shader = state->basic_shader;
    material.base_color = color;
    material.use_texture = false;
    material.use_lighting = false;
    material.depth_test = false;
    return material;
}

static henka_result sandbox3d_mark_gizmo_helper_entity(sandbox3d_state* state, henka_entity entity)
{
    henka_result result;

    if (state == NULL || state->scene == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_scene_set_entity_flags(state->scene, entity, HENKA_SCENE_ENTITY_FLAG_HELPER);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    return henka_scene_clear_entity_local_bounds(state->scene, entity);
}

static henka_physics_body_id sandbox3d_get_physics_body_for_entity(const sandbox3d_state* state, henka_entity entity)
{
    size_t index;
    if (state == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return HENKA_INVALID_PHYSICS_BODY_ID;
    }
    for (index = 0U; index < SANDBOX3D_OBJECT_COUNT; ++index)
    {
        if (state->descriptors[index].entity == entity)
        {
            return state->physics.bodies[index];
        }
    }
    return HENKA_INVALID_PHYSICS_BODY_ID;
}

static void sandbox3d_sync_physics_body_from_entity(sandbox3d_state* state, henka_entity entity)
{
    henka_physics_body_id body;
    henka_transform transform;
    if (state == NULL || state->physics.world == NULL || state->scene == NULL)
    {
        return;
    }
    body = sandbox3d_get_physics_body_for_entity(state, entity);
    if (body != HENKA_INVALID_PHYSICS_BODY_ID &&
        henka_scene_get_entity_transform(state->scene, entity, &transform) == HENKA_SUCCESS)
    {
        (void)henka_physics_body_set_transform(state->physics.world, body, transform, true);
        snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Editor transform synchronized body %u", (unsigned int)body);
    }
}

static henka_result sandbox3d_add_physics_body(
    sandbox3d_state* state,
    sandbox3d_object_kind kind,
    henka_physics_body_type type,
    henka_physics_collider_desc collider)
{
    henka_physics_body_desc desc;
    if (state == NULL || state->physics.world == NULL || kind < 0 || kind >= SANDBOX3D_OBJECT_COUNT)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(&desc, 0, sizeof(desc));
    desc.type = type;
    desc.transform = state->descriptors[kind].default_transform;
    desc.mass = 1.0f;
    desc.material = henka_physics_material_default();
    desc.collider = collider;
    desc.linked_scene = state->scene;
    desc.linked_entity = state->descriptors[kind].entity;
    return henka_physics_body_create(state->physics.world, &desc, &state->physics.bodies[kind]);
}

static henka_result sandbox3d_initialize_physics(sandbox3d_state* state)
{
    henka_physics_collider_desc trigger;
    henka_result result;
    if (state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_physics_world_create(&state->physics.world);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    state->physics.paused = true;
    state->physics.gravity_enabled = true;
    state->physics.debug_colliders = false;
    state->physics.debug_contacts = false;
    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Physics ready; use Enable Physics");
    if (sandbox3d_add_physics_body(state, SANDBOX3D_OBJECT_GROUND, HENKA_PHYSICS_BODY_STATIC, henka_physics_collider_plane((henka_vec3){0.0f, 1.0f, 0.0f}, 0.0f)) != HENKA_SUCCESS ||
        sandbox3d_add_physics_body(state, SANDBOX3D_OBJECT_TEXTURED_CUBE, HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_box((henka_vec3){0.5f, 0.5f, 0.5f})) != HENKA_SUCCESS ||
        sandbox3d_add_physics_body(state, SANDBOX3D_OBJECT_COLORED_CUBE, HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_box((henka_vec3){0.5f, 0.5f, 0.5f})) != HENKA_SUCCESS ||
        sandbox3d_add_physics_body(state, SANDBOX3D_OBJECT_OBJ_MARKER, HENKA_PHYSICS_BODY_DYNAMIC, henka_physics_collider_sphere(0.65f)) != HENKA_SUCCESS ||
        sandbox3d_add_physics_body(state, SANDBOX3D_OBJECT_MISSING_TEXTURE, HENKA_PHYSICS_BODY_STATIC, henka_physics_collider_box((henka_vec3){0.5f, 0.5f, 0.5f})) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    trigger = henka_physics_collider_box((henka_vec3){1.0f, 0.8f, 1.0f});
    trigger.is_trigger = true;
    return sandbox3d_add_physics_body(state, SANDBOX3D_OBJECT_MISSING_MODEL, HENKA_PHYSICS_BODY_STATIC, trigger);
}

static void sandbox3d_prepare_physics_demo(sandbox3d_state* state)
{
    henka_transform transform;
    if (state == NULL || state->physics.world == NULL)
    {
        return;
    }
    (void)henka_physics_world_reset(state->physics.world);
    transform = state->descriptors[SANDBOX3D_OBJECT_TEXTURED_CUBE].default_transform;
    transform.position = (henka_vec3){-1.2f, 4.5f, 0.0f};
    (void)henka_physics_body_set_transform(state->physics.world, state->physics.bodies[SANDBOX3D_OBJECT_TEXTURED_CUBE], transform, true);
    transform = state->descriptors[SANDBOX3D_OBJECT_COLORED_CUBE].default_transform;
    transform.position = (henka_vec3){0.4f, 6.0f, 0.0f};
    (void)henka_physics_body_set_transform(state->physics.world, state->physics.bodies[SANDBOX3D_OBJECT_COLORED_CUBE], transform, true);
    transform = state->descriptors[SANDBOX3D_OBJECT_OBJ_MARKER].default_transform;
    transform.position = (henka_vec3){2.4f, 5.0f, 0.0f};
    (void)henka_physics_body_set_transform(state->physics.world, state->physics.bodies[SANDBOX3D_OBJECT_OBJ_MARKER], transform, true);
    transform = state->descriptors[SANDBOX3D_OBJECT_MISSING_TEXTURE].default_transform;
    transform.position = (henka_vec3){0.4f, 0.55f, 0.0f};
    (void)henka_physics_body_set_transform(state->physics.world, state->physics.bodies[SANDBOX3D_OBJECT_MISSING_TEXTURE], transform, true);
    transform = state->descriptors[SANDBOX3D_OBJECT_MISSING_MODEL].default_transform;
    transform.position = (henka_vec3){2.4f, 1.2f, 0.0f};
    (void)henka_physics_body_set_transform(state->physics.world, state->physics.bodies[SANDBOX3D_OBJECT_MISSING_MODEL], transform, true);
    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Physics demo reset to start positions");
}

static void sandbox3d_update_physics(sandbox3d_state* state, double delta_seconds)
{
    size_t event_count;
    const henka_physics_event* events;
    if (state == NULL || state->physics.world == NULL || !state->physics.enabled || state->physics.paused)
    {
        return;
    }
    if (henka_physics_world_step(state->physics.world, (float)delta_seconds) != HENKA_SUCCESS)
    {
        state->physics.paused = true;
        sandbox3d_set_status(state, true, "Physics paused because stepping failed.");
        return;
    }
    events = henka_physics_world_get_events(state->physics.world, &event_count);
    if (events != NULL && event_count > 0U)
    {
        snprintf(
            state->physics.last_action,
            sizeof(state->physics.last_action),
            "%s: %u with %u",
            henka_physics_event_type_get_label(events[event_count - 1U].type),
            (unsigned int)events[event_count - 1U].contact.body_a,
            (unsigned int)events[event_count - 1U].contact.body_b);
    }
}

static henka_result sandbox3d_initialize_gizmo_rendering(henka_engine* engine, sandbox3d_state* state)
{
    henka_result result;
    size_t index;

    if (engine == NULL || state == NULL || state->scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_mesh_create_line(engine, (henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){1.0f, 0.0f, 0.0f}, &state->gizmo_render.axis_mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_mesh_create_circle_ring(engine, 1.0f, 48, &state->gizmo_render.ring_mesh);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    for (index = 0U; index < 3U; ++index)
    {
        state->gizmo_render.axis_entities[index] = henka_scene_create_entity(state->scene);
        state->gizmo_render.axis_handle_entities[index] = henka_scene_create_entity(state->scene);
        state->gizmo_render.ring_entities[index] = henka_scene_create_entity(state->scene);
        if (state->gizmo_render.axis_entities[index] == HENKA_INVALID_ENTITY ||
            state->gizmo_render.axis_handle_entities[index] == HENKA_INVALID_ENTITY ||
            state->gizmo_render.ring_entities[index] == HENKA_INVALID_ENTITY)
        {
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }

    state->gizmo_render.center_entity = henka_scene_create_entity(state->scene);
    if (state->gizmo_render.center_entity == HENKA_INVALID_ENTITY)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    for (index = 0U; index < 3U; ++index)
    {
        henka_material material;
        henka_transform transform;

        material = sandbox3d_make_gizmo_material(state, (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f});
        transform = henka_transform_identity();
        result = sandbox3d_configure_entity(state->scene, state->gizmo_render.axis_entities[index], state->gizmo_render.axis_mesh, material, transform);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        result = sandbox3d_mark_gizmo_helper_entity(state, state->gizmo_render.axis_entities[index]);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        result = sandbox3d_configure_entity(state->scene, state->gizmo_render.axis_handle_entities[index], state->cube_mesh, material, transform);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        result = sandbox3d_mark_gizmo_helper_entity(state, state->gizmo_render.axis_handle_entities[index]);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        result = sandbox3d_configure_entity(state->scene, state->gizmo_render.ring_entities[index], state->gizmo_render.ring_mesh, material, transform);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        result = sandbox3d_mark_gizmo_helper_entity(state, state->gizmo_render.ring_entities[index]);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        henka_scene_set_entity_visible(state->scene, state->gizmo_render.axis_entities[index], false);
        henka_scene_set_entity_visible(state->scene, state->gizmo_render.axis_handle_entities[index], false);
        henka_scene_set_entity_visible(state->scene, state->gizmo_render.ring_entities[index], false);
    }

    result = sandbox3d_configure_entity(
        state->scene,
        state->gizmo_render.center_entity,
        state->cube_mesh,
        sandbox3d_make_gizmo_material(state, (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f}),
        henka_transform_identity());
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_mark_gizmo_helper_entity(state, state->gizmo_render.center_entity);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    henka_scene_set_entity_visible(state->scene, state->gizmo_render.center_entity, false);
    return HENKA_SUCCESS;
}

static void sandbox3d_release_owned_resources(sandbox3d_state* state)
{
    if (state == NULL)
    {
        return;
    }

    henka_mesh_destroy(state->gizmo_render.axis_mesh);
    henka_mesh_destroy(state->gizmo_render.ring_mesh);
    henka_mesh_destroy(state->grid_mesh);
    henka_mesh_destroy(state->ground_mesh);
    henka_mesh_destroy(state->cube_mesh);
    henka_physics_world_destroy(state->physics.world);
    henka_scene_destroy(state->scene);
    henka_action_context_destroy(state->actions);
    henka_settings_destroy(state->settings);
    henka_ui_destroy(state->ui);
    henka_ui_destroy(state->native_panel_ui);

    state->grid_mesh = NULL;
    state->ground_mesh = NULL;
    state->cube_mesh = NULL;
    state->physics.world = NULL;
    state->gizmo_render.axis_mesh = NULL;
    state->gizmo_render.ring_mesh = NULL;
    state->marker_mesh = NULL;
    state->missing_model_mesh = NULL;
    state->scene = NULL;
    state->actions = NULL;
    state->settings = NULL;
    state->ui = NULL;
    state->native_panel_ui = NULL;
}

static bool sandbox3d_open_native_panel_test(henka_engine* engine, sandbox3d_state* state)
{
    henka_tool_window_desc desc;
    henka_result result;

    if (engine == NULL || state == NULL || state->native_panel_ui == NULL)
    {
        return false;
    }
    if (state->native_panel_window_id != HENKA_INVALID_WINDOW_ID &&
        henka_engine_get_tool_window_state(engine, state->native_panel_window_id, &(henka_tool_window_state){0}) == HENKA_SUCCESS)
    {
        sandbox3d_set_status(state, false, "Native Panel Test is already open.");
        return true;
    }

    state->native_panel_window_id = HENKA_INVALID_WINDOW_ID;
    desc.title = "Henka Native Panel Test";
    desc.width = 420;
    desc.height = 300;
    desc.minimum_width = 360;
    desc.minimum_height = 240;
    result = henka_engine_open_tool_window(engine, &desc, state->native_panel_ui, &state->native_panel_window_id);
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_set_status(state, true, "Native Panel Test could not be opened.");
        return false;
    }

    sandbox3d_set_status(state, false, "Native Panel Test opened in a separate window.");
    printf("Native Panel Test: opened (window %u).\n", (unsigned int)state->native_panel_window_id);
    fflush(stdout);
    return true;
}

static void sandbox3d_close_native_panel_test(henka_engine* engine, sandbox3d_state* state)
{
    if (engine == NULL || state == NULL || state->native_panel_window_id == HENKA_INVALID_WINDOW_ID)
    {
        return;
    }

    if (henka_engine_close_tool_window(engine, state->native_panel_window_id) == HENKA_SUCCESS)
    {
        printf("Native Panel Test: closed.\n");
        fflush(stdout);
    }
    state->native_panel_window_id = HENKA_INVALID_WINDOW_ID;
}

static void sandbox3d_build_native_panel_test_ui(henka_engine* engine, sandbox3d_state* state)
{
    char value[96];
    henka_engine_diagnostics diagnostics;
    henka_tool_window_state window_state;
    henka_ui_frame_desc frame_desc;

    if (engine == NULL || state == NULL || state->native_panel_ui == NULL ||
        state->native_panel_window_id == HENKA_INVALID_WINDOW_ID)
    {
        return;
    }

    if (henka_engine_get_tool_window_state(engine, state->native_panel_window_id, &window_state) != HENKA_SUCCESS)
    {
        state->native_panel_window_id = HENKA_INVALID_WINDOW_ID;
        sandbox3d_set_status(state, false, "Native Panel Test was closed. Open it again from Controls.");
        return;
    }

    memset(&frame_desc, 0, sizeof(frame_desc));
    frame_desc.framebuffer_width = window_state.width > 0 ? window_state.width : 420;
    frame_desc.framebuffer_height = window_state.height > 0 ? window_state.height : 300;
    if (henka_ui_begin_frame(state->native_panel_ui, &frame_desc) != HENKA_SUCCESS)
    {
        return;
    }

    henka_ui_panel(
        state->native_panel_ui,
        (henka_ui_rect){8.0f, 8.0f, (float)frame_desc.framebuffer_width - 16.0f, (float)frame_desc.framebuffer_height - 16.0f},
        "Native Panel Test");
    henka_ui_label(state->native_panel_ui, 22.0f, 50.0f, 1.0f, "Separate OS-level tool window foundation");
    snprintf(value, sizeof(value), "%u / native %u", (unsigned int)window_state.id, (unsigned int)window_state.native_window_id);
    sandbox3d_draw_value_row(state->native_panel_ui, 22.0f, 72.0f, (float)frame_desc.framebuffer_width - 44.0f, "Window ID", value);
    sandbox3d_draw_value_row(state->native_panel_ui, 22.0f, 100.0f, (float)frame_desc.framebuffer_width - 44.0f, "Focus", window_state.focused ? "Focused" : "Not focused");
    snprintf(value, sizeof(value), "%d x %d", window_state.width, window_state.height);
    sandbox3d_draw_value_row(state->native_panel_ui, 22.0f, 128.0f, (float)frame_desc.framebuffer_width - 44.0f, "Size", value);
    sandbox3d_draw_value_row(state->native_panel_ui, 22.0f, 156.0f, (float)frame_desc.framebuffer_width - 44.0f, "Last Event", window_state.last_event);
    if (henka_engine_get_diagnostics(engine, &diagnostics) == HENKA_SUCCESS)
    {
        sandbox3d_draw_value_row(
            state->native_panel_ui,
            22.0f,
            184.0f,
            (float)frame_desc.framebuffer_width - 44.0f,
            "Route",
            henka_window_event_route_to_string(diagnostics.last_window_event_route));
    }
    henka_ui_label(state->native_panel_ui, 22.0f, 222.0f, 1.0f, "Close this window safely with its OS close button.");
    henka_ui_label(state->native_panel_ui, 22.0f, 238.0f, 1.0f, "Scene View remains in the main sandbox window.");
    henka_ui_end_frame(state->native_panel_ui);
}

static void sandbox3d_reset_camera_defaults(sandbox3d_state* state)
{
    henka_camera defaults;

    if (state == NULL)
    {
        return;
    }

    defaults = state->camera;
    defaults.position = g_camera_start_position;
    defaults.yaw_radians = g_camera_start_yaw;
    defaults.pitch_radians = g_camera_start_pitch;
    defaults.movement_speed = g_default_camera_movement_speed;
    defaults.fast_movement_multiplier = 2.5f;
    henka_camera_reset(&state->camera, &defaults);
    sandbox3d_set_view_navigation_target(state, sandbox3d_get_default_orbit_target());
}

static void sandbox3d_adjust_mouse_sensitivity(sandbox3d_state* state, float delta)
{
    float next_value;

    if (state == NULL || state->settings == NULL)
    {
        return;
    }

    next_value = sandbox3d_get_mouse_sensitivity(state) + delta;
    if (next_value < 0.0005f)
    {
        next_value = 0.0005f;
    }
    if (next_value > 0.02f)
    {
        next_value = 0.02f;
    }

    henka_settings_set_float(state->settings, g_setting_key_mouse_sensitivity, next_value);
    sandbox3d_set_statusf(state, false, false, "Mouse sensitivity set to %.4f.", next_value);
}

static void sandbox3d_adjust_camera_speed(sandbox3d_state* state, float delta)
{
    float next_value;

    if (state == NULL)
    {
        return;
    }

    next_value = state->camera.movement_speed + delta;
    if (next_value < 1.0f)
    {
        next_value = 1.0f;
    }
    if (next_value > 16.0f)
    {
        next_value = 16.0f;
    }

    state->camera.movement_speed = next_value;
    sandbox3d_set_statusf(state, false, false, "Camera speed set to %.1f.", next_value);
}

static float sandbox3d_get_mouse_sensitivity(const sandbox3d_state* state)
{
    float value;

    if (state == NULL || state->settings == NULL)
    {
        return g_default_mouse_look_sensitivity;
    }

    value = henka_settings_get_float(state->settings, g_setting_key_mouse_sensitivity, g_default_mouse_look_sensitivity);
    return value > 0.0f ? value : g_default_mouse_look_sensitivity;
}

static bool sandbox3d_execute_action(
    sandbox3d_state* state,
    const henka_action_request* request,
    henka_action_result* out_result)
{
    henka_action_result result;

    if (state == NULL || state->actions == NULL || request == NULL)
    {
        return false;
    }

    memset(&result, 0, sizeof(result));
    if (henka_action_execute(state->actions, request, &result) != HENKA_SUCCESS)
    {
        sandbox3d_record_action_result(state, request->command, NULL);
        return false;
    }

    state->selected_entity = henka_action_context_get_selected_entity(state->actions);
    sandbox3d_record_action_result(state, request->command, &result);
    if (result.success && result.selected_entity != HENKA_INVALID_ENTITY)
    {
        sandbox3d_sync_physics_body_from_entity(state, result.selected_entity);
    }
    if (out_result != NULL)
    {
        *out_result = result;
    }

    return result.success;
}

static void sandbox3d_select_entity(sandbox3d_state* state, henka_entity entity)
{
    henka_action_request request;
    henka_action_result action_result;
    const sandbox3d_object_descriptor* descriptor;
    henka_entity previous_entity;

    if (state == NULL || state->scene == NULL || state->actions == NULL)
    {
        return;
    }

    previous_entity = state->selected_entity;
    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_SELECT_OBJECT;
    request.params.entity.entity = entity;
    if (sandbox3d_is_selectable_entity(state, entity) &&
        sandbox3d_execute_action(state, &request, &action_result))
    {
        state->selected_entity = action_result.selected_entity;
    }
    else
    {
        state->selected_entity = HENKA_INVALID_ENTITY;
    }

    descriptor = sandbox3d_get_selected_descriptor(state);
    if (descriptor != NULL)
    {
        if (previous_entity != state->selected_entity)
        {
            sandbox3d_clear_gizmo_drag(state, true);
            sandbox3d_sync_view_navigation_target_to_selection(state);
        }
        sandbox3d_record_success_result(state, "Selected %s", descriptor->display_name);
        sandbox3d_set_statusf(state, false, false, "Selected %s.", descriptor->display_name);
        snprintf(state->diagnostics.last_selection_action, sizeof(state->diagnostics.last_selection_action), "Selected %s", descriptor->display_name);
    }
    else if (previous_entity != state->selected_entity)
    {
        sandbox3d_clear_gizmo_drag(state, true);
        snprintf(state->diagnostics.last_selection_action, sizeof(state->diagnostics.last_selection_action), "Selection cleared");
    }
}

static void sandbox3d_clear_selection(sandbox3d_state* state, const char* reason)
{
    henka_action_request request;
    henka_action_result result;
    henka_entity previous_entity;

    if (state == NULL || state->actions == NULL)
    {
        return;
    }

    previous_entity = state->selected_entity;
    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_CLEAR_SELECTION;
    if (sandbox3d_execute_action(state, &request, &result))
    {
        state->selected_entity = HENKA_INVALID_ENTITY;
        if (previous_entity != HENKA_INVALID_ENTITY)
        {
            sandbox3d_clear_gizmo_drag(state, true);
        }
        sandbox3d_record_success_result(state, "Selection cleared");
        snprintf(
            state->diagnostics.last_selection_action,
            sizeof(state->diagnostics.last_selection_action),
            "%s",
            reason != NULL ? reason : "Selection cleared");
        sandbox3d_set_statusf(state, false, false, "%s", reason != NULL ? reason : "Selection cleared.");
    }
}

static void sandbox3d_restore_selected_object(sandbox3d_state* state)
{
    const char* selected_name;
    henka_entity selected_entity;

    if (state == NULL || state->settings == NULL || state->scene == NULL)
    {
        return;
    }

    selected_entity = HENKA_INVALID_ENTITY;
    selected_name = henka_settings_get_string(state->settings, g_setting_key_selected_object_name, "");
    if (selected_name[0] != '\0')
    {
        if (henka_scene_find_entity_by_name(state->scene, selected_name, &selected_entity) != HENKA_SUCCESS)
        {
            selected_entity = HENKA_INVALID_ENTITY;
        }
    }

    if (selected_entity == HENKA_INVALID_ENTITY)
    {
        selected_entity = sandbox3d_get_first_selectable_entity(state);
    }
    else if (!sandbox3d_is_selectable_entity(state, selected_entity))
    {
        selected_entity = sandbox3d_get_first_selectable_entity(state);
    }

    sandbox3d_select_entity(state, selected_entity);
}

static void sandbox3d_apply_loaded_settings(henka_engine* engine, sandbox3d_state* state)
{
    bool grid_visible;
    const char* layout_mode_value;
    bool wireframe_enabled;
    float movement_speed;
    henka_result result;

    if (engine == NULL || state == NULL || state->settings == NULL || state->scene == NULL)
    {
        return;
    }

    grid_visible = henka_settings_get_bool(state->settings, g_setting_key_grid_visible, true);
    wireframe_enabled = henka_settings_get_bool(state->settings, g_setting_key_wireframe_enabled, false);
    movement_speed = henka_settings_get_float(state->settings, g_setting_key_camera_speed, g_default_camera_movement_speed);

    state->camera.position.x = henka_settings_get_float(state->settings, g_setting_key_camera_position_x, g_camera_start_position.x);
    state->camera.position.y = henka_settings_get_float(state->settings, g_setting_key_camera_position_y, g_camera_start_position.y);
    state->camera.position.z = henka_settings_get_float(state->settings, g_setting_key_camera_position_z, g_camera_start_position.z);
    state->camera.yaw_radians = henka_settings_get_float(state->settings, g_setting_key_camera_yaw, g_camera_start_yaw);
    state->camera.pitch_radians = henka_settings_get_float(state->settings, g_setting_key_camera_pitch, g_camera_start_pitch);
    state->camera.movement_speed = movement_speed > 0.0f ? movement_speed : g_default_camera_movement_speed;
    state->camera.fast_movement_multiplier = 2.5f;

    result = henka_scene_set_entity_visible(state->scene, state->grid_entity, grid_visible);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_WARN("Debug grid visibility could not be restored from sandbox settings.");
    }

    result = henka_engine_set_wireframe(engine, wireframe_enabled);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_WARN("Wireframe state could not be restored from sandbox settings.");
    }

    if (henka_settings_get_float(state->settings, g_setting_key_mouse_sensitivity, g_default_mouse_look_sensitivity) <= 0.0f)
    {
        henka_settings_set_float(state->settings, g_setting_key_mouse_sensitivity, g_default_mouse_look_sensitivity);
    }

    layout_mode_value = henka_settings_get_string(state->settings, g_setting_key_layout_mode, "view");
    state->workspace.layout_mode = sandbox3d_parse_layout_mode(layout_mode_value);
    state->workspace.scene_objects_panel_visible = henka_settings_get_bool(state->settings, g_setting_key_scene_panel_visible, true);
    state->workspace.object_details_panel_visible = henka_settings_get_bool(state->settings, g_setting_key_details_panel_visible, true);
    state->workspace.active_utility = sandbox3d_parse_utility_view(
        henka_settings_get_string(state->settings, g_setting_key_active_utility, "none"));
    sandbox3d_restore_selected_object(state);
}

static henka_result sandbox3d_save_settings(henka_engine* engine, sandbox3d_state* state)
{
    const sandbox3d_object_descriptor* descriptor;
    char* settings_path;
    henka_result result;

    if (engine == NULL || state == NULL || state->settings == NULL || state->scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_settings_set_bool(state->settings, g_setting_key_grid_visible, henka_scene_is_entity_visible(state->scene, state->grid_entity));
    henka_settings_set_bool(state->settings, g_setting_key_wireframe_enabled, henka_engine_is_wireframe_enabled(engine));
    henka_settings_set_float(state->settings, g_setting_key_mouse_sensitivity, sandbox3d_get_mouse_sensitivity(state));
    henka_settings_set_float(state->settings, g_setting_key_camera_speed, state->camera.movement_speed);
    henka_settings_set_float(state->settings, g_setting_key_camera_position_x, state->camera.position.x);
    henka_settings_set_float(state->settings, g_setting_key_camera_position_y, state->camera.position.y);
    henka_settings_set_float(state->settings, g_setting_key_camera_position_z, state->camera.position.z);
    henka_settings_set_float(state->settings, g_setting_key_camera_yaw, state->camera.yaw_radians);
    henka_settings_set_float(state->settings, g_setting_key_camera_pitch, state->camera.pitch_radians);
    henka_settings_set_string(state->settings, g_setting_key_layout_mode, sandbox3d_get_layout_mode_setting_value(state->workspace.layout_mode));
    henka_settings_set_bool(state->settings, g_setting_key_scene_panel_visible, state->workspace.scene_objects_panel_visible);
    henka_settings_set_bool(state->settings, g_setting_key_details_panel_visible, state->workspace.object_details_panel_visible);
    henka_settings_set_string(state->settings, g_setting_key_active_utility, sandbox3d_get_utility_setting_value(state->workspace.active_utility));

    descriptor = sandbox3d_get_selected_descriptor(state);
    if (descriptor != NULL && descriptor->display_name != NULL)
    {
        henka_settings_set_string(state->settings, g_setting_key_selected_object_name, descriptor->display_name);
    }
    else
    {
        henka_settings_remove(state->settings, g_setting_key_selected_object_name);
    }

    result = sandbox3d_get_settings_path(engine, &settings_path);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_WARN("Sandbox settings could not be saved because the local settings path could not be resolved.");
        return result;
    }

    result = henka_settings_save_file(state->settings, settings_path);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_WARN("Sandbox settings could not be saved to '%s'.", settings_path);
    }

    henka_free(settings_path);
    return result;
}

static henka_result sandbox3d_reset_settings_object(sandbox3d_state* state)
{
    henka_result result;

    if (state == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    henka_settings_destroy(state->settings);
    state->settings = NULL;
    result = henka_settings_create(&state->settings);
    return result;
}

static henka_result sandbox3d_reset_settings(henka_engine* engine, sandbox3d_state* state)
{
    char* settings_path;
    henka_result result;

    if (engine == NULL || state == NULL || state->scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = sandbox3d_reset_settings_object(state);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    sandbox3d_reset_camera_defaults(state);
    sandbox3d_reset_workspace_layout(state);
    sandbox3d_select_entity(state, sandbox3d_get_first_selectable_entity(state));

    result = henka_scene_set_entity_visible(state->scene, state->grid_entity, true);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = henka_engine_set_wireframe(engine, false);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = sandbox3d_get_settings_path(engine, &settings_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    remove(settings_path);
    henka_free(settings_path);
    return sandbox3d_save_settings(engine, state);
}

static bool sandbox3d_toggle_grid_visibility(sandbox3d_state* state, bool visible, bool print_status)
{
    if (state == NULL || state->scene == NULL)
    {
        return false;
    }

    if (henka_scene_set_entity_visible(state->scene, state->grid_entity, visible) != HENKA_SUCCESS)
    {
        return false;
    }

    if (print_status)
    {
        sandbox3d_set_statusf(state, false, true, "Grid %s.", visible ? "shown" : "hidden");
    }

    return true;
}

static bool sandbox3d_toggle_wireframe(henka_engine* engine, bool enabled, bool print_status)
{
    if (engine == NULL)
    {
        return false;
    }

    if (henka_engine_set_wireframe(engine, enabled) != HENKA_SUCCESS)
    {
        return false;
    }

    if (print_status)
    {
        printf("Wireframe: %s\n", enabled ? "on" : "off");
        fflush(stdout);
    }

    return true;
}

static bool sandbox3d_reset_selected_entity_transform(sandbox3d_state* state)
{
    henka_action_request request;
    henka_action_result result;
    const sandbox3d_object_descriptor* descriptor;

    descriptor = sandbox3d_get_selected_descriptor(state);
    if (state == NULL || state->scene == NULL || state->actions == NULL || descriptor == NULL || !descriptor->can_reset)
    {
        return false;
    }

    sandbox3d_clear_gizmo_drag(state, true);
    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_RESET_TRANSFORM;
    request.params.entity.entity = descriptor->entity;
    return sandbox3d_execute_action(state, &request, &result);
}

static bool sandbox3d_toggle_selected_entity_visibility(sandbox3d_state* state)
{
    henka_action_request request;
    henka_action_result result;
    bool currently_visible;
    const sandbox3d_object_descriptor* descriptor;

    descriptor = sandbox3d_get_selected_descriptor(state);
    if (state == NULL || state->scene == NULL || state->actions == NULL || descriptor == NULL || !descriptor->can_hide)
    {
        return false;
    }

    currently_visible = henka_scene_is_entity_visible(state->scene, descriptor->entity);
    memset(&request, 0, sizeof(request));
    request.command = currently_visible ? HENKA_ACTION_COMMAND_HIDE_OBJECT : HENKA_ACTION_COMMAND_SHOW_OBJECT;
    request.params.entity.entity = descriptor->entity;
    if (!sandbox3d_execute_action(state, &request, &result))
    {
        return false;
    }

    if (currently_visible)
    {
        sandbox3d_clear_gizmo_drag(state, true);
    }
    return true;
}

static bool sandbox3d_focus_camera_on_selected(sandbox3d_state* state)
{
    henka_action_request request;
    henka_action_result result;
    henka_bounds bounds;
    const sandbox3d_object_descriptor* descriptor;

    if (state == NULL || state->scene == NULL || state->actions == NULL)
    {
        return false;
    }

    descriptor = sandbox3d_get_selected_descriptor(state);
    if (descriptor == NULL)
    {
        return false;
    }

    memset(&request, 0, sizeof(request));
    request.command = HENKA_ACTION_COMMAND_FOCUS_CAMERA_ON_OBJECT;
    request.params.entity.entity = descriptor->entity;
    if (!sandbox3d_execute_action(state, &request, &result))
    {
        return false;
    }

    if (sandbox3d_get_selected_bounds(state, &bounds))
    {
        sandbox3d_set_view_navigation_target(state, bounds.center);
    }

    return true;
}

static bool sandbox3d_get_selected_bounds(const sandbox3d_state* state, henka_bounds* out_bounds)
{
    const sandbox3d_object_descriptor* descriptor;

    if (state == NULL || state->scene == NULL || out_bounds == NULL)
    {
        return false;
    }

    descriptor = sandbox3d_get_selected_descriptor(state);
    return descriptor != NULL &&
        henka_scene_get_entity_world_bounds(state->scene, descriptor->entity, out_bounds) == HENKA_SUCCESS;
}

static henka_vec3 sandbox3d_get_view_navigation_target(const sandbox3d_state* state)
{
    henka_bounds bounds;

    if (sandbox3d_get_selected_bounds(state, &bounds))
    {
        return bounds.center;
    }

    if (state != NULL && state->view_navigation.orbit_target_valid)
    {
        return state->view_navigation.orbit_target;
    }

    return sandbox3d_get_default_orbit_target();
}

static void sandbox3d_set_view_navigation_target(sandbox3d_state* state, henka_vec3 target)
{
    if (state == NULL)
    {
        return;
    }

    state->view_navigation.orbit_target = target;
    state->view_navigation.orbit_target_valid = true;
}

static void sandbox3d_sync_view_navigation_target_to_selection(sandbox3d_state* state)
{
    henka_bounds bounds;

    if (sandbox3d_get_selected_bounds(state, &bounds))
    {
        sandbox3d_set_view_navigation_target(state, bounds.center);
    }
}

static void sandbox3d_zoom_camera_to_target(sandbox3d_state* state, float direction_scale)
{
    henka_vec3 target;
    float distance;
    float step;

    if (state == NULL)
    {
        return;
    }

    target = sandbox3d_get_view_navigation_target(state);
    distance = henka_vec3_length(henka_vec3_subtract(target, state->camera.position));
    step = fmaxf(0.25f, distance * 0.12f) * direction_scale;
    if (henka_camera_dolly_target(&state->camera, target, step, 0.5f))
    {
        sandbox3d_set_view_navigation_target(state, target);
    }
}

static void sandbox3d_frame_selected_object(sandbox3d_state* state, bool print_status)
{
    henka_bounds bounds;

    if (state == NULL)
    {
        return;
    }

    if (!sandbox3d_get_selected_bounds(state, &bounds))
    {
        sandbox3d_set_status(state, true, "Select an object before framing the view.");
        return;
    }

    if (henka_camera_frame_bounds(&state->camera, bounds, g_camera_start_yaw, -0.32f))
    {
        sandbox3d_set_view_navigation_target(state, bounds.center);
        if (print_status)
        {
            sandbox3d_set_statusf(state, false, false, "Framed %s.", sandbox3d_get_selected_descriptor(state)->display_name);
        }
    }
}

static bool sandbox3d_collect_gizmo_overlay_state(
    henka_engine* engine,
    sandbox3d_state* state,
    henka_gizmo_model* out_model,
    henka_gizmo_overlay_model* out_overlay,
    sandbox3d_gizmo_axis* out_hover_axis,
    sandbox3d_gizmo_handle_type* out_hover_type,
    bool* out_in_dead_zone)
{
    bool in_dead_zone;
    sandbox3d_gizmo_axis hover_axis;
    sandbox3d_gizmo_handle_type hover_type;
    henka_gizmo_overlay_model overlay;
    henka_gizmo_model model;

    if (engine == NULL || state == NULL)
    {
        return false;
    }

    memset(&model, 0, sizeof(model));
    memset(&overlay, 0, sizeof(overlay));
    hover_axis = SANDBOX3D_GIZMO_AXIS_NONE;
    hover_type = SANDBOX3D_GIZMO_HANDLE_NONE;
    in_dead_zone = false;
    if (!sandbox3d_try_build_gizmo_model(engine, state, &model) ||
        henka_gizmo_build_overlay_model(&model, &overlay) != HENKA_SUCCESS)
    {
        return false;
    }

    sandbox3d_gizmo_hit_test_model(&model, &hover_axis, &hover_type, &in_dead_zone);
    if (out_model != NULL)
    {
        *out_model = model;
    }
    if (out_overlay != NULL)
    {
        *out_overlay = overlay;
    }
    if (out_hover_axis != NULL)
    {
        *out_hover_axis = hover_axis;
    }
    if (out_hover_type != NULL)
    {
        *out_hover_type = hover_type;
    }
    if (out_in_dead_zone != NULL)
    {
        *out_in_dead_zone = in_dead_zone;
    }
    return true;
}

static bool sandbox3d_ui_owns_mouse_at_point(const sandbox3d_state* state, henka_vec2 framebuffer_mouse)
{
    henka_ui_rect panels[6];
    size_t panel_count;

    if (state == NULL || state->ui == NULL || !henka_ui_is_visible(state->ui))
    {
        return false;
    }

    panel_count = 0U;
    panels[panel_count++] = state->frame_layout.controls_panel;
    if (sandbox3d_workspace_shows_scene_panel(state))
    {
        panels[panel_count++] = state->frame_layout.scene_objects_panel;
    }
    if (sandbox3d_workspace_shows_details_panel(state))
    {
        panels[panel_count++] = state->frame_layout.object_details_panel;
    }
    if (sandbox3d_workspace_shows_utility_panel(state))
    {
        panels[panel_count++] = state->frame_layout.utility_panel;
    }
    if (state->frame_layout.left_splitter.width > 0.0f)
    {
        panels[panel_count++] = state->frame_layout.left_splitter;
    }
    if (state->frame_layout.right_splitter.width > 0.0f)
    {
        panels[panel_count++] = state->frame_layout.right_splitter;
    }

    return state->workspace.model.active_drag_panel != SANDBOX3D_WORKSPACE_PANEL_NONE ||
        state->workspace.model.resize_target != SANDBOX3D_WORKSPACE_RESIZE_NONE ||
        sandbox3d_point_is_owned_by_panels(framebuffer_mouse, panels, panel_count);
}

static bool sandbox3d_handle_workspace_input(
    henka_engine* engine,
    sandbox3d_state* state,
    henka_vec2 framebuffer_mouse,
    int framebuffer_width,
    int framebuffer_height)
{
    const bool left_down = henka_input_is_mouse_button_down(engine, HENKA_MOUSE_BUTTON_LEFT);
    const bool left_pressed = henka_input_was_mouse_button_pressed(engine, HENKA_MOUSE_BUTTON_LEFT);
    const bool left_released = henka_input_was_mouse_button_released(engine, HENKA_MOUSE_BUTTON_LEFT);
    const sandbox3d_workspace_panel* panel;
    henka_ui_rect bounds;
    sandbox3d_workspace_panel_id top_panel;
    bool top_panel_is_floating;
    unsigned int top_z;
    int index;

    if (engine == NULL || state == NULL || state->ui == NULL || !henka_ui_is_visible(state->ui))
    {
        return false;
    }

    state->workspace.model.hovered_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    top_panel = SANDBOX3D_WORKSPACE_PANEL_NONE;
    top_panel_is_floating = false;
    top_z = 0U;
    for (index = 0; index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++index)
    {
        if (!sandbox3d_workspace_panel_visible(state, (sandbox3d_workspace_panel_id)index))
        {
            continue;
        }
        panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, (sandbox3d_workspace_panel_id)index);
        bounds = sandbox3d_get_panel_rect(&state->frame_layout, (sandbox3d_workspace_panel_id)index);
        if (panel != NULL && bounds.width > 0.0f && henka_ui_rect_contains(bounds, framebuffer_mouse) &&
            ((panel->dock == SANDBOX3D_WORKSPACE_DOCK_FLOATING &&
              (!top_panel_is_floating || panel->z_order >= top_z)) ||
             (panel->dock != SANDBOX3D_WORKSPACE_DOCK_FLOATING && !top_panel_is_floating)))
        {
            top_panel = (sandbox3d_workspace_panel_id)index;
            top_panel_is_floating = panel->dock == SANDBOX3D_WORKSPACE_DOCK_FLOATING;
            top_z = top_panel_is_floating ? panel->z_order : top_z;
        }
    }
    state->workspace.model.hovered_panel = top_panel;

    if (state->workspace.model.active_drag_panel != SANDBOX3D_WORKSPACE_PANEL_NONE)
    {
        const henka_ui_rect left_target = sandbox3d_get_workspace_dock_target_rect(
            state,
            &state->frame_layout,
            SANDBOX3D_WORKSPACE_DOCK_LEFT,
            framebuffer_width,
            framebuffer_height);
        const henka_ui_rect right_target = sandbox3d_get_workspace_dock_target_rect(
            state,
            &state->frame_layout,
            SANDBOX3D_WORKSPACE_DOCK_RIGHT,
            framebuffer_width,
            framebuffer_height);
        if (left_down)
        {
            sandbox3d_workspace_update_panel_drag(
                &state->workspace.model,
                framebuffer_mouse,
                framebuffer_width,
                framebuffer_height);
            state->workspace.model.active_dock_target = SANDBOX3D_WORKSPACE_DOCK_FLOATING;
            if (henka_ui_rect_contains(left_target, framebuffer_mouse) &&
                sandbox3d_workspace_can_dock_panel(state, state->workspace.model.active_drag_panel, SANDBOX3D_WORKSPACE_DOCK_LEFT))
            {
                state->workspace.model.active_dock_target = SANDBOX3D_WORKSPACE_DOCK_LEFT;
            }
            else if (henka_ui_rect_contains(right_target, framebuffer_mouse) &&
                sandbox3d_workspace_can_dock_panel(state, state->workspace.model.active_drag_panel, SANDBOX3D_WORKSPACE_DOCK_RIGHT))
            {
                state->workspace.model.active_dock_target = SANDBOX3D_WORKSPACE_DOCK_RIGHT;
            }
        }
        if (left_released)
        {
            const sandbox3d_workspace_panel_id active_panel = state->workspace.model.active_drag_panel;
            const sandbox3d_workspace_dock_zone target = state->workspace.model.active_dock_target;
            if (target != SANDBOX3D_WORKSPACE_DOCK_FLOATING)
            {
                sandbox3d_dock_workspace_panel(state, active_panel, target);
            }
            else
            {
                snprintf(state->workspace.model.last_action, sizeof(state->workspace.model.last_action), "%s moved", sandbox3d_workspace_panel_name(active_panel));
                sandbox3d_set_statusf(state, false, false, "%s moved.", sandbox3d_workspace_panel_name(active_panel));
            }
            sandbox3d_workspace_end_interaction(&state->workspace.model);
        }
        return true;
    }

    if (state->workspace.model.resize_target != SANDBOX3D_WORKSPACE_RESIZE_NONE)
    {
        if (left_down)
        {
            if (state->workspace.model.resize_target == SANDBOX3D_WORKSPACE_RESIZE_FLOATING_PANEL)
            {
                sandbox3d_workspace_update_panel_resize(
                    &state->workspace.model,
                    framebuffer_mouse,
                    framebuffer_width,
                    framebuffer_height);
            }
            else
            {
                const float other_width = state->workspace.model.resize_target == SANDBOX3D_WORKSPACE_RESIZE_LEFT_DOCK
                    ? state->frame_layout.right_dock.width
                    : state->frame_layout.left_dock.width;
                const float minimum_dock_width = state->workspace.model.resize_target == SANDBOX3D_WORKSPACE_RESIZE_LEFT_DOCK
                    ? 300.0f
                    : 332.0f;
                sandbox3d_workspace_update_dock_resize(
                    &state->workspace.model,
                    framebuffer_mouse,
                    framebuffer_width,
                    520.0f,
                    minimum_dock_width,
                    other_width);
            }
        }
        if (left_released)
        {
            snprintf(state->workspace.model.last_action, sizeof(state->workspace.model.last_action), "Resize completed");
            sandbox3d_set_status(state, false, "Workspace resize completed.");
            sandbox3d_workspace_end_interaction(&state->workspace.model);
        }
        return true;
    }

    if (!left_pressed)
    {
        return false;
    }

    if (state->frame_layout.left_splitter.width > 0.0f &&
        henka_ui_rect_contains(state->frame_layout.left_splitter, framebuffer_mouse))
    {
        sandbox3d_workspace_begin_dock_resize(&state->workspace.model, SANDBOX3D_WORKSPACE_RESIZE_LEFT_DOCK, framebuffer_mouse);
        sandbox3d_clear_gizmo_drag(state, true);
        return true;
    }
    if (state->frame_layout.right_splitter.width > 0.0f &&
        henka_ui_rect_contains(state->frame_layout.right_splitter, framebuffer_mouse))
    {
        sandbox3d_workspace_begin_dock_resize(&state->workspace.model, SANDBOX3D_WORKSPACE_RESIZE_RIGHT_DOCK, framebuffer_mouse);
        sandbox3d_clear_gizmo_drag(state, true);
        return true;
    }

    if (top_panel != SANDBOX3D_WORKSPACE_PANEL_NONE &&
        sandbox3d_workspace_panel_is_floating(&state->workspace.model, top_panel))
    {
        bounds = sandbox3d_get_panel_rect(&state->frame_layout, top_panel);
        sandbox3d_workspace_bring_to_front(&state->workspace.model, top_panel);
        if (henka_ui_rect_contains(sandbox3d_workspace_resize_rect(bounds), framebuffer_mouse))
        {
            sandbox3d_workspace_begin_panel_resize(&state->workspace.model, top_panel, framebuffer_mouse);
            sandbox3d_clear_gizmo_drag(state, true);
            return true;
        }
        if (henka_ui_rect_contains(sandbox3d_workspace_title_drag_rect(bounds), framebuffer_mouse))
        {
            sandbox3d_workspace_begin_panel_drag(&state->workspace.model, top_panel, framebuffer_mouse);
            sandbox3d_clear_gizmo_drag(state, true);
            return true;
        }
    }
    else if (top_panel != SANDBOX3D_WORKSPACE_PANEL_NONE)
    {
        bounds = sandbox3d_get_panel_rect(&state->frame_layout, top_panel);
        if (henka_ui_rect_contains(sandbox3d_workspace_docked_title_drag_rect(bounds), framebuffer_mouse))
        {
            sandbox3d_workspace_begin_docked_panel_drag(
                &state->workspace.model,
                top_panel,
                bounds,
                framebuffer_mouse,
                framebuffer_width,
                framebuffer_height);
            sandbox3d_clear_gizmo_drag(state, true);
            sandbox3d_set_statusf(state, false, false, "%s undocked. Drag to place or redock.", sandbox3d_workspace_panel_name(top_panel));
            return true;
        }
    }

    return false;
}

static void sandbox3d_refresh_interaction_diagnostics(henka_engine* engine, sandbox3d_state* state)
{
    henka_bounds bounds;
    henka_gizmo_overlay_model overlay;
    henka_gizmo_model model;
    henka_vec2 framebuffer_mouse;
    henka_vec2 viewport_local;
    henka_ui_rect hovered_panel_bounds;
    const sandbox3d_workspace_panel* hovered_panel;
    sandbox3d_gizmo_axis hover_axis;
    sandbox3d_gizmo_handle_type hover_type;
    bool in_dead_zone;
    henka_entity selected_entity;
    henka_viewport viewport;

    if (engine == NULL || state == NULL)
    {
        return;
    }

    viewport = state->frame_layout.scene_viewport;
    state->diagnostics.window_mouse = henka_input_get_mouse_position(engine);
    state->diagnostics.viewport_tool = state->viewport_tool;
    state->diagnostics.selected_entity = sandbox3d_get_real_selected_entity(state);
    state->diagnostics.drag_target_entity = state->gizmo.drag.target_entity;
    state->diagnostics.dragging = state->gizmo.drag.dragging;
    state->diagnostics.hovered_panel = state->workspace.model.hovered_panel;
    state->diagnostics.cursor_in_panel_header = false;
    state->diagnostics.active_panel_drag = state->workspace.model.active_drag_panel;
    state->diagnostics.active_panel_resize = state->workspace.model.active_resize_panel;
    state->diagnostics.active_workspace_resize = state->workspace.model.resize_target;
    snprintf(
        state->diagnostics.last_workspace_action,
        sizeof(state->diagnostics.last_workspace_action),
        "%s",
        state->workspace.model.last_action);
    state->diagnostics.mouse_framebuffer_valid = sandbox3d_try_get_mouse_framebuffer_position(engine, &framebuffer_mouse);
    hovered_panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, state->diagnostics.hovered_panel);
    hovered_panel_bounds = sandbox3d_get_panel_rect(&state->frame_layout, state->diagnostics.hovered_panel);
    if (state->diagnostics.mouse_framebuffer_valid && hovered_panel != NULL)
    {
        state->diagnostics.cursor_in_panel_header = henka_ui_rect_contains(
            hovered_panel->dock == SANDBOX3D_WORKSPACE_DOCK_FLOATING
                ? sandbox3d_workspace_title_drag_rect(hovered_panel_bounds)
                : sandbox3d_workspace_docked_title_drag_rect(hovered_panel_bounds),
            framebuffer_mouse);
    }
    state->diagnostics.ui_wants_mouse =
        state->diagnostics.mouse_framebuffer_valid &&
        sandbox3d_ui_owns_mouse_at_point(state, framebuffer_mouse);
    state->diagnostics.cursor_in_viewport =
        state->diagnostics.mouse_framebuffer_valid &&
        henka_viewport_contains_point(viewport, framebuffer_mouse);
    if (state->diagnostics.mouse_framebuffer_valid)
    {
        state->diagnostics.framebuffer_mouse = framebuffer_mouse;
    }
    state->diagnostics.viewport_local_valid =
        state->diagnostics.mouse_framebuffer_valid &&
        henka_viewport_window_to_local(viewport, framebuffer_mouse, &viewport_local) == HENKA_SUCCESS;
    if (state->diagnostics.viewport_local_valid)
    {
        state->diagnostics.viewport_local = viewport_local;
    }

    selected_entity = state->diagnostics.selected_entity;
    state->diagnostics.selected_entity_valid =
        state->scene != NULL &&
        selected_entity != HENKA_INVALID_ENTITY &&
        henka_scene_is_entity_valid(state->scene, selected_entity);
    state->diagnostics.selected_entity_visible =
        state->diagnostics.selected_entity_valid &&
        henka_scene_is_entity_visible(state->scene, selected_entity);
    state->diagnostics.selected_entity_selectable = sandbox3d_is_selectable_entity(state, selected_entity);
    state->diagnostics.selected_bounds_valid = sandbox3d_get_selected_bounds(state, &bounds);
    state->diagnostics.selected_highlight_active =
        state->diagnostics.selected_entity_valid &&
        state->diagnostics.selected_entity_visible &&
        state->diagnostics.selected_entity_selectable &&
        state->diagnostics.selected_bounds_valid;
    state->diagnostics.gizmo_model_valid = false;
    state->diagnostics.overlay_primitive_count = 0U;
    state->gizmo.hover_handle_type = SANDBOX3D_GIZMO_HANDLE_NONE;
    if (sandbox3d_viewport_tool_mode_uses_gizmo(state->viewport_tool) &&
        sandbox3d_collect_gizmo_overlay_state(engine, state, &model, &overlay, &hover_axis, &hover_type, &in_dead_zone))
    {
        state->diagnostics.gizmo_model_valid = model.valid;
        state->diagnostics.overlay_primitive_count = overlay.primitive_count;
        if (!state->gizmo.drag.dragging)
        {
            state->gizmo.hover_axis = hover_axis;
            state->gizmo.hover_handle_type = hover_type;
        }
    }
}

static bool sandbox3d_apply_transform_action(
    sandbox3d_state* state,
    henka_action_command command,
    henka_entity entity,
    henka_vec3 vector_value,
    henka_quat rotation_value)
{
    henka_action_request request;
    henka_action_result result;

    if (state == NULL || entity == HENKA_INVALID_ENTITY)
    {
        sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_NO_SELECTED_OBJECT, true);
        return false;
    }

    memset(&request, 0, sizeof(request));
    request.command = command;
    switch (command)
    {
        case HENKA_ACTION_COMMAND_MOVE_BY_DELTA:
            request.params.move_by_delta.entity = entity;
            request.params.move_by_delta.delta = vector_value;
            break;
        case HENKA_ACTION_COMMAND_ROTATE_BY_DELTA:
            request.params.rotate_by_delta.entity = entity;
            request.params.rotate_by_delta.delta_rotation = rotation_value;
            break;
        case HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER:
            request.params.scale_by_multiplier.entity = entity;
            request.params.scale_by_multiplier.scale_multiplier = vector_value;
            break;
        case HENKA_ACTION_COMMAND_RESET_TRANSFORM:
            request.params.entity.entity = entity;
            break;
        default:
            return false;
    }

    if (!sandbox3d_execute_action(state, &request, &result))
    {
        sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_ACTION_COMMAND_FAILED, true);
        return false;
    }

    sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_NONE, false);
    sandbox3d_record_success_result(state, "%s", result.message[0] != '\0' ? result.message : "Action succeeded");
    return true;
}

static bool sandbox3d_apply_move_step(sandbox3d_state* state, henka_gizmo_axis axis, float amount)
{
    const henka_entity entity = sandbox3d_get_real_selected_entity(state);
    return sandbox3d_apply_transform_action(
        state,
        HENKA_ACTION_COMMAND_MOVE_BY_DELTA,
        entity,
        sandbox3d_make_move_delta(axis, amount),
        henka_quat_identity());
}

static bool sandbox3d_apply_rotate_step(sandbox3d_state* state, henka_gizmo_axis axis, float radians)
{
    const henka_entity entity = sandbox3d_get_real_selected_entity(state);
    return sandbox3d_apply_transform_action(
        state,
        HENKA_ACTION_COMMAND_ROTATE_BY_DELTA,
        entity,
        (henka_vec3){0.0f, 0.0f, 0.0f},
        sandbox3d_make_rotation_delta(axis, radians));
}

static bool sandbox3d_apply_scale_step(sandbox3d_state* state, float delta_scale)
{
    const henka_entity entity = sandbox3d_get_real_selected_entity(state);
    return sandbox3d_apply_transform_action(
        state,
        HENKA_ACTION_COMMAND_SCALE_BY_MULTIPLIER,
        entity,
        sandbox3d_make_uniform_scale_multiplier(delta_scale),
        henka_quat_identity());
}

static sandbox3d_panel_scroll_target sandbox3d_get_panel_scroll_target(const sandbox3d_state* state, henka_vec2 point)
{
    if (state == NULL || state->ui == NULL || !henka_ui_is_visible(state->ui))
    {
        return SANDBOX3D_PANEL_SCROLL_NONE;
    }

    if (state->workspace.model.hovered_panel == SANDBOX3D_WORKSPACE_PANEL_CONTROLS ||
        (state->workspace.model.hovered_panel == SANDBOX3D_WORKSPACE_PANEL_NONE &&
         henka_ui_rect_contains(state->frame_layout.controls_panel, point)))
    {
        return SANDBOX3D_PANEL_SCROLL_CONTROLS;
    }
    if (state->workspace.model.hovered_panel == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS ||
        (state->workspace.model.hovered_panel == SANDBOX3D_WORKSPACE_PANEL_NONE &&
         henka_ui_rect_contains(state->frame_layout.scene_objects_panel, point)))
    {
        return SANDBOX3D_PANEL_SCROLL_SCENE_OBJECTS;
    }
    if (state->workspace.model.hovered_panel == SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS ||
        (state->workspace.model.hovered_panel == SANDBOX3D_WORKSPACE_PANEL_NONE &&
         henka_ui_rect_contains(state->frame_layout.object_details_panel, point)))
    {
        return SANDBOX3D_PANEL_SCROLL_DETAILS;
    }
    if (state->workspace.model.hovered_panel == SANDBOX3D_WORKSPACE_PANEL_UTILITY ||
        (state->workspace.model.hovered_panel == SANDBOX3D_WORKSPACE_PANEL_NONE &&
         henka_ui_rect_contains(state->frame_layout.utility_panel, point)))
    {
        return SANDBOX3D_PANEL_SCROLL_UTILITY;
    }

    return SANDBOX3D_PANEL_SCROLL_NONE;
}

static void sandbox3d_advance_panel_paging(sandbox3d_state* state, sandbox3d_panel_scroll_target target, int delta)
{
    if (state == NULL || delta == 0)
    {
        return;
    }

    switch (target)
    {
        case SANDBOX3D_PANEL_SCROLL_CONTROLS:
            state->paging.controls_page += delta;
            if (state->paging.controls_page < 0)
            {
                state->paging.controls_page = 0;
            }
            if (state->paging.controls_page > 1)
            {
                state->paging.controls_page = 1;
            }
            break;

        case SANDBOX3D_PANEL_SCROLL_SCENE_OBJECTS:
            state->paging.scene_objects_page += delta;
            if (state->paging.scene_objects_page < 0)
            {
                state->paging.scene_objects_page = 0;
            }
            break;

        case SANDBOX3D_PANEL_SCROLL_DETAILS:
        case SANDBOX3D_PANEL_SCROLL_UTILITY:
        case SANDBOX3D_PANEL_SCROLL_NONE:
        default:
            break;
    }
}

static void sandbox3d_update_gizmo_rendering(sandbox3d_state* state)
{
    sandbox3d_hide_gizmo_helpers(state);
}

static bool sandbox3d_workspace_layout_is_valid(const sandbox3d_workspace_layout* layout)
{
    return layout != NULL && henka_viewport_is_valid(layout->scene_viewport);
}

static void sandbox3d_update_gizmo_hover(henka_engine* engine, sandbox3d_state* state)
{
    sandbox3d_gizmo_axis hit_axis;
    sandbox3d_gizmo_handle_type hit_type;
    bool in_dead_zone;

    if (engine == NULL || state == NULL || state->scene == NULL || state->gizmo.drag.dragging)
    {
        return;
    }

    state->gizmo.hover_axis = SANDBOX3D_GIZMO_AXIS_NONE;
    state->gizmo.hover_handle_type = SANDBOX3D_GIZMO_HANDLE_NONE;
    memset(&state->gizmo_model, 0, sizeof(state->gizmo_model));
    if (state->gizmo.mode == SANDBOX3D_GIZMO_MODE_SELECT ||
        !sandbox3d_try_build_gizmo_model(engine, state, &state->gizmo_model))
    {
        return;
    }

    if (sandbox3d_gizmo_hit_test_model(&state->gizmo_model, &hit_axis, &hit_type, &in_dead_zone))
    {
        (void)in_dead_zone;
        state->gizmo.hover_axis = hit_axis;
        state->gizmo.hover_handle_type = hit_type;
    }
}

static void sandbox3d_try_pick_object(henka_engine* engine, sandbox3d_state* state)
{
    bool in_dead_zone;
    henka_gizmo_overlay_model overlay;
    sandbox3d_interaction_gate gate;
    sandbox3d_gizmo_axis gizmo_axis;
    sandbox3d_gizmo_handle_type handle_type;
    henka_gizmo_drag_state drag_state;
    henka_gizmo_handle_hit hit;
    henka_vec2 mouse_framebuffer;
    henka_vec2 mouse_local;
    henka_entity picked_entity;
    henka_ray ray;
    float distance;
    const sandbox3d_object_descriptor* selected_descriptor;
    henka_entity selected_entity;
    henka_viewport scene_viewport;

    if (engine == NULL || state == NULL || state->scene == NULL)
    {
        return;
    }

    memset(&gate, 0, sizeof(gate));
    if (henka_engine_get_scene_viewport(engine, &scene_viewport) != HENKA_SUCCESS ||
        !henka_viewport_is_valid(scene_viewport) ||
        !sandbox3d_try_get_mouse_viewport_local(engine, scene_viewport, &mouse_framebuffer, &mouse_local) ||
        !henka_viewport_contains_point(scene_viewport, mouse_framebuffer) ||
        henka_camera_screen_point_to_ray(
            &state->camera,
            scene_viewport.width,
            scene_viewport.height,
            mouse_local,
            &ray) != HENKA_SUCCESS)
    {
        sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_CURSOR_OUTSIDE_VIEWPORT, false);
        return;
    }

    selected_entity = sandbox3d_get_real_selected_entity(state);
    selected_descriptor = sandbox3d_get_selected_descriptor(state);
    gate.supported_mouse_button = true;
    gate.cursor_in_viewport = true;
    gate.selected_object_present = selected_entity != HENKA_INVALID_ENTITY;
    gate.selected_object_valid = gate.selected_object_present;
    gate.selected_object_visible = gate.selected_object_present && henka_scene_is_entity_visible(state->scene, selected_entity);
    gate.selected_object_selectable = gate.selected_object_present && sandbox3d_is_selectable_entity(state, selected_entity);
    gate.gizmo_mode_active = sandbox3d_viewport_tool_mode_uses_gizmo(state->viewport_tool);
    if (state->gizmo.mode != SANDBOX3D_GIZMO_MODE_SELECT &&
        selected_entity != HENKA_INVALID_ENTITY &&
        selected_descriptor != NULL &&
        sandbox3d_try_build_gizmo_model(engine, state, &state->gizmo_model))
    {
        gate.gizmo_model_valid = state->gizmo_model.valid;
        gate.overlay_has_primitives =
            henka_gizmo_build_overlay_model(&state->gizmo_model, &overlay) == HENKA_SUCCESS &&
            overlay.primitive_count > 0U;
        if (sandbox3d_gizmo_hit_test_model(&state->gizmo_model, &gizmo_axis, &handle_type, &in_dead_zone))
        {
            memset(&hit, 0, sizeof(hit));
            hit.hit = true;
            hit.axis = (henka_gizmo_axis)gizmo_axis;
            hit.type = (henka_gizmo_handle_type)handle_type;
            if (henka_gizmo_begin_drag(&state->gizmo_model, &hit, &drag_state) != HENKA_SUCCESS)
            {
                sandbox3d_record_drag_result(state, "Gizmo drag could not start");
                sandbox3d_set_status(state, true, "Gizmo drag could not start.");
                return;
            }
            state->gizmo.drag = drag_state;
            state->gizmo.drag.drag_start_viewport = scene_viewport;
            state->gizmo.hover_axis = gizmo_axis;
            state->gizmo.hover_handle_type = handle_type;
            state->gizmo.active_handle_type = handle_type;
            sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_NONE, false);
            sandbox3d_record_drag_result(state, "Dragging selected object");
            sandbox3d_record_success_result(
                state,
                "Drag started: %s on %s",
                selected_descriptor->display_name,
                sandbox3d_get_gizmo_axis_label(gizmo_axis));
            sandbox3d_set_statusf(
                state,
                false,
                false,
                "Gizmo drag started for %s on %s.",
                selected_descriptor->display_name,
                sandbox3d_get_gizmo_axis_label(gizmo_axis));
            return;
        }
        if (in_dead_zone)
        {
            sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_CURSOR_IN_GIZMO_DEAD_ZONE, true);
            sandbox3d_set_status(state, false, "Gizmo area hit. Drag a visible handle.");
            return;
        }
        gate.handle_under_cursor = false;
        gate.cursor_in_gizmo_dead_zone = false;
        sandbox3d_record_reject_reason(
            state,
            sandbox3d_evaluate_gizmo_reject_reason(state->viewport_tool, &gate),
            true);
        if (sandbox3d_viewport_tool_mode_uses_gizmo(state->viewport_tool))
        {
            return;
        }
    }
    else if (sandbox3d_viewport_tool_mode_uses_gizmo(state->viewport_tool))
    {
        sandbox3d_record_reject_reason(
            state,
            sandbox3d_evaluate_gizmo_reject_reason(state->viewport_tool, &gate),
            true);
        return;
    }

    if (henka_scene_pick_entity(state->scene, ray, &picked_entity, &distance) == HENKA_SUCCESS &&
        sandbox3d_is_selectable_entity(state, picked_entity))
    {
        sandbox3d_select_entity(state, picked_entity);
        sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_NONE, false);
        sandbox3d_record_success_result(state, "Picked %s", sandbox3d_safe_entity_name(state, picked_entity, "object"));
        sandbox3d_set_statusf(state, false, false, "Picked %s.", sandbox3d_safe_entity_name(state, picked_entity, "object"));
    }
    else
    {
        sandbox3d_clear_selection(state, "No viewport object hit. Selection cleared.");
        sandbox3d_record_drag_result(state, "No viewport object hit");
    }
}

static void sandbox3d_apply_gizmo_drag(henka_engine* engine, sandbox3d_state* state)
{
    henka_action_request request;
    henka_action_result action_result;
    henka_vec2 current_mouse_framebuffer;
    henka_viewport current_viewport;
    henka_vec2 current_mouse_local;
    henka_transform transform;
    henka_gizmo_snap_settings snap_settings;
    const char* entity_name;
    henka_entity target_entity;

    if (engine == NULL || state == NULL || state->scene == NULL || !state->gizmo.drag.dragging)
    {
        return;
    }

    if (henka_engine_get_scene_viewport(engine, &current_viewport) != HENKA_SUCCESS ||
        !henka_viewport_is_valid(current_viewport) ||
        !sandbox3d_viewports_match(current_viewport, state->gizmo.drag.drag_start_viewport))
    {
        sandbox3d_clear_gizmo_drag(state, true);
        sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_VIEWPORT_CHANGED_DURING_DRAG, false);
        sandbox3d_record_drag_result(state, "Drag cancelled: viewport changed");
        sandbox3d_set_status(state, true, "Viewport changed. Gizmo drag cancelled.");
        return;
    }

    target_entity = state->gizmo.drag.target_entity;
    if (!sandbox3d_is_drag_target_valid(state, target_entity))
    {
        sandbox3d_clear_gizmo_drag(state, true);
        sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_DRAG_TARGET_INVALID, false);
        sandbox3d_record_drag_result(state, "Drag cancelled: target invalid");
        sandbox3d_set_status(state, true, "Gizmo drag cancelled. Target is no longer valid.");
        return;
    }
    if (!sandbox3d_try_get_mouse_viewport_local(engine, current_viewport, &current_mouse_framebuffer, &current_mouse_local))
    {
        sandbox3d_interaction_reject_reason reason = SANDBOX3D_INTERACTION_REJECT_CURSOR_OUTSIDE_VIEWPORT;
        henka_vec2 framebuffer_mouse;

        if (sandbox3d_try_get_mouse_framebuffer_position(engine, &framebuffer_mouse) &&
            sandbox3d_ui_owns_mouse_at_point(state, framebuffer_mouse))
        {
            reason = SANDBOX3D_INTERACTION_REJECT_UI_OWNS_MOUSE;
        }
        sandbox3d_clear_gizmo_drag(state, true);
        sandbox3d_record_reject_reason(state, reason, false);
        sandbox3d_record_drag_result(state, "Drag cancelled: pointer left viewport");
        sandbox3d_set_statusf(
            state,
            true,
            false,
            "Gizmo drag cancelled: %s.",
            sandbox3d_interaction_reject_reason_to_string(reason));
        return;
    }

    snap_settings.enabled = state->gizmo.snap.enabled;
    snap_settings.move_snap_increment = state->gizmo.snap.move_snap_increment;
    snap_settings.rotate_snap_increment = state->gizmo.snap.rotate_snap_increment;
    snap_settings.scale_snap_increment = state->gizmo.snap.scale_snap_increment;
    snap_settings.minimum_scale = 0.01f;
    if (henka_gizmo_apply_drag_to_transform(&state->gizmo.drag, current_mouse_local, &snap_settings, &transform) != HENKA_SUCCESS)
    {
        sandbox3d_record_drag_result(state, "Drag update produced no transform");
        return;
    }

    entity_name = sandbox3d_safe_entity_name(state, target_entity, "object");
    memset(&request, 0, sizeof(request));

    switch ((sandbox3d_gizmo_mode)state->gizmo.drag.active_mode)
    {
        case SANDBOX3D_GIZMO_MODE_MOVE:
            request.command = HENKA_ACTION_COMMAND_SET_POSITION;
            request.params.set_position.entity = target_entity;
            request.params.set_position.position = transform.position;
            if (sandbox3d_execute_action(state, &request, &action_result))
            {
                sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_NONE, false);
                sandbox3d_record_drag_result(state, "Move drag updated object");
                sandbox3d_record_success_result(state, "Move drag updated %s", entity_name);
                sandbox3d_set_statusf(state, false, false, "Moving %s on %s.", entity_name, sandbox3d_get_gizmo_axis_label(state->gizmo.drag.active_axis));
            }
            else
            {
                sandbox3d_clear_gizmo_drag(state, true);
                sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_ACTION_COMMAND_FAILED, false);
                sandbox3d_record_drag_result(state, "Move drag action failed");
                sandbox3d_set_statusf(state, true, false, "%s could not be moved.", entity_name);
            }
            break;

        case SANDBOX3D_GIZMO_MODE_ROTATE:
            request.command = HENKA_ACTION_COMMAND_SET_ROTATION;
            request.params.set_rotation.entity = target_entity;
            request.params.set_rotation.rotation = transform.rotation;
            if (sandbox3d_execute_action(state, &request, &action_result))
            {
                sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_NONE, false);
                sandbox3d_record_drag_result(state, "Rotate drag updated object");
                sandbox3d_record_success_result(state, "Rotate drag updated %s", entity_name);
                sandbox3d_set_statusf(state, false, false, "Rotating %s on %s.", entity_name, sandbox3d_get_gizmo_axis_label(state->gizmo.drag.active_axis));
            }
            else
            {
                sandbox3d_clear_gizmo_drag(state, true);
                sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_ACTION_COMMAND_FAILED, false);
                sandbox3d_record_drag_result(state, "Rotate drag action failed");
                sandbox3d_set_statusf(state, true, false, "%s could not be rotated.", entity_name);
            }
            break;

        case SANDBOX3D_GIZMO_MODE_SCALE:
            request.command = HENKA_ACTION_COMMAND_SET_SCALE;
            request.params.set_scale.entity = target_entity;
            request.params.set_scale.scale = transform.scale;
            if (sandbox3d_execute_action(state, &request, &action_result))
            {
                sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_NONE, false);
                sandbox3d_record_drag_result(state, "Scale drag updated object");
                sandbox3d_record_success_result(state, "Scale drag updated %s", entity_name);
                sandbox3d_set_statusf(state, false, false, "Scaling %s uniformly.", entity_name);
            }
            else
            {
                sandbox3d_clear_gizmo_drag(state, true);
                sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_ACTION_COMMAND_FAILED, false);
                sandbox3d_record_drag_result(state, "Scale drag action failed");
                sandbox3d_set_statusf(state, true, false, "%s could not be scaled.", entity_name);
            }
            break;

        case SANDBOX3D_GIZMO_MODE_SELECT:
        default:
            break;
    }
}

static void sandbox3d_print_selected_object_info(const sandbox3d_state* state)
{
    char material_summary[128];
    bool visible;
    const sandbox3d_object_descriptor* descriptor;
    henka_interaction_desc interaction;
    henka_material material;
    henka_transform transform;

    if (state == NULL || state->scene == NULL)
    {
        return;
    }

    descriptor = sandbox3d_get_selected_descriptor(state);
    if (descriptor == NULL)
    {
        printf("No sandbox object is currently selected.\n");
        fflush(stdout);
        return;
    }

    if (henka_scene_get_entity_transform(state->scene, descriptor->entity, &transform) != HENKA_SUCCESS)
    {
        printf("Selected object info could not be read.\n");
        fflush(stdout);
        return;
    }

    visible = henka_scene_is_entity_visible(state->scene, descriptor->entity);
    if (henka_scene_get_entity_material(state->scene, descriptor->entity, &material) != HENKA_SUCCESS)
    {
        material = henka_material_default();
    }
    if (henka_material_describe(&material, material_summary, sizeof(material_summary)) != HENKA_SUCCESS)
    {
        snprintf(material_summary, sizeof(material_summary), "%s", descriptor->material_summary);
    }
    if (henka_scene_get_entity_interaction(state->scene, descriptor->entity, &interaction) != HENKA_SUCCESS)
    {
        interaction = (henka_interaction_desc){false, 0.0f, NULL};
    }

    printf("Object Info\n");
    printf("  Name: %s\n", descriptor->display_name);
    printf("  Entity: %u\n", (unsigned int)descriptor->entity);
    printf("  Tag: %s\n", henka_scene_get_entity_tag(state->scene, descriptor->entity) != NULL ? henka_scene_get_entity_tag(state->scene, descriptor->entity) : "(none)");
    printf("  Visible: %s\n", visible ? "Yes" : "No");
    printf("  Position: %.2f %.2f %.2f\n", transform.position.x, transform.position.y, transform.position.z);
    printf("  Rotation: %.2f %.2f %.2f %.2f\n", transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
    printf("  Scale: %.2f %.2f %.2f\n", transform.scale.x, transform.scale.y, transform.scale.z);
    printf("  Demonstrates: %s\n", descriptor->short_explanation);
    printf("  Detail: %s\n", descriptor->developer_detail);
    printf("  Mesh: %s\n", descriptor->mesh_summary);
    printf("  Material: %s\n", material_summary);
    printf("  Texture: %s\n", descriptor->texture_summary);
    printf("  Interaction: %s\n", interaction.enabled ? (interaction.prompt != NULL ? interaction.prompt : "Available") : "Disabled");
    fflush(stdout);
}

static void sandbox3d_reserve_debug_strip(sandbox3d_workspace_layout* layout)
{
    const float gap = 6.0f;

    if (layout == NULL || !henka_viewport_is_valid(layout->scene_viewport))
    {
        return;
    }

    if (layout->scene_viewport.height > (int)(g_ui_debug_strip_height + gap + 80.0f))
    {
        layout->scene_viewport.height -= (int)(g_ui_debug_strip_height + gap);
    }
    layout->debug_strip = (henka_ui_rect)
    {
        (float)layout->scene_viewport.x,
        (float)(layout->scene_viewport.y + layout->scene_viewport.height) + gap,
        (float)layout->scene_viewport.width,
        g_ui_debug_strip_height
    };
}

static sandbox3d_workspace_layout sandbox3d_get_workspace_layout(
    const sandbox3d_state* state,
    int framebuffer_width,
    int framebuffer_height)
{
    float controls_height;
    float details_height;
    henka_result layout_result;
    henka_workspace_desc workspace_desc;
    henka_workspace_layout docked_layout;
    sandbox3d_workspace_layout layout;
    sandbox3d_layout_mode layout_mode;
    bool controls_left;
    bool controls_right;
    bool details_visible;
    bool details_left;
    bool details_right;
    bool left_visible;
    const sandbox3d_workspace_panel* panel;
    bool right_visible;
    bool scene_left;
    bool scene_right;
    bool scene_visible;
    bool utility_left;
    bool utility_right;
    bool utility_visible;

    memset(&layout, 0, sizeof(layout));
    layout.controls_panel = (henka_ui_rect){g_ui_panel_margin, g_ui_panel_margin, g_ui_controls_width, g_ui_panel_height};
    layout.scene_objects_panel = (henka_ui_rect){g_ui_panel_margin, g_ui_panel_margin, g_ui_scene_width, g_ui_panel_height};
    layout.object_details_panel = (henka_ui_rect){g_ui_panel_margin, g_ui_panel_margin, g_ui_details_width, g_ui_panel_height};
    layout.utility_panel = (henka_ui_rect){g_ui_panel_margin, g_ui_panel_margin, g_ui_details_width, 228.0f};
    layout.scene_viewport = (henka_viewport){0, 0, framebuffer_width > 0 ? framebuffer_width : 1, framebuffer_height > 0 ? framebuffer_height : 1};

    if (framebuffer_width <= 0 || framebuffer_height <= 0)
    {
        return layout;
    }

    if (state != NULL && state->ui != NULL && !henka_ui_is_visible(state->ui))
    {
        layout.scene_frame = (henka_ui_rect){0.0f, 0.0f, (float)framebuffer_width, (float)framebuffer_height};
        layout.scene_viewport = (henka_viewport){0, 0, framebuffer_width, framebuffer_height};
        return layout;
    }

    layout_mode = state != NULL ? state->workspace.layout_mode : SANDBOX3D_LAYOUT_VIEW;
    scene_visible = sandbox3d_workspace_shows_scene_panel(state);
    details_visible = sandbox3d_workspace_shows_details_panel(state);
    utility_visible = sandbox3d_workspace_shows_utility_panel(state);
    panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    controls_left = panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT;
    controls_right = panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT;
    panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    scene_left = scene_visible && panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT;
    scene_right = scene_visible && panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT;
    panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS);
    details_left = details_visible && panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT;
    details_right = details_visible && panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT;
    panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_UTILITY);
    utility_left = utility_visible && panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_LEFT;
    utility_right = utility_visible && panel != NULL && panel->dock == SANDBOX3D_WORKSPACE_DOCK_RIGHT;
    left_visible = controls_left || scene_left || details_left || utility_left;
    right_visible = controls_right || scene_right || details_right || utility_right;

    memset(&workspace_desc, 0, sizeof(workspace_desc));
    workspace_desc.framebuffer_width = framebuffer_width;
    workspace_desc.framebuffer_height = framebuffer_height;
    workspace_desc.margin = 16.0f;
    workspace_desc.gap = 14.0f;
    workspace_desc.scene_header_height = 30.0f;
    workspace_desc.scene_padding = 8.0f;
    workspace_desc.min_scene_width = framebuffer_width >= 1280 ? 520 : (framebuffer_width >= 960 ? 420 : 260);
    workspace_desc.min_scene_height = framebuffer_height >= 720 ? 404 : (framebuffer_height >= 640 ? 344 : 244);
    workspace_desc.left_dock_visible = left_visible;
    workspace_desc.right_dock_visible = right_visible;
    workspace_desc.bottom_dock_visible = false;
    workspace_desc.left_dock_width = state->workspace.model.left_dock_width;
    workspace_desc.right_dock_width = state->workspace.model.right_dock_width;

    layout_result = henka_workspace_layout_docked(&workspace_desc, &docked_layout);
    if (layout_result != HENKA_SUCCESS)
    {
        return layout;
    }

    layout.left_dock = docked_layout.left_dock;
    layout.scene_frame = docked_layout.scene_frame;
    layout.right_dock = docked_layout.right_dock;
    layout.scene_viewport = docked_layout.scene_viewport;
    sandbox3d_reserve_debug_strip(&layout);
    layout.controls_panel = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    layout.scene_objects_panel = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    layout.object_details_panel = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    layout.utility_panel = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};

    controls_height = layout.left_dock.height * (layout_mode == SANDBOX3D_LAYOUT_FULL ? 0.70f : 0.69f);
    if (controls_height < 470.0f)
    {
        controls_height = 470.0f;
    }
    if (controls_height > layout.left_dock.height)
    {
        controls_height = layout.left_dock.height;
    }

    if (controls_left && scene_left)
    {
        layout.controls_panel = (henka_ui_rect){layout.left_dock.x, layout.left_dock.y, layout.left_dock.width, controls_height};
        layout.scene_objects_panel = (henka_ui_rect)
        {
            layout.left_dock.x,
            layout.controls_panel.y + layout.controls_panel.height + g_ui_panel_gap,
            layout.left_dock.width,
            layout.left_dock.height - layout.controls_panel.height - g_ui_panel_gap
        };
        if (layout.scene_objects_panel.height < 0.0f)
        {
            layout.scene_objects_panel.height = 0.0f;
        }
    }
    else if (controls_left)
    {
        layout.controls_panel = layout.left_dock;
    }
    else if (scene_left)
    {
        layout.scene_objects_panel = layout.left_dock;
    }
    else if (details_left)
    {
        layout.object_details_panel = layout.left_dock;
    }
    else if (utility_left)
    {
        layout.utility_panel = layout.left_dock;
    }

    if (utility_right && sandbox3d_utility_uses_full_dock(state))
    {
        layout.object_details_panel = (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
        layout.utility_panel = layout.right_dock;
    }
    else if (details_right && utility_right)
    {
        details_height = layout.right_dock.height * (layout_mode == SANDBOX3D_LAYOUT_FULL ? 0.56f : 0.54f);
        if (details_height < 400.0f)
        {
            details_height = 400.0f;
        }
        if (details_height > layout.right_dock.height)
        {
            details_height = layout.right_dock.height;
        }

        layout.object_details_panel = (henka_ui_rect){layout.right_dock.x, layout.right_dock.y, layout.right_dock.width, details_height};
        layout.utility_panel = (henka_ui_rect)
        {
            layout.right_dock.x,
            layout.right_dock.y + details_height + g_ui_panel_gap,
            layout.right_dock.width,
            layout.right_dock.height - details_height - g_ui_panel_gap
        };
    }
    else if (details_right)
    {
        layout.object_details_panel = layout.right_dock;
    }
    else if (utility_right)
    {
        layout.utility_panel = layout.right_dock;
    }
    else if (controls_right)
    {
        layout.controls_panel = layout.right_dock;
    }
    else if (scene_right)
    {
        layout.scene_objects_panel = layout.right_dock;
    }

    if (sandbox3d_workspace_panel_is_floating(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS))
    {
        layout.controls_panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS)->floating_rect;
    }
    if (scene_visible && sandbox3d_workspace_panel_is_floating(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS))
    {
        layout.scene_objects_panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS)->floating_rect;
    }
    if (details_visible && sandbox3d_workspace_panel_is_floating(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS))
    {
        layout.object_details_panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS)->floating_rect;
    }
    if (utility_visible && sandbox3d_workspace_panel_is_floating(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_UTILITY))
    {
        layout.utility_panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_UTILITY)->floating_rect;
    }

    if (left_visible)
    {
        layout.left_splitter = sandbox3d_workspace_left_splitter_rect(layout.left_dock, layout.scene_frame);
    }
    if (right_visible)
    {
        layout.right_splitter = sandbox3d_workspace_right_splitter_rect(layout.scene_frame, layout.right_dock);
    }

    return layout;
}

static henka_ui_rect sandbox3d_get_panel_rect(
    const sandbox3d_workspace_layout* layout,
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

static henka_ui_rect sandbox3d_get_workspace_dock_target_rect(
    const sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout,
    sandbox3d_workspace_dock_zone dock_zone,
    int framebuffer_width,
    int framebuffer_height)
{
    const float margin = 14.0f;
    float height;
    float width;

    if (state == NULL || layout == NULL || framebuffer_width <= 0 || framebuffer_height <= 0)
    {
        return (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
    }

    if (dock_zone == SANDBOX3D_WORKSPACE_DOCK_LEFT && layout->left_dock.width > 0.0f)
    {
        return layout->left_dock;
    }
    if (dock_zone == SANDBOX3D_WORKSPACE_DOCK_RIGHT && layout->right_dock.width > 0.0f)
    {
        return layout->right_dock;
    }

    height = (float)framebuffer_height - margin * 2.0f - g_ui_debug_strip_height;
    if (dock_zone == SANDBOX3D_WORKSPACE_DOCK_LEFT)
    {
        width = state->workspace.model.left_dock_width;
        return (henka_ui_rect){margin, margin, width, height};
    }
    if (dock_zone == SANDBOX3D_WORKSPACE_DOCK_RIGHT)
    {
        width = state->workspace.model.right_dock_width;
        return (henka_ui_rect){(float)framebuffer_width - margin - width, margin, width, height};
    }
    return (henka_ui_rect){0.0f, 0.0f, 0.0f, 0.0f};
}

static bool sandbox3d_workspace_can_dock_panel(
    const sandbox3d_state* state,
    sandbox3d_workspace_panel_id panel_id,
    sandbox3d_workspace_dock_zone dock_zone)
{
    const sandbox3d_workspace_panel* panel;
    int index;

    if (state == NULL || dock_zone == SANDBOX3D_WORKSPACE_DOCK_FLOATING)
    {
        return false;
    }

    for (index = 0; index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++index)
    {
        if (index == (int)panel_id)
        {
            continue;
        }
        panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, (sandbox3d_workspace_panel_id)index);
        if (panel != NULL && panel->dock == dock_zone)
        {
            const bool supported_default_pair =
                (dock_zone == SANDBOX3D_WORKSPACE_DOCK_LEFT &&
                 ((panel_id == SANDBOX3D_WORKSPACE_PANEL_CONTROLS && index == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS) ||
                  (panel_id == SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS && index == SANDBOX3D_WORKSPACE_PANEL_CONTROLS))) ||
                (dock_zone == SANDBOX3D_WORKSPACE_DOCK_RIGHT &&
                 ((panel_id == SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS && index == SANDBOX3D_WORKSPACE_PANEL_UTILITY) ||
                  (panel_id == SANDBOX3D_WORKSPACE_PANEL_UTILITY && index == SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS)));
            if (!supported_default_pair)
            {
                return false;
            }
        }
    }
    return true;
}

static void sandbox3d_dock_workspace_panel(
    sandbox3d_state* state,
    sandbox3d_workspace_panel_id panel_id,
    sandbox3d_workspace_dock_zone dock_zone)
{
    if (!sandbox3d_workspace_can_dock_panel(state, panel_id, dock_zone))
    {
        sandbox3d_set_statusf(
            state,
            true,
            false,
            "Dock %s is reserved. Undock its home panels first or use Home.",
            sandbox3d_workspace_dock_name(dock_zone));
        snprintf(state->workspace.model.last_action, sizeof(state->workspace.model.last_action), "Dock rejected: occupied");
        return;
    }

    sandbox3d_workspace_dock_panel(&state->workspace.model, panel_id, dock_zone);
    sandbox3d_set_statusf(
        state,
        false,
        false,
        "%s docked %s.",
        sandbox3d_workspace_panel_name(panel_id),
        sandbox3d_workspace_dock_name(dock_zone));
}

static void sandbox3d_draw_panel_workspace_controls(
    henka_engine* engine,
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout,
    sandbox3d_workspace_panel_id panel_id)
{
    char button_id[48];
    const sandbox3d_workspace_panel* panel;
    henka_ui_rect bounds;
    if (engine == NULL || state == NULL || layout == NULL)
    {
        return;
    }
    bounds = sandbox3d_get_panel_rect(layout, panel_id);
    panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, panel_id);
    if (panel == NULL || bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return;
    }

    if (panel->dock != SANDBOX3D_WORKSPACE_DOCK_FLOATING)
    {
        henka_ui_label(state->ui, bounds.x + bounds.width - 40.0f, bounds.y + 10.0f, 1.0f, "drag");
        return;
    }

    henka_ui_label(state->ui, bounds.x + bounds.width - 194.0f, bounds.y + 10.0f, 1.0f, "drag");
    snprintf(button_id, sizeof(button_id), "dock_left_%d", (int)panel_id);
    if (henka_ui_button(state->ui, button_id, (henka_ui_rect){bounds.x + bounds.width - 158.0f, bounds.y + 4.0f, 34.0f, 22.0f}, "L"))
    {
        sandbox3d_dock_workspace_panel(state, panel_id, SANDBOX3D_WORKSPACE_DOCK_LEFT);
    }
    snprintf(button_id, sizeof(button_id), "dock_right_%d", (int)panel_id);
    if (henka_ui_button(state->ui, button_id, (henka_ui_rect){bounds.x + bounds.width - 120.0f, bounds.y + 4.0f, 34.0f, 22.0f}, "R"))
    {
        sandbox3d_dock_workspace_panel(state, panel_id, SANDBOX3D_WORKSPACE_DOCK_RIGHT);
    }
    snprintf(button_id, sizeof(button_id), "dock_home_%d", (int)panel_id);
    if (henka_ui_button(state->ui, button_id, (henka_ui_rect){bounds.x + bounds.width - 82.0f, bounds.y + 4.0f, 74.0f, 22.0f}, "Home"))
    {
        sandbox3d_dock_workspace_panel(state, panel_id, panel->default_dock);
    }
}

static void sandbox3d_draw_workspace_affordances(
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout,
    int framebuffer_width,
    int framebuffer_height)
{
    const sandbox3d_workspace_panel* panel;
    henka_ui_rect bounds;
    int index;

    if (state == NULL || layout == NULL || state->ui == NULL)
    {
        return;
    }

    if (layout->left_splitter.width > 0.0f)
    {
        henka_ui_overlay_rect(state->ui, layout->left_splitter, (henka_vec4){0.20f, 0.40f, 0.60f, 0.90f});
    }
    if (layout->right_splitter.width > 0.0f)
    {
        henka_ui_overlay_rect(state->ui, layout->right_splitter, (henka_vec4){0.20f, 0.40f, 0.60f, 0.90f});
    }

    if (state->workspace.model.active_drag_panel != SANDBOX3D_WORKSPACE_PANEL_NONE)
    {
        bounds = sandbox3d_get_workspace_dock_target_rect(
            state,
            layout,
            state->workspace.model.active_dock_target,
            framebuffer_width,
            framebuffer_height);
        if (state->workspace.model.active_dock_target != SANDBOX3D_WORKSPACE_DOCK_FLOATING &&
            bounds.width > 0.0f)
        {
            henka_ui_overlay_rect(state->ui, bounds, (henka_vec4){0.10f, 0.42f, 0.62f, 0.22f});
        }
    }

    for (index = 0; index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++index)
    {
        if (!sandbox3d_workspace_panel_visible(state, (sandbox3d_workspace_panel_id)index))
        {
            continue;
        }
        panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, (sandbox3d_workspace_panel_id)index);
        if (panel == NULL || panel->dock != SANDBOX3D_WORKSPACE_DOCK_FLOATING)
        {
            continue;
        }
        bounds = sandbox3d_workspace_resize_rect(sandbox3d_get_panel_rect(layout, (sandbox3d_workspace_panel_id)index));
        henka_ui_overlay_rect(
            state->ui,
            bounds,
            state->workspace.model.active_resize_panel == (sandbox3d_workspace_panel_id)index
                ? (henka_vec4){0.96f, 0.72f, 0.18f, 1.0f}
                : (henka_vec4){0.26f, 0.55f, 0.72f, 1.0f});
    }
}

static void sandbox3d_draw_section_heading(henka_ui_context* ui, float x, float y, const char* title)
{
    if (ui == NULL || title == NULL)
    {
        return;
    }

    henka_ui_heading(ui, x, y, 1.0f, title);
}

static void sandbox3d_draw_value_row(
    henka_ui_context* ui,
    float x,
    float y,
    float width,
    const char* label,
    const char* value)
{
    if (ui == NULL || label == NULL || value == NULL)
    {
        return;
    }

    henka_ui_value_row(ui, (henka_ui_rect){x, y, width, 22.0f}, label, value);
}

static void sandbox3d_draw_status_block(
    const sandbox3d_state* state,
    float x,
    float y,
    float width,
    bool compact)
{
    char layout_row[48];
    const sandbox3d_object_descriptor* descriptor;
    const char* status_text;

    if (state == NULL || state->ui == NULL)
    {
        return;
    }

    status_text = state->status_message[0] != '\0' ? state->status_message : "Ready.";
    descriptor = sandbox3d_get_selected_descriptor(state);

    henka_ui_status_chip(
        state->ui,
        (henka_ui_rect){x, y, 86.0f, 22.0f},
        state->status_warning ? "Warning" : "Status",
        state->status_warning);
    sandbox3d_draw_value_row(state->ui, x, y + 28.0f, width, "Last", status_text);
    snprintf(layout_row, sizeof(layout_row), "%s / %s", sandbox3d_get_layout_mode_label(state->workspace.layout_mode), sandbox3d_get_utility_label(state->workspace.active_utility));
    sandbox3d_draw_value_row(state->ui, x, y + 54.0f, width, "Mode", layout_row);
    if (!compact)
    {
        sandbox3d_draw_value_row(state->ui, x, y + 80.0f, width, "Selected", descriptor != NULL ? descriptor->display_name : "(none)");
    }
}

static void sandbox3d_draw_scene_viewport_frame(henka_ui_context* ui, henka_ui_rect bounds)
{
    if (ui == NULL || bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return;
    }

    henka_ui_viewport_frame(ui, bounds, "Scene View");
}

static void sandbox3d_draw_viewport_debug_strip(
    henka_engine* engine,
    const sandbox3d_state* state,
    henka_ui_rect bounds)
{
    char fit_first_row[160];
    char fit_second_row[192];
    char fit_third_row[192];
    char first_row[160];
    char second_row[192];
    char third_row[192];
    char selected_text[32];
    const char* hover_axis;
    const char* selected_name;
    size_t max_characters;

    if (engine == NULL || state == NULL || state->ui == NULL || bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return;
    }

    selected_name = sandbox3d_safe_entity_name(state, state->diagnostics.selected_entity, "(none)");
    sandbox3d_truncate_text(selected_name, selected_text, sizeof(selected_text), 18U);
    hover_axis = sandbox3d_get_gizmo_axis_label(state->gizmo.hover_axis);
    snprintf(
        first_row,
        sizeof(first_row),
        "Tool:%s | Sel:%s | HL:%s | Cap:%s | View:%s | UI:%s",
        sandbox3d_viewport_tool_mode_to_string(state->viewport_tool),
        selected_text,
        state->diagnostics.selected_highlight_active ? "On" : "Off",
        henka_engine_is_mouse_captured(engine) ? "On" : "Off",
        state->diagnostics.cursor_in_viewport ? "Yes" : "No",
        state->diagnostics.ui_wants_mouse ? "Yes" : "No");
    snprintf(
        second_row,
        sizeof(second_row),
        "Giz:%s | H:%zu | Hover:%s | Drag:%s/%u | Reject:%s",
        state->diagnostics.gizmo_model_valid ? "Valid" : "None",
        state->diagnostics.overlay_primitive_count,
        hover_axis,
        state->diagnostics.dragging ? "Yes" : "No",
        (unsigned int)state->diagnostics.drag_target_entity,
        sandbox3d_interaction_reject_reason_to_string(state->diagnostics.last_reject_reason));
    snprintf(
        third_row,
        sizeof(third_row),
        "P:%s | Head:%s | Move:%s | Size:%s | Dock:%s | WS:%s",
        sandbox3d_workspace_panel_name(state->diagnostics.hovered_panel),
        state->diagnostics.cursor_in_panel_header ? "Yes" : "No",
        sandbox3d_workspace_panel_name(state->diagnostics.active_panel_drag),
        state->diagnostics.active_workspace_resize == SANDBOX3D_WORKSPACE_RESIZE_LEFT_DOCK
            ? "Left Dock"
            : (state->diagnostics.active_workspace_resize == SANDBOX3D_WORKSPACE_RESIZE_RIGHT_DOCK
                ? "Right Dock"
                : sandbox3d_workspace_panel_name(state->diagnostics.active_panel_resize)),
        state->workspace.model.active_drag_panel == SANDBOX3D_WORKSPACE_PANEL_NONE
            ? "None"
            : sandbox3d_workspace_dock_name(state->workspace.model.active_dock_target),
        state->diagnostics.last_workspace_action);
    max_characters = bounds.width > 24.0f ? (size_t)((bounds.width - 16.0f) / 6.0f) : 4U;
    sandbox3d_truncate_text(first_row, fit_first_row, sizeof(fit_first_row), max_characters);
    sandbox3d_truncate_text(second_row, fit_second_row, sizeof(fit_second_row), max_characters);
    sandbox3d_truncate_text(third_row, fit_third_row, sizeof(fit_third_row), max_characters);

    henka_ui_overlay_rect(state->ui, bounds, (henka_vec4){0.05f, 0.08f, 0.12f, 0.94f});
    henka_ui_overlay_rect(
        state->ui,
        (henka_ui_rect){bounds.x, bounds.y, bounds.width, 1.0f},
        (henka_vec4){0.22f, 0.36f, 0.56f, 1.0f});
    henka_ui_label(state->ui, bounds.x + 8.0f, bounds.y + 8.0f, 1.0f, fit_first_row);
    henka_ui_label(state->ui, bounds.x + 8.0f, bounds.y + 24.0f, 1.0f, fit_second_row);
    henka_ui_label(state->ui, bounds.x + 8.0f, bounds.y + 40.0f, 1.0f, fit_third_row);
}

static void sandbox3d_draw_panel_recall_hint(henka_ui_context* ui, henka_viewport viewport)
{
    henka_ui_rect bounds;
    float height;
    float margin;
    float width;

    if (ui == NULL || !henka_viewport_is_valid(viewport))
    {
        return;
    }

    width = 252.0f;
    height = 44.0f;
    margin = 18.0f;
    bounds.x = (float)(viewport.x + viewport.width) - width - margin;
    bounds.y = (float)(viewport.y + viewport.height) - height - margin;
    bounds.width = width;
    bounds.height = height;

    henka_ui_overlay_hint(ui, bounds, "Panels hidden. Press F4 to show tools.", "F5 changes layout");
}

static void sandbox3d_draw_controls_panel(
    henka_engine* engine,
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout)
{
    bool details_panel_visible;
    bool full_mode;
    bool grid_visible;
    bool inspect_mode;
    sandbox3d_layout_mode layout_mode;
    float button_width;
    float half_button_width;
    int controls_page;
    bool scene_panel_visible;
    float third_button_width;
    bool wireframe_enabled;
    float x_left;
    float x_middle;
    float x_right;
    float y;
    henka_ui_rect panel_bounds;

    if (engine == NULL || state == NULL || layout == NULL)
    {
        return;
    }

    panel_bounds = layout->controls_panel;
    if (panel_bounds.width <= 0.0f || panel_bounds.height <= 0.0f)
    {
        return;
    }

    controls_page = state->paging.controls_page;
    if (controls_page < 0)
    {
        controls_page = 0;
    }
    if (controls_page > 1)
    {
        controls_page = 1;
    }
    state->paging.controls_page = controls_page;
    x_left = panel_bounds.x + 14.0f;
    half_button_width = (panel_bounds.width - 38.0f) * 0.5f;
    x_middle = x_left + half_button_width + 10.0f;
    third_button_width = (panel_bounds.width - 36.0f) / 3.0f;
    x_right = x_left + third_button_width * 2.0f + 8.0f;
    button_width = (panel_bounds.width - 36.0f) / 3.0f;
    scene_panel_visible = state->workspace.scene_objects_panel_visible;
    details_panel_visible = state->workspace.object_details_panel_visible;
    grid_visible = henka_scene_is_entity_visible(state->scene, state->grid_entity);
    wireframe_enabled = henka_engine_is_wireframe_enabled(engine);
    layout_mode = state->workspace.layout_mode;
    inspect_mode = layout_mode == SANDBOX3D_LAYOUT_INSPECT;
    full_mode = layout_mode == SANDBOX3D_LAYOUT_FULL;
    henka_ui_panel(state->ui, panel_bounds, "Controls");
    sandbox3d_draw_panel_workspace_controls(engine, state, layout, SANDBOX3D_WORKSPACE_PANEL_CONTROLS);
    if (henka_ui_tab(state->ui, "controls_page_main", (henka_ui_rect){x_left, panel_bounds.y + 38.0f, half_button_width, 24.0f}, "Main", controls_page == 0))
    {
        state->paging.controls_page = 0;
    }
    if (henka_ui_tab(state->ui, "controls_page_tools", (henka_ui_rect){x_middle, panel_bounds.y + 38.0f, half_button_width, 24.0f}, "Panels/Status", controls_page == 1))
    {
        state->paging.controls_page = 1;
    }

    if (controls_page == 0)
    {
        y = panel_bounds.y + 74.0f;
        sandbox3d_draw_section_heading(state->ui, x_left, y, "Workspace");
        y += 20.0f;
        if (henka_ui_tab(state->ui, "layout_view", (henka_ui_rect){x_left, y, third_button_width, 28.0f}, "View", layout_mode == SANDBOX3D_LAYOUT_VIEW))
        {
            state->workspace.layout_mode = SANDBOX3D_LAYOUT_VIEW;
            sandbox3d_set_statusf(state, false, false, "Layout set to %s.", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
            sandbox3d_print_layout_mode(state, false);
        }
        if (henka_ui_tab(state->ui, "layout_inspect", (henka_ui_rect){x_left + third_button_width + 4.0f, y, third_button_width, 28.0f}, "Inspect", layout_mode == SANDBOX3D_LAYOUT_INSPECT))
        {
            state->workspace.layout_mode = SANDBOX3D_LAYOUT_INSPECT;
            sandbox3d_set_statusf(state, false, false, "Layout set to %s.", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
            sandbox3d_print_layout_mode(state, false);
        }
        if (henka_ui_tab(state->ui, "layout_full", (henka_ui_rect){x_right, y, third_button_width, 28.0f}, "Full Tools", layout_mode == SANDBOX3D_LAYOUT_FULL))
        {
            state->workspace.layout_mode = SANDBOX3D_LAYOUT_FULL;
            sandbox3d_set_statusf(state, false, false, "Layout set to %s.", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
            sandbox3d_print_layout_mode(state, false);
        }
        y += 42.0f;
        sandbox3d_draw_section_heading(state->ui, x_left, y, "Viewer");
        y += 20.0f;
        if (henka_ui_toggle(state->ui, "grid", (henka_ui_rect){x_left, y, half_button_width, 28.0f}, "Grid", &grid_visible))
        {
            sandbox3d_toggle_grid_visibility(state, grid_visible, true);
        }
        else
        {
            sandbox3d_toggle_grid_visibility(state, grid_visible, false);
        }
        if (henka_ui_toggle(state->ui, "wireframe", (henka_ui_rect){x_middle, y, half_button_width, 28.0f}, "Wire", &wireframe_enabled))
        {
            sandbox3d_toggle_wireframe(engine, wireframe_enabled, true);
            sandbox3d_set_statusf(state, false, false, "Wireframe %s.", wireframe_enabled ? "on" : "off");
        }
        else
        {
            sandbox3d_toggle_wireframe(engine, wireframe_enabled, false);
        }

        y += 42.0f;
        sandbox3d_draw_section_heading(state->ui, x_left, y, "Viewport");
        y += 20.0f;
        if (henka_ui_primary_button(state->ui, "frame_selected", (henka_ui_rect){x_left, y, half_button_width, 28.0f}, "Frame Selected"))
        {
            sandbox3d_frame_selected_object(state, true);
        }
        if (henka_ui_button(state->ui, "reset_camera", (henka_ui_rect){x_middle, y, half_button_width, 28.0f}, "Reset View"))
        {
            sandbox3d_reset_camera_defaults(state);
            sandbox3d_set_statusf(state, false, true, "Camera reset to the default sandbox view.");
        }
        y += 36.0f;
        if (henka_ui_button(state->ui, "zoom_in", (henka_ui_rect){x_left, y, half_button_width, 28.0f}, "Zoom In"))
        {
            sandbox3d_zoom_camera_to_target(state, -1.0f);
        }
        if (henka_ui_button(state->ui, "zoom_out", (henka_ui_rect){x_middle, y, half_button_width, 28.0f}, "Zoom Out"))
        {
            sandbox3d_zoom_camera_to_target(state, 1.0f);
        }
        y += 42.0f;
        sandbox3d_draw_section_heading(state->ui, x_left, y, "Viewport Tool");
        y += 20.0f;
        if (henka_ui_tab(state->ui, "tool_select", (henka_ui_rect){x_left, y, button_width, 28.0f}, "Select", state->viewport_tool == SANDBOX3D_VIEWPORT_TOOL_SELECT))
        {
            sandbox3d_set_viewport_tool_mode(state, SANDBOX3D_VIEWPORT_TOOL_SELECT, true);
        }
        if (henka_ui_tab(state->ui, "tool_orbit", (henka_ui_rect){x_left + button_width + 4.0f, y, button_width, 28.0f}, "Orbit", state->viewport_tool == SANDBOX3D_VIEWPORT_TOOL_ORBIT))
        {
            sandbox3d_set_viewport_tool_mode(state, SANDBOX3D_VIEWPORT_TOOL_ORBIT, true);
        }
        if (henka_ui_tab(state->ui, "tool_pan", (henka_ui_rect){x_right, y, button_width, 28.0f}, "Pan", state->viewport_tool == SANDBOX3D_VIEWPORT_TOOL_PAN))
        {
            sandbox3d_set_viewport_tool_mode(state, SANDBOX3D_VIEWPORT_TOOL_PAN, true);
        }
        y += 36.0f;
        if (henka_ui_tab(state->ui, "tool_move", (henka_ui_rect){x_left, y, button_width, 28.0f}, "Move", state->viewport_tool == SANDBOX3D_VIEWPORT_TOOL_MOVE))
        {
            sandbox3d_set_viewport_tool_mode(state, SANDBOX3D_VIEWPORT_TOOL_MOVE, true);
        }
        if (henka_ui_tab(state->ui, "tool_rotate", (henka_ui_rect){x_left + button_width + 4.0f, y, button_width, 28.0f}, "Rotate", state->viewport_tool == SANDBOX3D_VIEWPORT_TOOL_ROTATE))
        {
            sandbox3d_set_viewport_tool_mode(state, SANDBOX3D_VIEWPORT_TOOL_ROTATE, true);
        }
        if (henka_ui_tab(state->ui, "tool_scale", (henka_ui_rect){x_right, y, button_width, 28.0f}, "Scale", state->viewport_tool == SANDBOX3D_VIEWPORT_TOOL_SCALE))
        {
            sandbox3d_set_viewport_tool_mode(state, SANDBOX3D_VIEWPORT_TOOL_SCALE, true);
        }
        y += 36.0f;
        if (henka_ui_primary_button(state->ui, "gizmo_snap", (henka_ui_rect){x_left + button_width + 4.0f, y, panel_bounds.width - 28.0f - button_width - 4.0f, 28.0f}, state->gizmo.snap.enabled ? "Snap Enabled" : "Snap Disabled"))
        {
            sandbox3d_gizmo_toggle_snap(state);
        }
        if (henka_ui_toggle(state->ui, "debug_hit_boxes", (henka_ui_rect){x_left, y, button_width, 28.0f}, "Hit Boxes", &state->diagnostics.show_handle_hit_boxes))
        {
            sandbox3d_set_statusf(state, false, false, "Handle hit boxes %s.", state->diagnostics.show_handle_hit_boxes ? "shown" : "hidden");
        }
        y += 36.0f;
        if (henka_ui_primary_button(state->ui, "quick_diagnostics", (henka_ui_rect){x_left, y, third_button_width, 28.0f}, "Diagnostics"))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_DIAGNOSTICS);
            sandbox3d_set_statusf(state, false, false, "Diagnostics are open in the Utility panel.");
        }
        if (henka_ui_primary_button(state->ui, "quick_transform_qa", (henka_ui_rect){x_left + third_button_width + 4.0f, y, third_button_width, 28.0f}, "Transform QA"))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_TRANSFORM_QA);
            sandbox3d_set_statusf(state, false, false, "Transform QA is open in the Utility panel.");
        }
        if (henka_ui_primary_button(state->ui, "quick_physics_qa", (henka_ui_rect){x_right, y, third_button_width, 28.0f}, "Physics QA"))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_PHYSICS_QA);
            sandbox3d_set_statusf(state, false, false, "Physics QA is open in the Utility panel.");
        }
        y += 36.0f;
        if (henka_ui_primary_button(
                state->ui,
                "native_panel_test",
                (henka_ui_rect){x_left, y, panel_bounds.width - 28.0f, 28.0f},
                "Open Native Panel Test"))
        {
            sandbox3d_open_native_panel_test(engine, state);
        }
    }
    else
    {
        y = panel_bounds.y + 74.0f;
        if (inspect_mode || full_mode)
        {
            sandbox3d_draw_section_heading(state->ui, x_left, y, "Panels");
            y += 20.0f;
            if (henka_ui_toggle(state->ui, "scene_panel_visible", (henka_ui_rect){x_left, y, half_button_width, 28.0f}, "Objects", &scene_panel_visible))
            {
                state->workspace.scene_objects_panel_visible = scene_panel_visible;
                sandbox3d_set_statusf(state, false, false, "Objects panel %s.", scene_panel_visible ? "shown" : "hidden");
            }
            else
            {
                state->workspace.scene_objects_panel_visible = scene_panel_visible;
            }
            if (henka_ui_toggle(state->ui, "details_panel_visible", (henka_ui_rect){x_middle, y, half_button_width, 28.0f}, "Details", &details_panel_visible))
            {
                state->workspace.object_details_panel_visible = details_panel_visible;
                sandbox3d_set_statusf(state, false, false, "Details panel %s.", details_panel_visible ? "shown" : "hidden");
            }
            else
            {
                state->workspace.object_details_panel_visible = details_panel_visible;
            }
            y += 44.0f;
        }

        sandbox3d_draw_section_heading(state->ui, x_left, y, "Utility");
        y += 20.0f;
        if (henka_ui_tab(state->ui, "utility_help", (henka_ui_rect){x_left, y, button_width, 26.0f}, "Help", state->workspace.active_utility == SANDBOX3D_UTILITY_HELP))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_HELP);
            sandbox3d_set_statusf(state, false, false, "Help is open in the Utility panel.");
            sandbox3d_print_help(state);
        }
        if (henka_ui_tab(state->ui, "utility_legend", (henka_ui_rect){x_left + button_width + 4.0f, y, button_width, 26.0f}, "Legend", state->workspace.active_utility == SANDBOX3D_UTILITY_SCENE_LEGEND))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_SCENE_LEGEND);
            sandbox3d_set_statusf(state, false, false, "Scene legend is open in the Utility panel.");
            sandbox3d_print_scene_legend(state);
            fflush(stdout);
        }
        if (henka_ui_tab(state->ui, "utility_paths", (henka_ui_rect){x_right, y, button_width, 26.0f}, "Paths", state->workspace.active_utility == SANDBOX3D_UTILITY_PATHS))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_PATHS);
            sandbox3d_set_statusf(state, false, false, "Paths are open in the Utility panel.");
        }
        y += 34.0f;
        if (henka_ui_tab(state->ui, "utility_settings", (henka_ui_rect){x_left, y, button_width, 26.0f}, "Settings", state->workspace.active_utility == SANDBOX3D_UTILITY_SETTINGS))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_SETTINGS);
            sandbox3d_set_statusf(state, false, false, "Settings summary is open in the Utility panel.");
        }
        if (henka_ui_tab(state->ui, "utility_diag", (henka_ui_rect){x_left + button_width + 4.0f, y, button_width, 26.0f}, "Diag", state->workspace.active_utility == SANDBOX3D_UTILITY_DIAGNOSTICS))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_DIAGNOSTICS);
            sandbox3d_set_statusf(state, false, false, "Diagnostics are open in the Utility panel.");
        }
        if (henka_ui_tab(state->ui, "utility_qa", (henka_ui_rect){x_right, y, button_width, 26.0f}, "T QA", state->workspace.active_utility == SANDBOX3D_UTILITY_TRANSFORM_QA))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_TRANSFORM_QA);
            sandbox3d_set_statusf(state, false, false, "Transform QA is open in the Utility panel.");
        }
        y += 34.0f;
        if (henka_ui_tab(state->ui, "utility_physics", (henka_ui_rect){x_left, y, button_width, 26.0f}, "Physics QA", state->workspace.active_utility == SANDBOX3D_UTILITY_PHYSICS_QA))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_PHYSICS_QA);
            sandbox3d_set_statusf(state, false, false, "Physics QA is open in the Utility panel.");
        }

        y += 46.0f;
        sandbox3d_draw_section_heading(state->ui, x_left, y, "Status");
        y += 20.0f;
        sandbox3d_draw_status_block(state, x_left, y, panel_bounds.width - 28.0f, true);

        y = panel_bounds.y + panel_bounds.height - 106.0f;
        if (henka_ui_button(state->ui, "save_settings", (henka_ui_rect){x_left, y, half_button_width, 28.0f}, "Save Settings"))
        {
            if (sandbox3d_save_settings(engine, state) == HENKA_SUCCESS)
            {
                sandbox3d_set_statusf(state, false, true, "Settings saved.");
            }
            else
            {
                sandbox3d_set_statusf(state, true, true, "Settings could not be saved.");
            }
        }
        if (henka_ui_button(state->ui, "reset_layout", (henka_ui_rect){x_middle, y, half_button_width, 28.0f}, "Reset Layout"))
        {
            sandbox3d_close_native_panel_test(engine, state);
            sandbox3d_reset_workspace_layout(state);
            sandbox3d_set_statusf(state, false, true, "Layout reset to View. Panels redocked and dock sizes restored.");
            sandbox3d_print_layout_mode(state, false);
        }
        y += 34.0f;
        if (henka_ui_button(state->ui, "reset_settings", (henka_ui_rect){x_left, y, half_button_width, 28.0f}, "Reset Settings"))
        {
            if (sandbox3d_reset_settings(engine, state) == HENKA_SUCCESS)
            {
                sandbox3d_set_statusf(state, false, true, "Settings reset to defaults.");
            }
            else
            {
                sandbox3d_set_statusf(state, true, true, "Settings could not be reset.");
            }
        }
        if (henka_ui_primary_button(state->ui, "open_legend", (henka_ui_rect){x_middle, y, half_button_width, 28.0f}, "Open Legend"))
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_SCENE_LEGEND);
            sandbox3d_set_statusf(state, false, false, "Scene legend is open in the Utility panel.");
            sandbox3d_print_scene_legend(state);
            fflush(stdout);
        }
    }
}

static void sandbox3d_draw_scene_objects_panel(
    henka_engine* engine,
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout)
{
    int items_per_page;
    int page_count;
    int page_index;
    char row_label[96];
    char subtitle_text[96];
    const sandbox3d_object_descriptor* descriptor;
    const char* entity_name;
    float footer_y;
    float row_y;
    henka_entity entity;
    henka_ui_rect panel_bounds;
    size_t selectable_count;
    size_t descriptor_index;
    size_t visible_index;

    if (engine == NULL || state == NULL || layout == NULL || state->scene == NULL || !sandbox3d_workspace_shows_scene_panel(state))
    {
        return;
    }

    panel_bounds = layout->scene_objects_panel;
    if (panel_bounds.width <= 0.0f || panel_bounds.height <= 0.0f)
    {
        return;
    }
    henka_ui_panel(state->ui, panel_bounds, "Scene Objects");
    sandbox3d_draw_panel_workspace_controls(engine, state, layout, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS);
    henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 38.0f, 1.0f, "Current scene examples");

    selectable_count = 0U;
    for (descriptor_index = 0U; descriptor_index < SANDBOX3D_OBJECT_COUNT; ++descriptor_index)
    {
        if (sandbox3d_is_selectable_entity(state, state->descriptors[descriptor_index].entity))
        {
            ++selectable_count;
        }
    }

    items_per_page = (int)((panel_bounds.height - 106.0f) / 34.0f);
    if (items_per_page < 1)
    {
        items_per_page = 1;
    }
    page_count = (int)((selectable_count + (size_t)items_per_page - 1U) / (size_t)items_per_page);
    if (page_count < 1)
    {
        page_count = 1;
    }
    if (state->paging.scene_objects_page >= page_count)
    {
        state->paging.scene_objects_page = page_count - 1;
    }
    page_index = state->paging.scene_objects_page;

    row_y = panel_bounds.y + 64.0f;
    visible_index = 0U;
    for (descriptor_index = 0U; descriptor_index < SANDBOX3D_OBJECT_COUNT; ++descriptor_index)
    {
        descriptor = &state->descriptors[descriptor_index];
        entity = descriptor->entity;
        if (!sandbox3d_is_selectable_entity(state, entity))
        {
            continue;
        }

        if ((int)(visible_index / (size_t)items_per_page) != page_index)
        {
            ++visible_index;
            continue;
        }

        entity_name = henka_scene_get_entity_name(state->scene, entity);
        if (entity_name == NULL)
        {
            ++visible_index;
            continue;
        }

        snprintf(
            subtitle_text,
            sizeof(subtitle_text),
            "%s - %s",
            entity_name != NULL ? entity_name : descriptor->display_name,
            sandbox3d_get_descriptor_role_label(descriptor));
        sandbox3d_truncate_text(subtitle_text, row_label, sizeof(row_label), 30U);

        if (!henka_scene_is_entity_visible(state->scene, entity))
        {
            sandbox3d_truncate_text("[Hidden]", subtitle_text, sizeof(subtitle_text), 12U);
            snprintf(row_label, sizeof(row_label), "%s %s", subtitle_text, row_label);
        }

        if (henka_ui_selectable(
            state->ui,
            entity_name != NULL ? entity_name : row_label,
            (henka_ui_rect){panel_bounds.x + 14.0f, row_y, panel_bounds.width - 28.0f, 28.0f},
            row_label,
            sandbox3d_get_real_selected_entity(state) == entity))
        {
            sandbox3d_select_entity(state, entity);
        }

        row_y += 34.0f;
        ++visible_index;
    }

    footer_y = panel_bounds.y + panel_bounds.height - 34.0f;
    if (page_count > 1)
    {
        char page_text[32];

        if (henka_ui_button(state->ui, "scene_objects_prev", (henka_ui_rect){panel_bounds.x + 14.0f, footer_y, 74.0f, 24.0f}, "Prev"))
        {
            sandbox3d_advance_panel_paging(state, SANDBOX3D_PANEL_SCROLL_SCENE_OBJECTS, -1);
        }
        snprintf(page_text, sizeof(page_text), "Page %d/%d", page_index + 1, page_count);
        henka_ui_label(state->ui, panel_bounds.x + 102.0f, footer_y + 6.0f, 1.0f, page_text);
        if (henka_ui_button(state->ui, "scene_objects_next", (henka_ui_rect){panel_bounds.x + panel_bounds.width - 88.0f, footer_y, 74.0f, 24.0f}, "Next"))
        {
            sandbox3d_advance_panel_paging(state, SANDBOX3D_PANEL_SCROLL_SCENE_OBJECTS, 1);
        }
    }
}

static void sandbox3d_draw_object_details_panel(
    henka_engine* engine,
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout)
{
    bool visible;
    char action_label[32];
    char detail_text[96];
    char developer_text[96];
    char interaction_text[64];
    char material_text[96];
    char physics_text[96];
    char position_text[64];
    char rotation_text[64];
    char texture_text[96];
    char velocity_text[64];
    char scale_text[64];
    const sandbox3d_object_descriptor* descriptor;
    henka_physics_body_id physics_body;
    henka_physics_body_state body_state;
    henka_interaction_desc interaction;
    henka_interaction_result interaction_result;
    henka_material material;
    henka_scene_object_info object_info;
    henka_result result;
    henka_transform transform;
    henka_ui_rect panel_bounds;
    float action_button_width;

    bool compact_mode;
    if (engine == NULL || state == NULL || layout == NULL || !sandbox3d_workspace_shows_details_panel(state))
    {
        return;
    }

    panel_bounds = layout->object_details_panel;
    if (panel_bounds.width <= 0.0f || panel_bounds.height <= 0.0f)
    {
        return;
    }
    henka_ui_panel(state->ui, panel_bounds, "Object Details");
    sandbox3d_draw_panel_workspace_controls(engine, state, layout, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS);
    compact_mode = true;
    action_button_width = (panel_bounds.width - 42.0f) * 0.5f;

    descriptor = sandbox3d_get_selected_descriptor(state);
    if (state->scene == NULL || descriptor == NULL)
    {
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 38.0f, 1.0f, "No object selected.");
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 56.0f, 1.0f, "Click a viewport object or Scene Objects row to inspect it.");
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 74.0f, 1.0f, "The yellow viewport highlight appears only for a real selected object.");
        return;
    }

    result = henka_scene_get_entity_transform(state->scene, descriptor->entity, &transform);
    if (result != HENKA_SUCCESS)
    {
        henka_ui_label(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 38.0f, 1.0f, "Selected object details could not be read.");
        return;
    }
    henka_scene_get_entity_info(state->scene, descriptor->entity, &object_info);
    if (henka_scene_get_entity_material(state->scene, descriptor->entity, &material) != HENKA_SUCCESS)
    {
        material = henka_material_default();
    }
    if (henka_material_describe(&material, material_text, sizeof(material_text)) != HENKA_SUCCESS)
    {
        snprintf(material_text, sizeof(material_text), "%s", descriptor->material_summary);
    }
    if (henka_scene_get_entity_interaction(state->scene, descriptor->entity, &interaction) != HENKA_SUCCESS)
    {
        interaction = (henka_interaction_desc){false, 0.0f, NULL};
    }
    interaction_result = henka_scene_can_interact(state->scene, descriptor->entity, state->camera.position);

    visible = henka_scene_is_entity_visible(state->scene, descriptor->entity);
    physics_body = sandbox3d_get_physics_body_for_entity(state, descriptor->entity);
    if (physics_body != HENKA_INVALID_PHYSICS_BODY_ID &&
        henka_physics_body_get_state(state->physics.world, physics_body, &body_state) == HENKA_SUCCESS)
    {
        snprintf(
            physics_text,
            sizeof(physics_text),
            "%s %s m%.1f",
            henka_physics_body_type_get_label(body_state.type),
            henka_physics_shape_type_get_label(body_state.collider.shape),
            body_state.mass);
        snprintf(
            velocity_text,
            sizeof(velocity_text),
            "%.2f %.2f %.2f",
            body_state.linear_velocity.x,
            body_state.linear_velocity.y,
            body_state.linear_velocity.z);
    }
    else
    {
        snprintf(physics_text, sizeof(physics_text), "No physics body");
        snprintf(velocity_text, sizeof(velocity_text), "(none)");
    }
    snprintf(position_text, sizeof(position_text), "%.2f %.2f %.2f", transform.position.x, transform.position.y, transform.position.z);
    snprintf(rotation_text, sizeof(rotation_text), "%.2f %.2f %.2f %.2f", transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w);
    snprintf(scale_text, sizeof(scale_text), "%.2f %.2f %.2f", transform.scale.x, transform.scale.y, transform.scale.z);
    sandbox3d_truncate_text(descriptor->short_explanation, detail_text, sizeof(detail_text), compact_mode ? 42U : 54U);
    sandbox3d_truncate_text(descriptor->developer_detail, developer_text, sizeof(developer_text), compact_mode ? 42U : 54U);
    sandbox3d_truncate_text(descriptor->texture_summary, texture_text, sizeof(texture_text), compact_mode ? 42U : 54U);

    henka_ui_heading(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 38.0f, 1.0f, descriptor->display_name);
    sandbox3d_draw_section_heading(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 58.0f, "Overview");
    sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 76.0f, panel_bounds.width - 28.0f, "Visible", visible ? "Yes" : "No");
    sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 102.0f, panel_bounds.width - 28.0f, "Shows", detail_text);
    sandbox3d_draw_section_heading(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 132.0f, "Transform");
    sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 150.0f, panel_bounds.width - 28.0f, "Position", position_text);
    sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 176.0f, panel_bounds.width - 28.0f, "Scale", scale_text);
    if (!compact_mode)
    {
        sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 202.0f, panel_bounds.width - 28.0f, "Rotation", rotation_text);
        sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 228.0f, panel_bounds.width - 28.0f, "Tag", object_info.tag != NULL ? object_info.tag : "(none)");
    }

    sandbox3d_draw_section_heading(state->ui, panel_bounds.x + 14.0f, compact_mode ? panel_bounds.y + 208.0f : panel_bounds.y + 260.0f, "Physics");
    sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, compact_mode ? panel_bounds.y + 226.0f : panel_bounds.y + 278.0f, panel_bounds.width - 28.0f, "Body", physics_text);
    sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, compact_mode ? panel_bounds.y + 252.0f : panel_bounds.y + 304.0f, panel_bounds.width - 28.0f, "Velocity", velocity_text);
    if (!compact_mode)
    {
        sandbox3d_draw_value_row(state->ui, panel_bounds.x + 14.0f, panel_bounds.y + 330.0f, panel_bounds.width - 28.0f, "Texture", texture_text);
    }

    if (!interaction.enabled)
    {
        snprintf(interaction_text, sizeof(interaction_text), "%s", "Disabled");
    }
    else if (interaction_result == HENKA_INTERACTION_RESULT_OUT_OF_RANGE)
    {
        snprintf(interaction_text, sizeof(interaction_text), "Out of range");
    }
    else
    {
        snprintf(interaction_text, sizeof(interaction_text), "%s", interaction.prompt != NULL ? interaction.prompt : "Available");
    }
    sandbox3d_draw_value_row(
        state->ui,
        panel_bounds.x + 14.0f,
        compact_mode ? panel_bounds.y + 278.0f : panel_bounds.y + 332.0f,
        panel_bounds.width - 28.0f,
        "Object Use",
        interaction_text);

    snprintf(action_label, sizeof(action_label), "%s", visible ? "Hide Object" : "Show Object");
    sandbox3d_draw_section_heading(state->ui, panel_bounds.x + 14.0f, compact_mode ? panel_bounds.y + 308.0f : panel_bounds.y + 356.0f, "Actions");
    if (henka_ui_button(state->ui, "toggle_selected_visibility", (henka_ui_rect){panel_bounds.x + 14.0f, compact_mode ? panel_bounds.y + 326.0f : panel_bounds.y + 374.0f, action_button_width, 28.0f}, action_label))
    {
        if (sandbox3d_toggle_selected_entity_visibility(state))
        {
            sandbox3d_set_statusf(state, false, true, "%s visibility updated.", descriptor->display_name);
        }
        else
        {
            sandbox3d_set_statusf(state, true, true, "%s visibility could not be changed.", descriptor->display_name);
        }
    }

    if (henka_ui_primary_button(state->ui, "focus_selected_camera", (henka_ui_rect){panel_bounds.x + 28.0f + action_button_width, compact_mode ? panel_bounds.y + 326.0f : panel_bounds.y + 374.0f, action_button_width, 28.0f}, "Focus Camera"))
    {
        if (sandbox3d_focus_camera_on_selected(state))
        {
            sandbox3d_set_statusf(state, false, true, "Focused camera on %s.", descriptor->display_name);
        }
        else
        {
            sandbox3d_set_statusf(state, true, true, "Camera could not focus on %s.", descriptor->display_name);
        }
    }

    if (henka_ui_button(state->ui, "reset_selected_transform", (henka_ui_rect){panel_bounds.x + 14.0f, compact_mode ? panel_bounds.y + 360.0f : panel_bounds.y + 402.0f, action_button_width, 28.0f}, "Reset Transform"))
    {
        if (sandbox3d_reset_selected_entity_transform(state))
        {
            sandbox3d_set_statusf(state, false, true, "%s reset to its default transform.", descriptor->display_name);
        }
        else
        {
            sandbox3d_set_statusf(state, true, true, "%s could not be reset.", descriptor->display_name);
        }
    }

    if (henka_ui_button(state->ui, "clear_selection", (henka_ui_rect){panel_bounds.x + 28.0f + action_button_width, compact_mode ? panel_bounds.y + 360.0f : panel_bounds.y + 402.0f, action_button_width, 28.0f}, "Clear Selection"))
    {
        sandbox3d_clear_selection(state, "Selection cleared from Object Details.");
    }

}

static void sandbox3d_draw_utility_panel(
    henka_engine* engine,
    sandbox3d_state* state,
    const sandbox3d_workspace_layout* layout,
    const char* asset_path_text,
    const char* user_path_text,
    const char* settings_path_text,
    const char* save_path_text,
    const char* capture_text,
    const char* fps_text,
    int framebuffer_width,
    int framebuffer_height)
{
    char detail_text[96];
    char diag_text[96];
    char package_text[48];
    char row_value[96];
    const henka_asset_manager* assets;
    const sandbox3d_object_descriptor* descriptor;
    float button_width;
    henka_engine_diagnostics diagnostics;
    float x_left;
    float y_start;
    henka_transform transform;
    henka_ui_rect panel_bounds;

    if (engine == NULL || state == NULL || layout == NULL || !sandbox3d_workspace_shows_utility_panel(state))
    {
        return;
    }

    panel_bounds = layout->utility_panel;
    if (panel_bounds.width <= 0.0f || panel_bounds.height <= 0.0f)
    {
        return;
    }
    x_left = panel_bounds.x + 14.0f;
    button_width = (panel_bounds.width - 44.0f) / 3.0f;
    y_start = panel_bounds.y + 38.0f;
    assets = henka_engine_get_asset_manager_const(engine);
    if (henka_engine_get_diagnostics(engine, &diagnostics) != HENKA_SUCCESS)
    {
        memset(&diagnostics, 0, sizeof(diagnostics));
    }
    snprintf(package_text, sizeof(package_text), "%s", henka_engine_get_package_mode_label(diagnostics.package_mode));

    henka_ui_panel(state->ui, panel_bounds, "Utility");
    sandbox3d_draw_panel_workspace_controls(engine, state, layout, SANDBOX3D_WORKSPACE_PANEL_UTILITY);
    if (henka_ui_tab(state->ui, "utility_tab_help", (henka_ui_rect){x_left, y_start, button_width, 24.0f}, "Help", state->workspace.active_utility == SANDBOX3D_UTILITY_HELP))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_HELP);
    }
    if (henka_ui_tab(state->ui, "utility_tab_legend", (henka_ui_rect){x_left + button_width + 8.0f, y_start, button_width, 24.0f}, "Legend", state->workspace.active_utility == SANDBOX3D_UTILITY_SCENE_LEGEND))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_SCENE_LEGEND);
    }
    if (henka_ui_tab(state->ui, "utility_tab_info", (henka_ui_rect){x_left + (button_width + 8.0f) * 2.0f, y_start, button_width, 24.0f}, "Info", state->workspace.active_utility == SANDBOX3D_UTILITY_OBJECT_INFO))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_OBJECT_INFO);
    }
    if (henka_ui_tab(state->ui, "utility_tab_paths", (henka_ui_rect){x_left, y_start + 30.0f, button_width, 24.0f}, "Paths", state->workspace.active_utility == SANDBOX3D_UTILITY_PATHS))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_PATHS);
    }
    if (henka_ui_tab(state->ui, "utility_tab_settings", (henka_ui_rect){x_left + button_width + 8.0f, y_start + 30.0f, button_width, 24.0f}, "Settings", state->workspace.active_utility == SANDBOX3D_UTILITY_SETTINGS))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_SETTINGS);
    }
    if (henka_ui_tab(state->ui, "utility_tab_diag", (henka_ui_rect){x_left + (button_width + 8.0f) * 2.0f, y_start + 30.0f, button_width, 24.0f}, "Diag", state->workspace.active_utility == SANDBOX3D_UTILITY_DIAGNOSTICS))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_DIAGNOSTICS);
    }
    if (henka_ui_tab(state->ui, "utility_tab_transform_qa", (henka_ui_rect){x_left, y_start + 60.0f, button_width, 24.0f}, "T QA", state->workspace.active_utility == SANDBOX3D_UTILITY_TRANSFORM_QA))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_TRANSFORM_QA);
    }
    if (henka_ui_tab(state->ui, "utility_tab_physics_qa", (henka_ui_rect){x_left + button_width + 8.0f, y_start + 60.0f, button_width, 24.0f}, "Physics", state->workspace.active_utility == SANDBOX3D_UTILITY_PHYSICS_QA))
    {
        sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_PHYSICS_QA);
    }

    y_start = panel_bounds.y + 126.0f;
    switch (state->workspace.active_utility)
    {
        case SANDBOX3D_UTILITY_HELP:
            sandbox3d_draw_section_heading(state->ui, x_left, y_start, "Viewer help");
            henka_ui_label(state->ui, x_left, y_start + 18.0f, 1.0f, "Select, Orbit, Pan, Move, Rotate, and Scale are explicit tools.");
            henka_ui_label(state->ui, x_left, y_start + 34.0f, 1.0f, "Left drag uses the selected viewport tool when capture is released.");
            henka_ui_label(state->ui, x_left, y_start + 50.0f, 1.0f, "Wheel zooms over the viewport and pages over docked panels.");
            henka_ui_label(state->ui, x_left, y_start + 66.0f, 1.0f, "Transform QA confirms Action API object mutation without gizmo hits.");
            henka_ui_label(state->ui, x_left, y_start + 82.0f, 1.0f, "Diagnostics show input ownership, tool state, and last failures.");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 112.0f, panel_bounds.width - 28.0f, "Status", "Use T QA if gizmo input still fails.");
            break;

        case SANDBOX3D_UTILITY_SCENE_LEGEND:
            sandbox3d_draw_section_heading(state->ui, x_left, y_start, "Scene legend");
            henka_ui_label(state->ui, x_left, y_start + 18.0f, 1.0f, "Ground: textured plane under the scene.");
            henka_ui_label(state->ui, x_left, y_start + 34.0f, 1.0f, "Textured Cube: texture material rendering.");
            henka_ui_label(state->ui, x_left, y_start + 50.0f, 1.0f, "Colored Cube: untextured base color.");
            henka_ui_label(state->ui, x_left, y_start + 66.0f, 1.0f, "OBJ Marker: current OBJ mesh loading path.");
            henka_ui_label(state->ui, x_left, y_start + 82.0f, 1.0f, "Missing Texture and Missing Model show fallback behavior.");
            break;

        case SANDBOX3D_UTILITY_OBJECT_INFO:
            descriptor = sandbox3d_get_selected_descriptor(state);
            if (descriptor == NULL || state->scene == NULL || henka_scene_get_entity_transform(state->scene, descriptor->entity, &transform) != HENKA_SUCCESS)
            {
                henka_ui_label(state->ui, x_left, y_start, 1.0f, "Select an object to inspect it here.");
                break;
            }

            sandbox3d_draw_section_heading(state->ui, x_left, y_start, descriptor->display_name);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 18.0f, panel_bounds.width - 28.0f, "Visible", henka_scene_is_entity_visible(state->scene, descriptor->entity) ? "Yes" : "No");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 44.0f, panel_bounds.width - 28.0f, "Shows", descriptor->short_explanation);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 70.0f, panel_bounds.width - 28.0f, "Detail", descriptor->developer_detail);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 96.0f, panel_bounds.width - 28.0f, "Mesh", descriptor->mesh_summary);
            snprintf(row_value, sizeof(row_value), "%.2f %.2f %.2f", transform.position.x, transform.position.y, transform.position.z);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 122.0f, panel_bounds.width - 28.0f, "Position", row_value);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 148.0f, panel_bounds.width - 28.0f, "Tag", henka_scene_get_entity_tag(state->scene, descriptor->entity) != NULL ? henka_scene_get_entity_tag(state->scene, descriptor->entity) : "(none)");
            break;

        case SANDBOX3D_UTILITY_PATHS:
            sandbox3d_draw_section_heading(state->ui, x_left, y_start, "Runtime paths");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 18.0f, panel_bounds.width - 28.0f, "Assets", asset_path_text + 8);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 44.0f, panel_bounds.width - 28.0f, "User", user_path_text + 6);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 70.0f, panel_bounds.width - 28.0f, "Settings", settings_path_text + 10);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 96.0f, panel_bounds.width - 28.0f, "Save", save_path_text + 6);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 122.0f, panel_bounds.width - 28.0f, "Mode", package_text);
            break;

        case SANDBOX3D_UTILITY_SETTINGS:
            descriptor = sandbox3d_get_selected_descriptor(state);
            sandbox3d_draw_section_heading(state->ui, x_left, y_start, "Sandbox settings");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 18.0f, panel_bounds.width - 28.0f, "Layout", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 44.0f, panel_bounds.width - 28.0f, "Selected", descriptor != NULL ? descriptor->display_name : "(none)");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 70.0f, panel_bounds.width - 28.0f, "Grid", henka_scene_is_entity_visible(state->scene, state->grid_entity) ? "Visible" : "Hidden");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 96.0f, panel_bounds.width - 28.0f, "Wireframe", henka_engine_is_wireframe_enabled(engine) ? "On" : "Off");
            snprintf(row_value, sizeof(row_value), "%.4f  /  %.1f", sandbox3d_get_mouse_sensitivity(state), state->camera.movement_speed);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 122.0f, panel_bounds.width - 28.0f, "Sense/Speed", row_value);
            if (henka_ui_button(state->ui, "utility_mouse_less", (henka_ui_rect){x_left, y_start + 152.0f, 60.0f, 24.0f}, "Sense-"))
            {
                sandbox3d_adjust_mouse_sensitivity(state, -0.0005f);
            }
            if (henka_ui_button(state->ui, "utility_mouse_more", (henka_ui_rect){x_left + 68.0f, y_start + 152.0f, 60.0f, 24.0f}, "Sense+"))
            {
                sandbox3d_adjust_mouse_sensitivity(state, 0.0005f);
            }
            if (henka_ui_button(state->ui, "utility_speed_less", (henka_ui_rect){x_left + 146.0f, y_start + 152.0f, 60.0f, 24.0f}, "Speed-"))
            {
                sandbox3d_adjust_camera_speed(state, -0.5f);
            }
            if (henka_ui_button(state->ui, "utility_speed_more", (henka_ui_rect){x_left + 214.0f, y_start + 152.0f, 60.0f, 24.0f}, "Speed+"))
            {
                sandbox3d_adjust_camera_speed(state, 0.5f);
            }
            break;

        case SANDBOX3D_UTILITY_DIAGNOSTICS:
        {
            size_t contact_count = 0U;
            if (state->physics.world != NULL)
            {
                (void)henka_physics_world_get_contacts(state->physics.world, &contact_count);
            }
            descriptor = sandbox3d_get_selected_descriptor(state);
            sandbox3d_draw_section_heading(state->ui, x_left, y_start, "Diagnostics");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 18.0f, panel_bounds.width - 28.0f, "Frame", fps_text + 7);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 44.0f, panel_bounds.width - 28.0f, "Layout", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 70.0f, panel_bounds.width - 28.0f, "Tool", sandbox3d_viewport_tool_mode_to_string(state->viewport_tool));
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 96.0f, panel_bounds.width - 28.0f, "Gizmo", sandbox3d_get_gizmo_mode_label(state->gizmo.mode));
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 122.0f, panel_bounds.width - 28.0f, "Capture", capture_text + 9);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 148.0f, panel_bounds.width - 28.0f, "UI Mouse", state->diagnostics.ui_wants_mouse ? "Yes" : "No");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 174.0f, panel_bounds.width - 28.0f, "In View", state->diagnostics.cursor_in_viewport ? "Yes" : "No");
            snprintf(row_value, sizeof(row_value), "%.0f, %.0f", state->diagnostics.window_mouse.x, state->diagnostics.window_mouse.y);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 200.0f, panel_bounds.width - 28.0f, "Mouse W", row_value);
            snprintf(row_value, sizeof(row_value), "%.0f, %.0f", state->diagnostics.framebuffer_mouse.x, state->diagnostics.framebuffer_mouse.y);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 226.0f, panel_bounds.width - 28.0f, "Mouse FB", row_value);
            snprintf(row_value, sizeof(row_value), "%.0f, %.0f", state->diagnostics.viewport_local.x, state->diagnostics.viewport_local.y);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 252.0f, panel_bounds.width - 28.0f, "View XY", state->diagnostics.viewport_local_valid ? row_value : "(out)");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 278.0f, panel_bounds.width - 28.0f, "Selected", descriptor != NULL ? descriptor->display_name : "(none)");
            snprintf(
                row_value,
                sizeof(row_value),
                "%u / %s / %s / %s / HL:%s",
                (unsigned int)state->diagnostics.selected_entity,
                state->diagnostics.selected_entity_valid ? "Valid" : "Invalid",
                state->diagnostics.selected_bounds_valid ? "Bounds" : "NoBounds",
                state->diagnostics.selected_entity_selectable ? "Selectable" : "No",
                state->diagnostics.selected_highlight_active ? "On" : "Off");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 304.0f, panel_bounds.width - 28.0f, "Entity", row_value);
            snprintf(row_value, sizeof(row_value), "%s / %zu", state->diagnostics.gizmo_model_valid ? "Valid" : "Invalid", state->diagnostics.overlay_primitive_count);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 330.0f, panel_bounds.width - 28.0f, "Gizmo", row_value);
            snprintf(row_value, sizeof(row_value), "%s / %s", sandbox3d_get_gizmo_axis_label(state->gizmo.hover_axis), state->gizmo.hover_handle_type == SANDBOX3D_GIZMO_HANDLE_NONE ? "None" : (state->gizmo.hover_handle_type == SANDBOX3D_GIZMO_HANDLE_ROTATE_RING ? "Ring" : (state->gizmo.hover_handle_type == SANDBOX3D_GIZMO_HANDLE_SCALE_UNIFORM ? "Scale" : "Handle")));
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 356.0f, panel_bounds.width - 28.0f, "Hover", row_value);
            snprintf(
                row_value,
                sizeof(row_value),
                "%s / %s / %u",
                state->diagnostics.dragging ? "Yes" : "No",
                state->gizmo.active_handle_type == SANDBOX3D_GIZMO_HANDLE_NONE ? "None" : (state->gizmo.active_handle_type == SANDBOX3D_GIZMO_HANDLE_ROTATE_RING ? "Ring" : (state->gizmo.active_handle_type == SANDBOX3D_GIZMO_HANDLE_SCALE_UNIFORM ? "Scale" : "Handle")),
                (unsigned int)state->diagnostics.drag_target_entity);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 382.0f, panel_bounds.width - 28.0f, "Drag", row_value);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 408.0f, panel_bounds.width - 28.0f, "Reject", sandbox3d_interaction_reject_reason_to_string(state->diagnostics.last_reject_reason));
            snprintf(
                row_value,
                sizeof(row_value),
                "%s H:%s M:%s",
                sandbox3d_workspace_panel_name(state->diagnostics.hovered_panel),
                state->diagnostics.cursor_in_panel_header ? "Yes" : "No",
                sandbox3d_workspace_panel_name(state->diagnostics.active_panel_drag));
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 434.0f, panel_bounds.width - 28.0f, "Panel/Hdr", row_value);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 460.0f, panel_bounds.width - 28.0f, "Action", state->diagnostics.last_action_command);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 486.0f, panel_bounds.width - 28.0f, "Result", state->diagnostics.last_action_result);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 512.0f, panel_bounds.width - 28.0f, "Selection", state->diagnostics.last_selection_action);
            snprintf(
                row_value,
                sizeof(row_value),
                "%s / %s / G:%s / %zu contacts",
                state->physics.enabled ? "Enabled" : "Off",
                state->physics.paused ? "Paused" : "Running",
                state->physics.gravity_enabled ? "On" : "Off",
                contact_count);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 538.0f, panel_bounds.width - 28.0f, "Physics", row_value);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 564.0f, panel_bounds.width - 28.0f, "Workspace", state->diagnostics.last_workspace_action);
            if (state->native_panel_window_id != HENKA_INVALID_WINDOW_ID)
            {
                henka_tool_window_state native_state;
                if (henka_engine_get_tool_window_state(engine, state->native_panel_window_id, &native_state) == HENKA_SUCCESS)
                {
                    snprintf(
                        row_value,
                        sizeof(row_value),
                        "Open %u %s %dx%d",
                        (unsigned int)native_state.id,
                        native_state.focused ? "Focus" : "Idle",
                        native_state.width,
                        native_state.height);
                }
                else
                {
                    snprintf(row_value, sizeof(row_value), "Closed | Route %s", henka_window_event_route_to_string(diagnostics.last_window_event_route));
                }
            }
            else
            {
                snprintf(row_value, sizeof(row_value), "Closed | Route %s", henka_window_event_route_to_string(diagnostics.last_window_event_route));
            }
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 590.0f, panel_bounds.width - 28.0f, "Native", row_value);
            break;
        }

        case SANDBOX3D_UTILITY_TRANSFORM_QA:
            descriptor = sandbox3d_get_selected_descriptor(state);
            sandbox3d_draw_section_heading(state->ui, x_left, y_start, "Transform QA");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 18.0f, panel_bounds.width - 28.0f, "Selected", descriptor != NULL ? descriptor->display_name : "(none)");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 44.0f, panel_bounds.width - 28.0f, "Move Step", state->gizmo.snap.enabled ? "Snap move" : "0.25 units");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 70.0f, panel_bounds.width - 28.0f, "Rotate Step", state->gizmo.snap.enabled ? "Snap rotate" : "15 degrees");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 96.0f, panel_bounds.width - 28.0f, "Scale Step", state->gizmo.snap.enabled ? "Snap scale" : "0.10");
            if (henka_ui_button(state->ui, "qa_move_x_minus", (henka_ui_rect){x_left, y_start + 126.0f, 72.0f, 24.0f}, "X-"))
            {
                sandbox3d_apply_move_step(state, HENKA_GIZMO_AXIS_X, -(state->gizmo.snap.enabled ? state->gizmo.snap.move_snap_increment : 0.25f));
            }
            if (henka_ui_button(state->ui, "qa_move_x_plus", (henka_ui_rect){x_left + 78.0f, y_start + 126.0f, 72.0f, 24.0f}, "X+"))
            {
                sandbox3d_apply_move_step(state, HENKA_GIZMO_AXIS_X, state->gizmo.snap.enabled ? state->gizmo.snap.move_snap_increment : 0.25f);
            }
            if (henka_ui_button(state->ui, "qa_move_y_minus", (henka_ui_rect){x_left + 156.0f, y_start + 126.0f, 72.0f, 24.0f}, "Y-"))
            {
                sandbox3d_apply_move_step(state, HENKA_GIZMO_AXIS_Y, -(state->gizmo.snap.enabled ? state->gizmo.snap.move_snap_increment : 0.25f));
            }
            if (henka_ui_button(state->ui, "qa_move_y_plus", (henka_ui_rect){x_left + 234.0f, y_start + 126.0f, 72.0f, 24.0f}, "Y+"))
            {
                sandbox3d_apply_move_step(state, HENKA_GIZMO_AXIS_Y, state->gizmo.snap.enabled ? state->gizmo.snap.move_snap_increment : 0.25f);
            }
            if (henka_ui_button(state->ui, "qa_move_z_minus", (henka_ui_rect){x_left, y_start + 156.0f, 72.0f, 24.0f}, "Z-"))
            {
                sandbox3d_apply_move_step(state, HENKA_GIZMO_AXIS_Z, -(state->gizmo.snap.enabled ? state->gizmo.snap.move_snap_increment : 0.25f));
            }
            if (henka_ui_button(state->ui, "qa_move_z_plus", (henka_ui_rect){x_left + 78.0f, y_start + 156.0f, 72.0f, 24.0f}, "Z+"))
            {
                sandbox3d_apply_move_step(state, HENKA_GIZMO_AXIS_Z, state->gizmo.snap.enabled ? state->gizmo.snap.move_snap_increment : 0.25f);
            }
            if (henka_ui_button(state->ui, "qa_rotate_x_minus", (henka_ui_rect){x_left + 156.0f, y_start + 156.0f, 72.0f, 24.0f}, "Rx-"))
            {
                sandbox3d_apply_rotate_step(state, HENKA_GIZMO_AXIS_X, -(state->gizmo.snap.enabled ? state->gizmo.snap.rotate_snap_increment : 15.0f * HENKA_DEG_TO_RAD));
            }
            if (henka_ui_button(state->ui, "qa_rotate_x_plus", (henka_ui_rect){x_left + 234.0f, y_start + 156.0f, 72.0f, 24.0f}, "Rx+"))
            {
                sandbox3d_apply_rotate_step(state, HENKA_GIZMO_AXIS_X, state->gizmo.snap.enabled ? state->gizmo.snap.rotate_snap_increment : 15.0f * HENKA_DEG_TO_RAD);
            }
            if (henka_ui_button(state->ui, "qa_rotate_y_minus", (henka_ui_rect){x_left, y_start + 186.0f, 72.0f, 24.0f}, "Ry-"))
            {
                sandbox3d_apply_rotate_step(state, HENKA_GIZMO_AXIS_Y, -(state->gizmo.snap.enabled ? state->gizmo.snap.rotate_snap_increment : 15.0f * HENKA_DEG_TO_RAD));
            }
            if (henka_ui_button(state->ui, "qa_rotate_y_plus", (henka_ui_rect){x_left + 78.0f, y_start + 186.0f, 72.0f, 24.0f}, "Ry+"))
            {
                sandbox3d_apply_rotate_step(state, HENKA_GIZMO_AXIS_Y, state->gizmo.snap.enabled ? state->gizmo.snap.rotate_snap_increment : 15.0f * HENKA_DEG_TO_RAD);
            }
            if (henka_ui_button(state->ui, "qa_rotate_z_minus", (henka_ui_rect){x_left + 156.0f, y_start + 186.0f, 72.0f, 24.0f}, "Rz-"))
            {
                sandbox3d_apply_rotate_step(state, HENKA_GIZMO_AXIS_Z, -(state->gizmo.snap.enabled ? state->gizmo.snap.rotate_snap_increment : 15.0f * HENKA_DEG_TO_RAD));
            }
            if (henka_ui_button(state->ui, "qa_rotate_z_plus", (henka_ui_rect){x_left + 234.0f, y_start + 186.0f, 72.0f, 24.0f}, "Rz+"))
            {
                sandbox3d_apply_rotate_step(state, HENKA_GIZMO_AXIS_Z, state->gizmo.snap.enabled ? state->gizmo.snap.rotate_snap_increment : 15.0f * HENKA_DEG_TO_RAD);
            }
            if (henka_ui_button(state->ui, "qa_scale_down", (henka_ui_rect){x_left, y_start + 216.0f, 90.0f, 24.0f}, "Scale-"))
            {
                sandbox3d_apply_scale_step(state, -(state->gizmo.snap.enabled ? state->gizmo.snap.scale_snap_increment : 0.10f));
            }
            if (henka_ui_button(state->ui, "qa_scale_up", (henka_ui_rect){x_left + 96.0f, y_start + 216.0f, 90.0f, 24.0f}, "Scale+"))
            {
                sandbox3d_apply_scale_step(state, state->gizmo.snap.enabled ? state->gizmo.snap.scale_snap_increment : 0.10f);
            }
            if (henka_ui_button(state->ui, "qa_reset_transform", (henka_ui_rect){x_left + 192.0f, y_start + 216.0f, 114.0f, 24.0f}, "Reset"))
            {
                sandbox3d_apply_transform_action(state, HENKA_ACTION_COMMAND_RESET_TRANSFORM, sandbox3d_get_real_selected_entity(state), (henka_vec3){0.0f, 0.0f, 0.0f}, henka_quat_identity());
            }
            if (henka_ui_primary_button(state->ui, "qa_test_move", (henka_ui_rect){x_left, y_start + 250.0f, 90.0f, 24.0f}, "Test Move"))
            {
                sandbox3d_apply_move_step(state, HENKA_GIZMO_AXIS_X, state->gizmo.snap.enabled ? state->gizmo.snap.move_snap_increment : 0.25f);
            }
            if (henka_ui_primary_button(state->ui, "qa_test_rotate", (henka_ui_rect){x_left + 96.0f, y_start + 250.0f, 90.0f, 24.0f}, "Test Rotate"))
            {
                sandbox3d_apply_rotate_step(state, HENKA_GIZMO_AXIS_Y, state->gizmo.snap.enabled ? state->gizmo.snap.rotate_snap_increment : 15.0f * HENKA_DEG_TO_RAD);
            }
            if (henka_ui_primary_button(state->ui, "qa_test_scale", (henka_ui_rect){x_left + 192.0f, y_start + 250.0f, 114.0f, 24.0f}, "Test Scale"))
            {
                sandbox3d_apply_scale_step(state, state->gizmo.snap.enabled ? state->gizmo.snap.scale_snap_increment : 0.10f);
            }
            if (henka_ui_button(state->ui, "qa_reset_test_object", (henka_ui_rect){x_left, y_start + 280.0f, 140.0f, 24.0f}, "Reset Test Object"))
            {
                sandbox3d_apply_transform_action(state, HENKA_ACTION_COMMAND_RESET_TRANSFORM, sandbox3d_get_real_selected_entity(state), (henka_vec3){0.0f, 0.0f, 0.0f}, henka_quat_identity());
            }
            break;

        case SANDBOX3D_UTILITY_PHYSICS_QA:
        {
            henka_physics_body_id selected_body = sandbox3d_get_physics_body_for_entity(state, sandbox3d_get_real_selected_entity(state));
            henka_physics_body_state body_state;
            const char* mode_hint;
            bool has_body = selected_body != HENKA_INVALID_PHYSICS_BODY_ID &&
                henka_physics_body_get_state(state->physics.world, selected_body, &body_state) == HENKA_SUCCESS;
            size_t contact_count = 0U;
            size_t event_count = 0U;
            const henka_physics_event* events = henka_physics_world_get_events(state->physics.world, &event_count);
            (void)henka_physics_world_get_contacts(state->physics.world, &contact_count);
            sandbox3d_draw_section_heading(state->ui, x_left, y_start, "Rigid-body Physics QA");
            mode_hint = "Select a physics body. Enable physics to run the demo.";
            if (has_body && body_state.type == HENKA_PHYSICS_BODY_STATIC)
            {
                mode_hint = "Static: no gravity, forces, or impulses.";
            }
            else if (has_body && body_state.type == HENKA_PHYSICS_BODY_DYNAMIC)
            {
                mode_hint = "Dynamic: falls and responds to physics.";
            }
            else if (has_body && body_state.type == HENKA_PHYSICS_BODY_KINEMATIC)
            {
                mode_hint = "Kinematic: tool/code movement only; no gravity fall.";
            }
            snprintf(row_value, sizeof(row_value), "%s / %s / %.4f s", state->physics.enabled ? "Enabled" : "Off", state->physics.paused ? "Paused" : "Running", henka_physics_world_get_fixed_timestep(state->physics.world));
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 18.0f, panel_bounds.width - 28.0f, "World", row_value);
            snprintf(row_value, sizeof(row_value), "%zu bodies / %zu contacts / %zu events", henka_physics_world_get_body_count(state->physics.world), contact_count, event_count);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 44.0f, panel_bounds.width - 28.0f, "State", row_value);
            if (has_body)
            {
                snprintf(row_value, sizeof(row_value), "%u | %s | %s", (unsigned int)selected_body, henka_physics_body_type_get_label(body_state.type), henka_physics_shape_type_get_label(body_state.collider.shape));
            }
            else
            {
                snprintf(row_value, sizeof(row_value), "(none) | Select sample");
            }
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 70.0f, panel_bounds.width - 28.0f, "Selected Body", row_value);
            if (has_body)
            {
                snprintf(row_value, sizeof(row_value), "%.2f %.2f %.2f", body_state.linear_velocity.x, body_state.linear_velocity.y, body_state.linear_velocity.z);
            }
            else
            {
                snprintf(row_value, sizeof(row_value), "(none)");
            }
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 96.0f, panel_bounds.width - 28.0f, "Velocity", row_value);
            if (has_body)
            {
                snprintf(row_value, sizeof(row_value), "%.2f %.2f %.2f", body_state.angular_velocity.x, body_state.angular_velocity.y, body_state.angular_velocity.z);
            }
            else
            {
                snprintf(row_value, sizeof(row_value), "(none)");
            }
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 122.0f, panel_bounds.width - 28.0f, "Angular", row_value);
            if (has_body)
            {
                snprintf(row_value, sizeof(row_value), "m %.1f | r %.2f | f %.2f | %s", body_state.mass, body_state.material.restitution, body_state.material.dynamic_friction, body_state.grounded ? "Grounded" : (body_state.colliding ? "Contact" : "Free"));
            }
            else
            {
                snprintf(row_value, sizeof(row_value), "(none)");
            }
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 148.0f, panel_bounds.width - 28.0f, "Material", row_value);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 174.0f, panel_bounds.width - 28.0f, "Body Rule", mode_hint);
            if (henka_ui_primary_button(state->ui, "physics_enable", (henka_ui_rect){x_left, y_start + 206.0f, 92.0f, 24.0f}, "Enable"))
            {
                sandbox3d_prepare_physics_demo(state);
                state->physics.enabled = true;
                state->physics.paused = false;
                sandbox3d_set_status(state, false, "Physics enabled. Demo bodies are running.");
            }
            if (henka_ui_button(state->ui, "physics_pause_resume", (henka_ui_rect){x_left + 100.0f, y_start + 206.0f, 92.0f, 24.0f}, state->physics.paused ? "Resume" : "Pause"))
            {
                state->physics.enabled = true;
                state->physics.paused = !state->physics.paused;
                snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Simulation %s", state->physics.paused ? "paused" : "resumed");
            }
            if (henka_ui_button(state->ui, "physics_step", (henka_ui_rect){x_left + 200.0f, y_start + 206.0f, 106.0f, 24.0f}, "Step"))
            {
                state->physics.enabled = true;
                state->physics.paused = true;
                (void)henka_physics_world_step_fixed(state->physics.world);
                snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Advanced one fixed step");
            }
            if (henka_ui_button(state->ui, "physics_reset", (henka_ui_rect){x_left, y_start + 236.0f, 92.0f, 24.0f}, "Reset Demo"))
            {
                sandbox3d_prepare_physics_demo(state);
                state->physics.enabled = true;
                state->physics.paused = true;
                sandbox3d_set_status(state, false, "Physics demo reset and paused.");
            }
            if (henka_ui_toggle(state->ui, "physics_gravity", (henka_ui_rect){x_left + 100.0f, y_start + 236.0f, 92.0f, 24.0f}, "Gravity", &state->physics.gravity_enabled))
            {
                (void)henka_physics_world_set_gravity(state->physics.world, state->physics.gravity_enabled ? (henka_vec3){0.0f, -9.81f, 0.0f} : (henka_vec3){0.0f, 0.0f, 0.0f});
                snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Gravity %s", state->physics.gravity_enabled ? "enabled" : "disabled");
            }
            if (henka_ui_toggle(state->ui, "physics_colliders", (henka_ui_rect){x_left + 200.0f, y_start + 236.0f, 106.0f, 24.0f}, "Colliders", &state->physics.debug_colliders))
            {
                snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Collider debug %s", state->physics.debug_colliders ? "shown" : "hidden");
            }
            if (henka_ui_toggle(state->ui, "physics_contacts", (henka_ui_rect){x_left, y_start + 266.0f, 92.0f, 24.0f}, "Contacts", &state->physics.debug_contacts))
            {
                state->physics.debug_colliders = state->physics.debug_contacts || state->physics.debug_colliders;
            }
            if (henka_ui_button(state->ui, "physics_impulse_up", (henka_ui_rect){x_left + 100.0f, y_start + 266.0f, 92.0f, 24.0f}, "Impulse Up"))
            {
                if (has_body && henka_physics_body_apply_impulse(state->physics.world, selected_body, (henka_vec3){0.0f, 5.5f, 0.0f}) == HENKA_SUCCESS)
                {
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Applied upward impulse to body %u", (unsigned int)selected_body);
                }
                else
                {
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Impulse rejected: select a dynamic physics body");
                }
            }
            if (henka_ui_button(state->ui, "physics_clear_velocity", (henka_ui_rect){x_left + 200.0f, y_start + 266.0f, 106.0f, 24.0f}, "Clear Velocity"))
            {
                if (has_body)
                {
                    (void)henka_physics_body_clear_velocity(state->physics.world, selected_body);
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Cleared velocity for body %u", (unsigned int)selected_body);
                }
                else
                {
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Clear rejected: no selected physics body");
                }
            }
            if (henka_ui_button(state->ui, "physics_impulse_forward", (henka_ui_rect){x_left, y_start + 296.0f, 120.0f, 24.0f}, "Impulse Forward"))
            {
                henka_vec3 forward = henka_camera_get_forward(&state->camera);
                forward.y = 0.0f;
                forward = henka_vec3_normalize(forward);
                if (has_body && henka_physics_body_apply_impulse(state->physics.world, selected_body, henka_vec3_scale(forward, 4.0f)) == HENKA_SUCCESS)
                {
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Applied forward impulse to body %u", (unsigned int)selected_body);
                }
                else
                {
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Impulse rejected: select a dynamic physics body");
                }
            }
            if (henka_ui_primary_button(state->ui, "physics_make_dynamic_drop", (henka_ui_rect){x_left + 128.0f, y_start + 296.0f, 178.0f, 24.0f}, "Make Dynamic + Drop"))
            {
                if (has_body &&
                    henka_physics_body_set_type(state->physics.world, selected_body, HENKA_PHYSICS_BODY_DYNAMIC) == HENKA_SUCCESS)
                {
                    state->physics.enabled = true;
                    state->physics.paused = false;
                    state->physics.gravity_enabled = true;
                    (void)henka_physics_world_set_gravity(state->physics.world, (henka_vec3){0.0f, -9.81f, 0.0f});
                    (void)henka_physics_body_clear_velocity(state->physics.world, selected_body);
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Body %u set Dynamic; gravity running", (unsigned int)selected_body);
                    sandbox3d_set_status(state, false, "Selected physics body is Dynamic with gravity running.");
                }
                else
                {
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Drop rejected: select a non-plane physics body");
                    sandbox3d_set_status(state, true, "Drop rejected: select a supported physics body.");
                }
            }
            if (has_body && henka_ui_tab(state->ui, "physics_static", (henka_ui_rect){x_left, y_start + 326.0f, 92.0f, 24.0f}, "Static", body_state.type == HENKA_PHYSICS_BODY_STATIC))
            {
                (void)henka_physics_body_set_type(state->physics.world, selected_body, HENKA_PHYSICS_BODY_STATIC);
                snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Body %u set Static", (unsigned int)selected_body);
            }
            if (has_body && henka_ui_tab(state->ui, "physics_dynamic", (henka_ui_rect){x_left + 100.0f, y_start + 326.0f, 92.0f, 24.0f}, "Dynamic", body_state.type == HENKA_PHYSICS_BODY_DYNAMIC))
            {
                if (henka_physics_body_set_type(state->physics.world, selected_body, HENKA_PHYSICS_BODY_DYNAMIC) == HENKA_SUCCESS)
                {
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Body %u set Dynamic", (unsigned int)selected_body);
                }
                else
                {
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Dynamic rejected for this collider");
                }
            }
            if (has_body && henka_ui_tab(state->ui, "physics_kinematic", (henka_ui_rect){x_left + 200.0f, y_start + 326.0f, 106.0f, 24.0f}, "Kinematic", body_state.type == HENKA_PHYSICS_BODY_KINEMATIC))
            {
                if (henka_physics_body_set_type(state->physics.world, selected_body, HENKA_PHYSICS_BODY_KINEMATIC) == HENKA_SUCCESS)
                {
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Body %u set Kinematic", (unsigned int)selected_body);
                }
                else
                {
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Kinematic rejected for this collider");
                }
            }
            if (henka_ui_button(state->ui, "physics_raycast", (henka_ui_rect){x_left, y_start + 356.0f, 120.0f, 24.0f}, "Camera Raycast"))
            {
                henka_ray ray = {state->camera.position, henka_camera_get_forward(&state->camera)};
                (void)henka_physics_world_raycast(state->physics.world, ray, 100.0f, HENKA_PHYSICS_ALL_LAYERS, &state->physics.last_raycast);
                if (state->physics.last_raycast.hit)
                {
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Ray hit body %u at %.2f", (unsigned int)state->physics.last_raycast.body, state->physics.last_raycast.distance);
                }
                else
                {
                    snprintf(state->physics.last_action, sizeof(state->physics.last_action), "Raycast missed");
                }
            }
            snprintf(row_value, sizeof(row_value), "%s", events != NULL && event_count > 0U ? henka_physics_event_type_get_label(events[event_count - 1U].type) : "(none)");
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 390.0f, panel_bounds.width - 28.0f, "Latest Event", row_value);
            sandbox3d_draw_value_row(state->ui, x_left, y_start + 416.0f, panel_bounds.width - 28.0f, "Last Action", state->physics.last_action);
            break;
        }

        case SANDBOX3D_UTILITY_NONE:
        default:
            break;
    }
}

static void sandbox3d_build_ui(henka_engine* engine, sandbox3d_state* state)
{
    char asset_path_text[128];
    char capture_text[64];
    char fps_text[56];
    char save_path_text[128];
    char settings_path_text[128];
    char user_path_text[128];
    char* save_path;
    char* settings_path;
    float fps;
    float milliseconds;
    int panel_index;
    henka_result result;
    henka_ui_frame_desc frame_desc;
    sandbox3d_workspace_layout layout;
    const sandbox3d_workspace_panel* workspace_panel;
    unsigned int z_order;

    if (engine == NULL || state == NULL || state->ui == NULL)
    {
        return;
    }

    frame_desc.framebuffer_width = 0;
    frame_desc.framebuffer_height = 0;
    if (henka_engine_get_framebuffer_size(engine, &frame_desc.framebuffer_width, &frame_desc.framebuffer_height) != HENKA_SUCCESS)
    {
        frame_desc.framebuffer_width = 1280;
        frame_desc.framebuffer_height = 720;
    }
    frame_desc.mouse_position = henka_input_get_mouse_position(engine);
    sandbox3d_try_get_mouse_framebuffer_position(engine, &frame_desc.mouse_position);
    frame_desc.mouse_left_down = henka_input_is_mouse_button_down(engine, HENKA_MOUSE_BUTTON_LEFT);
    frame_desc.mouse_left_pressed = henka_input_was_mouse_button_pressed(engine, HENKA_MOUSE_BUTTON_LEFT);
    frame_desc.mouse_left_released = henka_input_was_mouse_button_released(engine, HENKA_MOUSE_BUTTON_LEFT);

    if (henka_ui_begin_frame(state->ui, &frame_desc) != HENKA_SUCCESS)
    {
        return;
    }

    layout = sandbox3d_workspace_layout_is_valid(&state->frame_layout)
        ? state->frame_layout
        : sandbox3d_get_workspace_layout(state, frame_desc.framebuffer_width, frame_desc.framebuffer_height);

    if (henka_ui_is_visible(state->ui))
    {
        settings_path = NULL;
        save_path = NULL;
        result = sandbox3d_get_settings_path(engine, &settings_path);
        milliseconds = (float)(henka_engine_get_delta_time(engine) * 1000.0);
        fps = milliseconds > 0.0f ? 1000.0f / milliseconds : 0.0f;
        sandbox3d_format_display_path("Assets", henka_engine_get_asset_base_path(engine), asset_path_text, sizeof(asset_path_text));
        sandbox3d_format_display_path("User", henka_engine_get_user_data_base_path(engine), user_path_text, sizeof(user_path_text));
        sandbox3d_format_display_path("Settings", result == HENKA_SUCCESS ? settings_path : NULL, settings_path_text, sizeof(settings_path_text));
        result = henka_save_data_build_slot_path(henka_engine_get_user_data_base_path(engine), "sandbox3d_preview", &save_path);
        sandbox3d_format_display_path("Save", result == HENKA_SUCCESS ? save_path : NULL, save_path_text, sizeof(save_path_text));
        snprintf(capture_text, sizeof(capture_text), "Capture: %s", henka_engine_is_mouse_captured(engine) ? "On" : "Off");
        snprintf(fps_text, sizeof(fps_text), "Frame: %.2f ms  FPS: %.1f", milliseconds, fps);
        sandbox3d_draw_scene_viewport_frame(state->ui, layout.scene_frame);
        sandbox3d_draw_selection_highlight(state, layout.scene_viewport);
        sandbox3d_draw_gizmo_overlay(engine, state, layout.scene_viewport);
        sandbox3d_draw_physics_overlay(state, layout.scene_viewport);
        sandbox3d_draw_viewport_debug_strip(engine, state, layout.debug_strip);
        if (!sandbox3d_workspace_panel_is_floating(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_CONTROLS))
        {
            sandbox3d_draw_controls_panel(engine, state, &layout);
        }
        if (!sandbox3d_workspace_panel_is_floating(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS))
        {
            sandbox3d_draw_scene_objects_panel(engine, state, &layout);
        }
        if (!sandbox3d_workspace_panel_is_floating(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS))
        {
            sandbox3d_draw_object_details_panel(engine, state, &layout);
        }
        if (!sandbox3d_workspace_panel_is_floating(&state->workspace.model, SANDBOX3D_WORKSPACE_PANEL_UTILITY))
        {
            sandbox3d_draw_utility_panel(
                engine, state, &layout, asset_path_text, user_path_text, settings_path_text,
                save_path_text, capture_text, fps_text, frame_desc.framebuffer_width, frame_desc.framebuffer_height);
        }

        for (z_order = 1U; z_order < state->workspace.model.next_z_order; ++z_order)
        {
            for (panel_index = 0; panel_index < SANDBOX3D_WORKSPACE_PANEL_COUNT; ++panel_index)
            {
                workspace_panel = sandbox3d_workspace_get_panel_const(&state->workspace.model, (sandbox3d_workspace_panel_id)panel_index);
                if (workspace_panel == NULL ||
                    workspace_panel->dock != SANDBOX3D_WORKSPACE_DOCK_FLOATING ||
                    workspace_panel->z_order != z_order ||
                    !sandbox3d_workspace_panel_visible(state, (sandbox3d_workspace_panel_id)panel_index))
                {
                    continue;
                }
                switch ((sandbox3d_workspace_panel_id)panel_index)
                {
                    case SANDBOX3D_WORKSPACE_PANEL_CONTROLS:
                        sandbox3d_draw_controls_panel(engine, state, &layout);
                        break;
                    case SANDBOX3D_WORKSPACE_PANEL_SCENE_OBJECTS:
                        sandbox3d_draw_scene_objects_panel(engine, state, &layout);
                        break;
                    case SANDBOX3D_WORKSPACE_PANEL_OBJECT_DETAILS:
                        sandbox3d_draw_object_details_panel(engine, state, &layout);
                        break;
                    case SANDBOX3D_WORKSPACE_PANEL_UTILITY:
                        sandbox3d_draw_utility_panel(
                            engine, state, &layout, asset_path_text, user_path_text, settings_path_text,
                            save_path_text, capture_text, fps_text, frame_desc.framebuffer_width, frame_desc.framebuffer_height);
                        break;
                    case SANDBOX3D_WORKSPACE_PANEL_NONE:
                    default:
                        break;
                }
            }
        }
        sandbox3d_draw_workspace_affordances(
            state,
            &layout,
            frame_desc.framebuffer_width,
            frame_desc.framebuffer_height);

        if (state->ui_visibility_report_pending)
        {
            printf(
                "Sandbox UI ready: %s mode, framebuffer %dx%d, draw rects %zu, scene panel %s, details panel %s, utility %s.\n",
                sandbox3d_get_layout_mode_label(state->workspace.layout_mode),
                frame_desc.framebuffer_width,
                frame_desc.framebuffer_height,
                henka_ui_get_draw_rect_count(state->ui),
                sandbox3d_workspace_shows_scene_panel(state) ? "on" : "off",
                sandbox3d_workspace_shows_details_panel(state) ? "on" : "off",
                sandbox3d_workspace_shows_utility_panel(state) ? sandbox3d_get_utility_label(state->workspace.active_utility) : "off");
            printf(
                "Sandbox viewport: origin %d,%d size %dx%d.\n",
                layout.scene_viewport.x,
                layout.scene_viewport.y,
                layout.scene_viewport.width,
                layout.scene_viewport.height);
            fflush(stdout);
            state->ui_visibility_report_pending = false;
        }

        henka_free(settings_path);
        henka_free(save_path);
    }
    else
    {
        sandbox3d_draw_selection_highlight(state, layout.scene_viewport);
        sandbox3d_draw_gizmo_overlay(engine, state, layout.scene_viewport);
        sandbox3d_draw_physics_overlay(state, layout.scene_viewport);
        sandbox3d_draw_panel_recall_hint(state->ui, layout.scene_viewport);
    }

    henka_ui_end_frame(state->ui);
}

static henka_result sandbox3d_initialize(henka_engine* engine, void* user_data)
{
    henka_asset_manager* assets;
    henka_material colored_material;
    henka_material cube_material;
    henka_material fallback_material;
    henka_material fallback_model_material;
    henka_material grid_material;
    henka_material ground_material;
    henka_material marker_material;
    henka_result result;
    henka_transform transform;
    int framebuffer_height;
    int framebuffer_width;
    sandbox3d_state* state;
    sandbox3d_workspace_layout layout;

    state = (sandbox3d_state*)user_data;

    result = henka_settings_create(&state->settings);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_ui_create(&state->ui);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_ui_create(&state->native_panel_ui);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    henka_ui_set_visible(state->native_panel_ui, true);
    state->native_panel_window_id = HENKA_INVALID_WINDOW_ID;
    henka_ui_set_visible(state->ui, false);
    sandbox3d_reset_workspace_layout(state);

    result = henka_scene_create(&state->scene);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_action_context_create(&state->actions);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    result = henka_action_context_set_scene(state->actions, state->scene);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    result = henka_action_context_set_camera(state->actions, &state->camera);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    assets = henka_engine_get_asset_manager(engine);
    if (assets == NULL)
    {
        result = HENKA_ERROR_UNKNOWN;
        goto fail;
    }

    result = henka_assets_load_shader(assets, "assets/shaders/basic_lit.vert", "assets/shaders/basic_lit.frag", &state->basic_shader);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_assets_load_shader(assets, "assets/shaders/debug_grid.vert", "assets/shaders/debug_grid.frag", &state->grid_shader);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_assets_load_texture(assets, "assets/textures/cube_albedo.png", &state->cube_texture);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_assets_load_texture(assets, "assets/textures/ground_checker.png", &state->ground_texture);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_assets_load_texture(assets, "assets/textures/missing_texture.png", &state->missing_texture);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_mesh_create_cube(engine, &state->cube_mesh);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_mesh_create_plane(engine, 12.0f, 12.0f, &state->ground_mesh);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_mesh_create_debug_grid(engine, 12, 1.0f, &state->grid_mesh);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_assets_load_obj_mesh(assets, "assets/models/henka_marker.obj", &state->marker_mesh);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_assets_load_obj_mesh(assets, "assets/models/missing_marker.obj", &state->missing_model_mesh);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    state->ground_entity = henka_scene_create_entity_named(state->scene, "Ground");
    state->cube_entity = henka_scene_create_entity_named(state->scene, "Textured Cube");
    state->colored_cube_entity = henka_scene_create_entity_named(state->scene, "Colored Cube");
    state->marker_entity = henka_scene_create_entity_named(state->scene, "OBJ Marker");
    state->fallback_cube_entity = henka_scene_create_entity_named(state->scene, "Missing Texture");
    state->fallback_model_entity = henka_scene_create_entity_named(state->scene, "Missing Model");
    state->grid_entity = henka_scene_create_entity_named(state->scene, "Debug Grid");

    if (state->ground_entity == HENKA_INVALID_ENTITY ||
        state->cube_entity == HENKA_INVALID_ENTITY ||
        state->colored_cube_entity == HENKA_INVALID_ENTITY ||
        state->marker_entity == HENKA_INVALID_ENTITY ||
        state->fallback_cube_entity == HENKA_INVALID_ENTITY ||
        state->fallback_model_entity == HENKA_INVALID_ENTITY ||
        state->grid_entity == HENKA_INVALID_ENTITY)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto fail;
    }

    ground_material = henka_material_default();
    ground_material.name = "Ground Checker";
    ground_material.type = HENKA_MATERIAL_TYPE_LIT;
    ground_material.shader = state->basic_shader;
    ground_material.base_color_texture = state->ground_texture;
    ground_material.use_texture = true;
    ground_material.base_color = (henka_vec4){0.88f, 0.88f, 0.90f, 1.0f};

    cube_material = henka_material_default();
    cube_material.name = "Cube Albedo";
    cube_material.type = HENKA_MATERIAL_TYPE_LIT;
    cube_material.shader = state->basic_shader;
    cube_material.base_color_texture = state->cube_texture;
    cube_material.use_texture = true;
    cube_material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};

    colored_material = henka_material_default();
    colored_material.name = "Flat Color";
    colored_material.type = HENKA_MATERIAL_TYPE_UNLIT;
    colored_material.shader = state->basic_shader;
    colored_material.base_color = (henka_vec4){0.20f, 0.72f, 0.56f, 1.0f};
    colored_material.use_texture = false;
    colored_material.use_lighting = false;

    marker_material = henka_material_default();
    marker_material.name = "OBJ Marker";
    marker_material.type = HENKA_MATERIAL_TYPE_LIT;
    marker_material.shader = state->basic_shader;
    marker_material.base_color = (henka_vec4){0.96f, 0.72f, 0.18f, 1.0f};
    marker_material.use_texture = false;

    fallback_material = henka_material_default();
    fallback_material.name = "Fallback Texture";
    fallback_material.type = HENKA_MATERIAL_TYPE_LIT;
    fallback_material.shader = state->basic_shader;
    fallback_material.base_color_texture = state->missing_texture;
    fallback_material.use_texture = true;
    fallback_material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};

    fallback_model_material = henka_material_default();
    fallback_model_material.name = "Fallback Model";
    fallback_model_material.type = HENKA_MATERIAL_TYPE_LIT;
    fallback_model_material.shader = state->basic_shader;
    fallback_model_material.base_color_texture = state->missing_texture;
    fallback_model_material.use_texture = true;
    fallback_model_material.base_color = (henka_vec4){1.0f, 1.0f, 1.0f, 1.0f};

    grid_material = henka_material_default();
    grid_material.name = "Debug Grid";
    grid_material.type = HENKA_MATERIAL_TYPE_UNLIT;
    grid_material.shader = state->grid_shader;
    grid_material.base_color = (henka_vec4){0.48f, 0.78f, 0.98f, 1.0f};
    grid_material.use_texture = false;
    grid_material.use_lighting = false;

    transform = sandbox3d_make_transform((henka_vec3){0.0f, -0.02f, 0.0f}, (henka_vec3){1.0f, 1.0f, 1.0f});
    result = sandbox3d_configure_entity(state->scene, state->ground_entity, state->ground_mesh, ground_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_apply_entity_foundation(
        state,
        state->ground_entity,
        "ground",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){6.0f, 0.05f, 6.0f}),
        true,
        "Inspect ground sample");
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_GROUND,
        state->ground_entity,
        "Ground",
        "Under the scene,",
        "shows repeated local texture use.",
        "Uses a built-in plane mesh with a lit checker texture.",
        "Built-in plane mesh.",
        "Textured lit material.",
        "Ground checker texture from assets/textures/ground_checker.png.",
        true,
        true,
        transform);

    transform = sandbox3d_make_transform(g_textured_cube_position, (henka_vec3){1.0f, 1.0f, 1.0f});
    result = sandbox3d_configure_entity(state->scene, state->cube_entity, state->cube_mesh, cube_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_apply_entity_foundation(
        state,
        state->cube_entity,
        "textured_cube",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){0.5f, 0.5f, 0.5f}),
        true,
        "Inspect textured cube");
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_TEXTURED_CUBE,
        state->cube_entity,
        "Textured Cube",
        "Near the center,",
        "shows a cube using a texture material.",
        "Uses the built-in cube mesh with a base-color texture.",
        "Built-in cube mesh.",
        "Textured lit material.",
        "Cube albedo texture from assets/textures/cube_albedo.png.",
        true,
        true,
        transform);

    transform = sandbox3d_make_transform(g_colored_cube_position, (henka_vec3){1.0f, 1.0f, 1.0f});
    result = sandbox3d_configure_entity(state->scene, state->colored_cube_entity, state->cube_mesh, colored_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_apply_entity_foundation(
        state,
        state->colored_cube_entity,
        "colored_cube",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){0.5f, 0.5f, 0.5f}),
        true,
        "Inspect colored cube");
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_COLORED_CUBE,
        state->colored_cube_entity,
        "Colored Cube",
        "Left of center,",
        "shows an untextured material color.",
        "Uses the built-in cube mesh without a base-color texture.",
        "Built-in cube mesh.",
        "Colored lit material.",
        "No texture is used for this object.",
        true,
        true,
        transform);

    transform = sandbox3d_make_transform(g_marker_position, (henka_vec3){1.35f, 1.35f, 1.35f});
    result = sandbox3d_configure_entity(state->scene, state->marker_entity, state->marker_mesh, marker_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_apply_entity_foundation(
        state,
        state->marker_entity,
        "obj_marker",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.35f, 0.0f}, (henka_vec3){0.65f, 0.7f, 0.65f}),
        true,
        "Inspect OBJ sample");
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_OBJ_MARKER,
        state->marker_entity,
        "OBJ Marker",
        "Farther left,",
        "shows the current OBJ loading path.",
        "Uses an OBJ mesh loaded through the asset manager cache.",
        "OBJ mesh from assets/models/henka_marker.obj.",
        "Colored lit material.",
        "No texture is used for this object.",
        true,
        true,
        transform);

    transform = sandbox3d_make_transform(g_missing_texture_position, (henka_vec3){1.0f, 1.0f, 1.0f});
    result = sandbox3d_configure_entity(state->scene, state->fallback_cube_entity, state->cube_mesh, fallback_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_apply_entity_foundation(
        state,
        state->fallback_cube_entity,
        "missing_texture",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){0.5f, 0.5f, 0.5f}),
        true,
        "Inspect fallback texture sample");
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_MISSING_TEXTURE,
        state->fallback_cube_entity,
        "Missing Texture",
        "Right of center,",
        "shows the magenta fallback texture when an image cannot be loaded.",
        "Uses the built-in cube mesh with the engine error texture fallback.",
        "Built-in cube mesh.",
        "Textured lit material.",
        "Error texture fallback after a missing texture load.",
        true,
        true,
        transform);

    transform = sandbox3d_make_transform(g_missing_model_position, (henka_vec3){0.95f, 0.95f, 0.95f});
    result = sandbox3d_configure_entity(state->scene, state->fallback_model_entity, state->missing_model_mesh, fallback_model_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_apply_entity_foundation(
        state,
        state->fallback_model_entity,
        "missing_model",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.35f, 0.0f}, (henka_vec3){0.65f, 0.7f, 0.65f}),
        true,
        "Inspect fallback mesh sample");
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_MISSING_MODEL,
        state->fallback_model_entity,
        "Missing Model",
        "Farther right,",
        "shows the fallback mesh when an OBJ file is missing.",
        "Uses the asset manager fallback mesh and the error texture fallback.",
        "Fallback mesh from the asset manager.",
        "Textured lit material.",
        "Error texture fallback after a missing OBJ load.",
        true,
        true,
        transform);

    transform = sandbox3d_make_transform((henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){1.0f, 1.0f, 1.0f});
    result = sandbox3d_configure_entity(state->scene, state->grid_entity, state->grid_mesh, grid_material, transform);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }
    sandbox3d_apply_entity_foundation(
        state,
        state->grid_entity,
        "debug_grid",
        sandbox3d_make_bounds((henka_vec3){0.0f, 0.0f, 0.0f}, (henka_vec3){6.0f, 0.05f, 6.0f}),
        false,
        NULL);
    sandbox3d_register_object_descriptor(
        state,
        SANDBOX3D_OBJECT_DEBUG_GRID,
        state->grid_entity,
        "Debug Grid",
        "Across the floor,",
        "helps judge position, depth, and movement.",
        "Uses the built-in debug grid mesh with an unlit line material.",
        "Built-in debug grid mesh.",
        "Unlit grid material.",
        "No texture is used for this object.",
        true,
        true,
        transform);

    result = sandbox3d_initialize_physics(state);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = sandbox3d_initialize_gizmo_rendering(engine, state);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    henka_scene_set_light_direction(state->scene, (henka_vec3){-0.5f, -1.0f, -0.3f});
    henka_scene_set_ambient_color(state->scene, (henka_vec3){0.18f, 0.20f, 0.25f});

    result = henka_engine_get_framebuffer_size(engine, &framebuffer_width, &framebuffer_height);
    if (result != HENKA_SUCCESS || framebuffer_height <= 0)
    {
        framebuffer_width = 1280;
        framebuffer_height = 720;
    }

    state->camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, (float)framebuffer_width / (float)framebuffer_height, 0.1f, 100.0f);
    sandbox3d_reset_camera_defaults(state);

    {
        FILE* settings_file;
        char* settings_path;
        henka_result load_result;

        settings_path = NULL;
        result = sandbox3d_get_settings_path(engine, &settings_path);
        if (result == HENKA_SUCCESS)
        {
            settings_file = NULL;
            if (fopen_s(&settings_file, settings_path, "r") == 0 && settings_file != NULL)
            {
                state->settings_file_found = true;
                fclose(settings_file);

                load_result = henka_settings_load_file(state->settings, settings_path);
                if (load_result == HENKA_ERROR_UNKNOWN)
                {
                    HENKA_LOG_WARN("Sandbox settings were loaded with one or more invalid lines. Defaults were kept for the invalid values.");
                }
                else if (load_result != HENKA_SUCCESS)
                {
                    HENKA_LOG_WARN("Sandbox settings were not loaded. The sandbox is using first-run defaults.");
                }
            }

            henka_free(settings_path);
        }
        else
        {
            HENKA_LOG_WARN("Sandbox settings path could not be resolved. The sandbox is using first-run defaults.");
        }
    }

    result = henka_scene_set_camera(state->scene, &state->camera);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_engine_set_scene(engine, state->scene);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    result = henka_engine_set_ui_context(engine, state->ui);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    sandbox3d_apply_loaded_settings(engine, state);
    state->startup_panels_auto_opened = sandbox3d_workspace_should_start_panels_visible(state->settings_file_found);
    if (!state->settings_file_found && state->workspace.active_utility == SANDBOX3D_UTILITY_NONE)
    {
        state->workspace.active_utility = SANDBOX3D_UTILITY_PHYSICS_QA;
    }
    henka_ui_set_visible(state->ui, state->startup_panels_auto_opened);
    state->ui_visibility_report_pending = state->startup_panels_auto_opened;
    sandbox3d_set_status(state, false, "Panels are open. Use Controls or Physics QA to start testing.");

    result = henka_engine_set_mouse_capture(engine, false);
    if (result != HENKA_SUCCESS)
    {
        goto fail;
    }

    sandbox3d_print_help(state);
    printf("Runtime mode: %s\n", henka_engine_get_package_mode_label(henka_engine_get_package_mode(engine)));
    fflush(stdout);
    sandbox3d_print_startup_ui_cue(state);
    sandbox3d_print_layout_mode(state, true);
    return HENKA_SUCCESS;

fail:
    sandbox3d_release_owned_resources(state);
    return result;
}

static void sandbox3d_update(henka_engine* engine, double delta_seconds, void* user_data)
{
    henka_vec2 framebuffer_mouse_position;
    henka_vec2 mouse_position;
    henka_vec2 mouse_delta;
    henka_vec2 mouse_wheel_delta;
    bool navigation_active;
    bool ui_toggled_with_f4;
    bool ui_visible;
    bool workspace_input_active;
    int framebuffer_height;
    int framebuffer_width;
    sandbox3d_workspace_layout layout;
    sandbox3d_state* state;

    state = (sandbox3d_state*)user_data;
    framebuffer_width = 1280;
    framebuffer_height = 720;
    ui_toggled_with_f4 = false;
    navigation_active = false;
    workspace_input_active = false;

    if (henka_input_was_key_pressed(engine, HENKA_KEY_H))
    {
        if (state->ui != NULL)
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_HELP);
        }
        sandbox3d_set_statusf(state, false, false, "Help is open in the Utility panel.");
        sandbox3d_print_help(state);
    }

    if (henka_input_action_was_pressed(engine, HENKA_INPUT_ACTION_OPEN_PANELS) && state->ui != NULL)
    {
        ui_visible = !henka_ui_is_visible(state->ui);
        sandbox3d_clear_gizmo_drag(state, true);
        sandbox3d_workspace_end_interaction(&state->workspace.model);
        henka_ui_set_visible(state->ui, ui_visible);
        if (ui_visible)
        {
            henka_engine_set_mouse_capture(engine, false);
            state->ui_visibility_report_pending = true;
        }
        sandbox3d_set_statusf(
            state,
            false,
            false,
            "%s",
            ui_visible ? "Panels shown." : "Panels hidden. Press F4 to show tools.");
        sandbox3d_print_ui_state(ui_visible);
        ui_toggled_with_f4 = true;
    }

    if (henka_input_action_was_pressed(engine, HENKA_INPUT_ACTION_CHANGE_LAYOUT) && state->ui != NULL)
    {
        sandbox3d_clear_gizmo_drag(state, true);
        sandbox3d_workspace_end_interaction(&state->workspace.model);
        state->workspace.layout_mode = sandbox3d_cycle_layout_mode(state->workspace.layout_mode);
        if (!henka_ui_is_visible(state->ui))
        {
            henka_ui_set_visible(state->ui, true);
            henka_engine_set_mouse_capture(engine, false);
        }
        state->ui_visibility_report_pending = true;
        sandbox3d_set_statusf(state, false, false, "Layout set to %s.", sandbox3d_get_layout_mode_label(state->workspace.layout_mode));
        sandbox3d_print_layout_mode(state, false);
    }

    if (henka_input_was_key_pressed(engine, HENKA_KEY_F2))
    {
        if (state->ui != NULL)
        {
            sandbox3d_set_active_utility(state, SANDBOX3D_UTILITY_SCENE_LEGEND);
        }
        sandbox3d_set_statusf(state, false, false, "Scene legend is open in the Utility panel.");
        sandbox3d_print_scene_legend(state);
        fflush(stdout);
    }

    if (henka_input_was_key_pressed(engine, HENKA_KEY_F1))
    {
        sandbox3d_toggle_wireframe(engine, !henka_engine_is_wireframe_enabled(engine), true);
    }

    if (henka_input_was_key_pressed(engine, HENKA_KEY_F3))
    {
        sandbox3d_toggle_grid_visibility(state, !henka_scene_is_entity_visible(state->scene, state->grid_entity), true);
    }

    ui_visible = state->ui != NULL && henka_ui_is_visible(state->ui);
    if (!ui_toggled_with_f4 && state->ui_visible_last_frame && !ui_visible)
    {
        sandbox3d_print_ui_state(false);
    }

    if (!ui_visible && henka_input_action_was_pressed(engine, HENKA_INPUT_ACTION_TOGGLE_MOUSE_CAPTURE))
    {
        sandbox3d_clear_gizmo_drag(state, true);
        henka_engine_set_mouse_capture(engine, !henka_engine_is_mouse_captured(engine));
        sandbox3d_set_statusf(state, false, false, "Mouse capture %s.", henka_engine_is_mouse_captured(engine) ? "enabled" : "released");
        sandbox3d_print_capture_state(engine, "bound action");
    }

    if (henka_engine_get_framebuffer_size(engine, &framebuffer_width, &framebuffer_height) == HENKA_SUCCESS && framebuffer_height > 0)
    {
        layout = sandbox3d_get_workspace_layout(state, framebuffer_width, framebuffer_height);
        if (state->gizmo.drag.dragging &&
            !sandbox3d_viewports_match(layout.scene_viewport, state->gizmo.drag.drag_start_viewport))
        {
            sandbox3d_clear_gizmo_drag(state, true);
            sandbox3d_set_status(state, false, "Viewport changed. Gizmo drag stopped.");
        }
        state->frame_layout = layout;
        henka_engine_set_scene_viewport(engine, layout.scene_viewport);
        henka_camera_set_aspect_ratio(&state->camera, henka_viewport_get_aspect_ratio(layout.scene_viewport));
    }
    else
    {
        state->frame_layout = sandbox3d_get_workspace_layout(state, 1280, 720);
        henka_engine_set_scene_viewport(engine, state->frame_layout.scene_viewport);
    }

    if (henka_input_was_key_pressed(engine, HENKA_KEY_HOME))
    {
        sandbox3d_reset_camera_defaults(state);
        sandbox3d_set_statusf(state, false, false, "Reset view.");
    }

    if (henka_input_was_key_pressed(engine, HENKA_KEY_F))
    {
        sandbox3d_frame_selected_object(state, true);
    }

    mouse_position = henka_input_get_mouse_position(engine);
    mouse_delta = henka_input_get_mouse_delta(engine);
    mouse_wheel_delta = henka_input_get_mouse_wheel_delta(engine);
    framebuffer_mouse_position = mouse_position;
    sandbox3d_try_get_mouse_framebuffer_position(engine, &framebuffer_mouse_position);
    if (ui_visible && !henka_engine_is_mouse_captured(engine))
    {
        workspace_input_active = sandbox3d_handle_workspace_input(
            engine,
            state,
            framebuffer_mouse_position,
            framebuffer_width > 0 ? framebuffer_width : 1280,
            framebuffer_height > 0 ? framebuffer_height : 720);
        if (workspace_input_active)
        {
            layout = sandbox3d_get_workspace_layout(
                state,
                framebuffer_width > 0 ? framebuffer_width : 1280,
                framebuffer_height > 0 ? framebuffer_height : 720);
            state->frame_layout = layout;
            henka_engine_set_scene_viewport(engine, layout.scene_viewport);
            henka_camera_set_aspect_ratio(&state->camera, henka_viewport_get_aspect_ratio(layout.scene_viewport));
        }
    }

    if (!ui_visible && henka_engine_is_mouse_captured(engine))
    {
        henka_camera_apply_mouse_look(
            &state->camera,
            -mouse_delta.x * sandbox3d_get_mouse_sensitivity(state),
            -mouse_delta.y * sandbox3d_get_mouse_sensitivity(state));
    }
    if (henka_engine_is_mouse_captured(engine) &&
        henka_input_was_mouse_button_pressed(engine, HENKA_MOUSE_BUTTON_LEFT) &&
        state->viewport_tool != SANDBOX3D_VIEWPORT_TOOL_SELECT)
    {
        sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_MOUSE_CAPTURE_ACTIVE, true);
    }

    if (!henka_engine_is_mouse_captured(engine) && !workspace_input_active)
    {
        const bool mouse_in_viewport = henka_viewport_contains_point(state->frame_layout.scene_viewport, framebuffer_mouse_position);
        const bool ui_wants_mouse = sandbox3d_ui_owns_mouse_at_point(state, framebuffer_mouse_position);
        const bool alt_orbit_held = henka_input_is_key_down(engine, HENKA_KEY_LEFT_ALT) && henka_input_is_mouse_button_down(engine, HENKA_MOUSE_BUTTON_LEFT);
        const bool alt_orbit_pressed = henka_input_is_key_down(engine, HENKA_KEY_LEFT_ALT) && henka_input_was_mouse_button_pressed(engine, HENKA_MOUSE_BUTTON_LEFT);
        const bool middle_pan_held = henka_input_is_mouse_button_down(engine, HENKA_MOUSE_BUTTON_MIDDLE);
        const bool middle_pan_pressed = henka_input_was_mouse_button_pressed(engine, HENKA_MOUSE_BUTTON_MIDDLE);
        const bool left_pressed = henka_input_was_mouse_button_pressed(engine, HENKA_MOUSE_BUTTON_LEFT);
        const bool left_down = henka_input_is_mouse_button_down(engine, HENKA_MOUSE_BUTTON_LEFT);
        const henka_entity selected_entity = sandbox3d_get_real_selected_entity(state);
        sandbox3d_interaction_gate gate;

        memset(&gate, 0, sizeof(gate));
        gate.supported_mouse_button = true;
        gate.cursor_in_viewport = mouse_in_viewport;
        gate.ui_wants_mouse = ui_wants_mouse;
        gate.panel_click_rejected = ui_wants_mouse && !mouse_in_viewport;
        gate.selected_object_present = selected_entity != HENKA_INVALID_ENTITY;
        gate.selected_object_valid = gate.selected_object_present && henka_scene_is_entity_valid(state->scene, selected_entity);
        gate.selected_object_visible = gate.selected_object_valid && henka_scene_is_entity_visible(state->scene, selected_entity);
        gate.selected_object_selectable = gate.selected_object_valid && sandbox3d_is_selectable_entity(state, selected_entity);
        gate.selected_bounds_valid = sandbox3d_get_selected_bounds(state, &(henka_bounds){0});

        if (alt_orbit_pressed && mouse_in_viewport)
        {
            sandbox3d_clear_gizmo_drag(state, true);
            state->view_navigation.orbiting = true;
            sandbox3d_set_view_navigation_target(state, sandbox3d_get_view_navigation_target(state));
            sandbox3d_record_success_result(state, "Orbit shortcut started");
        }
        if (middle_pan_pressed && mouse_in_viewport)
        {
            sandbox3d_clear_gizmo_drag(state, true);
            state->view_navigation.panning = true;
            sandbox3d_set_view_navigation_target(state, sandbox3d_get_view_navigation_target(state));
            sandbox3d_record_success_result(state, "Pan shortcut started");
        }
        if (state->view_navigation.orbiting &&
            !alt_orbit_held &&
            !(state->viewport_tool == SANDBOX3D_VIEWPORT_TOOL_ORBIT && left_down))
        {
            state->view_navigation.orbiting = false;
        }
        if (state->view_navigation.panning &&
            !middle_pan_held &&
            !(state->viewport_tool == SANDBOX3D_VIEWPORT_TOOL_PAN && left_down))
        {
            state->view_navigation.panning = false;
        }

        if (mouse_in_viewport && mouse_wheel_delta.y != 0.0f)
        {
            sandbox3d_zoom_camera_to_target(state, mouse_wheel_delta.y > 0.0f ? -1.0f : 1.0f);
            navigation_active = true;
        }
        else if (mouse_wheel_delta.y != 0.0f)
        {
            sandbox3d_advance_panel_paging(
                state,
                sandbox3d_get_panel_scroll_target(state, framebuffer_mouse_position),
                mouse_wheel_delta.y > 0.0f ? -1 : 1);
        }

        if (state->view_navigation.orbiting && mouse_in_viewport)
        {
            const henka_vec3 target = sandbox3d_get_view_navigation_target(state);
            henka_camera_orbit_target(
                &state->camera,
                target,
                -mouse_delta.x * sandbox3d_get_mouse_sensitivity(state),
                -mouse_delta.y * sandbox3d_get_mouse_sensitivity(state));
            sandbox3d_set_view_navigation_target(state, target);
            navigation_active = true;
            sandbox3d_record_success_result(state, "Orbit updated");
        }
        else if (state->view_navigation.panning && mouse_in_viewport)
        {
            henka_vec3 target = sandbox3d_get_view_navigation_target(state);
            const float distance = henka_vec3_length(henka_vec3_subtract(target, state->camera.position));
            const float pan_scale = fmaxf(0.0035f, distance * 0.0016f);

            henka_camera_pan_target(&state->camera, &target, -mouse_delta.x * pan_scale, mouse_delta.y * pan_scale);
            sandbox3d_set_view_navigation_target(state, target);
            navigation_active = true;
            sandbox3d_record_success_result(state, "Pan updated");
        }

        if (!navigation_active && left_pressed)
        {
            switch (state->viewport_tool)
            {
                case SANDBOX3D_VIEWPORT_TOOL_ORBIT:
                    if (sandbox3d_evaluate_navigation_reject_reason(state->viewport_tool, &gate) == SANDBOX3D_INTERACTION_REJECT_NONE)
                    {
                        sandbox3d_clear_gizmo_drag(state, true);
                        state->view_navigation.orbiting = true;
                        state->view_navigation.panning = false;
                        sandbox3d_set_view_navigation_target(state, sandbox3d_get_view_navigation_target(state));
                        sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_NONE, false);
                        sandbox3d_record_success_result(state, "Orbit tool started");
                    }
                    else
                    {
                        sandbox3d_record_reject_reason(state, sandbox3d_evaluate_navigation_reject_reason(state->viewport_tool, &gate), true);
                    }
                    break;

                case SANDBOX3D_VIEWPORT_TOOL_PAN:
                    if (sandbox3d_evaluate_navigation_reject_reason(state->viewport_tool, &gate) == SANDBOX3D_INTERACTION_REJECT_NONE)
                    {
                        sandbox3d_clear_gizmo_drag(state, true);
                        state->view_navigation.panning = true;
                        state->view_navigation.orbiting = false;
                        sandbox3d_set_view_navigation_target(state, sandbox3d_get_view_navigation_target(state));
                        sandbox3d_record_reject_reason(state, SANDBOX3D_INTERACTION_REJECT_NONE, false);
                        sandbox3d_record_success_result(state, "Pan tool started");
                    }
                    else
                    {
                        sandbox3d_record_reject_reason(state, sandbox3d_evaluate_navigation_reject_reason(state->viewport_tool, &gate), true);
                    }
                    break;

                case SANDBOX3D_VIEWPORT_TOOL_SELECT:
                case SANDBOX3D_VIEWPORT_TOOL_MOVE:
                case SANDBOX3D_VIEWPORT_TOOL_ROTATE:
                case SANDBOX3D_VIEWPORT_TOOL_SCALE:
                default:
                    break;
            }
        }

        if (!left_down && !alt_orbit_held)
        {
            state->view_navigation.orbiting = false;
        }
        if (!left_down && !middle_pan_held)
        {
            state->view_navigation.panning = false;
        }

        if (!navigation_active)
        {
            sandbox3d_update_gizmo_hover(engine, state);
        }

        if (!navigation_active && state->gizmo.drag.dragging)
        {
            sandbox3d_apply_gizmo_drag(engine, state);
        }

        if (!navigation_active && state->gizmo.drag.dragging && !henka_input_is_mouse_button_down(engine, HENKA_MOUSE_BUTTON_LEFT))
        {
            sandbox3d_set_statusf(
                state,
                false,
                false,
                "Gizmo drag completed on %s axis.",
                sandbox3d_get_gizmo_axis_label(state->gizmo.drag.active_axis));
            sandbox3d_clear_gizmo_drag(state, false);
        }

        if (!navigation_active && left_pressed && !henka_input_is_key_down(engine, HENKA_KEY_LEFT_ALT))
        {
            if (state->viewport_tool == SANDBOX3D_VIEWPORT_TOOL_SELECT)
            {
                if (sandbox3d_evaluate_select_reject_reason(&gate) == SANDBOX3D_INTERACTION_REJECT_NONE)
                {
                    sandbox3d_try_pick_object(engine, state);
                }
                else
                {
                    sandbox3d_record_reject_reason(state, sandbox3d_evaluate_select_reject_reason(&gate), true);
                }
            }
            else if (sandbox3d_viewport_tool_mode_uses_gizmo(state->viewport_tool))
            {
                const sandbox3d_interaction_reject_reason reason =
                    sandbox3d_evaluate_select_reject_reason(&gate);
                if (reason == SANDBOX3D_INTERACTION_REJECT_NONE)
                {
                    sandbox3d_try_pick_object(engine, state);
                }
                else
                {
                    sandbox3d_record_reject_reason(state, reason, true);
                }
            }
        }
    }

    if (!ui_visible)
    {
        henka_camera_move_fly(&state->camera, engine, delta_seconds);
    }

    sandbox3d_update_physics(state, delta_seconds);
    henka_scene_set_camera(state->scene, &state->camera);
    sandbox3d_update_gizmo_rendering(state);
    sandbox3d_refresh_interaction_diagnostics(engine, state);
    sandbox3d_build_ui(engine, state);
    sandbox3d_build_native_panel_test_ui(engine, state);
    state->ui_visible_last_frame = ui_visible;
}

static void sandbox3d_shutdown(henka_engine* engine, void* user_data)
{
    sandbox3d_state* state;

    state = (sandbox3d_state*)user_data;
    sandbox3d_save_settings(engine, state);
    sandbox3d_close_native_panel_test(engine, state);
    henka_engine_set_mouse_capture(engine, false);
    henka_engine_set_scene(engine, NULL);
    henka_engine_set_ui_context(engine, NULL);
    sandbox3d_release_owned_resources(state);
}

int main(void)
{
    henka_engine* engine;
    henka_engine_config config;
    henka_result result;
    sandbox3d_state state;
    size_t index;

    memset(&state, 0, sizeof(state));
    state.camera = henka_camera_create_perspective(60.0f * HENKA_DEG_TO_RAD, 16.0f / 9.0f, 0.1f, 100.0f);
    state.cube_entity = HENKA_INVALID_ENTITY;
    state.ground_entity = HENKA_INVALID_ENTITY;
    state.grid_entity = HENKA_INVALID_ENTITY;
    state.colored_cube_entity = HENKA_INVALID_ENTITY;
    state.fallback_cube_entity = HENKA_INVALID_ENTITY;
    state.marker_entity = HENKA_INVALID_ENTITY;
    state.fallback_model_entity = HENKA_INVALID_ENTITY;
    state.selected_entity = HENKA_INVALID_ENTITY;
    state.native_panel_window_id = HENKA_INVALID_WINDOW_ID;
    state.ui_visible_last_frame = false;
    sandbox3d_reset_workspace_layout(&state);
    sandbox3d_gizmo_init_defaults(&state.gizmo);

    for (index = 0U; index < SANDBOX3D_OBJECT_COUNT; ++index)
    {
        state.descriptors[index].kind = (sandbox3d_object_kind)index;
        state.descriptors[index].entity = HENKA_INVALID_ENTITY;
    }

    config.application_name = "Henka Engine Sandbox 3D";
    config.window_width = 1280;
    config.window_height = 720;
    config.enable_vsync = true;
    config.asset_base_path = NULL;
    config.user_data_base_path = NULL;
    config.package_mode = HENKA_PACKAGE_MODE_AUTO;
    config.on_initialize = sandbox3d_initialize;
    config.on_update = sandbox3d_update;
    config.on_shutdown = sandbox3d_shutdown;
    config.user_data = &state;

    result = henka_engine_create(&config, &engine);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("Unable to start the sandbox: %s", henka_result_to_string(result));
        return 1;
    }

    result = henka_engine_run(engine);
    if (result != HENKA_SUCCESS)
    {
        HENKA_LOG_ERROR("The sandbox stopped with an error: %s", henka_result_to_string(result));
        henka_engine_destroy(engine);
        return 1;
    }

    henka_engine_destroy(engine);
    return 0;
}
