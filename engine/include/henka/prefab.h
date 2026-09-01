#ifndef HENKA_PREFAB_H
#define HENKA_PREFAB_H

#include <stddef.h>

#include <henka/math.h>
#include <henka/result.h>
#include <henka/scene.h>

typedef struct henka_prefab henka_prefab;
typedef struct henka_prefab_instance henka_prefab_instance;

/* A prefab snapshot is intentionally bounded until project persistence and
 * stable serialized prefab identities are available. */
#define HENKA_MAX_PREFAB_ENTITIES ((size_t)4096U)

/* Captures the selected entity and its active descendants in deterministic
 * scene order. Names, tags, transforms, materials, visibility, bounds,
 * interaction data, and hierarchy are copied. Meshes, textures, shaders, and
 * material definitions remain borrowed from their existing owners and must
 * outlive the prefab and any instances created from it. Logical selection
 * owners inside the captured subtree are remapped during instantiation;
 * external owners are not retained as cross-instance references. */
henka_result henka_prefab_create_from_scene(
    const henka_scene* source_scene,
    henka_entity root_entity,
    henka_prefab** out_prefab);
void henka_prefab_destroy(henka_prefab* prefab);
size_t henka_prefab_get_entity_count(const henka_prefab* prefab);

/* Resolves a captured source entity to its deterministic prefab-local index.
 * The source handle is retained only as a lookup key; the prefab does not
 * retain the source scene. */
henka_result henka_prefab_find_source_index(
    const henka_prefab* prefab,
    henka_entity source_entity,
    size_t* out_index);

/* Instantiates one independent set of real scene entities and retains a
 * bounded source-to-instance mapping. The mapping borrows target_scene and
 * remains queryable until this handle is destroyed or a mapped scene entity
 * is destroyed. Destroy the mapping handle before target_scene; destroying
 * the handle does not destroy scene entities. */
henka_result henka_prefab_instantiate_with_instance(
    const henka_prefab* prefab,
    henka_scene* target_scene,
    henka_transform root_transform,
    henka_prefab_instance** out_instance);
void henka_prefab_instance_destroy(henka_prefab_instance* instance);
size_t henka_prefab_instance_get_entity_count(
    const henka_prefab_instance* instance);
henka_result henka_prefab_instance_get_entity_at(
    const henka_prefab_instance* instance,
    size_t index,
    henka_entity* out_entity);
henka_result henka_prefab_instance_get_root_entity(
    const henka_prefab_instance* instance,
    henka_entity* out_entity);

/* Instantiates one mapped prefab beneath a live scene entity. root_transform
 * is local to parent_entity; stale or invalid parents fail before allocation. */
henka_result henka_prefab_instantiate_under_parent_with_instance(
    const henka_prefab* prefab,
    henka_scene* target_scene,
    henka_entity parent_entity,
    henka_transform root_transform,
    henka_prefab_instance** out_instance);

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
