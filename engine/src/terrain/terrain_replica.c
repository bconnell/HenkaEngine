#include <henka/terrain_replica.h>

#include <string.h>

#include <henka/memory.h>

#include "terrain_internal.h"

struct henka_terrain_replica
{
    henka_terrain_world* world;
    uint32_t max_snapshot_bytes;
    uint8_t* snapshot_data;
    uint32_t snapshot_total_bytes;
    uint32_t snapshot_received_bytes;
    uint32_t snapshot_fragment_count;
    uint32_t snapshot_received_fragments;
    uint64_t snapshot_transfer_id;
    henka_terrain_region_id snapshot_region_id;
    henka_terrain_revision snapshot_revision;
    henka_terrain_generation snapshot_generation;
    uint8_t snapshot_received[HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENTS];
    henka_terrain_replica_diagnostics diagnostics;
};

henka_terrain_replica_desc henka_terrain_replica_desc_default(void)
{
    return (henka_terrain_replica_desc){NULL, HENKA_TERRAIN_MAX_REGION_RECORD_BYTES};
}

henka_result henka_terrain_replica_create(
    const henka_terrain_replica_desc* desc,
    henka_terrain_replica** out_replica)
{
    henka_terrain_replica* replica;
    if (out_replica == NULL || desc == NULL || desc->world == NULL ||
        desc->max_snapshot_bytes == 0U || desc->max_snapshot_bytes > HENKA_TERRAIN_MAX_REGION_RECORD_BYTES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_replica = NULL;
    replica = henka_calloc(1U, sizeof(*replica));
    if (replica == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    replica->world = desc->world;
    replica->max_snapshot_bytes = desc->max_snapshot_bytes;
    *out_replica = replica;
    return HENKA_SUCCESS;
}

static void henka_terrain_replica_reset_snapshot(henka_terrain_replica* replica)
{
    henka_free(replica->snapshot_data);
    replica->snapshot_data = NULL;
    replica->snapshot_total_bytes = 0U;
    replica->snapshot_received_bytes = 0U;
    replica->snapshot_fragment_count = 0U;
    replica->snapshot_received_fragments = 0U;
    replica->snapshot_transfer_id = 0U;
    memset(replica->snapshot_received, 0, sizeof(replica->snapshot_received));
}

void henka_terrain_replica_destroy(henka_terrain_replica* replica)
{
    if (replica == NULL)
    {
        return;
    }
    henka_terrain_replica_reset_snapshot(replica);
    henka_free(replica);
}

static bool henka_terrain_replica_identity_matches(
    const henka_terrain_world_desc* desc,
    henka_terrain_world_identity world_identity,
    henka_terrain_base_asset_identity base_asset_identity)
{
    return desc->world_identity == world_identity && desc->base_asset_identity == base_asset_identity;
}

henka_result henka_terrain_replica_apply_delta(
    henka_terrain_replica* replica,
    const henka_terrain_edit_delta* delta,
    bool* out_applied)
{
    henka_terrain_world_desc desc;
    henka_terrain_region_id affected[HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS];
    uint32_t affected_count = HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS;
    henka_result result;
    uint32_t index;
    bool duplicate = true;
    bool has_duplicate_region = false;
    if (out_applied == NULL || replica == NULL || delta == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_applied = false;
    if (henka_terrain_world_get_desc(replica->world, &desc) != HENKA_SUCCESS ||
        !henka_terrain_replica_identity_matches(
            &desc, delta->world_identity, delta->base_asset_identity) ||
        delta->server_command_id == 0U)
    {
        ++replica->diagnostics.rejected_delta_count;
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_terrain_edit_get_affected_regions(
        replica->world, &delta->command, affected, &affected_count);
    if (result != HENKA_SUCCESS || affected_count != delta->affected_region_count)
    {
        ++replica->diagnostics.rejected_delta_count;
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < affected_count; ++index)
    {
        henka_terrain_region_state state;
        if (!henka_terrain_region_id_equal(affected[index], delta->affected_regions[index].region_id) ||
            henka_terrain_world_get_region_state(replica->world, affected[index], &state) != HENKA_SUCCESS ||
            delta->affected_regions[index].revision == 0U)
        {
            ++replica->diagnostics.rejected_delta_count;
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        if (state.revision != delta->affected_regions[index].revision)
        {
            duplicate = false;
        }
        if (state.revision == delta->affected_regions[index].revision)
        {
            has_duplicate_region = true;
        }
        else if (state.revision == UINT64_MAX ||
            state.revision + 1U != delta->affected_regions[index].revision)
        {
            ++replica->diagnostics.rejected_delta_count;
            return HENKA_ERROR_ASSET_SOURCE;
        }
    }
    if (duplicate)
    {
        ++replica->diagnostics.duplicate_delta_count;
        return HENKA_SUCCESS;
    }
    if (has_duplicate_region)
    {
        ++replica->diagnostics.rejected_delta_count;
        return HENKA_ERROR_ASSET_SOURCE;
    }
    result = henka_terrain_world_apply_edit(
        replica->world, &delta->command, delta->server_command_id);
    if (result != HENKA_SUCCESS)
    {
        ++replica->diagnostics.rejected_delta_count;
        return result;
    }
    for (index = 0U; index < affected_count; ++index)
    {
        henka_terrain_region_state state;
        if (henka_terrain_world_get_region_state(replica->world, affected[index], &state) != HENKA_SUCCESS ||
            state.revision != delta->affected_regions[index].revision)
        {
            ++replica->diagnostics.rejected_delta_count;
            return HENKA_ERROR_UNKNOWN;
        }
    }
    ++replica->diagnostics.applied_delta_count;
    *out_applied = true;
    return HENKA_SUCCESS;
}

static bool henka_terrain_replica_snapshot_metadata_matches(
    const henka_terrain_replica* replica,
    const henka_terrain_snapshot_fragment* fragment)
{
    return replica->snapshot_transfer_id == fragment->transfer_id &&
        henka_terrain_region_id_equal(replica->snapshot_region_id, fragment->region_id) &&
        replica->snapshot_revision == fragment->revision &&
        replica->snapshot_generation == fragment->generation &&
        replica->snapshot_total_bytes == fragment->total_bytes &&
        replica->snapshot_fragment_count == fragment->fragment_count;
}

henka_result henka_terrain_replica_apply_snapshot_fragment(
    henka_terrain_replica* replica,
    const henka_terrain_snapshot_fragment* fragment,
    bool* out_complete)
{
    henka_terrain_world_desc desc;
    henka_terrain_layout layout;
    size_t offset;
    size_t sample_bytes;
    henka_terrain_sample* samples = NULL;
    henka_terrain_region_storage_info info;
    henka_result result;
    if (out_complete == NULL || replica == NULL || fragment == NULL || fragment->data == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_complete = false;
    if (henka_terrain_world_get_desc(replica->world, &desc) != HENKA_SUCCESS ||
        !henka_terrain_replica_identity_matches(
            &desc, fragment->world_identity, fragment->base_asset_identity) ||
        fragment->transfer_id == 0U || fragment->fragment_count == 0U ||
        fragment->fragment_count > HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENTS ||
        fragment->fragment_index >= fragment->fragment_count || fragment->total_bytes == 0U ||
        fragment->total_bytes > replica->max_snapshot_bytes || fragment->data_size == 0U ||
        fragment->data_size > HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES)
    {
        ++replica->diagnostics.rejected_snapshot_count;
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    offset = (size_t)fragment->fragment_index * HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES;
    if (offset > fragment->total_bytes || fragment->data_size > fragment->total_bytes - offset)
    {
        ++replica->diagnostics.rejected_snapshot_count;
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (replica->snapshot_transfer_id == 0U ||
        !henka_terrain_replica_snapshot_metadata_matches(replica, fragment))
    {
        henka_terrain_replica_reset_snapshot(replica);
        replica->snapshot_data = henka_malloc(fragment->total_bytes);
        if (replica->snapshot_data == NULL)
        {
            ++replica->diagnostics.rejected_snapshot_count;
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        replica->snapshot_total_bytes = fragment->total_bytes;
        replica->snapshot_fragment_count = fragment->fragment_count;
        replica->snapshot_transfer_id = fragment->transfer_id;
        replica->snapshot_region_id = fragment->region_id;
        replica->snapshot_revision = fragment->revision;
        replica->snapshot_generation = fragment->generation;
    }
    if (replica->snapshot_received[fragment->fragment_index] != 0U)
    {
        ++replica->diagnostics.rejected_snapshot_count;
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memcpy(replica->snapshot_data + offset, fragment->data, fragment->data_size);
    replica->snapshot_received[fragment->fragment_index] = 1U;
    ++replica->snapshot_received_fragments;
    replica->snapshot_received_bytes += fragment->data_size;
    ++replica->diagnostics.accepted_snapshot_fragment_count;
    if (replica->snapshot_received_fragments < replica->snapshot_fragment_count ||
        replica->snapshot_received_bytes != replica->snapshot_total_bytes)
    {
        return HENKA_SUCCESS;
    }
    if (henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        layout.samples_per_region > SIZE_MAX / sizeof(*samples))
    {
        ++replica->diagnostics.rejected_snapshot_count;
        henka_terrain_replica_reset_snapshot(replica);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    sample_bytes = layout.samples_per_region * sizeof(*samples);
    samples = henka_malloc(sample_bytes);
    if (samples == NULL)
    {
        ++replica->diagnostics.rejected_snapshot_count;
        henka_terrain_replica_reset_snapshot(replica);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    result = henka_terrain_region_decode(
        &desc, replica->snapshot_data, replica->snapshot_total_bytes,
        &info, samples, layout.samples_per_region);
    if (result == HENKA_SUCCESS &&
        (!henka_terrain_region_id_equal(info.id, replica->snapshot_region_id) ||
         info.revision != replica->snapshot_revision || info.generation != replica->snapshot_generation))
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_terrain_world_apply_region_snapshot(
            replica->world, info, samples, layout.samples_per_region);
    }
    henka_free(samples);
    henka_terrain_replica_reset_snapshot(replica);
    if (result != HENKA_SUCCESS)
    {
        ++replica->diagnostics.rejected_snapshot_count;
        return result;
    }
    ++replica->diagnostics.completed_snapshot_count;
    *out_complete = true;
    return HENKA_SUCCESS;
}

void henka_terrain_replica_get_diagnostics(
    const henka_terrain_replica* replica,
    henka_terrain_replica_diagnostics* out_diagnostics)
{
    if (out_diagnostics == NULL)
    {
        return;
    }
    *out_diagnostics = replica == NULL
        ? (henka_terrain_replica_diagnostics){0}
        : replica->diagnostics;
}
