#ifndef SANDBOX3D_PHYSICS_TOOLS_H
#define SANDBOX3D_PHYSICS_TOOLS_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/physics.h>

typedef enum sandbox3d_physics_sample_slot
{
    SANDBOX3D_PHYSICS_SAMPLE_GROUND = 0,
    SANDBOX3D_PHYSICS_SAMPLE_TEXTURED_CUBE,
    SANDBOX3D_PHYSICS_SAMPLE_COLORED_CUBE,
    SANDBOX3D_PHYSICS_SAMPLE_OBJ_MARKER,
    SANDBOX3D_PHYSICS_SAMPLE_MISSING_TEXTURE,
    SANDBOX3D_PHYSICS_SAMPLE_MISSING_MODEL,
    SANDBOX3D_PHYSICS_SAMPLE_DEBUG_GRID,
    SANDBOX3D_PHYSICS_SAMPLE_COUNT
} sandbox3d_physics_sample_slot;

henka_physics_body_type sandbox3d_physics_initial_body_type(sandbox3d_physics_sample_slot slot);
henka_physics_body_type sandbox3d_physics_demo_body_type(sandbox3d_physics_sample_slot slot);

henka_result sandbox3d_physics_activate_only_body(
    henka_physics_world* world,
    const henka_physics_body_id* bodies,
    size_t body_count,
    henka_physics_body_id selected_body,
    henka_scene* scene,
    henka_entity selected_entity);

#endif
