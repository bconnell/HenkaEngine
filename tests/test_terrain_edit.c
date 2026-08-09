#include <stdint.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/terrain_edit.h>

#include "../engine/src/core/memory_internal.h"

static int test_deterministic_raise_and_paint(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_world* first = NULL;
    henka_terrain_world* second = NULL;
    henka_terrain_edit_command command = henka_terrain_edit_command_default();
    const henka_terrain_sample* first_samples = NULL;
    const henka_terrain_sample* second_samples = NULL;
    size_t first_count = 0U;
    size_t second_count = 0U;
    henka_terrain_region_state state;
    int result = 0;

    desc.max_resident_regions = 2U;
    if (henka_terrain_world_create(&desc, &first) != HENKA_SUCCESS ||
        henka_terrain_world_create(&desc, &second) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(first, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(second, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    command.operation = HENKA_TERRAIN_EDIT_RAISE;
    command.center_sample_x = 100;
    command.center_sample_z = 100;
    command.radius_samples = 8U;
    command.value_millimeters = 500;
    if (henka_terrain_world_apply_edit(first, &command, 10U) != HENKA_SUCCESS ||
        henka_terrain_world_apply_edit(second, &command, 10U) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(first, (henka_terrain_region_id){0, 0}, &first_samples, &first_count) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(second, (henka_terrain_region_id){0, 0}, &second_samples, &second_count) != HENKA_SUCCESS ||
        first_count != second_count ||
        memcmp(first_samples, second_samples, first_count * sizeof(*first_samples)) != 0 ||
        henka_terrain_world_get_region_state(first, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        state.revision != 1U || !state.dirty)
    {
        goto cleanup;
    }
    command.operation = HENKA_TERRAIN_EDIT_PAINT;
    command.paint_layer = 2U;
    command.paint_strength = 200U;
    if (henka_terrain_world_apply_edit(first, &command, 11U) != HENKA_SUCCESS ||
        henka_terrain_world_apply_edit(second, &command, 11U) != HENKA_SUCCESS ||
        memcmp(first_samples, second_samples, first_count * sizeof(*first_samples)) != 0 ||
        henka_terrain_world_apply_edit(first, &(henka_terrain_edit_command){
            .algorithm_version = 999U,
            .operation = HENKA_TERRAIN_EDIT_RAISE,
            .center_sample_x = 100,
            .center_sample_z = 100,
            .radius_samples = 2U,
            .value_millimeters = 1}, 12U) == HENKA_SUCCESS)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_world_destroy(second);
    henka_terrain_world_destroy(first);
    return result;
}

static int test_allocation_failure_preserves_region(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_edit_command command = henka_terrain_edit_command_default();
    const henka_terrain_sample* samples = NULL;
    henka_terrain_sample* before = NULL;
    henka_terrain_region_state before_state;
    henka_terrain_region_state after_state;
    size_t sample_count = 0U;
    size_t bytes;
    int result = 0;

    desc.max_resident_regions = 1U;
    if (henka_terrain_world_create(&desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(
            world, (henka_terrain_region_id){0, 0}, &samples, &sample_count) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_state(
            world, (henka_terrain_region_id){0, 0}, &before_state) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    bytes = sample_count * sizeof(*before);
    before = henka_malloc(bytes);
    if (before == NULL)
    {
        goto cleanup;
    }
    memcpy(before, samples, bytes);
    command.operation = HENKA_TERRAIN_EDIT_RAISE;
    command.center_sample_x = 64;
    command.center_sample_z = 64;
    command.radius_samples = 4U;
    command.value_millimeters = 1000;
    henka_memory_test_fail_after(0U);
    if (henka_terrain_world_apply_edit(world, &command, 1U) != HENKA_ERROR_OUT_OF_MEMORY)
    {
        henka_memory_test_disable_failures();
        goto cleanup;
    }
    henka_memory_test_disable_failures();
    if (henka_terrain_world_get_region_samples(
            world, (henka_terrain_region_id){0, 0}, &samples, &sample_count) != HENKA_SUCCESS ||
        memcmp(before, samples, bytes) != 0 ||
        henka_terrain_world_get_region_state(
            world, (henka_terrain_region_id){0, 0}, &after_state) != HENKA_SUCCESS ||
        after_state.revision != before_state.revision ||
        after_state.dirty != before_state.dirty)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_memory_test_disable_failures();
    henka_free(before);
    henka_terrain_world_destroy(world);
    return result;
}

int main(void)
{
    return test_deterministic_raise_and_paint() &&
        test_allocation_failure_preserves_region() ? 0 : 1;
}
