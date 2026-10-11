#ifndef SANDBOX3D_CAMERA_TOOLS_H
#define SANDBOX3D_CAMERA_TOOLS_H

#include <stdbool.h>

#include <henka/camera.h>
#include <henka/scene.h>

/*
 * Resolve a stable automatic framing subject.
 *
 * A valid visible preferred entity wins only when it is ordinary scene
 * content. Helper and transform-locked foundation entities are deliberately
 * excluded from automatic framing so a large ground/foundation does not
 * destroy useful editor composition.
 *
 * Explicit Frame Selected remains a separate caller workflow and is not
 * restricted by this policy.
 */
bool sandbox3d_camera_resolve_focus_bounds(
    const henka_scene* scene,
    henka_entity preferred_entity,
    const henka_bounds* preferred_bounds,
    henka_bounds* out_bounds);

/*
 * Transactionally apply a camera preset and frame the supplied bounds using
 * the preset orientation. The camera is unchanged on failure.
 */
bool sandbox3d_camera_apply_framed_preset(
    henka_camera* camera,
    henka_camera_preset preset,
    henka_bounds bounds);

/*
 * Apply a signed, magnitude-sensitive zoom toward a stable target.
 * Negative values zoom in, positive values zoom out, and one unit preserves
 * the existing Sandbox wheel-notch behavior. The camera is unchanged when
 * the input or resulting candidate is invalid.
 */
bool sandbox3d_camera_apply_zoom_delta(
    henka_camera* camera,
    henka_vec3 target,
    float direction_scale);

#endif
