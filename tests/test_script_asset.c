#include <assert.h>
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#include <henka/memory.h>
#include <henka/scene_document.h>
#include <henka/script_asset.h>
#include <henka/script_source.h>

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
    char* loaded_source = NULL;
    size_t source_size;
    size_t loaded_source_size = 0U;
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
    assert(henka_script_asset_read_source(
               ".", hks_path, &loaded_source, &loaded_source_size) == HENKA_SUCCESS);
    assert(loaded_source != NULL && loaded_source_size > 0U);
    assert(loaded_source_size >= strlen("fn OnCreate()") &&
           strstr(loaded_source, "fn OnCreate()") != NULL);
    henka_free(loaded_source);
    loaded_source = NULL;
    assert(henka_script_asset_read_source(
               ".", "../henka_template_escape.lua", &loaded_source,
               &loaded_source_size) != HENKA_SUCCESS);
    assert(loaded_source == NULL);
    (void)remove(lua_path);
    (void)remove(hks_path);
}

static void write_binary_file(const char* path, const char* source)
{
    FILE* file = NULL;
    const size_t source_size = strlen(source);
#if defined(_MSC_VER)
    assert(fopen_s(&file, path, "wb") == 0);
#else
    file = fopen(path, "wb");
    assert(file != NULL);
#endif
    assert(fwrite(source, 1U, source_size, file) == source_size);
    assert(fclose(file) == 0);
}

static void test_source_document_persistence(void)
{
    static const char valid_source[] =
        "function OnCreate()\n"
        "end\n";
    static const char invalid_source[] =
        "function OnCreate(\n"
        "end\n";
    static const char lua_path[] = "test_tmp/source_authoring.lua";
    static const char hks_path[] = "test_tmp/source_authoring.hks";
    const char* loaded_source = NULL;
    char* retained_source = NULL;
    size_t loaded_source_size = 0U;
    henka_script_source_document* document = NULL;
    henka_script_source_document* loaded_document = NULL;
    henka_script_source_diagnostic diagnostic;

    (void)remove(lua_path);
    (void)remove(hks_path);
    write_binary_file(lua_path, "old source\n");
    assert(henka_script_source_create(
               HENKA_SCRIPT_LANGUAGE_LUA, &document) == HENKA_SUCCESS);
    assert(henka_script_source_set_text(
               document, valid_source, strlen(valid_source)) == HENKA_SUCCESS);
    assert(henka_script_asset_save_source_document(
               ".", lua_path, document) == HENKA_SUCCESS);
    assert(!henka_script_source_is_dirty(document));
    assert(henka_script_asset_load_source_document(
               ".", lua_path, &loaded_document) == HENKA_SUCCESS);
    assert(henka_script_source_get_language(loaded_document) == HENKA_SCRIPT_LANGUAGE_LUA);
    assert(!henka_script_source_is_dirty(loaded_document));
    assert(henka_script_source_get_text(
               loaded_document, &loaded_source, &loaded_source_size) == HENKA_SUCCESS);
    assert(loaded_source_size == strlen(valid_source));
    assert(strcmp(loaded_source, valid_source) == 0);
    henka_script_source_destroy(loaded_document);
    loaded_document = NULL;

    assert(henka_script_source_set_text(
               document, invalid_source, strlen(invalid_source)) == HENKA_SUCCESS);
    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_script_source_validate(document, &diagnostic) != HENKA_SUCCESS);
    assert(henka_script_asset_save_source_document(
               ".", lua_path, document) == HENKA_SUCCESS);
    assert(!henka_script_source_is_dirty(document));
    assert(henka_script_asset_save_source_document(
               ".", hks_path, document) == HENKA_ERROR_INVALID_ARGUMENT);
    assert(henka_script_asset_load_source_document(
               ".", "../source_authoring.lua", &loaded_document) != HENKA_SUCCESS);
    assert(loaded_document == NULL);

    assert(henka_script_source_set_text(
               document, valid_source, strlen(valid_source)) == HENKA_SUCCESS);
    write_binary_file(lua_path, "retained destination\n");
    assert(henka_script_asset_save_source_document(
               lua_path, "nested.lua", document) != HENKA_SUCCESS);
    assert(henka_script_source_is_dirty(document));
    retained_source = NULL;
    loaded_source_size = 0U;
    assert(henka_script_asset_read_source(
               ".", lua_path, &retained_source, &loaded_source_size) == HENKA_SUCCESS);
    assert(loaded_source_size == strlen("retained destination\n"));
    assert(strcmp(retained_source, "retained destination\n") == 0);
    henka_free(retained_source);

    henka_script_source_destroy(document);
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
    test_source_document_persistence();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_script_asset_tests: PASS");
    return 0;
}
