#ifndef SANDBOX3D_VIEW_COMPASS_H
#define SANDBOX3D_VIEW_COMPASS_H

#include <stdbool.h>

#include <henka/camera.h>
#include <henka/core.h>
#include <henka/ui.h>

#include "editor_preferences.h"

typedef enum sandbox3d_view_compass_axis_view
{
    SANDBOX3D_VIEW_COMPASS_FRONT = 0,
    SANDBOX3D_VIEW_COMPASS_BACK,
    SANDBOX3D_VIEW_COMPASS_LEFT,
    SANDBOX3D_VIEW_COMPASS_RIGHT,
    SANDBOX3D_VIEW_COMPASS_TOP,
    SANDBOX3D_VIEW_COMPASS_BOTTOM,
    SANDBOX3D_VIEW_COMPASS_AXIS_VIEW_COUNT
} sandbox3d_view_compass_axis_view;

typedef struct sandbox3d_view_compass_layout
{
    henka_ui_rect circle_bounds;
    henka_ui_rect info_bounds;
    henka_vec2 center;
    float radius;
    float scale;
} sandbox3d_view_compass_layout;

typedef struct sandbox3d_view_compass_state
{
    bool transition_active;
    double transition_elapsed_seconds;
    double transition_duration_seconds;
    henka_camera transition_start_camera;
    henka_camera transition_target_camera;
    henka_vec3 transition_target;
    bool transition_target_valid;
    bool drag_active;
    bool drag_moved;
    henka_vec2 drag_start_pointer;
    henka_vec2 drag_last_pointer;
    float last_heading_degrees;
    bool last_heading_valid;
} sandbox3d_view_compass_state;

void sandbox3d_view_compass_state_reset(
    sandbox3d_view_compass_state* state);

bool sandbox3d_view_compass_compute_layout(
    henka_viewport viewport,
    sandbox3d_view_compass_preferences* preferences,
    sandbox3d_view_compass_layout* out_layout);

float sandbox3d_view_compass_normalize_heading_degrees(float degrees);

bool sandbox3d_view_compass_get_heading_degrees(
    const henka_camera* camera,
    sandbox3d_view_compass_state* state,
    float* out_heading_degrees);

bool sandbox3d_view_compass_project_direction(
    const henka_camera* camera,
    henka_vec3 direction,
    float radius,
    henka_vec2 center,
    henka_vec2* out_point,
    bool* out_front);

bool sandbox3d_view_compass_build_axis_camera(
    const henka_camera* current_camera,
    henka_vec3 target,
    sandbox3d_view_compass_axis_view view,
    henka_camera* out_camera);

bool sandbox3d_view_compass_begin_snap(
    sandbox3d_view_compass_state* state,
    const henka_camera* current_camera,
    henka_vec3 target,
    sandbox3d_view_compass_axis_view view,
    bool smooth_navigation);

void sandbox3d_view_compass_cancel_transition(
    sandbox3d_view_compass_state* state);

void sandbox3d_view_compass_update_transition(
    sandbox3d_view_compass_state* state,
    henka_camera* in_out_camera,
    double delta_seconds);

bool sandbox3d_view_compass_toggle_projection(
    henka_camera* in_out_camera,
    henka_vec3 target,
    bool target_valid);

/* Returns true when the Compass changed the camera or navigation target. */
bool sandbox3d_view_compass_draw(
    henka_ui_context* ui,
    henka_viewport viewport,
    henka_camera* in_out_camera,
    henka_vec3* in_out_target,
    bool* in_out_target_valid,
    sandbox3d_view_compass_state* state,
    sandbox3d_view_compass_preferences* preferences,
    bool input_blocked,
    double delta_seconds);

#endif
