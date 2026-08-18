#include "camera_tools.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

static bool sandbox3d_camera_bounds_are_valid(
    henka_bounds bounds)
{
    return
        isfinite(bounds.center.x) &&
        isfinite(bounds.center.y) &&
        isfinite(bounds.center.z) &&
        isfinite(bounds.extents.x) &&
        isfinite(bounds.extents.y) &&
        isfinite(bounds.extents.z) &&
        bounds.extents.x >= 0.0f &&
        bounds.extents.y >= 0.0f &&
        bounds.extents.z >= 0.0f;
}

static bool sandbox3d_camera_entity_is_automatic_content(
    const henka_scene* scene,
    henka_entity entity)
{
    if (scene == NULL ||
        entity == HENKA_INVALID_ENTITY ||
        !henka_scene_is_entity_valid(scene, entity) ||
        !henka_scene_is_entity_visible(scene, entity) ||
        henka_scene_is_entity_helper(scene, entity) ||
        henka_scene_is_entity_transform_locked(scene, entity))
    {
        return false;
    }

    return true;
}

static bool sandbox3d_camera_accumulate_bounds(
    henka_bounds bounds,
    bool* in_out_has_bounds,
    double minimum[3],
    double maximum[3])
{
    double candidate_minimum[3];
    double candidate_maximum[3];
    size_t axis;

    if (in_out_has_bounds == NULL ||
        minimum == NULL ||
        maximum == NULL ||
        !sandbox3d_camera_bounds_are_valid(bounds))
    {
        return false;
    }

    candidate_minimum[0] =
        (double)bounds.center.x - (double)bounds.extents.x;
    candidate_minimum[1] =
        (double)bounds.center.y - (double)bounds.extents.y;
    candidate_minimum[2] =
        (double)bounds.center.z - (double)bounds.extents.z;

    candidate_maximum[0] =
        (double)bounds.center.x + (double)bounds.extents.x;
    candidate_maximum[1] =
        (double)bounds.center.y + (double)bounds.extents.y;
    candidate_maximum[2] =
        (double)bounds.center.z + (double)bounds.extents.z;

    for (axis = 0U; axis < 3U; ++axis)
    {
        if (!isfinite(candidate_minimum[axis]) ||
            !isfinite(candidate_maximum[axis]) ||
            candidate_minimum[axis] > candidate_maximum[axis])
        {
            return false;
        }
    }

    if (!*in_out_has_bounds)
    {
        for (axis = 0U; axis < 3U; ++axis)
        {
            minimum[axis] = candidate_minimum[axis];
            maximum[axis] = candidate_maximum[axis];
        }

        *in_out_has_bounds = true;
        return true;
    }

    for (axis = 0U; axis < 3U; ++axis)
    {
        if (candidate_minimum[axis] < minimum[axis])
        {
            minimum[axis] = candidate_minimum[axis];
        }

        if (candidate_maximum[axis] > maximum[axis])
        {
            maximum[axis] = candidate_maximum[axis];
        }
    }

    return true;
}

static bool sandbox3d_camera_finish_bounds(
    const double minimum[3],
    const double maximum[3],
    henka_bounds* out_bounds)
{
    double center[3];
    double extents[3];
    size_t axis;

    if (minimum == NULL ||
        maximum == NULL ||
        out_bounds == NULL)
    {
        return false;
    }

    for (axis = 0U; axis < 3U; ++axis)
    {
        center[axis] =
            minimum[axis] +
            (maximum[axis] - minimum[axis]) * 0.5;

        extents[axis] =
            (maximum[axis] - minimum[axis]) * 0.5;

        if (!isfinite(center[axis]) ||
            !isfinite(extents[axis]) ||
            fabs(center[axis]) > (double)FLT_MAX ||
            extents[axis] > (double)FLT_MAX ||
            extents[axis] < 0.0)
        {
            return false;
        }
    }

    out_bounds->center =
        (henka_vec3){
            (float)center[0],
            (float)center[1],
            (float)center[2]};

    out_bounds->extents =
        (henka_vec3){
            (float)extents[0],
            (float)extents[1],
            (float)extents[2]};

    return sandbox3d_camera_bounds_are_valid(*out_bounds);
}

bool sandbox3d_camera_resolve_focus_bounds(
    const henka_scene* scene,
    henka_entity preferred_entity,
    const henka_bounds* preferred_bounds,
    henka_bounds* out_bounds)
{
    bool has_bounds;
    double minimum[3];
    double maximum[3];
    size_t entity_count;
    size_t index;

    if (scene == NULL || out_bounds == NULL)
    {
        return false;
    }

    *out_bounds =
        (henka_bounds){
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f}};

    if (preferred_bounds != NULL &&
        sandbox3d_camera_entity_is_automatic_content(
            scene,
            preferred_entity) &&
        sandbox3d_camera_bounds_are_valid(*preferred_bounds))
    {
        *out_bounds = *preferred_bounds;
        return true;
    }

    has_bounds = false;
    minimum[0] = 0.0;
    minimum[1] = 0.0;
    minimum[2] = 0.0;
    maximum[0] = 0.0;
    maximum[1] = 0.0;
    maximum[2] = 0.0;

    entity_count = henka_scene_get_entity_count(scene);

    for (index = 0U; index < entity_count; ++index)
    {
        henka_bounds bounds;
        const henka_entity entity =
            henka_scene_get_entity_at_index(scene, index);

        if (!sandbox3d_camera_entity_is_automatic_content(
                scene,
                entity))
        {
            continue;
        }

        if (henka_scene_get_entity_world_bounds(
                scene,
                entity,
                &bounds) != HENKA_SUCCESS)
        {
            continue;
        }

        (void)sandbox3d_camera_accumulate_bounds(
            bounds,
            &has_bounds,
            minimum,
            maximum);
    }

    if (!has_bounds)
    {
        return false;
    }

    return sandbox3d_camera_finish_bounds(
        minimum,
        maximum,
        out_bounds);
}

bool sandbox3d_camera_apply_framed_preset(
    henka_camera* camera,
    henka_camera_preset preset,
    henka_bounds bounds)
{
    henka_camera candidate;

    if (camera == NULL ||
        !henka_camera_is_valid(camera) ||
        !sandbox3d_camera_bounds_are_valid(bounds))
    {
        return false;
    }

    candidate = *camera;

    if (henka_camera_apply_preset(
            &candidate,
            preset,
            bounds.center) != HENKA_SUCCESS)
    {
        return false;
    }

    if (!henka_camera_frame_bounds(
            &candidate,
            bounds,
            candidate.yaw_radians,
            candidate.pitch_radians))
    {
        return false;
    }

    if (!henka_camera_is_valid(&candidate))
    {
        return false;
    }

    *camera = candidate;
    return true;
}