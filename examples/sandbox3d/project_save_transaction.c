#include "project_save_transaction.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <henka/memory.h>
#include <henka/persistence.h>

#define SANDBOX3D_PROJECT_SAVE_COPY_BUFFER_BYTES 16384U
#define SANDBOX3D_PROJECT_SAVE_STAGE_ATTEMPTS 128U
#define SANDBOX3D_PROJECT_SAVE_MANIFEST_STAGE_PATH ".henka-save"
#define SANDBOX3D_PROJECT_SAVE_SCENE_STAGE_PREFIX "henka_save_scene_"
#define SANDBOX3D_PROJECT_SAVE_BACKUP_PREFIX "henka_save_backup_"

static henka_result sandbox3d_project_save_path_state(
    const char* path,
    bool* out_exists,
    bool* out_regular_file)
{
    if (path == NULL || out_exists == NULL || out_regular_file == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
#ifdef _WIN32
    {
        DWORD attributes = GetFileAttributesA(path);
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            {
                *out_exists = false;
                *out_regular_file = false;
                return HENKA_SUCCESS;
            }
            return HENKA_ERROR_PLATFORM;
        }
        *out_exists = true;
        *out_regular_file =
            (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0U;
    }
#else
    {
        struct stat information;
        if (lstat(path, &information) != 0)
        {
            if (errno == ENOENT)
            {
                *out_exists = false;
                *out_regular_file = false;
                return HENKA_SUCCESS;
            }
            return HENKA_ERROR_PLATFORM;
        }
        *out_exists = true;
        *out_regular_file = S_ISREG(information.st_mode) != 0;
    }
#endif
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_project_save_reserve_path(const char* path)
{
#ifdef _WIN32
    int descriptor = _open(
        path,
        _O_BINARY | _O_CREAT | _O_EXCL | _O_WRONLY,
        _S_IREAD | _S_IWRITE);
    if (descriptor < 0)
    {
        return errno == EEXIST ? HENKA_ERROR_LIMIT : HENKA_ERROR_PLATFORM;
    }
    if (_close(descriptor) != 0)
    {
        (void)remove(path);
        return HENKA_ERROR_PLATFORM;
    }
#else
    int descriptor = open(path, O_CREAT | O_EXCL | O_WRONLY, 0600);
    if (descriptor < 0)
    {
        return errno == EEXIST ? HENKA_ERROR_LIMIT : HENKA_ERROR_PLATFORM;
    }
    if (close(descriptor) != 0)
    {
        (void)remove(path);
        return HENKA_ERROR_PLATFORM;
    }
#endif
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_project_save_make_sibling_relative_path(
    const char* relative_path,
    const char* prefix,
    uint32_t token,
    const char* suffix,
    char** out_relative_path)
{
    const char* separator;
    size_t directory_length;
    size_t prefix_length;
    size_t suffix_length;
    size_t required;
    int name_length;
    char name[64];
    char* path;

    if (relative_path == NULL || prefix == NULL || suffix == NULL ||
        out_relative_path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_relative_path = NULL;
    separator = strrchr(relative_path, '/');
    {
        const char* windows_separator = strrchr(relative_path, '\\');
        if (windows_separator != NULL &&
            (separator == NULL || windows_separator > separator))
        {
            separator = windows_separator;
        }
    }
    directory_length = separator == NULL
        ? 0U
        : (size_t)(separator - relative_path) + 1U;
    name_length = snprintf(name, sizeof(name), "%s%08x%s", prefix, token, suffix);
    if (name_length < 0 || (size_t)name_length >= sizeof(name))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    prefix_length = directory_length;
    suffix_length = (size_t)name_length;
    if (prefix_length > SIZE_MAX - suffix_length - 1U)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    required = prefix_length + suffix_length + 1U;
    path = (char*)henka_malloc(required);
    if (path == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (directory_length != 0U)
    {
        memcpy(path, relative_path, directory_length);
    }
    memcpy(path + directory_length, name, (size_t)name_length + 1U);
    *out_relative_path = path;
    return HENKA_SUCCESS;
}

static henka_result sandbox3d_project_save_append_suffix(
    const char* path,
    const char* suffix,
    char** out_path)
{
    size_t path_length;
    size_t suffix_length;
    char* result;

    if (path == NULL || suffix == NULL || out_path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_path = NULL;
    path_length = strlen(path);
    suffix_length = strlen(suffix);
    if (path_length > SIZE_MAX - suffix_length - 1U)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    result = (char*)henka_malloc(path_length + suffix_length + 1U);
    if (result == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    memcpy(result, path, path_length);
    memcpy(result + path_length, suffix, suffix_length + 1U);
    *out_path = result;
    return HENKA_SUCCESS;
}

static bool sandbox3d_project_save_paths_equal(
    const char* left,
    const char* right)
{
#ifdef _WIN32
    return left != NULL && right != NULL && _stricmp(left, right) == 0;
#else
    return left != NULL && right != NULL && strcmp(left, right) == 0;
#endif
}

static henka_result sandbox3d_project_save_reserve_scene_stage(
    const char* project_root,
    const char* scene_relative_path,
    char** out_relative_path,
    char** out_absolute_path)
{
    uint32_t token = (uint32_t)(uintptr_t)out_absolute_path ^
        (uint32_t)(uintptr_t)project_root;
    uint32_t attempt;

    if (project_root == NULL || scene_relative_path == NULL ||
        out_relative_path == NULL || out_absolute_path == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_relative_path = NULL;
    *out_absolute_path = NULL;
    for (attempt = 0U; attempt < SANDBOX3D_PROJECT_SAVE_STAGE_ATTEMPTS; ++attempt)
    {
        char* relative_path = NULL;
        char* absolute_path = NULL;
        char* inner_temporary_path = NULL;
        bool exists = false;
        bool regular_file = false;
        henka_result result;

        token += UINT32_C(0x9e3779b9);
        result = sandbox3d_project_save_make_sibling_relative_path(
            scene_relative_path,
            SANDBOX3D_PROJECT_SAVE_SCENE_STAGE_PREFIX,
            token,
            ".stage",
            &relative_path);
        if (result == HENKA_SUCCESS)
        {
            result = henka_path_resolve_confined(
                project_root, relative_path, &absolute_path);
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_project_save_append_suffix(
                absolute_path, ".tmp", &inner_temporary_path);
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_project_save_path_state(
                inner_temporary_path, &exists, &regular_file);
        }
        if (result == HENKA_SUCCESS && exists)
        {
            result = HENKA_ERROR_LIMIT;
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_project_save_path_state(
                absolute_path, &exists, &regular_file);
            if (result == HENKA_SUCCESS && exists)
            {
                result = HENKA_ERROR_LIMIT;
            }
            if (result == HENKA_SUCCESS)
            {
                result = sandbox3d_project_save_reserve_path(absolute_path);
            }
        }
        henka_free(inner_temporary_path);
        if (result == HENKA_ERROR_LIMIT)
        {
            henka_free(absolute_path);
            henka_free(relative_path);
            continue;
        }
        if (result != HENKA_SUCCESS)
        {
            henka_free(absolute_path);
            henka_free(relative_path);
            return result;
        }
        *out_relative_path = relative_path;
        *out_absolute_path = absolute_path;
        return HENKA_SUCCESS;
    }
    return HENKA_ERROR_LIMIT;
}

static henka_result sandbox3d_project_save_reserve_backup(
    const char* project_root,
    const char* scene_relative_path,
    char** out_absolute_path)
{
    uint32_t token = (uint32_t)(uintptr_t)out_absolute_path ^
        (uint32_t)(uintptr_t)scene_relative_path;
    uint32_t attempt;

    *out_absolute_path = NULL;
    for (attempt = 0U; attempt < SANDBOX3D_PROJECT_SAVE_STAGE_ATTEMPTS; ++attempt)
    {
        char* relative_path = NULL;
        char* absolute_path = NULL;
        bool exists = false;
        bool regular_file = false;
        henka_result result;

        token += UINT32_C(0x9e3779b9);
        result = sandbox3d_project_save_make_sibling_relative_path(
            scene_relative_path,
            SANDBOX3D_PROJECT_SAVE_BACKUP_PREFIX,
            token,
            ".stage",
            &relative_path);
        if (result == HENKA_SUCCESS)
        {
            result = henka_path_resolve_confined(
                project_root, relative_path, &absolute_path);
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_project_save_path_state(
                absolute_path, &exists, &regular_file);
        }
        if (result == HENKA_SUCCESS && exists)
        {
            result = HENKA_ERROR_LIMIT;
        }
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_project_save_reserve_path(absolute_path);
        }
        henka_free(relative_path);
        if (result == HENKA_ERROR_LIMIT)
        {
            henka_free(absolute_path);
            continue;
        }
        if (result != HENKA_SUCCESS)
        {
            henka_free(absolute_path);
            return result;
        }
        *out_absolute_path = absolute_path;
        return HENKA_SUCCESS;
    }
    return HENKA_ERROR_LIMIT;
}

static henka_result sandbox3d_project_save_copy_file(
    const char* source_path,
    const char* reserved_destination_path)
{
    FILE* source = NULL;
    FILE* destination = NULL;
    unsigned char buffer[SANDBOX3D_PROJECT_SAVE_COPY_BUFFER_BYTES];
    henka_result result = HENKA_SUCCESS;

#ifdef _WIN32
    if (fopen_s(&source, source_path, "rb") != 0 || source == NULL ||
        fopen_s(&destination, reserved_destination_path, "wb") != 0 ||
        destination == NULL)
#else
    source = fopen(source_path, "rb");
    destination = source == NULL ? NULL : fopen(reserved_destination_path, "wb");
    if (source == NULL || destination == NULL)
#endif
    {
        result = HENKA_ERROR_PLATFORM;
    }
    while (result == HENKA_SUCCESS)
    {
        size_t bytes_read = fread(buffer, 1U, sizeof(buffer), source);
        if (bytes_read != 0U && fwrite(buffer, 1U, bytes_read, destination) != bytes_read)
        {
            result = HENKA_ERROR_PLATFORM;
        }
        if (bytes_read < sizeof(buffer))
        {
            if (ferror(source)) result = HENKA_ERROR_PLATFORM;
            break;
        }
    }
    if (destination != NULL && fflush(destination) != 0)
    {
        result = HENKA_ERROR_PLATFORM;
    }
    if (destination != NULL && result == HENKA_SUCCESS)
    {
#ifdef _WIN32
        if (_commit(_fileno(destination)) != 0)
#else
        if (fsync(fileno(destination)) != 0)
#endif
        {
            result = HENKA_ERROR_PLATFORM;
        }
    }
    if (destination != NULL && fclose(destination) != 0)
    {
        result = HENKA_ERROR_PLATFORM;
    }
    if (source != NULL && fclose(source) != 0)
    {
        result = HENKA_ERROR_PLATFORM;
    }
    if (result != HENKA_SUCCESS)
    {
        (void)remove(reserved_destination_path);
    }
    return result;
}

static henka_result sandbox3d_project_save_restore_scene(
    const char* scene_path,
    const char* backup_path,
    bool had_previous_scene)
{
    if (had_previous_scene)
    {
        return henka_file_atomic_replace(backup_path, scene_path);
    }
    if (remove(scene_path) != 0 && errno != ENOENT)
    {
        return HENKA_ERROR_PLATFORM;
    }
    return HENKA_SUCCESS;
}

henka_result sandbox3d_project_save_transaction(
    const henka_scene_document* candidate_scene,
    const henka_settings* candidate_manifest,
    const char* project_root,
    const char* scene_relative_path,
    const char* manifest_relative_path,
    bool allow_scene_replace)
{
    char* scene_path = NULL;
    char* scene_stage_relative = NULL;
    char* scene_stage_path = NULL;
    char* scene_backup_path = NULL;
    char* manifest_path = NULL;
    char* manifest_stage_path = NULL;
    char* manifest_stage_inner_temp = NULL;
    bool scene_exists = false;
    bool scene_regular = false;
    bool manifest_exists = false;
    bool manifest_regular = false;
    bool manifest_stage_reserved = false;
    bool scene_backup_ready = false;
    bool scene_published = false;
    bool preserve_backup = false;
    henka_result result;

    if (candidate_scene == NULL || candidate_manifest == NULL ||
        project_root == NULL || project_root[0] == '\0' ||
        scene_relative_path == NULL ||
        scene_relative_path[0] == '\0' || manifest_relative_path == NULL ||
        manifest_relative_path[0] == '\0')
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_path_resolve_confined(
        project_root, scene_relative_path, &scene_path);
    if (result == HENKA_SUCCESS)
    {
        result = henka_path_resolve_confined(
            project_root, manifest_relative_path, &manifest_path);
    }
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (sandbox3d_project_save_paths_equal(scene_path, manifest_path))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }

    result = sandbox3d_project_save_path_state(
        scene_path, &scene_exists, &scene_regular);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (scene_exists && !scene_regular)
    {
        result = HENKA_ERROR_PLATFORM;
        goto cleanup;
    }
    if (scene_exists && !allow_scene_replace)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }

    result = henka_path_ensure_parent_directory(scene_path);
    if (result != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    result = sandbox3d_project_save_reserve_scene_stage(
        project_root,
        scene_relative_path,
        &scene_stage_relative,
        &scene_stage_path);
    if (result == HENKA_SUCCESS)
    {
        result = henka_scene_document_save_file(
            candidate_scene, project_root, scene_stage_relative);
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_path_resolve_confined(
            project_root,
            SANDBOX3D_PROJECT_SAVE_MANIFEST_STAGE_PATH,
            &manifest_stage_path);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_project_save_append_suffix(
            manifest_stage_path, ".henka-tmp",
            &manifest_stage_inner_temp);
    }
    if (result == HENKA_SUCCESS &&
        (sandbox3d_project_save_paths_equal(scene_path, manifest_stage_path) ||
         sandbox3d_project_save_paths_equal(scene_path, manifest_stage_inner_temp) ||
         sandbox3d_project_save_paths_equal(scene_stage_path, manifest_path) ||
         sandbox3d_project_save_paths_equal(scene_stage_path, manifest_stage_path) ||
         sandbox3d_project_save_paths_equal(scene_stage_path, manifest_stage_inner_temp)))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        bool exists = false;
        bool regular = false;
        result = sandbox3d_project_save_path_state(
            manifest_stage_inner_temp, &exists, &regular);
        if (result == HENKA_SUCCESS && exists)
        {
            result = HENKA_ERROR_PLATFORM;
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_project_save_reserve_path(manifest_stage_path);
        manifest_stage_reserved = result == HENKA_SUCCESS;
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_save_file(candidate_manifest, manifest_stage_path);
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_project_save_path_state(
            scene_path, &scene_exists, &scene_regular);
    }
    if (result == HENKA_SUCCESS && scene_exists && !scene_regular)
    {
        result = HENKA_ERROR_PLATFORM;
    }
    if (result == HENKA_SUCCESS && scene_exists && !allow_scene_replace)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        result = sandbox3d_project_save_path_state(
            manifest_path, &manifest_exists, &manifest_regular);
    }
    if (result == HENKA_SUCCESS && manifest_exists && !manifest_regular)
    {
        result = HENKA_ERROR_PLATFORM;
    }
    if (result == HENKA_SUCCESS && scene_exists)
    {
        result = sandbox3d_project_save_reserve_backup(
            project_root, scene_relative_path, &scene_backup_path);
        if (result == HENKA_SUCCESS)
        {
            result = sandbox3d_project_save_copy_file(
                scene_path, scene_backup_path);
        }
        if (result == HENKA_SUCCESS)
        {
            scene_backup_ready = true;
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_file_atomic_replace(scene_stage_path, scene_path);
        scene_published = result == HENKA_SUCCESS;
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_file_atomic_replace(manifest_stage_path, manifest_path);
    }
    if (result != HENKA_SUCCESS && scene_published)
    {
        henka_result rollback_result = sandbox3d_project_save_restore_scene(
            scene_path, scene_backup_path, scene_exists);
        if (rollback_result == HENKA_SUCCESS)
        {
            scene_published = false;
        }
        else
        {
            preserve_backup = scene_backup_ready;
            if (scene_backup_ready)
            {
                (void)fprintf(
                    stderr,
                    "Project save failed and scene rollback could not complete; the previous scene is retained at '%s'.\n",
                    scene_backup_path);
            }
            else
            {
                (void)fprintf(
                    stderr,
                    "Project save failed and the new scene could not be removed; the scene file remains at '%s'.\n",
                    scene_path);
            }
        }
    }

cleanup:
    if (scene_stage_path != NULL) (void)remove(scene_stage_path);
    if (manifest_stage_reserved) (void)remove(manifest_stage_path);
    if (scene_backup_path != NULL && !preserve_backup)
    {
        (void)remove(scene_backup_path);
    }
    henka_free(scene_path);
    henka_free(scene_stage_relative);
    henka_free(scene_stage_path);
    henka_free(scene_backup_path);
    henka_free(manifest_path);
    henka_free(manifest_stage_path);
    henka_free(manifest_stage_inner_temp);
    return result;
}
