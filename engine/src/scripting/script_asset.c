#include <henka/script_asset.h>

#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/persistence.h>
#include <henka/script_backends.h>

#ifndef HENKA_ENABLE_LUA
#define HENKA_ENABLE_LUA 1
#endif

struct henka_script_behavior_asset
{
    henka_script_language language;
    void* backend;
    henka_script_behavior_desc runtime_desc;
};

static bool henka_script_asset_has_suffix(
    const char* path,
    const char* suffix)
{
    const size_t path_length = path == NULL ? 0U : strlen(path);
    const size_t suffix_length = suffix == NULL ? 0U : strlen(suffix);
    return path_length >= suffix_length &&
        suffix_length > 0U &&
        strcmp(path + path_length - suffix_length, suffix) == 0;
}

static henka_result henka_script_asset_read_source(
    const char* project_root,
    const char* relative_path,
    char** out_source,
    size_t* out_source_size)
{
    char* path = NULL;
    char* source = NULL;
    FILE* file = NULL;
    long file_size;
    size_t source_size;
    henka_result result;
    if (out_source != NULL)
    {
        *out_source = NULL;
    }
    if (out_source_size != NULL)
    {
        *out_source_size = 0U;
    }
    if (project_root == NULL || relative_path == NULL ||
        out_source == NULL || out_source_size == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_path_resolve_confined(project_root, relative_path, &path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
#if defined(_MSC_VER)
    if (fopen_s(&file, path, "rb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(path, "rb");
#endif
    henka_free(path);
    if (file == NULL || fseek(file, 0L, SEEK_END) != 0)
    {
        if (file != NULL)
        {
            (void)fclose(file);
        }
        return HENKA_ERROR_ASSET_SOURCE;
    }
    file_size = ftell(file);
    if (file_size < 0L ||
        (unsigned long)file_size > (unsigned long)HENKA_HKS_MAX_SOURCE_BYTES)
    {
        (void)fclose(file);
        return HENKA_ERROR_LIMIT;
    }
    if (fseek(file, 0L, SEEK_SET) != 0)
    {
        (void)fclose(file);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    source_size = (size_t)file_size;
    source = (char*)henka_malloc(source_size == 0U ? 1U : source_size);
    if (source == NULL)
    {
        (void)fclose(file);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (source_size > 0U && fread(source, 1U, source_size, file) != source_size)
    {
        henka_free(source);
        (void)fclose(file);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (fclose(file) != 0)
    {
        henka_free(source);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    *out_source = source;
    *out_source_size = source_size;
    return HENKA_SUCCESS;
}

henka_result henka_script_behavior_asset_create(
    const char* project_root,
    const henka_scene_document_behavior* behavior,
    uint64_t entity_id,
    bool enabled,
    uint32_t instruction_budget,
    henka_script_behavior_asset** out_asset)
{
    henka_script_behavior_asset* asset;
    henka_hks_behavior_backend* hks_backend = NULL;
#if HENKA_ENABLE_LUA
    henka_lua_behavior_backend* lua_backend = NULL;
#endif
    char* source = NULL;
    size_t source_size = 0U;
    henka_result result;
    if (out_asset == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_asset = NULL;
    if (project_root == NULL || behavior == NULL ||
        behavior->id == HENKA_INVALID_SCENE_DOCUMENT_BEHAVIOR_ID ||
        behavior->asset_path[0] == '\0' || entity_id == 0U ||
        instruction_budget == 0U ||
#if !HENKA_ENABLE_LUA
        behavior->language == HENKA_SCRIPT_LANGUAGE_LUA ||
#endif
        (behavior->language != HENKA_SCRIPT_LANGUAGE_LUA &&
         behavior->language != HENKA_SCRIPT_LANGUAGE_HENKASCRIPT) ||
        (behavior->language == HENKA_SCRIPT_LANGUAGE_LUA &&
         !henka_script_asset_has_suffix(behavior->asset_path, ".lua")) ||
        (behavior->language == HENKA_SCRIPT_LANGUAGE_HENKASCRIPT &&
         !henka_script_asset_has_suffix(behavior->asset_path, ".hks")))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_asset_read_source(
        project_root,
        behavior->asset_path,
        &source,
        &source_size);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    asset = (henka_script_behavior_asset*)henka_calloc(1U, sizeof(*asset));
    if (asset == NULL)
    {
        henka_free(source);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    asset->language = behavior->language;
    if (behavior->language == HENKA_SCRIPT_LANGUAGE_LUA)
    {
#if HENKA_ENABLE_LUA
        result = henka_lua_behavior_backend_create(
            source, source_size, &lua_backend, NULL);
        asset->backend = lua_backend;
        asset->runtime_desc.callback = henka_lua_behavior_backend_callback;
#else
        result = HENKA_ERROR_ASSET_SOURCE;
#endif
    }
    else
    {
        result = henka_hks_behavior_backend_create(
            source, source_size, &hks_backend, NULL);
        asset->backend = hks_backend;
        asset->runtime_desc.callback = henka_hks_behavior_backend_callback;
    }
    henka_free(source);
    if (result != HENKA_SUCCESS)
    {
        henka_free(asset);
        return result;
    }
    asset->runtime_desc.entity_id = entity_id;
    asset->runtime_desc.language = behavior->language;
    asset->runtime_desc.enabled = enabled;
    asset->runtime_desc.instruction_budget = instruction_budget;
    asset->runtime_desc.user_data = asset->backend;
    *out_asset = asset;
    return HENKA_SUCCESS;
}

void henka_script_behavior_asset_destroy(henka_script_behavior_asset* asset)
{
    if (asset != NULL)
    {
        if (asset->language == HENKA_SCRIPT_LANGUAGE_LUA)
        {
#if HENKA_ENABLE_LUA
            henka_lua_behavior_backend_destroy(
                (henka_lua_behavior_backend*)asset->backend);
#endif
        }
        else
        {
            henka_hks_behavior_backend_destroy(
                (henka_hks_behavior_backend*)asset->backend);
        }
        henka_free(asset);
    }
}

henka_result henka_script_behavior_asset_get_runtime_desc(
    const henka_script_behavior_asset* asset,
    henka_script_behavior_desc* out_desc)
{
    if (asset == NULL || out_desc == NULL || asset->backend == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_desc = asset->runtime_desc;
    return HENKA_SUCCESS;
}
