#include <stdint.h>

#include <henka/memory.h>
#include <henka/terrain_authority.h>

#include "../engine/src/core/memory_internal.h"

static int test_authoritative_acceptance_and_stale_rejection(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_world* world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_terrain_authority* authority = NULL;
    henka_terrain_authority_desc authority_desc;
    henka_terrain_edit_request request = {0};
    henka_terrain_authority_response response;
    henka_terrain_region_state state;
    henka_terrain_region_storage_info info;
    henka_terrain_layout layout;
    henka_terrain_sample* samples = NULL;
    size_t allocations_before_partial_validation;
    int result = 0;
    uint32_t index;

    world_desc.max_resident_regions = 2U;
    if (henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){1, 0}) != HENKA_SUCCESS ||
        henka_terrain_storage_create(&world_desc, "test_tmp/terrain_authority_v1", &storage) != HENKA_SUCCESS)
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
    if (henka_terrain_storage_begin(storage, 1U) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(storage, (henka_terrain_region_id){0, 0}, 0U, 0U, samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_storage_write_region(storage, (henka_terrain_region_id){1, 0}, 0U, 0U, samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_storage_commit(storage, 1U) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    authority_desc = henka_terrain_authority_desc_default();
    authority_desc.world = world;
    authority_desc.storage = storage;
    if (henka_terrain_authority_create(&authority_desc, &authority) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    request.world_identity = world_desc.world_identity;
    request.base_asset_identity = world_desc.base_asset_identity;
    request.client_nonce = 77U;
    request.command = henka_terrain_edit_command_default();
    request.command.center_sample_x = 100;
    request.command.center_sample_z = 100;
    request.command.radius_samples = 4U;
    request.affected_region_count = 1U;
    request.affected_regions[0] = (henka_terrain_network_region_revision){{0, 0}, 0U};
    if (henka_terrain_authority_process_request(authority, 1U, &request, 1000U, &response) != HENKA_SUCCESS ||
        !response.accepted || response.acceptance.server_command_id != 1U ||
        henka_terrain_world_get_region_state(world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        state.revision != 1U ||
        state.dirty ||
        henka_terrain_storage_load_region(storage, (henka_terrain_region_id){0, 0}, &info, samples, layout.samples_per_region) != HENKA_SUCCESS ||
        info.revision != 1U)
    {
        goto cleanup;
    }
    henka_terrain_authority_retire_peer(authority, 1U);
    request.client_nonce = 78U;
    request.world_identity ^= UINT64_C(1);
    if (henka_terrain_authority_process_request(authority, 2U, &request, 1001U, &response) != HENKA_SUCCESS ||
        response.accepted || response.rejection.reason != HENKA_TERRAIN_EDIT_REJECT_WORLD_MISMATCH)
    {
        goto cleanup;
    }
    request.world_identity = world_desc.world_identity;
    request.base_asset_identity ^= UINT64_C(1);
    if (henka_terrain_authority_process_request(authority, 2U, &request, 1001U, &response) != HENKA_SUCCESS ||
        response.accepted || response.rejection.reason != HENKA_TERRAIN_EDIT_REJECT_BASE_MISMATCH)
    {
        goto cleanup;
    }
    request.base_asset_identity = world_desc.base_asset_identity;
    request.command.operation = HENKA_TERRAIN_EDIT_FLATTEN;
    if (henka_terrain_authority_process_request(authority, 2U, &request, 1001U, &response) != HENKA_SUCCESS ||
        response.accepted || response.rejection.reason != HENKA_TERRAIN_EDIT_REJECT_STALE_REVISION)
    {
        goto cleanup;
    }
    request.client_nonce = 79U;
    request.command.operation = HENKA_TERRAIN_EDIT_SMOOTH;
    request.affected_regions[0].revision = 1U;
    if (henka_terrain_authority_process_request(authority, 2U, &request, 1002U, &response) != HENKA_SUCCESS ||
        !response.accepted || response.acceptance.server_command_id != 2U ||
        henka_terrain_world_get_region_state(world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        state.revision != 2U ||
        state.dirty)
    {
        goto cleanup;
    }
    allocations_before_partial_validation = henka_memory_get_allocation_count();
    request.client_nonce = 80U;
    request.command.operation = HENKA_TERRAIN_EDIT_RAISE;
    request.command.center_sample_x = 512;
    request.affected_region_count = 2U;
    request.affected_regions[0] = (henka_terrain_network_region_revision){{0, 0}, 2U};
    request.affected_regions[1] = (henka_terrain_network_region_revision){{1, 0}, 999U};
    if (henka_terrain_authority_process_request(authority, 2U, &request, 1003U, &response) != HENKA_SUCCESS ||
        response.accepted || response.rejection.reason != HENKA_TERRAIN_EDIT_REJECT_STALE_REVISION ||
        henka_memory_get_allocation_count() != allocations_before_partial_validation)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_authority_destroy(authority);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(world);
    henka_free(samples);
    return result;
}

int main(void)
{
    return test_authoritative_acceptance_and_stale_rejection() ? 0 : 1;
}
