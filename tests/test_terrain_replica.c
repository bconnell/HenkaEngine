#include <string.h>

#include <henka/memory.h>
#include <henka/terrain_replica.h>

#include "../engine/src/core/memory_internal.h"

static int test_replica_delta_and_snapshot(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_world* snapshot_world = NULL;
    henka_terrain_replica* replica = NULL;
    henka_terrain_replica* snapshot_replica = NULL;
    henka_terrain_replica_desc replica_desc;
    henka_terrain_sample* samples = NULL;
    uint8_t* record = NULL;
    henka_terrain_edit_delta delta = {0};
    henka_terrain_snapshot_fragment fragment = {0};
    size_t record_size = 0U;
    size_t offset = 0U;
    uint32_t fragment_count;
    uint32_t fragment_index;
    bool applied = false;
    bool complete = false;
    henka_result fragment_result;
    henka_terrain_replica_diagnostics diagnostics;
    henka_terrain_region_state state;
    henka_terrain_region_state state_before_invalid_transfer;
    int result = 0;
    uint32_t index;

    world_desc.max_resident_regions = 1U;
    if (henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    replica_desc = henka_terrain_replica_desc_default();
    replica_desc.world = world;
    if (henka_terrain_replica_create(&replica_desc, &replica) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    delta.world_identity = world_desc.world_identity;
    delta.base_asset_identity = world_desc.base_asset_identity;
    delta.client_nonce = 55U;
    delta.server_command_id = 1U;
    delta.command = henka_terrain_edit_command_default();
    delta.command.client_nonce = 55U;
    delta.command.center_sample_x = 100;
    delta.command.center_sample_z = 100;
    delta.command.radius_samples = 4U;
    delta.affected_region_count = 1U;
    delta.affected_regions[0] = (henka_terrain_network_region_revision){{0, 0}, 1U};
    if (henka_terrain_replica_apply_delta(replica, &delta, &applied) != HENKA_SUCCESS ||
        !applied || henka_terrain_replica_apply_delta(replica, &delta, &applied) != HENKA_SUCCESS ||
        applied || henka_terrain_world_get_region_state(
            world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS || state.revision != 1U)
    {
        goto cleanup;
    }
    if (henka_terrain_world_create(&world_desc, &snapshot_world) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    replica_desc.world = snapshot_world;
    if (henka_terrain_replica_create(&replica_desc, &snapshot_replica) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
    record = henka_malloc(HENKA_TERRAIN_MAX_REGION_RECORD_BYTES);
    if (samples == NULL || record == NULL)
    {
        goto cleanup;
    }
    for (index = 0U; index < layout.samples_per_region; ++index)
    {
        samples[index].material_weights[0] = 255U;
    }
    if (henka_terrain_region_encode(
            &world_desc, (henka_terrain_region_id){0, 0}, 2U, 3U,
            samples, layout.samples_per_region, record,
            HENKA_TERRAIN_MAX_REGION_RECORD_BYTES, &record_size) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    fragment_count = (uint32_t)((record_size + HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES - 1U) /
        HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES);
    if (fragment_count < 2U)
    {
        goto cleanup;
    }
    fragment.world_identity = world_desc.world_identity;
    fragment.base_asset_identity = world_desc.base_asset_identity;
    fragment.transfer_id = 6U;
    fragment.region_id = (henka_terrain_region_id){0, 0};
    fragment.revision = 2U;
    fragment.generation = 3U;
    fragment.fragment_index = 0U;
    fragment.fragment_count = fragment_count;
    fragment.total_bytes = (uint32_t)record_size;
    fragment.data_size = 1U;
    fragment.data = record;
    if (henka_terrain_replica_apply_snapshot_fragment(
            snapshot_replica, &fragment, &complete) != HENKA_ERROR_INVALID_ARGUMENT ||
        complete)
    {
        goto cleanup;
    }
    for (fragment_index = 0U; fragment_index < fragment_count; ++fragment_index)
    {
        size_t remaining = record_size - offset;
        size_t data_size = remaining > HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES
            ? HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES : remaining;
        fragment.world_identity = world_desc.world_identity;
        fragment.base_asset_identity = world_desc.base_asset_identity;
        fragment.transfer_id = 7U;
        fragment.region_id = (henka_terrain_region_id){0, 0};
        fragment.revision = 2U;
        fragment.generation = 3U;
        fragment.fragment_index = fragment_index;
        fragment.fragment_count = fragment_count;
        fragment.total_bytes = (uint32_t)record_size;
        fragment.data_size = (uint32_t)data_size;
        fragment.data = record + offset;
        if (henka_terrain_replica_apply_snapshot_fragment(
                snapshot_replica, &fragment, &complete) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        offset += data_size;
    }
    if (!complete || henka_terrain_world_get_region_state(
            snapshot_world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        state.revision != 2U || state.generation != 3U)
    {
        goto cleanup;
    }
    state_before_invalid_transfer = state;
    fragment.transfer_id = 11U;
    fragment.fragment_index = 0U;
    fragment.data_size = (uint32_t)(record_size > HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES
        ? HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES : record_size);
    fragment.data = record;
    fragment.world_identity ^= UINT64_C(1);
    complete = false;
    if (henka_terrain_replica_apply_snapshot_fragment(
            snapshot_replica, &fragment, &complete) != HENKA_ERROR_INVALID_ARGUMENT ||
        complete ||
        henka_terrain_world_get_region_state(
            snapshot_world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        memcmp(&state, &state_before_invalid_transfer, sizeof(state)) != 0)
    {
        goto cleanup;
    }
    fragment.world_identity = world_desc.world_identity;
    fragment.base_asset_identity ^= UINT64_C(1);
    if (henka_terrain_replica_apply_snapshot_fragment(
            snapshot_replica, &fragment, &complete) != HENKA_ERROR_INVALID_ARGUMENT ||
        complete ||
        henka_terrain_world_get_region_state(
            snapshot_world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        memcmp(&state, &state_before_invalid_transfer, sizeof(state)) != 0)
    {
        goto cleanup;
    }
    fragment.base_asset_identity = world_desc.base_asset_identity;
    fragment.transfer_id = 10U;
    fragment.fragment_index = 0U;
    fragment.data_size = HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES;
    fragment.data = record;
    complete = false;
    if (henka_terrain_replica_apply_snapshot_fragment(
            snapshot_replica, &fragment, &complete) != HENKA_SUCCESS ||
        complete ||
        henka_terrain_replica_apply_snapshot_fragment(
            snapshot_replica, &fragment, &complete) != HENKA_ERROR_INVALID_ARGUMENT ||
        complete)
    {
        goto cleanup;
    }
    fragment.transfer_id = 8U;
    fragment_result = HENKA_SUCCESS;
    complete = false;
    offset = 0U;
    record[0] ^= 0x5AU;
    for (fragment_index = 0U; fragment_index < fragment_count; ++fragment_index)
    {
        size_t remaining = record_size - offset;
        size_t data_size = remaining > HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES
            ? HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES : remaining;
        fragment.fragment_index = fragment_index;
        fragment.data_size = (uint32_t)data_size;
        fragment.data = record + offset;
        fragment_result = henka_terrain_replica_apply_snapshot_fragment(
            snapshot_replica, &fragment, &complete);
        if (fragment_index + 1U < fragment_count && fragment_result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        offset += data_size;
    }
    record[0] ^= 0x5AU;
    if (fragment_result == HENKA_SUCCESS || complete ||
        henka_terrain_world_get_region_state(
            snapshot_world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        state.revision != 2U || state.generation != 3U)
    {
        goto cleanup;
    }
    henka_terrain_replica_get_diagnostics(snapshot_replica, &diagnostics);
    if (diagnostics.rejected_snapshot_count == 0U)
    {
        goto cleanup;
    }
    state_before_invalid_transfer = state;
    fragment.transfer_id = 12U;
    complete = false;
    offset = 0U;
    for (fragment_index = 0U; fragment_index < fragment_count; ++fragment_index)
    {
        size_t remaining = record_size - offset;
        size_t data_size = remaining > HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES
            ? HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES : remaining;
        fragment.fragment_index = fragment_index;
        fragment.data_size = (uint32_t)data_size;
        fragment.data = record + offset;
        if (fragment_index + 1U == fragment_count)
        {
            henka_memory_test_fail_after(0U);
        }
        fragment_result = henka_terrain_replica_apply_snapshot_fragment(
            snapshot_replica, &fragment, &complete);
        henka_memory_test_disable_failures();
        if (fragment_index + 1U < fragment_count && fragment_result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        if (fragment_index + 1U == fragment_count &&
            (fragment_result != HENKA_ERROR_OUT_OF_MEMORY || complete ||
             henka_terrain_world_get_region_state(
                 snapshot_world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
             memcmp(&state, &state_before_invalid_transfer, sizeof(state)) != 0))
        {
            goto cleanup;
        }
        offset += data_size;
    }
    fragment.transfer_id = 9U;
    complete = false;
    offset = 0U;
    for (fragment_index = fragment_count; fragment_index-- > 0U;)
    {
        size_t fragment_offset = (size_t)fragment_index * HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES;
        size_t remaining = record_size - fragment_offset;
        size_t data_size = remaining > HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES
            ? HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES : remaining;
        fragment.fragment_index = fragment_index;
        fragment.data_size = (uint32_t)data_size;
        fragment.data = record + fragment_offset;
        if (henka_terrain_replica_apply_snapshot_fragment(
                snapshot_replica, &fragment, &complete) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
    }
    if (!complete)
    {
        goto cleanup;
    }
    state_before_invalid_transfer = state;
    if (henka_terrain_region_encode(
            &world_desc, (henka_terrain_region_id){0, 0}, 1U, 3U,
            samples, layout.samples_per_region, record,
            HENKA_TERRAIN_MAX_REGION_RECORD_BYTES, &record_size) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    fragment.transfer_id = 13U;
    fragment.revision = 1U;
    fragment.generation = 3U;
    complete = false;
    offset = 0U;
    for (fragment_index = 0U; fragment_index < fragment_count; ++fragment_index)
    {
        size_t remaining = record_size - offset;
        size_t data_size = remaining > HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES
            ? HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES : remaining;
        fragment.fragment_index = fragment_index;
        fragment.data_size = (uint32_t)data_size;
        fragment.data = record + offset;
        if (henka_terrain_replica_apply_snapshot_fragment(
                snapshot_replica, &fragment, &complete) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        offset += data_size;
    }
    henka_terrain_replica_get_diagnostics(snapshot_replica, &diagnostics);
    if (!complete || diagnostics.stale_snapshot_count != 1U ||
        memcmp(&state, &state_before_invalid_transfer, sizeof(state)) != 0)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_replica_destroy(snapshot_replica);
    henka_terrain_replica_destroy(replica);
    henka_terrain_world_destroy(snapshot_world);
    henka_terrain_world_destroy(world);
    henka_free(record);
    henka_free(samples);
    return result;
}

int main(void)
{
    return test_replica_delta_and_snapshot() ? 0 : 1;
}
