#include "game_authoring.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include <henka/memory.h>
#include <henka/authoring_modeling.h>
#include <henka/engine.h>
#include <henka/persistence.h>
#include <henka/script_asset.h>

#define SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS HENKA_SCENE_DOCUMENT_MAX_OBJECTS
#define SANDBOX3D_GAME_AUTHORING_MAX_RELATIVE_PATH_BYTES HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES
#define SANDBOX3D_GAME_AUTHORING_MAX_HISTORY_STEPS 32U
#define SANDBOX3D_GAME_AUTHORING_PROJECT_MANIFEST_PATH "henka.project"
#define SANDBOX3D_GAME_AUTHORING_PROJECT_MANIFEST_SCHEMA_VERSION 1

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
     * manager-backed sources remain owned by the asset manager. */
    henka_mesh* owned_project_meshes[SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS];
    size_t owned_project_mesh_count;
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

static henka_result sandbox3d_game_authoring_build_object(
    const henka_scene* scene,
    henka_entity entity,
    henka_scene_document_object* out_object)
{
    henka_scene_object_info info;
    henka_interaction_desc interaction;
    henka_material material;
    const henka_material_asset* material_asset = NULL;
    int written;
    if (scene == NULL || out_object == NULL ||
        !henka_scene_is_entity_valid(scene, entity) ||
        henka_scene_get_entity_info(scene, entity, &info) != HENKA_SUCCESS ||
        henka_scene_get_entity_interaction(scene, entity, &interaction) != HENKA_SUCCESS ||
        henka_scene_get_entity_material(scene, entity, &material) != HENKA_SUCCESS ||
        henka_scene_get_entity_material_asset(scene, entity, &material_asset) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
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
    if (material_asset != NULL || material.base_color_texture != NULL ||
        material.normal_texture != NULL || material.metallic_roughness_texture != NULL ||
        material.occlusion_texture != NULL || material.emissive_texture != NULL ||
        material.transmission_texture != NULL || material.thickness_texture != NULL ||
        material.terrain_layers_enabled)
    {
        /* This bridge has no material-resource path/authority with which to
         * reconstruct borrowed asset or texture state. Do not collapse that
         * state into an apparently complete inline document. */
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    /* Pointer-free inline material state is document-owned only when no
     * manager-owned definition or borrowed texture state is attached. */
    out_object->renderer.material_override =
        material_asset == NULL && material.shader != NULL;
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

static henka_result sandbox3d_game_authoring_save_project_manifest(
    const sandbox3d_game_authoring* authoring,
    const char* project_root)
{
    henka_settings* settings = NULL;
    char* manifest_path = NULL;
    char* scene_path = NULL;
    henka_result result;

    if (authoring == NULL || project_root == NULL ||
        authoring->relative_path[0] == '\0')
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
    result = sandbox3d_game_authoring_get_project_manifest_path(
        project_root,
        &manifest_path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
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
    henka_free(manifest_path);
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
        return HENKA_SUCCESS;
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
                candidate_authoring->binding_count >=
                    SANDBOX3D_GAME_AUTHORING_MAX_BINDINGS)
            {
                result = HENKA_ERROR_INVALID_ARGUMENT;
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
        authoring->scene, entity, &object);
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
        authoring->scene, entity, &target_object);
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
            authoring->scene, current_entity, &preflight_object);
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

henka_result sandbox3d_game_authoring_unregister_entity(
    sandbox3d_game_authoring* authoring,
    henka_entity entity)
{
    size_t index;
    if (authoring == NULL || sandbox3d_game_authoring_is_play_locked(authoring) ||
        (index = sandbox3d_game_authoring_find_binding(authoring, entity)) == SIZE_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (sandbox3d_scene_document_bridge_unbind(
            authoring->bridge,
            authoring->bindings[index].document_id) != HENKA_SUCCESS ||
        henka_scene_document_remove_object(
            authoring->document,
            authoring->bindings[index].document_id) != HENKA_SUCCESS)
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
    if (!sandbox3d_game_authoring_can_undo(authoring) ||
        sandbox3d_game_authoring_is_play_locked(authoring))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    entry = &authoring->history[authoring->history_applied_count - 1U];
    authoring->history_replaying = true;
    result = sandbox3d_game_authoring_update_object_for_entity(
        authoring, entry->entity, &entry->before);
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
    if (!sandbox3d_game_authoring_can_redo(authoring) ||
        sandbox3d_game_authoring_is_play_locked(authoring))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    entry = &authoring->history[authoring->history_applied_count];
    authoring->history_replaying = true;
    result = sandbox3d_game_authoring_update_object_for_entity(
        authoring, entry->entity, &entry->after);
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
    henka_camera previous_camera;
    const bool had_authored_camera =
        authoring != NULL && authoring->document != NULL &&
        henka_scene_document_has_camera(authoring->document);
    henka_result result;
    if (authoring == NULL || project_root == NULL ||
        sandbox3d_game_authoring_is_play_locked(authoring) ||
        strlen(project_root) >= HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (had_authored_camera &&
        henka_scene_document_get_camera(authoring->document, &previous_camera) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = sandbox3d_scene_document_bridge_sync_camera(authoring->bridge);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = sandbox3d_game_authoring_save_project_manifest(
        authoring,
        project_root);
    if (result != HENKA_SUCCESS)
    {
        if (had_authored_camera)
        {
            (void)henka_scene_document_set_camera(authoring->document, &previous_camera);
        }
        else
        {
            (void)henka_scene_document_clear_camera(authoring->document);
        }
        return result;
    }

    result = henka_scene_document_save_file(
        authoring->document,
        project_root,
        authoring->relative_path);
    if (result != HENKA_SUCCESS)
    {
        if (had_authored_camera)
        {
            (void)henka_scene_document_set_camera(authoring->document, &previous_camera);
        }
        else
        {
            (void)henka_scene_document_clear_camera(authoring->document);
        }
        return result;
    }
    result = sandbox3d_game_authoring_set_project_root(
        authoring,
        project_root);
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
            result = sandbox3d_game_authoring_materialize_source(
                project_root,
                authoring->project_engine,
                authoring->project_assets,
                candidate_scene,
                authoring->bindings[index].entity,
                &object,
                &owned_mesh);
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
