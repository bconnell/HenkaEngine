#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#include <henka/memory.h>
#include <henka/scene_document.h>
#include <henka/script_asset.h>

static henka_scene_document_behavior make_behavior(
    henka_script_language language,
    const char* path,
    henka_scene_document_behavior_id id)
{
    henka_scene_document_behavior behavior = henka_scene_document_behavior_default();
    behavior.id = id;
    behavior.language = language;
    (void)snprintf(
        behavior.asset_path,
        sizeof(behavior.asset_path),
        "%s",
        path);
    return behavior;
}

static FILE* open_binary_read(const char* path)
{
    FILE* file = NULL;
#if defined(_MSC_VER)
    if (path == NULL || fopen_s(&file, path, "rb") != 0)
    {
        return NULL;
    }
#else
    file = fopen(path, "rb");
#endif
    return file;
}

static void test_mixed_language_runtime_assets(void)
{
    henka_scene_document_behavior hks_behavior = make_behavior(
        HENKA_SCRIPT_LANGUAGE_HENKASCRIPT,
        "scripts/mixed.hks",
        1U);
    henka_scene_document_behavior lua_behavior = make_behavior(
        HENKA_SCRIPT_LANGUAGE_LUA,
        "scripts/mixed.lua",
        2U);
    henka_script_behavior_asset* hks_asset = NULL;
    henka_script_behavior_asset* lua_asset = NULL;
    henka_script_behavior_desc hks_desc;
    henka_script_behavior_desc lua_desc;
    henka_script_behavior_runtime* runtime = NULL;
    henka_script_behavior_batch_report report;

    assert(henka_script_behavior_asset_create(
               "tests/fixtures",
               &hks_behavior,
               101U,
               true,
               64U,
               &hks_asset) == HENKA_SUCCESS);
    assert(henka_script_behavior_asset_create(
               "tests/fixtures",
               &lua_behavior,
               202U,
               true,
               64U,
               &lua_asset) == HENKA_SUCCESS);
    assert(henka_script_behavior_asset_get_runtime_desc(hks_asset, &hks_desc) == HENKA_SUCCESS);
    assert(henka_script_behavior_asset_get_runtime_desc(lua_asset, &lua_desc) == HENKA_SUCCESS);
    assert(hks_desc.language == HENKA_SCRIPT_LANGUAGE_HENKASCRIPT);
    assert(lua_desc.language == HENKA_SCRIPT_LANGUAGE_LUA);
    assert(henka_script_behavior_runtime_create(&runtime) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_add(runtime, &hks_desc, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    assert(henka_script_behavior_runtime_add(
               runtime, &hks_desc, &(henka_script_behavior_handle){0}) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_add(
               runtime, &lua_desc, &(henka_script_behavior_handle){0}) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_SUCCESS);
    assert(report.attempted == 2U && report.executed == 2U && report.failed == 0U);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_START, 0.0f, 2U, &report) == HENKA_SUCCESS);
    assert(report.attempted == 2U && report.executed == 2U);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_UPDATE, 0.016f, 3U, &report) == HENKA_SUCCESS);
    assert(report.attempted == 2U && report.executed == 2U && report.failed == 0U);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_STOP, 0.0f, 4U, &report) == HENKA_SUCCESS);
    assert(report.attempted == 2U && report.executed == 2U);
    henka_script_behavior_runtime_destroy(runtime);
    henka_script_behavior_asset_destroy(lua_asset);
    henka_script_behavior_asset_destroy(hks_asset);
}

static void test_asset_path_and_source_rejection(void)
{
    henka_scene_document_behavior behavior = make_behavior(
        HENKA_SCRIPT_LANGUAGE_LUA,
        "../scripts/mixed.lua",
        3U);
    henka_script_behavior_asset* asset = NULL;

    assert(henka_script_behavior_asset_create(
               "tests/fixtures", &behavior, 1U, true, 64U, &asset) != HENKA_SUCCESS);
    assert(asset == NULL);
    behavior = make_behavior(HENKA_SCRIPT_LANGUAGE_LUA, "scripts/mixed.hks", 4U);
    assert(henka_script_behavior_asset_create(
               "tests/fixtures", &behavior, 1U, true, 64U, &asset) == HENKA_ERROR_INVALID_ARGUMENT);
    assert(asset == NULL);
    behavior = make_behavior(HENKA_SCRIPT_LANGUAGE_LUA, "scripts/missing.lua", 5U);
    assert(henka_script_behavior_asset_create(
               "tests/fixtures", &behavior, 1U, true, 64U, &asset) == HENKA_ERROR_ASSET_SOURCE);
    assert(asset == NULL);
}

static void test_exclusive_script_templates(void)
{
    static const char lua_path[] = "test_tmp/henka_template_test.lua";
    static const char hks_path[] = "test_tmp/henka_template_test.hks";
    char source[512];
    size_t source_size;
    FILE* file;

    (void)remove(lua_path);
    (void)remove(hks_path);
    assert(henka_script_asset_create_template(
               ".", lua_path, HENKA_SCRIPT_LANGUAGE_LUA) == HENKA_SUCCESS);
    assert(henka_script_asset_create_template(
               ".", lua_path, HENKA_SCRIPT_LANGUAGE_LUA) != HENKA_SUCCESS);
    assert(henka_script_asset_create_template(
               ".", hks_path, HENKA_SCRIPT_LANGUAGE_HENKASCRIPT) == HENKA_SUCCESS);
    assert(henka_script_asset_create_template(
               ".", "test_tmp/invalid.hks", HENKA_SCRIPT_LANGUAGE_LUA) ==
           HENKA_ERROR_INVALID_ARGUMENT);
    assert(henka_script_asset_create_template(
               ".", "../henka_template_escape.lua", HENKA_SCRIPT_LANGUAGE_LUA) !=
           HENKA_SUCCESS);

    file = open_binary_read(lua_path);
    assert(file != NULL);
    source_size = fread(source, 1U, sizeof(source) - 1U, file);
    assert(fclose(file) == 0);
    source[source_size] = '\0';
    assert(strstr(source, "function OnCreate()") != NULL);

    file = open_binary_read(hks_path);
    assert(file != NULL);
    source_size = fread(source, 1U, sizeof(source) - 1U, file);
    assert(fclose(file) == 0);
    source[source_size] = '\0';
    assert(strstr(source, "fn OnCreate()") != NULL);
    (void)remove(lua_path);
    (void)remove(hks_path);
}

int main(void)
{
    const size_t allocations_before = henka_memory_get_allocation_count();
#if defined(_MSC_VER) && defined(_DEBUG)
    (void)_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    (void)_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    test_mixed_language_runtime_assets();
    test_asset_path_and_source_rejection();
    test_exclusive_script_templates();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_script_asset_tests: PASS");
    return 0;
}
