#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define TEST_MKDIR(path) _mkdir(path)
#define TEST_RMDIR(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define TEST_MKDIR(path) mkdir(path, 0775)
#define TEST_RMDIR(path) rmdir(path)
#define TEST_GET_PROCESS_ID() getpid()
#endif

#if defined(_WIN32)
#define TEST_GET_PROCESS_ID() _getpid()
#endif

#include "../examples/sandbox3d/project_scene_catalog.h"
#include "../examples/sandbox3d/scene_path_editor.h"

#define TEST_FIXTURE_PATH_CAPACITY 512U
#define TEST_OVERFLOW_SCENES \
    (SANDBOX3D_PROJECT_SCENE_CATALOG_MAX_SCENES + 1U)

static bool test_make_directory(const char* path)
{
    return TEST_MKDIR(path) == 0 || errno == EEXIST;
}

static bool test_write_file(const char* path)
{
    FILE* file = fopen(path, "wb");
    if (file == NULL)
    {
        return false;
    }
    return fclose(file) == 0;
}

static bool test_remove_file(const char* path)
{
    return remove(path) == 0 || errno == ENOENT;
}

static bool test_remove_directory(const char* path)
{
    return TEST_RMDIR(path) == 0 || errno == ENOENT;
}

static bool test_create_unique_fixture_paths(
    char* parent,
    size_t parent_capacity,
    char* project,
    size_t project_capacity,
    char* outside,
    size_t outside_capacity)
{
    const unsigned long process_id = (unsigned long)TEST_GET_PROCESS_ID();
    const unsigned long long timestamp = (unsigned long long)time(NULL);
    unsigned int attempt;

    if (parent == NULL || project == NULL || outside == NULL ||
        parent_capacity == 0U || project_capacity == 0U ||
        outside_capacity == 0U)
    {
        return false;
    }
    parent[0] = '\0';
    project[0] = '\0';
    outside[0] = '\0';
    for (attempt = 0U; attempt < 32U; ++attempt)
    {
        const int parent_length = snprintf(
            parent, parent_capacity,
            "build/test_tmp/project_scene_catalog_%lu_%llu_%u",
            process_id, timestamp, attempt);
        int project_length;
        int outside_length;

        if (parent_length < 0 || (size_t)parent_length >= parent_capacity)
        {
            return false;
        }
        if (TEST_MKDIR(parent) != 0)
        {
            if (errno == EEXIST)
            {
                continue;
            }
            parent[0] = '\0';
            return false;
        }
        project_length = snprintf(
            project, project_capacity, "%s/project", parent);
        outside_length = snprintf(
            outside, outside_capacity, "%s/outside", parent);
        if (project_length < 0 ||
            (size_t)project_length >= project_capacity ||
            outside_length < 0 ||
            (size_t)outside_length >= outside_capacity)
        {
            (void)TEST_RMDIR(parent);
            parent[0] = '\0';
            project[0] = '\0';
            outside[0] = '\0';
            return false;
        }
        return true;
    }
    parent[0] = '\0';
    return false;
}

static bool test_join_path(
    char* output,
    size_t capacity,
    const char* directory,
    const char* name)
{
    int length;
    if (output == NULL || directory == NULL || name == NULL || capacity == 0U)
    {
        return false;
    }
    length = snprintf(output, capacity, "%s/%s", directory, name);
    return length >= 0 && (size_t)length < capacity;
}

static int test_create_directory_link(
    const char* link_path,
    const char* target_path)
{
#if defined(_WIN32)
    wchar_t wide_link[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    wchar_t wide_target[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    const DWORD link_flags = 0x1U | 0x2U;
    if (MultiByteToWideChar(CP_UTF8, 0, link_path, -1,
            wide_link, (int)(sizeof(wide_link) / sizeof(wide_link[0]))) <= 0 ||
        MultiByteToWideChar(CP_UTF8, 0, target_path, -1,
            wide_target, (int)(sizeof(wide_target) / sizeof(wide_target[0]))) <= 0)
    {
        return -1;
    }
    if (CreateSymbolicLinkW(wide_link, wide_target, link_flags) != 0)
    {
        return 1;
    }
    {
        const DWORD error = GetLastError();
        if (error == ERROR_PRIVILEGE_NOT_HELD ||
            error == ERROR_ACCESS_DENIED)
        {
            return 0;
        }
    }
    return -1;
#else
    return symlink(target_path, link_path) == 0 ? 1 : -1;
#endif
}

static bool test_remove_directory_link(const char* path)
{
#if defined(_WIN32)
    wchar_t wide_path[HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1,
            wide_path, (int)(sizeof(wide_path) / sizeof(wide_path[0]))) > 0)
    {
        if (RemoveDirectoryW(wide_path) != 0)
        {
            return true;
        }
        return GetLastError() == ERROR_FILE_NOT_FOUND ||
            GetLastError() == ERROR_PATH_NOT_FOUND;
    }
    return false;
#else
    return unlink(path) == 0 || errno == ENOENT;
#endif
}

int main(void)
{
    sandbox3d_project_scene_catalog catalog = {0};
    sandbox3d_project_scene_catalog preserved = {0};
    sandbox3d_scene_path_editor path_editor = {0};
    char fixture_parent[TEST_FIXTURE_PATH_CAPACITY] = {0};
    char project_root[TEST_FIXTURE_PATH_CAPACITY] = {0};
    char outside_root[TEST_FIXTURE_PATH_CAPACITY] = {0};
    char overflow_root[TEST_FIXTURE_PATH_CAPACITY] = {0};
    char path[TEST_FIXTURE_PATH_CAPACITY] = {0};
    char link_path[TEST_FIXTURE_PATH_CAPACITY] = {0};
    char missing_root[TEST_FIXTURE_PATH_CAPACITY] = {0};
    size_t overflow_scene_count = 0U;
    bool passed = false;
    bool cleanup_succeeded = true;
    bool directory_link_created = false;
    bool reparse_test_exercised = false;

    if (!test_make_directory("build") ||
        !test_make_directory("build/test_tmp") ||
        !test_create_unique_fixture_paths(
            fixture_parent, sizeof(fixture_parent),
            project_root, sizeof(project_root),
            outside_root, sizeof(outside_root)) ||
        !test_join_path(
            overflow_root, sizeof(overflow_root),
            fixture_parent, "overflow") ||
        !test_make_directory(project_root) ||
        !test_make_directory(outside_root) ||
        !test_make_directory(overflow_root))
    {
        fprintf(stderr, "project scene catalog fixture setup failed\n");
        goto cleanup;
    }

    if (!test_join_path(path, sizeof(path), project_root, "Scenes") ||
        !test_make_directory(path) ||
        !test_join_path(path, sizeof(path), project_root, "Nested") ||
        !test_make_directory(path) ||
        !test_join_path(path, sizeof(path), project_root, "zeta.hscene") ||
        !test_write_file(path) ||
        !test_join_path(path, sizeof(path), project_root,
            "Scenes/alpha.hscene") || !test_write_file(path) ||
        !test_join_path(path, sizeof(path), project_root,
            "Nested/Beta.HSCENE") || !test_write_file(path) ||
        !test_join_path(path, sizeof(path), project_root,
            "Scenes/gamma.hscene") || !test_write_file(path) ||
        !test_join_path(path, sizeof(path), project_root,
            "Scenes/ignore.txt") || !test_write_file(path) ||
        !test_join_path(path, sizeof(path), outside_root,
            "escaped.hscene") || !test_write_file(path))
    {
        fprintf(stderr, "project scene catalog fixture content setup failed\n");
        goto cleanup;
    }

    {
        const int link_length = snprintf(
            link_path, sizeof(link_path), "%s/LinkedScenes", project_root);
        int link_result;
        if (link_length < 0 || (size_t)link_length >= sizeof(link_path))
        {
            fprintf(stderr, "project scene catalog reparse path exceeded bound\n");
            goto cleanup;
        }
        link_result = test_create_directory_link(link_path, "../outside");
        if (link_result < 0)
        {
            fprintf(stderr, "project scene catalog reparse fixture setup failed\n");
            goto cleanup;
        }
        directory_link_created = link_result > 0;
        reparse_test_exercised = directory_link_created;
        if (!directory_link_created)
        {
            puts("Project scene catalog: directory-link exclusion not exercised (host policy denied link creation).");
        }
    }

    if (sandbox3d_project_scene_catalog_build(project_root, &catalog) !=
            HENKA_SUCCESS ||
        catalog.count != 4U ||
        strcmp(catalog.entries[0].relative_path,
            "Nested/Beta.HSCENE") != 0 ||
        strcmp(catalog.entries[1].relative_path,
            "Scenes/alpha.hscene") != 0 ||
        strcmp(catalog.entries[2].relative_path,
            "Scenes/gamma.hscene") != 0 ||
        strcmp(catalog.entries[3].relative_path, "zeta.hscene") != 0)
    {
        fprintf(stderr,
            "project scene catalog did not return sorted nested .hscene files without leaving the project root\n");
        goto cleanup;
    }

    if (sandbox3d_scene_path_editor_begin(
            &path_editor,
            SANDBOX3D_SCENE_PATH_EDITOR_MODE_OPEN) != HENKA_SUCCESS ||
        sandbox3d_scene_path_editor_begin(
            &path_editor,
            SANDBOX3D_SCENE_PATH_EDITOR_MODE_OPEN) !=
                HENKA_ERROR_INVALID_ARGUMENT ||
        sandbox3d_scene_path_editor_refresh_catalog(
            &path_editor, project_root) != HENKA_SUCCESS)
    {
        fprintf(stderr, "Open Scene catalog state initialization failed\n");
        goto cleanup;
    }
    {
        const char* visible_path = NULL;
        char first_page_signature[
            HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES * 4U + 160U] = {0};
        char second_page_signature[
            HENKA_SCENE_DOCUMENT_MAX_PATH_BYTES * 4U + 160U] = {0};
        size_t scene_count = 0U;
        size_t page = 0U;
        size_t page_count = 0U;
        henka_result refresh_result = HENKA_ERROR_INVALID_ARGUMENT;
        bool stale = true;
        sandbox3d_scene_path_editor_get_catalog_state(
            &path_editor,
            &scene_count,
            &page,
            &page_count,
            &refresh_result,
            &stale);
        if (scene_count != 4U || page != 0U || page_count != 2U || stale ||
            refresh_result != HENKA_SUCCESS ||
            sandbox3d_scene_path_editor_get_catalog_signature(
                &path_editor,
                first_page_signature,
                sizeof(first_page_signature)) != HENKA_SUCCESS ||
            sandbox3d_scene_path_editor_get_visible_catalog_entry(
                &path_editor, 0U, &visible_path) != HENKA_SUCCESS ||
            visible_path == NULL ||
            strcmp(visible_path, "Nested/Beta.HSCENE") != 0)
        {
            fprintf(stderr,
                "Open Scene catalog did not expose the first sorted page\n");
            goto cleanup;
        }
        path_editor.catalog_page = 1U;
        if (sandbox3d_scene_path_editor_get_catalog_signature(
                &path_editor,
                second_page_signature,
                sizeof(second_page_signature)) != HENKA_SUCCESS ||
            strcmp(first_page_signature, second_page_signature) == 0)
        {
            fprintf(stderr,
                "Open Scene catalog signature did not track page state\n");
            goto cleanup;
        }
        visible_path = NULL;
        if (sandbox3d_scene_path_editor_get_visible_catalog_entry(
                &path_editor, 0U, &visible_path) != HENKA_SUCCESS ||
            visible_path == NULL || strcmp(visible_path, "zeta.hscene") != 0 ||
            sandbox3d_scene_path_editor_get_visible_catalog_entry(
                &path_editor, 1U, &visible_path) != HENKA_ERROR_LIMIT ||
            visible_path != NULL)
        {
            fprintf(stderr,
                "Open Scene catalog pagination exceeded the final page\n");
            goto cleanup;
        }
    }

    for (size_t index = 0U; index < TEST_OVERFLOW_SCENES; ++index)
    {
        const int path_length = snprintf(
            path, sizeof(path), "%s/overflow_%03lu.hscene",
            overflow_root, (unsigned long)index);
        if (path_length < 0 || (size_t)path_length >= sizeof(path))
        {
            fprintf(stderr, "project scene catalog capacity fixture failed\n");
            goto cleanup;
        }
        overflow_scene_count = index + 1U;
        if (!test_write_file(path))
        {
            fprintf(stderr, "project scene catalog capacity fixture failed\n");
            goto cleanup;
        }
    }

    preserved.count = 1U;
    memcpy(preserved.entries[0].relative_path,
        "keep.hscene", sizeof("keep.hscene"));
    catalog = preserved;
    if (sandbox3d_project_scene_catalog_build(overflow_root, &catalog) !=
            HENKA_ERROR_LIMIT ||
        catalog.count != preserved.count ||
        strcmp(catalog.entries[0].relative_path,
            preserved.entries[0].relative_path) != 0)
    {
        fprintf(stderr,
            "over-capacity project scene catalog scan was not bounded and atomic\n");
        goto cleanup;
    }

    catalog = preserved;
    if (!test_join_path(missing_root, sizeof(missing_root),
            fixture_parent, "missing") ||
        sandbox3d_project_scene_catalog_build(missing_root, &catalog) !=
            HENKA_ERROR_PLATFORM ||
        catalog.count != preserved.count ||
        strcmp(catalog.entries[0].relative_path,
            preserved.entries[0].relative_path) != 0)
    {
        fprintf(stderr,
            "failed project scene catalog scan partially replaced its output\n");
        goto cleanup;
    }

    {
        const char* const invalid_roots[] = {NULL, ""};
        size_t invalid_root_index;
        for (invalid_root_index = 0U;
             invalid_root_index <
                sizeof(invalid_roots) / sizeof(invalid_roots[0]);
             ++invalid_root_index)
        {
            const char* preserved_path = NULL;
            size_t scene_count = 0U;
            size_t page = 0U;
            henka_result refresh_result = HENKA_SUCCESS;
            bool stale = false;
            if (sandbox3d_scene_path_editor_refresh_catalog(
                    &path_editor, invalid_roots[invalid_root_index]) !=
                HENKA_ERROR_INVALID_ARGUMENT)
            {
                fprintf(stderr,
                    "missing Open Scene project root was not reported as a failed refresh\n");
                goto cleanup;
            }
            sandbox3d_scene_path_editor_get_catalog_state(
                &path_editor,
                &scene_count,
                &page,
                NULL,
                &refresh_result,
                &stale);
            if (scene_count != 4U || page != 1U || !stale ||
                refresh_result != HENKA_ERROR_INVALID_ARGUMENT ||
                sandbox3d_scene_path_editor_get_visible_catalog_entry(
                    &path_editor, 0U, &preserved_path) != HENKA_SUCCESS ||
                preserved_path == NULL ||
                strcmp(preserved_path, "zeta.hscene") != 0)
            {
                fprintf(stderr,
                    "missing-root refresh did not mark and preserve the prior page\n");
                goto cleanup;
            }
        }
    }

    if (sandbox3d_scene_path_editor_refresh_catalog(
            &path_editor, missing_root) != HENKA_ERROR_PLATFORM)
    {
        fprintf(stderr, "failed Open Scene catalog refresh was not reported\n");
        goto cleanup;
    }
    {
        const char* preserved_path = NULL;
        size_t scene_count = 0U;
        size_t page = 0U;
        henka_result refresh_result = HENKA_SUCCESS;
        bool stale = false;
        sandbox3d_scene_path_editor_get_catalog_state(
            &path_editor,
            &scene_count,
            &page,
            NULL,
            &refresh_result,
            &stale);
        if (scene_count != 4U || page != 1U || !stale ||
            refresh_result != HENKA_ERROR_PLATFORM ||
            sandbox3d_scene_path_editor_get_visible_catalog_entry(
                &path_editor, 0U, &preserved_path) != HENKA_SUCCESS ||
            preserved_path == NULL ||
            strcmp(preserved_path, "zeta.hscene") != 0)
        {
            fprintf(stderr,
                "failed refresh did not preserve the last complete catalog\n");
            goto cleanup;
        }
    }

    if (sandbox3d_scene_path_editor_refresh_catalog(
            &path_editor, project_root) != HENKA_SUCCESS)
    {
        fprintf(stderr, "Open Scene catalog did not recover after refresh failure\n");
        goto cleanup;
    }
    {
        size_t scene_count = 0U;
        size_t page = 1U;
        henka_result refresh_result = HENKA_ERROR_PLATFORM;
        bool stale = true;
        sandbox3d_scene_path_editor_get_catalog_state(
            &path_editor,
            &scene_count,
            &page,
            NULL,
            &refresh_result,
            &stale);
        if (scene_count != 4U || page != 0U || stale ||
            refresh_result != HENKA_SUCCESS)
        {
            fprintf(stderr,
                "successful Open Scene refresh did not restore current catalog state\n");
            goto cleanup;
        }
    }

    passed = true;

cleanup:
    sandbox3d_scene_path_editor_reset(&path_editor);
    if (directory_link_created && link_path[0] != '\0')
    {
        cleanup_succeeded = test_remove_directory_link(link_path) &&
            cleanup_succeeded;
    }
    if (project_root[0] != '\0')
    {
        if (test_join_path(path, sizeof(path), project_root, "zeta.hscene"))
        {
            cleanup_succeeded = test_remove_file(path) && cleanup_succeeded;
        }
        if (test_join_path(path, sizeof(path), project_root,
                "Scenes/alpha.hscene"))
        {
            cleanup_succeeded = test_remove_file(path) && cleanup_succeeded;
        }
        if (test_join_path(path, sizeof(path), project_root,
                "Nested/Beta.HSCENE"))
        {
            cleanup_succeeded = test_remove_file(path) && cleanup_succeeded;
        }
        if (test_join_path(path, sizeof(path), project_root,
                "Scenes/gamma.hscene"))
        {
            cleanup_succeeded = test_remove_file(path) && cleanup_succeeded;
        }
        if (test_join_path(path, sizeof(path), project_root,
                "Scenes/ignore.txt"))
        {
            cleanup_succeeded = test_remove_file(path) && cleanup_succeeded;
        }
        if (test_join_path(path, sizeof(path), project_root, "Scenes"))
        {
            cleanup_succeeded = test_remove_directory(path) && cleanup_succeeded;
        }
        if (test_join_path(path, sizeof(path), project_root, "Nested"))
        {
            cleanup_succeeded = test_remove_directory(path) && cleanup_succeeded;
        }
        cleanup_succeeded = test_remove_directory(project_root) &&
            cleanup_succeeded;
    }
    if (outside_root[0] != '\0')
    {
        if (test_join_path(path, sizeof(path), outside_root,
                "escaped.hscene"))
        {
            cleanup_succeeded = test_remove_file(path) && cleanup_succeeded;
        }
        cleanup_succeeded = test_remove_directory(outside_root) &&
            cleanup_succeeded;
    }
    if (overflow_root[0] != '\0')
    {
        for (size_t index = 0U; index < overflow_scene_count; ++index)
        {
            const int path_length = snprintf(path, sizeof(path),
                "%s/overflow_%03lu.hscene",
                overflow_root, (unsigned long)index);
            cleanup_succeeded = path_length >= 0 &&
                (size_t)path_length < sizeof(path) &&
                test_remove_file(path) && cleanup_succeeded;
        }
        cleanup_succeeded = test_remove_directory(overflow_root) &&
            cleanup_succeeded;
    }
    if (fixture_parent[0] != '\0')
    {
        cleanup_succeeded = test_remove_directory(fixture_parent) &&
            cleanup_succeeded;
    }
    if (!cleanup_succeeded)
    {
        fprintf(stderr,
            "project scene catalog unique fixture cleanup was incomplete\n");
    }
    if (!passed || !cleanup_succeeded)
    {
        return 1;
    }
    printf("Project scene catalog: nested filtering, deterministic order, reparse exclusion %s, capacity bounds, and failure atomicity passed.\n",
        reparse_test_exercised ? "exercised" : "not exercised (host policy denied link creation)");
    return 0;
}
