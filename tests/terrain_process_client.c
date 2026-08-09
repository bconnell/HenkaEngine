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
    uint32_t elapsed_milliseconds = 0U;
    uint32_t event_count = 0U;
    int edit_sent = 0;
    int response_received = 0;
    uint32_t iteration;
    int result = 1;

    if ((argc > 1 && (!parse_uint(argv[1], 65535UL, &port) || port == 0UL)) ||
        (argc > 2 && !parse_uint(argv[2], 5000UL, &delay_milliseconds)) ||
        (argc > 3 && (!parse_uint(argv[3], UINT32_MAX, &nonce) || nonce == 0UL)) ||
        argc > 4)
    {
        fprintf(stderr, "usage: terrain_process_client [port] [delay-ms] [nonce]\n");
        return 2;
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
        if (!edit_sent && diagnostics.completed_snapshot_count != 0U &&
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
            request.command.center_sample_x = 100;
            request.command.center_sample_z = 100;
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
            (diagnostics.acceptance_count != 0U || diagnostics.rejection_count != 0U))
        {
            response_received = 1;
            break;
        }
        elapsed_milliseconds += 2U;
    }
    if (diagnostics.connected_event_count != 0U &&
        diagnostics.completed_snapshot_count != 0U && response_received)
    {
        printf(
            "terrain process client connected snapshots=%llu accepted=%llu rejected=%llu revision=%llu\n",
            (unsigned long long)diagnostics.completed_snapshot_count,
            (unsigned long long)diagnostics.acceptance_count,
            (unsigned long long)diagnostics.rejection_count,
            (unsigned long long)(henka_terrain_world_get_region_state(
                world, (henka_terrain_region_id){0, 0}, &region) == HENKA_SUCCESS
                ? region.revision : 0U));
        result = 0;
    }

cleanup:
    henka_terrain_client_destroy(client);
    henka_network_client_destroy(network);
    henka_terrain_world_destroy(world);
    return result;
}
