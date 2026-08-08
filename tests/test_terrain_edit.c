#include <stdint.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/terrain_edit.h>

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

int main(void)
{
    return test_deterministic_raise_and_paint() ? 0 : 1;
}
