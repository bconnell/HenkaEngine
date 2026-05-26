#include <henka/gizmo.h>

#include <float.h>
#include <math.h>

#include <henka/math.h>

static float henka_gizmo_absf(float value)
{
    return value < 0.0f ? -value : value;
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
