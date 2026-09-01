#ifndef HENKA_PREFAB_H
#define HENKA_PREFAB_H

#include <stddef.h>

#include <henka/math.h>
#include <henka/result.h>
#include <henka/scene.h>

typedef struct henka_prefab henka_prefab;

/* A prefab snapshot is intentionally bounded until project persistence and
 * stable serialized prefab identities are available. */
#define HENKA_MAX_PREFAB_ENTITIES ((size_t)4096U)

/* Captures the selected entity and its active descendants in deterministic
 * scene order. Names, tags, transforms, materials, visibility, bounds,
 * interaction data, and hierarchy are copied. Meshes, textures, shaders, and
 * material definitions remain borrowed from their existing owners and must
 * outlive the prefab and any instances created from it. */
henka_result henka_prefab_create_from_scene(
    const henka_scene* source_scene,
    henka_entity root_entity,
    henka_prefab** out_prefab);
void henka_prefab_destroy(henka_prefab* prefab);
size_t henka_prefab_get_entity_count(const henka_prefab* prefab);

/* Instantiates one independent set of real scene entities. root_transform is
 * the new root world transform; descendant local transforms are preserved.
 * The operation rolls back every entity it created if any validation or
 * allocation step fails. The target scene and borrowed asset owners must be
 * used according to their normal thread/lifetime contracts. */
henka_result henka_prefab_instantiate(
    const henka_prefab* prefab,
    henka_scene* target_scene,
    henka_transform root_transform,
    henka_entity* out_root_entity);

/* Instantiates one prefab beneath an existing scene entity. root_transform is
 * local to parent_entity; the operation remains transactional and rejects a
 * stale or invalid parent before creating target entities. */
henka_result henka_prefab_instantiate_under_parent(
    const henka_prefab* prefab,
    henka_scene* target_scene,
    henka_entity parent_entity,
    henka_transform root_transform,
    henka_entity* out_root_entity);

#endif
