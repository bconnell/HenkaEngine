#ifndef SANDBOX3D_MODELING_SELECTION_SCENE_H
#define SANDBOX3D_MODELING_SELECTION_SCENE_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/camera.h>
#include <henka/core.h>
#include <henka/scene.h>
#include <henka/workspace.h>

#include "modeling_selection.h"
#include "object_authoring_tools.h"

/* Projects source-authoritative components, applies the documented
 * front-facing and occlusion policy, and commits one atomic selection. */
henka_result sandbox3d_modeling_selection_apply_scene(
    const sandbox3d_modeling_selection_session* session,
    bool xray_enabled,
    const henka_camera* camera,
    henka_scene* scene,
    henka_viewport viewport,
    sandbox3d_authoring_object* object,
    size_t* out_selected_count);

#endif
