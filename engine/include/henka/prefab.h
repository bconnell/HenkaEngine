#ifndef HENKA_PREFAB_H
#define HENKA_PREFAB_H

#include <stddef.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/result.h>
#include <henka/scene.h>

typedef struct henka_prefab henka_prefab;
typedef struct henka_prefab_instance henka_prefab_instance;
typedef struct henka_asset_manager henka_asset_manager;

typedef uint64_t henka_prefab_source_id;

#define HENKA_INVALID_PREFAB_SOURCE_ID ((henka_prefab_source_id)0)

/* Prefab capture and persisted prefab assets are bounded by the same source
 * membership limit. */
#define HENKA_MAX_PREFAB_ENTITIES ((size_t)4096U)
#define HENKA_PREFAB_ASSET_FORMAT_VERSION 1U
#define HENKA_PREFAB_MAX_ASSET_PATH_BYTES ((size_t)4096U)

/* Captures the selected entity and its active descendants in deterministic
 * scene order. Names, tags, transforms, materials, visibility, renderer
 * enablement, bounds, interaction data, and hierarchy are copied. Meshes,
 * textures, shaders, and material definitions remain borrowed from their
 * existing owners and must outlive the prefab and any instances created from
 * it. Logical selection
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
uint64_t henka_prefab_get_revision(const henka_prefab* prefab);

/* The project-relative asset path is the durable prefab identity. A newly
 * captured snapshot has no identity until it is assigned or saved. The path
 * is confined and normalized without touching the filesystem. */
henka_result henka_prefab_set_asset_path(
    henka_prefab* prefab,
    const char* project_relative_path);
const char* henka_prefab_get_asset_path(const henka_prefab* prefab);

/* Saves a prefab asset through the existing confined, atomic settings
 * persistence path. Manager-owned meshes and material assets are recorded by
 * their canonical source paths; inline materials are value-persisted only
 * when they have no borrowed texture dependencies. Unresolved borrowed
 * dependencies fail closed rather than becoming copied or stale pointers. */
henka_result henka_prefab_save_file(
    const henka_prefab* prefab,
    const henka_asset_manager* asset_manager,
    const char* project_root,
    const char* relative_path);
/* Loads a persisted prefab into an independent candidate. Meshes and
 * material assets are resolved through the supplied manager; inline material
 * values require the caller's runtime shader authority. The destination
 * output remains NULL on every failure, including malformed, future-version,
 * missing-dependency, and path-confinement failures. */
henka_result henka_prefab_load_file(
    henka_asset_manager* asset_manager,
    henka_shader* inline_material_shader,
    const char* project_root,
    const char* relative_path,
    henka_prefab** out_prefab);

/* Resolves the stable source-local identity stored at a captured index. The
 * identity survives an in-memory refresh for surviving source entities and is
 * persisted as the prefab entry identity when the asset is saved. */
henka_result henka_prefab_get_source_id_at(
    const henka_prefab* prefab,
    size_t index,
    henka_prefab_source_id* out_source_id);
henka_result henka_prefab_find_source_id(
    const henka_prefab* prefab,
    henka_prefab_source_id source_id,
    size_t* out_index);

/* Rebuilds the bounded snapshot from a live source root. The existing
 * snapshot remains unchanged if capture or validation fails. A successful
 * refresh increments the in-memory revision; persisted prefab assets are
 * updated only by an explicit save call. */
henka_result henka_prefab_refresh_from_scene(
    henka_prefab* prefab,
    const henka_scene* source_scene,
    henka_entity root_entity);

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
/* Destroys every currently live entity in the mapping through the normal
 * scene lifetime path. The operation preflights the complete scene revision
 * budget and leaves both the scene and mapping unchanged when capacity is
 * exhausted. A successful call retains the mapping handle with all entries
 * stale so repeated cleanup is an idempotent no-op. */
henka_result henka_prefab_instance_destroy_entities(
    henka_prefab_instance* instance);
size_t henka_prefab_instance_get_entity_count(
    const henka_prefab_instance* instance);
uint64_t henka_prefab_instance_get_prefab_revision(
    const henka_prefab_instance* instance);
/* Resolves an instance entity by the source-local identity captured in the
 * prefab. The lookup remains valid when source ordering changes; a destroyed
 * mapped entity is reported as invalid rather than returned as stale state. */
henka_result henka_prefab_instance_get_entity_for_source_id(
    const henka_prefab_instance* instance,
    henka_prefab_source_id source_id,
    henka_entity* out_entity);
/* Applies a bounded per-instance local-transform override without changing
 * the reusable prefab snapshot. The override is keyed by source-local ID,
 * uses the scene's canonical transform representation, and remains in memory
 * until it is cleared or the instance handle is destroyed. */
henka_result henka_prefab_instance_set_local_transform_override(
    henka_prefab_instance* instance,
    henka_prefab_source_id source_id,
    henka_transform transform);
henka_result henka_prefab_instance_clear_local_transform_override(
    henka_prefab_instance* instance,
    henka_prefab_source_id source_id);
henka_result henka_prefab_instance_get_local_transform_override(
    const henka_prefab_instance* instance,
    henka_prefab_source_id source_id,
    bool* out_has_override,
    henka_transform* out_transform);
/* Reapplies the current source snapshot to the live mapped entities. The
 * source prefab is borrowed and must outlive the instance. Stable source IDs
 * and live Scene identities must still match the captured membership.
 * Supported inline-presentation values refresh atomically; local transform
 * overrides remain in place and their source baselines are updated. Local
 * transforms edited through the general scene API are adopted before a
 * refresh: the root remains instance placement and a changed non-root becomes
 * an instance override. Snapshot entries with borrowed material assets are
 * rejected until their asset state can be refreshed atomically through the
 * owning asset authority. A call for the already-applied prefab revision is
 * an idempotent no-op. */
henka_result henka_prefab_instance_refresh(henka_prefab_instance* instance);
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
 * The operation preflights the complete revision budget, including worst-case
 * rollback, before its first target-scene mutation and rolls back every entity
 * it created if any later validation or allocation step fails. The target
 * scene and borrowed asset owners must be used according to their normal
 * thread/lifetime contracts. */
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
