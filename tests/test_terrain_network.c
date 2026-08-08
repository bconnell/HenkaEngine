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
    return 1;
}

int main(void)
{
    return test_request_and_response_codecs() ? 0 : 1;
}
