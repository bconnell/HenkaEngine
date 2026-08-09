#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <henka/memory.h>
#include <henka/terrain_streaming.h>

static int test_streaming(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_terrain_streamer* streamer = NULL;
    henka_terrain_stream_desc stream_desc = henka_terrain_stream_desc_default();
    henka_terrain_sample* samples = NULL;
    henka_terrain_stream_observer observer = {1U, {2, 3}, 0U, 1U, 1U, 0U};
    henka_terrain_stream_observer moved_observer = {1U, {4, 3}, 0U, 1U, 1U, 0U};
    henka_terrain_stream_stats stats;
    henka_terrain_region_state region_state;
    size_t allocations_before_failed_request;
    uint32_t index;
    int result = 0;

    world_desc.max_resident_regions = 2U;
    if (henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_storage_create(&world_desc, "build/test_tmp/terrain_streaming_v1", &storage) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
    if (samples == NULL)
    {
        goto cleanup;
    }
    for (index = 0U; index < layout.samples_per_region; ++index)
    {
        samples[index].height_millimeters = 1234;
        samples[index].material_weights[0] = 255U;
    }
    if (henka_terrain_storage_begin(storage, 1U) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(storage, observer.center_region, 8U, 2U, samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(storage, moved_observer.center_region, 9U, 3U, samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_storage_commit(storage, 1U) != HENKA_SUCCESS ||
        henka_terrain_streamer_create(world, storage, &stream_desc, &streamer) != HENKA_SUCCESS ||
        henka_terrain_streamer_add_observer(streamer, &observer) != HENKA_SUCCESS ||
        henka_terrain_streamer_request_region(streamer, observer.center_region) != HENKA_SUCCESS ||
        henka_terrain_streamer_request_region(streamer, observer.center_region) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_streamer_get_stats(streamer, &stats);
    if (stats.queued_request_count != 1U ||
        stats.max_queued_request_count < 1U ||
        stats.max_observer_count < 1U ||
        stats.coalesced_request_count == 0U)
    {
        goto cleanup;
    }
    for (index = 0U; index < 200U; ++index)
    {
        if (henka_terrain_streamer_pump(streamer, 1U) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_streamer_get_stats(streamer, &stats);
        if (stats.completed_request_count == 1U)
        {
            break;
        }
#if defined(_WIN32)
        Sleep(1U);
#endif
    }
    if (stats.completed_request_count != 1U || stats.active_request_count != 0U ||
        stats.max_active_request_count > 1U || stats.max_completion_count == 0U ||
        henka_terrain_world_get_region_state(world, observer.center_region, &region_state) != HENKA_SUCCESS ||
        region_state.revision != 8U || region_state.generation != 2U ||
        !region_state.cpu_resident || !region_state.physics_resident ||
        !region_state.render_resident)
    {
        goto cleanup;
    }
    allocations_before_failed_request = henka_memory_get_allocation_count();
    if (henka_terrain_streamer_request_region(
            streamer, (henka_terrain_region_id){3, 3}) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < 200U; ++index)
    {
        if (henka_terrain_streamer_pump(streamer, 1U) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_streamer_get_stats(streamer, &stats);
        if (stats.failed_request_count == 1U)
        {
            break;
        }
#if defined(_WIN32)
        Sleep(1U);
#endif
    }
    if (stats.failed_request_count != 1U ||
        henka_memory_get_allocation_count() != allocations_before_failed_request)
    {
        goto cleanup;
    }
    if (henka_terrain_streamer_update_observer(streamer, &moved_observer) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < 200U; ++index)
    {
        if (henka_terrain_streamer_pump(streamer, 1U) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_streamer_get_stats(streamer, &stats);
        if (stats.completed_request_count == 2U)
        {
            break;
        }
#if defined(_WIN32)
        Sleep(1U);
#endif
    }
    if (stats.completed_request_count != 2U || stats.evicted_region_count != 1U ||
        henka_terrain_world_get_region_state(world, observer.center_region, &region_state) == HENKA_SUCCESS ||
        henka_terrain_world_get_region_state(world, moved_observer.center_region, &region_state) != HENKA_SUCCESS ||
        region_state.revision != 9U || region_state.generation != 3U)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_streamer_destroy(streamer);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(world);
    henka_free(samples);
    return result;
}

static int test_observer_center_is_loaded_before_distant_regions(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_terrain_streamer* streamer = NULL;
    henka_terrain_stream_desc stream_desc = {16U, 16U};
    henka_terrain_stream_observer observer = {7U, {2, 2}, 1U, 1U, 0U, 1U};
    henka_terrain_sample* samples = NULL;
    henka_terrain_stream_stats stats;
    uint32_t index;
    int result = 0;

    world_desc.max_resident_regions = 1U;
    if (henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_terrain_storage_create(
            &world_desc, "build/test_tmp/terrain_streaming_priority_v1", &storage) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
    if (samples == NULL)
    {
        goto cleanup;
    }
    for (index = 0U; index < layout.samples_per_region; ++index)
    {
        samples[index].material_weights[0] = 255U;
    }
    if (henka_terrain_storage_begin(storage, 1U) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_terrain_storage_write_region(
            storage, observer.center_region, 4U, 2U, samples, layout.samples_per_region) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_terrain_storage_commit(storage, 1U) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_terrain_streamer_create(world, storage, &stream_desc, &streamer) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_terrain_streamer_add_observer(streamer, &observer) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < 200U; ++index)
    {
        if (henka_terrain_streamer_pump(streamer, 1U) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_streamer_get_stats(streamer, &stats);
        if (stats.completed_request_count != 0U || stats.failed_request_count != 0U)
        {
            break;
        }
#if defined(_WIN32)
        Sleep(1U);
#endif
    }
    henka_terrain_streamer_get_stats(streamer, &stats);
    if (stats.completed_request_count != 1U || stats.failed_request_count != 0U)
    {
        goto cleanup;
    }
    {
        henka_terrain_region_state state;
        if (henka_terrain_world_get_region_state(world, observer.center_region, &state) != HENKA_SUCCESS ||
            state.revision != 4U || state.generation != 2U || !state.cpu_resident)
        {
            goto cleanup;
        }
    }
    result = 1;

cleanup:
    henka_terrain_streamer_destroy(streamer);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(world);
    henka_free(samples);
    return result;
}

static int test_observer_update_cancels_stale_requests(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_terrain_streamer* streamer = NULL;
    const henka_terrain_stream_desc stream_desc = {16U, 16U};
    const henka_terrain_stream_observer initial_observer = {8U, {2, 2}, 1U, 1U, 1U, 1U};
    const henka_terrain_stream_observer focused_observer = {8U, {2, 2}, 0U, 0U, 0U, 0U};
    henka_terrain_stream_stats stats;
    int result = 0;

    world_desc.max_resident_regions = 1U;
    if (henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_storage_create(
            &world_desc, "build/test_tmp/terrain_streaming_cancel_v1", &storage) != HENKA_SUCCESS ||
        henka_terrain_streamer_create(world, storage, &stream_desc, &streamer) != HENKA_SUCCESS ||
        henka_terrain_streamer_add_observer(streamer, &initial_observer) != HENKA_SUCCESS ||
        henka_terrain_streamer_update_observer(streamer, &focused_observer) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_streamer_get_stats(streamer, &stats);
    if (stats.cancelled_request_count == 0U || stats.queued_request_count > 1U)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_streamer_destroy(streamer);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(world);
    return result;
}

static int test_explicit_request_survives_observer_cancellation(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_terrain_streamer* streamer = NULL;
    const henka_terrain_stream_desc stream_desc = {16U, 16U};
    const henka_terrain_stream_observer broad_observer = {9U, {2, 2}, 1U, 1U, 1U, 1U};
    const henka_terrain_stream_observer focused_observer = {9U, {2, 2}, 0U, 0U, 0U, 0U};
    henka_terrain_sample* samples = NULL;
    henka_terrain_stream_stats stats;
    henka_terrain_region_state state;
    uint32_t index;
    int result = 0;

    world_desc.max_resident_regions = 2U;
    if (henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_storage_create(
            &world_desc, "build/test_tmp/terrain_streaming_explicit_v1", &storage) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
    if (samples == NULL)
    {
        goto cleanup;
    }
    for (index = 0U; index < layout.samples_per_region; ++index)
    {
        samples[index].height_millimeters = 4321;
        samples[index].material_weights[1] = 255U;
    }
    if (henka_terrain_storage_begin(storage, 1U) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(
            storage, broad_observer.center_region, 11U, 4U, samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(
            storage, (henka_terrain_region_id){3, 3}, 12U, 5U, samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_storage_commit(storage, 1U) != HENKA_SUCCESS ||
        henka_terrain_streamer_create(world, storage, &stream_desc, &streamer) != HENKA_SUCCESS ||
        henka_terrain_streamer_add_observer(streamer, &broad_observer) != HENKA_SUCCESS ||
        henka_terrain_streamer_request_region(streamer, (henka_terrain_region_id){3, 3}) != HENKA_SUCCESS ||
        henka_terrain_streamer_update_observer(streamer, &focused_observer) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_streamer_get_stats(streamer, &stats);
    if (stats.cancelled_request_count == 0U || stats.queued_request_count == 0U)
    {
        goto cleanup;
    }
    for (index = 0U; index < 400U; ++index)
    {
        if (henka_terrain_streamer_pump(streamer, 1U) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        if (henka_terrain_world_get_region_state(
                world, (henka_terrain_region_id){3, 3}, &state) == HENKA_SUCCESS)
        {
            break;
        }
#if defined(_WIN32)
        Sleep(1U);
#endif
    }
    if (henka_terrain_world_get_region_state(
            world, (henka_terrain_region_id){3, 3}, &state) != HENKA_SUCCESS ||
        state.revision != 12U || state.generation != 5U || !state.cpu_resident)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_streamer_destroy(streamer);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(world);
    henka_free(samples);
    return result;
}

int main(void)
{
    return test_streaming() &&
        test_observer_center_is_loaded_before_distant_regions() &&
        test_observer_update_cancels_stale_requests() &&
        test_explicit_request_survives_observer_cancellation() ? 0 : 1;
}
