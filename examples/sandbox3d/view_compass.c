#include "view_compass.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define SANDBOX3D_VIEW_COMPASS_RING_SAMPLES 48U
#define SANDBOX3D_VIEW_COMPASS_SMALL_DIAMETER 160.0f
#define SANDBOX3D_VIEW_COMPASS_NORMAL_DIAMETER 192.0f
#define SANDBOX3D_VIEW_COMPASS_LARGE_DIAMETER 224.0f
#define SANDBOX3D_VIEW_COMPASS_MIN_DIAMETER 128.0f
#define SANDBOX3D_VIEW_COMPASS_MAX_DIAMETER SANDBOX3D_VIEW_COMPASS_LARGE_DIAMETER
#define SANDBOX3D_VIEW_COMPASS_INFO_HEIGHT 29.0f
#define SANDBOX3D_VIEW_COMPASS_INFO_WIDTH 156.0f
#define SANDBOX3D_VIEW_COMPASS_INFO_PROJECTION_WIDTH 48.0f
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

    diameter = SANDBOX3D_VIEW_COMPASS_NORMAL_DIAMETER * preferences->scale;
    display_scale = preferences->scale;
    available_width = (float)viewport.width - 24.0f;
    available_height = (float)viewport.height - 52.0f;
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
    if (available_width < SANDBOX3D_VIEW_COMPASS_SMALL_DIAMETER ||
        available_height < SANDBOX3D_VIEW_COMPASS_SMALL_DIAMETER)
    {
        diameter = sandbox3d_compass_clamp(
            fminf(available_width, available_height),
            SANDBOX3D_VIEW_COMPASS_MIN_DIAMETER,
            SANDBOX3D_VIEW_COMPASS_MAX_DIAMETER);
    }
    else
    {
        diameter = sandbox3d_compass_clamp(
            diameter,
            SANDBOX3D_VIEW_COMPASS_SMALL_DIAMETER,
            SANDBOX3D_VIEW_COMPASS_MAX_DIAMETER);
    }
    display_scale = diameter / SANDBOX3D_VIEW_COMPASS_NORMAL_DIAMETER;
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

    {
        const float info_y = y + diameter + 6.0f * display_scale;
        const float info_height = SANDBOX3D_VIEW_COMPASS_INFO_HEIGHT * display_scale;
        const henka_ui_rect info_bounds = {
            x + (diameter - info_width) * 0.5f,
            info_y,
            info_width,
            info_height};
        const float projection_width = SANDBOX3D_VIEW_COMPASS_INFO_PROJECTION_WIDTH * display_scale;
        const henka_ui_rect projection_bounds = {
            info_bounds.x + info_bounds.width - projection_width - 3.0f * display_scale,
            info_bounds.y + 3.0f * display_scale,
            projection_width,
            info_height - 6.0f * display_scale};
        *out_layout = (sandbox3d_view_compass_layout){
            (henka_ui_rect){x, y, diameter, diameter},
            info_bounds,
            (henka_ui_rect){
                info_bounds.x,
                info_bounds.y,
                info_bounds.width - projection_width - 5.0f * display_scale,
                info_bounds.height},
            projection_bounds,
            (henka_vec2){x + diameter * 0.5f, y + diameter * 0.5f},
            diameter * 0.5f,
            display_scale};
    }
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

static void sandbox3d_compass_draw_world_ring_layer(
    henka_ui_context* ui,
    const henka_camera* camera,
    henka_vec2 center,
    float radius,
    henka_vec3 (*sample)(float),
    bool front_layer,
    henka_vec4 color,
    float thickness)
{
    henka_vec2 points[SANDBOX3D_VIEW_COMPASS_RING_SAMPLES];
    bool fronts[SANDBOX3D_VIEW_COMPASS_RING_SAMPLES];
    size_t index;

    for (index = 0U; index < SANDBOX3D_VIEW_COMPASS_RING_SAMPLES; ++index)
    {
        const float angle = HENKA_PI * 2.0f * (float)index /
            (float)SANDBOX3D_VIEW_COMPASS_RING_SAMPLES;
        if (!sandbox3d_view_compass_project_direction(
                camera,
                sample(angle),
                radius,
                center,
                &points[index],
                &fronts[index]))
        {
            return;
        }
    }
    for (index = 0U; index < SANDBOX3D_VIEW_COMPASS_RING_SAMPLES; ++index)
    {
        const size_t next = (index + 1U) % SANDBOX3D_VIEW_COMPASS_RING_SAMPLES;
        if (fronts[index] == front_layer && fronts[next] == front_layer)
        {
            (void)henka_ui_overlay_line(
                ui,
                points[index],
                points[next],
                thickness,
                color);
        }
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

static henka_vec3 sandbox3d_compass_lower_latitude_sample(float angle)
{
    const float latitude = -0.52f;
    const float ring_radius = cosf(latitude);
    return (henka_vec3){ring_radius * cosf(angle), sinf(latitude), ring_radius * sinf(angle)};
}

static void sandbox3d_compass_draw_centered_label(
    henka_ui_context* ui,
    henka_vec2 point,
    float scale,
    const char* label,
    henka_ui_semantic_color color)
{
    int width = 0;
    int height = 0;

    if (ui == NULL || label == NULL || label[0] == '\0' || scale <= 0.0f)
    {
        return;
    }
    if (henka_ui_measure_text_for_context(ui, label, scale, &width, &height) != HENKA_SUCCESS)
    {
        width = 0;
        height = 7;
    }
    (void)henka_ui_label_colored(
        ui,
        point.x - (float)width * 0.5f,
        point.y - (float)height * 0.5f,
        scale,
        label,
        color);
}

static void sandbox3d_compass_draw_cardinal_glyph(
    henka_ui_context* ui,
    henka_vec2 center,
    const char* label,
    float scale,
    henka_vec4 color)
{
    const float left = center.x - 3.0f * scale;
    const float right = center.x + 3.0f * scale;
    const float top = center.y - 4.0f * scale;
    const float middle = center.y;
    const float bottom = center.y + 4.0f * scale;
    const float stroke = 1.25f * scale;

    if (ui == NULL || label == NULL || label[0] == '\0' || label[1] != '\0' ||
        scale <= 0.0f)
    {
        return;
    }
    switch (label[0])
    {
        case 'N':
            (void)henka_ui_overlay_line(ui, (henka_vec2){left, bottom}, (henka_vec2){left, top}, stroke, color);
            (void)henka_ui_overlay_line(ui, (henka_vec2){left, top}, (henka_vec2){right, bottom}, stroke, color);
            (void)henka_ui_overlay_line(ui, (henka_vec2){right, bottom}, (henka_vec2){right, top}, stroke, color);
            break;
        case 'E':
            (void)henka_ui_overlay_line(ui, (henka_vec2){right, top}, (henka_vec2){left, top}, stroke, color);
            (void)henka_ui_overlay_line(ui, (henka_vec2){left, top}, (henka_vec2){left, bottom}, stroke, color);
            (void)henka_ui_overlay_line(ui, (henka_vec2){left, middle}, (henka_vec2){right - scale, middle}, stroke, color);
            (void)henka_ui_overlay_line(ui, (henka_vec2){left, bottom}, (henka_vec2){right, bottom}, stroke, color);
            break;
        case 'S':
            (void)henka_ui_overlay_line(ui, (henka_vec2){right, top}, (henka_vec2){left, top}, stroke, color);
            (void)henka_ui_overlay_line(ui, (henka_vec2){left, top}, (henka_vec2){left, middle}, stroke, color);
            (void)henka_ui_overlay_line(ui, (henka_vec2){left, middle}, (henka_vec2){right, middle}, stroke, color);
            (void)henka_ui_overlay_line(ui, (henka_vec2){right, middle}, (henka_vec2){right, bottom}, stroke, color);
            (void)henka_ui_overlay_line(ui, (henka_vec2){right, bottom}, (henka_vec2){left, bottom}, stroke, color);
            break;
        case 'W':
            (void)henka_ui_overlay_line(ui, (henka_vec2){left, top}, (henka_vec2){left + scale, bottom}, stroke, color);
            (void)henka_ui_overlay_line(ui, (henka_vec2){left + scale, bottom}, (henka_vec2){center.x, top}, stroke, color);
            (void)henka_ui_overlay_line(ui, (henka_vec2){center.x, top}, (henka_vec2){right - scale, bottom}, stroke, color);
            (void)henka_ui_overlay_line(ui, (henka_vec2){right - scale, bottom}, (henka_vec2){right, top}, stroke, color);
            break;
        default:
            break;
    }
}

static void sandbox3d_compass_draw_ring_ticks(
    henka_ui_context* ui,
    const henka_camera* camera,
    henka_vec2 center,
    float radius,
    bool front_layer,
    float scale,
    bool highlighted)
{
    size_t index;

    for (index = 0U; index < 12U; ++index)
    {
        const float angle = HENKA_PI * 2.0f * (float)index / 12.0f;
        const henka_vec3 direction = sandbox3d_compass_horizontal_sample(angle);
        henka_vec2 outer;
        henka_vec2 inner;
        bool front;
        if (!sandbox3d_view_compass_project_direction(
                camera, direction, radius + 2.5f * scale, center, &outer, &front) ||
            front != front_layer ||
            !sandbox3d_view_compass_project_direction(
                camera, direction, radius - 3.5f * scale, center, &inner, &front))
        {
            continue;
        }
        (void)henka_ui_overlay_line(
            ui,
            inner,
            outer,
            (index % 3U == 0U ? 1.4f : 0.8f) * scale,
            front_layer
                ? (highlighted
                    ? (henka_vec4){0.28f, 0.62f, 0.92f, 0.80f}
                    : (henka_vec4){0.45f, 0.57f, 0.68f, 0.55f})
                : (henka_vec4){0.18f, 0.24f, 0.30f, 0.28f});
    }
}

static void sandbox3d_compass_draw_heading_needle(
    henka_ui_context* ui,
    const henka_camera* camera,
    henka_vec2 center,
    float radius,
    float heading,
    float scale,
    bool highlighted)
{
    henka_vec2 point;
    bool front;
    henka_vec2 radial;
    henka_vec2 tangent;
    henka_vec2 base;
    henka_vec2 left;
    henka_vec2 right;

    if (!sandbox3d_view_compass_project_direction(
            camera,
            (henka_vec3){cosf(heading * HENKA_DEG_TO_RAD), 0.0f, sinf(heading * HENKA_DEG_TO_RAD)},
            radius,
            center,
            &point,
            &front))
    {
        return;
    }
    radial = sandbox3d_compass_subtract(point, center);
    {
        const float length = sandbox3d_compass_vec2_length(radial);
        if (!isfinite(length) || length <= 0.001f)
        {
            return;
        }
        radial.x /= length;
        radial.y /= length;
    }
    tangent = (henka_vec2){-radial.y, radial.x};
    base = (henka_vec2){point.x - radial.x * 8.0f * scale, point.y - radial.y * 8.0f * scale};
    left = (henka_vec2){base.x + tangent.x * 3.5f * scale, base.y + tangent.y * 3.5f * scale};
    right = (henka_vec2){base.x - tangent.x * 3.5f * scale, base.y - tangent.y * 3.5f * scale};
    {
        const henka_vec4 color = highlighted
            ? (henka_vec4){0.28f, 0.76f, 1.0f, 0.98f}
            : (henka_vec4){0.25f, 0.55f, 0.78f, 0.70f};
        (void)henka_ui_overlay_line(ui, point, left, 1.8f * scale, color);
        (void)henka_ui_overlay_line(ui, point, right, 1.8f * scale, color);
        (void)henka_ui_overlay_line(ui, left, right, 1.2f * scale, color);
        (void)henka_ui_overlay_disc(ui, point, 2.0f * scale, color);
    }
}

static bool sandbox3d_compass_draw_marker(
    henka_ui_context* ui,
    const henka_camera* camera,
    henka_vec2 center,
    float radius,
    henka_vec3 direction,
    const char* id,
    const char* label,
    bool show_label,
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

    if (!sandbox3d_view_compass_project_direction(camera, direction, radius * 1.08f, center, &point, &front))
    {
        return false;
    }
    hit_bounds = (henka_ui_rect){point.x - 13.0f * scale, point.y - 13.0f * scale, 26.0f * scale, 26.0f * scale};
    interaction = (henka_ui_interaction_state){0};
    if (henka_ui_custom_interaction(
            ui,
            id,
            henka_ui_rect_contains(hit_bounds, henka_ui_get_mouse_position(ui)),
            enabled,
            &interaction) != HENKA_SUCCESS)
    {
        return false;
    }
    {
        const float marker_radius = (interaction.hovered ? 5.0f : 3.5f) * scale;
        const henka_vec4 marker_color = interaction.hovered
            ? (henka_vec4){0.25f, 0.70f, 1.0f, 0.96f}
            : (front
                ? (henka_vec4){0.86f, 0.91f, 0.96f, 0.94f}
                : (henka_vec4){0.42f, 0.50f, 0.60f, 0.30f});
        const henka_vec2 radial = sandbox3d_compass_subtract(point, center);
        const float radial_length = sandbox3d_compass_vec2_length(radial);
        henka_vec2 label_point = point;

        if (radial_length > 0.001f)
        {
            label_point.x += radial.x / radial_length * 8.0f * scale;
            label_point.y += radial.y / radial_length * 8.0f * scale;
        }
        (void)henka_ui_overlay_disc(ui, point, marker_radius, marker_color);
        (void)henka_ui_overlay_circle(ui, point, marker_radius + 2.0f * scale, 0.8f * scale, marker_color);
        if (show_label)
        {
            sandbox3d_compass_draw_cardinal_glyph(
                ui,
                label_point,
                label,
                1.40f * scale,
                interaction.hovered
                    ? (henka_vec4){0.48f, 0.69f, 0.87f, 1.0f}
                    : (front
                        ? (henka_vec4){0.88f, 0.94f, 1.0f, 1.0f}
                        : (henka_vec4){0.40f, 0.52f, 0.64f, 0.85f}));
        }
        else
        {
            (void)henka_ui_overlay_line(ui,
                (henka_vec2){point.x - 4.0f * scale, point.y},
                (henka_vec2){point.x, point.y - 4.0f * scale},
                1.0f * scale,
                marker_color);
            (void)henka_ui_overlay_line(ui,
                (henka_vec2){point.x, point.y - 4.0f * scale},
                (henka_vec2){point.x + 4.0f * scale, point.y},
                1.0f * scale,
                marker_color);
        }
    }
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
    bool compass_hovered;
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

    compass_hovered = !input_blocked && sandbox3d_compass_point_inside(
        mouse,
        layout.center,
        layout.radius * 0.94f);

    /* The UI line list is ordered, so the rear ring is emitted first, the
     * layered sphere then masks it, and the front ring is emitted last. */
    (void)henka_ui_overlay_disc(
        ui,
        (henka_vec2){layout.center.x + 2.0f * scale, layout.center.y + 3.0f * scale},
        layout.radius + 3.0f * scale,
        (henka_vec4){0.0f, 0.0f, 0.0f, 0.34f});
    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 1.14f,
        sandbox3d_compass_horizontal_sample, false,
        (henka_vec4){0.10f, 0.15f, 0.20f, 0.78f}, 4.0f * scale);
    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 1.145f,
        sandbox3d_compass_horizontal_sample, false,
        (henka_vec4){0.12f, 0.32f, 0.52f, 0.34f}, 1.1f * scale);

    (void)henka_ui_overlay_disc(
        ui,
        layout.center,
        layout.radius,
        input_blocked
            ? (henka_vec4){0.035f, 0.045f, 0.060f, 0.90f}
            : (henka_vec4){0.055f, 0.075f, 0.105f, 0.98f});
    (void)henka_ui_overlay_disc(
        ui,
        (henka_vec2){layout.center.x - 2.0f * scale, layout.center.y - 2.0f * scale},
        layout.radius * 0.91f,
        (henka_vec4){0.11f, 0.14f, 0.18f, 0.90f});
    (void)henka_ui_overlay_disc(
        ui,
        (henka_vec2){layout.center.x - 9.0f * scale, layout.center.y - 11.0f * scale},
        layout.radius * 0.66f,
        (henka_vec4){0.25f, 0.31f, 0.38f, 0.13f});
    (void)henka_ui_overlay_disc(
        ui,
        (henka_vec2){layout.center.x + 8.0f * scale, layout.center.y + 10.0f * scale},
        layout.radius * 0.68f,
        (henka_vec4){0.005f, 0.010f, 0.018f, 0.25f});

    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 0.80f,
        sandbox3d_compass_horizontal_sample, false,
        (henka_vec4){0.18f, 0.24f, 0.30f, 0.18f}, 1.0f * scale);
    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 0.74f,
        sandbox3d_compass_longitude_sample, false,
        (henka_vec4){0.16f, 0.22f, 0.28f, 0.13f}, 0.75f * scale);
    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 0.74f,
        sandbox3d_compass_meridian_sample, false,
        (henka_vec4){0.16f, 0.22f, 0.28f, 0.13f}, 0.75f * scale);
    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 0.66f,
        sandbox3d_compass_latitude_sample, false,
        (henka_vec4){0.18f, 0.25f, 0.32f, 0.12f}, 0.65f * scale);
    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 0.66f,
        sandbox3d_compass_lower_latitude_sample, false,
        (henka_vec4){0.18f, 0.25f, 0.32f, 0.12f}, 0.65f * scale);
    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 0.80f,
        sandbox3d_compass_horizontal_sample, true,
        (henka_vec4){0.55f, 0.67f, 0.78f, 0.58f}, 1.25f * scale);
    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 0.74f,
        sandbox3d_compass_longitude_sample, true,
        (henka_vec4){0.56f, 0.67f, 0.77f, 0.34f}, 0.8f * scale);
    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 0.74f,
        sandbox3d_compass_meridian_sample, true,
        (henka_vec4){0.56f, 0.67f, 0.77f, 0.32f}, 0.8f * scale);
    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 0.66f,
        sandbox3d_compass_latitude_sample, true,
        (henka_vec4){0.48f, 0.59f, 0.69f, 0.22f}, 0.65f * scale);
    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 0.66f,
        sandbox3d_compass_lower_latitude_sample, true,
        (henka_vec4){0.48f, 0.59f, 0.69f, 0.22f}, 0.65f * scale);

    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 1.14f,
        sandbox3d_compass_horizontal_sample, true,
        (henka_vec4){0.23f, 0.40f, 0.57f, 0.88f}, 4.0f * scale);
    sandbox3d_compass_draw_world_ring_layer(
        ui, in_out_camera, layout.center, layout.radius * 1.145f,
        sandbox3d_compass_horizontal_sample, true,
        compass_hovered || state->drag_active
            ? (henka_vec4){0.25f, 0.70f, 1.0f, 0.98f}
            : (henka_vec4){0.24f, 0.53f, 0.78f, 0.82f}, 1.15f * scale);
    sandbox3d_compass_draw_ring_ticks(
        ui, in_out_camera, layout.center, layout.radius * 1.14f,
        false, scale, compass_hovered || state->drag_active);
    sandbox3d_compass_draw_ring_ticks(
        ui, in_out_camera, layout.center, layout.radius * 1.14f,
        true, scale, compass_hovered || state->drag_active);
    (void)henka_ui_overlay_circle(
        ui,
        layout.center,
        layout.radius * 0.985f,
        1.2f * scale,
        compass_hovered || state->drag_active
            ? (henka_vec4){0.28f, 0.60f, 0.86f, 0.86f}
            : (henka_vec4){0.36f, 0.45f, 0.54f, 0.70f});

    if (sandbox3d_view_compass_get_heading_degrees(in_out_camera, state, &heading))
    {
        const float angle = heading * HENKA_DEG_TO_RAD;
        henka_vec2 heading_point;
        bool heading_front;
        if (sandbox3d_view_compass_project_direction(
                in_out_camera,
            (henka_vec3){cosf(angle), 0.0f, sinf(angle)},
                layout.radius * 1.14f,
                layout.center,
                &heading_point,
                &heading_front))
        {
            (void)heading_point;
            (void)heading_front;
            sandbox3d_compass_draw_heading_needle(
                ui,
                in_out_camera,
                layout.center,
                layout.radius * 1.14f,
                heading,
                scale,
                !input_blocked && (compass_hovered || state->drag_active));
        }
    }

    if (!input_blocked)
    {
        changed |= sandbox3d_compass_draw_marker(ui, in_out_camera, layout.center, layout.radius, (henka_vec3){1.0f, 0.0f, 0.0f}, "compass_front", "N", true, SANDBOX3D_VIEW_COMPASS_FRONT, true, state, in_out_camera, *in_out_target, target_valid, preferences->smooth_navigation, scale);
        changed |= sandbox3d_compass_draw_marker(ui, in_out_camera, layout.center, layout.radius, (henka_vec3){-1.0f, 0.0f, 0.0f}, "compass_back", "S", true, SANDBOX3D_VIEW_COMPASS_BACK, true, state, in_out_camera, *in_out_target, target_valid, preferences->smooth_navigation, scale);
        changed |= sandbox3d_compass_draw_marker(ui, in_out_camera, layout.center, layout.radius, (henka_vec3){0.0f, 0.0f, 1.0f}, "compass_left", "E", true, SANDBOX3D_VIEW_COMPASS_LEFT, true, state, in_out_camera, *in_out_target, target_valid, preferences->smooth_navigation, scale);
        changed |= sandbox3d_compass_draw_marker(ui, in_out_camera, layout.center, layout.radius, (henka_vec3){0.0f, 0.0f, -1.0f}, "compass_right", "W", true, SANDBOX3D_VIEW_COMPASS_RIGHT, true, state, in_out_camera, *in_out_target, target_valid, preferences->smooth_navigation, scale);
        changed |= sandbox3d_compass_draw_marker(ui, in_out_camera, layout.center, layout.radius, (henka_vec3){0.0f, 1.0f, 0.0f}, "compass_bottom", "", false, SANDBOX3D_VIEW_COMPASS_BOTTOM, true, state, in_out_camera, *in_out_target, target_valid, preferences->smooth_navigation, scale);
        changed |= sandbox3d_compass_draw_marker(ui, in_out_camera, layout.center, layout.radius, (henka_vec3){0.0f, -1.0f, 0.0f}, "compass_top", "", false, SANDBOX3D_VIEW_COMPASS_TOP, true, state, in_out_camera, *in_out_target, target_valid, preferences->smooth_navigation, scale);
    }

    projection_interaction = (henka_ui_interaction_state){0};
    (void)henka_ui_custom_interaction(
        ui,
        "view_compass_projection",
        henka_ui_rect_contains(layout.projection_bounds, mouse),
        !input_blocked,
        &projection_interaction);
    if (projection_interaction.released &&
        sandbox3d_view_compass_toggle_projection(in_out_camera, *in_out_target, target_valid))
    {
        changed = true;
    }
    (void)henka_ui_overlay_rect(
        ui,
        layout.projection_bounds,
        projection_interaction.hovered
            ? (henka_vec4){0.10f, 0.24f, 0.38f, 0.86f}
            : (henka_vec4){0.06f, 0.10f, 0.15f, 0.80f});
    sandbox3d_compass_draw_centered_label(
        ui,
        (henka_vec2){
            layout.projection_bounds.x + layout.projection_bounds.width * 0.5f,
            layout.projection_bounds.y + layout.projection_bounds.height * 0.5f},
        0.92f * scale,
        in_out_camera->projection_mode == HENKA_CAMERA_PROJECTION_ORTHOGRAPHIC ? "Ortho" : "Persp",
        projection_interaction.hovered ? HENKA_UI_COLOR_INFO : HENKA_UI_COLOR_NORMAL);

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
                snprintf(info, sizeof(info), "X%.1f Y%.1f Z%.1f", in_out_camera->position.x, in_out_camera->position.y, in_out_camera->position.z);
                break;
            case SANDBOX3D_VIEW_COMPASS_INFO_TARGET:
                if (target_valid)
                {
                    const float distance = henka_vec3_length(henka_vec3_subtract(*in_out_target, in_out_camera->position));
                    snprintf(info, sizeof(info), "Dist %.1f", distance);
                }
                else
                {
                    snprintf(info, sizeof(info), "No target");
                }
                break;
            case SANDBOX3D_VIEW_COMPASS_INFO_ORIENTATION:
            default:
                snprintf(info, sizeof(info), "%03.0f  %+03.0f", heading_value, pitch_degrees);
                break;
        }
        (void)henka_ui_overlay_rect(ui, layout.info_bounds, (henka_vec4){0.035f, 0.050f, 0.075f, 0.96f});
        (void)henka_ui_overlay_line(ui,
            (henka_vec2){layout.info_bounds.x, layout.info_bounds.y},
            (henka_vec2){layout.info_bounds.x + layout.info_bounds.width, layout.info_bounds.y},
            1.1f * scale,
            (henka_vec4){0.28f, 0.54f, 0.76f, 0.90f});
        (void)henka_ui_overlay_line(ui,
            (henka_vec2){layout.info_bounds.x + layout.info_bounds.width * 0.5f - 8.0f * scale, layout.info_bounds.y},
            (henka_vec2){layout.info_bounds.x + layout.info_bounds.width * 0.5f, layout.info_bounds.y - 4.0f * scale},
            1.0f * scale,
            (henka_vec4){0.24f, 0.46f, 0.66f, 0.75f});
        (void)henka_ui_overlay_line(ui,
            (henka_vec2){layout.info_bounds.x + layout.info_bounds.width * 0.5f, layout.info_bounds.y - 4.0f * scale},
            (henka_vec2){layout.info_bounds.x + layout.info_bounds.width * 0.5f + 8.0f * scale, layout.info_bounds.y},
            1.0f * scale,
            (henka_vec4){0.24f, 0.46f, 0.66f, 0.75f});
        sandbox3d_compass_draw_centered_label(
            ui,
            (henka_vec2){
                layout.info_cycle_bounds.x + layout.info_cycle_bounds.width * 0.5f,
                layout.info_cycle_bounds.y + layout.info_cycle_bounds.height * 0.5f},
            0.98f * scale,
            info,
            HENKA_UI_COLOR_NORMAL);
        info_interaction = (henka_ui_interaction_state){0};
        (void)henka_ui_custom_interaction(ui, "view_compass_info", henka_ui_rect_contains(layout.info_cycle_bounds, mouse), !input_blocked, &info_interaction);
        if (info_interaction.released)
        {
            info_mode = ((size_t)preferences->info_mode + 1U) % (size_t)SANDBOX3D_VIEW_COMPASS_INFO_COUNT;
            preferences->info_mode = (sandbox3d_view_compass_info_mode)info_mode;
            changed = true;
        }
    }
    return changed;
}
