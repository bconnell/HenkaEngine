#ifndef HENKA_ASSETS_H
#define HENKA_ASSETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

typedef struct henka_texture_residency_diagnostics
{
    uint64_t budget_bytes;
    uint64_t resident_bytes;
    uint64_t uploaded_bytes;
    uint64_t evicted_bytes;
    uint32_t budget_rejection_count;
    size_t managed_texture_count;
    size_t fallback_texture_count;
    size_t queued_request_count;
    uint64_t completed_request_count;
    uint64_t failed_request_count;
    uint64_t eviction_count;
    uint64_t eviction_failure_count;
    bool budget_exceeded;
} henka_texture_residency_diagnostics;

#define HENKA_MAX_TEXTURE_RESIDENCY_REQUESTS 64U

#define HENKA_MATERIAL_MAX_TEXTURE_DEPENDENCIES 5U

typedef enum henka_material_instance_parameter
{
    HENKA_MATERIAL_INSTANCE_METALLIC = 0,
    HENKA_MATERIAL_INSTANCE_ROUGHNESS,
    HENKA_MATERIAL_INSTANCE_SPECULAR_FACTOR,
    HENKA_MATERIAL_INSTANCE_IOR,
    HENKA_MATERIAL_INSTANCE_TRANSMISSION,
    HENKA_MATERIAL_INSTANCE_NORMAL_SCALE,
    HENKA_MATERIAL_INSTANCE_OCCLUSION_STRENGTH,
    HENKA_MATERIAL_INSTANCE_EMISSIVE_STRENGTH,
    HENKA_MATERIAL_INSTANCE_CLEARCOAT,
    HENKA_MATERIAL_INSTANCE_CLEARCOAT_ROUGHNESS,
    HENKA_MATERIAL_INSTANCE_ALPHA_CUTOFF,
    HENKA_MATERIAL_INSTANCE_SHEEN_ROUGHNESS,
    HENKA_MATERIAL_INSTANCE_BASE_COLOR,
    HENKA_MATERIAL_INSTANCE_EMISSIVE_COLOR,
    HENKA_MATERIAL_INSTANCE_SPECULAR_COLOR,
    HENKA_MATERIAL_INSTANCE_SHEEN_COLOR,
    HENKA_MATERIAL_INSTANCE_USE_LIGHTING,
    HENKA_MATERIAL_INSTANCE_DEPTH_TEST,
    HENKA_MATERIAL_INSTANCE_DOUBLE_SIDED,
    HENKA_MATERIAL_INSTANCE_CAST_SHADOWS,
    HENKA_MATERIAL_INSTANCE_RECEIVE_SHADOWS,
    HENKA_MATERIAL_INSTANCE_ALPHA_MODE,
    HENKA_MATERIAL_INSTANCE_THICKNESS,
    HENKA_MATERIAL_INSTANCE_ATTENUATION_DISTANCE,
    HENKA_MATERIAL_INSTANCE_ATTENUATION_COLOR,
    HENKA_MATERIAL_INSTANCE_BASE_COLOR_TEXTURE,
    HENKA_MATERIAL_INSTANCE_NORMAL_TEXTURE,
    HENKA_MATERIAL_INSTANCE_METALLIC_ROUGHNESS_TEXTURE,
    HENKA_MATERIAL_INSTANCE_OCCLUSION_TEXTURE,
    HENKA_MATERIAL_INSTANCE_EMISSIVE_TEXTURE,
    HENKA_MATERIAL_INSTANCE_PARAMETER_COUNT
} henka_material_instance_parameter;

typedef enum henka_material_texture_slot
{
    HENKA_MATERIAL_TEXTURE_SLOT_BASE_COLOR = 0,
    HENKA_MATERIAL_TEXTURE_SLOT_NORMAL,
    HENKA_MATERIAL_TEXTURE_SLOT_METALLIC_ROUGHNESS,
    HENKA_MATERIAL_TEXTURE_SLOT_OCCLUSION,
    HENKA_MATERIAL_TEXTURE_SLOT_EMISSIVE
} henka_material_texture_slot;

typedef struct henka_material_dependency
{
    henka_material_texture_slot slot;
    henka_texture_usage usage;
    const henka_texture* texture;
} henka_material_dependency;

typedef struct henka_material_dependency_info
{
    uint64_t definition_revision;
    size_t dependency_count;
    henka_material_dependency dependencies[HENKA_MATERIAL_MAX_TEXTURE_DEPENDENCIES];
} henka_material_dependency_info;

/* A stack-owned effective material view. Texture and shader pointers remain
 * borrowed from the manager and the definition identity is not owned. */
typedef struct henka_material_instance
{
    const henka_material_asset* definition;
    henka_material material;
    uint64_t definition_revision;
    uint32_t override_mask;
} henka_material_instance;

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
/* A zero budget disables the limit. New loads and replacements that would
 * exceed a non-zero budget are rejected until the caller trims residency. */
henka_result henka_assets_set_texture_residency_budget(
    henka_asset_manager* manager,
    uint64_t budget_bytes);
henka_result henka_assets_get_texture_residency_diagnostics(
    const henka_asset_manager* manager,
    henka_texture_residency_diagnostics* out_diagnostics);
/* Coalesces a bounded manager-owned KTX2 mip request. Repeated requests for
 * one texture retain the strongest resident-mip target. Processing is
 * explicit and synchronous; background I/O remains unfinished. */
henka_result henka_assets_queue_texture_residency_request(
    henka_asset_manager* manager,
    henka_texture* texture,
    uint32_t resident_mip_count);
/* Queues a bounded request with a deterministic priority. Higher priorities
 * are serviced first; equal priorities use mip demand and then request order. Repeated
 * references coalesce to the strongest mip target and priority. */
henka_result henka_assets_queue_texture_residency_request_with_priority(
    henka_asset_manager* manager,
    henka_texture* texture,
    uint32_t resident_mip_count,
    uint32_t priority);
henka_result henka_assets_process_texture_residency_requests(
    henka_asset_manager* manager,
    size_t max_requests,
    size_t* out_processed_requests);
/* Synchronously replaces a manager-owned KTX2 texture with a bounded prefix
 * of its mip chain. The source is reread transactionally; zero is not a
 * valid request, and non-KTX2 sources remain non-streamable. */
henka_result henka_assets_set_texture_resident_mips(
    henka_asset_manager* manager,
    henka_texture* texture,
    uint32_t resident_mip_count,
    henka_texture_info* out_info);
/* Synchronously trims the largest eligible manager-owned KTX2 textures to
 * their smallest valid resident prefix until target_bytes is met. A zero
 * max_evictions means no operation-count limit. Non-KTX2 sources are not
 * candidates, and failure leaves each texture's prior payload intact. */
henka_result henka_assets_trim_texture_residency(
    henka_asset_manager* manager,
    uint64_t target_bytes,
    size_t max_evictions,
    size_t* out_evicted_textures);
/* Applies the manager's configured non-zero budget using the same bounded,
 * transactional trim policy. A zero configured budget is a successful no-op. */
henka_result henka_assets_enforce_texture_residency_budget(
    henka_asset_manager* manager,
    size_t max_evictions,
    size_t* out_evicted_textures);
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
henka_result henka_assets_get_material_asset_revision(
    const henka_material_asset* asset,
    uint64_t* out_revision);
henka_result henka_assets_get_material_asset_dependencies(
    const henka_material_asset* asset,
    henka_material_dependency_info* out_dependencies);
/* Reports the effective borrowed texture dependencies after instance
 * overrides. The definition revision remains available for refresh checks. */
henka_result henka_assets_get_material_instance_dependencies(
    const henka_material_instance* instance,
    henka_material_dependency_info* out_dependencies);
henka_result henka_assets_create_material_instance(
    const henka_material_asset* asset,
    henka_material_instance* out_instance);
henka_result henka_assets_refresh_material_instance(
    henka_material_instance* instance);
henka_result henka_assets_get_material_instance_material(
    const henka_material_instance* instance,
    henka_material* out_material);
/* Applies the validated effective instance view to a scene entity. The scene
 * receives its own material value; the definition and instance remain
 * caller-owned, so callers can refresh/reimport and apply again transactionally. */
henka_result henka_assets_apply_material_instance_to_entity(
    const henka_material_instance* instance,
    henka_scene* scene,
    henka_entity entity);
henka_result henka_assets_material_instance_set_float(
    henka_material_instance* instance,
    henka_material_instance_parameter parameter,
    float value);
henka_result henka_assets_material_instance_set_vec3(
    henka_material_instance* instance,
    henka_material_instance_parameter parameter,
    henka_vec3 value);
henka_result henka_assets_material_instance_set_vec4(
    henka_material_instance* instance,
    henka_material_instance_parameter parameter,
    henka_vec4 value);
henka_result henka_assets_material_instance_set_bool(
    henka_material_instance* instance,
    henka_material_instance_parameter parameter,
    bool value);
henka_result henka_assets_material_instance_set_alpha_mode(
    henka_material_instance* instance,
    henka_material_alpha_mode mode);
/* Assigns a borrowed semantic texture to an instance without changing the
 * shared definition. A null base-color texture disables texture sampling for
 * the effective instance; all other null values clear only that slot. */
henka_result henka_assets_material_instance_set_texture(
    henka_material_instance* instance,
    henka_material_texture_slot slot,
    henka_texture* texture);
/* Clears one validated override or all overrides, restoring definition values
 * transactionally while preserving the stable definition identity. */
henka_result henka_assets_material_instance_reset_override(
    henka_material_instance* instance,
    henka_material_instance_parameter parameter);
henka_result henka_assets_material_instance_reset_overrides(
    henka_material_instance* instance);
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
henka_result henka_assets_set_gltf_scene_active_scene(
    henka_gltf_scene_asset* asset,
    size_t scene_index);
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
