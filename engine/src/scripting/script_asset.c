#include <henka/script_asset.h>

#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/henkascript.h>
#include <henka/persistence.h>
#include <henka/script_backends.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <stdatomic.h>
#endif

#ifndef HENKA_ENABLE_LUA
#define HENKA_ENABLE_LUA 1
#endif

struct henka_script_behavior_asset
{
    henka_script_language language;
    void* backend;
    henka_script_behavior_desc runtime_desc;
};

#define HENKA_SCRIPT_ASSET_TEMPORARY_SUFFIX_BYTES 40U

#if defined(_WIN32)
static volatile LONG g_henka_script_asset_save_sequence = 0L;
#else
static atomic_uint g_henka_script_asset_save_sequence = 0U;
#endif

static uint32_t henka_script_asset_next_save_sequence(void)
{
#if defined(_WIN32)
    return (uint32_t)InterlockedIncrement(&g_henka_script_asset_save_sequence);
#else
    return atomic_fetch_add_explicit(
        &g_henka_script_asset_save_sequence,
        1U,
        memory_order_relaxed) + 1U;
#endif
}

static bool henka_script_asset_has_suffix(
    const char* path,
    const char* suffix)
{
    const size_t path_length = path == NULL ? 0U : strlen(path);
    const size_t suffix_length = suffix == NULL ? 0U : strlen(suffix);
    return path_length >= suffix_length && suffix_length > 0U &&
        strcmp(path + path_length - suffix_length, suffix) == 0;
}

static henka_script_language henka_script_asset_language_for_path(
    const char* relative_path)
{
    if (henka_script_asset_has_suffix(relative_path, ".lua"))
    {
        return HENKA_SCRIPT_LANGUAGE_LUA;
    }
    if (henka_script_asset_has_suffix(relative_path, ".hks"))
    {
        return HENKA_SCRIPT_LANGUAGE_HENKASCRIPT;
    }
    return HENKA_SCRIPT_LANGUAGE_NONE;
}

static henka_result henka_script_asset_make_temporary_relative_path(
    const char* relative_path,
    char** out_relative_path)
{
    const size_t relative_length = relative_path == NULL ? 0U : strlen(relative_path);
    char* temporary_relative_path;
    int written;
    if (out_relative_path == NULL || relative_path == NULL ||
        relative_length > SIZE_MAX - HENKA_SCRIPT_ASSET_TEMPORARY_SUFFIX_BYTES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    temporary_relative_path = (char*)henka_malloc(
        relative_length + HENKA_SCRIPT_ASSET_TEMPORARY_SUFFIX_BYTES);
    if (temporary_relative_path == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    written = snprintf(
        temporary_relative_path,
        relative_length + HENKA_SCRIPT_ASSET_TEMPORARY_SUFFIX_BYTES,
        "%s.tmp.%lu",
        relative_path,
        (unsigned long)henka_script_asset_next_save_sequence());
    if (written < 0 ||
        (size_t)written >= relative_length + HENKA_SCRIPT_ASSET_TEMPORARY_SUFFIX_BYTES)
    {
        henka_free(temporary_relative_path);
        return HENKA_ERROR_LIMIT;
    }
    *out_relative_path = temporary_relative_path;
    return HENKA_SUCCESS;
}

static bool henka_script_asset_atomic_replace(
    const char* temporary_path,
    const char* path)
{
#if defined(_WIN32)
    return temporary_path != NULL && path != NULL &&
        MoveFileExA(
            temporary_path,
            path,
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return temporary_path != NULL && path != NULL &&
        rename(temporary_path, path) == 0;
#endif
}

henka_result henka_script_asset_create_template(
    const char* project_root,
    const char* relative_path,
    henka_script_language language)
{
    static const char lua_source[] =
        "-- Henka Lua V1 behavior template.\n"
        "function OnCreate()\n"
        "end\n\n"
        "function OnStart()\n"
        "end\n\n"
        "function OnUpdate()\n"
        "end\n";
    const char* source = NULL;
    size_t source_size = 0U;
    char* path = NULL;
    FILE* file = NULL;
    henka_result result;
    if (project_root == NULL || project_root[0] == '\0' ||
        relative_path == NULL || relative_path[0] == '\0' ||
        (language != HENKA_SCRIPT_LANGUAGE_LUA &&
         language != HENKA_SCRIPT_LANGUAGE_HENKASCRIPT) ||
        (language == HENKA_SCRIPT_LANGUAGE_LUA &&
         !henka_script_asset_has_suffix(relative_path, ".lua")) ||
        (language == HENKA_SCRIPT_LANGUAGE_HENKASCRIPT &&
         !henka_script_asset_has_suffix(relative_path, ".hks")))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_path_resolve_confined(project_root, relative_path, &path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_path_ensure_parent_directory(path);
    if (result != HENKA_SUCCESS)
    {
        henka_free(path);
        return result;
    }
    if (language == HENKA_SCRIPT_LANGUAGE_LUA)
    {
        source = lua_source;
        source_size = strlen(source);
    }
    else if (henka_hks_get_default_behavior_source(
                 &source,
                 &source_size) != HENKA_SUCCESS)
    {
        henka_free(path);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
#if defined(_MSC_VER)
    if (fopen_s(&file, path, "wbx") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(path, "wbx");
#endif
    if (file == NULL)
    {
        henka_free(path);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    if (fwrite(source, 1U, source_size, file) != source_size ||
        fflush(file) != 0 ||
        fclose(file) != 0)
    {
        (void)remove(path);
        henka_free(path);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    henka_free(path);
    return HENKA_SUCCESS;
}

henka_result henka_script_asset_read_source(
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
    if (project_root == NULL || project_root[0] == '\0' ||
        relative_path == NULL || relative_path[0] == '\0' ||
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
    source = (char*)henka_calloc(source_size + 1U, sizeof(*source));
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
    source[source_size] = '\0';
    if (fclose(file) != 0)
    {
        henka_free(source);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    *out_source = source;
    *out_source_size = source_size;
    return HENKA_SUCCESS;
}

henka_result henka_script_asset_load_source_document(
    const char* project_root,
    const char* relative_path,
    henka_script_source_document** out_document)
{
    henka_script_language language;
    henka_script_source_document* document = NULL;
    char* source = NULL;
    size_t source_size = 0U;
    henka_result result;
    if (out_document == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_document = NULL;
    language = henka_script_asset_language_for_path(relative_path);
    if (project_root == NULL || project_root[0] == '\0' ||
        relative_path == NULL || relative_path[0] == '\0' ||
        language == HENKA_SCRIPT_LANGUAGE_NONE)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_asset_read_source(
        project_root,
        relative_path,
        &source,
        &source_size);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_script_source_create(language, &document);
    if (result == HENKA_SUCCESS)
    {
        result = henka_script_source_set_text(document, source, source_size);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_script_source_mark_clean(document);
    }
    henka_free(source);
    if (result != HENKA_SUCCESS)
    {
        henka_script_source_destroy(document);
        return result;
    }
    *out_document = document;
    return HENKA_SUCCESS;
}

henka_result henka_script_asset_save_source_document(
    const char* project_root,
    const char* relative_path,
    henka_script_source_document* document)
{
    const char* source = NULL;
    char* path = NULL;
    char* temporary_relative_path = NULL;
    char* temporary_path = NULL;
    size_t source_size = 0U;
    henka_script_language language;
    FILE* file = NULL;
    henka_result result;
    int close_result;
    if (project_root == NULL || project_root[0] == '\0' ||
        relative_path == NULL || relative_path[0] == '\0' || document == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    language = henka_script_source_get_language(document);
    if (language == HENKA_SCRIPT_LANGUAGE_NONE ||
        henka_script_asset_language_for_path(relative_path) != language ||
        henka_script_source_get_text(document, &source, &source_size) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_path_resolve_confined(project_root, relative_path, &path);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_script_asset_make_temporary_relative_path(
        relative_path,
        &temporary_relative_path);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    result = henka_path_resolve_confined(
        project_root,
        temporary_relative_path,
        &temporary_path);
    if (result != HENKA_SUCCESS ||
        henka_path_ensure_parent_directory(path) != HENKA_SUCCESS)
    {
        result = HENKA_ERROR_ASSET_SOURCE;
        goto cleanup;
    }
#if defined(_MSC_VER)
    if (fopen_s(&file, temporary_path, "wb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(temporary_path, "wb");
#endif
    if (file == NULL)
    {
        result = HENKA_ERROR_ASSET_SOURCE;
        goto cleanup;
    }
    if ((source_size > 0U && fwrite(source, 1U, source_size, file) != source_size) ||
        fflush(file) != 0)
    {
        result = HENKA_ERROR_ASSET_SOURCE;
        goto cleanup;
    }
    close_result = fclose(file);
    file = NULL;
    if (close_result != 0 || !henka_script_asset_atomic_replace(temporary_path, path))
    {
        result = HENKA_ERROR_ASSET_SOURCE;
        goto cleanup;
    }
    result = henka_script_source_mark_clean(document);

cleanup:
    if (file != NULL)
    {
        (void)fclose(file);
    }
    if (result != HENKA_SUCCESS && temporary_path != NULL)
    {
        (void)remove(temporary_path);
    }
    henka_free(temporary_path);
    henka_free(temporary_relative_path);
    henka_free(path);
    return result;
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
