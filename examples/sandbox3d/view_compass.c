#include "view_compass.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define SANDBOX3D_VIEW_COMPASS_RING_SAMPLES 48U
#define SANDBOX3D_VIEW_COMPASS_MIN_DIAMETER 72.0f
#define SANDBOX3D_VIEW_COMPASS_MAX_DIAMETER 112.0f
#define SANDBOX3D_VIEW_COMPASS_INFO_HEIGHT 24.0f
#define SANDBOX3D_VIEW_COMPASS_INFO_WIDTH 132.0f
#define SANDBOX3D_VIEW_COMPASS_DRAG_THRESHOLD 4.0f

static bool sandbox3d_compass_vec3_is_finite(henka_vec3 value)
{
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool sandbox3d_compass_vec2_is_finite(henka_vec2 value)
{
    return isfinite(value.x) && isfinite(value.y);
}

static float sandbox3d_compass_clamp(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

static henka_vec2 sandbox3d_compass_add(henka_vec2 left, henka_vec2 right)
{
    return (henka_vec2){left.x + right.x, left.y + right.y};
}

static henka_vec2 sandbox3d_compass_subtract(henka_vec2 left, henka_vec2 right)
{
    return (henka_vec2){left.x - right.x, left.y - right.y};
}

static float sandbox3d_compass_vec2_length(henka_vec2 value)
{
    return hypotf(value.x, value.y);
}

static henka_vec3 sandbox3d_compass_direction_for_view(
    sandbox3d_view_compass_axis_view view)
{
    switch (view)
    {
        case SANDBOX3D_VIEW_COMPASS_BACK:
            return (henka_vec3){-1.0f, 0.0f, 0.0f};
        case SANDBOX3D_VIEW_COMPASS_LEFT:
            return (henka_vec3){0.0f, 0.0f, 1.0f};
        case SANDBOX3D_VIEW_COMPASS_RIGHT:
            return (henka_vec3){0.0f, 0.0f, -1.0f};
        case SANDBOX3D_VIEW_COMPASS_TOP:
            return (henka_vec3){0.0f, -1.0f, 0.0f};
        case SANDBOX3D_VIEW_COMPASS_BOTTOM:
            return (henka_vec3){0.0f, 1.0f, 0.0f};
        case SANDBOX3D_VIEW_COMPASS_FRONT:
        default:
            return (henka_vec3){1.0f, 0.0f, 0.0f};
    }
}

static henka_vec3 sandbox3d_compass_deterministic_up(henka_vec3 direction)
{
    return fabsf(direction.y) > 0.92f
        ? (henka_vec3){0.0f, 0.0f, 1.0f}
        : (henka_vec3){0.0f, 1.0f, 0.0f};
}

static henka_vec3 sandbox3d_compass_slerp(
    henka_vec3 start,
    henka_vec3 target,
    float amount)
{
    float dot;
    float theta;
    float sine_theta;
    henka_vec3 axis;

    start = henka_vec3_normalize(start);
    target = henka_vec3_normalize(target);
    dot = sandbox3d_compass_clamp(henka_vec3_dot(start, target), -1.0f, 1.0f);
    amount = sandbox3d_compass_clamp(amount, 0.0f, 1.0f);
    if (dot > 0.9995f)
    {
        return henka_vec3_normalize(henka_vec3_add(
            henka_vec3_scale(start, 1.0f - amount),
            henka_vec3_scale(target, amount)));
    }
    if (dot < -0.9995f)
    {
        axis = henka_vec3_cross(start, (henka_vec3){0.0f, 1.0f, 0.0f});
        if (henka_vec3_length(axis) <= 0.0001f)
        {
            axis = henka_vec3_cross(start, (henka_vec3){1.0f, 0.0f, 0.0f});
        }
        axis = henka_vec3_normalize(axis);
        theta = HENKA_PI * amount;
        return henka_vec3_add(
            henka_vec3_scale(start, cosf(theta)),
            henka_vec3_scale(henka_vec3_cross(axis, start), sinf(theta)));
    }

    theta = acosf(dot);
    sine_theta = sinf(theta);
    if (!isfinite(sine_theta) || fabsf(sine_theta) <= 0.0001f)
    {
        return henka_vec3_normalize(henka_vec3_add(
            henka_vec3_scale(start, 1.0f - amount),
            henka_vec3_scale(target, amount)));
    }
    return henka_vec3_add(
        henka_vec3_scale(start, sinf((1.0f - amount) * theta) / sine_theta),
        henka_vec3_scale(target, sinf(amount * theta) / sine_theta));
}

static bool sandbox3d_compass_project_world(
    const henka_camera* camera,
    henka_vec3 world_point,
    float radius,
    henka_vec2 center,
    henka_vec2* out_point,
    bool* out_front)
{
    henka_vec3 right;
    henka_vec3 up;
    henka_vec3 forward;
    float depth;

    if (out_point != NULL)
    {
        *out_point = (henka_vec2){0.0f, 0.0f};
    }
    if (out_front != NULL)
    {
        *out_front = false;
    }
    if (camera == NULL || out_point == NULL || out_front == NULL ||
        !henka_camera_is_valid(camera) || !sandbox3d_compass_vec3_is_finite(world_point) ||
        !isfinite(radius) || radius <= 0.0f || !sandbox3d_compass_vec2_is_finite(center))
    {
        return false;
    }

    right = henka_camera_get_right(camera);
    up = henka_camera_get_up(camera);
    forward = henka_camera_get_forward(camera);
    depth = henka_vec3_dot(world_point, forward);
    out_point->x = center.x + radius * henka_vec3_dot(world_point, right);
    out_point->y = center.y - radius * henka_vec3_dot(world_point, up);
    *out_front = depth >= 0.0f;
    return sandbox3d_compass_vec2_is_finite(*out_point) && isfinite(depth);
}

void sandbox3d_view_compass_state_reset(
    sandbox3d_view_compass_state* state)
{
    if (state == NULL)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
}

bool sandbox3d_view_compass_compute_layout(
    henka_viewport viewport,
    sandbox3d_view_compass_preferences* preferences,
    sandbox3d_view_compass_layout* out_layout)
{
    float diameter;
    float available_width;
    float available_height;
    float display_scale;
    float info_width;
    float x;
    float y;

    if (out_layout != NULL)
    {
        *out_layout = (sandbox3d_view_compass_layout){0};
    }
    if (out_layout == NULL || preferences == NULL || !preferences->visible ||
        viewport.width <= 0 || viewport.height <= 0 ||
        !sandbox3d_view_compass_preferences_validate(preferences))
    {
        return false;
    }

    diameter = 96.0f * preferences->scale;
    display_scale = preferences->scale;
    available_width = (float)viewport.width - 24.0f;
    available_height = (float)viewport.height - 44.0f;
    if (diameter > available_width)
    {
        display_scale *= available_width / diameter;
        diameter = available_width;
    }
    if (diameter > available_height)
    {
        display_scale *= available_height / diameter;
        diameter = available_height;
    }
    diameter = sandbox3d_compass_clamp(diameter, SANDBOX3D_VIEW_COMPASS_MIN_DIAMETER, SANDBOX3D_VIEW_COMPASS_MAX_DIAMETER);
    display_scale = diameter / 96.0f;
    info_width = SANDBOX3D_VIEW_COMPASS_INFO_WIDTH * display_scale;
    x = preferences->side == SANDBOX3D_VIEW_COMPASS_SIDE_LEFT
        ? (float)viewport.x + 12.0f
        : (float)viewport.x + (float)viewport.width - diameter - 12.0f;
    y = (float)viewport.y + 12.0f;
    if (x < (float)viewport.x + 4.0f ||
        x + diameter > (float)viewport.x + (float)viewport.width - 4.0f ||
        y < (float)viewport.y + 4.0f ||
        y + diameter > (float)viewport.y + (float)viewport.height - 4.0f)
    {
        return false;
    }

    *out_layout = (sandbox3d_view_compass_layout){
        (henka_ui_rect){x, y, diameter, diameter},
        (henka_ui_rect){x + (diameter - info_width) * 0.5f, y + diameter + 5.0f * display_scale, info_width, SANDBOX3D_VIEW_COMPASS_INFO_HEIGHT * display_scale},
        (henka_vec2){x + diameter * 0.5f, y + diameter * 0.5f},
        diameter * 0.5f,
        display_scale};
    return true;
}

float sandbox3d_view_compass_normalize_heading_degrees(float degrees)
{
    float normalized;

    if (!isfinite(degrees))
    {
        return 0.0f;
    }
    normalized = fmodf(degrees, 360.0f);
    if (normalized < 0.0f)
    {
        normalized += 360.0f;
    }
    return normalized >= 360.0f ? 0.0f : normalized;
}

bool sandbox3d_view_compass_get_heading_degrees(
    const henka_camera* camera,
    sandbox3d_view_compass_state* state,
    float* out_heading_degrees)
{
    henka_vec3 forward;
    float horizontal_length;
    float heading;

    if (out_heading_degrees != NULL)
    {
        *out_heading_degrees = 0.0f;
    }
    if (camera == NULL || state == NULL || out_heading_degrees == NULL ||
        !henka_camera_is_valid(camera))
    {
        return false;
    }
    forward = henka_camera_get_forward(camera);
    horizontal_length = hypotf(forward.x, forward.z);
    if (!isfinite(horizontal_length) || horizontal_length <= 0.0001f)
    {
        if (!state->last_heading_valid)
        {
            return false;
        }
        *out_heading_degrees = state->last_heading_degrees;
        return true;
    }
    heading = sandbox3d_view_compass_normalize_heading_degrees(
        atan2f(forward.z, forward.x) * HENKA_RAD_TO_DEG);
    state->last_heading_degrees = heading;
    state->last_heading_valid = true;
    *out_heading_degrees = heading;
    return true;
}

bool sandbox3d_view_compass_project_direction(
    const henka_camera* camera,
    henka_vec3 direction,
    float radius,
    henka_vec2 center,
    henka_vec2* out_point,
    bool* out_front)
{
    float length;

    if (!sandbox3d_compass_vec3_is_finite(direction))
    {
        return false;
    }
    length = henka_vec3_length(direction);
    if (!isfinite(length) || length <= 0.0001f)
    {
        return false;
    }
    return sandbox3d_compass_project_world(
        camera,
        henka_vec3_scale(direction, 1.0f / length),
        radius,
        center,
        out_point,
        out_front);
}

bool sandbox3d_view_compass_build_axis_camera(
    const henka_camera* current_camera,
    henka_vec3 target,
    sandbox3d_view_compass_axis_view view,
    henka_camera* out_camera)
{
    henka_camera candidate;
    henka_vec3 direction;
    float distance;

    if (out_camera != NULL)
    {
        *out_camera = (henka_camera){0};
    }
    if (current_camera == NULL || out_camera == NULL ||
        !henka_camera_is_valid(current_camera) || !sandbox3d_compass_vec3_is_finite(target) ||
        view < SANDBOX3D_VIEW_COMPASS_FRONT || view >= SANDBOX3D_VIEW_COMPASS_AXIS_VIEW_COUNT)
    {
        return false;
    }
    direction = sandbox3d_compass_direction_for_view(view);
    distance = henka_vec3_length(henka_vec3_subtract(target, current_camera->position));
    if (!isfinite(distance) || distance <= 0.0001f)
    {
        distance = 6.0f;
    }
    candidate = *current_camera;
    candidate.position = henka_vec3_subtract(target, henka_vec3_scale(direction, distance));
    if (!henka_camera_look_at_with_up(
            &candidate,
            target,
            sandbox3d_compass_deterministic_up(direction)) ||
        !henka_camera_is_valid(&candidate))
    {
        return false;
    }
    *out_camera = candidate;
    return true;
}

bool sandbox3d_view_compass_begin_snap(
    sandbox3d_view_compass_state* state,
    const henka_camera* current_camera,
    henka_vec3 target,
    sandbox3d_view_compass_axis_view view,
    bool smooth_navigation)
{
    henka_camera target_camera;

    if (state == NULL || current_camera == NULL ||
        !sandbox3d_view_compass_build_axis_camera(current_camera, target, view, &target_camera))
    {
        return false;
    }
    state->transition_start_camera = *current_camera;
    state->transition_target_camera = target_camera;
    state->transition_target = target;
    state->transition_target_valid = true;
    state->transition_elapsed_seconds = 0.0;
    state->transition_duration_seconds = 0.24;
    state->transition_active = smooth_navigation;
    if (!smooth_navigation)
    {
        state->transition_active = false;
    }
    return true;
}

void sandbox3d_view_compass_cancel_transition(
    sandbox3d_view_compass_state* state)
{
    if (state != NULL)
    {
        state->transition_active = false;
    }
}

void sandbox3d_view_compass_update_transition(
    sandbox3d_view_compass_state* state,
    henka_camera* in_out_camera,
    double delta_seconds)
{
    double normalized_time;
    float amount;
    float distance_start;
    float distance_target;
    henka_vec3 start_direction;
    henka_vec3 target_direction;
    henka_vec3 direction;
    henka_camera candidate;

    if (state == NULL || in_out_camera == NULL || !state->transition_active ||
        !henka_camera_is_valid(in_out_camera) || !isfinite(delta_seconds) || delta_seconds < 0.0)
    {
        return;
    }
    state->transition_elapsed_seconds += delta_seconds;
    normalized_time = state->transition_duration_seconds > 0.0
        ? state->transition_elapsed_seconds / state->transition_duration_seconds
        : 1.0;
    if (normalized_time >= 1.0)
    {
        *in_out_camera = state->transition_target_camera;
        state->transition_active = false;
        return;
    }
    amount = (float)sandbox3d_compass_clamp((float)normalized_time, 0.0f, 1.0f);
    amount = amount * amount * (3.0f - 2.0f * amount);
    start_direction = henka_camera_get_forward(&state->transition_start_camera);
    target_direction = henka_camera_get_forward(&state->transition_target_camera);
    direction = sandbox3d_compass_slerp(start_direction, target_direction, amount);
    distance_start = henka_vec3_length(henka_vec3_subtract(
        state->transition_start_camera.position, state->transition_target));
    distance_target = henka_vec3_length(henka_vec3_subtract(
        state->transition_target_camera.position, state->transition_target));
    if (!isfinite(distance_start) || distance_start <= 0.0001f)
    {
        distance_start = distance_target;
    }
    candidate = state->transition_start_camera;
    candidate.position = henka_vec3_subtract(
        state->transition_target,
        henka_vec3_scale(
            direction,
            distance_start + (distance_target - distance_start) * amount));
    if (henka_camera_look_at_with_up(
            &candidate,
            state->transition_target,
            sandbox3d_compass_deterministic_up(direction)) &&
        henka_camera_is_valid(&candidate))
    {
        *in_out_camera = candidate;
    }
}

bool sandbox3d_view_compass_toggle_projection(
    henka_camera* in_out_camera,
    henka_vec3 target,
    bool target_valid)
{
    henka_camera candidate;
    float distance;
    float tangent;

    if (in_out_camera == NULL || !target_valid || !sandbox3d_compass_vec3_is_finite(target) ||
        !henka_camera_is_valid(in_out_camera))
    {
        return false;
    }
    candidate = *in_out_camera;
    distance = henka_vec3_length(henka_vec3_subtract(target, candidate.position));
    tangent = tanf(candidate.field_of_view_radians * 0.5f);
    if (!isfinite(distance) || distance <= 0.0001f || !isfinite(tangent) || tangent <= 0.001f)
    {
        return false;
    }
    if (candidate.projection_mode == HENKA_CAMERA_PROJECTION_PERSPECTIVE)
    {
        candidate.projection_mode = HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC;
        candidate.orthographic_height = sandbox3d_compass_clamp(2.0f * distance * tangent, 0.5f, 80.0f);
    }
    else
    {
        candidate.projection_mode = HENKA_CAMERA_PROJECTION_PERSPECTIVE;
        distance = sandbox3d_compass_clamp(candidate.orthographic_height / (2.0f * tangent), 0.5f, 80.0f);
        candidate.position = henka_vec3_subtract(
            target,
            henka_vec3_scale(henka_camera_get_forward(&candidate), distance));
    }
    if (!henka_camera_is_valid(&candidate))
    {
        return false;
    }
    *in_out_camera = candidate;
    return true;
}

static bool sandbox3d_compass_point_inside(
    henka_vec2 point,
    henka_vec2 center,
    float radius)
{
    const henka_vec2 delta = sandbox3d_compass_subtract(point, center);
    return sandbox3d_compass_vec2_length(delta) <= radius;
}

static void sandbox3d_compass_draw_circle_fill(
    henka_ui_context* ui,
    henka_vec2 center,
    float radius,
    henka_vec4 color)
{
    size_t index;

    for (index = 0U; index < 13U; ++index)
    {
        const float normalized = ((float)index - 6.0f) / 6.0f;
        const float half_width = radius * sqrtf(fmaxf(0.0f, 1.0f - normalized * normalized));
        const float height = radius * 2.0f / 13.0f + 0.75f;
        if (half_width > 0.0f)
        {
            (void)henka_ui_overlay_rect(
                ui,
                (henka_ui_rect){center.x - half_width, center.y - radius + (float)index * height, half_width * 2.0f, height},
                color);
        }
    }
}

static void sandbox3d_compass_draw_world_ring(
    henka_ui_context* ui,
    const henka_camera* camera,
    henka_vec2 center,
    float radius,
    henka_vec3 (*sample)(float),
    henka_vec4 front_color,
    henka_vec4 back_color,
    float thickness)
{
    size_t index;
    henka_vec2 previous_point = {0.0f, 0.0f};
    bool previous_front = false;
    bool previous_valid = false;

    for (index = 0U; index <= SANDBOX3D_VIEW_COMPASS_RING_SAMPLES; ++index)
    {
        const float angle = HENKA_PI * 2.0f * (float)(index % SANDBOX3D_VIEW_COMPASS_RING_SAMPLES) /
            (float)SANDBOX3D_VIEW_COMPASS_RING_SAMPLES;
        henka_vec2 point;
        bool front;
        if (!sandbox3d_view_compass_project_direction(camera, sample(angle), radius, center, &point, &front))
        {
            previous_valid = false;
            continue;
        }
        if (previous_valid)
        {
            (void)henka_ui_overlay_line(
                ui,
                previous_point,
                point,
                thickness,
                previous_front ? front_color : back_color);
        }
        previous_point = point;
        previous_front = front;
        previous_valid = true;
    }
}

static henka_vec3 sandbox3d_compass_horizontal_sample(float angle)
{
    return (henka_vec3){cosf(angle), 0.0f, sinf(angle)};
}

static henka_vec3 sandbox3d_compass_longitude_sample(float angle)
{
    return (henka_vec3){cosf(angle), sinf(angle), 0.0f};
}

static henka_vec3 sandbox3d_compass_meridian_sample(float angle)
{
    return (henka_vec3){0.0f, sinf(angle), cosf(angle)};
}

static henka_vec3 sandbox3d_compass_latitude_sample(float angle)
{
    const float latitude = 0.52f;
    const float ring_radius = cosf(latitude);
    return (henka_vec3){ring_radius * cosf(angle), sinf(latitude), ring_radius * sinf(angle)};
}

static bool sandbox3d_compass_draw_marker(
    henka_ui_context* ui,
    const henka_camera* camera,
    henka_vec2 center,
    float radius,
    henka_vec3 direction,
    const char* label,
    sandbox3d_view_compass_axis_view view,
    bool enabled,
    sandbox3d_view_compass_state* state,
    henka_camera* in_out_camera,
    henka_vec3 target,
    bool target_valid,
    bool smooth_navigation,
    float scale)
{
    henka_ui_interaction_state interaction;
    henka_vec2 point;
    bool front;
    henka_ui_rect hit_bounds;

    if (!sandbox3d_view_compass_project_direction(camera, direction, radius * 1.02f, center, &point, &front))
    {
        return false;
    }
    hit_bounds = (henka_ui_rect){point.x - 11.0f * scale, point.y - 11.0f * scale, 22.0f * scale, 22.0f * scale};
    interaction = (henka_ui_interaction_state){0};
    if (henka_ui_custom_interaction(
            ui,
            label,
            henka_ui_rect_contains(hit_bounds, henka_ui_get_mouse_position(ui)),
            enabled,
            &interaction) != HENKA_SUCCESS)
    {
        return false;
    }
    (void)henka_ui_overlay_line(
        ui,
        (henka_vec2){point.x - 4.0f * scale, point.y},
        (henka_vec2){point.x, point.y - 4.0f * scale},
        interaction.hovered ? 2.0f : 1.0f,
        front ? (henka_vec4){0.90f, 0.93f, 0.97f, 0.90f} : (henka_vec4){0.45f, 0.52f, 0.60f, 0.35f});
    (void)henka_ui_overlay_line(
        ui,
        (henka_vec2){point.x, point.y - 4.0f * scale},
        (henka_vec2){point.x + 4.0f * scale, point.y},
        interaction.hovered ? 2.0f : 1.0f,
        front ? (henka_vec4){0.90f, 0.93f, 0.97f, 0.90f} : (henka_vec4){0.45f, 0.52f, 0.60f, 0.35f});
    (void)henka_ui_label_colored(
        ui,
        point.x - 3.0f * scale,
        point.y - 4.0f * scale,
        scale,
        label,
        interaction.hovered ? HENKA_UI_COLOR_INFO : (front ? HENKA_UI_COLOR_NORMAL : HENKA_UI_COLOR_MUTED));
    if (interaction.released && target_valid &&
        sandbox3d_view_compass_begin_snap(state, in_out_camera, target, view, smooth_navigation))
    {
        if (!smooth_navigation)
        {
            *in_out_camera = state->transition_target_camera;
        }
        return true;
    }
    return false;
}

bool sandbox3d_view_compass_draw(
    henka_ui_context* ui,
    henka_viewport viewport,
    henka_camera* in_out_camera,
    henka_vec3* in_out_target,
    bool* in_out_target_valid,
    sandbox3d_view_compass_state* state,
    sandbox3d_view_compass_preferences* preferences,
    bool input_blocked,
    double delta_seconds)
{
    sandbox3d_view_compass_layout layout;
    henka_vec2 mouse;
    henka_ui_interaction_state globe_interaction;
    henka_ui_interaction_state projection_interaction;
    henka_ui_interaction_state info_interaction;
    henka_vec2 drag_delta;
    float heading;
    float scale;
    char info[64];
    bool changed = false;
    bool target_valid;
    size_t info_mode;

    if (ui == NULL || in_out_camera == NULL || in_out_target == NULL ||
        in_out_target_valid == NULL || state == NULL || preferences == NULL ||
        !henka_camera_is_valid(in_out_camera) ||
        !sandbox3d_view_compass_preferences_validate(preferences) ||
        !sandbox3d_view_compass_compute_layout(viewport, preferences, &layout))
    {
        return false;
    }

    target_valid = *in_out_target_valid && sandbox3d_compass_vec3_is_finite(*in_out_target);
    sandbox3d_view_compass_update_transition(state, in_out_camera, delta_seconds);
    changed = state->transition_active;
    scale = layout.scale;
    mouse = henka_ui_get_mouse_position(ui);

    sandbox3d_compass_draw_circle_fill(
        ui,
        layout.center,
        layout.radius,
        input_blocked ? (henka_vec4){0.045f, 0.055f, 0.070f, 0.78f} : (henka_vec4){0.045f, 0.060f, 0.080f, 0.94f});
    (void)henka_ui_overlay_line(
        ui,
        (henka_vec2){layout.center.x + layout.radius, layout.center.y},
        (henka_vec2){layout.center.x + layout.radius, layout.center.y},
        1.0f,
        (henka_vec4){0.26f, 0.38f, 0.52f, 0.85f});
    sandbox3d_compass_draw_world_ring(ui, in_out_camera, layout.center, layout.radius * 0.76f, sandbox3d_compass_horizontal_sample,
        (henka_vec4){0.62f, 0.72f, 0.82f, 0.72f}, (henka_vec4){0.37f, 0.44f, 0.52f, 0.22f}, 1.0f * scale);
    sandbox3d_compass_draw_world_ring(ui, in_out_camera, layout.center, layout.radius * 0.72f, sandbox3d_compass_longitude_sample,
        (henka_vec4){0.70f, 0.76f, 0.82f, 0.48f}, (henka_vec4){0.38f, 0.44f, 0.52f, 0.18f}, 0.7f * scale);
    sandbox3d_compass_draw_world_ring(ui, in_out_camera, layout.center, layout.radius * 0.72f, sandbox3d_compass_meridian_sample,
        (henka_vec4){0.70f, 0.76f, 0.82f, 0.42f}, (henka_vec4){0.38f, 0.44f, 0.52f, 0.16f}, 0.7f * scale);
    sandbox3d_compass_draw_world_ring(ui, in_out_camera, layout.center, layout.radius * 0.66f, sandbox3d_compass_latitude_sample,
        (henka_vec4){0.58f, 0.68f, 0.78f, 0.32f}, (henka_vec4){0.35f, 0.42f, 0.50f, 0.14f}, 0.6f * scale);
    sandbox3d_compass_draw_world_ring(ui, in_out_camera, layout.center, layout.radius * 0.96f, sandbox3d_compass_horizontal_sample,
        (henka_vec4){0.42f, 0.66f, 0.92f, 0.78f}, (henka_vec4){0.26f, 0.38f, 0.52f, 0.28f}, 1.4f * scale);

    if (sandbox3d_view_compass_get_heading_degrees(in_out_camera, state, &heading))
    {
        const float angle = heading * HENKA_DEG_TO_RAD;
        henka_vec2 heading_point;
        bool heading_front;
        if (sandbox3d_view_compass_project_direction(
                in_out_camera,
                (henka_vec3){cosf(angle), 0.0f, sinf(angle)},
                layout.radius * 0.90f,
                layout.center,
                &heading_point,
                &heading_front))
        {
            (void)henka_ui_overlay_line(ui, layout.center, heading_point, 2.0f * scale,
                input_blocked ? (henka_vec4){0.25f, 0.45f, 0.66f, 0.35f} : (henka_vec4){0.28f, 0.72f, 1.0f, 0.92f});
        }
    }

    if (!input_blocked)
    {
        changed |= sandbox3d_compass_draw_marker(ui, in_out_camera, layout.center, layout.radius, (henka_vec3){1.0f, 0.0f, 0.0f}, "N", SANDBOX3D_VIEW_COMPASS_FRONT, true, state, in_out_camera, *in_out_target, target_valid, preferences->smooth_navigation, scale);
        changed |= sandbox3d_compass_draw_marker(ui, in_out_camera, layout.center, layout.radius, (henka_vec3){-1.0f, 0.0f, 0.0f}, "S", SANDBOX3D_VIEW_COMPASS_BACK, true, state, in_out_camera, *in_out_target, target_valid, preferences->smooth_navigation, scale);
        changed |= sandbox3d_compass_draw_marker(ui, in_out_camera, layout.center, layout.radius, (henka_vec3){0.0f, 0.0f, 1.0f}, "E", SANDBOX3D_VIEW_COMPASS_LEFT, true, state, in_out_camera, *in_out_target, target_valid, preferences->smooth_navigation, scale);
        changed |= sandbox3d_compass_draw_marker(ui, in_out_camera, layout.center, layout.radius, (henka_vec3){0.0f, 0.0f, -1.0f}, "W", SANDBOX3D_VIEW_COMPASS_RIGHT, true, state, in_out_camera, *in_out_target, target_valid, preferences->smooth_navigation, scale);
        changed |= sandbox3d_compass_draw_marker(ui, in_out_camera, layout.center, layout.radius, (henka_vec3){0.0f, 1.0f, 0.0f}, "B", SANDBOX3D_VIEW_COMPASS_BOTTOM, true, state, in_out_camera, *in_out_target, target_valid, preferences->smooth_navigation, scale);
        changed |= sandbox3d_compass_draw_marker(ui, in_out_camera, layout.center, layout.radius, (henka_vec3){0.0f, -1.0f, 0.0f}, "T", SANDBOX3D_VIEW_COMPASS_TOP, true, state, in_out_camera, *in_out_target, target_valid, preferences->smooth_navigation, scale);
    }

    projection_interaction = (henka_ui_interaction_state){0};
    (void)henka_ui_custom_interaction(
        ui,
        "view_compass_projection",
        henka_ui_rect_contains(
            (henka_ui_rect){layout.center.x - 15.0f * scale, layout.center.y + layout.radius * 0.35f, 30.0f * scale, 18.0f * scale},
            mouse),
        !input_blocked,
        &projection_interaction);
    if (projection_interaction.released &&
        sandbox3d_view_compass_toggle_projection(in_out_camera, *in_out_target, target_valid))
    {
        changed = true;
    }
    (void)henka_ui_label_colored(
        ui,
        layout.center.x - 4.0f * scale,
        layout.center.y + layout.radius * 0.36f,
        scale,
        in_out_camera->projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC ? "O" : "P",
        projection_interaction.hovered ? HENKA_UI_COLOR_INFO : HENKA_UI_COLOR_MUTED);

    globe_interaction = (henka_ui_interaction_state){0};
    (void)henka_ui_custom_interaction(
        ui,
        "view_compass_globe",
        sandbox3d_compass_point_inside(mouse, layout.center, layout.radius * 0.82f),
        !input_blocked,
        &globe_interaction);
    if (globe_interaction.pressed)
    {
        state->drag_active = true;
        state->drag_moved = false;
        state->drag_start_pointer = mouse;
        state->drag_last_pointer = mouse;
        sandbox3d_view_compass_cancel_transition(state);
    }
    if (state->drag_active && globe_interaction.held)
    {
        drag_delta = sandbox3d_compass_subtract(mouse, state->drag_last_pointer);
        if (sandbox3d_compass_vec2_length(sandbox3d_compass_subtract(mouse, state->drag_start_pointer)) > SANDBOX3D_VIEW_COMPASS_DRAG_THRESHOLD)
        {
            state->drag_moved = true;
        }
        if (state->drag_moved && target_valid)
        {
            if (henka_camera_orbit_target(in_out_camera, *in_out_target, -drag_delta.x * 0.006f, -drag_delta.y * 0.006f))
            {
                changed = true;
            }
        }
        state->drag_last_pointer = mouse;
    }
    if (globe_interaction.released)
    {
        state->drag_active = false;
    }

    if (preferences->show_info)
    {
        const float heading_value = state->last_heading_valid ? state->last_heading_degrees : 0.0f;
        const float pitch_degrees = in_out_camera->pitch_radians * HENKA_RAD_TO_DEG;
        switch (preferences->info_mode)
        {
            case SANDBOX3D_VIEW_COMPASS_INFO_POSITION:
                snprintf(info, sizeof(info), "X %.1f Y %.1f Z %.1f", in_out_camera->position.x, in_out_camera->position.y, in_out_camera->position.z);
                break;
            case SANDBOX3D_VIEW_COMPASS_INFO_TARGET:
                if (target_valid)
                {
                    const float distance = henka_vec3_length(henka_vec3_subtract(*in_out_target, in_out_camera->position));
                    snprintf(info, sizeof(info), "T %.1f D %.1f", in_out_target->x, distance);
                }
                else
                {
                    snprintf(info, sizeof(info), "No target");
                }
                break;
            case SANDBOX3D_VIEW_COMPASS_INFO_ORIENTATION:
            default:
                snprintf(info, sizeof(info), "H %03.0f P %+03.0f %s", heading_value, pitch_degrees,
                    in_out_camera->projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC ? "Ortho" : "Persp");
                break;
        }
        (void)henka_ui_overlay_rect(ui, layout.info_bounds, (henka_vec4){0.045f, 0.055f, 0.070f, 0.92f});
        (void)henka_ui_overlay_line(ui, (henka_vec2){layout.info_bounds.x, layout.info_bounds.y}, (henka_vec2){layout.info_bounds.x + layout.info_bounds.width, layout.info_bounds.y}, 1.0f, (henka_vec4){0.25f, 0.45f, 0.66f, 0.80f});
        (void)henka_ui_label_colored(ui, layout.info_bounds.x + 5.0f * scale, layout.info_bounds.y + 8.0f * scale, scale, info, HENKA_UI_COLOR_NORMAL);
        info_interaction = (henka_ui_interaction_state){0};
        (void)henka_ui_custom_interaction(ui, "view_compass_info", henka_ui_rect_contains(layout.info_bounds, mouse), !input_blocked, &info_interaction);
        if (info_interaction.released)
        {
            info_mode = ((size_t)preferences->info_mode + 1U) % (size_t)SANDBOX3D_VIEW_COMPASS_INFO_COUNT;
            preferences->info_mode = (sandbox3d_view_compass_info_mode)info_mode;
            changed = true;
        }
    }
    return changed;
}
