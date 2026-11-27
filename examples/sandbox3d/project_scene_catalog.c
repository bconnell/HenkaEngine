#include "project_scene_catalog.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_filesystem.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#define SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_ABSOLUTE_PATH_BYTES 4096U

typedef struct sandbox3d_project_scene_catalog_directory
{
    char relative_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    size_t depth;
} sandbox3d_project_scene_catalog_directory;

typedef struct sandbox3d_project_scene_catalog_scan
{
    char project_root[SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_ABSOLUTE_PATH_BYTES];
    sandbox3d_project_scene_catalog candidate;
    sandbox3d_project_scene_catalog_directory directories[
        SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_DIRECTORIES];
    size_t directory_count;
    size_t next_directory;
    size_t enumerated_entry_count;
    char current_relative_directory[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    size_t current_depth;
    henka_result failure;
} sandbox3d_project_scene_catalog_scan;

static henka_result sandbox3d_project_scene_catalog_copy_path(
    char* destination,
    size_t destination_capacity,
    const char* source)
{
    const size_t length = source == NULL ? 0U : strlen(source);
    if (destination == NULL || source == NULL || length == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (length >= destination_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }
    memcpy(destination, source, length + 1U);
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_project_scene_catalog_append_component(
    char* destination,
    size_t destination_capacity,
    const char* parent,
    const char* name)
{
    const size_t parent_length = parent == NULL ? 0U : strlen(parent);
    const size_t name_length = name == NULL ? 0U : strlen(name);
    const size_t separator_length = parent_length == 0U ? 0U : 1U;
    size_t offset = 0U;

    if (destination == NULL || name == NULL || name_length == 0U ||
        strchr(name, '/') != NULL || strchr(name, '\\') != NULL ||
        strcmp(name, ".") == 0 || strcmp(name, "..") == 0
#if defined(_WIN32)
        || strchr(name, ':') != NULL
#endif
        )
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (parent_length + separator_length + name_length >=
        destination_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }
    if (parent_length > 0U)
    {
        memcpy(destination, parent, parent_length);
        offset = parent_length;
        destination[offset++] = '/';
    }
    memcpy(destination + offset, name, name_length + 1U);
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_project_scene_catalog_join_directory(
    char* destination,
    size_t destination_capacity,
    const char* project_root,
    const char* relative_path)
{
    const size_t root_length = project_root == NULL ? 0U : strlen(project_root);
    const size_t relative_length = relative_path == NULL
        ? 0U : strlen(relative_path);
    const bool root_has_separator = root_length > 0U &&
        (project_root[root_length - 1U] == '/' ||
         project_root[root_length - 1U] == '\\');
    const size_t separator_length = root_has_separator || relative_length == 0U
        ? 0U : 1U;

    if (destination == NULL || project_root == NULL || root_length == 0U ||
        relative_path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (root_length + separator_length + relative_length >=
        destination_capacity)
    {
        return HENKA_ERROR_LIMIT;
    }
    memcpy(destination, project_root, root_length);
    if (separator_length > 0U)
    {
        destination[root_length] = '/';
    }
    if (relative_length > 0U)
    {
        memcpy(destination + root_length + separator_length,
            relative_path, relative_length);
    }
    destination[root_length + separator_length + relative_length] = '\0';
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_project_scene_catalog_is_reparse_point(
    const char* path,
    bool* out_is_reparse_point)
{
    if (path == NULL || path[0] == '\0' || out_is_reparse_point == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

#if defined(_WIN32)
    {
        wchar_t wide_path[SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_ABSOLUTE_PATH_BYTES];
        const int wide_length = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            path,
            -1,
            wide_path,
            (int)(sizeof(wide_path) / sizeof(wide_path[0])));
        DWORD attributes;

        if (wide_length <= 0)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        attributes = GetFileAttributesW(wide_path);
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            return HENKA_ERROR_PLATFORM;
        }
        *out_is_reparse_point =
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
    }
#else
    {
        struct stat path_status;
        if (lstat(path, &path_status) != 0)
        {
            return HENKA_ERROR_PLATFORM;
        }
        *out_is_reparse_point = S_ISLNK(path_status.st_mode) != 0;
    }
#endif
    return HENKA_SUCCESS;
}

static bool sandbox3d_project_scene_catalog_has_scene_extension(
    const char* path)
{
    static const char suffix[] = ".hscene";
    const size_t path_length = path == NULL ? 0U : strlen(path);
    const size_t suffix_length = sizeof(suffix) - 1U;
    size_t index;

    if (path_length <= suffix_length)
    {
        return false;
    }
    for (index = 0U; index < suffix_length; ++index)
    {
        const unsigned char path_character = (unsigned char)
            path[path_length - suffix_length + index];
        const unsigned char suffix_character = (unsigned char)suffix[index];
        if (tolower(path_character) != tolower(suffix_character))
        {
            return false;
        }
    }
    return true;
}

static SDL_EnumerationResult SDLCALL sandbox3d_project_scene_catalog_visit(
    void* userdata,
    const char* directory_name,
    const char* entry_name)
{
    sandbox3d_project_scene_catalog_scan* scan =
        (sandbox3d_project_scene_catalog_scan*)userdata;
    char full_path[SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_ABSOLUTE_PATH_BYTES];
    char relative_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    SDL_PathInfo path_info;
    bool is_reparse_point = false;

    if (scan == NULL || directory_name == NULL || entry_name == NULL ||
        scan->failure != HENKA_SUCCESS)
    {
        return SDL_ENUM_FAILURE;
    }
    if (entry_name[0] == '\0' || strcmp(entry_name, ".") == 0 ||
        strcmp(entry_name, "..") == 0)
    {
        return SDL_ENUM_CONTINUE;
    }
    if (++scan->enumerated_entry_count >
        SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_ENUMERATED_ENTRIES)
    {
        scan->failure = HENKA_ERROR_LIMIT;
        return SDL_ENUM_FAILURE;
    }

    scan->failure = sandbox3d_project_scene_catalog_append_component(
        relative_path,
        sizeof(relative_path),
        scan->current_relative_directory,
        entry_name);
    if (scan->failure != HENKA_SUCCESS)
    {
        return SDL_ENUM_FAILURE;
    }
    scan->failure = sandbox3d_project_scene_catalog_join_directory(
        full_path,
        sizeof(full_path),
        directory_name,
        entry_name);
    if (scan->failure != HENKA_SUCCESS)
    {
        return SDL_ENUM_FAILURE;
    }

    scan->failure = sandbox3d_project_scene_catalog_is_reparse_point(
        full_path,
        &is_reparse_point);
    if (scan->failure != HENKA_SUCCESS)
    {
        return SDL_ENUM_FAILURE;
    }
    if (is_reparse_point)
    {
        return SDL_ENUM_CONTINUE;
    }
    if (!SDL_GetPathInfo(full_path, &path_info))
    {
        scan->failure = HENKA_ERROR_PLATFORM;
        return SDL_ENUM_FAILURE;
    }

    if (path_info.type == SDL_PATHTYPE_DIRECTORY)
    {
        sandbox3d_project_scene_catalog_directory* child;
        if (scan->current_depth >=
                SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_DEPTH ||
            scan->directory_count >=
                SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_DIRECTORIES)
        {
            scan->failure = HENKA_ERROR_LIMIT;
            return SDL_ENUM_FAILURE;
        }
        child = &scan->directories[scan->directory_count++];
        memcpy(child->relative_path, relative_path, strlen(relative_path) + 1U);
        child->depth = scan->current_depth + 1U;
        return SDL_ENUM_CONTINUE;
    }

    if (path_info.type == SDL_PATHTYPE_FILE &&
        sandbox3d_project_scene_catalog_has_scene_extension(entry_name))
    {
        sandbox3d_project_scene_catalog_entry* scene_entry;
        if (scan->candidate.count >=
            SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_SCENES)
        {
            scan->failure = HENKA_ERROR_LIMIT;
            return SDL_ENUM_FAILURE;
        }
        scene_entry = &scan->candidate.entries[scan->candidate.count++];
        memcpy(scene_entry->relative_path,
            relative_path, strlen(relative_path) + 1U);
    }
    return SDL_ENUM_CONTINUE;
}

static int sandbox3d_project_scene_catalog_compare_entries(
    const void* left,
    const void* right)
{
    const sandbox3d_project_scene_catalog_entry* left_entry =
        (const sandbox3d_project_scene_catalog_entry*)left;
    const sandbox3d_project_scene_catalog_entry* right_entry =
        (const sandbox3d_project_scene_catalog_entry*)right;
    return strcmp(left_entry->relative_path, right_entry->relative_path);
}

henka_result sandbox3d_project_scene_catalog_build(
    const char* project_root,
    sandbox3d_project_scene_catalog* out_catalog)
{
    sandbox3d_project_scene_catalog_scan* scan;
    SDL_PathInfo root_info;
    size_t root_length;
    henka_result result;

    if (project_root == NULL || project_root[0] == '\0' || out_catalog == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    scan = (sandbox3d_project_scene_catalog_scan*)calloc(1U, sizeof(*scan));
    if (scan == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    result = sandbox3d_project_scene_catalog_copy_path(
        scan->project_root,
        sizeof(scan->project_root),
        project_root);
    if (result != HENKA_SUCCESS)
    {
        free(scan);
        return result;
    }
    root_length = strlen(scan->project_root);
    while (root_length > 1U &&
        (scan->project_root[root_length - 1U] == '/' ||
         scan->project_root[root_length - 1U] == '\\') &&
        !(root_length == 3U && scan->project_root[1U] == ':'))
    {
        scan->project_root[--root_length] = '\0';
    }

    if (!SDL_GetPathInfo(scan->project_root, &root_info) ||
        root_info.type != SDL_PATHTYPE_DIRECTORY)
    {
        free(scan);
        return HENKA_ERROR_PLATFORM;
    }

    scan->failure = HENKA_SUCCESS;
    scan->directory_count = 1U;
    scan->directories[0].relative_path[0] = '\0';
    scan->directories[0].depth = 0U;
    while (scan->next_directory < scan->directory_count)
    {
        const sandbox3d_project_scene_catalog_directory* directory =
            &scan->directories[scan->next_directory++];
        char absolute_directory[
            SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_ABSOLUTE_PATH_BYTES];

        result = sandbox3d_project_scene_catalog_join_directory(
            absolute_directory,
            sizeof(absolute_directory),
            scan->project_root,
            directory->relative_path);
        if (result != HENKA_SUCCESS)
        {
            free(scan);
            return result;
        }
        memcpy(scan->current_relative_directory,
            directory->relative_path,
            strlen(directory->relative_path) + 1U);
        scan->current_depth = directory->depth;
        if (!SDL_EnumerateDirectory(
                absolute_directory,
                sandbox3d_project_scene_catalog_visit,
                scan))
        {
            result = scan->failure == HENKA_SUCCESS
                ? HENKA_ERROR_PLATFORM : scan->failure;
            free(scan);
            return result;
        }
    }

    qsort(
        scan->candidate.entries,
        scan->candidate.count,
        sizeof(scan->candidate.entries[0]),
        sandbox3d_project_scene_catalog_compare_entries);
    *out_catalog = scan->candidate;
    free(scan);
    return HENKA_SUCCESS;
}
