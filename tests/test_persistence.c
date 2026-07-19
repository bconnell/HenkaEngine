#include "test_suite.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/persistence.h>

static bool henka_test_write_file(const char* path, const char* content)
{
    FILE* file = NULL;

    if (fopen_s(&file, path, "wb") != 0 || file == NULL)
    {
        return false;
    }

    if (fputs(content, file) < 0)
    {
        fclose(file);
        return false;
    }

    return fclose(file) == 0;
}

void henka_test_persistence(void)
{
    char long_slot_name[66];
    char* parent_directory;
    char* path;
    FILE* file;
    float loaded_pitch;
    henka_vec3 loaded_position;
    float loaded_yaw;
    henka_save_data* loaded_save;
    henka_settings* reloaded;
    henka_result result;
    henka_save_data* save_data;
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
    HENKA_TEST_ASSERT(henka_settings_set_string(settings, "bad=key", "bad") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_settings_set_string(settings, "bad key", "bad") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_settings_set_string(settings, "safe_key", "line1\nline2") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_settings_set_string(settings, "safe_key", "value\twith_tab") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_settings_set_string(NULL, "bad", "bad") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_settings_set_float(settings, "not_finite", NAN) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_settings_set_float(settings, "not_finite", INFINITY) == HENKA_ERROR_INVALID_ARGUMENT);

    HENKA_TEST_ASSERT(henka_settings_set_string(settings, "overflow_int", "2147483648") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_get_int(settings, "overflow_int", 91) == 91);
    HENKA_TEST_ASSERT(henka_settings_set_string(settings, "nan_float", "nan") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_settings_get_float(settings, "nan_float", 3.25f), 3.25f, 0.0001f);

    HENKA_TEST_ASSERT(henka_path_resolve(NULL, NULL, NULL) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_path_resolve("C:/HenkaSandbox3D", "user/sandbox3d.settings", &path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(path, "C:/HenkaSandbox3D/user/sandbox3d.settings") == 0);
    henka_free(path);

    HENKA_TEST_ASSERT(henka_path_resolve("C:/HenkaSandbox3D", "\\\\server\\share\\sandbox3d.settings", &path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(path, "\\\\server\\share\\sandbox3d.settings") == 0);
    henka_free(path);

    HENKA_TEST_ASSERT(henka_path_resolve_confined("C:/HenkaSandbox3D", "user\\sandbox3d.settings", &path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(path, "C:/HenkaSandbox3D/user/sandbox3d.settings") == 0);
    henka_free(path);

    path = NULL;
    HENKA_TEST_ASSERT(henka_path_resolve_confined("C:/HenkaSandbox3D", "../escape.settings", &path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(path == NULL);
    HENKA_TEST_ASSERT(henka_path_resolve_confined("C:/HenkaSandbox3D", "user/../escape.settings", &path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(path == NULL);
    HENKA_TEST_ASSERT(henka_path_resolve_confined("C:/HenkaSandbox3D", "D:/escape.settings", &path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(path == NULL);
    HENKA_TEST_ASSERT(henka_path_resolve_confined("C:/HenkaSandbox3D", "user/CON/settings", &path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(path == NULL);

    HENKA_TEST_ASSERT(henka_path_parent_directory("C:/HenkaSandbox3D/user/sandbox3d.settings", &parent_directory) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(parent_directory, "C:/HenkaSandbox3D/user") == 0);
    henka_free(parent_directory);

    HENKA_TEST_ASSERT(henka_settings_set_bool(settings, "grid_visible", true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_float(settings, "camera_position_x", 2.125f) == HENKA_SUCCESS);
    result = henka_settings_save_file(settings, "build/test_tmp/persistence_roundtrip.settings");
    HENKA_TEST_ASSERT(result == HENKA_SUCCESS);

    file = NULL;
    HENKA_TEST_ASSERT(fopen_s(&file, "build/test_tmp/persistence_roundtrip.settings.henka-tmp", "rb") != 0);
    HENKA_TEST_ASSERT(file == NULL);

    HENKA_TEST_ASSERT(henka_settings_create(&reloaded) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_set_string(reloaded, "old_value", "preserve") == HENKA_SUCCESS);
    result = henka_settings_load_file(reloaded, "build/test_tmp/persistence_roundtrip.settings");
    HENKA_TEST_ASSERT(result == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_settings_has_key(reloaded, "old_value") == false);
    HENKA_TEST_ASSERT(henka_settings_get_bool(reloaded, "grid_visible", false) == true);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(henka_settings_get_float(reloaded, "camera_position_x", 0.0f), 2.125f, 0.0001f);

    HENKA_TEST_ASSERT(henka_test_write_file(
        "build/test_tmp/persistence_malformed.settings",
        "# comment\n"
        "good=true\n"
        "bad_line_without_equals\n"));

    HENKA_TEST_ASSERT(henka_settings_set_string(reloaded, "stable", "unchanged") == HENKA_SUCCESS);
    result = henka_settings_load_file(reloaded, "build/test_tmp/persistence_malformed.settings");
    HENKA_TEST_ASSERT(result == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(strcmp(henka_settings_get_string(reloaded, "stable", ""), "unchanged") == 0);
    HENKA_TEST_ASSERT(henka_settings_has_key(reloaded, "good") == false);
    henka_settings_destroy(reloaded);

    result = henka_settings_load_file(settings, "build/test_tmp/does_not_exist.settings");
    HENKA_TEST_ASSERT(result == HENKA_ERROR_UNKNOWN);

    HENKA_TEST_ASSERT(henka_save_data_create(&save_data) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_save_data_set_scene_id(save_data, "sample_scene") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_save_data_set_scene_id(save_data, "bad\nscene") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_save_data_set_camera_pose(save_data, (henka_vec3){1.0f, 2.0f, 3.0f}, -1.2f, 0.35f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_save_data_set_camera_pose(save_data, (henka_vec3){NAN, 2.0f, 3.0f}, -1.2f, 0.35f) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(henka_save_data_set_flag_bool(save_data, "grid_visible", true) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_save_data_set_flag_bool(save_data, "bad=flag", true) == HENKA_ERROR_INVALID_ARGUMENT);

    HENKA_TEST_ASSERT(henka_save_data_build_slot_path("build/test_tmp", "slot_a", &path) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strstr(path, "saves/slot_a.save") != NULL);
    henka_free(path);

    path = NULL;
    HENKA_TEST_ASSERT(henka_save_data_build_slot_path("build/test_tmp", "../escape", &path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(path == NULL);
    HENKA_TEST_ASSERT(henka_save_data_build_slot_path("build/test_tmp", "folder/slot", &path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(path == NULL);
    HENKA_TEST_ASSERT(henka_save_data_build_slot_path("build/test_tmp", "CON", &path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(path == NULL);
    HENKA_TEST_ASSERT(henka_save_data_build_slot_path("build/test_tmp", "LPT1", &path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(path == NULL);

    memset(long_slot_name, 'a', sizeof(long_slot_name) - 1U);
    long_slot_name[sizeof(long_slot_name) - 1U] = '\0';
    HENKA_TEST_ASSERT(henka_save_data_build_slot_path("build/test_tmp", long_slot_name, &path) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(path == NULL);

    HENKA_TEST_ASSERT(henka_save_data_save_file(save_data, "build/test_tmp/save_roundtrip.save") == HENKA_SUCCESS);

    HENKA_TEST_ASSERT(henka_save_data_create(&loaded_save) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_save_data_set_scene_id(loaded_save, "before_load") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_save_data_set_camera_pose(loaded_save, (henka_vec3){9.0f, 8.0f, 7.0f}, 0.5f, -0.5f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_save_data_load_file(loaded_save, "build/test_tmp/save_roundtrip.save") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(henka_save_data_get_scene_id(loaded_save), "sample_scene") == 0);
    HENKA_TEST_ASSERT(henka_save_data_get_camera_pose(loaded_save, &loaded_position, &loaded_yaw, &loaded_pitch) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(loaded_position.x, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(loaded_yaw, -1.2f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(loaded_pitch, 0.35f, 0.0001f);
    HENKA_TEST_ASSERT(henka_save_data_get_flag_bool(loaded_save, "grid_visible", false) == true);

    HENKA_TEST_ASSERT(henka_test_write_file(
        "build/test_tmp/save_bad_version.save",
        "save.version=99\n"
        "save.scene_id=bad\n"));

    HENKA_TEST_ASSERT(henka_save_data_set_scene_id(loaded_save, "preserved_scene") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_save_data_set_camera_pose(loaded_save, (henka_vec3){4.0f, 5.0f, 6.0f}, 0.25f, -0.25f) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(henka_save_data_load_file(loaded_save, "build/test_tmp/save_bad_version.save") == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(strcmp(henka_save_data_get_scene_id(loaded_save), "preserved_scene") == 0);
    HENKA_TEST_ASSERT(henka_save_data_get_camera_pose(loaded_save, &loaded_position, &loaded_yaw, &loaded_pitch) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(loaded_position.x, 4.0f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(loaded_yaw, 0.25f, 0.0001f);
    HENKA_TEST_ASSERT_FLOAT_CLOSE(loaded_pitch, -0.25f, 0.0001f);

    HENKA_TEST_ASSERT(henka_test_write_file(
        "build/test_tmp/save_bad_float.save",
        "save.version=1\n"
        "save.scene_id=bad_float\n"
        "camera.position.x=nan\n"));

    HENKA_TEST_ASSERT(henka_save_data_load_file(loaded_save, "build/test_tmp/save_bad_float.save") == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(strcmp(henka_save_data_get_scene_id(loaded_save), "preserved_scene") == 0);

    HENKA_TEST_ASSERT(henka_test_write_file(
        "build/test_tmp/save_missing_field.save",
        "save.version=1\n"
        "save.scene_id=missing_field\n"
        "camera.position.x=1.0\n"
        "camera.position.y=2.0\n"
        "camera.position.z=3.0\n"
        "camera.yaw_radians=0.5\n"));

    HENKA_TEST_ASSERT(henka_save_data_load_file(loaded_save, "build/test_tmp/save_missing_field.save") == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(strcmp(henka_save_data_get_scene_id(loaded_save), "preserved_scene") == 0);

    HENKA_TEST_ASSERT(henka_test_write_file(
        "build/test_tmp/save_bad_flag.save",
        "save.version=1\n"
        "save.scene_id=bad_flag\n"
        "camera.position.x=1.0\n"
        "camera.position.y=2.0\n"
        "camera.position.z=3.0\n"
        "camera.yaw_radians=0.5\n"
        "camera.pitch_radians=-0.5\n"
        "flag.grid_visible=maybe\n"));

    HENKA_TEST_ASSERT(henka_save_data_load_file(loaded_save, "build/test_tmp/save_bad_flag.save") == HENKA_ERROR_UNKNOWN);
    HENKA_TEST_ASSERT(strcmp(henka_save_data_get_scene_id(loaded_save), "preserved_scene") == 0);

    henka_save_data_destroy(loaded_save);
    henka_save_data_destroy(save_data);
    henka_settings_destroy(settings);

    remove("build/test_tmp/persistence_roundtrip.settings");
    remove("build/test_tmp/persistence_malformed.settings");
    remove("build/test_tmp/save_roundtrip.save");
    remove("build/test_tmp/save_bad_version.save");
    remove("build/test_tmp/save_bad_float.save");
    remove("build/test_tmp/save_missing_field.save");
    remove("build/test_tmp/save_bad_flag.save");
}
