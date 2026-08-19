#ifndef HENKA_TEMPORAL_CAMERA_POLICY_H
#define HENKA_TEMPORAL_CAMERA_POLICY_H

#include <stdbool.h>

#include <henka/camera.h>

typedef struct henka_temporal_camera_motion
{
    float position_delta;
    float angle_delta;
    float projection_delta;
    float maximum_delta;
    bool projection_mode_changed;
} henka_temporal_camera_motion;

bool henka_temporal_camera_measure(
    const henka_camera* previous,
    const henka_camera* current,
    henka_temporal_camera_motion* out_motion);

bool henka_temporal_camera_is_static(
    const henka_temporal_camera_motion* motion);

bool henka_temporal_camera_transform_is_moving(
    const henka_temporal_camera_motion* motion);

bool henka_temporal_camera_is_cut(
    const henka_temporal_camera_motion* motion);

bool henka_temporal_camera_should_jitter(
    bool rendered,
    bool hdr_presentation,
    bool history_ready,
    henka_camera_projection_mode projection_mode,
    bool camera_transform_moving,
    bool camera_static,
    bool camera_cut);

#endif