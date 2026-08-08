#include <henka/memory.h>
#include <henka/terrain_client.h>
#include <henka/terrain_server.h>

static int test_client_snapshot_and_delta_path(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* server_world = NULL;
    henka_terrain_world* client_world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_network_server* network_server = NULL;
    henka_network_client* network_client = NULL;
    henka_terrain_server* terrain_server = NULL;
    henka_terrain_client* terrain_client = NULL;
    henka_terrain_sample* samples = NULL;
    henka_network_server_desc server_desc = henka_network_server_desc_default();
    henka_network_client_desc client_desc = henka_network_client_desc_default();
    henka_terrain_server_desc server_config;
    henka_terrain_client_desc client_config;
    henka_terrain_client_diagnostics client_diagnostics;
    henka_terrain_edit_request edit_request = {0};
    henka_terrain_region_state state;
    uint32_t event_count;
    uint32_t index;
    uint32_t iteration;
    int result = 0;

    world_desc.max_resident_regions = 1U;
    server_desc.bind_endpoint.host[0] = '1';
    server_desc.bind_endpoint.host[1] = '2';
    server_desc.bind_endpoint.host[2] = '7';
    server_desc.bind_endpoint.host[3] = '.';
    server_desc.bind_endpoint.host[4] = '0';
    server_desc.bind_endpoint.host[5] = '.';
    server_desc.bind_endpoint.host[6] = '0';
    server_desc.bind_endpoint.host[7] = '.';
    server_desc.bind_endpoint.host[8] = '1';
    server_desc.bind_endpoint.port = 7801U;
    client_desc.remote_endpoint = server_desc.bind_endpoint;
    if (henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &server_world) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &client_world) != HENKA_SUCCESS ||
        henka_terrain_storage_create(&world_desc, "build/test_tmp/terrain_client_v1", &storage) != HENKA_SUCCESS)
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
        henka_terrain_storage_write_region(
            storage, (henka_terrain_region_id){0, 0}, 0U, 0U,
            samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_storage_commit(storage, 1U) != HENKA_SUCCESS ||
        henka_network_server_create(&server_desc, &network_server) != HENKA_SUCCESS ||
        henka_network_client_create(&client_desc, &network_client) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    server_config = henka_terrain_server_desc_default();
    server_config.network = network_server;
    server_config.world = server_world;
    server_config.storage = storage;
    if (henka_terrain_server_create(&server_config, &terrain_server) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    client_config = henka_terrain_client_desc_default();
    client_config.network = network_client;
    client_config.world = client_world;
    if (henka_terrain_client_create(&client_config, &terrain_client) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (iteration = 0U; iteration < 2000U; ++iteration)
    {
        if (henka_terrain_server_poll(terrain_server, 2U, 1000U + iteration) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client, 2U, 4U, &event_count) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
        if (client_diagnostics.connected_event_count != 0U)
        {
            break;
        }
    }
    if (iteration == 2000U ||
        henka_terrain_client_request_snapshot(
            terrain_client,
            (henka_terrain_snapshot_request){
                world_desc.world_identity, world_desc.base_asset_identity, {0, 0}, 0U}) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (iteration = 0U; iteration < 2000U; ++iteration)
    {
        if (henka_terrain_server_poll(terrain_server, 2U, 3000U + iteration) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client, 2U, 8U, &event_count) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
        if (client_diagnostics.completed_snapshot_count == 1U)
        {
            break;
        }
    }
    if (iteration == 2000U ||
        henka_terrain_world_get_region_state(client_world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        state.revision != 0U)
    {
        goto cleanup;
    }
    edit_request.world_identity = world_desc.world_identity;
    edit_request.base_asset_identity = world_desc.base_asset_identity;
    edit_request.client_nonce = 404U;
    edit_request.command = henka_terrain_edit_command_default();
    edit_request.command.center_sample_x = 100;
    edit_request.command.center_sample_z = 100;
    edit_request.command.radius_samples = 4U;
    edit_request.affected_region_count = 1U;
    edit_request.affected_regions[0] =
        (henka_terrain_network_region_revision){{0, 0}, 0U};
    if (henka_terrain_client_send_edit_request(terrain_client, &edit_request) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (iteration = 0U; iteration < 2000U; ++iteration)
    {
        if (henka_terrain_server_poll(terrain_server, 2U, 5000U + iteration) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client, 2U, 8U, &event_count) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
        if (client_diagnostics.applied_delta_count == 1U)
        {
            break;
        }
    }
    if (iteration == 2000U || !client_diagnostics.last_acceptance_valid ||
        client_diagnostics.last_acceptance.client_nonce != edit_request.client_nonce ||
        henka_terrain_world_get_region_state(client_world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        state.revision != 1U)
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    henka_terrain_client_destroy(terrain_client);
    henka_terrain_server_destroy(terrain_server);
    henka_network_client_destroy(network_client);
    henka_network_server_destroy(network_server);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(client_world);
    henka_terrain_world_destroy(server_world);
    henka_free(samples);
    return result;
}

int main(void)
{
    return test_client_snapshot_and_delta_path() ? 0 : 1;
}
