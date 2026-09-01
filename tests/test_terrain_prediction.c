#include <henka/terrain_prediction.h>

#include "../engine/src/core/memory_internal.h"

static int test_submit_failure_fails_closed_when_rebuild_rollback_fails(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_world* authoritative = NULL;
    henka_terrain_prediction* prediction = NULL;
    henka_terrain_prediction_desc prediction_desc;
    henka_terrain_prediction_stats stats;
    henka_terrain_edit_command command = henka_terrain_edit_command_default();
    const henka_terrain_sample* samples = NULL;
    size_t sample_count = 0U;
    int result = 1;

    desc.max_resident_regions = 2U;
    if (henka_terrain_world_create(&desc, &authoritative) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(authoritative, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    prediction_desc = henka_terrain_prediction_desc_default();
    prediction_desc.authoritative_world = authoritative;
    if (henka_terrain_prediction_create(&prediction_desc, &prediction) != HENKA_SUCCESS ||
        henka_terrain_world_release_region(
            henka_terrain_prediction_get_world(prediction),
            (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    command.client_nonce = 7U;
    command.operation = HENKA_TERRAIN_EDIT_RAISE;
    command.center_sample_x = 2;
    command.center_sample_z = 2;
    command.radius_samples = 1U;
    command.value_millimeters = 1000;
    henka_memory_test_fail_after(0U);
    if (henka_terrain_prediction_submit(prediction, &command) != HENKA_ERROR_ASSET_SOURCE)
    {
        henka_memory_test_disable_failures();
        goto cleanup;
    }
    henka_memory_test_disable_failures();
    henka_terrain_prediction_get_stats(prediction, &stats);
    if (stats.pending_command_count != 0U || stats.prediction_enabled)
    {
        goto cleanup;
    }
    if (henka_terrain_prediction_refresh(prediction) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(
            henka_terrain_prediction_get_world(prediction),
            (henka_terrain_region_id){0, 0}, &samples, &sample_count) != HENKA_SUCCESS ||
        sample_count == 0U)
    {
        goto cleanup;
    }
    henka_terrain_prediction_get_stats(prediction, &stats);
    if (!stats.prediction_enabled)
    {
        goto cleanup;
    }
    result = 0;

cleanup:
    henka_memory_test_disable_failures();
    henka_terrain_prediction_destroy(prediction);
    henka_terrain_world_destroy(authoritative);
    return result;
}

int main(void)
{
    henka_terrain_world_desc desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* authoritative = NULL;
    henka_terrain_prediction* prediction = NULL;
    henka_terrain_prediction_desc prediction_desc;
    henka_terrain_prediction_stats stats;
    henka_terrain_edit_command command = henka_terrain_edit_command_default();
    const henka_terrain_sample* samples = NULL;
    size_t sample_count = 0U;
    size_t center_index;
    int result = 1;

    desc.max_resident_regions = 2U;
    if (henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&desc, &authoritative) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(authoritative, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    prediction_desc = henka_terrain_prediction_desc_default();
    prediction_desc.authoritative_world = authoritative;
    if (henka_terrain_prediction_create(&prediction_desc, &prediction) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    center_index = 2U * layout.samples_per_region_edge + 2U;
    command.client_nonce = 42U;
    command.operation = HENKA_TERRAIN_EDIT_RAISE;
    command.center_sample_x = 2;
    command.center_sample_z = 2;
    command.radius_samples = 1U;
    command.value_millimeters = 1000;
    if (henka_terrain_prediction_submit(prediction, &command) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(
            henka_terrain_prediction_get_world(prediction),
            (henka_terrain_region_id){0, 0}, &samples, &sample_count) != HENKA_SUCCESS ||
        sample_count != layout.samples_per_region || samples[center_index].height_millimeters == 0)
    {
        goto cleanup;
    }
    if (henka_terrain_prediction_reject(prediction, 42U) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(
            henka_terrain_prediction_get_world(prediction),
            (henka_terrain_region_id){0, 0}, &samples, &sample_count) != HENKA_SUCCESS ||
        samples[center_index].height_millimeters != 0)
    {
        goto cleanup;
    }
    if (henka_terrain_prediction_submit(prediction, &command) != HENKA_SUCCESS ||
        henka_terrain_world_apply_edit(authoritative, &command, 1U) != HENKA_SUCCESS ||
        henka_terrain_prediction_accept(prediction, 42U) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(
            henka_terrain_prediction_get_world(prediction),
            (henka_terrain_region_id){0, 0}, &samples, &sample_count) != HENKA_SUCCESS ||
        samples[center_index].height_millimeters != 1000)
    {
        goto cleanup;
    }
    henka_terrain_prediction_get_stats(prediction, &stats);
    if (stats.pending_command_count != 0U || !stats.prediction_enabled ||
        stats.submitted_count != 2U || stats.accepted_count != 1U || stats.rejected_count != 1U)
    {
        goto cleanup;
    }
    result = 0;

cleanup:
    henka_terrain_prediction_destroy(prediction);
    henka_terrain_world_destroy(authoritative);
    if (result != 0)
    {
        return result;
    }
    return test_submit_failure_fails_closed_when_rebuild_rollback_fails() ? 1 : 0;
}
