#ifndef HENKA_ASSETS_H
#define HENKA_ASSETS_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/result.h>
#include <henka/scene.h>
#include <henka/shader.h>
#include <henka/texture.h>

typedef struct henka_asset_manager henka_asset_manager;
typedef struct henka_engine henka_engine;
typedef struct henka_mesh henka_mesh;
typedef struct henka_shader henka_shader;
typedef struct henka_texture henka_texture;
typedef struct henka_material_asset henka_material_asset;
typedef struct henka_gltf_scene_asset henka_gltf_scene_asset;

typedef enum henka_asset_type
{
    HENKA_ASSET_TYPE_UNKNOWN = 0,
    HENKA_ASSET_TYPE_SHADER,
    HENKA_ASSET_TYPE_TEXTURE,
    HENKA_ASSET_TYPE_MESH,
    HENKA_ASSET_TYPE_MATERIAL,
    HENKA_ASSET_TYPE_GLTF_SCENE
} henka_asset_type;

typedef struct henka_asset_metadata
{
    henka_asset_type type;
    const char* source_path;
    const char* display_name;
    bool loaded;
    bool fallback;
    bool reload_supported;
    const char* summary;
    const char* error_summary;
    bool has_texture_descriptor;
    henka_texture_descriptor texture_descriptor;
} henka_asset_metadata;

henka_asset_manager* henka_engine_get_asset_manager(henka_engine* engine);
const henka_asset_manager* henka_engine_get_asset_manager_const(const henka_engine* engine);

/*
 * Resolves a safe relative asset path beneath base_path. Rooted, UNC,
 * device, drive-qualified, traversal, and URI-like inputs are rejected.
 */
henka_result henka_assets_resolve_path(const char* base_path, const char* asset_path, char** out_path);
henka_result henka_assets_load_shader(
    henka_asset_manager* manager,
    const char* vertex_path,
    const char* fragment_path,
    henka_shader** out_shader);
henka_result henka_assets_load_shader_with_contract(
    henka_asset_manager* manager,
    const char* vertex_path,
    const char* fragment_path,
    const henka_shader_contract_desc* contract,
    henka_shader** out_shader);
/*
 * Assets returned by the manager are borrowed and remain owned by the manager.
 * Do not pass them to the public mesh, shader, or texture destroy functions.
 * Equivalent confined path spellings share one canonical cache identity.
 * Windows identities are ASCII case-insensitive while metadata preserves the
 * normalized source spelling first used to create the cache entry.
 */
henka_result henka_assets_load_texture(henka_asset_manager* manager, const char* path, henka_texture** out_texture);
henka_result henka_assets_load_texture_with_descriptor(
    henka_asset_manager* manager,
    const char* path,
    const henka_texture_descriptor* descriptor,
    henka_texture** out_texture);
henka_result henka_assets_load_obj_mesh(henka_asset_manager* manager, const char* path, henka_mesh** out_mesh);
henka_result henka_assets_load_gltf_mesh(henka_asset_manager* manager, const char* path, henka_mesh** out_mesh);
henka_result henka_assets_load_gltf_mesh_with_material(
    henka_asset_manager* manager,
    const char* path,
    henka_shader* shader,
    henka_mesh** out_mesh,
    henka_material* out_material);
henka_result henka_assets_load_gltf_material_asset(
    henka_asset_manager* manager,
    const char* path,
    henka_shader* shader,
    henka_material_asset** out_asset);
henka_result henka_assets_get_material_asset_material(
    const henka_material_asset* asset,
    henka_material* out_material);
henka_result henka_assets_reload_gltf_material_asset(
    henka_asset_manager* manager,
    const char* path,
    henka_material_asset** out_asset);
henka_result henka_assets_load_gltf_scene_asset(
    henka_asset_manager* manager,
    const char* path,
    henka_shader* shader,
    henka_gltf_scene_asset** out_asset);
henka_result henka_assets_reload_gltf_scene_asset(
    henka_asset_manager* manager,
    const char* path,
    henka_gltf_scene_asset** out_asset);
henka_result henka_assets_instantiate_gltf_scene(
    henka_asset_manager* manager,
    const henka_gltf_scene_asset* asset,
    henka_scene* target_scene,
    const char* name_prefix,
    size_t* out_entity_count);

/*
 * Retries only a cached texture fallback from a previous failed load.
 * The fallback entry and borrowed texture pointer remain intact when the
 * replacement load fails. Failure leaves out_texture null. Success updates the
 * existing borrowed texture object in place, so materials do not retain a
 * permanently stale fallback pointer.
 */
henka_result henka_assets_retry_failed_texture(
    henka_asset_manager* manager,
    const char* path,
    henka_texture** out_texture);

/*
 * Retries only a cached fallback entry from a previous failed OBJ load.
 * The fallback entry remains intact when the replacement load fails.
 */
henka_result henka_assets_retry_failed_obj_mesh(henka_asset_manager* manager, const char* path, henka_mesh** out_mesh);
henka_result henka_assets_retry_failed_gltf_mesh(henka_asset_manager* manager, const char* path, henka_mesh** out_mesh);
const char* henka_assets_get_type_label(henka_asset_type type);
size_t henka_assets_get_metadata_count(const henka_asset_manager* manager);
henka_result henka_assets_get_metadata_at_index(const henka_asset_manager* manager, size_t index, henka_asset_metadata* out_metadata);
henka_result henka_assets_get_shader_metadata(const henka_asset_manager* manager, const henka_shader* shader, henka_asset_metadata* out_metadata);
/*
 * Path-specific fallback aliases support pointer metadata. The shared
 * manager error texture itself has no path-specific metadata.
 */
henka_result henka_assets_get_texture_metadata(const henka_asset_manager* manager, const henka_texture* texture, henka_asset_metadata* out_metadata);
henka_result henka_assets_get_mesh_metadata(const henka_asset_manager* manager, const henka_mesh* mesh, henka_asset_metadata* out_metadata);
henka_result henka_assets_get_texture_metadata_for_path(
    const henka_asset_manager* manager,
    const char* path,
    henka_asset_metadata* out_metadata);
henka_result henka_assets_get_mesh_metadata_for_path(
    const henka_asset_manager* manager,
    const char* path,
    henka_asset_metadata* out_metadata);
/* Borrowed manager-owned fallback assets. */
henka_texture* henka_assets_get_white_texture(henka_asset_manager* manager);
henka_texture* henka_assets_get_error_texture(henka_asset_manager* manager);
henka_mesh* henka_assets_get_fallback_mesh(henka_asset_manager* manager);

#endif
