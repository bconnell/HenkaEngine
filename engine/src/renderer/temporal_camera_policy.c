#include "temporal_camera_policy.h"

#include <math.h>

#include <henka/core.h>

static float henka_temporal_angle_delta(
    float previous,
    float current)
{
    float delta;

    delta =
        fmodf(
            current - previous,
            HENKA_PI * 2.0f);

    if (delta > HENKA_PI)
    {
        delta -= HENKA_PI * 2.0f;
    }
    else if (delta < -HENKA_PI)
    {
        delta += HENKA_PI * 2.0f;
    }

    return fabsf(delta);
}

bool henka_temporal_camera_measure(
    const henka_camera* previous,
    const henka_camera* current,
    henka_temporal_camera_motion* out_motion)
{
    float angle_delta;
    float pitch_delta;
    float position_delta;
    float projection_delta;
    float roll_delta;
    float yaw_delta;

    if (out_motion == NULL)
    {
        return false;
    }

    *out_motion =
        (henka_temporal_camera_motion){0};

    if (previous == NULL ||
        current == NULL ||
        !henka_camera_is_valid(previous) ||
        !henka_camera_is_valid(current))
    {
        return false;
    }

    position_delta =
        henka_vec3_length(
            henka_vec3_subtract(
                current->position,
                previous->position));

    yaw_delta =
        henka_temporal_angle_delta(
            previous->yaw_radians,
            current->yaw_radians);

    pitch_delta =
        fabsf(
            current->pitch_radians -
            previous->pitch_radians);

    roll_delta =
        henka_temporal_angle_delta(
            previous->roll_radians,
            current->roll_radians);

    angle_delta =
        fmaxf(
            yaw_delta,
            fmaxf(
                pitch_delta,
                roll_delta));

    /*
     * Near/far/aspect affect both projection modes.
     *
     * FOV is active only for perspective.
     * Orthographic height is active only for orthographic.
     *
     * Comparing inactive values would manufacture camera motion
     * and can cause temporal jitter/history churn even though the
     * visible projection did not change.
     */
    projection_delta =
        fmaxf(
            fabsf(
                current->near_plane -
                previous->near_plane),
            fmaxf(
                fabsf(
                    current->far_plane -
                    previous->far_plane),
                fabsf(
                    current->aspect_ratio -
                    previous->aspect_ratio)));

    if (current->projection_mode ==
            previous->projection_mode)
    {
        if (current->projection_mode ==
            HENKA_CAMERA_PROJECTION_PERSPECTIVE)
        {
            projection_delta =
                fmaxf(
                    projection_delta,
                    fabsf(
                        current->field_of_view_radians -
                        previous->field_of_view_radians));
        }
        else
        {
            projection_delta =
                fmaxf(
                    projection_delta,
                    fabsf(
                        current->orthographic_height -
                        previous->orthographic_height));
        }
    }

    if (!isfinite(position_delta) ||
        !isfinite(angle_delta) ||
        !isfinite(projection_delta))
    {
        return false;
    }

    out_motion->position_delta =
        position_delta;

    out_motion->angle_delta =
        angle_delta;

    out_motion->projection_delta =
        projection_delta;

    out_motion->projection_mode_changed =
        current->projection_mode !=
        previous->projection_mode;

    out_motion->maximum_delta =
        fmaxf(
            position_delta,
            fmaxf(
                angle_delta,
                projection_delta));

    return true;
}

bool henka_temporal_camera_is_static(
    const henka_temporal_camera_motion* motion)
{
    return motion != NULL &&
        !motion->projection_mode_changed &&
        motion->position_delta <= 0.000001f &&
        motion->angle_delta <= 0.000001f &&
        motion->projection_delta <= 0.000001f;
}

bool henka_temporal_camera_transform_is_moving(
    const henka_temporal_camera_motion* motion)
{
    return motion != NULL &&
        (motion->position_delta > 0.000001f ||
         motion->angle_delta > 0.000001f);
}

bool henka_temporal_camera_is_cut(
    const henka_temporal_camera_motion* motion)
{
    if (motion == NULL)
    {
        return false;
    }

    /*
     * These quantities have different units.  Do not compare
     * metres/world units, radians, and projection scale against
     * one shared threshold.
     */
    return
        motion->projection_mode_changed ||
        motion->position_delta > 0.75f ||
        motion->angle_delta > 0.35f ||
        motion->projection_delta > 0.20f;
}

bool henka_temporal_camera_should_jitter(
    bool rendered,
    bool hdr_presentation,
    bool history_ready,
    henka_camera_projection_mode projection_mode,
    bool camera_transform_moving,
    bool camera_static,
    bool camera_cut)
{
    return
        rendered &&
        hdr_presentation &&
        history_ready &&
        projection_mode ==
            HENKA_CAMERA_PROJECTION_PERSPECTIVE &&
        camera_transform_moving &&
        !camera_static &&
        !camera_cut;
}