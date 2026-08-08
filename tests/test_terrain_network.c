#include <string.h>

#include <henka/terrain_network.h>

static int test_request_and_response_codecs(void)
{
    henka_terrain_edit_request request = {0};
    henka_terrain_edit_request decoded_request;
    henka_terrain_edit_acceptance acceptance = {0};
    henka_terrain_edit_acceptance decoded_acceptance;
    henka_terrain_edit_rejection rejection = {42U, HENKA_TERRAIN_EDIT_REJECT_STALE_REVISION};
    henka_terrain_edit_rejection decoded_rejection;
    henka_terrain_edit_delta delta = {0};
    henka_terrain_edit_delta decoded_delta;
    henka_terrain_snapshot_request snapshot_request = {0};
    henka_terrain_snapshot_request decoded_snapshot_request;
    henka_terrain_snapshot_fragment snapshot_fragment = {0};
    henka_terrain_snapshot_fragment decoded_snapshot_fragment;
    uint8_t buffer[HENKA_TERRAIN_NETWORK_MAX_EDIT_REQUEST_BYTES];
    size_t size = 0U;

    request.world_identity = 11U;
    request.base_asset_identity = 22U;
    request.client_nonce = 33U;
    request.command = henka_terrain_edit_command_default();
    request.command.client_nonce = request.client_nonce;
    request.command.center_sample_x = 100;
    request.command.center_sample_z = 200;
    request.command.radius_samples = 8U;
    request.affected_region_count = 1U;
    request.affected_regions[0] = (henka_terrain_network_region_revision){{2, 3}, 7U};
    if (henka_terrain_edit_request_encode(&request, buffer, sizeof(buffer), &size) != HENKA_SUCCESS ||
        henka_terrain_edit_request_decode(buffer, size, &decoded_request) != HENKA_SUCCESS ||
        decoded_request.world_identity != 11U || decoded_request.base_asset_identity != 22U ||
        decoded_request.client_nonce != 33U || decoded_request.command.center_sample_z != 200 ||
        decoded_request.affected_regions[0].revision != 7U)
    {
        return 0;
    }
    buffer[48] = 255U;
    if (henka_terrain_edit_request_decode(buffer, size, &decoded_request) == HENKA_SUCCESS)
    {
        return 0;
    }
    acceptance.client_nonce = 33U;
    acceptance.server_command_id = 99U;
    acceptance.affected_region_count = 1U;
    acceptance.affected_regions[0] = request.affected_regions[0];
    if (henka_terrain_edit_acceptance_encode(&acceptance, buffer, sizeof(buffer), &size) != HENKA_SUCCESS ||
        henka_terrain_edit_acceptance_decode(buffer, size, &decoded_acceptance) != HENKA_SUCCESS ||
        decoded_acceptance.server_command_id != 99U)
    {
        return 0;
    }
    if (henka_terrain_edit_rejection_encode(&rejection, buffer, sizeof(buffer), &size) != HENKA_SUCCESS ||
        henka_terrain_edit_rejection_decode(buffer, size, &decoded_rejection) != HENKA_SUCCESS ||
        decoded_rejection.reason != HENKA_TERRAIN_EDIT_REJECT_STALE_REVISION)
    {
        return 0;
    }
    delta.world_identity = 11U;
    delta.base_asset_identity = 22U;
    delta.client_nonce = 33U;
    delta.server_command_id = 99U;
    delta.command = request.command;
    delta.affected_region_count = 1U;
    delta.affected_regions[0] = (henka_terrain_network_region_revision){{2, 3}, 8U};
    if (henka_terrain_edit_delta_encode(
            &delta, buffer, HENKA_TERRAIN_NETWORK_MAX_DELTA_BYTES, &size) != HENKA_SUCCESS ||
        henka_terrain_edit_delta_decode(buffer, size, &decoded_delta) != HENKA_SUCCESS ||
        decoded_delta.server_command_id != 99U || decoded_delta.client_nonce != 33U ||
        decoded_delta.affected_regions[0].revision != 8U)
    {
        return 0;
    }
    snapshot_request.world_identity = 11U;
    snapshot_request.base_asset_identity = 22U;
    snapshot_request.region_id = (henka_terrain_region_id){2, 3};
    snapshot_request.expected_revision = 8U;
    if (henka_terrain_snapshot_request_encode(
            &snapshot_request, buffer, sizeof(buffer), &size) != HENKA_SUCCESS ||
        henka_terrain_snapshot_request_decode(buffer, size, &decoded_snapshot_request) != HENKA_SUCCESS ||
        decoded_snapshot_request.region_id.z != 3 || decoded_snapshot_request.expected_revision != 8U)
    {
        return 0;
    }
    {
        const uint8_t fragment_data[] = {1U, 2U, 3U};
        snapshot_fragment.world_identity = 11U;
        snapshot_fragment.base_asset_identity = 22U;
        snapshot_fragment.transfer_id = 44U;
        snapshot_fragment.region_id = snapshot_request.region_id;
        snapshot_fragment.revision = 8U;
        snapshot_fragment.generation = 9U;
        snapshot_fragment.fragment_index = 0U;
        snapshot_fragment.fragment_count = 2U;
        snapshot_fragment.total_bytes = 5U;
        snapshot_fragment.data_size = (uint32_t)sizeof(fragment_data);
        snapshot_fragment.data = fragment_data;
        if (henka_terrain_snapshot_fragment_encode(
                &snapshot_fragment, buffer, HENKA_NETWORK_MAX_SNAPSHOT_FRAGMENT_PAYLOAD, &size) != HENKA_SUCCESS ||
            henka_terrain_snapshot_fragment_decode(buffer, size, &decoded_snapshot_fragment) != HENKA_SUCCESS ||
            decoded_snapshot_fragment.transfer_id != 44U || decoded_snapshot_fragment.data_size != 3U ||
            decoded_snapshot_fragment.data[2] != 3U)
        {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    return test_request_and_response_codecs() ? 0 : 1;
}
