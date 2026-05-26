#include <henka/gizmo.h>

#include <float.h>
#include <math.h>
#include <string.h>

#include <henka/math.h>
#include <henka/workspace.h>

static float henka_gizmo_absf(float value)
{
    return value < 0.0f ? -value : value;
}

static float henka_gizmo_distance_sq_2d(henka_vec2 left, henka_vec2 right)
{
    const float dx = left.x - right.x;
    const float dy = left.y - right.y;
    return dx * dx + dy * dy;
}

static henka_vec2 henka_gizmo_vec2_subtract(henka_vec2 left, henka_vec2 right)
{
    return (henka_vec2){left.x - right.x, left.y - right.y};
}

static float henka_gizmo_vec2_length(henka_vec2 value)
{
    return sqrtf(value.x * value.x + value.y * value.y);
}

static float henka_gizmo_vec2_dot(henka_vec2 left, henka_vec2 right)
{
    return left.x * right.x + left.y * right.y;
}

static henka_vec2 henka_gizmo_vec2_normalize(henka_vec2 value)
{
    const float length = henka_gizmo_vec2_length(value);
    if (length <= 0.0001f)
    {
        return (henka_vec2){0.0f, 0.0f};
    }

    return (henka_vec2){value.x / length, value.y / length};
}

static henka_result henka_gizmo_project_handle_point(
    const henka_camera* camera,
    henka_viewport viewport,
    henka_vec3 world_point,
    henka_vec2* out_screen_local)
{
    henka_vec2 projected;

    if (camera == NULL || out_screen_local == NULL || !henka_viewport_is_valid(viewport))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_camera_world_to_screen(camera, viewport.width, viewport.height, world_point, &projected, NULL) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    *out_screen_local = projected;
    return HENKA_SUCCESS;
}

static bool henka_gizmo_project_handle_box(
    const henka_camera* camera,
    henka_viewport viewport,
    henka_vec3 world_center,
    float world_half_size,
    henka_vec2* out_screen_center,
    henka_vec2* out_screen_half_extents)
{
    henka_vec2 center;
    henka_vec2 projected_x;
    henka_vec2 projected_y;
    henka_vec2 projected_z;
    float half_width;
    float half_height;

    if (camera == NULL ||
        out_screen_center == NULL ||
        out_screen_half_extents == NULL ||
        world_half_size <= 0.0f ||
        !henka_viewport_is_valid(viewport))
    {
        return false;
    }

    if (henka_gizmo_project_handle_point(camera, viewport, world_center, &center) != HENKA_SUCCESS ||
        henka_gizmo_project_handle_point(camera, viewport, henka_vec3_add(world_center, (henka_vec3){world_half_size, 0.0f, 0.0f}), &projected_x) != HENKA_SUCCESS ||
        henka_gizmo_project_handle_point(camera, viewport, henka_vec3_add(world_center, (henka_vec3){0.0f, world_half_size, 0.0f}), &projected_y) != HENKA_SUCCESS ||
        henka_gizmo_project_handle_point(camera, viewport, henka_vec3_add(world_center, (henka_vec3){0.0f, 0.0f, world_half_size}), &projected_z) != HENKA_SUCCESS)
    {
        return false;
    }

    half_width = fmaxf(fabsf(projected_x.x - center.x), fmaxf(fabsf(projected_y.x - center.x), fabsf(projected_z.x - center.x)));
    half_height = fmaxf(fabsf(projected_x.y - center.y), fmaxf(fabsf(projected_y.y - center.y), fabsf(projected_z.y - center.y)));
    if (half_width < 8.0f)
    {
        half_width = 8.0f;
    }
    if (half_height < 8.0f)
    {
        half_height = 8.0f;
    }

    *out_screen_center = center;
    *out_screen_half_extents = (henka_vec2){half_width, half_height};
    return true;
}

static void henka_gizmo_append_handle(henka_gizmo_model* model, henka_gizmo_handle_model handle)
{
    if (model == NULL || model->handle_count >= HENKA_GIZMO_MAX_HANDLES)
    {
        return;
    }

    model->handles[model->handle_count++] = handle;
}

static henka_result henka_gizmo_build_axis_drag_plane(
    henka_vec3 origin,
    henka_vec3 axis_direction,
    henka_vec3 camera_forward,
    henka_vec3* out_plane_origin,
    henka_vec3* out_plane_normal)
{
    henka_vec3 side;
    henka_vec3 normal;

    if (out_plane_origin == NULL || out_plane_normal == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    axis_direction = henka_vec3_normalize(axis_direction);
    camera_forward = henka_vec3_normalize(camera_forward);
    side = henka_vec3_cross(camera_forward, axis_direction);
    if (henka_vec3_length(side) <= 0.0001f)
    {
        side = henka_vec3_cross((henka_vec3){0.0f, 1.0f, 0.0f}, axis_direction);
        if (henka_vec3_length(side) <= 0.0001f)
        {
            side = henka_vec3_cross((henka_vec3){1.0f, 0.0f, 0.0f}, axis_direction);
        }
    }

    normal = henka_vec3_cross(axis_direction, side);
    normal = henka_vec3_normalize(normal);
    if (henka_vec3_length(normal) <= 0.0001f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_plane_origin = origin;
    *out_plane_normal = normal;
    return HENKA_SUCCESS;
}

henka_result henka_gizmo_get_axis_direction(henka_gizmo_axis axis, henka_vec3* out_direction)
{
    if (out_direction == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    switch (axis)
    {
        case HENKA_GIZMO_AXIS_X:
            *out_direction = (henka_vec3){1.0f, 0.0f, 0.0f};
            return HENKA_SUCCESS;
        case HENKA_GIZMO_AXIS_Y:
            *out_direction = (henka_vec3){0.0f, 1.0f, 0.0f};
            return HENKA_SUCCESS;
        case HENKA_GIZMO_AXIS_Z:
            *out_direction = (henka_vec3){0.0f, 0.0f, 1.0f};
            return HENKA_SUCCESS;
        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }
}

const char* henka_gizmo_mode_to_string(henka_gizmo_mode mode)
{
    switch (mode)
    {
        case HENKA_GIZMO_MODE_MOVE:
            return "Move";
        case HENKA_GIZMO_MODE_ROTATE:
            return "Rotate";
        case HENKA_GIZMO_MODE_SCALE:
            return "Scale";
        case HENKA_GIZMO_MODE_SELECT:
        default:
            return "Select";
    }
}

const char* henka_gizmo_axis_to_string(henka_gizmo_axis axis)
{
    switch (axis)
    {
        case HENKA_GIZMO_AXIS_X:
            return "X";
        case HENKA_GIZMO_AXIS_Y:
            return "Y";
        case HENKA_GIZMO_AXIS_Z:
            return "Z";
        case HENKA_GIZMO_AXIS_UNIFORM:
            return "Uniform";
        case HENKA_GIZMO_AXIS_NONE:
        default:
            return "None";
    }
}

henka_result henka_gizmo_snap_move(float value, float increment, float* out_value)
{
    if (out_value == NULL || increment <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_value = floorf(value / increment + 0.5f) * increment;
    return HENKA_SUCCESS;
}

henka_result henka_gizmo_snap_rotate(float angle_radians, float increment, float* out_angle_radians)
{
    if (out_angle_radians == NULL || increment <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_angle_radians = floorf(angle_radians / increment + 0.5f) * increment;
    return HENKA_SUCCESS;
}

henka_result henka_gizmo_snap_scale(float value, float increment, float minimum_scale, float* out_value)
{
    if (out_value == NULL || increment <= 0.0f || minimum_scale <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (value < minimum_scale)
    {
        value = minimum_scale;
    }

    *out_value = floorf(value / increment + 0.5f) * increment;
    if (*out_value < minimum_scale)
    {
        *out_value = minimum_scale;
    }
    return HENKA_SUCCESS;
}

henka_result henka_gizmo_intersect_ray_plane(
    henka_ray ray,
    henka_vec3 plane_origin,
    henka_vec3 plane_normal,
    henka_vec3* out_point,
    float* out_distance)
{
    float denominator;
    float distance;

    if (out_point == NULL || out_distance == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    plane_normal = henka_vec3_normalize(plane_normal);
    denominator = henka_vec3_dot(plane_normal, ray.direction);
    if (henka_gizmo_absf(denominator) <= 0.0001f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    distance = henka_vec3_dot(henka_vec3_subtract(plane_origin, ray.origin), plane_normal) / denominator;
    if (distance < 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_distance = distance;
    *out_point = henka_vec3_add(ray.origin, henka_vec3_scale(ray.direction, distance));
    return HENKA_SUCCESS;
}

henka_result henka_gizmo_hit_test_axis(
    henka_vec3 origin,
    henka_ray ray,
    float axis_length,
    float axis_radius,
    henka_gizmo_axis* out_axis,
    float* out_distance)
{
    float best_distance;
    henka_gizmo_axis best_axis;
    henka_gizmo_axis axis;

    if (out_axis == NULL || out_distance == NULL || axis_length <= 0.0f || axis_radius <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    best_axis = HENKA_GIZMO_AXIS_NONE;
    best_distance = FLT_MAX;
    for (axis = HENKA_GIZMO_AXIS_X; axis <= HENKA_GIZMO_AXIS_Z; ++axis)
    {
        henka_vec3 axis_direction;
        henka_vec3 plane_origin;
        henka_vec3 plane_normal;
        henka_vec3 hit_point;
        henka_vec3 closest_point;
        float plane_distance;
        float axis_distance;
        float along_axis;

        if (henka_gizmo_get_axis_direction(axis, &axis_direction) != HENKA_SUCCESS)
        {
            continue;
        }

        if (henka_gizmo_build_axis_drag_plane(origin, axis_direction, ray.direction, &plane_origin, &plane_normal) != HENKA_SUCCESS)
        {
            continue;
        }

        if (henka_gizmo_intersect_ray_plane(ray, plane_origin, plane_normal, &hit_point, &plane_distance) != HENKA_SUCCESS)
        {
            continue;
        }

        along_axis = henka_vec3_dot(henka_vec3_subtract(hit_point, origin), axis_direction);
        if (along_axis < 0.0f || along_axis > axis_length)
        {
            continue;
        }

        closest_point = henka_vec3_add(origin, henka_vec3_scale(axis_direction, along_axis));
        axis_distance = henka_vec3_length(henka_vec3_subtract(hit_point, closest_point));
        if (axis_distance > axis_radius)
        {
            continue;
        }

        if (plane_distance < best_distance)
        {
            best_distance = plane_distance;
            best_axis = axis;
        }
    }

    if (best_axis == HENKA_GIZMO_AXIS_NONE)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    *out_axis = best_axis;
    *out_distance = best_distance;
    return HENKA_SUCCESS;
}

henka_result henka_gizmo_hit_test_rotation_rings(
    henka_vec3 origin,
    henka_ray ray,
    float ring_radius,
    float ring_thickness,
    henka_gizmo_axis* out_axis,
    float* out_distance)
{
    float best_distance;
    henka_gizmo_axis best_axis;
    henka_gizmo_axis axis;

    if (out_axis == NULL || out_distance == NULL || ring_radius <= 0.0f || ring_thickness <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    best_axis = HENKA_GIZMO_AXIS_NONE;
    best_distance = FLT_MAX;
    for (axis = HENKA_GIZMO_AXIS_X; axis <= HENKA_GIZMO_AXIS_Z; ++axis)
    {
        henka_vec3 axis_direction;
        henka_vec3 hit_point;
        float distance;
        float radial_distance;
        float ring_distance;

        if (henka_gizmo_get_axis_direction(axis, &axis_direction) != HENKA_SUCCESS)
        {
            continue;
        }

        if (henka_gizmo_intersect_ray_plane(ray, origin, axis_direction, &hit_point, &distance) != HENKA_SUCCESS)
        {
            continue;
        }

        radial_distance = henka_vec3_length(henka_vec3_subtract(hit_point, origin));
        ring_distance = henka_gizmo_absf(radial_distance - ring_radius);
        if (ring_distance > ring_thickness)
        {
            continue;
        }

        if (distance < best_distance)
        {
            best_distance = distance;
            best_axis = axis;
        }
    }

    if (best_axis == HENKA_GIZMO_AXIS_NONE)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    *out_axis = best_axis;
    *out_distance = best_distance;
    return HENKA_SUCCESS;
}

henka_result henka_gizmo_hit_test_uniform_handle(
    henka_vec3 origin,
    henka_ray ray,
    float radius,
    float* out_distance)
{
    henka_vec3 offset;
    float projection;
    henka_vec3 closest_point;
    float closest_distance;

    if (out_distance == NULL || radius <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    offset = henka_vec3_subtract(origin, ray.origin);
    projection = henka_vec3_dot(offset, ray.direction);
    if (projection < 0.0f)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    closest_point = henka_vec3_add(ray.origin, henka_vec3_scale(ray.direction, projection));
    closest_distance = henka_vec3_length(henka_vec3_subtract(origin, closest_point));
    if (closest_distance > radius)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    *out_distance = projection;
    return HENKA_SUCCESS;
}

henka_result henka_gizmo_project_axis_delta(
    henka_vec3 origin,
    henka_vec3 axis_direction,
    henka_vec3 camera_forward,
    henka_ray start_ray,
    henka_ray current_ray,
    float* out_delta)
{
    henka_vec3 plane_origin;
    henka_vec3 plane_normal;
    henka_vec3 start_point;
    henka_vec3 current_point;
    float start_distance;
    float current_distance;

    if (out_delta == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_gizmo_build_axis_drag_plane(origin, axis_direction, camera_forward, &plane_origin, &plane_normal) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (henka_gizmo_intersect_ray_plane(start_ray, plane_origin, plane_normal, &start_point, &start_distance) != HENKA_SUCCESS ||
        henka_gizmo_intersect_ray_plane(current_ray, plane_origin, plane_normal, &current_point, &current_distance) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_delta = henka_vec3_dot(henka_vec3_subtract(current_point, start_point), henka_vec3_normalize(axis_direction));
    return HENKA_SUCCESS;
}

henka_result henka_gizmo_project_rotation_delta(
    henka_vec3 origin,
    henka_vec3 axis_direction,
    henka_ray start_ray,
    henka_ray current_ray,
    float* out_angle_radians)
{
    henka_vec3 start_point;
    henka_vec3 current_point;
    henka_vec3 start_vector;
    henka_vec3 current_vector;
    float start_distance;
    float current_distance;
    float cosine;
    float sine;

    if (out_angle_radians == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    axis_direction = henka_vec3_normalize(axis_direction);
    if (henka_gizmo_intersect_ray_plane(start_ray, origin, axis_direction, &start_point, &start_distance) != HENKA_SUCCESS ||
        henka_gizmo_intersect_ray_plane(current_ray, origin, axis_direction, &current_point, &current_distance) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    start_vector = henka_vec3_normalize(henka_vec3_subtract(start_point, origin));
    current_vector = henka_vec3_normalize(henka_vec3_subtract(current_point, origin));
    if (henka_vec3_length(start_vector) <= 0.0001f || henka_vec3_length(current_vector) <= 0.0001f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    cosine = henka_vec3_dot(start_vector, current_vector);
    if (cosine > 1.0f)
    {
        cosine = 1.0f;
    }
    if (cosine < -1.0f)
    {
        cosine = -1.0f;
    }
    sine = henka_vec3_dot(axis_direction, henka_vec3_cross(start_vector, current_vector));
    *out_angle_radians = atan2f(sine, cosine);
    return HENKA_SUCCESS;
}

henka_result henka_gizmo_hit_test_segment_2d(
    henka_vec2 point,
    henka_vec2 start,
    henka_vec2 end,
    float tolerance,
    float* out_distance)
{
    const float segment_dx = end.x - start.x;
    const float segment_dy = end.y - start.y;
    const float segment_length_sq = segment_dx * segment_dx + segment_dy * segment_dy;
    float closest_x;
    float closest_y;
    float distance;
    float t;

    if (out_distance == NULL || tolerance <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (segment_length_sq <= 0.00001f)
    {
        distance = sqrtf(henka_gizmo_distance_sq_2d(point, start));
        if (distance > tolerance)
        {
            return HENKA_ERROR_UNKNOWN;
        }

        *out_distance = distance;
        return HENKA_SUCCESS;
    }

    t = ((point.x - start.x) * segment_dx + (point.y - start.y) * segment_dy) / segment_length_sq;
    if (t < 0.0f)
    {
        t = 0.0f;
    }
    if (t > 1.0f)
    {
        t = 1.0f;
    }

    closest_x = start.x + segment_dx * t;
    closest_y = start.y + segment_dy * t;
    distance = sqrtf((point.x - closest_x) * (point.x - closest_x) + (point.y - closest_y) * (point.y - closest_y));
    if (distance > tolerance)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    *out_distance = distance;
    return HENKA_SUCCESS;
}

bool henka_gizmo_hit_test_rect_2d(
    henka_vec2 point,
    henka_vec2 center,
    henka_vec2 half_extents,
    float padding)
{
    if (half_extents.x < 0.0f || half_extents.y < 0.0f || padding < 0.0f)
    {
        return false;
    }

    return point.x >= center.x - (half_extents.x + padding) &&
        point.x <= center.x + (half_extents.x + padding) &&
        point.y >= center.y - (half_extents.y + padding) &&
        point.y <= center.y + (half_extents.y + padding);
}

henka_result henka_gizmo_hit_test_polyline_2d(
    henka_vec2 point,
    const henka_vec2* points,
    size_t point_count,
    float tolerance,
    float* out_distance)
{
    float best_distance;
    size_t index;

    if (points == NULL || out_distance == NULL || point_count < 2U || tolerance <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    best_distance = FLT_MAX;
    for (index = 0U; index + 1U < point_count; ++index)
    {
        float segment_distance;

        if (henka_gizmo_hit_test_segment_2d(point, points[index], points[index + 1U], tolerance, &segment_distance) == HENKA_SUCCESS &&
            segment_distance < best_distance)
        {
            best_distance = segment_distance;
        }
    }

    if (best_distance == FLT_MAX)
    {
        return HENKA_ERROR_UNKNOWN;
    }

    *out_distance = best_distance;
    return HENKA_SUCCESS;
}

henka_result henka_gizmo_build_model(
    const henka_camera* camera,
    henka_viewport viewport,
    henka_entity target_entity,
    henka_transform target_transform,
    henka_gizmo_mode mode,
    henka_vec2 mouse_framebuffer,
    float gizmo_size,
    henka_gizmo_model* out_model)
{
    henka_gizmo_model model;
    size_t index;

    if (camera == NULL ||
        out_model == NULL ||
        !henka_viewport_is_valid(viewport) ||
        !henka_viewport_contains_point(viewport, mouse_framebuffer) ||
        gizmo_size <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(&model, 0, sizeof(model));
    if (henka_viewport_window_to_local(viewport, mouse_framebuffer, &model.mouse_local) != HENKA_SUCCESS ||
        henka_gizmo_project_handle_point(camera, viewport, target_transform.position, &model.screen_center) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    model.valid = true;
    model.target_entity = target_entity;
    model.mode = mode;
    model.target_transform = target_transform;
    model.viewport = viewport;
    model.mouse_framebuffer = mouse_framebuffer;
    model.gizmo_size = gizmo_size;

    for (index = 0U; index < 3U; ++index)
    {
        const henka_gizmo_axis axis = (henka_gizmo_axis)(index + 1U);
        henka_vec3 axis_direction;
        henka_vec3 world_end;
        henka_vec2 screen_end;

        if (henka_gizmo_get_axis_direction(axis, &axis_direction) != HENKA_SUCCESS)
        {
            continue;
        }

        world_end = henka_vec3_add(target_transform.position, henka_vec3_scale(axis_direction, model.gizmo_size));
        if (henka_gizmo_project_handle_point(camera, viewport, world_end, &screen_end) != HENKA_SUCCESS)
        {
            continue;
        }

        if (mode == HENKA_GIZMO_MODE_MOVE)
        {
            henka_gizmo_handle_model line_handle;
            henka_gizmo_handle_model box_handle;

            memset(&line_handle, 0, sizeof(line_handle));
            line_handle.visible = true;
            line_handle.mode = mode;
            line_handle.axis = axis;
            line_handle.type = HENKA_GIZMO_HANDLE_MOVE_AXIS;
            line_handle.world_start = target_transform.position;
            line_handle.world_end = world_end;
            line_handle.screen_start = model.screen_center;
            line_handle.screen_end = screen_end;
            line_handle.hit_tolerance = 10.0f;
            henka_gizmo_append_handle(&model, line_handle);

            memset(&box_handle, 0, sizeof(box_handle));
            box_handle.visible = henka_gizmo_project_handle_box(
                camera,
                viewport,
                world_end,
                model.gizmo_size * 0.08f,
                &box_handle.screen_center,
                &box_handle.screen_half_extents);
            box_handle.mode = mode;
            box_handle.axis = axis;
            box_handle.type = HENKA_GIZMO_HANDLE_MOVE_BOX;
            box_handle.world_center = world_end;
            box_handle.hit_tolerance = 4.0f;
            if (box_handle.visible)
            {
                henka_gizmo_append_handle(&model, box_handle);
            }
        }
        else if (mode == HENKA_GIZMO_MODE_ROTATE)
        {
            henka_gizmo_handle_model ring_handle;
            henka_vec3 basis_a;
            henka_vec3 basis_b;
            size_t point_index;

            memset(&ring_handle, 0, sizeof(ring_handle));
            ring_handle.visible = true;
            ring_handle.mode = mode;
            ring_handle.axis = axis;
            ring_handle.type = HENKA_GIZMO_HANDLE_ROTATE_RING;
            ring_handle.world_center = target_transform.position;
            ring_handle.hit_tolerance = 12.0f;

            switch (axis)
            {
                case HENKA_GIZMO_AXIS_X:
                    basis_a = (henka_vec3){0.0f, 1.0f, 0.0f};
                    basis_b = (henka_vec3){0.0f, 0.0f, 1.0f};
                    break;
                case HENKA_GIZMO_AXIS_Y:
                    basis_a = (henka_vec3){1.0f, 0.0f, 0.0f};
                    basis_b = (henka_vec3){0.0f, 0.0f, 1.0f};
                    break;
                case HENKA_GIZMO_AXIS_Z:
                default:
                    basis_a = (henka_vec3){1.0f, 0.0f, 0.0f};
                    basis_b = (henka_vec3){0.0f, 1.0f, 0.0f};
                    break;
            }

            for (point_index = 0U; point_index < HENKA_GIZMO_RING_SAMPLES; ++point_index)
            {
                const float angle = ((float)point_index / (float)(HENKA_GIZMO_RING_SAMPLES - 1U)) * HENKA_PI * 2.0f;
                const henka_vec3 world_point = henka_vec3_add(
                    target_transform.position,
                    henka_vec3_add(
                        henka_vec3_scale(basis_a, cosf(angle) * model.gizmo_size),
                        henka_vec3_scale(basis_b, sinf(angle) * model.gizmo_size)));

                if (henka_gizmo_project_handle_point(camera, viewport, world_point, &ring_handle.points[point_index]) != HENKA_SUCCESS)
                {
                    ring_handle.visible = false;
                    break;
                }
            }

            if (ring_handle.visible)
            {
                float max_radius = 0.0f;

                ring_handle.point_count = HENKA_GIZMO_RING_SAMPLES;
                for (point_index = 0U; point_index < ring_handle.point_count; ++point_index)
                {
                    const float radius = henka_gizmo_vec2_length(henka_gizmo_vec2_subtract(ring_handle.points[point_index], model.screen_center));
                    if (radius > max_radius)
                    {
                        max_radius = radius;
                    }
                }
                if (max_radius + 18.0f > model.dead_zone_radius)
                {
                    model.dead_zone_radius = max_radius + 18.0f;
                }
                henka_gizmo_append_handle(&model, ring_handle);
            }
        }
    }

    if (mode == HENKA_GIZMO_MODE_SCALE)
    {
        henka_gizmo_handle_model scale_handle;

        memset(&scale_handle, 0, sizeof(scale_handle));
        scale_handle.visible = henka_gizmo_project_handle_box(
            camera,
            viewport,
            target_transform.position,
            model.gizmo_size * 0.12f,
            &scale_handle.screen_center,
            &scale_handle.screen_half_extents);
        scale_handle.mode = mode;
        scale_handle.axis = HENKA_GIZMO_AXIS_UNIFORM;
        scale_handle.type = HENKA_GIZMO_HANDLE_SCALE_UNIFORM;
        scale_handle.world_center = target_transform.position;
        scale_handle.hit_tolerance = 6.0f;
        if (scale_handle.visible)
        {
            henka_gizmo_append_handle(&model, scale_handle);
            model.dead_zone_radius = fmaxf(scale_handle.screen_half_extents.x, scale_handle.screen_half_extents.y) + 18.0f;
        }
    }
    else
    {
        model.dead_zone_radius = 22.0f;
        for (index = 0U; index < model.handle_count; ++index)
        {
            const henka_gizmo_handle_model* handle = &model.handles[index];
            if (handle->type == HENKA_GIZMO_HANDLE_MOVE_BOX)
            {
                const float radius = fmaxf(handle->screen_half_extents.x, handle->screen_half_extents.y) + 18.0f;
                if (radius > model.dead_zone_radius)
                {
                    model.dead_zone_radius = radius;
                }
            }
        }
    }

    *out_model = model;
    return HENKA_SUCCESS;
}

henka_result henka_gizmo_hit_test_model(
    const henka_gizmo_model* model,
    henka_gizmo_handle_hit* out_hit)
{
    float best_distance;
    size_t index;

    if (model == NULL || out_hit == NULL || !model->valid)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(out_hit, 0, sizeof(*out_hit));
    out_hit->axis = HENKA_GIZMO_AXIS_NONE;
    out_hit->type = HENKA_GIZMO_HANDLE_NONE;
    out_hit->distance = FLT_MAX;
    out_hit->in_dead_zone = henka_gizmo_vec2_length(henka_gizmo_vec2_subtract(model->mouse_local, model->screen_center)) <= model->dead_zone_radius;

    best_distance = FLT_MAX;
    for (index = 0U; index < model->handle_count; ++index)
    {
        const henka_gizmo_handle_model* handle = &model->handles[index];
        float hit_distance;

        if (!handle->visible)
        {
            continue;
        }

        switch (handle->type)
        {
            case HENKA_GIZMO_HANDLE_MOVE_BOX:
            case HENKA_GIZMO_HANDLE_SCALE_UNIFORM:
                if (henka_gizmo_hit_test_rect_2d(model->mouse_local, handle->screen_center, handle->screen_half_extents, handle->hit_tolerance))
                {
                    hit_distance = henka_gizmo_vec2_length(henka_gizmo_vec2_subtract(model->mouse_local, handle->screen_center));
                }
                else
                {
                    continue;
                }
                break;
            case HENKA_GIZMO_HANDLE_MOVE_AXIS:
                if (henka_gizmo_hit_test_segment_2d(model->mouse_local, handle->screen_start, handle->screen_end, handle->hit_tolerance, &hit_distance) != HENKA_SUCCESS)
                {
                    continue;
                }
                break;
            case HENKA_GIZMO_HANDLE_ROTATE_RING:
                if (henka_gizmo_hit_test_polyline_2d(model->mouse_local, handle->points, handle->point_count, handle->hit_tolerance, &hit_distance) != HENKA_SUCCESS)
                {
                    continue;
                }
                break;
            case HENKA_GIZMO_HANDLE_NONE:
            default:
                continue;
        }

        if (hit_distance < best_distance)
        {
            best_distance = hit_distance;
            out_hit->hit = true;
            out_hit->axis = handle->axis;
            out_hit->type = handle->type;
            out_hit->distance = hit_distance;
        }
    }

    return HENKA_SUCCESS;
}

henka_result henka_gizmo_begin_drag(
    const henka_gizmo_model* model,
    const henka_gizmo_handle_hit* hit,
    henka_gizmo_drag_state* out_drag)
{
    size_t handle_index;

    if (model == NULL || hit == NULL || out_drag == NULL || !model->valid || !hit->hit)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    memset(out_drag, 0, sizeof(*out_drag));
    out_drag->dragging = true;
    out_drag->target_entity = model->target_entity;
    out_drag->drag_start_mouse_framebuffer = model->mouse_framebuffer;
    out_drag->drag_start_mouse_local = model->mouse_local;
    out_drag->drag_start_viewport = model->viewport;
    out_drag->drag_start_transform = model->target_transform;
    out_drag->active_axis = hit->axis;
    out_drag->active_mode = model->mode;
    out_drag->drag_center_screen = model->screen_center;
    out_drag->gizmo_size = model->gizmo_size;
    out_drag->drag_start_angle = atan2f(
        out_drag->drag_start_mouse_local.y - out_drag->drag_center_screen.y,
        out_drag->drag_start_mouse_local.x - out_drag->drag_center_screen.x);

    if (hit->type == HENKA_GIZMO_HANDLE_MOVE_AXIS || hit->type == HENKA_GIZMO_HANDLE_MOVE_BOX)
    {
        for (handle_index = 0U; handle_index < model->handle_count; ++handle_index)
        {
            const henka_gizmo_handle_model* handle = &model->handles[handle_index];
            if (handle->axis == hit->axis &&
                (handle->type == HENKA_GIZMO_HANDLE_MOVE_AXIS || handle->type == HENKA_GIZMO_HANDLE_MOVE_BOX))
            {
                henka_vec2 axis_delta = henka_gizmo_vec2_subtract(handle->screen_end, handle->screen_start);
                out_drag->drag_axis_screen_length = henka_gizmo_vec2_length(axis_delta);
                out_drag->drag_axis_screen_direction = henka_gizmo_vec2_normalize(axis_delta);
                break;
            }
        }
    }
    else if (hit->type == HENKA_GIZMO_HANDLE_SCALE_UNIFORM)
    {
        henka_vec2 start_from_center = henka_gizmo_vec2_subtract(out_drag->drag_start_mouse_local, out_drag->drag_center_screen);

        out_drag->drag_axis_screen_direction = henka_gizmo_vec2_normalize(start_from_center);
        if (henka_gizmo_vec2_length(out_drag->drag_axis_screen_direction) <= 0.0001f)
        {
            out_drag->drag_axis_screen_direction = henka_gizmo_vec2_normalize((henka_vec2){1.0f, -1.0f});
        }
        out_drag->drag_start_projection = henka_gizmo_vec2_dot(start_from_center, out_drag->drag_axis_screen_direction);
        if (out_drag->drag_start_projection < 14.0f)
        {
            out_drag->drag_start_projection = 14.0f;
        }
    }

    return HENKA_SUCCESS;
}

henka_result henka_gizmo_apply_drag_to_transform(
    const henka_gizmo_drag_state* drag,
    henka_vec2 current_mouse_local,
    const henka_gizmo_snap_settings* snap_settings,
    henka_transform* out_transform)
{
    henka_transform transform;
    float delta;
    float snapped_value;

    if (drag == NULL || out_transform == NULL || !drag->dragging)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    transform = drag->drag_start_transform;
    switch (drag->active_mode)
    {
        case HENKA_GIZMO_MODE_MOVE:
        {
            henka_vec3 axis_direction;
            henka_vec2 drag_delta;

            if (henka_gizmo_get_axis_direction(drag->active_axis, &axis_direction) != HENKA_SUCCESS ||
                drag->drag_axis_screen_length <= 1.0f)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }

            drag_delta = henka_gizmo_vec2_subtract(current_mouse_local, drag->drag_start_mouse_local);
            delta = henka_gizmo_vec2_dot(drag_delta, drag->drag_axis_screen_direction);
            delta *= drag->gizmo_size / drag->drag_axis_screen_length;
            transform.position = henka_vec3_add(drag->drag_start_transform.position, henka_vec3_scale(axis_direction, delta));
            if (snap_settings != NULL && snap_settings->enabled)
            {
                if (drag->active_axis == HENKA_GIZMO_AXIS_X)
                {
                    henka_gizmo_snap_move(transform.position.x, snap_settings->move_snap_increment, &transform.position.x);
                }
                else if (drag->active_axis == HENKA_GIZMO_AXIS_Y)
                {
                    henka_gizmo_snap_move(transform.position.y, snap_settings->move_snap_increment, &transform.position.y);
                }
                else if (drag->active_axis == HENKA_GIZMO_AXIS_Z)
                {
                    henka_gizmo_snap_move(transform.position.z, snap_settings->move_snap_increment, &transform.position.z);
                }
            }
            break;
        }

        case HENKA_GIZMO_MODE_ROTATE:
        {
            henka_vec3 axis_direction;
            float current_angle;

            if (henka_gizmo_get_axis_direction(drag->active_axis, &axis_direction) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }

            current_angle = atan2f(
                current_mouse_local.y - drag->drag_center_screen.y,
                current_mouse_local.x - drag->drag_center_screen.x);
            delta = current_angle - drag->drag_start_angle;
            while (delta > HENKA_PI)
            {
                delta -= HENKA_PI * 2.0f;
            }
            while (delta < -HENKA_PI)
            {
                delta += HENKA_PI * 2.0f;
            }
            if (snap_settings != NULL && snap_settings->enabled)
            {
                henka_gizmo_snap_rotate(delta, snap_settings->rotate_snap_increment, &delta);
            }
            transform.rotation = henka_quat_multiply(henka_quat_from_axis_angle(axis_direction, delta), transform.rotation);
            break;
        }

        case HENKA_GIZMO_MODE_SCALE:
        {
            henka_vec2 current_from_center;
            float current_projection;
            float scale_factor;
            float minimum_scale;

            if (drag->active_axis != HENKA_GIZMO_AXIS_UNIFORM)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }

            current_from_center = henka_gizmo_vec2_subtract(current_mouse_local, drag->drag_center_screen);
            current_projection = henka_gizmo_vec2_dot(current_from_center, drag->drag_axis_screen_direction);
            scale_factor = current_projection / drag->drag_start_projection;
            if (scale_factor < 0.05f)
            {
                scale_factor = 0.05f;
            }

            transform.scale.x = drag->drag_start_transform.scale.x * scale_factor;
            transform.scale.y = drag->drag_start_transform.scale.y * scale_factor;
            transform.scale.z = drag->drag_start_transform.scale.z * scale_factor;
            minimum_scale = (snap_settings != NULL && snap_settings->minimum_scale > 0.0f) ? snap_settings->minimum_scale : 0.01f;
            if (snap_settings != NULL && snap_settings->enabled)
            {
                henka_gizmo_snap_scale(transform.scale.x, snap_settings->scale_snap_increment, minimum_scale, &snapped_value);
                transform.scale.x = snapped_value;
                transform.scale.y = snapped_value;
                transform.scale.z = snapped_value;
            }
            break;
        }

        case HENKA_GIZMO_MODE_SELECT:
        default:
            return HENKA_ERROR_INVALID_ARGUMENT;
    }

    *out_transform = transform;
    return HENKA_SUCCESS;
}
