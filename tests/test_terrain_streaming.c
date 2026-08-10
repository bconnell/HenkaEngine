#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include <henka/memory.h>
#include <henka/terrain_edit.h>
#include <henka/terrain_streaming.h>

#define HENKA_TERRAIN_STREAM_TEST_POLL_LIMIT 2000U

typedef struct test_region_generator_state
{
    uint32_t calls;
    size_t sample_count;
    bool contract_valid;
    bool emit_invalid_weights;
} test_region_generator_state;

static henka_result test_generate_region(
    void* user_data,
    henka_terrain_region_id region_id,
    const henka_terrain_world_desc* world_desc,
    const henka_terrain_layout* layout,
    henka_terrain_sample* samples,
    size_t sample_count)
{
    test_region_generator_state* state = (test_region_generator_state*)user_data;
    size_t index;
    if (state == NULL || world_desc == NULL || layout == NULL || samples == NULL ||
        sample_count != layout->samples_per_region ||
        !henka_terrain_region_id_is_valid(world_desc, region_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    ++state->calls;
    state->sample_count = sample_count;
    state->contract_valid = true;
    for (index = 0U; index < sample_count; ++index)
    {
        samples[index].height_millimeters = 1000 + region_id.x * 10 + region_id.z;
        memset(samples[index].material_weights, 0, sizeof(samples[index].material_weights));
        samples[index].material_weights[0] = state->emit_invalid_weights ? 1U : 255U;
        if (state->emit_invalid_weights)
        {
            samples[index].material_weights[1] = 1U;
        }
    }
    return HENKA_SUCCESS;
}

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
    for (index = 0U; index < HENKA_TERRAIN_STREAM_TEST_POLL_LIMIT; ++index)
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
    for (index = 0U; index < HENKA_TERRAIN_STREAM_TEST_POLL_LIMIT; ++index)
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
    for (index = 0U; index < HENKA_TERRAIN_STREAM_TEST_POLL_LIMIT; ++index)
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
    for (index = 0U; index < HENKA_TERRAIN_STREAM_TEST_POLL_LIMIT; ++index)
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

static int test_observer_union_preserves_other_observer_demand(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_terrain_streamer* streamer = NULL;
    const henka_terrain_stream_desc stream_desc = {32U, 32U};
    const henka_terrain_stream_observer first_observer = {10U, {1, 1}, 1U, 1U, 1U, 1U};
    const henka_terrain_stream_observer second_observer = {11U, {3, 3}, 1U, 1U, 1U, 1U};
    const henka_terrain_stream_observer focused_first_observer = {10U, {0, 0}, 0U, 0U, 0U, 0U};
    henka_terrain_sample* samples = NULL;
    henka_terrain_stream_stats stats;
    henka_terrain_region_state state;
    uint32_t index;
    int result = 0;

    world_desc.max_resident_regions = 2U;
    if (henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_storage_create(
            &world_desc, "build/test_tmp/terrain_streaming_observer_union_v1", &storage) != HENKA_SUCCESS)
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
        samples[index].height_millimeters = 2468;
        samples[index].material_weights[2] = 255U;
    }
    if (henka_terrain_storage_begin(storage, 1U) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(
            storage, second_observer.center_region, 22U, 6U, samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_storage_commit(storage, 1U) != HENKA_SUCCESS ||
        henka_terrain_streamer_create(world, storage, &stream_desc, &streamer) != HENKA_SUCCESS ||
        henka_terrain_streamer_add_observer(streamer, &first_observer) != HENKA_SUCCESS ||
        henka_terrain_streamer_add_observer(streamer, &second_observer) != HENKA_SUCCESS ||
        henka_terrain_streamer_update_observer(streamer, &focused_first_observer) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_streamer_get_stats(streamer, &stats);
    if (stats.cancelled_request_count == 0U)
    {
        goto cleanup;
    }
    for (index = 0U; index < 500U; ++index)
    {
        if (henka_terrain_streamer_pump(streamer, 1U) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        if (henka_terrain_world_get_region_state(
                world, second_observer.center_region, &state) == HENKA_SUCCESS)
        {
            break;
        }
#if defined(_WIN32)
        Sleep(1U);
#endif
    }
    if (henka_terrain_world_get_region_state(
            world, second_observer.center_region, &state) != HENKA_SUCCESS ||
        state.revision != 22U || state.generation != 6U || !state.cpu_resident)
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

static int test_observer_radius_loads_bounded_camera_window(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_terrain_streamer* streamer = NULL;
    henka_terrain_stream_desc stream_desc = henka_terrain_stream_desc_default();
    const henka_terrain_stream_observer observer = {12U, {0, 0}, 1U, 1U, 1U, 1U};
    test_region_generator_state generator_state = {0};
    henka_terrain_stream_stats stats;
    uint32_t index;
    int result = 0;

    /* Keep this contract test small while retaining the production 2x2
     * camera-window shape. */
    world_desc.world_width_meters = 128U;
    world_desc.world_depth_meters = 128U;
    world_desc.region_edge_meters = 64U;
    world_desc.chunk_edge_meters = 64U;
    world_desc.samples_per_chunk = 65U;
    world_desc.chunks_per_region_edge = 1U;
    world_desc.regions_across = 2U;
    world_desc.regions_down = 2U;
    world_desc.max_resident_regions = 4U;
    world_desc.max_resident_chunks = 4U;
    world_desc.max_pending_io = 16U;
    world_desc.max_stream_observers = 1U;
    stream_desc.generate_region = test_generate_region;
    stream_desc.generate_region_user_data = &generator_state;
    if (henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_storage_create(
            &world_desc, "build/test_tmp/terrain_streaming_radius_v1", &storage) != HENKA_SUCCESS ||
        henka_terrain_streamer_create(world, storage, &stream_desc, &streamer) != HENKA_SUCCESS ||
        henka_terrain_streamer_add_observer(streamer, &observer) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < 400U; ++index)
    {
        if (henka_terrain_streamer_pump(streamer, 4U) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_streamer_get_stats(streamer, &stats);
        if (stats.completed_request_count == 4U || stats.failed_request_count != 0U)
        {
            break;
        }
#if defined(_WIN32)
        Sleep(1U);
#endif
    }
    henka_terrain_streamer_get_stats(streamer, &stats);
    if (stats.completed_request_count != 4U || stats.failed_request_count != 0U ||
        generator_state.calls != 4U)
    {
        goto cleanup;
    }
    for (index = 0U; index < 4U; ++index)
    {
        const henka_terrain_region_id region_id = {
            (int32_t)(index % 2U), (int32_t)(index / 2U)};
        henka_terrain_region_state state;
        if (henka_terrain_world_get_region_state(world, region_id, &state) != HENKA_SUCCESS ||
            !state.cpu_resident || !state.physics_resident || !state.render_resident)
        {
            goto cleanup;
        }
    }
    result = 1;

cleanup:
    henka_terrain_streamer_destroy(streamer);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(world);
    return result;
}

static int test_missing_region_uses_bounded_generator(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_terrain_streamer* streamer = NULL;
    henka_terrain_stream_desc stream_desc = henka_terrain_stream_desc_default();
    henka_terrain_stream_stats stats;
    henka_terrain_region_state region_state;
    const henka_terrain_sample* samples = NULL;
    size_t sample_count = 0U;
    test_region_generator_state generator_state = {0};
    uint32_t index;
    int result = 0;

    world_desc.max_resident_regions = 1U;
    if (henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_storage_create(
            &world_desc, "build/test_tmp/terrain_streaming_generated_v1", &storage) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    stream_desc.generate_region = test_generate_region;
    stream_desc.generate_region_user_data = &generator_state;
    if (henka_terrain_streamer_create(world, storage, &stream_desc, &streamer) != HENKA_SUCCESS ||
        henka_terrain_streamer_request_region(streamer, (henka_terrain_region_id){5, 6}) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < 400U; ++index)
    {
        if (henka_terrain_streamer_pump(streamer, 1U) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_streamer_get_stats(streamer, &stats);
        if (stats.completed_request_count == 1U || stats.failed_request_count != 0U)
        {
            break;
        }
#if defined(_WIN32)
        Sleep(1U);
#endif
    }
    if (generator_state.calls != 1U || !generator_state.contract_valid ||
        generator_state.sample_count != layout.samples_per_region ||
        stats.completed_request_count != 1U || stats.generated_region_count != 1U ||
        stats.generator_failure_count != 0U ||
        henka_terrain_world_get_region_state(
            world, (henka_terrain_region_id){5, 6}, &region_state) != HENKA_SUCCESS ||
        region_state.revision != 1U || region_state.generation != 1U ||
        !region_state.cpu_resident ||
        henka_terrain_world_get_region_samples(
            world, (henka_terrain_region_id){5, 6}, &samples, &sample_count) != HENKA_SUCCESS ||
        sample_count != layout.samples_per_region || samples[0].height_millimeters != 1056 ||
        samples[0].material_weights[0] != 255U)
    {
        goto cleanup;
    }
    generator_state.emit_invalid_weights = true;
    if (henka_terrain_streamer_request_region(
            streamer, (henka_terrain_region_id){7, 7}) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (index = 0U; index < 400U; ++index)
    {
        henka_terrain_streamer_get_stats(streamer, &stats);
        if (stats.failed_request_count == 1U)
        {
            break;
        }
        if (henka_terrain_streamer_pump(streamer, 1U) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
#if defined(_WIN32)
        Sleep(1U);
#endif
    }
    if (stats.failed_request_count != 1U || stats.generator_failure_count != 1U ||
        stats.generated_region_count != 1U ||
        henka_terrain_world_get_region_state(
            world, (henka_terrain_region_id){7, 7}, &region_state) == HENKA_SUCCESS)
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

int main(void)
{
    return test_streaming() &&
        test_observer_center_is_loaded_before_distant_regions() &&
        test_observer_update_cancels_stale_requests() &&
        test_explicit_request_survives_observer_cancellation() &&
        test_observer_union_preserves_other_observer_demand() &&
        test_observer_radius_loads_bounded_camera_window() &&
        test_missing_region_uses_bounded_generator() ? 0 : 1;
}
