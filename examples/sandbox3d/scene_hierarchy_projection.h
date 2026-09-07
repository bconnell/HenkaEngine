#ifndef SANDBOX3D_SCENE_HIERARCHY_PROJECTION_H
#define SANDBOX3D_SCENE_HIERARCHY_PROJECTION_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/scene.h>

typedef struct sandbox3d_scene_hierarchy_row
{
    henka_entity entity;
    size_t depth;
} sandbox3d_scene_hierarchy_row;

typedef bool (*sandbox3d_scene_hierarchy_include_fn)(
    const henka_scene* scene,
    henka_entity entity,
    void* context);

/* Builds a transient parent-first view of the canonical scene hierarchy.
 * Parent/child relationships and sibling order remain owned by henka_scene;
 * this result contains only the rows needed by a presentation consumer. */
henka_result sandbox3d_scene_hierarchy_projection_build(
    const henka_scene* scene,
    sandbox3d_scene_hierarchy_include_fn include,
    void* context,
    sandbox3d_scene_hierarchy_row* rows,
    size_t row_capacity,
    size_t* out_row_count);

#endif
