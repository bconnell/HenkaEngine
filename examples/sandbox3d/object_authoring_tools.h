#ifndef SANDBOX3D_OBJECT_AUTHORING_TOOLS_H
#define SANDBOX3D_OBJECT_AUTHORING_TOOLS_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/scene.h>

/* Returns all active non-helper entities in scene enumeration order. */
size_t sandbox3d_object_authoring_collect_user_entities(
    const henka_scene* scene,
    henka_entity* out_entities,
    size_t capacity);

/* Returns whether an entity is a valid non-helper target for editor mutation. */
bool sandbox3d_object_authoring_can_edit_entity(
    const henka_scene* scene,
    henka_entity entity);

/* Duplicates the complete scene-owned object state and leaves the source unchanged. */
henka_result sandbox3d_object_authoring_duplicate_entity(
    henka_scene* scene,
    henka_entity source,
    const char* duplicate_name,
    henka_entity* out_duplicate);

#endif
