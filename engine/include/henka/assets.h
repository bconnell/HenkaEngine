#ifndef HENKA_ASSETS_H
#define HENKA_ASSETS_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/result.h>

typedef struct henka_asset_manager henka_asset_manager;
typedef struct henka_engine henka_engine;
typedef struct henka_mesh henka_mesh;
typedef struct henka_shader henka_shader;
typedef struct henka_texture henka_texture;

typedef enum henka_asset_type
{
    HENKA_ASSET_TYPE_UNKNOWN = 0,
    HENKA_ASSET_TYPE_SHADER,
    HENKA_ASSET_TYPE_TEXTURE,
    HENKA_ASSET_TYPE_MESH
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
} henka_asset_metadata;

henka_asset_manager* henka_engine_get_asset_manager(henka_engine* engine);
const henka_asset_manager* henka_engine_get_asset_manager_const(const henka_engine* engine);
/*
 * Resolves an asset path against a base directory. The caller owns the
 * returned string and must release it with henka_free.
 */
henka_result henka_assets_resolve_path(const char* base_path, const char* asset_path, char** out_path);
henka_result henka_assets_load_shader(
    henka_asset_manager* manager,
    const char* vertex_path,
    const char* fragment_path,
    henka_shader** out_shader);
henka_result henka_assets_load_texture(henka_asset_manager* manager, const char* path, henka_texture** out_texture);
henka_result henka_assets_load_obj_mesh(henka_asset_manager* manager, const char* path, henka_mesh** out_mesh);
/*
 * Retries an OBJ mesh path only when the cached entry is a fallback from a
 * previous failed load. Already-loaded real meshes are returned unchanged so
 * scenes do not lose a mesh they may still reference.
 */
henka_result henka_assets_retry_failed_obj_mesh(henka_asset_manager* manager, const char* path, henka_mesh** out_mesh);
const char* henka_assets_get_type_label(henka_asset_type type);
size_t henka_assets_get_metadata_count(const henka_asset_manager* manager);
henka_result henka_assets_get_metadata_at_index(const henka_asset_manager* manager, size_t index, henka_asset_metadata* out_metadata);
henka_result henka_assets_get_shader_metadata(const henka_asset_manager* manager, const henka_shader* shader, henka_asset_metadata* out_metadata);
henka_result henka_assets_get_texture_metadata(const henka_asset_manager* manager, const henka_texture* texture, henka_asset_metadata* out_metadata);
henka_result henka_assets_get_mesh_metadata(const henka_asset_manager* manager, const henka_mesh* mesh, henka_asset_metadata* out_metadata);
henka_texture* henka_assets_get_white_texture(henka_asset_manager* manager);
henka_texture* henka_assets_get_error_texture(henka_asset_manager* manager);
henka_mesh* henka_assets_get_fallback_mesh(henka_asset_manager* manager);

#endif
