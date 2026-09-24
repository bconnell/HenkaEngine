#include "game_authoring.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <henka/memory.h>
#include <henka/authoring_modeling.h>
#include <henka/engine.h>
#include <henka/persistence.h>
#include <henka/prefab.h>
#include <henka/script_asset.h>

#define SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS HENKA_SCENE_DOCUMENT_MAX_OBJECTS
#define SANDBOX3D_GAME_AUTHORING_MAX_RELATIVE_PATH_BYTES HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES
#define SANDBOX3D_GAME_AUTHORING_MAX_HISTORY_STEPS 32U
#define SANDBOX3D_GAME_AUTHORING_PROJECT_MANIFEST_PATH "henka.project"
#define SANDBOX3D_GAME_AUTHORING_PROJECT_MANIFEST_SCHEMA_VERSION 1
#define SANDBOX3D_GAME_AUTHORING_PROJECT_STAGE_SUFFIX ".henka-project-stage"
#define SANDBOX3D_GAME_AUTHORING_PROJECT_ROLLBACK_SUFFIX ".henka-project-rollback"
#define SANDBOX3D_GAME_AUTHORING_PROJECT_MANIFEST_MAX_BYTES (64U * 1024U)

typedef struct sandbox3d_game_authoring_binding
{
    henka_scene_document_id document_id;
    henka_entity entity;
} sandbox3d_game_authoring_binding;

typedef struct sandbox3d_game_authoring_history_entry
{
    henka_entity entity;
    henka_scene_document_object before;
    henka_scene_document_object after;
} sandbox3d_game_authoring_history_entry;

struct sandbox3d_game_authoring
{
    henka_scene* scene;
    henka_scene_document* document;
    sandbox3d_scene_document_bridge* bridge;
    henka_physics_world* play_world;
    henka_script_state_store* script_state_store;
    /* Edit-owned state is never borrowed by a live Play host.  The last
     * stopped Play store is retained only for an explicit save operation. */
    henka_script_state_store* play_script_state_store;
    henka_scene* play_scene;
    sandbox3d_scene_document_bridge* play_bridge;
    sandbox3d_play_session* play_session;
    henka_audio_system* audio_system;
    henka_asset_manager* audio_asset_manager;
    sandbox3d_game_authoring_input_query play_input_query;
    void* play_input_user_data;
    henka_vec3 play_observer_position;
    bool play_observer_position_valid;
    char relative_path[SANDBOX3D_GAME_AUTHORING_MAX_RELATIVE_PATH_BYTES];
    char project_root[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    /* Project source authorities are borrowed. The engine must outlive this
     * coordinator when native or authoring meshes are materialized; the asset
     * manager must outlive it when manager-owned sources are rematerialized. */
    henka_engine* project_engine;
    henka_asset_manager* project_assets;
    sandbox3d_game_authoring_binding bindings[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    size_t binding_count;
    /* Scene entities borrow render meshes. Native primitive sources reconstructed
     * during project open are retained here until the coordinator is destroyed;
     * manager-backed sources remain owned by the asset manager. Prefab snapshots
     * and instance mappings are retained as the borrowed source authority for
     * materialized prefab entities. */
    henka_mesh* owned_project_meshes[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    size_t owned_project_mesh_count;
    henka_prefab* project_prefabs[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    henka_prefab_instance* project_prefab_instances[
        SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    bool project_prefab_manager_owned[
        SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    size_t project_prefab_count;
    sandbox3d_game_authoring_history_entry history[
        SANDBOX3D_GAME_AUTHORING_MAX_HISTORY_STEPS];
    size_t history_entry_count;
    size_t history_applied_count;
    bool history_replaying;
};

static void sandbox3d_game_authoring_clear_history(
    sandbox3d_game_authoring* authoring)
{
    if (authoring == NULL || authoring->history_replaying)
    {
        return;
    }
    authoring->history_entry_count = 0U;
    authoring->history_applied_count = 0U;
}

static bool sandbox3d_game_authoring_authored_state_equal(
    const henka_scene_document_object* left,
    const henka_scene_document_object* right)
{
    return left != NULL && right != NULL &&
        memcmp(left, right, sizeof(*left)) == 0;
}

static void sandbox3d_game_authoring_append_history(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    const henka_scene_document_object* before,
    const henka_scene_document_object* after)
{
    if (authoring == NULL || entity == HENKA_INVALID_ENTITY ||
        before == NULL || after == NULL ||
        sandbox3d_game_authoring_authored_state_equal(before, after))
    {
        return;
    }
    if (authoring->history_applied_count < authoring->history_entry_count)
    {
        authoring->history_entry_count = authoring->history_applied_count;
    }
    if (authoring->history_entry_count >= SANDBOX3D_GAME_AUTHORING_MAX_HISTORY_STEPS)
    {
        memmove(
            &authoring->history[0],
            &authoring->history[1],
            (SANDBOX3D_GAME_AUTHORING_MAX_HISTORY_STEPS - 1U) *
                sizeof(authoring->history[0]));
        authoring->history_entry_count =
            SANDBOX3D_GAME_AUTHORING_MAX_HISTORY_STEPS - 1U;
        authoring->history_applied_count = authoring->history_entry_count;
    }
    authoring->history[authoring->history_entry_count++] =
        (sandbox3d_game_authoring_history_entry){entity, *before, *after};
    authoring->history_applied_count = authoring->history_entry_count;
}

static size_t sandbox3d_game_authoring_find_binding(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity)
{
    size_t index;
    if (authoring == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return SIZE_MAX;
    }
    for (index = 0U; index < authoring->binding_count; ++index)
    {
        if (authoring->bindings[index].entity == entity)
        {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool sandbox3d_game_authoring_material_definition_resources_match(
    const henka_material* effective,
    const henka_material* definition)
{
    if (effective == NULL || definition == NULL ||
        effective->shader != definition->shader ||
        effective->terrain_layers_enabled != definition->terrain_layers_enabled)
    {
        return false;
    }
    if (effective->terrain_layers_enabled)
    {
        size_t layer_index;
        for (layer_index = 0U;
             layer_index < HENKA_MATERIAL_TERRAIN_LAYER_COUNT;
             ++layer_index)
        {
            const henka_material_layer* effective_layer =
                &effective->terrain_layers[layer_index];
            const henka_material_layer* definition_layer =
                &definition->terrain_layers[layer_index];
            if (effective_layer->base_color_texture !=
                    definition_layer->base_color_texture ||
                effective_layer->normal_texture !=
                    definition_layer->normal_texture ||
                effective_layer->metallic_roughness_texture !=
                    definition_layer->metallic_roughness_texture)
            {
                return false;
            }
        }
    }
    return true;
}

static henka_result sandbox3d_game_authoring_capture_texture_overrides(
    const sandbox3d_game_authoring* authoring,
    const henka_material* effective,
    const henka_material* definition,
    henka_scene_document_renderer* renderer)
{
    const henka_texture* effective_textures[] = {
        effective->base_color_texture,
        effective->normal_texture,
        effective->metallic_roughness_texture,
        effective->occlusion_texture,
        effective->emissive_texture,
        effective->transmission_texture,
        effective->thickness_texture};
    const henka_texture* definition_textures[] = {
        definition->base_color_texture,
        definition->normal_texture,
        definition->metallic_roughness_texture,
        definition->occlusion_texture,
        definition->emissive_texture,
        definition->transmission_texture,
        definition->thickness_texture};
    const uint32_t texture_override_bits[] = {
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_BASE_COLOR,
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_NORMAL,
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_METALLIC_ROUGHNESS,
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_OCCLUSION,
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_EMISSIVE,
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_TRANSMISSION,
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_THICKNESS};
    char* texture_paths[] = {
        renderer->base_color_texture_path,
        renderer->normal_texture_path,
        renderer->metallic_roughness_texture_path,
        renderer->occlusion_texture_path,
        renderer->emissive_texture_path,
        renderer->transmission_texture_path,
        renderer->thickness_texture_path};
    size_t texture_index;

    if (authoring == NULL || effective == NULL || definition == NULL ||
        renderer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    renderer->texture_override_mask = 0U;
    for (texture_index = 0U;
         texture_index < sizeof(effective_textures) / sizeof(effective_textures[0]);
         ++texture_index)
    {
        henka_asset_metadata metadata;
        if (effective_textures[texture_index] == definition_textures[texture_index])
        {
            continue;
        }
        renderer->texture_override_mask |= texture_override_bits[texture_index];
        if (effective_textures[texture_index] == NULL)
        {
            continue;
        }
        if (authoring->project_assets == NULL ||
            henka_assets_get_texture_metadata(
                authoring->project_assets,
                effective_textures[texture_index],
                &metadata) != HENKA_SUCCESS ||
            metadata.source_path == NULL ||
            strlen(metadata.source_path) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES ||
            snprintf(
                texture_paths[texture_index],
                HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES,
                "%s",
                metadata.source_path) < 0)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_game_authoring_build_object(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_object* out_object)
{
    const henka_scene* scene;
    henka_scene_object_info info;
    henka_interaction_desc interaction;
    henka_material material;
    henka_material definition_material;
    henka_asset_metadata material_metadata;
    const henka_material_asset* material_asset = NULL;
    bool material_asset_overridden = false;
    uint64_t material_asset_revision = 0U;
    int written;
    scene = authoring == NULL ? NULL : authoring->scene;
    if (scene == NULL || out_object == NULL ||
        !henka_scene_is_entity_valid(scene, entity) ||
        henka_scene_get_entity_info(scene, entity, &info) != HENKA_SUCCESS ||
        henka_scene_get_entity_interaction(scene, entity, &interaction) != HENKA_SUCCESS ||
        henka_scene_get_entity_material(scene, entity, &material) != HENKA_SUCCESS ||
        henka_scene_get_entity_material_asset(scene, entity, &material_asset) != HENKA_SUCCESS ||
        henka_scene_get_entity_material_asset_state(
            scene,
            entity,
            &material_asset_revision,
            &material_asset_overridden) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    (void)material_asset_revision;
    *out_object = henka_scene_document_object_default();
    written = snprintf(
        out_object->name,
        sizeof(out_object->name),
        "%s",
        info.name == NULL ? "Object" : info.name);
    if (written < 0 || (size_t)written >= sizeof(out_object->name))
    {
        return HENKA_ERROR_LIMIT;
    }
    out_object->visible = info.visible;
    out_object->transform = info.transform;
    out_object->renderer.enabled = info.renderer_enabled;
    out_object->renderer.material_type = material.type;
    out_object->renderer.base_color_uv_set = material.base_color_uv_set;
    out_object->renderer.normal_uv_set = material.normal_uv_set;
    out_object->renderer.metallic_roughness_uv_set = material.metallic_roughness_uv_set;
    out_object->renderer.occlusion_uv_set = material.occlusion_uv_set;
    out_object->renderer.emissive_uv_set = material.emissive_uv_set;
    out_object->renderer.transmission_uv_set = material.transmission_uv_set;
    out_object->renderer.thickness_uv_set = material.thickness_uv_set;
    out_object->renderer.base_color = material.base_color;
    out_object->renderer.metallic = material.metallic;
    out_object->renderer.roughness = material.roughness;
    out_object->renderer.emissive = material.emissive_color;
    out_object->renderer.emissive_strength = material.emissive_strength;
    out_object->renderer.specular_factor = material.specular_factor;
    out_object->renderer.specular_color = material.specular_color;
    out_object->renderer.ior = material.ior;
    out_object->renderer.transmission = material.transmission;
    out_object->renderer.thickness = material.thickness;
    out_object->renderer.attenuation_distance = material.attenuation_distance;
    out_object->renderer.attenuation_color = material.attenuation_color;
    out_object->renderer.subsurface = material.subsurface;
    out_object->renderer.subsurface_color = material.subsurface_color;
    out_object->renderer.normal_scale = material.normal_scale;
    out_object->renderer.occlusion_strength = material.occlusion_strength;
    out_object->renderer.clearcoat = material.clearcoat;
    out_object->renderer.clearcoat_roughness = material.clearcoat_roughness;
    out_object->renderer.alpha_cutoff = material.alpha_cutoff;
    out_object->renderer.alpha_mode = material.alpha_mode;
    out_object->renderer.use_texture = material.use_texture;
    out_object->renderer.use_lighting = material.use_lighting;
    out_object->renderer.depth_test = material.depth_test;
    out_object->renderer.double_sided = material.double_sided;
    out_object->renderer.cast_shadows = material.cast_shadows;
    out_object->renderer.receive_shadows = material.receive_shadows;
    out_object->renderer.sheen_color = material.sheen_color;
    out_object->renderer.sheen_roughness = material.sheen_roughness;
    if (material_asset != NULL &&
        (authoring->project_assets == NULL ||
            henka_assets_get_material_asset_material(
                material_asset,
                &definition_material) != HENKA_SUCCESS ||
            !sandbox3d_game_authoring_material_definition_resources_match(
                &material,
                &definition_material) ||
            (material_asset_overridden &&
                sandbox3d_game_authoring_capture_texture_overrides(
                    authoring,
                    &material,
                    &definition_material,
                    &out_object->renderer) != HENKA_SUCCESS)))
    {
        /* Manager-owned definitions remain asset authority. Resource
         * dependencies are captured only as stable paths for explicit
         * supported instance overrides; terrain resources remain external. */
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (material_asset == NULL &&
            (material.base_color_texture != NULL ||
             material.normal_texture != NULL ||
             material.metallic_roughness_texture != NULL ||
             material.occlusion_texture != NULL ||
             material.emissive_texture != NULL ||
             material.transmission_texture != NULL ||
             material.thickness_texture != NULL ||
             material.terrain_layers_enabled))
    {
        /* An inline document cannot reconstruct borrowed resource pointers.
         * Reject instead of silently retaining a second authority. */
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (material_asset != NULL)
    {
        if (authoring->project_assets == NULL ||
            henka_assets_get_material_metadata(
                authoring->project_assets,
                material_asset,
                &material_metadata) != HENKA_SUCCESS ||
            material_metadata.source_path == NULL ||
            strlen(material_metadata.source_path) >=
                sizeof(out_object->renderer.material_path))
        {
            /* A manager-owned definition is persisted by source identity. If
             * that identity is unavailable, reject capture instead of
             * discarding the asset authority or inventing inline truth. */
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (snprintf(
                out_object->renderer.material_path,
                sizeof(out_object->renderer.material_path),
                "%s",
                material_metadata.source_path) < 0)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    /* Pointer-free inline material state is document-owned only when no
     * manager-owned definition or borrowed texture state is attached. */
    out_object->renderer.material_override =
        material_asset_overridden ||
        out_object->renderer.texture_override_mask != 0U ||
        (material_asset == NULL && material.shader != NULL);
    out_object->interaction.enabled = interaction.enabled;
    out_object->interaction.max_distance = interaction.max_distance;
    written = snprintf(
        out_object->interaction.prompt,
        sizeof(out_object->interaction.prompt),
        "%s",
        interaction.prompt == NULL ? "" : interaction.prompt);
    if (written < 0 || (size_t)written >= sizeof(out_object->interaction.prompt))
    {
        return HENKA_ERROR_LIMIT;
    }
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_game_authoring_set_project_root(
    sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    int written;
    if (authoring == NULL || project_root == NULL || project_root[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    written = snprintf(
        authoring->project_root,
        sizeof(authoring->project_root),
        "%s",
        project_root);
    return written < 0
        ? HENKA_ERROR_INVALID_ARGUMENT
        : (size_t)written >= sizeof(authoring->project_root)
            ? HENKA_ERROR_LIMIT
            : HENKA_SUCCESS;
}


typedef struct sandbox3d_game_authoring_file_snapshot
{
    unsigned char* data;
    size_t size;
    bool existed;
} sandbox3d_game_authoring_file_snapshot;

static void sandbox3d_game_authoring_file_snapshot_destroy(
    sandbox3d_game_authoring_file_snapshot* snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }
    henka_free(snapshot->data);
    memset(snapshot, 0, sizeof(*snapshot));
}

static char* sandbox3d_game_authoring_append_path_suffix(
    const char* path,
    const char* suffix)
{
    const size_t path_length = path != NULL ? strlen(path) : 0U;
    const size_t suffix_length = suffix != NULL ? strlen(suffix) : 0U;
    char* combined;

    if (path == NULL || suffix == NULL ||
        path_length > SIZE_MAX - suffix_length - 1U)
    {
        return NULL;
    }
    combined = (char*)henka_malloc(path_length + suffix_length + 1U);
    if (combined == NULL)
    {
        return NULL;
    }
    memcpy(combined, path, path_length);
    memcpy(combined + path_length, suffix, suffix_length + 1U);
    return combined;
}

static henka_result sandbox3d_game_authoring_snapshot_file(
    const char* path,
    size_t max_bytes,
    sandbox3d_game_authoring_file_snapshot* out_snapshot)
{
    FILE* file = NULL;
    long length;
    size_t size;

    if (out_snapshot != NULL)
    {
        memset(out_snapshot, 0, sizeof(*out_snapshot));
    }
    if (path == NULL || path[0] == '\0' || max_bytes == 0U ||
        out_snapshot == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

#if defined(_MSC_VER)
    {
        const errno_t open_result = fopen_s(&file, path, "rb");
        if (open_result != 0)
        {
            return open_result == ENOENT
                ? HENKA_SUCCESS
                : HENKA_ERROR_PLATFORM;
        }
    }
#else
    errno = 0;
    file = fopen(path, "rb");
    if (file == NULL)
    {
        return errno == ENOENT
            ? HENKA_SUCCESS
            : HENKA_ERROR_PLATFORM;
    }
#endif

    if (fseek(file, 0L, SEEK_END) != 0 ||
        (length = ftell(file)) < 0L ||
        (uint64_t)length > (uint64_t)max_bytes ||
        fseek(file, 0L, SEEK_SET) != 0)
    {
        fclose(file);
        return HENKA_ERROR_LIMIT;
    }

    size = (size_t)length;
    if (size > 0U)
    {
        out_snapshot->data = (unsigned char*)henka_malloc(size);
        if (out_snapshot->data == NULL)
        {
            fclose(file);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        if (fread(out_snapshot->data, 1U, size, file) != size)
        {
            fclose(file);
            sandbox3d_game_authoring_file_snapshot_destroy(out_snapshot);
            return HENKA_ERROR_PLATFORM;
        }
    }
    if (fclose(file) != 0)
    {
        sandbox3d_game_authoring_file_snapshot_destroy(out_snapshot);
        return HENKA_ERROR_PLATFORM;
    }

    out_snapshot->size = size;
    out_snapshot->existed = true;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_game_authoring_replace_file(
    const char* source_path,
    const char* destination_path)
{
    if (source_path == NULL || source_path[0] == '\0' ||
        destination_path == NULL || destination_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
#if defined(_WIN32)
    return MoveFileExA(
        source_path,
        destination_path,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
        ? HENKA_SUCCESS
        : HENKA_ERROR_PLATFORM;
#else
    return rename(source_path, destination_path) == 0
        ? HENKA_SUCCESS
        : HENKA_ERROR_PLATFORM;
#endif
}

static henka_result sandbox3d_game_authoring_restore_snapshot(
    const char* path,
    const sandbox3d_game_authoring_file_snapshot* snapshot)
{
    char* rollback_path = NULL;
    FILE* file = NULL;
    henka_result result = HENKA_SUCCESS;

    if (path == NULL || path[0] == '\0' || snapshot == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!snapshot->existed)
    {
        if (remove(path) != 0 && errno != ENOENT)
        {
            return HENKA_ERROR_PLATFORM;
        }
        return HENKA_SUCCESS;
    }

    rollback_path = sandbox3d_game_authoring_append_path_suffix(
        path,
        SANDBOX3D_GAME_AUTHORING_PROJECT_ROLLBACK_SUFFIX);
    if (rollback_path == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    (void)remove(rollback_path);
    result = henka_path_ensure_parent_directory(path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(rollback_path);
        return result;
    }

#if defined(_MSC_VER)
    if (fopen_s(&file, rollback_path, "wb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(rollback_path, "wb");
#endif
    if (file == NULL)
    {
        henka_free(rollback_path);
        return HENKA_ERROR_PLATFORM;
    }
    if ((snapshot->size > 0U &&
            fwrite(snapshot->data, 1U, snapshot->size, file) != snapshot->size) ||
        fflush(file) != 0)
    {
        result = HENKA_ERROR_PLATFORM;
    }
    if (fclose(file) != 0 && result == HENKA_SUCCESS)
    {
        result = HENKA_ERROR_PLATFORM;
    }
    file = NULL;

    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_game_authoring_replace_file(
            rollback_path,
            path);
    }
    if (result != HENKA_SUCCESS)
    {
        (void)remove(rollback_path);
    }
    henka_free(rollback_path);
    return result;
}

static henka_result sandbox3d_game_authoring_get_project_manifest_path(
    const char* project_root,
    char** out_manifest_path)
{
    if (project_root == NULL || project_root[0] == '\0' ||
        out_manifest_path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_path_resolve_confined(
        project_root,
        SANDBOX3D_GAME_AUTHORING_PROJECT_MANIFEST_PATH,
        out_manifest_path);
}

static henka_result sandbox3d_game_authoring_save_project_manifest_to_path(
    const sandbox3d_game_authoring* authoring,
    const char* project_root,
    const char* manifest_path)
{
    henka_settings* settings = NULL;
    char* scene_path = NULL;
    henka_result result;

    if (authoring == NULL || project_root == NULL ||
        authoring->relative_path[0] == '\0' ||
        manifest_path == NULL || manifest_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_path_resolve_confined(
        project_root,
        authoring->relative_path,
        &scene_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    henka_free(scene_path);

    result = henka_settings_create(&settings);
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_int(
            settings,
            "schema_version",
            SANDBOX3D_GAME_AUTHORING_PROJECT_MANIFEST_SCHEMA_VERSION);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_set_string(
            settings,
            "startup_scene",
            authoring->relative_path);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_save_file(settings, manifest_path);
    }
    henka_settings_destroy(settings);
    return result;
}

static henka_result sandbox3d_game_authoring_get_startup_scene_path(
    const sandbox3d_game_authoring* authoring,
    const char* project_root,
    char* out_relative_path,
    size_t out_relative_path_capacity,
    bool allow_legacy_fallback)
{
    FILE* manifest_file = NULL;
    henka_settings* settings = NULL;
    const char* startup_scene;
    char* manifest_path = NULL;
    char* scene_path = NULL;
    int written;
    henka_result result;

    if (authoring == NULL || project_root == NULL ||
        out_relative_path == NULL || out_relative_path_capacity == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_game_authoring_get_project_manifest_path(
        project_root,
        &manifest_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
#if defined(_MSC_VER)
    {
        const errno_t open_result = fopen_s(&manifest_file, manifest_path, "rb");
        if (open_result != 0 && open_result != ENOENT)
        {
            henka_free(manifest_path);
            return HENKA_ERROR_UNKNOWN;
        }
    }
#else
    errno = 0;
    manifest_file = fopen(manifest_path, "rb");
    if (manifest_file == NULL && errno != ENOENT)
    {
        henka_free(manifest_path);
        return HENKA_ERROR_UNKNOWN;
    }
#endif
    if (manifest_file == NULL)
    {
        henka_free(manifest_path);
        if (!allow_legacy_fallback)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        written = snprintf(
            out_relative_path,
            out_relative_path_capacity,
            "%s",
            authoring->relative_path);
        return written < 0
            ? HENKA_ERROR_INVALID_ARGUMENT
            : (size_t)written >= out_relative_path_capacity
                ? HENKA_ERROR_LIMIT
                : HENKA_SUCCESS;
    }
    if (fclose(manifest_file) != 0)
    {
        henka_free(manifest_path);
        return HENKA_ERROR_UNKNOWN;
    }
    result = henka_settings_create(&settings);
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_load_file(settings, manifest_path);
    }
    henka_free(manifest_path);
    if (result != HENKA_SUCCESS)
    {
        henka_settings_destroy(settings);
        return result;
    }
    if (!henka_settings_has_key(settings, "schema_version") ||
        henka_settings_get_int(settings, "schema_version", 0) !=
            SANDBOX3D_GAME_AUTHORING_PROJECT_MANIFEST_SCHEMA_VERSION ||
        !henka_settings_has_key(settings, "startup_scene"))
    {
        henka_settings_destroy(settings);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    startup_scene = henka_settings_get_string(settings, "startup_scene", "");
    result = henka_path_resolve_confined(
        project_root,
        startup_scene,
        &scene_path);
    henka_free(scene_path);
    if (result != HENKA_SUCCESS || startup_scene[0] == '\0')
    {
        henka_settings_destroy(settings);
        return result == HENKA_SUCCESS ? HENKA_ERROR_INVALID_ARGUMENT : result;
    }
    written = snprintf(
        out_relative_path,
        out_relative_path_capacity,
        "%s",
        startup_scene);
    henka_settings_destroy(settings);
    return written < 0
        ? HENKA_ERROR_INVALID_ARGUMENT
        : (size_t)written >= out_relative_path_capacity
            ? HENKA_ERROR_LIMIT
            : HENKA_SUCCESS;
}

static henka_result sandbox3d_game_authoring_get_state_relative_path(
    const sandbox3d_game_authoring* authoring,
    char* out_path,
    size_t out_path_capacity)
{
    int written;
    if (authoring == NULL || out_path == NULL || out_path_capacity == 0U ||
        authoring->relative_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    written = snprintf(
        out_path,
        out_path_capacity,
        "%s.hstate",
        authoring->relative_path);
    if (written < 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return (size_t)written >= out_path_capacity
        ? HENKA_ERROR_LIMIT
        : HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_create(
    henka_scene* scene,
    const char* relative_path,
    sandbox3d_game_authoring** out_authoring)
{
    sandbox3d_game_authoring* authoring;
    int written;
    henka_result result;
    if (out_authoring == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_authoring = NULL;
    if (scene == NULL || relative_path == NULL || relative_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    authoring = (sandbox3d_game_authoring*)henka_calloc(1U, sizeof(*authoring));
    if (authoring == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    written = snprintf(authoring->relative_path, sizeof(authoring->relative_path), "%s", relative_path);
    if (written < 0 || (size_t)written >= sizeof(authoring->relative_path))
    {
        henka_free(authoring);
        return HENKA_ERROR_LIMIT;
    }
    authoring->scene = scene;
    (void)snprintf(
        authoring->project_root,
        sizeof(authoring->project_root),
        "%s",
        ".");
    result = henka_scene_document_create(&authoring->document);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_create(
            authoring->document,
            scene,
            &authoring->bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_physics_world_create(&authoring->play_world);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_script_state_store_create(&authoring->script_state_store);
    }
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_play_session_destroy(authoring->play_session);
        henka_physics_world_destroy(authoring->play_world);
        henka_script_state_store_destroy(authoring->script_state_store);
        sandbox3d_scene_document_bridge_destroy(authoring->bridge);
        henka_scene_document_destroy(authoring->document);
        henka_free(authoring);
        return result;
    }
    *out_authoring = authoring;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_create_with_engine(
    henka_scene* scene,
    const char* relative_path,
    henka_engine* engine,
    sandbox3d_game_authoring** out_authoring)
{
    henka_asset_manager* assets;
    henka_result result;

    if (out_authoring != NULL)
    {
        *out_authoring = NULL;
    }
    if (engine == NULL || out_authoring == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    assets = henka_engine_get_asset_manager(engine);
    if (assets == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_game_authoring_create(
        scene,
        relative_path,
        out_authoring);
    if (result == HENKA_SUCCESS)
    {
        (*out_authoring)->project_engine = engine;
        (*out_authoring)->project_assets = assets;
        result = sandbox3d_scene_document_bridge_set_asset_manager(
            (*out_authoring)->bridge,
            assets);
    }
    return result;
}

static bool sandbox3d_game_authoring_path_has_suffix(
    const char* path,
    const char* suffix)
{
    size_t path_length;
    size_t suffix_length;
    size_t index;

    if (path == NULL || suffix == NULL)
    {
        return false;
    }
    path_length = strlen(path);
    suffix_length = strlen(suffix);
    if (suffix_length > path_length)
    {
        return false;
    }
    path += path_length - suffix_length;
    for (index = 0U; index < suffix_length; ++index)
    {
        if (tolower((unsigned char)path[index]) !=
            tolower((unsigned char)suffix[index]))
        {
            return false;
        }
    }
    return true;
}

static henka_result sandbox3d_game_authoring_materialize_texture_overrides(
    henka_asset_manager* assets,
    const henka_scene_document_renderer* renderer,
    henka_material* in_out_material)
{
    const uint32_t texture_override_bits[] = {
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_BASE_COLOR,
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_NORMAL,
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_METALLIC_ROUGHNESS,
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_OCCLUSION,
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_EMISSIVE,
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_TRANSMISSION,
        HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_THICKNESS};
    const char* texture_paths[] = {
        renderer->base_color_texture_path,
        renderer->normal_texture_path,
        renderer->metallic_roughness_texture_path,
        renderer->occlusion_texture_path,
        renderer->emissive_texture_path,
        renderer->transmission_texture_path,
        renderer->thickness_texture_path};
    henka_texture** texture_slots[] = {
        &in_out_material->base_color_texture,
        &in_out_material->normal_texture,
        &in_out_material->metallic_roughness_texture,
        &in_out_material->occlusion_texture,
        &in_out_material->emissive_texture,
        &in_out_material->transmission_texture,
        &in_out_material->thickness_texture};
    size_t texture_index;

    if (assets == NULL || renderer == NULL || in_out_material == NULL ||
        (renderer->texture_override_mask &
            ~HENKA_SCENE_DOCUMENT_TEXTURE_OVERRIDE_KNOWN_MASK) != 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (texture_index = 0U;
         texture_index < sizeof(texture_override_bits) /
             sizeof(texture_override_bits[0]);
         ++texture_index)
    {
        henka_texture* texture = NULL;
        henka_result result;
        if ((renderer->texture_override_mask &
                texture_override_bits[texture_index]) == 0U)
        {
            continue;
        }
        if (texture_paths[texture_index][0] != '\0')
        {
            result = henka_assets_load_texture(
                assets,
                texture_paths[texture_index],
                &texture);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
        }
        *texture_slots[texture_index] = texture;
        if (texture_index == 0U)
        {
            in_out_material->use_texture = texture != NULL;
        }
    }
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_game_authoring_materialize_material(
    henka_asset_manager* assets,
    henka_scene* scene,
    henka_entity entity,
    const henka_scene_document_object* object)
{
    const henka_material_asset* material_asset = NULL;
    henka_material material;
    size_t refreshed_count = 0U;
    henka_result result;

    if (scene == NULL || object == NULL ||
        !henka_scene_is_entity_valid(scene, entity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (object->renderer.material_path[0] == '\0')
    {
        return HENKA_SUCCESS;
    }
    if (assets == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_assets_get_material_asset_for_path(
        assets,
        object->renderer.material_path,
        &material_asset);
    if (result != HENKA_SUCCESS || material_asset == NULL)
    {
        return result == HENKA_SUCCESS ? HENKA_ERROR_UNKNOWN : result;
    }
    if (object->renderer.material_override)
    {
        if (henka_assets_get_material_asset_material(
                material_asset,
                &material) != HENKA_SUCCESS ||
            sandbox3d_scene_document_bridge_overlay_material(
                &object->renderer,
                &material) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        result = sandbox3d_game_authoring_materialize_texture_overrides(
            assets,
            &object->renderer,
            &material);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        return henka_scene_apply_material_asset_override(
            scene,
            entity,
            material_asset,
            material);
    }
    result = henka_scene_set_entity_material_asset(
        scene,
        entity,
        material_asset);
    if (result == HENKA_SUCCESS)
    {
        result = henka_assets_refresh_scene_material_bindings(
            assets,
            scene,
            &refreshed_count);
    }
    return result;
}

static henka_result sandbox3d_game_authoring_materialize_source(
    const char* project_root,
    henka_engine* engine,
    henka_asset_manager* assets,
    henka_scene* scene,
    henka_entity entity,
    const henka_scene_document_object* object,
    henka_mesh** out_owned_mesh)
{
    henka_authoring_mesh* authoring_mesh = NULL;
    henka_mesh* mesh = NULL;
    henka_result result;

    if (out_owned_mesh != NULL)
    {
        *out_owned_mesh = NULL;
    }
    if (project_root == NULL || project_root[0] == '\0' ||
        scene == NULL || object == NULL ||
        !henka_scene_is_entity_valid(scene, entity))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (object->source.kind == HENKA_SCENE_DOCUMENT_SOURCE_NONE)
    {
        return sandbox3d_game_authoring_materialize_material(
            assets,
            scene,
            entity,
            object);
    }
    if (object->source.kind == HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE)
    {
        const henka_authoring_mesh_desc description =
            henka_authoring_mesh_desc_default();

        if (engine == NULL)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        switch (object->source.primitive)
        {
            case HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX:
                result = henka_authoring_mesh_create_box(
                    &description,
                    object->source.primitive_dimensions.x,
                    object->source.primitive_dimensions.y,
                    object->source.primitive_dimensions.z,
                    &authoring_mesh);
                break;
            case HENKA_SCENE_DOCUMENT_PRIMITIVE_SPHERE:
                if (object->source.primitive_dimensions.x !=
                        object->source.primitive_dimensions.y ||
                    object->source.primitive_dimensions.x !=
                        object->source.primitive_dimensions.z)
                {
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
                result = henka_authoring_mesh_create_uv_sphere(
                    &description,
                    object->source.primitive_dimensions.x,
                    32U,
                    16U,
                    &authoring_mesh);
                break;
            case HENKA_SCENE_DOCUMENT_PRIMITIVE_PLANE:
                result = henka_authoring_mesh_create_plane(
                    &description,
                    object->source.primitive_dimensions.x,
                    object->source.primitive_dimensions.z,
                    &authoring_mesh);
                break;
            default:
                return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_mesh_create_from_authoring_mesh(
                engine,
                authoring_mesh,
                &mesh);
        }
        henka_authoring_mesh_destroy(authoring_mesh);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        result = henka_scene_set_entity_mesh(scene, entity, mesh);
        if (result != HENKA_SUCCESS)
        {
            henka_mesh_destroy(mesh);
            return result;
        }
        result = sandbox3d_game_authoring_materialize_material(
            assets,
            scene,
            entity,
            object);
        if (result != HENKA_SUCCESS)
        {
            henka_mesh_destroy(mesh);
            return result;
        }
        if (out_owned_mesh != NULL)
        {
            *out_owned_mesh = mesh;
        }
        return HENKA_SUCCESS;
    }
    if (object->source.kind == HENKA_SCENE_DOCUMENT_SOURCE_AUTHORING_MESH)
    {
        char* resolved_path = NULL;

        if (engine == NULL || object->source.path[0] == '\0')
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        result = henka_assets_resolve_path(
            project_root,
            object->source.path,
            &resolved_path);
        if (result == HENKA_SUCCESS)
        {
            result = henka_authoring_mesh_load_file_new(
                resolved_path,
                &authoring_mesh);
        }
        henka_free(resolved_path);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        result = henka_mesh_create_from_authoring_mesh(
            engine,
            authoring_mesh,
            &mesh);
        henka_authoring_mesh_destroy(authoring_mesh);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        result = henka_scene_set_entity_mesh(scene, entity, mesh);
        if (result != HENKA_SUCCESS)
        {
            henka_mesh_destroy(mesh);
            return result;
        }
        result = sandbox3d_game_authoring_materialize_material(
            assets,
            scene,
            entity,
            object);
        if (result != HENKA_SUCCESS)
        {
            henka_mesh_destroy(mesh);
            return result;
        }
        if (out_owned_mesh != NULL)
        {
            *out_owned_mesh = mesh;
        }
        return HENKA_SUCCESS;
    }
    if (assets == NULL ||
        object->source.kind != HENKA_SCENE_DOCUMENT_SOURCE_ASSET ||
        object->source.asset_kind != HENKA_SCENE_DOCUMENT_ASSET_MESH ||
        object->source.path[0] == '\0')
    {
        /* Do not expose an entity whose persisted source was silently lost. */
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (sandbox3d_game_authoring_path_has_suffix(object->source.path, ".obj"))
    {
        result = henka_assets_load_obj_mesh(assets, object->source.path, &mesh);
    }
    else if (sandbox3d_game_authoring_path_has_suffix(object->source.path, ".gltf") ||
             sandbox3d_game_authoring_path_has_suffix(object->source.path, ".glb"))
    {
        result = henka_assets_load_gltf_mesh(assets, object->source.path, &mesh);
    }
    else
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_entity_mesh(scene, entity, mesh);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_game_authoring_materialize_material(
            assets,
            scene,
            entity,
            object);
    }
    return result;
}

static bool sandbox3d_game_authoring_prefab_object_matches(
    const henka_scene_document_object* object,
    henka_scene_document_id instance_root_id,
    const char* asset_path,
    uint64_t source_revision)
{
    return object != NULL && asset_path != NULL &&
        object->source.kind == HENKA_SCENE_DOCUMENT_SOURCE_ASSET &&
        object->source.asset_kind == HENKA_SCENE_DOCUMENT_ASSET_PREFAB &&
        object->source.prefab_instance_root_id == instance_root_id &&
        object->source.prefab_source_id !=
            HENKA_INVALID_SCENE_DOCUMENT_PREFAB_SOURCE_ID &&
        object->source.prefab_source_revision == source_revision &&
        strcmp(object->source.path, asset_path) == 0;
}

static henka_result sandbox3d_game_authoring_find_prefab_instance(
    const sandbox3d_game_authoring* authoring,
    const henka_scene_document* document,
    const henka_scene_document_object* object,
    henka_prefab_instance** out_instance,
    henka_entity* out_entity)
{
    henka_scene_document_object root_object;
    size_t prefab_index;
    size_t binding_index;
    henka_entity entity = HENKA_INVALID_ENTITY;

    if (authoring == NULL || document == NULL || object == NULL ||
        out_instance == NULL || out_entity == NULL ||
        object->source.kind != HENKA_SCENE_DOCUMENT_SOURCE_ASSET ||
        object->source.asset_kind != HENKA_SCENE_DOCUMENT_ASSET_PREFAB ||
        object->source.prefab_instance_root_id ==
            HENKA_INVALID_SCENE_DOCUMENT_ID ||
        object->source.prefab_source_id ==
            HENKA_INVALID_SCENE_DOCUMENT_PREFAB_SOURCE_ID ||
        object->source.path[0] == '\0' ||
        object->source.prefab_source_revision == 0U ||
        henka_scene_document_get_object(
            document,
            object->source.prefab_instance_root_id,
            &root_object) != HENKA_SUCCESS ||
        root_object.source.kind != HENKA_SCENE_DOCUMENT_SOURCE_ASSET ||
        root_object.source.asset_kind != HENKA_SCENE_DOCUMENT_ASSET_PREFAB ||
        root_object.source.prefab_instance_root_id != root_object.id ||
        root_object.source.prefab_source_id ==
            HENKA_INVALID_SCENE_DOCUMENT_PREFAB_SOURCE_ID ||
        root_object.source.prefab_source_revision !=
            object->source.prefab_source_revision ||
        strcmp(root_object.source.path, object->source.path) != 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (prefab_index = 0U;
         prefab_index < authoring->project_prefab_count;
         ++prefab_index)
    {
        henka_prefab* prefab = authoring->project_prefabs[prefab_index];
        henka_prefab_instance* instance =
            authoring->project_prefab_instances[prefab_index];
        const char* asset_path = prefab == NULL
            ? NULL
            : henka_prefab_get_asset_path(prefab);

        if (prefab == NULL || instance == NULL || asset_path == NULL ||
            strcmp(asset_path, root_object.source.path) != 0 ||
            henka_prefab_get_revision(prefab) !=
                root_object.source.prefab_source_revision ||
            henka_prefab_instance_get_prefab_revision(instance) !=
                root_object.source.prefab_source_revision ||
            henka_prefab_instance_get_entity_for_source_id(
                instance,
                (henka_prefab_source_id)root_object.source.prefab_source_id,
                &entity) != HENKA_SUCCESS)
        {
            continue;
        }

        for (binding_index = 0U;
             binding_index < authoring->binding_count;
             ++binding_index)
        {
            if (authoring->bindings[binding_index].document_id ==
                    object->source.prefab_instance_root_id &&
                authoring->bindings[binding_index].entity == entity)
            {
                if (henka_prefab_instance_get_entity_for_source_id(
                        instance,
                        (henka_prefab_source_id)object->source.prefab_source_id,
                        &entity) != HENKA_SUCCESS)
                {
                    return HENKA_ERROR_INVALID_ARGUMENT;
                }
                for (size_t object_binding_index = 0U;
                     object_binding_index < authoring->binding_count;
                     ++object_binding_index)
                {
                    if (authoring->bindings[object_binding_index].document_id ==
                            object->id &&
                        authoring->bindings[object_binding_index].entity == entity)
                    {
                        *out_instance = instance;
                        *out_entity = entity;
                        return HENKA_SUCCESS;
                    }
                }
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

static henka_result sandbox3d_game_authoring_sync_prefab_transform_override(
    const sandbox3d_game_authoring* authoring,
    henka_scene_document* document,
    henka_scene_document_id document_id)
{
    henka_scene_document_object object;
    henka_prefab_instance* instance = NULL;
    henka_entity entity = HENKA_INVALID_ENTITY;
    bool has_override = false;
    henka_transform transform;
    henka_result result;

    if (authoring == NULL || document == NULL ||
        document_id == HENKA_INVALID_SCENE_DOCUMENT_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_scene_document_get_object(document, document_id, &object);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (object.source.kind != HENKA_SCENE_DOCUMENT_SOURCE_ASSET ||
        object.source.asset_kind != HENKA_SCENE_DOCUMENT_ASSET_PREFAB)
    {
        return HENKA_SUCCESS;
    }
    result = sandbox3d_game_authoring_find_prefab_instance(
        authoring, document, &object, &instance, &entity);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_prefab_instance_get_local_transform_override(
        instance,
        (henka_prefab_source_id)object.source.prefab_source_id,
        &has_override,
        &transform);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    (void)entity;
    object.source.prefab_local_transform_override = has_override;
    object.source.prefab_local_transform = has_override
        ? transform
        : henka_transform_identity();
    return henka_scene_document_set_object(document, &object);
}

static henka_result sandbox3d_game_authoring_apply_prefab_transform_overrides(
    const sandbox3d_game_authoring* authoring,
    const henka_scene_document* document,
    henka_scene* scene)
{
    size_t index;
    size_t document_count;

    if (authoring == NULL || document == NULL || scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    document_count = henka_scene_document_get_object_count(document);
    for (index = 0U; index < document_count; ++index)
    {
        henka_scene_document_object object;
        henka_prefab_instance* instance = NULL;
        henka_entity entity = HENKA_INVALID_ENTITY;
        henka_result result;

        result = henka_scene_document_get_object_at(document, index, &object);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (object.source.kind != HENKA_SCENE_DOCUMENT_SOURCE_ASSET ||
            object.source.asset_kind != HENKA_SCENE_DOCUMENT_ASSET_PREFAB)
        {
            continue;
        }
        result = sandbox3d_game_authoring_find_prefab_instance(
            authoring, document, &object, &instance, &entity);
        if (result != HENKA_SUCCESS || instance == NULL)
        {
            return result == HENKA_SUCCESS
                ? HENKA_ERROR_INVALID_ARGUMENT
                : result;
        }
        if (object.source.prefab_local_transform_override)
        {
            result = henka_scene_set_entity_local_transform(
                scene,
                entity,
                object.source.prefab_local_transform);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
        }
    }
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_game_authoring_materialize_prefab(
    const char* project_root,
    henka_asset_manager* assets,
    henka_scene* scene,
    sandbox3d_game_authoring* authoring,
    const henka_scene_document_object* root_object)
{
    henka_prefab* prefab = NULL;
    henka_prefab_instance* instance = NULL;
    bool prefab_manager_owned = false;
    henka_scene_document_object object;
    size_t document_count;
    const char* prefab_path;
    size_t index;
    size_t member_count = 0U;
    henka_entity root_entity = HENKA_INVALID_ENTITY;
    henka_entity instance_root_entity = HENKA_INVALID_ENTITY;
    henka_result result;

    if (project_root == NULL || project_root[0] == '\0' || assets == NULL ||
        scene == NULL || authoring == NULL || authoring->document == NULL ||
        root_object == NULL ||
        root_object->source.kind != HENKA_SCENE_DOCUMENT_SOURCE_ASSET ||
        root_object->source.asset_kind != HENKA_SCENE_DOCUMENT_ASSET_PREFAB ||
        root_object->source.prefab_instance_root_id != root_object->id ||
        root_object->source.prefab_source_id ==
            HENKA_INVALID_SCENE_DOCUMENT_PREFAB_SOURCE_ID ||
        root_object->source.prefab_source_revision == 0U ||
        root_object->source.path[0] == '\0' ||
        authoring->project_prefab_count >=
            SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    document_count = henka_scene_document_get_object_count(authoring->document);

    result = henka_assets_load_prefab_asset(
        assets,
        root_object->source.path,
        NULL,
        &prefab);
    if (result != HENKA_SUCCESS || prefab == NULL)
    {
        return result == HENKA_SUCCESS ? HENKA_ERROR_ASSET_SOURCE : result;
    }
    prefab_manager_owned = true;
    prefab_path = henka_prefab_get_asset_path(prefab);
    if (prefab_path == NULL || strcmp(prefab_path, root_object->source.path) != 0 ||
        henka_prefab_get_revision(prefab) !=
            root_object->source.prefab_source_revision)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    for (index = 0U; index < document_count; ++index)
    {
        if (henka_scene_document_get_object_at(
                authoring->document, index, &object) != HENKA_SUCCESS)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        if (object.source.prefab_instance_root_id == root_object->id)
        {
            if (!sandbox3d_game_authoring_prefab_object_matches(
                    &object,
                    root_object->id,
                    root_object->source.path,
                    root_object->source.prefab_source_revision))
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            member_count += 1U;
        }
    }
    if (member_count != henka_prefab_get_entity_count(prefab))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    for (index = 0U; index < henka_prefab_get_entity_count(prefab); ++index)
    {
        henka_prefab_source_id source_id = HENKA_INVALID_PREFAB_SOURCE_ID;
        size_t matching_members = 0U;
        size_t document_index;

        result = henka_prefab_get_source_id_at(prefab, index, &source_id);
        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        for (document_index = 0U;
             document_index < document_count;
             ++document_index)
        {
            if (henka_scene_document_get_object_at(
                    authoring->document, document_index, &object) !=
                    HENKA_SUCCESS)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            if (object.source.prefab_instance_root_id == root_object->id &&
                object.source.prefab_source_id == source_id)
            {
                matching_members += 1U;
            }
        }
        if (matching_members != 1U)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }
    result = henka_prefab_instantiate_with_instance(
        prefab,
        scene,
        root_object->transform,
        &instance);
    if (result != HENKA_SUCCESS || instance == NULL)
    {
        goto cleanup;
    }
    result = henka_prefab_instance_get_entity_for_source_id(
        instance,
        (henka_prefab_source_id)root_object->source.prefab_source_id,
        &root_entity);
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_instance_get_root_entity(
            instance, &instance_root_entity);
    }
    if (result != HENKA_SUCCESS || root_entity != instance_root_entity)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    for (index = 0U; index < document_count; ++index)
    {
        henka_entity entity = HENKA_INVALID_ENTITY;

        if (henka_scene_document_get_object_at(
                authoring->document, index, &object) != HENKA_SUCCESS)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        if (object.source.prefab_instance_root_id != root_object->id)
        {
            continue;
        }
        result = henka_prefab_instance_get_entity_for_source_id(
            instance,
            (henka_prefab_source_id)object.source.prefab_source_id,
            &entity);
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_scene_document_bridge_bind(
                authoring->bridge, object.id, entity);
        }
        if (result != HENKA_SUCCESS ||
            authoring->binding_count >= SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
        {
            result = result == HENKA_SUCCESS ? HENKA_ERROR_LIMIT : result;
            goto cleanup;
        }
        authoring->bindings[authoring->binding_count++] =
            (sandbox3d_game_authoring_binding){object.id, entity};
    }
    authoring->project_prefabs[authoring->project_prefab_count] = prefab;
    authoring->project_prefab_instances[
        authoring->project_prefab_count] = instance;
    authoring->project_prefab_manager_owned[
        authoring->project_prefab_count] = prefab_manager_owned;
    authoring->project_prefab_count += 1U;
    prefab = NULL;
    instance = NULL;
    return HENKA_SUCCESS;

cleanup:
    henka_prefab_instance_destroy(instance);
    if (prefab != NULL && !prefab_manager_owned)
    {
        henka_prefab_destroy(prefab);
    }
    return result;
}

static henka_result sandbox3d_game_authoring_validate_existing_prefab(
    const char* project_root,
    const sandbox3d_game_authoring* authoring,
    const henka_scene_document* candidate,
    const henka_scene_document_object* root_object)
{
    henka_prefab* loaded_prefab = NULL;
    const henka_prefab* retained_prefab = NULL;
    const henka_prefab_instance* retained_instance = NULL;
    size_t prefab_index;
    size_t document_count;
    size_t member_count = 0U;
    henka_entity root_entity = HENKA_INVALID_ENTITY;
    henka_result result;

    if (project_root == NULL || authoring == NULL || candidate == NULL ||
        root_object == NULL || authoring->project_assets == NULL ||
        root_object->id == HENKA_INVALID_SCENE_DOCUMENT_ID ||
        root_object->source.kind != HENKA_SCENE_DOCUMENT_SOURCE_ASSET ||
        root_object->source.asset_kind != HENKA_SCENE_DOCUMENT_ASSET_PREFAB ||
        root_object->source.prefab_instance_root_id != root_object->id ||
        root_object->source.prefab_source_id ==
            HENKA_INVALID_SCENE_DOCUMENT_PREFAB_SOURCE_ID ||
        root_object->source.prefab_source_revision == 0U ||
        root_object->source.path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_prefab_load_file(
        authoring->project_assets,
        NULL,
        project_root,
        root_object->source.path,
        &loaded_prefab);
    if (result != HENKA_SUCCESS || loaded_prefab == NULL)
    {
        return result == HENKA_SUCCESS ? HENKA_ERROR_ASSET_SOURCE : result;
    }
    if (henka_prefab_get_revision(loaded_prefab) !=
            root_object->source.prefab_source_revision ||
        henka_prefab_get_entity_count(loaded_prefab) == 0U ||
        henka_prefab_get_asset_path(loaded_prefab) == NULL ||
        strcmp(
            henka_prefab_get_asset_path(loaded_prefab),
            root_object->source.path) != 0)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }

    for (prefab_index = 0U;
         prefab_index < authoring->project_prefab_count;
         ++prefab_index)
    {
        const henka_prefab* prefab =
            authoring->project_prefabs[prefab_index];
        const henka_prefab_instance* instance =
            authoring->project_prefab_instances[prefab_index];
        size_t binding_index;

        if (prefab == NULL || instance == NULL ||
            henka_prefab_get_asset_path(prefab) == NULL ||
            strcmp(
                henka_prefab_get_asset_path(prefab),
                root_object->source.path) != 0 ||
            henka_prefab_get_revision(prefab) !=
                root_object->source.prefab_source_revision ||
            henka_prefab_instance_get_prefab_revision(instance) !=
                root_object->source.prefab_source_revision ||
            henka_prefab_instance_get_entity_for_source_id(
                instance,
                (henka_prefab_source_id)root_object->source.prefab_source_id,
                &root_entity) != HENKA_SUCCESS)
        {
            continue;
        }
        for (binding_index = 0U;
             binding_index < authoring->binding_count;
             ++binding_index)
        {
            if (authoring->bindings[binding_index].document_id ==
                    root_object->id &&
                authoring->bindings[binding_index].entity == root_entity)
            {
                retained_prefab = prefab;
                retained_instance = instance;
                break;
            }
        }
        if (retained_instance != NULL)
        {
            break;
        }
    }
    if (retained_prefab == NULL || retained_instance == NULL ||
        henka_prefab_get_entity_count(retained_prefab) !=
            henka_prefab_get_entity_count(loaded_prefab))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    for (prefab_index = 0U;
         prefab_index < henka_prefab_get_entity_count(retained_prefab);
         ++prefab_index)
    {
        henka_prefab_source_id retained_source_id =
            HENKA_INVALID_PREFAB_SOURCE_ID;
        henka_prefab_source_id loaded_source_id =
            HENKA_INVALID_PREFAB_SOURCE_ID;

        if (henka_prefab_get_source_id_at(
                retained_prefab, prefab_index, &retained_source_id) !=
                HENKA_SUCCESS ||
            henka_prefab_get_source_id_at(
                loaded_prefab, prefab_index, &loaded_source_id) !=
                HENKA_SUCCESS ||
            retained_source_id != loaded_source_id)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }

    document_count = henka_scene_document_get_object_count(candidate);
    for (prefab_index = 0U; prefab_index < document_count; ++prefab_index)
    {
        henka_scene_document_object object;

        if (henka_scene_document_get_object_at(
                candidate, prefab_index, &object) != HENKA_SUCCESS)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        if (object.source.prefab_instance_root_id != root_object->id)
        {
            continue;
        }
        if (!sandbox3d_game_authoring_prefab_object_matches(
                &object,
                root_object->id,
                root_object->source.path,
                root_object->source.prefab_source_revision))
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        member_count += 1U;
    }
    if (member_count != henka_prefab_get_entity_count(retained_prefab))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }

    for (prefab_index = 0U;
         prefab_index < henka_prefab_get_entity_count(retained_prefab);
         ++prefab_index)
    {
        henka_prefab_source_id source_id = HENKA_INVALID_PREFAB_SOURCE_ID;
        size_t matching_members = 0U;
        size_t document_index;

        result = henka_prefab_get_source_id_at(
            retained_prefab, prefab_index, &source_id);
        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        for (document_index = 0U;
             document_index < document_count;
             ++document_index)
        {
            henka_scene_document_object object;

            if (henka_scene_document_get_object_at(
                    candidate, document_index, &object) != HENKA_SUCCESS)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            if (object.source.prefab_instance_root_id == root_object->id &&
                object.source.prefab_source_id == source_id)
            {
                henka_entity entity = HENKA_INVALID_ENTITY;

                if (henka_prefab_instance_get_entity_for_source_id(
                        retained_instance, source_id, &entity) != HENKA_SUCCESS)
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                    goto cleanup;
                }
                matching_members += 1U;
            }
        }
        if (matching_members != 1U)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }
    result = HENKA_SUCCESS;

cleanup:
    henka_prefab_destroy(loaded_prefab);
    return result;
}

static bool sandbox3d_game_authoring_mesh_in_list(
    henka_mesh* const* meshes,
    size_t mesh_count,
    const henka_mesh* target)
{
    size_t index;
    if (meshes == NULL || target == NULL)
    {
        return false;
    }
    for (index = 0U; index < mesh_count; ++index)
    {
        if (meshes[index] == target)
        {
            return true;
        }
    }
    return false;
}

static bool sandbox3d_game_authoring_scene_references_mesh(
    const henka_scene* scene,
    const sandbox3d_game_authoring* authoring,
    const henka_mesh* target)
{
    size_t index;
    if (scene == NULL || authoring == NULL || target == NULL)
    {
        return false;
    }
    for (index = 0U; index < authoring->binding_count; ++index)
    {
        henka_mesh* mesh = NULL;
        if (henka_scene_get_entity_mesh(
                scene,
                authoring->bindings[index].entity,
                &mesh) == HENKA_SUCCESS &&
            mesh == target)
        {
            return true;
        }
    }
    return false;
}

static void sandbox3d_game_authoring_destroy_mesh_list(
    henka_mesh** meshes,
    size_t mesh_count)
{
    while (mesh_count > 0U)
    {
        --mesh_count;
        henka_mesh_destroy(meshes[mesh_count]);
        meshes[mesh_count] = NULL;
    }
}

static henka_result sandbox3d_game_authoring_open_project_internal(
    const char* project_root,
    henka_engine* engine,
    henka_asset_manager* assets,
    henka_scene** out_scene,
    sandbox3d_game_authoring** out_authoring)
{
    const char* bootstrap_relative_path = "project.hscene";
    henka_scene* candidate_scene = NULL;
    sandbox3d_game_authoring* candidate_authoring = NULL;
    char selected_relative_path[
        SANDBOX3D_GAME_AUTHORING_MAX_RELATIVE_PATH_BYTES];
    henka_scene_document_object object;
    henka_result result;
    size_t index;

    if (out_scene != NULL)
    {
        *out_scene = NULL;
    }
    if (out_authoring != NULL)
    {
        *out_authoring = NULL;
    }
    if (project_root == NULL || project_root[0] == '\0' ||
        strlen(project_root) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES ||
        out_scene == NULL || out_authoring == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* Open always starts from fresh runtime state. The bootstrap path is
     * private to this constructor and is never used when a valid manifest is
     * present; an absent manifest is rejected instead of falling back to a
     * path held by another authoring session. */
    result = henka_scene_create(&candidate_scene);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_game_authoring_create(
        candidate_scene,
        bootstrap_relative_path,
        &candidate_authoring);
    if (result != HENKA_SUCCESS)
    {
        henka_scene_destroy(candidate_scene);
        return result;
    }
    result = sandbox3d_game_authoring_get_startup_scene_path(
        candidate_authoring,
        project_root,
        selected_relative_path,
        sizeof(selected_relative_path),
        false);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_game_authoring_set_project_root(
            candidate_authoring,
            project_root);
    }
    if (result == HENKA_SUCCESS)
    {
        candidate_authoring->project_engine = engine;
        candidate_authoring->project_assets = assets;
        result = sandbox3d_scene_document_bridge_set_asset_manager(
            candidate_authoring->bridge,
            assets);
    }
    if (result == HENKA_SUCCESS)
    {
        if (snprintf(
                candidate_authoring->relative_path,
                sizeof(candidate_authoring->relative_path),
                "%s",
                selected_relative_path) < 0 ||
            strlen(selected_relative_path) >=
                sizeof(candidate_authoring->relative_path))
        {
            result = HENKA_ERROR_LIMIT;
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_load_file(
            candidate_authoring->document,
            project_root,
            selected_relative_path);
    }
    if (result == HENKA_SUCCESS)
    {
        for (index = 0U;
            index < henka_scene_document_get_object_count(
                candidate_authoring->document);
            ++index)
        {
            henka_entity entity;
            henka_result bind_result;
            if (henka_scene_document_get_object_at(
                    candidate_authoring->document,
                    index,
                    &object) != HENKA_SUCCESS ||
                object.id == HENKA_INVALID_SCENE_DOCUMENT_ID)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                break;
            }
            if (object.source.kind == HENKA_SCENE_DOCUMENT_SOURCE_ASSET &&
                object.source.asset_kind == HENKA_SCENE_DOCUMENT_ASSET_PREFAB)
            {
                if (object.source.prefab_instance_root_id == object.id)
                {
                    result = sandbox3d_game_authoring_materialize_prefab(
                        project_root,
                        assets,
                        candidate_scene,
                        candidate_authoring,
                        &object);
                    if (result != HENKA_SUCCESS)
                    {
                        break;
                    }
                }
                continue;
            }
            if (candidate_authoring->binding_count >=
                SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
            {
                result = HENKA_ERROR_LIMIT;
                break;
            }
            entity = henka_scene_create_entity_named(
                candidate_scene,
                object.name);
            if (entity == HENKA_INVALID_ENTITY)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
                break;
            }
            {
                henka_mesh* owned_mesh = NULL;
                result = sandbox3d_game_authoring_materialize_source(
                    project_root,
                    engine,
                    assets,
                    candidate_scene,
                    entity,
                    &object,
                    &owned_mesh);
                if (result != HENKA_SUCCESS)
                {
                    break;
                }
                if (owned_mesh != NULL)
                {
                    if (candidate_authoring->owned_project_mesh_count >=
                        SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
                    {
                        henka_mesh_destroy(owned_mesh);
                        result = HENKA_ERROR_LIMIT;
                        break;
                    }
                    candidate_authoring->owned_project_meshes[
                        candidate_authoring->owned_project_mesh_count++] = owned_mesh;
                }
            }
            bind_result = sandbox3d_scene_document_bridge_bind(
                candidate_authoring->bridge,
                object.id,
                entity);
            if (bind_result != HENKA_SUCCESS)
            {
                result = bind_result;
                break;
            }
            candidate_authoring->bindings[
                candidate_authoring->binding_count++] =
                (sandbox3d_game_authoring_binding){object.id, entity};
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_validate(
            candidate_authoring->bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_objects(
            candidate_authoring->bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_hierarchy(
            candidate_authoring->bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_camera(
            candidate_authoring->bridge);
    }
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_game_authoring_destroy(candidate_authoring);
        henka_scene_destroy(candidate_scene);
        return result;
    }

    *out_scene = candidate_scene;
    *out_authoring = candidate_authoring;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_open_project(
    const char* project_root,
    henka_scene** out_scene,
    sandbox3d_game_authoring** out_authoring)
{
    return sandbox3d_game_authoring_open_project_internal(
        project_root,
        NULL,
        NULL,
        out_scene,
        out_authoring);
}

henka_result sandbox3d_game_authoring_open_project_with_assets(
    const char* project_root,
    henka_asset_manager* assets,
    henka_scene** out_scene,
    sandbox3d_game_authoring** out_authoring)
{
    if (assets == NULL)
    {
        if (out_scene != NULL) *out_scene = NULL;
        if (out_authoring != NULL) *out_authoring = NULL;
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_game_authoring_open_project_internal(
        project_root,
        NULL,
        assets,
        out_scene,
        out_authoring);
}

henka_result sandbox3d_game_authoring_open_project_with_engine(
    const char* project_root,
    henka_engine* engine,
    henka_scene** out_scene,
    sandbox3d_game_authoring** out_authoring)
{
    henka_asset_manager* assets;

    if (engine == NULL)
    {
        if (out_scene != NULL) *out_scene = NULL;
        if (out_authoring != NULL) *out_authoring = NULL;
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    assets = henka_engine_get_asset_manager(engine);
    if (assets == NULL)
    {
        if (out_scene != NULL) *out_scene = NULL;
        if (out_authoring != NULL) *out_authoring = NULL;
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_game_authoring_open_project_internal(
        project_root,
        engine,
        assets,
        out_scene,
        out_authoring);
}

void sandbox3d_game_authoring_destroy(
    sandbox3d_game_authoring* authoring)
{
    if (authoring == NULL)
    {
        return;
    }
    if (authoring->play_session != NULL)
    {
        (void)sandbox3d_play_session_stop(authoring->play_session);
    }
    sandbox3d_play_session_destroy(authoring->play_session);
    sandbox3d_scene_document_bridge_destroy(authoring->play_bridge);
    henka_scene_destroy(authoring->play_scene);
    henka_physics_world_destroy(authoring->play_world);
    henka_script_state_store_destroy(authoring->play_script_state_store);
    henka_script_state_store_destroy(authoring->script_state_store);
    sandbox3d_scene_document_bridge_destroy(authoring->bridge);
    henka_scene_document_destroy(authoring->document);
    while (authoring->project_prefab_count > 0U)
    {
        --authoring->project_prefab_count;
        henka_prefab_instance_destroy(
            authoring->project_prefab_instances[
                authoring->project_prefab_count]);
        if (!authoring->project_prefab_manager_owned[
                authoring->project_prefab_count])
        {
            henka_prefab_destroy(
                authoring->project_prefabs[authoring->project_prefab_count]);
        }
        authoring->project_prefab_instances[
            authoring->project_prefab_count] = NULL;
        authoring->project_prefabs[
            authoring->project_prefab_count] = NULL;
        authoring->project_prefab_manager_owned[
            authoring->project_prefab_count] = false;
    }
    while (authoring->owned_project_mesh_count > 0U)
    {
        --authoring->owned_project_mesh_count;
        henka_mesh_destroy(
            authoring->owned_project_meshes[
                authoring->owned_project_mesh_count]);
        authoring->owned_project_meshes[
            authoring->owned_project_mesh_count] = NULL;
    }
    henka_free(authoring);
}

static henka_result sandbox3d_game_authoring_register_entity_once(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_id* out_document_id)
{
    henka_scene_document_object object;
    henka_scene_document_id document_id;
    henka_entity parent_entity = HENKA_INVALID_ENTITY;
    size_t parent_index;
    henka_result result;

    if (authoring == NULL || out_document_id == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (authoring->binding_count >= SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
    {
        return HENKA_ERROR_LIMIT;
    }
    if (sandbox3d_game_authoring_find_binding(authoring, entity) != SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_game_authoring_build_object(
        authoring, entity, &object);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (henka_scene_get_entity_parent(
            authoring->scene, entity, &parent_entity) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (parent_entity != HENKA_INVALID_ENTITY)
    {
        parent_index = sandbox3d_game_authoring_find_binding(
            authoring, parent_entity);
        if (parent_index == SIZE_MAX)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        object.parent_id = authoring->bindings[parent_index].document_id;
    }
    result = henka_scene_document_add_object(authoring->document, &object, &document_id);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_scene_document_bridge_bind(authoring->bridge, document_id, entity);
    if (result != HENKA_SUCCESS)
    {
        (void)henka_scene_document_remove_object(authoring->document, document_id);
        return result;
    }
    authoring->bindings[authoring->binding_count++] =
        (sandbox3d_game_authoring_binding){document_id, entity};
    *out_document_id = document_id;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_register_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_id* out_document_id)
{
    henka_entity chain[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    henka_entity current_entity;
    henka_scene_document_object target_object;
    henka_scene_document_id document_id;
    henka_result result;
    size_t chain_count = 0U;
    size_t initial_binding_count;
    size_t index;

    if (out_document_id != NULL)
    {
        *out_document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    }
    if (authoring == NULL || out_document_id == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* Validate the target before honoring an existing binding so stale scene
     * entities cannot be mistaken for an idempotent registration. */
    result = sandbox3d_game_authoring_build_object(
        authoring, entity, &target_object);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    index = sandbox3d_game_authoring_find_binding(authoring, entity);
    if (index != SIZE_MAX)
    {
        *out_document_id = authoring->bindings[index].document_id;
        return HENKA_SUCCESS;
    }

    /* Preflight the complete live parent chain before mutating the document
     * or bridge.  This makes registration independent of scene iteration
     * order and keeps failures before the first visible authoring mutation. */
    current_entity = entity;
    while (current_entity != HENKA_INVALID_ENTITY &&
           sandbox3d_game_authoring_find_binding(authoring, current_entity) == SIZE_MAX)
    {
        henka_scene_document_object preflight_object;
        henka_entity parent_entity = HENKA_INVALID_ENTITY;

        if (chain_count >= SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
        {
            return HENKA_ERROR_LIMIT;
        }
        for (index = 0U; index < chain_count; ++index)
        {
            if (chain[index] == current_entity)
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
        result = sandbox3d_game_authoring_build_object(
            authoring, current_entity, &preflight_object);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (henka_scene_get_entity_parent(
                authoring->scene, current_entity, &parent_entity) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        chain[chain_count++] = current_entity;
        current_entity = parent_entity;
    }
    if (chain_count > SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS - authoring->binding_count)
    {
        return HENKA_ERROR_LIMIT;
    }

    initial_binding_count = authoring->binding_count;
    for (index = chain_count; index > 0U; --index)
    {
        result = sandbox3d_game_authoring_register_entity_once(
            authoring, chain[index - 1U], &document_id);
        if (result != HENKA_SUCCESS)
        {
            while (authoring->binding_count > initial_binding_count)
            {
                henka_entity rollback_entity =
                    authoring->bindings[authoring->binding_count - 1U].entity;
                if (sandbox3d_game_authoring_unregister_entity(
                        authoring, rollback_entity) != HENKA_SUCCESS)
                {
                    return HENKA_ERROR_UNKNOWN;
                }
            }
            return result;
        }
    }

    index = sandbox3d_game_authoring_find_binding(authoring, entity);
    if (index == SIZE_MAX)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    if (chain_count > 0U)
    {
        sandbox3d_game_authoring_clear_history(authoring);
    }
    *out_document_id = authoring->bindings[index].document_id;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_register_duplicate_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity source_entity,
    henka_entity duplicate_entity,
    henka_scene_document_id* out_document_id)
{
    henka_scene_document_id source_document_id =
        HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_object source_object;

    if (out_document_id != NULL)
    {
        *out_document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    }
    if (authoring == NULL || out_document_id == NULL ||
        source_entity == HENKA_INVALID_ENTITY ||
        duplicate_entity == HENKA_INVALID_ENTITY ||
        source_entity == duplicate_entity ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        sandbox3d_game_authoring_find_binding(authoring, duplicate_entity) !=
            SIZE_MAX ||
        sandbox3d_game_authoring_build_object(
            authoring, source_entity, &source_object) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring,
            source_entity,
            &source_document_id,
            &source_object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (source_document_id == HENKA_INVALID_SCENE_DOCUMENT_ID ||
        source_object.source.prefab_instance_root_id !=
            HENKA_INVALID_SCENE_DOCUMENT_ID ||
        source_object.source.prefab_source_id !=
            HENKA_INVALID_SCENE_DOCUMENT_PREFAB_SOURCE_ID ||
        source_object.source.prefab_source_revision != 0U ||
        source_object.source.prefab_local_transform_override)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_game_authoring_register_entity(
        authoring, duplicate_entity, out_document_id);
}

henka_result sandbox3d_game_authoring_create_prefab_asset(
    sandbox3d_game_authoring* authoring,
    henka_entity root_entity,
    const char* project_root,
    const char* relative_path)
{
    henka_scene_document_id document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_object object;
    henka_prefab* prefab = NULL;
    henka_result result;

    if (authoring == NULL || authoring->scene == NULL ||
        root_entity == HENKA_INVALID_ENTITY || project_root == NULL ||
        project_root[0] == '\0' || relative_path == NULL ||
        relative_path[0] == '\0' ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        !henka_scene_is_entity_valid(authoring->scene, root_entity) ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, root_entity, &document_id, &object) != HENKA_SUCCESS ||
        document_id == HENKA_INVALID_SCENE_DOCUMENT_ID ||
        object.source.prefab_instance_root_id !=
            HENKA_INVALID_SCENE_DOCUMENT_ID ||
        object.source.prefab_source_id !=
            HENKA_INVALID_SCENE_DOCUMENT_PREFAB_SOURCE_ID ||
        object.source.prefab_source_revision != 0U ||
        object.source.prefab_local_transform_override)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_prefab_create_from_scene(
        authoring->scene, root_entity, &prefab);
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_set_asset_path(prefab, relative_path);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_prefab_save_file(
            prefab,
            authoring->project_assets,
            project_root,
            relative_path);
    }
    henka_prefab_destroy(prefab);
    return result;
}

static henka_result sandbox3d_game_authoring_instantiate_prefab_asset_internal(
    sandbox3d_game_authoring* authoring,
    const char* asset_path,
    henka_entity placement_parent_entity,
    henka_transform root_transform,
    henka_entity* out_root_entity)
{
    henka_prefab* prefab = NULL;
    henka_prefab_instance* instance = NULL;
    henka_entity entities[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    henka_scene_document_id document_ids[
        SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    size_t initial_binding_count;
    size_t entity_count;
    size_t index;
    size_t root_index = SIZE_MAX;
    henka_entity root_entity = HENKA_INVALID_ENTITY;
    henka_scene_document_id placement_parent_document_id =
        HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_object placement_parent_object;
    henka_scene_document_id root_document_id =
        HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_result result;

    if (out_root_entity != NULL)
    {
        *out_root_entity = HENKA_INVALID_ENTITY;
    }
    if (authoring == NULL || authoring->scene == NULL ||
        authoring->document == NULL || authoring->bridge == NULL ||
        authoring->project_assets == NULL || asset_path == NULL ||
        asset_path[0] == '\0' || out_root_entity == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        authoring->project_prefab_count >=
            SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (placement_parent_entity != HENKA_INVALID_ENTITY &&
        (henka_scene_is_entity_valid(
             authoring->scene, placement_parent_entity) == false ||
         sandbox3d_game_authoring_get_object_for_entity(
             authoring,
             placement_parent_entity,
             &placement_parent_document_id,
             &placement_parent_object) != HENKA_SUCCESS ||
         placement_parent_document_id == HENKA_INVALID_SCENE_DOCUMENT_ID ||
          (placement_parent_object.source.kind ==
               HENKA_SCENE_DOCUMENT_SOURCE_ASSET &&
              placement_parent_object.source.asset_kind ==
               HENKA_SCENE_DOCUMENT_ASSET_PREFAB)))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_assets_load_prefab_asset(
        authoring->project_assets, asset_path, NULL, &prefab);
    if (result != HENKA_SUCCESS || prefab == NULL)
    {
        return result == HENKA_SUCCESS ? HENKA_ERROR_ASSET_SOURCE : result;
    }
    entity_count = henka_prefab_get_entity_count(prefab);
    initial_binding_count = authoring->binding_count;
    if (entity_count == 0U ||
        entity_count > SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS ||
        initial_binding_count >
            SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS - entity_count)
    {
        return HENKA_ERROR_LIMIT;
    }

    if (placement_parent_entity == HENKA_INVALID_ENTITY)
    {
        result = henka_prefab_instantiate_with_instance(
            prefab, authoring->scene, root_transform, &instance);
    }
    else
    {
        result = henka_prefab_instantiate_under_parent_with_instance(
            prefab,
            authoring->scene,
            placement_parent_entity,
            root_transform,
            &instance);
    }
    if (result != HENKA_SUCCESS || instance == NULL)
    {
        return result == HENKA_SUCCESS ? HENKA_ERROR_UNKNOWN : result;
    }
    result = henka_prefab_instance_get_root_entity(instance, &root_entity);
    if (result != HENKA_SUCCESS || root_entity == HENKA_INVALID_ENTITY)
    {
        goto rollback;
    }

    for (index = 0U; index < entity_count; ++index)
    {
        henka_prefab_source_id source_id = HENKA_INVALID_PREFAB_SOURCE_ID;

        if (henka_prefab_get_source_id_at(prefab, index, &source_id) !=
                HENKA_SUCCESS ||
            source_id == HENKA_INVALID_PREFAB_SOURCE_ID ||
            henka_prefab_instance_get_entity_for_source_id(
                instance, source_id, &entities[index]) != HENKA_SUCCESS)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto rollback;
        }
        if (sandbox3d_game_authoring_register_entity(
                authoring, entities[index], &document_ids[index]) !=
                HENKA_SUCCESS)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto rollback;
        }
        if (entities[index] == root_entity)
        {
            root_index = index;
            root_document_id = document_ids[index];
        }
    }
    if (root_index == SIZE_MAX ||
        root_document_id == HENKA_INVALID_SCENE_DOCUMENT_ID)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto rollback;
    }

    for (index = 0U; index < entity_count; ++index)
    {
        henka_scene_document_object object;
        henka_entity parent_entity = HENKA_INVALID_ENTITY;
        size_t parent_index;
        henka_prefab_source_id source_id = HENKA_INVALID_PREFAB_SOURCE_ID;
        int path_written;

        if (sandbox3d_game_authoring_get_object_for_entity(
                authoring,
                entities[index],
                &document_ids[index],
                &object) != HENKA_SUCCESS ||
            henka_prefab_get_source_id_at(prefab, index, &source_id) !=
                HENKA_SUCCESS ||
            source_id == HENKA_INVALID_PREFAB_SOURCE_ID ||
            henka_scene_get_entity_parent(
                authoring->scene, entities[index], &parent_entity) !=
                HENKA_SUCCESS)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto rollback;
        }

        object.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_ASSET;
        object.source.asset_kind = HENKA_SCENE_DOCUMENT_ASSET_PREFAB;
        path_written = snprintf(
            object.source.path,
            sizeof(object.source.path),
            "%s",
            asset_path);
        if (path_written < 0 ||
            (size_t)path_written >= sizeof(object.source.path))
        {
            result = HENKA_ERROR_LIMIT;
            goto rollback;
        }
        object.source.prefab_instance_root_id = root_document_id;
        object.source.prefab_source_id = source_id;
        object.source.prefab_source_revision =
            henka_prefab_get_revision(prefab);
        object.source.prefab_local_transform_override = false;
        object.source.prefab_local_transform = henka_transform_identity();
        if (parent_entity == HENKA_INVALID_ENTITY)
        {
            object.parent_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
        }
        else if (parent_entity == placement_parent_entity)
        {
            object.parent_id = placement_parent_document_id;
        }
        else
        {
            for (parent_index = 0U; parent_index < entity_count; ++parent_index)
            {
                if (entities[parent_index] == parent_entity)
                {
                    object.parent_id = document_ids[parent_index];
                    break;
                }
            }
            if (parent_index == entity_count)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto rollback;
            }
        }
        result = henka_scene_document_set_object(authoring->document, &object);
        if (result != HENKA_SUCCESS)
        {
            goto rollback;
        }
    }

    authoring->project_prefabs[authoring->project_prefab_count] = prefab;
    authoring->project_prefab_instances[
        authoring->project_prefab_count] = instance;
    authoring->project_prefab_manager_owned[
        authoring->project_prefab_count] = true;
    authoring->project_prefab_count += 1U;
    *out_root_entity = root_entity;
    return HENKA_SUCCESS;

rollback:
    while (authoring->binding_count > initial_binding_count)
    {
        const sandbox3d_game_authoring_binding binding =
            authoring->bindings[authoring->binding_count - 1U];
        (void)sandbox3d_scene_document_bridge_unbind(
            authoring->bridge, binding.document_id);
        (void)henka_scene_document_remove_object(
            authoring->document, binding.document_id);
        authoring->binding_count -= 1U;
    }
    (void)henka_prefab_instance_destroy_entities(instance);
    henka_prefab_instance_destroy(instance);
    return result;
}

henka_result sandbox3d_game_authoring_instantiate_prefab_asset(
    sandbox3d_game_authoring* authoring,
    const char* asset_path,
    henka_transform root_transform,
    henka_entity* out_root_entity)
{
    return sandbox3d_game_authoring_instantiate_prefab_asset_internal(
        authoring,
        asset_path,
        HENKA_INVALID_ENTITY,
        root_transform,
        out_root_entity);
}

henka_result sandbox3d_game_authoring_instantiate_prefab_asset_under_parent(
    sandbox3d_game_authoring* authoring,
    const char* asset_path,
    henka_entity parent_entity,
    henka_transform root_transform,
    henka_entity* out_root_entity)
{
    if (parent_entity == HENKA_INVALID_ENTITY)
    {
        if (out_root_entity != NULL)
        {
            *out_root_entity = HENKA_INVALID_ENTITY;
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_game_authoring_instantiate_prefab_asset_internal(
        authoring,
        asset_path,
        parent_entity,
        root_transform,
        out_root_entity);
}

static bool sandbox3d_game_authoring_entity_is_prefab_member(
    const henka_entity* entities,
    size_t entity_count,
    henka_entity entity)
{
    size_t index;

    if (entities == NULL || entity == HENKA_INVALID_ENTITY)
    {
        return false;
    }
    for (index = 0U; index < entity_count; ++index)
    {
        if (entities[index] == entity)
        {
            return true;
        }
    }
    return false;
}

static bool sandbox3d_game_authoring_document_id_is_prefab_member(
    const henka_scene_document_id* document_ids,
    size_t document_count,
    henka_scene_document_id document_id)
{
    size_t index;

    if (document_ids == NULL ||
        document_id == HENKA_INVALID_SCENE_DOCUMENT_ID)
    {
        return false;
    }
    for (index = 0U; index < document_count; ++index)
    {
        if (document_ids[index] == document_id)
        {
            return true;
        }
    }
    return false;
}

static void sandbox3d_game_authoring_remove_binding_at(
    sandbox3d_game_authoring* authoring,
    size_t index)
{
    if (authoring == NULL || index >= authoring->binding_count)
    {
        return;
    }
    if (index + 1U < authoring->binding_count)
    {
        memmove(
            &authoring->bindings[index],
            &authoring->bindings[index + 1U],
            (authoring->binding_count - index - 1U) *
                sizeof(authoring->bindings[0]));
    }
    --authoring->binding_count;
    authoring->bindings[authoring->binding_count] =
        (sandbox3d_game_authoring_binding){
            HENKA_INVALID_SCENE_DOCUMENT_ID,
            HENKA_INVALID_ENTITY};
}

henka_result sandbox3d_game_authoring_destroy_prefab_instance(
    sandbox3d_game_authoring* authoring,
    henka_entity instance_entity)
{
    henka_scene_document_id instance_document_id =
        HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene_document_object instance_object;
    henka_scene_document_object root_object;
    henka_prefab_instance* instance = NULL;
    henka_entity resolved_entity = HENKA_INVALID_ENTITY;
    henka_entity entities[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    henka_scene_document_id document_ids[
        SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    henka_scene_document_id parent_ids[
        SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    bool removed[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS] = {false};
    size_t entity_count;
    size_t document_count;
    size_t index;
    size_t remaining;
    henka_result result;

    if (authoring == NULL || authoring->scene == NULL ||
        authoring->document == NULL || authoring->bridge == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        instance_entity == HENKA_INVALID_ENTITY ||
        !henka_scene_is_entity_valid(authoring->scene, instance_entity) ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring,
            instance_entity,
            &instance_document_id,
            &instance_object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = sandbox3d_game_authoring_find_prefab_instance(
        authoring,
        authoring->document,
        &instance_object,
        &instance,
        &resolved_entity);
    if (result != HENKA_SUCCESS || instance == NULL ||
        resolved_entity != instance_entity ||
        instance_document_id == HENKA_INVALID_SCENE_DOCUMENT_ID ||
        instance_object.source.prefab_instance_root_id ==
            HENKA_INVALID_SCENE_DOCUMENT_ID ||
        henka_scene_document_get_object(
            authoring->document,
            instance_object.source.prefab_instance_root_id,
            &root_object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    entity_count = henka_prefab_instance_get_entity_count(instance);
    if (entity_count == 0U ||
        entity_count > SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (root_object.id != instance_object.source.prefab_instance_root_id ||
        root_object.source.kind != HENKA_SCENE_DOCUMENT_SOURCE_ASSET ||
        root_object.source.asset_kind != HENKA_SCENE_DOCUMENT_ASSET_PREFAB ||
        root_object.source.prefab_instance_root_id != root_object.id ||
        root_object.source.prefab_source_id ==
            HENKA_INVALID_SCENE_DOCUMENT_PREFAB_SOURCE_ID ||
        root_object.source.prefab_source_revision !=
            instance_object.source.prefab_source_revision ||
        strcmp(root_object.source.path, instance_object.source.path) != 0)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    for (index = 0U; index < entity_count; ++index)
    {
        henka_scene_document_object object;
        henka_entity bound_entity = HENKA_INVALID_ENTITY;

        if (henka_prefab_instance_get_entity_at(
                instance, index, &entities[index]) != HENKA_SUCCESS ||
            sandbox3d_game_authoring_get_object_for_entity(
                authoring,
                entities[index],
                &document_ids[index],
                &object) != HENKA_SUCCESS ||
            document_ids[index] == HENKA_INVALID_SCENE_DOCUMENT_ID ||
            object.source.kind != HENKA_SCENE_DOCUMENT_SOURCE_ASSET ||
            object.source.asset_kind != HENKA_SCENE_DOCUMENT_ASSET_PREFAB ||
            object.source.prefab_instance_root_id !=
                root_object.id ||
            object.source.prefab_source_id ==
                HENKA_INVALID_SCENE_DOCUMENT_PREFAB_SOURCE_ID ||
            object.source.prefab_source_revision !=
                root_object.source.prefab_source_revision ||
            strcmp(object.source.path, root_object.source.path) != 0 ||
            henka_scene_document_get_object(
                authoring->document,
                document_ids[index],
                &object) != HENKA_SUCCESS ||
            sandbox3d_scene_document_bridge_get_entity(
                authoring->bridge,
                document_ids[index],
                &bound_entity) != HENKA_SUCCESS ||
            bound_entity != entities[index])
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        parent_ids[index] = object.parent_id;
    }

    for (index = 0U; index < entity_count; ++index)
    {
        size_t child_count;
        size_t child_index;

        if (henka_scene_get_entity_child_count(
                authoring->scene,
                entities[index],
                &child_count) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (child_index = 0U; child_index < child_count; ++child_index)
        {
            henka_entity child_entity = HENKA_INVALID_ENTITY;
            if (henka_scene_get_entity_child_at_index(
                    authoring->scene,
                    entities[index],
                    child_index,
                    &child_entity) != HENKA_SUCCESS ||
                !sandbox3d_game_authoring_entity_is_prefab_member(
                    entities,
                    entity_count,
                    child_entity))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }

    document_count = henka_scene_document_get_object_count(authoring->document);
    for (index = 0U; index < document_count; ++index)
    {
        henka_scene_document_object object;
        size_t member_index;

        if (henka_scene_document_get_object_at(
                authoring->document, index, &object) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        for (member_index = 0U; member_index < entity_count; ++member_index)
        {
            if (object.parent_id == document_ids[member_index] &&
                !sandbox3d_game_authoring_document_id_is_prefab_member(
                    document_ids,
                    entity_count,
                    object.id))
            {
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
        }
    }

    if (!henka_scene_has_render_revision_capacity(
            authoring->scene, (uint64_t)entity_count))
    {
        return HENKA_ERROR_LIMIT;
    }
    result = henka_prefab_instance_destroy_entities(instance);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    remaining = entity_count;
    while (remaining > 0U)
    {
        bool removed_one = false;
        for (index = 0U; index < entity_count; ++index)
        {
            size_t child_index;
            bool has_remaining_child = false;
            size_t binding_index;

            if (removed[index])
            {
                continue;
            }
            for (child_index = 0U; child_index < entity_count; ++child_index)
            {
                if (!removed[child_index] &&
                    parent_ids[child_index] == document_ids[index])
                {
                    has_remaining_child = true;
                    break;
                }
            }
            if (has_remaining_child)
            {
                continue;
            }
            if (sandbox3d_scene_document_bridge_unbind(
                    authoring->bridge, document_ids[index]) != HENKA_SUCCESS ||
                henka_scene_document_remove_object(
                    authoring->document, document_ids[index]) != HENKA_SUCCESS ||
                (binding_index = sandbox3d_game_authoring_find_binding(
                     authoring, entities[index])) == SIZE_MAX)
            {
                return HENKA_ERROR_UNKNOWN;
            }
            sandbox3d_game_authoring_remove_binding_at(authoring, binding_index);
            removed[index] = true;
            --remaining;
            removed_one = true;
            break;
        }
        if (!removed_one)
        {
            return HENKA_ERROR_UNKNOWN;
        }
    }
    sandbox3d_game_authoring_clear_history(authoring);
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_game_authoring_prepare_unpacked_source(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_object* object)
{
    henka_scene_document_object defaults;
    henka_asset_metadata metadata;
    henka_mesh* mesh = NULL;
    int written;

    if (authoring == NULL || authoring->scene == NULL || object == NULL ||
        entity == HENKA_INVALID_ENTITY ||
        !henka_scene_is_entity_valid(authoring->scene, entity) ||
        henka_scene_get_entity_mesh(authoring->scene, entity, &mesh) !=
            HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    defaults = henka_scene_document_object_default();
    object->source = defaults.source;
    if (mesh == NULL)
    {
        return HENKA_SUCCESS;
    }

    memset(&metadata, 0, sizeof(metadata));
    if (authoring->project_assets == NULL ||
        henka_assets_get_mesh_metadata(
            authoring->project_assets, mesh, &metadata) != HENKA_SUCCESS ||
        !metadata.loaded || metadata.fallback ||
        metadata.source_path == NULL || metadata.source_path[0] == '\0' ||
        strlen(metadata.source_path) >= sizeof(object->source.path))
    {
        /* Unpack must not turn a borrowed Prefab mesh into an unreconstructible
         * ordinary object. Unsupported native/anonymous mesh ownership stays
         * Prefab-backed until an explicit durable source authority exists. */
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    object->source.kind = HENKA_SCENE_DOCUMENT_SOURCE_ASSET;
    object->source.asset_kind = HENKA_SCENE_DOCUMENT_ASSET_MESH;
    written = snprintf(
        object->source.path,
        sizeof(object->source.path),
        "%s",
        metadata.source_path);
    if (written < 0 || (size_t)written >= sizeof(object->source.path))
    {
        return HENKA_ERROR_LIMIT;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_unpack_prefab_instance(
    sandbox3d_game_authoring* authoring,
    henka_entity instance_entity)
{
    henka_scene_document* candidate = NULL;
    sandbox3d_scene_document_bridge* candidate_bridge = NULL;
    henka_scene_document_object instance_object;
    henka_prefab_instance* instance = NULL;
    henka_entity resolved_entity = HENKA_INVALID_ENTITY;
    henka_entity entities[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    henka_scene_document_id document_ids[
        SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    henka_scene_document_object* unpacked_objects = NULL;
    henka_scene_document_id root_document_id =
        HENKA_INVALID_SCENE_DOCUMENT_ID;
    size_t entity_count;
    size_t root_index = SIZE_MAX;
    size_t prefab_index = SIZE_MAX;
    size_t binding_index;
    size_t index;
    henka_result result;

    if (authoring == NULL || authoring->scene == NULL ||
        authoring->document == NULL || authoring->bridge == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        instance_entity == HENKA_INVALID_ENTITY ||
        !henka_scene_is_entity_valid(authoring->scene, instance_entity) ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring,
            instance_entity,
            &document_ids[0],
            &instance_object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = sandbox3d_game_authoring_find_prefab_instance(
        authoring,
        authoring->document,
        &instance_object,
        &instance,
        &resolved_entity);
    if (result != HENKA_SUCCESS || instance == NULL ||
        resolved_entity != instance_entity ||
        instance_object.source.prefab_instance_root_id ==
            HENKA_INVALID_SCENE_DOCUMENT_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    root_document_id = instance_object.source.prefab_instance_root_id;

    for (index = 0U; index < authoring->project_prefab_count; ++index)
    {
        if (authoring->project_prefab_instances[index] == instance)
        {
            prefab_index = index;
            break;
        }
    }
    if (prefab_index == SIZE_MAX ||
        !authoring->project_prefab_manager_owned[prefab_index])
    {
        /* Durable Game Authoring unpack is intentionally limited to the
         * manager-owned asset-backed Prefab path. */
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    entity_count = henka_prefab_instance_get_entity_count(instance);
    if (entity_count == 0U ||
        entity_count > SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    unpacked_objects = (henka_scene_document_object*)henka_calloc(
        entity_count, sizeof(*unpacked_objects));
    if (unpacked_objects == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }

    result = henka_scene_document_create(&candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_copy(candidate, authoring->document);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_create(
            candidate,
            authoring->scene,
            &candidate_bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_set_asset_manager(
            candidate_bridge,
            authoring->project_assets);
    }
    for (binding_index = 0U;
         binding_index < authoring->binding_count && result == HENKA_SUCCESS;
         ++binding_index)
    {
        result = sandbox3d_scene_document_bridge_bind(
            candidate_bridge,
            authoring->bindings[binding_index].document_id,
            authoring->bindings[binding_index].entity);
    }
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    for (index = 0U; index < entity_count; ++index)
    {
        henka_scene_document_object current_object = {0};
        henka_transform local_transform = henka_transform_identity();

        result = henka_prefab_instance_get_entity_at(
            instance, index, &entities[index]);
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_game_authoring_get_object_for_entity(
                authoring,
                entities[index],
                &document_ids[index],
                &current_object);
        }
        if (result != HENKA_SUCCESS ||
            current_object.source.kind != HENKA_SCENE_DOCUMENT_SOURCE_ASSET ||
            current_object.source.asset_kind !=
                HENKA_SCENE_DOCUMENT_ASSET_PREFAB ||
            current_object.source.prefab_instance_root_id != root_document_id)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        if (document_ids[index] == root_document_id)
        {
            if (root_index != SIZE_MAX)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
                goto cleanup;
            }
            root_index = index;
        }

        result = sandbox3d_scene_document_bridge_sync_object(
            candidate_bridge,
            document_ids[index]);
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_document_get_object(
                candidate,
                document_ids[index],
                &unpacked_objects[index]);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_get_entity_local_transform(
                authoring->scene,
                entities[index],
                &local_transform);
        }
        if (result == HENKA_SUCCESS)
        {
            unpacked_objects[index].transform = local_transform;
            result = sandbox3d_game_authoring_prepare_unpacked_source(
                authoring,
                entities[index],
                &unpacked_objects[index]);
        }
        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (root_index == SIZE_MAX)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }

    sandbox3d_scene_document_bridge_destroy(candidate_bridge);
    candidate_bridge = NULL;

    /* Convert descendants first so every intermediate candidate remains a
     * valid document: the Prefab root remains available until the last write. */
    for (index = 0U; index < entity_count; ++index)
    {
        if (index == root_index)
        {
            continue;
        }
        result = henka_scene_document_set_object(
            candidate, &unpacked_objects[index]);
        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    result = henka_scene_document_set_object(
        candidate, &unpacked_objects[root_index]);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    result = henka_scene_document_swap_contents(authoring->document, candidate);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    result = henka_prefab_instance_detach(
        &authoring->project_prefab_instances[prefab_index]);
    if (result != HENKA_SUCCESS)
    {
        /* Swap is allocation-free; candidate owns the old document after the
         * first swap, so restore it if the guaranteed mapping release fails. */
        if (henka_scene_document_swap_contents(
                authoring->document, candidate) != HENKA_SUCCESS)
        {
            result = HENKA_ERROR_UNKNOWN;
        }
        goto cleanup;
    }

    if (prefab_index + 1U < authoring->project_prefab_count)
    {
        memmove(
            &authoring->project_prefabs[prefab_index],
            &authoring->project_prefabs[prefab_index + 1U],
            (authoring->project_prefab_count - prefab_index - 1U) *
                sizeof(authoring->project_prefabs[0]));
        memmove(
            &authoring->project_prefab_instances[prefab_index],
            &authoring->project_prefab_instances[prefab_index + 1U],
            (authoring->project_prefab_count - prefab_index - 1U) *
                sizeof(authoring->project_prefab_instances[0]));
        memmove(
            &authoring->project_prefab_manager_owned[prefab_index],
            &authoring->project_prefab_manager_owned[prefab_index + 1U],
            (authoring->project_prefab_count - prefab_index - 1U) *
                sizeof(authoring->project_prefab_manager_owned[0]));
    }
    --authoring->project_prefab_count;
    authoring->project_prefabs[authoring->project_prefab_count] = NULL;
    authoring->project_prefab_instances[authoring->project_prefab_count] = NULL;
    authoring->project_prefab_manager_owned[
        authoring->project_prefab_count] = false;

    sandbox3d_game_authoring_clear_history(authoring);
    henka_scene_document_destroy(candidate);
    henka_free(unpacked_objects);
    return HENKA_SUCCESS;

cleanup:
    sandbox3d_scene_document_bridge_destroy(candidate_bridge);
    henka_scene_document_destroy(candidate);
    henka_free(unpacked_objects);
    return result;
}
henka_result sandbox3d_game_authoring_unregister_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity)
{
    henka_scene_document_id document_id;
    size_t document_index;
    size_t child_detach_count = 0U;
    size_t index;
    if (authoring == NULL || sandbox3d_game_authoring_is_play_locked(authoring) ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* Scene destruction promotes direct children to roots.  Keep the authored
     * hierarchy in the same state before retiring the parent's binding, so a
     * later save or validation cannot retain a parent document ID that no
     * longer exists.  Preflight the live detach mutations before publishing
     * any document change. */
    document_id = authoring->bindings[index].document_id;
    for (document_index = 0U;
         document_index < henka_scene_document_get_object_count(authoring->document);
         ++document_index)
    {
        henka_scene_document_object child_object;
        henka_entity child_entity = HENKA_INVALID_ENTITY;
        henka_entity child_parent = HENKA_INVALID_ENTITY;

        if (henka_scene_document_get_object_at(
                authoring->document, document_index, &child_object) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (child_object.parent_id != document_id)
        {
            continue;
        }
        if (sandbox3d_game_authoring_get_entity_for_document_id(
                authoring, child_object.id, &child_entity) != HENKA_SUCCESS ||
            henka_scene_get_entity_parent(
                authoring->scene, child_entity, &child_parent) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (child_parent != HENKA_INVALID_ENTITY)
        {
            ++child_detach_count;
        }
    }
    if (child_detach_count > 0U &&
        !henka_scene_has_render_revision_capacity(
            authoring->scene, (uint64_t)child_detach_count))
    {
        return HENKA_ERROR_LIMIT;
    }
    for (document_index = 0U;
         document_index < henka_scene_document_get_object_count(authoring->document);
         ++document_index)
    {
        henka_scene_document_object child_object;
        henka_entity child_entity = HENKA_INVALID_ENTITY;
        henka_entity child_parent = HENKA_INVALID_ENTITY;

        if (henka_scene_document_get_object_at(
                authoring->document, document_index, &child_object) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (child_object.parent_id != document_id)
        {
            continue;
        }
        if (
            sandbox3d_game_authoring_get_entity_for_document_id(
                authoring, child_object.id, &child_entity) != HENKA_SUCCESS ||
            henka_scene_get_entity_parent(
                authoring->scene, child_entity, &child_parent) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (child_parent != HENKA_INVALID_ENTITY &&
            henka_scene_set_entity_parent(
                authoring->scene,
                child_entity,
                HENKA_INVALID_ENTITY,
                HENKA_SCENE_PARENT_KEEP_WORLD) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        child_object.parent_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
        if (henka_scene_document_set_object(authoring->document, &child_object) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_UNKNOWN;
        }
    }
    if (sandbox3d_scene_document_bridge_unbind(
            authoring->bridge,
            document_id) != HENKA_SUCCESS ||
        henka_scene_document_remove_object(
            authoring->document,
            document_id) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (index + 1U < authoring->binding_count)
    {
        memmove(
            &authoring->bindings[index],
            &authoring->bindings[index + 1U],
            (authoring->binding_count - index - 1U) * sizeof(authoring->bindings[0]));
    }
    --authoring->binding_count;
    authoring->bindings[authoring->binding_count] =
        (sandbox3d_game_authoring_binding){
            HENKA_INVALID_SCENE_DOCUMENT_ID,
            HENKA_INVALID_ENTITY};
    sandbox3d_game_authoring_clear_history(authoring);
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_get_object_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_id* out_document_id,
    henka_scene_document_object* out_object)
{
    size_t index;
    if (out_document_id != NULL)
    {
        *out_document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    }
    if (authoring == NULL || out_document_id == NULL || out_object == NULL ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_scene_document_get_object(
            authoring->document,
            authoring->bindings[index].document_id,
            out_object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_document_id = authoring->bindings[index].document_id;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_get_entity_for_document_id(
    const sandbox3d_game_authoring* authoring,
    henka_scene_document_id document_id,
    henka_entity* out_entity)
{
    if (out_entity != NULL)
    {
        *out_entity = HENKA_INVALID_ENTITY;
    }
    if (authoring == NULL || authoring->bridge == NULL ||
        document_id == HENKA_INVALID_SCENE_DOCUMENT_ID || out_entity == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_scene_document_bridge_get_entity(
        authoring->bridge,
        document_id,
        out_entity);
}

henka_result sandbox3d_game_authoring_update_object_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    const henka_scene_document_object* object)
{
    henka_scene_document_object previous;
    henka_scene* candidate_scene = NULL;
    sandbox3d_scene_document_bridge* candidate_bridge = NULL;
    henka_scene_document_id document_id;
    size_t binding_index;
    size_t binding_count;
    henka_scene_document_id binding_document_id;
    henka_entity binding_entity;
    henka_result rollback_result;
    henka_result result;
    if (authoring == NULL || object == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &document_id, &previous) != HENKA_SUCCESS ||
        object->id != document_id)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_scene_clone(authoring->scene, &candidate_scene);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_scene_document_bridge_create(
        authoring->document,
        candidate_scene,
        &candidate_bridge);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_set_asset_manager(
            candidate_bridge,
            authoring->project_assets);
    }
    if (result == HENKA_SUCCESS)
    {
        binding_count = sandbox3d_scene_document_bridge_get_binding_count(
            authoring->bridge);
        for (binding_index = 0U;
            binding_index < binding_count && result == HENKA_SUCCESS;
            ++binding_index)
        {
            result = sandbox3d_scene_document_bridge_get_binding_at(
                authoring->bridge,
                binding_index,
                &binding_document_id,
                &binding_entity);
            if (result == HENKA_SUCCESS)
            {
                result = sandbox3d_scene_document_bridge_bind(
                    candidate_bridge,
                    binding_document_id,
                    binding_entity);
            }
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_object_candidate(
            candidate_bridge,
            document_id,
            object);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_hierarchy_candidate(
            candidate_bridge,
            document_id,
            object);
    }
    sandbox3d_scene_document_bridge_destroy(candidate_bridge);
    henka_scene_destroy(candidate_scene);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_scene_document_set_object(authoring->document, object);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_scene_document_bridge_apply_object(authoring->bridge, document_id);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_hierarchy(authoring->bridge);
    }
    if (result != HENKA_SUCCESS)
    {
        rollback_result = henka_scene_document_set_object(
            authoring->document,
            &previous);
        if (rollback_result == HENKA_SUCCESS)
        {
            rollback_result = sandbox3d_scene_document_bridge_apply_object(
                authoring->bridge,
                document_id);
        }
        if (rollback_result == HENKA_SUCCESS)
        {
            rollback_result = sandbox3d_scene_document_bridge_apply_hierarchy(
                authoring->bridge);
        }
        if (rollback_result != HENKA_SUCCESS)
        {
            return HENKA_ERROR_UNKNOWN;
        }
    }
    if (result == HENKA_SUCCESS && !authoring->history_replaying)
    {
        sandbox3d_game_authoring_append_history(
            authoring,
            entity,
            &previous,
            object);
    }
    return result;
}

static henka_result sandbox3d_game_authoring_set_parent_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity child,
    henka_entity parent,
    henka_scene_parenting_mode mode)
{
    henka_scene_document_object candidate_object;
    henka_scene_document_id child_document_id;
    henka_scene_document_object parent_object;
    henka_scene_document_id parent_document_id = HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_scene* candidate_scene = NULL;
    henka_result result;

    if (authoring == NULL || sandbox3d_game_authoring_is_play_locked(authoring) ||
        child == HENKA_INVALID_ENTITY ||
        (parent != HENKA_INVALID_ENTITY && parent == child) ||
        (mode != HENKA_SCENE_PARENT_KEEP_LOCAL &&
            mode != HENKA_SCENE_PARENT_KEEP_WORLD) ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring,
            child,
            &child_document_id,
            &candidate_object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (parent != HENKA_INVALID_ENTITY &&
        sandbox3d_game_authoring_get_object_for_entity(
            authoring,
            parent,
            &parent_document_id,
            &parent_object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    /* Resolve the requested parenting semantics through the production scene
     * operation before changing the authored document. The clone preserves
     * generation-checked handles and the same bounded transform contract. */
    result = henka_scene_clone(authoring->scene, &candidate_scene);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_scene_set_entity_parent(
        candidate_scene,
        child,
        parent,
        mode);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_get_entity_world_transform(
            candidate_scene,
            child,
            &candidate_object.transform);
    }
    henka_scene_destroy(candidate_scene);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    candidate_object.parent_id = parent == HENKA_INVALID_ENTITY
        ? HENKA_INVALID_SCENE_DOCUMENT_ID
        : parent_document_id;
    return sandbox3d_game_authoring_update_object_for_entity(
        authoring,
        child,
        &candidate_object);
}

henka_result sandbox3d_game_authoring_reparent_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity child,
    henka_entity parent,
    henka_scene_parenting_mode mode)
{
    if (parent == HENKA_INVALID_ENTITY)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_game_authoring_set_parent_entity(
        authoring,
        child,
        parent,
        mode);
}

henka_result sandbox3d_game_authoring_unparent_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity child,
    henka_scene_parenting_mode mode)
{
    return sandbox3d_game_authoring_set_parent_entity(
        authoring,
        child,
        HENKA_INVALID_ENTITY,
        mode);
}

static henka_result sandbox3d_game_authoring_sync_prefab_transform_override(
    const sandbox3d_game_authoring* authoring,
    henka_scene_document* document,
    henka_scene_document_id document_id);

static henka_result sandbox3d_game_authoring_find_prefab_instance(
    const sandbox3d_game_authoring* authoring,
    const henka_scene_document* document,
    const henka_scene_document_object* object,
    henka_prefab_instance** out_instance,
    henka_entity* out_entity);

static bool sandbox3d_game_authoring_is_prefab_document_object(
    const henka_scene_document_object* object)
{
    return object != NULL &&
        object->source.kind == HENKA_SCENE_DOCUMENT_SOURCE_ASSET &&
        object->source.asset_kind == HENKA_SCENE_DOCUMENT_ASSET_PREFAB &&
        object->source.prefab_instance_root_id !=
            HENKA_INVALID_SCENE_DOCUMENT_ID &&
        object->source.prefab_source_id !=
            HENKA_INVALID_SCENE_DOCUMENT_PREFAB_SOURCE_ID &&
        object->source.prefab_source_revision != 0U &&
        object->source.path[0] != '\0';
}

henka_result sandbox3d_game_authoring_update_prefab_asset_from_entity(
    sandbox3d_game_authoring* authoring,
    const char* project_root,
    const char* asset_path,
    henka_entity source_entity)
{
    henka_scene_document_object source_object;
    henka_scene_document_id source_document_id =
        HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_prefab* prefab = NULL;
    henka_prefab* candidate_prefab = NULL;
    henka_prefab* reloaded_prefab = NULL;
    henka_prefab_instance* instances[
        SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    henka_scene_document* candidate_document = NULL;
    sandbox3d_scene_document_bridge* candidate_bridge = NULL;
    size_t instance_count = 0U;
    size_t prefab_index;
    size_t binding_index;
    size_t old_entity_count;
    uint64_t old_revision;
    uint64_t next_revision;
    henka_result result;

    if (authoring == NULL || authoring->scene == NULL ||
        authoring->document == NULL || authoring->bridge == NULL ||
        authoring->project_assets == NULL || project_root == NULL ||
        project_root[0] == '\0' || asset_path == NULL ||
        asset_path[0] == '\0' || source_entity == HENKA_INVALID_ENTITY ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        !henka_scene_is_entity_valid(authoring->scene, source_entity) ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring,
            source_entity,
            &source_document_id,
            &source_object) != HENKA_SUCCESS ||
        sandbox3d_game_authoring_is_prefab_document_object(&source_object))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_assets_load_prefab_asset(
        authoring->project_assets,
        asset_path,
        NULL,
        &prefab);
    if (result != HENKA_SUCCESS || prefab == NULL ||
        henka_prefab_get_asset_path(prefab) == NULL ||
        strcmp(henka_prefab_get_asset_path(prefab), asset_path) != 0)
    {
        return result == HENKA_SUCCESS
            ? HENKA_ERROR_INVALID_ARGUMENT
            : result;
    }
    old_entity_count = henka_prefab_get_entity_count(prefab);
    old_revision = henka_prefab_get_revision(prefab);
    if (old_entity_count == 0U || old_revision == UINT64_MAX)
    {
        return HENKA_ERROR_LIMIT;
    }

    result = henka_prefab_load_file(
        authoring->project_assets,
        NULL,
        project_root,
        asset_path,
        &candidate_prefab);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_prefab_refresh_from_scene(
        candidate_prefab,
        authoring->scene,
        source_entity);
    if (result != HENKA_SUCCESS ||
        henka_prefab_get_entity_count(candidate_prefab) != old_entity_count)
    {
        result = result == HENKA_SUCCESS
            ? HENKA_ERROR_INVALID_ARGUMENT
            : result;
        goto cleanup;
    }
    for (prefab_index = 0U;
         prefab_index < old_entity_count;
         ++prefab_index)
    {
        henka_prefab_source_id old_source_id =
            HENKA_INVALID_PREFAB_SOURCE_ID;
        henka_prefab_source_id new_source_id =
            HENKA_INVALID_PREFAB_SOURCE_ID;

        if (henka_prefab_get_source_id_at(
                prefab, prefab_index, &old_source_id) != HENKA_SUCCESS ||
            henka_prefab_get_source_id_at(
                candidate_prefab, prefab_index, &new_source_id) !=
                HENKA_SUCCESS ||
            old_source_id != new_source_id)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
    }
    next_revision = henka_prefab_get_revision(candidate_prefab);
    if (next_revision != old_revision + UINT64_C(1))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }

    for (prefab_index = 0U;
         prefab_index < authoring->project_prefab_count;
         ++prefab_index)
    {
        henka_prefab* tracked_prefab =
            authoring->project_prefabs[prefab_index];
        henka_prefab_instance* tracked_instance =
            authoring->project_prefab_instances[prefab_index];

        if (tracked_prefab == prefab && tracked_instance != NULL)
        {
            if (instance_count >=
                SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
            {
                result = HENKA_ERROR_LIMIT;
                goto cleanup;
            }
            instances[instance_count++] = tracked_instance;
        }
    }

    /* Prepare every allocation-bearing document/bridge object before the
     * manager authority or live instances are changed. The fixed-storage
     * document writes below are then part of the same prepared publication. */
    result = henka_scene_document_create(&candidate_document);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_copy(
            candidate_document, authoring->document);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_create(
            candidate_document,
            authoring->scene,
            &candidate_bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_set_asset_manager(
            candidate_bridge,
            authoring->project_assets);
    }
    for (binding_index = 0U;
         binding_index < authoring->binding_count &&
             result == HENKA_SUCCESS;
         ++binding_index)
    {
        result = sandbox3d_scene_document_bridge_bind(
            candidate_bridge,
            authoring->bindings[binding_index].document_id,
            authoring->bindings[binding_index].entity);
    }
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    result = henka_prefab_save_file(
        candidate_prefab,
        authoring->project_assets,
        project_root,
        asset_path);
    if (result == HENKA_SUCCESS)
    {
        result = henka_assets_reload_prefab_asset(
            authoring->project_assets,
            asset_path,
            NULL,
            &reloaded_prefab);
    }
    if (result != HENKA_SUCCESS || reloaded_prefab != prefab)
    {
        result = result == HENKA_SUCCESS
            ? HENKA_ERROR_INVALID_ARGUMENT
            : result;
        goto cleanup;
    }
    if (instance_count > 0U)
    {
        result = henka_prefab_instance_refresh_batch(
            instances, instance_count);
    }
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    for (prefab_index = 0U;
         prefab_index < instance_count && result == HENKA_SUCCESS;
         ++prefab_index)
    {
        for (binding_index = 0U;
             binding_index < old_entity_count && result == HENKA_SUCCESS;
             ++binding_index)
        {
            henka_prefab_source_id source_id =
                HENKA_INVALID_PREFAB_SOURCE_ID;
            henka_entity entity = HENKA_INVALID_ENTITY;
            henka_scene_document_id document_id =
                HENKA_INVALID_SCENE_DOCUMENT_ID;
            henka_scene_document_object candidate_object;
            henka_scene_document_object live_object;

            result = henka_prefab_get_source_id_at(
                reloaded_prefab, binding_index, &source_id);
            if (result == HENKA_SUCCESS)
            {
                result = henka_prefab_instance_get_entity_for_source_id(
                    instances[prefab_index], source_id, &entity);
            }
            if (result == HENKA_SUCCESS)
            {
                result = sandbox3d_game_authoring_get_object_for_entity(
                    authoring,
                    entity,
                    &document_id,
                    &candidate_object);
            }
            if (result == HENKA_SUCCESS)
            {
                result = sandbox3d_scene_document_bridge_sync_object(
                    candidate_bridge, document_id);
            }
            if (result == HENKA_SUCCESS)
            {
                result = henka_scene_document_get_object(
                    candidate_document, document_id, &candidate_object);
            }
            if (result == HENKA_SUCCESS)
            {
                result = sandbox3d_game_authoring_build_object(
                    authoring, entity, &live_object);
            }
            if (result == HENKA_SUCCESS)
            {
                candidate_object.renderer = live_object.renderer;
                candidate_object.source.prefab_source_revision =
                    next_revision;
                result = henka_scene_document_set_object(
                    candidate_document, &candidate_object);
            }
            if (result == HENKA_SUCCESS)
            {
                result = sandbox3d_game_authoring_sync_prefab_transform_override(
                    authoring, candidate_document, document_id);
            }
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_validate(candidate_document);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_validate(candidate_bridge);
    }
    sandbox3d_scene_document_bridge_destroy(candidate_bridge);
    candidate_bridge = NULL;
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_swap_contents(
            authoring->document,
            candidate_document);
    }
    if (result == HENKA_SUCCESS)
    {
        sandbox3d_game_authoring_clear_history(authoring);
    }

cleanup:
    sandbox3d_scene_document_bridge_destroy(candidate_bridge);
    henka_scene_document_destroy(candidate_document);
    henka_prefab_destroy(candidate_prefab);
    return result;
}

static henka_result sandbox3d_game_authoring_capture_prefab_edit_candidate(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document** out_candidate,
    henka_scene_document_object* out_before,
    henka_scene_document_object* out_captured)
{
    henka_scene_document* candidate = NULL;
    sandbox3d_scene_document_bridge* candidate_bridge = NULL;
    henka_scene_document_id document_id =
        HENKA_INVALID_SCENE_DOCUMENT_ID;
    size_t binding_index;
    henka_result result;

    if (out_candidate != NULL)
    {
        *out_candidate = NULL;
    }
    if (out_before != NULL)
    {
        *out_before = henka_scene_document_object_default();
    }
    if (out_captured != NULL)
    {
        *out_captured = henka_scene_document_object_default();
    }
    if (authoring == NULL || authoring->scene == NULL ||
        authoring->document == NULL || authoring->bridge == NULL ||
        out_candidate == NULL || out_before == NULL ||
        out_captured == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        entity == HENKA_INVALID_ENTITY ||
        !henka_scene_is_entity_valid(authoring->scene, entity) ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &document_id, out_before) != HENKA_SUCCESS ||
        !sandbox3d_game_authoring_is_prefab_document_object(out_before))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_scene_document_create(&candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_copy(candidate, authoring->document);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_create(
            candidate, authoring->scene, &candidate_bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_set_asset_manager(
            candidate_bridge, authoring->project_assets);
    }
    for (binding_index = 0U;
         binding_index < authoring->binding_count &&
            result == HENKA_SUCCESS;
         ++binding_index)
    {
        result = sandbox3d_scene_document_bridge_bind(
            candidate_bridge,
            authoring->bindings[binding_index].document_id,
            authoring->bindings[binding_index].entity);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_sync_object(
            candidate_bridge, document_id);
    }
    if (result == HENKA_SUCCESS)
    {
        henka_scene_document_object candidate_object;
        henka_scene_document_object live_object;

        /* The generic bridge does not own manager-aware material identity or
         * override state. Preserve the same canonical renderer merge used by
         * document save while building the transactional edit candidate. */
        result = henka_scene_document_get_object(
            candidate, document_id, &candidate_object);
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_game_authoring_build_object(
                authoring, entity, &live_object);
        }
        if (result == HENKA_SUCCESS)
        {
            candidate_object.renderer = live_object.renderer;
            result = henka_scene_document_set_object(
                candidate, &candidate_object);
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_game_authoring_sync_prefab_transform_override(
            authoring, candidate, document_id);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_get_object(
            candidate, document_id, out_captured);
    }

    sandbox3d_scene_document_bridge_destroy(candidate_bridge);
    if (result != HENKA_SUCCESS)
    {
        henka_scene_document_destroy(candidate);
        return result;
    }

    *out_candidate = candidate;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_game_authoring_reconcile_prefab_member_state(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    const henka_scene_document_object* object)
{
    henka_prefab_instance* instance = NULL;
    henka_entity resolved_entity = HENKA_INVALID_ENTITY;
    henka_prefab_source_id source_id;
    henka_material material;
    henka_result result;

    if (authoring == NULL || object == NULL ||
        authoring->document == NULL || authoring->scene == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (!sandbox3d_game_authoring_is_prefab_document_object(object))
    {
        return HENKA_SUCCESS;
    }

    result = sandbox3d_game_authoring_find_prefab_instance(
        authoring,
        authoring->document,
        object,
        &instance,
        &resolved_entity);
    if (result != HENKA_SUCCESS || instance == NULL ||
        resolved_entity != entity)
    {
        return result == HENKA_SUCCESS
            ? HENKA_ERROR_INVALID_ARGUMENT
            : result;
    }

    source_id = (henka_prefab_source_id)object->source.prefab_source_id;
    if (object->id == object->source.prefab_instance_root_id)
    {
        if (object->source.prefab_local_transform_override)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    else
    {
        result = object->source.prefab_local_transform_override
            ? henka_prefab_instance_set_local_transform_override(
                instance,
                source_id,
                object->source.prefab_local_transform)
            : henka_prefab_instance_clear_local_transform_override(
                instance,
                source_id);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }

    if (object->renderer.material_path[0] == '\0')
    {
        if (object->renderer.material_override)
        {
            /* The bounded Prefab instance authority intentionally supports
             * manager-owned asset-backed material overrides only. */
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    else if (object->renderer.material_override)
    {
        result = henka_scene_get_entity_material(
            authoring->scene, entity, &material);
        if (result == HENKA_SUCCESS)
        {
            result = henka_prefab_instance_set_material_override(
                instance, source_id, material);
        }
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    else
    {
        result = henka_prefab_instance_clear_material_override(
            instance, source_id);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }

    return sandbox3d_game_authoring_sync_prefab_transform_override(
        authoring, authoring->document, object->id);
}

henka_result sandbox3d_game_authoring_apply_prefab_instance_edits(
    sandbox3d_game_authoring* authoring,
    henka_entity instance_entity)
{
    henka_scene_document* candidate = NULL;
    henka_scene_document_object before;
    henka_scene_document_object captured;
    henka_scene_document_object applied;
    henka_scene_document_object committed;
    henka_prefab_instance* instance = NULL;
    henka_entity resolved_entity = HENKA_INVALID_ENTITY;
    henka_result result;
    henka_result rollback_result;

    result = sandbox3d_game_authoring_capture_prefab_edit_candidate(
        authoring,
        instance_entity,
        &candidate,
        &before,
        &captured);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    result = sandbox3d_game_authoring_find_prefab_instance(
        authoring,
        authoring->document,
        &before,
        &instance,
        &resolved_entity);
    if (result != HENKA_SUCCESS || instance == NULL ||
        resolved_entity != instance_entity)
    {
        henka_scene_document_destroy(candidate);
        return result == HENKA_SUCCESS
            ? HENKA_ERROR_INVALID_ARGUMENT
            : result;
    }
    (void)instance;

    if (captured.renderer.material_override &&
        captured.renderer.material_path[0] == '\0')
    {
        henka_scene_document_destroy(candidate);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    applied = before;
    applied.renderer = captured.renderer;
    /* Renderer enablement is ordinary Prefab presentation state, not part of
     * this bounded material override operation. */
    applied.renderer.enabled = before.renderer.enabled;

    if (before.id == before.source.prefab_instance_root_id)
    {
        /* Root transform is instance placement and remains document-owned. */
        applied.transform = captured.transform;
        applied.source.prefab_local_transform_override = false;
        applied.source.prefab_local_transform = henka_transform_identity();
    }
    else
    {
        applied.source.prefab_local_transform_override =
            captured.source.prefab_local_transform_override;
        applied.source.prefab_local_transform =
            captured.source.prefab_local_transform_override
                ? captured.source.prefab_local_transform
                : henka_transform_identity();
    }

    result = henka_scene_document_set_object(candidate, &applied);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_swap_contents(
            authoring->document, candidate);
    }
    if (result != HENKA_SUCCESS)
    {
        henka_scene_document_destroy(candidate);
        return result;
    }

    result = sandbox3d_game_authoring_reconcile_prefab_member_state(
        authoring, instance_entity, &applied);
    if (result != HENKA_SUCCESS)
    {
        rollback_result = henka_scene_document_swap_contents(
            authoring->document, candidate);
        henka_scene_document_destroy(candidate);
        return rollback_result == HENKA_SUCCESS
            ? result
            : HENKA_ERROR_UNKNOWN;
    }

    result = henka_scene_document_get_object(
        authoring->document, applied.id, &committed);
    if (result == HENKA_SUCCESS && !authoring->history_replaying)
    {
        sandbox3d_game_authoring_append_history(
            authoring,
            instance_entity,
            &before,
            &committed);
    }

    henka_scene_document_destroy(candidate);
    return result;
}

henka_result sandbox3d_game_authoring_revert_prefab_instance_edits(
    sandbox3d_game_authoring* authoring,
    henka_entity instance_entity)
{
    henka_scene_document* candidate = NULL;
    henka_scene_document_object before;
    henka_scene_document_object captured;
    henka_scene_document_object reverted;
    henka_scene_document_object committed;
    henka_prefab_instance* instance = NULL;
    henka_entity resolved_entity = HENKA_INVALID_ENTITY;
    henka_scene_document_id document_id =
        HENKA_INVALID_SCENE_DOCUMENT_ID;
    henka_prefab_source_id source_id;
    bool root_member;
    bool had_transform_override = false;
    bool had_material_override = false;
    henka_transform prior_transform = henka_transform_identity();
    henka_material prior_material = henka_material_default();
    henka_result result;
    henka_result rollback_result;

    if (authoring == NULL || authoring->scene == NULL ||
        authoring->document == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        instance_entity == HENKA_INVALID_ENTITY ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring,
            instance_entity,
            &document_id,
            &before) != HENKA_SUCCESS ||
        before.id != document_id ||
        !sandbox3d_game_authoring_is_prefab_document_object(&before))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = sandbox3d_game_authoring_find_prefab_instance(
        authoring,
        authoring->document,
        &before,
        &instance,
        &resolved_entity);
    if (result != HENKA_SUCCESS || instance == NULL ||
        resolved_entity != instance_entity)
    {
        return result == HENKA_SUCCESS
            ? HENKA_ERROR_INVALID_ARGUMENT
            : result;
    }

    source_id = (henka_prefab_source_id)before.source.prefab_source_id;
    root_member = before.id == before.source.prefab_instance_root_id;

    if (before.renderer.material_override &&
        before.renderer.material_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    if (!root_member)
    {
        result = henka_prefab_instance_get_local_transform_override(
            instance,
            source_id,
            &had_transform_override,
            &prior_transform);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }

    if (before.renderer.material_path[0] != '\0')
    {
        result = henka_prefab_instance_get_material_override(
            instance,
            source_id,
            &had_material_override,
            &prior_material);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }

    if (!root_member)
    {
        result = henka_prefab_instance_clear_local_transform_override(
            instance, source_id);
    }
    else
    {
        result = HENKA_SUCCESS;
    }

    if (result == HENKA_SUCCESS &&
        before.renderer.material_path[0] != '\0')
    {
        result = henka_prefab_instance_clear_material_override(
            instance, source_id);
    }

    if (result != HENKA_SUCCESS)
    {
        if (!root_member && had_transform_override)
        {
            (void)henka_prefab_instance_set_local_transform_override(
                instance, source_id, prior_transform);
        }
        if (before.renderer.material_path[0] != '\0' &&
            had_material_override)
        {
            (void)henka_prefab_instance_set_material_override(
                instance, source_id, prior_material);
        }
        return result;
    }

    result = sandbox3d_game_authoring_capture_prefab_edit_candidate(
        authoring,
        instance_entity,
        &candidate,
        &reverted,
        &captured);
    if (result != HENKA_SUCCESS)
    {
        goto restore_live;
    }

    /* The live document has not changed yet; the capture helper's "before"
     * should still be exactly the state captured above. */
    if (!sandbox3d_game_authoring_authored_state_equal(&before, &reverted))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto restore_live;
    }

    reverted = before;
    reverted.renderer = captured.renderer;
    reverted.renderer.enabled = before.renderer.enabled;
    if (!root_member)
    {
        reverted.source.prefab_local_transform_override =
            captured.source.prefab_local_transform_override;
        reverted.source.prefab_local_transform =
            captured.source.prefab_local_transform_override
                ? captured.source.prefab_local_transform
                : henka_transform_identity();
    }

    result = henka_scene_document_set_object(candidate, &reverted);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_swap_contents(
            authoring->document, candidate);
    }
    if (result != HENKA_SUCCESS)
    {
        goto restore_live;
    }

    result = sandbox3d_game_authoring_reconcile_prefab_member_state(
        authoring, instance_entity, &reverted);
    if (result != HENKA_SUCCESS)
    {
        rollback_result = henka_scene_document_swap_contents(
            authoring->document, candidate);
        if (rollback_result != HENKA_SUCCESS)
        {
            result = HENKA_ERROR_UNKNOWN;
        }
        goto restore_live;
    }

    result = henka_scene_document_get_object(
        authoring->document, reverted.id, &committed);
    if (result == HENKA_SUCCESS && !authoring->history_replaying)
    {
        sandbox3d_game_authoring_append_history(
            authoring,
            instance_entity,
            &before,
            &committed);
    }

    henka_scene_document_destroy(candidate);
    return result;

restore_live:
    henka_scene_document_destroy(candidate);
    rollback_result = HENKA_SUCCESS;
    if (!root_member)
    {
        rollback_result = had_transform_override
            ? henka_prefab_instance_set_local_transform_override(
                instance, source_id, prior_transform)
            : henka_prefab_instance_clear_local_transform_override(
                instance, source_id);
    }
    if (rollback_result == HENKA_SUCCESS &&
        before.renderer.material_path[0] != '\0')
    {
        rollback_result = had_material_override
            ? henka_prefab_instance_set_material_override(
                instance, source_id, prior_material)
            : henka_prefab_instance_clear_material_override(
                instance, source_id);
    }
    return rollback_result == HENKA_SUCCESS
        ? result
        : HENKA_ERROR_UNKNOWN;
}
bool sandbox3d_game_authoring_can_undo(
    const sandbox3d_game_authoring* authoring)
{
    return authoring != NULL && authoring->history_applied_count > 0U;
}

bool sandbox3d_game_authoring_can_redo(
    const sandbox3d_game_authoring* authoring)
{
    return authoring != NULL &&
        authoring->history_applied_count < authoring->history_entry_count;
}

henka_result sandbox3d_game_authoring_undo(
    sandbox3d_game_authoring* authoring)
{
    sandbox3d_game_authoring_history_entry* entry;
    henka_result result;
    henka_result rollback_result;
    if (!sandbox3d_game_authoring_can_undo(authoring) ||
        sandbox3d_game_authoring_is_play_locked(authoring))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    entry = &authoring->history[authoring->history_applied_count - 1U];
    authoring->history_replaying = true;
    result = sandbox3d_game_authoring_update_object_for_entity(
        authoring, entry->entity, &entry->before);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_game_authoring_reconcile_prefab_member_state(
            authoring, entry->entity, &entry->before);
    }
    if (result != HENKA_SUCCESS)
    {
        rollback_result = sandbox3d_game_authoring_update_object_for_entity(
            authoring, entry->entity, &entry->after);
        if (rollback_result == HENKA_SUCCESS)
        {
            rollback_result =
                sandbox3d_game_authoring_reconcile_prefab_member_state(
                    authoring, entry->entity, &entry->after);
        }
        if (rollback_result != HENKA_SUCCESS)
        {
            result = HENKA_ERROR_UNKNOWN;
        }
    }
    authoring->history_replaying = false;
    if (result == HENKA_SUCCESS)
    {
        --authoring->history_applied_count;
    }
    return result;
}

henka_result sandbox3d_game_authoring_redo(
    sandbox3d_game_authoring* authoring)
{
    sandbox3d_game_authoring_history_entry* entry;
    henka_result result;
    henka_result rollback_result;
    if (!sandbox3d_game_authoring_can_redo(authoring) ||
        sandbox3d_game_authoring_is_play_locked(authoring))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    entry = &authoring->history[authoring->history_applied_count];
    authoring->history_replaying = true;
    result = sandbox3d_game_authoring_update_object_for_entity(
        authoring, entry->entity, &entry->after);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_game_authoring_reconcile_prefab_member_state(
            authoring, entry->entity, &entry->after);
    }
    if (result != HENKA_SUCCESS)
    {
        rollback_result = sandbox3d_game_authoring_update_object_for_entity(
            authoring, entry->entity, &entry->before);
        if (rollback_result == HENKA_SUCCESS)
        {
            rollback_result =
                sandbox3d_game_authoring_reconcile_prefab_member_state(
                    authoring, entry->entity, &entry->before);
        }
        if (rollback_result != HENKA_SUCCESS)
        {
            result = HENKA_ERROR_UNKNOWN;
        }
    }
    authoring->history_replaying = false;
    if (result == HENKA_SUCCESS)
    {
        ++authoring->history_applied_count;
    }
    return result;
}

size_t sandbox3d_game_authoring_get_behavior_count_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity)
{
    size_t index;
    if (authoring == NULL ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return 0U;
    }
    return henka_scene_document_get_behavior_count(
        authoring->document,
        authoring->bindings[index].document_id);
}

henka_result sandbox3d_game_authoring_get_behavior_at_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity,
    size_t behavior_index,
    henka_scene_document_behavior* out_behavior)
{
    size_t index;
    if (authoring == NULL || out_behavior == NULL ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_get_behavior_at(
        authoring->document,
        authoring->bindings[index].document_id,
        behavior_index,
        out_behavior);
}

henka_result sandbox3d_game_authoring_get_behavior_for_entity(
    const sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_behavior_id behavior_id,
    henka_scene_document_behavior* out_behavior)
{
    size_t index;
    if (authoring == NULL || out_behavior == NULL ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_get_behavior(
        authoring->document,
        authoring->bindings[index].document_id,
        behavior_id,
        out_behavior);
}

henka_result sandbox3d_game_authoring_add_behavior_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    const henka_scene_document_behavior* behavior,
    henka_scene_document_behavior_id* out_behavior_id)
{
    size_t index;
    if (out_behavior_id != NULL)
    {
        *out_behavior_id = HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID;
    }
    if (authoring == NULL || behavior == NULL || out_behavior_id == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_add_behavior(
        authoring->document,
        authoring->bindings[index].document_id,
        behavior,
        out_behavior_id);
}

henka_result sandbox3d_game_authoring_update_behavior_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    const henka_scene_document_behavior* behavior)
{
    size_t index;
    if (authoring == NULL || behavior == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_set_behavior(
        authoring->document,
        authoring->bindings[index].document_id,
        behavior);
}

henka_result sandbox3d_game_authoring_remove_behavior_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_behavior_id behavior_id)
{
    size_t index;
    if (authoring == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_scene_document_remove_behavior(
        authoring->document,
        authoring->bindings[index].document_id,
        behavior_id);
}

henka_result sandbox3d_game_authoring_reload_behavior_for_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity,
    henka_scene_document_behavior_id behavior_id,
    henka_script_source_diagnostic* out_diagnostic)
{
    size_t index;
    if (out_diagnostic != NULL)
    {
        memset(out_diagnostic, 0, sizeof(*out_diagnostic));
        out_diagnostic->result = HENKA_ERROR_INVALID_ARGUMENT;
        (void)snprintf(
            out_diagnostic->message,
            sizeof(out_diagnostic->message),
            "Behavior reload rejected");
    }
    if (authoring == NULL || behavior_id == HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return sandbox3d_play_session_reload_behavior(
        authoring->play_session,
        authoring->bindings[index].document_id,
        behavior_id,
        out_diagnostic);
}

henka_result sandbox3d_game_authoring_attach_script_template(
    sandbox3d_game_authoring* authoring,
    const char* project_root,
    henka_entity entity,
    henka_script_language language)
{
    henka_scene_document_behavior behavior;
    henka_scene_document_behavior_id behavior_id;
    henka_scene_document_id document_id;
    henka_scene_document_object object;
    char relative_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    const char* extension;
    int written;
    size_t behavior_count;
    henka_result result;

    if (authoring == NULL || project_root == NULL || project_root[0] == '\0' ||
        strlen(project_root) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES ||
        entity == HENKA_INVALID_ENTITY ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        (language != HENKA_SCRIPT_LANGUAGE_LUA &&
         language != HENKA_SCRIPT_LANGUAGE_HENKASCRIPT) ||
        sandbox3d_game_authoring_get_object_for_entity(
            authoring, entity, &document_id, &object) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    behavior_count = object.behavior_count;
    if (behavior_count >= HENKA_SCENE_DOCUMENT_MAX_BEHAVIORS_PER_OBJECT)
    {
        return HENKA_ERROR_LIMIT;
    }
    extension = language == HENKA_SCRIPT_LANGUAGE_LUA ? "lua" : "hks";
    written = snprintf(
        relative_path,
        sizeof(relative_path),
        "scripts/behavior_%llu_%zu.%s",
        (unsigned long long)document_id,
        behavior_count + 1U,
        extension);
    if (written < 0 || (size_t)written >= sizeof(relative_path))
    {
        return HENKA_ERROR_LIMIT;
    }
    behavior = henka_scene_document_behavior_default();
    behavior.language = language;
    written = snprintf(
        behavior.asset_path,
        sizeof(behavior.asset_path),
        "%s",
        relative_path);
    if (written < 0 || (size_t)written >= sizeof(behavior.asset_path))
    {
        return HENKA_ERROR_LIMIT;
    }
    result = sandbox3d_game_authoring_add_behavior_for_entity(
        authoring,
        entity,
        &behavior,
        &behavior_id);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_script_asset_create_template(
        project_root,
        relative_path,
        language);
    if (result != HENKA_SUCCESS)
    {
        (void)sandbox3d_game_authoring_remove_behavior_for_entity(
            authoring,
            entity,
            behavior_id);
        return result;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_save(
    sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    henka_scene_document* candidate_document = NULL;
    sandbox3d_scene_document_bridge* candidate_bridge = NULL;
    henka_camera current_camera;
    henka_scene_environment_desc current_environment = {0};
    henka_scene_render_settings current_render_settings = {0};
    henka_scene_render_resources current_render_resources = {0};
    size_t binding_index;
    size_t binding_count = 0U;
    bool has_current_camera = false;
    henka_result result;
    if (authoring == NULL || project_root == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        strlen(project_root) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_scene_document_create(&candidate_document);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_scene_document_copy(candidate_document, authoring->document);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_create(
            candidate_document,
            authoring->scene,
            &candidate_bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_set_asset_manager(
            candidate_bridge,
            authoring->project_assets);
    }
    binding_count = sandbox3d_scene_document_bridge_get_binding_count(
        authoring->bridge);
    if (result == HENKA_SUCCESS && binding_count != authoring->binding_count)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (binding_index = 0U;
         binding_index < binding_count && result == HENKA_SUCCESS;
         ++binding_index)
    {
        henka_scene_document_id document_id;
        henka_entity entity;

        result = sandbox3d_scene_document_bridge_get_binding_at(
            authoring->bridge,
            binding_index,
            &document_id,
            &entity);
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_scene_document_bridge_bind(
                candidate_bridge,
                document_id,
                entity);
        }
    }
    for (binding_index = 0U;
         binding_index < binding_count && result == HENKA_SUCCESS;
         ++binding_index)
    {
        henka_scene_document_id document_id;
        henka_entity entity;

        result = sandbox3d_scene_document_bridge_get_binding_at(
            authoring->bridge,
            binding_index,
            &document_id,
            &entity);
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_scene_document_bridge_sync_object(
                candidate_bridge,
                document_id);
        }
        if (result == HENKA_SUCCESS)
        {
            henka_scene_document_object candidate_object;
            henka_scene_document_object live_object;

            /*
             * The generic bridge intentionally owns only generic presentation
             * synchronization. Manager-aware material identity, scalar
             * overrides, and texture override paths are captured by Game
             * Authoring's canonical object builder. Merge only renderer state
             * so Prefab provenance, hierarchy, physics, behaviors, and other
             * document-owned fields remain authoritative in the candidate.
             */
            result = henka_scene_document_get_object(
                candidate_document,
                document_id,
                &candidate_object);
            if (result == HENKA_SUCCESS)
            {
                result = sandbox3d_game_authoring_build_object(
                    authoring,
                    entity,
                    &live_object);
            }
            if (result == HENKA_SUCCESS)
            {
                candidate_object.renderer = live_object.renderer;
                result = henka_scene_document_set_object(
                    candidate_document,
                    &candidate_object);
            }
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_game_authoring_sync_prefab_transform_override(
                authoring,
                candidate_document,
                document_id);
        }
    }
    if (result == HENKA_SUCCESS &&
        henka_scene_get_environment(
            authoring->scene,
            &current_environment) != HENKA_SUCCESS)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS &&
        henka_scene_get_render_settings(
            authoring->scene,
            &current_render_settings) != HENKA_SUCCESS)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS &&
        henka_scene_get_render_resources(
            authoring->scene,
            &current_render_resources) != HENKA_SUCCESS)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_set_environment(
            candidate_document,
            current_environment);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_set_render_settings(
            candidate_document,
            current_render_settings);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_set_render_resources(
            candidate_document,
            current_render_resources);
    }
    if (result == HENKA_SUCCESS)
    {
        if (henka_scene_get_camera(authoring->scene, &current_camera) ==
            HENKA_SUCCESS)
        {
            has_current_camera = true;
        }
        if (has_current_camera)
        {
            result = henka_scene_document_set_camera(
                candidate_document, &current_camera);
        }
    }
    if (result == HENKA_SUCCESS)
    {
        char* manifest_path = NULL;
        char* manifest_stage_path = NULL;
        char* scene_path = NULL;
        char* scene_stage_relative_path = NULL;
        char* scene_stage_path = NULL;
        sandbox3d_game_authoring_file_snapshot manifest_snapshot = {0};
        sandbox3d_game_authoring_file_snapshot scene_snapshot = {0};
        bool scene_published = false;
        bool manifest_published = false;
        henka_result rollback_result = HENKA_SUCCESS;

        result = sandbox3d_game_authoring_get_project_manifest_path(
            project_root,
            &manifest_path);
        if (result == HENKA_SUCCESS)
        {
            result = henka_path_resolve_confined(
                project_root,
                authoring->relative_path,
                &scene_path);
        }
        if (result == HENKA_SUCCESS)
        {
            manifest_stage_path =
                sandbox3d_game_authoring_append_path_suffix(
                    manifest_path,
                    SANDBOX3D_GAME_AUTHORING_PROJECT_STAGE_SUFFIX);
            scene_stage_relative_path =
                sandbox3d_game_authoring_append_path_suffix(
                    authoring->relative_path,
                    SANDBOX3D_GAME_AUTHORING_PROJECT_STAGE_SUFFIX);
            if (manifest_stage_path == NULL ||
                scene_stage_relative_path == NULL)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
            }
        }
        if (result == HENKA_SUCCESS &&
            strlen(scene_stage_relative_path) >=
                SANDBOX3D_GAME_AUTHORING_MAX_RELATIVE_PATH_BYTES)
        {
            result = HENKA_ERROR_LIMIT;
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_path_resolve_confined(
                project_root,
                scene_stage_relative_path,
                &scene_stage_path);
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_game_authoring_snapshot_file(
                manifest_path,
                SANDBOX3D_GAME_AUTHORING_PROJECT_MANIFEST_MAX_BYTES,
                &manifest_snapshot);
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_game_authoring_snapshot_file(
                scene_path,
                HENKA_SCENE_DOCUMENT_MAX_FILE_BYTES,
                &scene_snapshot);
        }

        /*
         * Stage both files completely before either published project file
         * changes. This removes the old failure mode where henka.project
         * could advance before the selected .hscene had committed.
         */
        if (result == HENKA_SUCCESS)
        {
            result = henka_scene_document_save_file(
                candidate_document,
                project_root,
                scene_stage_relative_path);
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_game_authoring_save_project_manifest_to_path(
                authoring,
                project_root,
                manifest_stage_path);
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_game_authoring_replace_file(
                scene_stage_path,
                scene_path);
            scene_published = result == HENKA_SUCCESS;
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_game_authoring_replace_file(
                manifest_stage_path,
                manifest_path);
            manifest_published = result == HENKA_SUCCESS;
        }

        if (result != HENKA_SUCCESS && (scene_published || manifest_published))
        {
            /*
             * Publication is process-transactional: any second-file failure
             * restores the exact pre-save bytes (or absence) of both project
             * files before returning the original save failure.
             */
            rollback_result = sandbox3d_game_authoring_restore_snapshot(
                scene_path,
                &scene_snapshot);
            if (sandbox3d_game_authoring_restore_snapshot(
                    manifest_path,
                    &manifest_snapshot) != HENKA_SUCCESS)
            {
                rollback_result = HENKA_ERROR_PLATFORM;
            }
            if (rollback_result != HENKA_SUCCESS)
            {
                result = rollback_result;
            }
        }

        if (scene_stage_path != NULL)
        {
            (void)remove(scene_stage_path);
        }
        if (manifest_stage_path != NULL)
        {
            (void)remove(manifest_stage_path);
        }
        sandbox3d_game_authoring_file_snapshot_destroy(&scene_snapshot);
        sandbox3d_game_authoring_file_snapshot_destroy(&manifest_snapshot);
        henka_free(scene_stage_path);
        henka_free(scene_stage_relative_path);
        henka_free(scene_path);
        henka_free(manifest_stage_path);
        henka_free(manifest_path);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_swap_contents(
            authoring->document,
            candidate_document);
    }

cleanup:
    sandbox3d_scene_document_bridge_destroy(candidate_bridge);
    henka_scene_document_destroy(candidate_document);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_game_authoring_set_project_root(
            authoring,
            project_root);
    }
    return result;
}

henka_result sandbox3d_game_authoring_load(
    sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    henka_mesh* candidate_owned_meshes[
        SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS] = {0};
    henka_mesh* published_owned_meshes[
        SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS] = {0};
    henka_scene_document* candidate = NULL;
    henka_scene* candidate_scene = NULL;
    henka_scene_environment_desc candidate_environment;
    henka_scene_render_settings candidate_render_settings =
        henka_scene_render_settings_default();
    henka_scene_render_resources candidate_render_resources =
        henka_scene_render_resources_default();
    sandbox3d_scene_document_bridge* candidate_bridge = NULL;
    char selected_relative_path[
        SANDBOX3D_GAME_AUTHORING_MAX_RELATIVE_PATH_BYTES];
    size_t index;
    size_t candidate_owned_mesh_count = 0U;
    size_t published_owned_mesh_count = 0U;
    henka_result result;
    if (authoring == NULL || project_root == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        strlen(project_root) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_game_authoring_get_startup_scene_path(
        authoring,
        project_root,
        selected_relative_path,
        sizeof(selected_relative_path),
        true);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_scene_document_create(&candidate);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_scene_document_load_file(
        candidate,
        project_root,
        selected_relative_path);
    if (result != HENKA_SUCCESS)
    {
        henka_scene_document_destroy(candidate);
        return result;
    }
    if (henka_scene_document_get_object_count(candidate) != authoring->binding_count)
    {
        henka_scene_document_destroy(candidate);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < authoring->binding_count; ++index)
    {
        if (henka_scene_document_get_object(
                candidate,
                authoring->bindings[index].document_id,
                &(henka_scene_document_object){0}) == HENKA_SUCCESS)
        {
            continue;
        }
        henka_scene_document_destroy(candidate);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_scene_clone(authoring->scene, &candidate_scene);
    if (result != HENKA_SUCCESS)
    {
        goto load_cleanup;
    }
    result = henka_scene_document_get_environment(
        candidate,
        &candidate_environment);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_environment(
            candidate_scene,
            candidate_environment);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_get_render_settings(
            candidate,
            &candidate_render_settings);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_render_settings(
            candidate_scene,
            candidate_render_settings);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_get_render_resources(
            candidate,
            &candidate_render_resources);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_set_render_resources(
            candidate_scene,
            candidate_render_resources);
    }
    if (result != HENKA_SUCCESS)
    {
        goto load_cleanup;
    }
    for (index = 0U;
        index < authoring->binding_count && result == HENKA_SUCCESS;
        ++index)
    {
        henka_scene_document_object object;
        henka_mesh* owned_mesh = NULL;
        result = henka_scene_document_get_object(
            candidate,
            authoring->bindings[index].document_id,
            &object);
        if (result == HENKA_SUCCESS)
        {
            if (object.source.kind == HENKA_SCENE_DOCUMENT_SOURCE_ASSET &&
                object.source.asset_kind == HENKA_SCENE_DOCUMENT_ASSET_PREFAB)
            {
                henka_scene_document_object root_object;

                if (object.source.prefab_instance_root_id ==
                        HENKA_INVALID_SCENE_DOCUMENT_ID ||
                    henka_scene_document_get_object(
                        candidate,
                        object.source.prefab_instance_root_id,
                        &root_object) != HENKA_SUCCESS ||
                    root_object.source.kind !=
                        HENKA_SCENE_DOCUMENT_SOURCE_ASSET ||
                    root_object.source.asset_kind !=
                        HENKA_SCENE_DOCUMENT_ASSET_PREFAB ||
                    root_object.source.prefab_instance_root_id != root_object.id)
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                }
                else if (object.source.prefab_instance_root_id == object.id)
                {
                    result = sandbox3d_game_authoring_validate_existing_prefab(
                        project_root,
                        authoring,
                        candidate,
                        &root_object);
                }
            }
            else
            {
                result = sandbox3d_game_authoring_materialize_source(
                    project_root,
                    authoring->project_engine,
                    authoring->project_assets,
                    candidate_scene,
                    authoring->bindings[index].entity,
                    &object,
                    &owned_mesh);
            }
        }
        if (result == HENKA_SUCCESS && owned_mesh != NULL)
        {
            if (candidate_owned_mesh_count >=
                SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
            {
                henka_mesh_destroy(owned_mesh);
                result = HENKA_ERROR_LIMIT;
            }
            else
            {
                candidate_owned_meshes[candidate_owned_mesh_count++] = owned_mesh;
            }
        }
    }
    if (result != HENKA_SUCCESS)
    {
        goto load_cleanup;
    }
    result = sandbox3d_scene_document_bridge_create(
        candidate,
        candidate_scene,
        &candidate_bridge);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_set_asset_manager(
            candidate_bridge,
            authoring->project_assets);
    }
    for (index = 0U;
        index < authoring->binding_count && result == HENKA_SUCCESS;
        ++index)
    {
        result = sandbox3d_scene_document_bridge_bind(
            candidate_bridge,
            authoring->bindings[index].document_id,
            authoring->bindings[index].entity);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_objects(candidate_bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_hierarchy(
            candidate_bridge);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_game_authoring_apply_prefab_transform_overrides(
            authoring,
            candidate,
            candidate_scene);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_apply_camera(
            candidate_bridge);
    }
    sandbox3d_scene_document_bridge_destroy(candidate_bridge);
    candidate_bridge = NULL;
    if (result != HENKA_SUCCESS)
    {
        goto load_cleanup;
    }
    /* Publish only a candidate whose persistent object IDs exactly match the
     * live binding set. Runtime entities remain generation-checked derived
     * state; names are not identity and cannot silently remap a loaded file. */
    result = henka_scene_replace_contents(authoring->scene, candidate_scene);
    if (result != HENKA_SUCCESS)
    {
        goto load_cleanup;
    }
    result = henka_scene_document_swap_contents(
        authoring->document,
        candidate);
    if (result != HENKA_SUCCESS)
    {
        /* Both swaps are allocation-free. Restore the first swap if the
         * validated document invariant is violated rather than reconstructing
         * state through fallible setters. */
        if (henka_scene_replace_contents(authoring->scene, candidate_scene) !=
            HENKA_SUCCESS)
        {
            result = HENKA_ERROR_UNKNOWN;
        }
        goto load_cleanup;
    }
    henka_scene_document_destroy(candidate);
    candidate = NULL;
    henka_scene_destroy(candidate_scene);
    candidate_scene = NULL;
    for (index = 0U; index < candidate_owned_mesh_count; ++index)
    {
        published_owned_meshes[published_owned_mesh_count++] =
            candidate_owned_meshes[index];
    }
    for (index = 0U; index < authoring->owned_project_mesh_count; ++index)
    {
        henka_mesh* old_mesh = authoring->owned_project_meshes[index];
        if (sandbox3d_game_authoring_scene_references_mesh(
                authoring->scene,
                authoring,
                old_mesh) &&
            !sandbox3d_game_authoring_mesh_in_list(
                published_owned_meshes,
                published_owned_mesh_count,
                old_mesh))
        {
            published_owned_meshes[published_owned_mesh_count++] = old_mesh;
        }
    }
    for (index = 0U; index < authoring->owned_project_mesh_count; ++index)
    {
        henka_mesh* old_mesh = authoring->owned_project_meshes[index];
        if (!sandbox3d_game_authoring_mesh_in_list(
                published_owned_meshes,
                published_owned_mesh_count,
                old_mesh))
        {
            henka_mesh_destroy(old_mesh);
        }
    }
    memset(
        authoring->owned_project_meshes,
        0,
        sizeof(authoring->owned_project_meshes));
    memcpy(
        authoring->owned_project_meshes,
        published_owned_meshes,
        published_owned_mesh_count * sizeof(published_owned_meshes[0]));
    authoring->owned_project_mesh_count = published_owned_mesh_count;
    (void)sandbox3d_game_authoring_set_project_root(authoring, project_root);
    (void)snprintf(
        authoring->relative_path,
        sizeof(authoring->relative_path),
        "%s",
        selected_relative_path);
    return result;

load_cleanup:
    sandbox3d_scene_document_bridge_destroy(candidate_bridge);
    henka_scene_destroy(candidate_scene);
    henka_scene_document_destroy(candidate);
    sandbox3d_game_authoring_destroy_mesh_list(
        candidate_owned_meshes,
        candidate_owned_mesh_count);
    return result;
}

henka_result sandbox3d_game_authoring_save_play_state(
    sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    char state_relative_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    henka_result result;
    if (authoring == NULL || project_root == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        strlen(project_root) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES ||
        sandbox3d_game_authoring_get_state_relative_path(
            authoring,
            state_relative_path,
            sizeof(state_relative_path)) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_state_store_save_file(
        authoring->play_script_state_store != NULL
            ? authoring->play_script_state_store
            : authoring->script_state_store,
        project_root,
        state_relative_path);
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_game_authoring_set_project_root(authoring, project_root);
    }
    return result;
}

henka_result sandbox3d_game_authoring_load_play_state(
    sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    char state_relative_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    henka_result result;
    if (authoring == NULL || project_root == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        strlen(project_root) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES ||
        sandbox3d_game_authoring_get_state_relative_path(
            authoring,
            state_relative_path,
            sizeof(state_relative_path)) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_state_store_load_file(
        authoring->script_state_store,
        project_root,
        state_relative_path);
    if (result == HENKA_SUCCESS)
    {
        henka_script_state_store_destroy(authoring->play_script_state_store);
        authoring->play_script_state_store = NULL;
        result = sandbox3d_game_authoring_set_project_root(authoring, project_root);
    }
    return result;
}

henka_result sandbox3d_game_authoring_set_script_state_value(
    sandbox3d_game_authoring* authoring,
    henka_script_state_identity identity,
    uint32_t key,
    henka_script_state_value value)
{
    if (authoring == NULL || sandbox3d_game_authoring_is_play_locked(authoring))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_script_state_store_set(
        authoring->script_state_store, identity, key, value);
}

henka_result sandbox3d_game_authoring_get_script_state_value(
    const sandbox3d_game_authoring* authoring,
    henka_script_state_identity identity,
    uint32_t key,
    henka_script_state_value* out_value,
    bool* out_present)
{
    if (authoring == NULL || sandbox3d_game_authoring_is_play_locked(authoring))
    {
        if (out_value != NULL)
        {
            *out_value = (henka_script_state_value){HENKA_SCRIPT_STATE_VALUE_NONE};
        }
        if (out_present != NULL)
        {
            *out_present = false;
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_script_state_store_get(
        authoring->script_state_store,
        identity,
        key,
        out_value,
        out_present);
}

const char* sandbox3d_game_authoring_get_relative_path(
    const sandbox3d_game_authoring* authoring)
{
    return authoring == NULL ? NULL : authoring->relative_path;
}

henka_scene* sandbox3d_game_authoring_get_authoring_scene(
    const sandbox3d_game_authoring* authoring)
{
    return authoring == NULL ? NULL : authoring->scene;
}

henka_scene* sandbox3d_game_authoring_get_play_scene(
    const sandbox3d_game_authoring* authoring)
{
    return authoring == NULL ? NULL : authoring->play_scene;
}

sandbox3d_play_session_state sandbox3d_game_authoring_get_play_state(
    const sandbox3d_game_authoring* authoring)
{
    return authoring == NULL
        ? SANDBOX3D_PLAY_SESSION_FAILED
        : authoring->play_session == NULL
            ? SANDBOX3D_PLAY_SESSION_STOPPED
            : sandbox3d_play_session_get_state(authoring->play_session);
}

henka_result sandbox3d_game_authoring_start_play(
    sandbox3d_game_authoring* authoring)
{
    sandbox3d_play_session* play_session = NULL;
    henka_script_state_store* play_script_state_store = NULL;
    henka_result result;
    size_t index;

    if (authoring == NULL || sandbox3d_game_authoring_get_play_state(authoring) !=
        SANDBOX3D_PLAY_SESSION_STOPPED || authoring->play_scene != NULL ||
        authoring->play_bridge != NULL || authoring->play_session != NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_script_state_store_clone(
        authoring->script_state_store,
        &play_script_state_store);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_clone(authoring->scene, &authoring->play_scene);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_scene_document_bridge_create(
            authoring->document,
            authoring->play_scene,
            &authoring->play_bridge);
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_scene_document_bridge_set_asset_manager(
                authoring->play_bridge,
                authoring->project_assets);
        }
    }
    for (index = 0U; result == HENKA_SUCCESS && index < authoring->binding_count; ++index)
    {
        result = sandbox3d_scene_document_bridge_bind(
            authoring->play_bridge,
            authoring->bindings[index].document_id,
            authoring->bindings[index].entity);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_play_session_create_with_project_root(
            authoring->play_bridge,
            authoring->play_world,
            authoring->project_root,
            &play_session);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_play_session_set_script_state_store(
            play_session,
            play_script_state_store);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_play_session_set_audio_system(
            play_session,
            authoring->audio_system);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_play_session_set_audio_asset_manager(
            play_session,
            authoring->audio_asset_manager);
    }
    if (result == HENKA_SUCCESS && authoring->play_observer_position_valid)
    {
        result = sandbox3d_play_session_set_input_context(
            play_session,
            authoring->play_input_query,
            authoring->play_input_user_data,
            authoring->play_observer_position);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_play_session_start(play_session);
    }
    if (result != HENKA_SUCCESS)
    {
        sandbox3d_play_session_destroy(play_session);
        sandbox3d_scene_document_bridge_destroy(authoring->play_bridge);
        henka_scene_destroy(authoring->play_scene);
        henka_script_state_store_destroy(play_script_state_store);
        authoring->play_bridge = NULL;
        authoring->play_scene = NULL;
        return result;
    }
    henka_script_state_store_destroy(authoring->play_script_state_store);
    authoring->play_script_state_store = play_script_state_store;
    authoring->play_session = play_session;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_set_audio_system(
    sandbox3d_game_authoring* authoring,
    henka_audio_system* audio_system)
{
    if (authoring == NULL ||
        sandbox3d_game_authoring_get_play_state(authoring) !=
            SANDBOX3D_PLAY_SESSION_STOPPED)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    authoring->audio_system = audio_system;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_set_audio_asset_manager(
    sandbox3d_game_authoring* authoring,
    henka_asset_manager* asset_manager)
{
    if (authoring == NULL ||
        sandbox3d_game_authoring_get_play_state(authoring) !=
            SANDBOX3D_PLAY_SESSION_STOPPED)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    authoring->audio_asset_manager = asset_manager;
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_set_play_input_context(
    sandbox3d_game_authoring* authoring,
    sandbox3d_game_authoring_input_query input_query,
    void* input_user_data,
    henka_vec3 observer_position)
{
    if (authoring == NULL ||
        !isfinite(observer_position.x) ||
        !isfinite(observer_position.y) ||
        !isfinite(observer_position.z))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    authoring->play_input_query = input_query;
    authoring->play_input_user_data = input_user_data;
    authoring->play_observer_position = observer_position;
    authoring->play_observer_position_valid = true;
    if (authoring->play_session != NULL)
    {
        return sandbox3d_play_session_set_input_context(
            authoring->play_session,
            input_query,
            input_user_data,
            observer_position);
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_game_authoring_pause_play(
    sandbox3d_game_authoring* authoring)
{
    return authoring == NULL
        ? HENKA_ERROR_INVALID_ARGUMENT
        : authoring->play_session == NULL
            ? HENKA_ERROR_INVALID_ARGUMENT
            : sandbox3d_play_session_pause(authoring->play_session);
}

henka_result sandbox3d_game_authoring_resume_play(
    sandbox3d_game_authoring* authoring)
{
    return authoring == NULL
        ? HENKA_ERROR_INVALID_ARGUMENT
        : authoring->play_session == NULL
            ? HENKA_ERROR_INVALID_ARGUMENT
            : sandbox3d_play_session_resume(authoring->play_session);
}

henka_result sandbox3d_game_authoring_tick_play(
    sandbox3d_game_authoring* authoring)
{
    return authoring == NULL
        ? HENKA_ERROR_INVALID_ARGUMENT
        : authoring->play_session == NULL
            ? HENKA_ERROR_INVALID_ARGUMENT
            : sandbox3d_play_session_tick(authoring->play_session);
}

henka_result sandbox3d_game_authoring_step_play(
    sandbox3d_game_authoring* authoring)
{
    return authoring == NULL
        ? HENKA_ERROR_INVALID_ARGUMENT
        : authoring->play_session == NULL
            ? HENKA_ERROR_INVALID_ARGUMENT
            : sandbox3d_play_session_step_fixed(authoring->play_session);
}

henka_result sandbox3d_game_authoring_stop_play(
    sandbox3d_game_authoring* authoring)
{
    henka_result result;

    if (authoring == NULL || authoring->play_session == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_play_session_stop(authoring->play_session);
    sandbox3d_play_session_destroy(authoring->play_session);
    authoring->play_session = NULL;
    sandbox3d_scene_document_bridge_destroy(authoring->play_bridge);
    authoring->play_bridge = NULL;
    henka_scene_destroy(authoring->play_scene);
    authoring->play_scene = NULL;
    return result;
}

bool sandbox3d_game_authoring_is_play_locked(
    const sandbox3d_game_authoring* authoring)
{
    const sandbox3d_play_session_state state =
        sandbox3d_game_authoring_get_play_state(authoring);
    return authoring != NULL &&
        (state != SANDBOX3D_PLAY_SESSION_STOPPED ||
            sandbox3d_scene_document_bridge_is_play_locked(authoring->bridge));
}
