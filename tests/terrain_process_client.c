#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <henka/terrain_client.h>

static int parse_uint(const char* text, unsigned long maximum, unsigned long* out_value)
{
    char* end = NULL;
    unsigned long value;
    if (text == NULL || out_value == NULL || text[0] == '\0')
    {
        return 0;
    }
    value = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || value > maximum)
    {
        return 0;
    }
    *out_value = value;
    return 1;
}

static uint64_t hash_terrain_region(const henka_terrain_world* world)
{
    const henka_terrain_sample* samples = NULL;
    size_t sample_count = 0U;
    size_t index;
    uint64_t hash = UINT64_C(1469598103934665603);

    if (world == NULL ||
        henka_terrain_world_get_region_samples(
            world, (henka_terrain_region_id){0, 0}, &samples, &sample_count) != HENKA_SUCCESS ||
        samples == NULL)
    {
        return 0U;
    }
    for (index = 0U; index < sample_count; ++index)
    {
        const uint8_t* bytes = (const uint8_t*)&samples[index];
        size_t byte_index;
        for (byte_index = 0U; byte_index < sizeof(samples[index].height_millimeters); ++byte_index)
        {
            hash ^= bytes[byte_index];
            hash *= UINT64_C(1099511628211);
        }
        for (byte_index = 0U; byte_index < sizeof(samples[index].material_weights); ++byte_index)
        {
            hash ^= samples[index].material_weights[byte_index];
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

int main(int argc, char** argv)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_network_client_desc network_desc = henka_network_client_desc_default();
    henka_terrain_world* world = NULL;
    henka_network_client* network = NULL;
    henka_terrain_client* client = NULL;
    henka_terrain_client_desc client_desc;
    henka_terrain_client_diagnostics diagnostics;
    henka_terrain_region_state region;
    unsigned long port = 7777UL;
    unsigned long delay_milliseconds = 0UL;
    unsigned long nonce = 1001UL;
    const char* mode = "edit";
    uint32_t elapsed_milliseconds = 0U;
    uint32_t event_count = 0U;
    int edit_sent = 0;
    int response_received = 0;
    int reconnect_requested = 0;
    int reconnect_complete = 0;
    uint32_t iteration;
    int result = 1;

    if ((argc > 1 && (!parse_uint(argv[1], 65535UL, &port) || port == 0UL)) ||
        (argc > 2 && !parse_uint(argv[2], 5000UL, &delay_milliseconds)) ||
        (argc > 3 && (!parse_uint(argv[3], UINT32_MAX, &nonce) || nonce == 0UL)) ||
        (argc > 4 &&
            (strcmp(argv[4], "edit") != 0 &&
             strcmp(argv[4], "observe") != 0 &&
             strcmp(argv[4], "reconnect") != 0)) ||
        argc > 5)
    {
        fprintf(stderr, "usage: terrain_process_client [port] [delay-ms] [nonce] [edit|observe|reconnect]\n");
        return 2;
    }
    if (argc > 4)
    {
        mode = argv[4];
    }
    network_desc.remote_endpoint.port = (uint16_t)port;
    if (henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_network_client_create(&network_desc, &network) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    client_desc = henka_terrain_client_desc_default();
    client_desc.network = network;
    client_desc.world = world;
    client_desc.session_interest_enabled = true;
    client_desc.session_center_region = (henka_terrain_region_id){0, 0};
    client_desc.session_radius_regions = 0U;
    client_desc.session_max_regions = 1U;
    if (henka_terrain_client_create(&client_desc, &client) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (iteration = 0U; iteration < 6000U; ++iteration)
    {
        if (henka_terrain_client_poll(client, 2U, 8U, &event_count) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(client, &diagnostics);
        if (strcmp(mode, "observe") == 0 && diagnostics.completed_snapshot_count != 0U)
        {
            response_received = 1;
        }
        if (strcmp(mode, "observe") != 0 && !edit_sent && diagnostics.completed_snapshot_count != 0U &&
            elapsed_milliseconds >= delay_milliseconds &&
            henka_terrain_world_get_region_state(
                world, (henka_terrain_region_id){0, 0}, &region) == HENKA_SUCCESS)
        {
            henka_terrain_edit_request request = {0};
            request.world_identity = world_desc.world_identity;
            request.base_asset_identity = world_desc.base_asset_identity;
            request.client_nonce = nonce;
            request.command = henka_terrain_edit_command_default();
            request.command.client_nonce = nonce;
            request.command.center_sample_x = 32;
            request.command.center_sample_z = 32;
            request.command.radius_samples = 2U;
            request.command.value_millimeters = 10;
            request.affected_region_count = 1U;
            request.affected_regions[0] =
                (henka_terrain_network_region_revision){{0, 0}, region.revision};
            if (henka_terrain_client_send_edit_request(client, &request) != HENKA_SUCCESS)
            {
                goto cleanup;
            }
            edit_sent = 1;
        }
        if (edit_sent &&
            (diagnostics.rejection_count != 0U ||
             (diagnostics.acceptance_count != 0U && diagnostics.applied_delta_count != 0U)))
        {
            response_received = 1;
            if (strcmp(mode, "reconnect") == 0 && !reconnect_requested)
            {
                if (henka_terrain_client_reconnect(client) != HENKA_SUCCESS)
                {
                    goto cleanup;
                }
                reconnect_requested = 1;
            }
        }
        if (response_received &&
            (strcmp(mode, "reconnect") != 0 ||
             (diagnostics.connected_event_count >= 2U &&
              diagnostics.session_interest_region_count >= diagnostics.connected_event_count)))
        {
            reconnect_complete = strcmp(mode, "reconnect") == 0 ? 1 : 0;
            break;
        }
        elapsed_milliseconds += 2U;
    }
    if (diagnostics.connected_event_count != 0U &&
        diagnostics.completed_snapshot_count != 0U && response_received &&
        (strcmp(mode, "reconnect") != 0 || reconnect_complete != 0))
    {
        printf(
            "terrain process client connected=%llu disconnected=%llu snapshots=%llu accepted=%llu rejected=%llu session-interest=%llu filtered-regions=%llu revision=%llu checksum=%llu mode=%s\n",
            (unsigned long long)diagnostics.connected_event_count,
            (unsigned long long)diagnostics.disconnected_event_count,
            (unsigned long long)diagnostics.completed_snapshot_count,
            (unsigned long long)diagnostics.acceptance_count,
            (unsigned long long)diagnostics.rejection_count,
            (unsigned long long)diagnostics.session_interest_request_count,
            (unsigned long long)diagnostics.session_interest_region_count,
            (unsigned long long)(henka_terrain_world_get_region_state(
                world, (henka_terrain_region_id){0, 0}, &region) == HENKA_SUCCESS
                ? region.revision : 0U),
            (unsigned long long)hash_terrain_region(world),
            mode);
        result = 0;
    }
    else
    {
        fprintf(
            stderr,
            "terrain process client incomplete connected=%llu disconnected=%llu snapshots=%llu accepted=%llu rejected=%llu session-interest=%llu filtered-regions=%llu response=%d reconnect_requested=%d\n",
            (unsigned long long)diagnostics.connected_event_count,
            (unsigned long long)diagnostics.disconnected_event_count,
            (unsigned long long)diagnostics.completed_snapshot_count,
            (unsigned long long)diagnostics.acceptance_count,
            (unsigned long long)diagnostics.rejection_count,
            (unsigned long long)diagnostics.session_interest_request_count,
            (unsigned long long)diagnostics.session_interest_region_count,
            response_received,
            reconnect_requested);
    }

cleanup:
    henka_terrain_client_destroy(client);
    henka_network_client_destroy(network);
    henka_terrain_world_destroy(world);
    return result;
}
