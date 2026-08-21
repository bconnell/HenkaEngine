#ifndef HENKA_SCRIPT_STATE_H
#define HENKA_SCRIPT_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/result.h>

#define HENKA_SCRIPT_STATE_FORMAT_VERSION UINT32_C(1)
#define HENKA_SCRIPT_STATE_MAX_VALUES 512U
#define HENKA_SCRIPT_STATE_MAX_FILE_BYTES (64U * 1024U)

typedef struct henka_script_state_store henka_script_state_store;

typedef struct henka_script_state_identity
{
    uint64_t entity_id;
    uint64_t behavior_id;
} henka_script_state_identity;

typedef enum henka_script_state_value_type
{
    HENKA_SCRIPT_STATE_VALUE_NONE = 0,
    HENKA_SCRIPT_STATE_VALUE_BOOL,
    HENKA_SCRIPT_STATE_VALUE_I32,
    HENKA_SCRIPT_STATE_VALUE_FLOAT32,
    HENKA_SCRIPT_STATE_VALUE_VEC3
} henka_script_state_value_type;

typedef struct henka_script_state_value
{
    henka_script_state_value_type type;
    union
    {
        bool boolean;
        int32_t i32;
        float f32;
        henka_vec3 vec3;
    } as;
} henka_script_state_value;

henka_result henka_script_state_store_create(
    henka_script_state_store** out_store);
void henka_script_state_store_destroy(
    henka_script_state_store* store);
void henka_script_state_store_clear(
    henka_script_state_store* store);

henka_result henka_script_state_store_set(
    henka_script_state_store* store,
    henka_script_state_identity identity,
    uint32_t key,
    henka_script_state_value value);
henka_result henka_script_state_store_get(
    const henka_script_state_store* store,
    henka_script_state_identity identity,
    uint32_t key,
    henka_script_state_value* out_value,
    bool* out_present);
henka_result henka_script_state_store_remove(
    henka_script_state_store* store,
    henka_script_state_identity identity,
    uint32_t key);
size_t henka_script_state_store_get_count(
    const henka_script_state_store* store);

/* relative_path is confined beneath project_root. Loads are candidate-based;
 * malformed input leaves the existing store unchanged. Saves use a bounded
 * temporary file and atomic replacement. */
henka_result henka_script_state_store_load_file(
    henka_script_state_store* store,
    const char* project_root,
    const char* relative_path);
henka_result henka_script_state_store_save_file(
    const henka_script_state_store* store,
    const char* project_root,
    const char* relative_path);

#endif
