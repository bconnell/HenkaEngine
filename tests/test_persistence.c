#include "test_suite.h"

#include <string.h>
#include <windows.h>

#include <henka/memory.h>
#include <henka/persistence.h>

void henka_test_persistence(void)
{
    char* path;
    char* parent_directory;
    FILE* file;
    henka_result result;
    henka_settings* reloaded;
    henka_settings* settings;

    HENKA_TEST_ASSERT(henka_settings_create(NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_settings_create(&settings) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_has_key(settings, "grid_visible") == false);
    HENKA_TEST_ASSERT(strcmp(henka_settings_get_string(settings, "missing", "default"), "default") == 0);
    HENKA_TEST_ASSERT(henka_settings_get_int(settings, "missing_int", 7) == 7);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_settings_get_float(settings, "missing_float", 1.5f), 1.5f, 0.0001f);
    HENKA_TEST_ASSERT(henka_settings_get_bool(settings, "missing_bool", true) == true);

    HENKA_TEST_ASSERT(henka_settings_set_string(settings, "grid_visible", "true") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_int(settings, "window_width", 1280) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_float(settings, "camera_yaw_radians", -1.25f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_bool(settings, "wireframe_enabled", false) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(henka_settings_get_string(settings, "grid_visible", ""), "true") == 0);
    HENKA_TEST_ASSERT(henka_settings_get_int(settings, "window_width", 0) == 1280);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_settings_get_float(settings, "camera_yaw_radians", 0.0f), -1.25f, 0.0001f);
    HENKA_TEST_ASSERT(henka_settings_get_bool(settings, "wireframe_enabled", true) == false);

    HENKA_TEST_ASSERT(henka_settings_set_string(settings, "grid_visible", "false") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(henka_settings_get_string(settings, "grid_visible", ""), "false") == 0);
    HENKA_TEST_ASSERT(henka_settings_remove(settings, "grid_visible") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_has_key(settings, "grid_visible") == false);

    HENKA_TEST_ASSERT(henka_settings_set_string(settings, "", "bad") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_settings_set_string(NULL, "bad", "bad") == HENKA_ERROR_INVALID_ARGUMENT);

    HENKA_TEST_ASSERT(henka_path_resolve(NULL, NULL, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_path_resolve("C:/HenkaSandbox3D", "user/sandbox3d.settings", &path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(path, "C:/HenkaSandbox3D/user/sandbox3d.settings") == 0);
    henka_free(path);

    HENKA_TEST_ASSERT(henka_path_resolve("C:/HenkaSandbox3D/", "user/sandbox3d.settings", &path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(path, "C:/HenkaSandbox3D/user/sandbox3d.settings") == 0);
    henka_free(path);

    HENKA_TEST_ASSERT(henka_path_resolve("", "user/sandbox3d.settings", &path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(path, "user/sandbox3d.settings") == 0);
    henka_free(path);

    HENKA_TEST_ASSERT(henka_path_resolve("C:/HenkaSandbox3D", "\\\\server\\share\\sandbox3d.settings", &path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(path, "\\\\server\\share\\sandbox3d.settings") == 0);
    henka_free(path);

    HENKA_TEST_ASSERT(henka_path_parent_directory("C:/HenkaSandbox3D/user/sandbox3d.settings", &parent_directory) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(parent_directory, "C:/HenkaSandbox3D/user") == 0);
    henka_free(parent_directory);

    HENKA_TEST_ASSERT(henka_settings_set_bool(settings, "grid_visible", true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_float(settings, "camera_position_x", 2.125f) == HENKA_SUCCESS);
    result = henka_settings_save_file(settings, "build/test_tmp/persistence_roundtrip.settings");
    HENKA_TEST_ASSERT(result == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_settings_create(&reloaded) == HENKA_SUCCESS);
    result = henka_settings_load_file(reloaded, "build/test_tmp/persistence_roundtrip.settings");
    HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_get_bool(reloaded, "grid_visible", false) == true);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_settings_get_float(reloaded, "camera_position_x", 0.0f), 2.125f, 0.0001f);
    henka_settings_destroy(reloaded);

    file = NULL;
    HENKA_TEST_ASSERT(fopen_s(&file, "build/test_tmp/persistence_malformed.settings", "w") == 0);
    fputs("# comment\n", file);
    fputs("good=true\n", file);
    fputs("bad_line_without_equals\n", file);
    fputs("value_only=\n", file);
    fclose(file);

    HENKA_TEST_ASSERT(henka_settings_create(&reloaded) == HENKA_SUCCESS);
    result = henka_settings_load_file(reloaded, "build/test_tmp/persistence_malformed.settings");
    HENKA_TEST_ASSERT(result == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(henka_settings_get_bool(reloaded, "good", false) == true);
    HENKA_TEST_ASSERT(strcmp(henka_settings_get_string(reloaded, "value_only", "missing"), "") == 0);
    henka_settings_destroy(reloaded);

    result = henka_settings_load_file(settings, "build/test_tmp/does_not_exist.settings");
    HENKA_TEST_ASSERT(result == HENKA_ERROR_UNKNOWN);

    DeleteFileA("build/test_tmp/persistence_roundtrip.settings");
    DeleteFileA("build/test_tmp/persistence_malformed.settings");

    henka_settings_destroy(settings);
}
