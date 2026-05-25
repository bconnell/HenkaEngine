#ifndef HENKA_PERSISTENCE_H
#define HENKA_PERSISTENCE_H

#include <stdbool.h>

#include <henka/math.h>
#include <henka/result.h>

typedef struct henka_settings henka_settings;
typedef struct henka_save_data henka_save_data;

henka_result henka_path_resolve(const char* base_path, const char* relative_path, char** out_path);
henka_result henka_path_parent_directory(const char* path, char** out_parent_directory);

henka_result henka_settings_create(henka_settings** out_settings);
void henka_settings_destroy(henka_settings* settings);

henka_result henka_settings_load_file(henka_settings* settings, const char* path);
henka_result henka_settings_save_file(const henka_settings* settings, const char* path);

bool henka_settings_has_key(const henka_settings* settings, const char* key);
const char* henka_settings_get_string(const henka_settings* settings, const char* key, const char* default_value);
int henka_settings_get_int(const henka_settings* settings, const char* key, int default_value);
float henka_settings_get_float(const henka_settings* settings, const char* key, float default_value);
bool henka_settings_get_bool(const henka_settings* settings, const char* key, bool default_value);

henka_result henka_settings_set_string(henka_settings* settings, const char* key, const char* value);
henka_result henka_settings_set_int(henka_settings* settings, const char* key, int value);
henka_result henka_settings_set_float(henka_settings* settings, const char* key, float value);
henka_result henka_settings_set_bool(henka_settings* settings, const char* key, bool value);
henka_result henka_settings_remove(henka_settings* settings, const char* key);

henka_result henka_save_data_build_slot_path(const char* user_data_base_path, const char* slot_name, char** out_path);
henka_result henka_save_data_create(henka_save_data** out_save_data);
void henka_save_data_destroy(henka_save_data* save_data);
int henka_save_data_get_version(const henka_save_data* save_data);
const char* henka_save_data_get_scene_id(const henka_save_data* save_data);
henka_result henka_save_data_set_scene_id(henka_save_data* save_data, const char* scene_id);
henka_result henka_save_data_set_camera_pose(henka_save_data* save_data, henka_vec3 position, float yaw_radians, float pitch_radians);
henka_result henka_save_data_get_camera_pose(
    const henka_save_data* save_data,
    henka_vec3* out_position,
    float* out_yaw_radians,
    float* out_pitch_radians);
henka_result henka_save_data_set_flag_bool(henka_save_data* save_data, const char* key, bool value);
bool henka_save_data_get_flag_bool(const henka_save_data* save_data, const char* key, bool default_value);
henka_result henka_save_data_load_file(henka_save_data* save_data, const char* path);
henka_result henka_save_data_save_file(const henka_save_data* save_data, const char* path);

#endif
