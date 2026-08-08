#include <stdint.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/terrain_server.h>

static int test_loopback_authoritative_edit(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* world = NULL;
    henka_terrain_storage* storage = NULL;
    henka_network_server* network = NULL;
    henka_network_client* client = NULL;
    henka_terrain_server* server = NULL;
    henka_network_server_desc server_desc = henka_network_server_desc_default();
    henka_network_client_desc client_desc = henka_network_client_desc_default();
    henka_terrain_server_desc terrain_server_desc;
    henka_terrain_sample* samples = NULL;
    henka_terrain_edit_request request = {0};
    henka_network_event event;
    uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_EDIT_REQUEST_BYTES];
    uint8_t response_payload[HENKA_TERRAIN_NETWORK_MAX_EDIT_RESPONSE_BYTES];
    uint8_t snapshot_request_payload[HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_REQUEST_BYTES];
    uint8_t seen_fragments[HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENTS] = {0};
    size_t payload_size = 0U;
    henka_terrain_edit_acceptance acceptance;
    henka_terrain_edit_delta delta;
    henka_terrain_snapshot_request snapshot_request = {0};
    henka_terrain_snapshot_fragment snapshot_fragment;
    int result = 0;
    int acceptance_received = 0;
    int delta_received = 0;
    int snapshot_requested = 0;
    uint32_t snapshot_fragment_count = 0U;
    uint32_t snapshot_received = 0U;
    uint32_t snapshot_total_bytes = 0U;
    uint32_t index;
    uint32_t iteration;

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
    server_desc.bind_endpoint.port = 7799U;
    client_desc.remote_endpoint = server_desc.bind_endpoint;
    if (henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &world) != HENKA_SUCCESS ||
        henka_terrain_world_reserve_region(world, (henka_terrain_region_id){0, 0}) != HENKA_SUCCESS ||
        henka_terrain_storage_create(&world_desc, "build/test_tmp/terrain_server_v1", &storage) != HENKA_SUCCESS)
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
            storage, (henka_terrain_region_id){0, 0}, 0U, 0U, samples, layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_storage_commit(storage, 1U) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_network_server_create(&server_desc, &network) != HENKA_SUCCESS ||
        henka_network_client_create(&client_desc, &client) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    terrain_server_desc = henka_terrain_server_desc_default();
    terrain_server_desc.network = network;
    terrain_server_desc.world = world;
    terrain_server_desc.storage = storage;
    if (henka_terrain_server_create(&terrain_server_desc, &server) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (iteration = 0U; iteration < 2000U; ++iteration)
    {
        if (henka_terrain_server_poll(server, 2U, 1000U + iteration) != HENKA_SUCCESS ||
            henka_network_client_poll(client, 2U, &event) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        if (event.type == HENKA_NETWORK_EVENT_CONNECTED)
        {
            break;
        }
    }
    if (iteration == 2000U)
    {
        goto cleanup;
    }
    request.world_identity = world_desc.world_identity;
    request.base_asset_identity = world_desc.base_asset_identity;
    request.client_nonce = 991U;
    request.command = henka_terrain_edit_command_default();
    request.command.center_sample_x = 100;
    request.command.center_sample_z = 100;
    request.command.radius_samples = 4U;
    request.affected_region_count = 1U;
    request.affected_regions[0] = (henka_terrain_network_region_revision){{0, 0}, 0U};
    if (henka_terrain_edit_request_encode(&request, payload, sizeof(payload), &payload_size) != HENKA_SUCCESS ||
        henka_network_client_send(
            client, HENKA_NETWORK_CHANNEL_TERRAIN, HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REQUEST,
            payload, payload_size) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    for (iteration = 0U; iteration < 2000U; ++iteration)
    {
        if (henka_terrain_server_poll(server, 2U, 2000U + iteration) != HENKA_SUCCESS ||
            henka_network_client_poll(client, 2U, &event) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        if (event.type == HENKA_NETWORK_EVENT_MESSAGE &&
            event.message.type == HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_ACCEPTED)
        {
            if (event.message.payload_size > sizeof(response_payload))
            {
                goto cleanup;
            }
            memcpy(response_payload, event.message.payload, event.message.payload_size);
            if (henka_terrain_edit_acceptance_decode(
                    response_payload, event.message.payload_size, &acceptance) != HENKA_SUCCESS ||
                acceptance.client_nonce != request.client_nonce || acceptance.server_command_id != 1U)
            {
                goto cleanup;
            }
            acceptance_received = 1;
        }
        if (event.type == HENKA_NETWORK_EVENT_MESSAGE &&
            event.message.type == HENKA_NETWORK_MESSAGE_TERRAIN_DELTA)
        {
            if (event.message.payload_size > sizeof(response_payload) ||
                henka_terrain_edit_delta_decode(
                    event.message.payload, event.message.payload_size, &delta) != HENKA_SUCCESS ||
                delta.server_command_id != 1U || delta.affected_regions[0].revision != 1U)
            {
                goto cleanup;
            }
            if (acceptance_received)
            {
                delta_received = 1;
                snapshot_request.world_identity = world_desc.world_identity;
                snapshot_request.base_asset_identity = world_desc.base_asset_identity;
                snapshot_request.region_id = (henka_terrain_region_id){0, 0};
                snapshot_request.expected_revision = 1U;
                if (!snapshot_requested &&
                    henka_terrain_snapshot_request_encode(
                        &snapshot_request, snapshot_request_payload,
                        sizeof(snapshot_request_payload), &payload_size) == HENKA_SUCCESS &&
                    henka_network_client_send(
                        client, HENKA_NETWORK_CHANNEL_SNAPSHOT,
                        HENKA_NETWORK_MESSAGE_SNAPSHOT_REQUEST,
                        snapshot_request_payload, payload_size) == HENKA_SUCCESS)
                {
                    snapshot_requested = 1;
                }
            }
        }
        if (event.type == HENKA_NETWORK_EVENT_MESSAGE &&
            event.message.type == HENKA_NETWORK_MESSAGE_SNAPSHOT_FRAGMENT)
        {
            if (henka_terrain_snapshot_fragment_decode(
                    event.message.payload, event.message.payload_size, &snapshot_fragment) != HENKA_SUCCESS ||
                !delta_received || snapshot_fragment.transfer_id == 0U ||
                snapshot_fragment.region_id.x != 0 || snapshot_fragment.region_id.z != 0 ||
                snapshot_fragment.revision != 1U ||
                snapshot_fragment.fragment_index >= HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENTS)
            {
                goto cleanup;
            }
            if (snapshot_fragment_count == 0U)
            {
                snapshot_fragment_count = snapshot_fragment.fragment_count;
                snapshot_total_bytes = snapshot_fragment.total_bytes;
            }
            if (snapshot_fragment_count != snapshot_fragment.fragment_count ||
                snapshot_total_bytes != snapshot_fragment.total_bytes ||
                seen_fragments[snapshot_fragment.fragment_index] != 0U)
            {
                goto cleanup;
            }
            seen_fragments[snapshot_fragment.fragment_index] = 1U;
            ++snapshot_received;
            if (snapshot_received == snapshot_fragment_count)
            {
                result = 1;
                break;
            }
        }
    }

cleanup:
    henka_terrain_server_destroy(server);
    henka_network_client_destroy(client);
    henka_network_server_destroy(network);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(world);
    henka_free(samples);
    return result;
}

int main(void)
{
    return test_loopback_authoritative_edit() ? 0 : 1;
}
