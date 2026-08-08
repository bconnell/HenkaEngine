#ifndef HENKA_TERRAIN_EDIT_H
#define HENKA_TERRAIN_EDIT_H

#include <stdint.h>

#include <henka/terrain.h>

#define HENKA_TERRAIN_EDIT_ALGORITHM_VERSION UINT32_C(1)
#define HENKA_TERRAIN_EDIT_MAX_RADIUS_METERS 128U
#define HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS 16U

typedef enum henka_terrain_edit_operation
{
    HENKA_TERRAIN_EDIT_RAISE = 0,
    HENKA_TERRAIN_EDIT_LOWER,
    HENKA_TERRAIN_EDIT_FLATTEN,
    HENKA_TERRAIN_EDIT_SMOOTH,
    HENKA_TERRAIN_EDIT_PAINT
} henka_terrain_edit_operation;

typedef enum henka_terrain_edit_falloff
{
    HENKA_TERRAIN_EDIT_FALLOFF_LINEAR = 0,
    HENKA_TERRAIN_EDIT_FALLOFF_SMOOTH
} henka_terrain_edit_falloff;

typedef struct henka_terrain_edit_command
{
    uint64_t client_nonce;
    uint32_t algorithm_version;
    henka_terrain_edit_operation operation;
    int32_t center_sample_x;
    int32_t center_sample_z;
    uint32_t radius_samples;
    henka_terrain_edit_falloff falloff;
    int32_t value_millimeters;
    uint8_t paint_layer;
    uint8_t paint_strength;
} henka_terrain_edit_command;

henka_terrain_edit_command henka_terrain_edit_command_default(void);
henka_result henka_terrain_edit_command_validate(
    const henka_terrain_world* world,
    const henka_terrain_edit_command* command);
henka_result henka_terrain_edit_get_affected_regions(
    const henka_terrain_world* world,
    const henka_terrain_edit_command* command,
    henka_terrain_region_id* out_regions,
    uint32_t* in_out_region_count);
henka_result henka_terrain_world_apply_edit(
    henka_terrain_world* world,
    const henka_terrain_edit_command* command,
    henka_terrain_revision transaction_id);
henka_result henka_terrain_world_get_region_samples(
    const henka_terrain_world* world,
    henka_terrain_region_id region_id,
    const henka_terrain_sample** out_samples,
    size_t* out_sample_count);

#endif
