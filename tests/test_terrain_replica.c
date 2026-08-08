#include <string.h>

#include <henka/memory.h>
#include <henka/terrain_replica.h>

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
    henka_terrain_region_state state;
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
