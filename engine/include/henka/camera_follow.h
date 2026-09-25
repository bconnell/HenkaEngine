#ifndef HENKA_CAMERA_FOLLOW_H
#define HENKA_CAMERA_FOLLOW_H

#include <henka/scene.h>

/* Camera offsets are expressed in the followed entity's local space. A zero
 * position lag applies the target pose immediately; a positive lag uses
 * bounded exponential positional smoothing. */
typedef struct henka_camera_follow_desc
{
    henka_vec3 position_offset;
    henka_vec3 look_at_offset;
    double position_lag_seconds;
} henka_camera_follow_desc;

typedef enum henka_camera_rig_mode
{
    HENKA_CAMERA_RIG_FIRST_PERSON = 0,
    HENKA_CAMERA_RIG_THIRD_PERSON
} henka_camera_rig_mode;

typedef struct henka_camera_rig_desc
{
    henka_camera_rig_mode mode;
    float eye_height;
    float lateral_offset;
    float follow_distance;
    float look_ahead_distance;
    double position_lag_seconds;
} henka_camera_rig_desc;

henka_camera_follow_desc henka_camera_follow_desc_default(void);
henka_camera_rig_desc henka_camera_rig_desc_default(
    henka_camera_rig_mode mode);
henka_result henka_camera_rig_follow_scene_entity(
    henka_scene* scene,
    henka_entity target,
    const henka_camera_rig_desc* rig,
    double delta_seconds);

/* Updates the scene-owned camera from one live generation-checked entity.
 * The camera is changed only after the target, offsets, timing, and complete
 * candidate pose validate. The target and scene remain caller-owned. */
henka_result henka_camera_follow_scene_entity(
    henka_scene* scene,
    henka_entity target,
    const henka_camera_follow_desc* desc,
    double delta_seconds);

#endif
