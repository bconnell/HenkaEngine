#include <henka/memory.h>
#include <henka/terrain_client.h>
#include <henka/terrain_server.h>

#include <stdio.h>
#include <string.h>

static henka_result poll_terrain_server_capture_peer(
    henka_terrain_server* server,
    henka_network_server* network,
    uint32_t timeout_milliseconds,
    uint64_t now_milliseconds,
    henka_network_peer_id* in_out_peer_id)
{
    henka_network_event event;
    henka_result result = henka_network_server_poll(network, timeout_milliseconds, &event);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (event.type == HENKA_NETWORK_EVENT_CONNECTED && in_out_peer_id != NULL)
    {
        *in_out_peer_id = event.peer_id;
    }
    return henka_terrain_server_handle_event(server, &event, now_milliseconds);
}

static int terrain_region_samples_equal(
    const henka_terrain_world* first,
    const henka_terrain_world* second,
    henka_terrain_region_id region_id)
{
    const henka_terrain_sample* first_samples = NULL;
    const henka_terrain_sample* second_samples = NULL;
    size_t first_count = 0U;
    size_t second_count = 0U;
    if (henka_terrain_world_get_region_samples(
            first, region_id, &first_samples, &first_count) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(
            second, region_id, &second_samples, &second_count) != HENKA_SUCCESS ||
        first_count != second_count)
    {
        return 0;
    }
    return memcmp(first_samples, second_samples, first_count * sizeof(*first_samples)) == 0;
}

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
    henka_terrain_snapshot_fragment snapshot_fragment;
    henka_network_event snapshot_event;
    henka_network_event snapshot_failure_event;
    henka_network_event gap_event;
    henka_network_event session_info_event;
    henka_terrain_session_info session_info;
    henka_terrain_edit_delta gap_delta;
    henka_terrain_edit_delta invalid_identity_delta;
    uint8_t* encoded_record = NULL;
    uint8_t* corrupted_record = NULL;
    uint8_t* fragment_payload = NULL;
    uint8_t gap_payload[HENKA_TERRAIN_NETWORK_MAX_DELTA_BYTES];
    uint8_t session_info_payload[1024];
    uint8_t snapshot_failure_payload[HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FAILURE_BYTES];
    size_t encoded_record_size = 0U;
    size_t gap_payload_size = 0U;
    size_t session_info_payload_size = 0U;
    size_t snapshot_failure_payload_size = 0U;
    uint32_t snapshot_fragment_count;
    size_t snapshot_offset;
    henka_terrain_region_state state;
    uint32_t event_count;
    uint32_t index;
    uint32_t iteration = 0U;
    henka_network_peer_id server_peer_id = HENKA_NETWORK_INVALID_PEER_ID;
    const char* failure_stage = "setup";
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
        henka_terrain_world_apply_region_snapshot(
            server_world,
            (henka_terrain_region_storage_info){{0, 0}, 7U, 3U},
            samples,
            layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_apply_region_snapshot(
            client_world,
            (henka_terrain_region_storage_info){{0, 0}, 0U, 0U},
            samples,
            layout.samples_per_region) != HENKA_SUCCESS ||
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
    client_config.session_interest_enabled = true;
    client_config.session_center_region = (henka_terrain_region_id){0, 0};
    client_config.session_radius_regions = 0U;
    client_config.session_max_regions = 1U;
    if (henka_terrain_client_create(&client_config, &terrain_client) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    failure_stage = "initial connection";
    for (iteration = 0U; iteration < 2000U; ++iteration)
    {
        if (poll_terrain_server_capture_peer(
                terrain_server, network_server, 2U, 1000U + iteration, &server_peer_id) != HENKA_SUCCESS ||
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
    if (iteration == 2000U)
    {
        goto cleanup;
    }
    failure_stage = "initial snapshot";
    for (iteration = 0U; iteration < 2000U; ++iteration)
    {
        if (poll_terrain_server_capture_peer(
                terrain_server, network_server, 2U, 3000U + iteration, &server_peer_id) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client, 2U, 8U, &event_count) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
        if (client_diagnostics.completed_snapshot_count == 1U &&
            client_diagnostics.session_snapshot_request_count == 1U &&
            client_diagnostics.session_interest_request_count == 1U &&
            client_diagnostics.session_interest_region_count >= 1U)
        {
            break;
        }
    }
    if (iteration == 2000U ||
        henka_terrain_world_get_region_state(client_world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        state.revision != 7U)
    {
        goto cleanup;
    }
    session_info = (henka_terrain_session_info){
        world_desc.world_identity,
        world_desc.base_asset_identity,
        1U,
        {{{0, 0}, 8U, 4U}},
        HENKA_TERRAIN_SESSION_INFO_FLAG_RELEVANCE_FILTERED};
    session_info_event = (henka_network_event){
        HENKA_NETWORK_EVENT_MESSAGE,
        HENKA_NETWORK_INVALID_PEER_ID,
        HENKA_NETWORK_DISCONNECT_REASON_NONE,
        {HENKA_NETWORK_CHANNEL_CONTROL,
         HENKA_NETWORK_MESSAGE_CONNECT,
         session_info_payload,
         0U}};
    if (henka_terrain_session_info_encode(
            &session_info,
            session_info_payload,
            sizeof(session_info_payload),
            &session_info_payload_size) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    session_info_event.message.payload_size = (uint32_t)session_info_payload_size;
    if (henka_terrain_client_handle_event(terrain_client, &session_info_event) != HENKA_SUCCESS ||
        henka_terrain_client_handle_event(terrain_client, &session_info_event) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
    if (client_diagnostics.session_snapshot_request_count != 2U ||
        client_diagnostics.session_snapshot_suppressed_count != 1U)
    {
        goto cleanup;
    }
    session_info.regions[0].revision = 9U;
    session_info.regions[0].generation = 5U;
    if (henka_terrain_session_info_encode(
            &session_info,
            session_info_payload,
            sizeof(session_info_payload),
            &session_info_payload_size) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    session_info_event.message.payload_size = (uint32_t)session_info_payload_size;
    if (henka_terrain_client_handle_event(terrain_client, &session_info_event) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
    if (client_diagnostics.session_snapshot_request_count != 3U)
    {
        goto cleanup;
    }
    {
        henka_terrain_snapshot_failure failure = {
            world_desc.world_identity,
            world_desc.base_asset_identity,
            (henka_terrain_region_id){0, 0},
            9U,
            HENKA_TERRAIN_SNAPSHOT_FAILURE_STORAGE};
        if (henka_terrain_snapshot_failure_encode(
                &failure, snapshot_failure_payload, sizeof(snapshot_failure_payload),
                &snapshot_failure_payload_size) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        snapshot_failure_event = (henka_network_event){
            HENKA_NETWORK_EVENT_MESSAGE,
            HENKA_NETWORK_INVALID_PEER_ID,
            HENKA_NETWORK_DISCONNECT_REASON_NONE,
            {HENKA_NETWORK_CHANNEL_SNAPSHOT,
             HENKA_NETWORK_MESSAGE_SNAPSHOT_FAILED,
             snapshot_failure_payload,
             (uint32_t)snapshot_failure_payload_size}};
        if (henka_terrain_client_handle_event(
                terrain_client, &snapshot_failure_event) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
        if (client_diagnostics.snapshot_failure_count != 1U)
        {
            goto cleanup;
        }
        if (henka_terrain_client_handle_event(
                terrain_client, &snapshot_failure_event) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
        if (client_diagnostics.session_snapshot_suppressed_count != 1U)
        {
            goto cleanup;
        }
    }
    encoded_record = henka_malloc(HENKA_TERRAIN_MAX_REGION_RECORD_BYTES);
    corrupted_record = henka_malloc(HENKA_TERRAIN_MAX_REGION_RECORD_BYTES);
    fragment_payload = henka_malloc(HENKA_NETWORK_MAX_SNAPSHOT_FRAGMENT_PAYLOAD);
    if (encoded_record == NULL || corrupted_record == NULL || fragment_payload == NULL ||
        henka_terrain_region_encode(
            &world_desc,
            (henka_terrain_region_id){0, 0},
            7U,
            3U,
            samples,
            layout.samples_per_region,
            encoded_record,
            HENKA_TERRAIN_MAX_REGION_RECORD_BYTES,
            &encoded_record_size) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    memcpy(corrupted_record, encoded_record, encoded_record_size);
    corrupted_record[0] ^= 0x5AU;
    snapshot_fragment_count = (uint32_t)((encoded_record_size + HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES - 1U) /
        HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES);
    snapshot_offset = 0U;
    for (index = 0U; index < snapshot_fragment_count; ++index)
    {
        size_t remaining = encoded_record_size - snapshot_offset;
        size_t data_size = remaining > HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES
            ? HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES : remaining;
        size_t payload_size = 0U;
        snapshot_fragment = (henka_terrain_snapshot_fragment){
            world_desc.world_identity,
            world_desc.base_asset_identity,
            500U,
            {0, 0},
            7U,
            3U,
            index,
            snapshot_fragment_count,
            (uint32_t)encoded_record_size,
            (uint32_t)data_size,
            corrupted_record + snapshot_offset};
        snapshot_event = (henka_network_event){
            HENKA_NETWORK_EVENT_MESSAGE,
            HENKA_NETWORK_INVALID_PEER_ID,
            HENKA_NETWORK_DISCONNECT_REASON_NONE,
            {HENKA_NETWORK_CHANNEL_SNAPSHOT,
             HENKA_NETWORK_MESSAGE_SNAPSHOT_FRAGMENT,
             fragment_payload,
             0U}};
        if (henka_terrain_snapshot_fragment_encode(
                &snapshot_fragment,
                fragment_payload,
                HENKA_NETWORK_MAX_SNAPSHOT_FRAGMENT_PAYLOAD,
                &payload_size) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        snapshot_event.message.payload_size = (uint32_t)payload_size;
        if (henka_terrain_client_handle_event(terrain_client, &snapshot_event) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        snapshot_offset += data_size;
    }
    henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
    if (client_diagnostics.recovery_snapshot_request_count != 1U ||
        client_diagnostics.completed_snapshot_count != 1U)
    {
        goto cleanup;
    }
    failure_stage = "corrupt snapshot recovery";
    for (iteration = 0U; iteration < 2000U; ++iteration)
    {
        if (poll_terrain_server_capture_peer(
                terrain_server, network_server, 2U, 4000U + iteration, &server_peer_id) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client, 2U, 8U, &event_count) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
        if (client_diagnostics.completed_snapshot_count >= 2U)
        {
            break;
        }
    }
    if (iteration == 2000U)
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
        (henka_terrain_network_region_revision){{0, 0}, 7U};
    if (henka_terrain_client_send_edit_request(terrain_client, &edit_request) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    failure_stage = "delta edit";
    for (iteration = 0U; iteration < 2000U; ++iteration)
    {
        if (poll_terrain_server_capture_peer(
                terrain_server, network_server, 2U, 5000U + iteration, &server_peer_id) != HENKA_SUCCESS ||
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
        state.revision != 8U)
    {
        goto cleanup;
    }
    gap_delta = (henka_terrain_edit_delta){0};
    gap_delta.world_identity = world_desc.world_identity;
    gap_delta.base_asset_identity = world_desc.base_asset_identity;
    gap_delta.server_command_id = 100U;
    gap_delta.command = henka_terrain_edit_command_default();
    gap_delta.command.center_sample_x = 100;
    gap_delta.command.center_sample_z = 100;
    gap_delta.command.radius_samples = 4U;
    gap_delta.affected_region_count = 1U;
    gap_delta.affected_regions[0] =
        (henka_terrain_network_region_revision){{0, 0}, 10U};
    gap_event = (henka_network_event){
        HENKA_NETWORK_EVENT_MESSAGE,
        HENKA_NETWORK_INVALID_PEER_ID,
        HENKA_NETWORK_DISCONNECT_REASON_NONE,
        {HENKA_NETWORK_CHANNEL_TERRAIN,
         HENKA_NETWORK_MESSAGE_TERRAIN_DELTA,
         gap_payload,
         0U}};
    if (henka_terrain_edit_delta_encode(
            &gap_delta, gap_payload, sizeof(gap_payload), &gap_payload_size) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    invalid_identity_delta = gap_delta;
    invalid_identity_delta.world_identity += 1U;
    if (henka_terrain_edit_delta_encode(
            &invalid_identity_delta, gap_payload, sizeof(gap_payload), &gap_payload_size) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    gap_event.message.payload_size = (uint32_t)gap_payload_size;
    if (henka_terrain_client_handle_event(terrain_client, &gap_event) != HENKA_ERROR_INVALID_ARGUMENT)
    {
        goto cleanup;
    }
    henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
    if (client_diagnostics.recovery_delta_request_count != 0U ||
        client_diagnostics.rejected_delta_count == 0U)
    {
        goto cleanup;
    }
    if (henka_terrain_edit_delta_encode(
            &gap_delta, gap_payload, sizeof(gap_payload), &gap_payload_size) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    gap_event.message.payload_size = (uint32_t)gap_payload_size;
    if (henka_terrain_client_handle_event(terrain_client, &gap_event) != HENKA_SUCCESS ||
        henka_terrain_client_handle_event(terrain_client, &gap_event) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
    if (client_diagnostics.recovery_delta_request_count != 1U ||
        client_diagnostics.recovery_delta_suppressed_count != 1U ||
        client_diagnostics.pending_recovery_count != 1U)
    {
        goto cleanup;
    }
    if (server_peer_id == HENKA_NETWORK_INVALID_PEER_ID ||
        henka_network_server_disconnect(
            network_server, server_peer_id, HENKA_NETWORK_DISCONNECT_REASON_APPLICATION) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    failure_stage = "disconnect";
    for (iteration = 0U; iteration < 2000U; ++iteration)
    {
        if (poll_terrain_server_capture_peer(
                terrain_server, network_server, 2U, 6000U + iteration, &server_peer_id) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client, 2U, 8U, &event_count) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
        if (client_diagnostics.disconnected_event_count >= 1U)
        {
            break;
        }
    }
    if (iteration == 2000U)
    {
        goto cleanup;
    }
    henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
    if (client_diagnostics.pending_recovery_count != 0U ||
        henka_terrain_client_reconnect(terrain_client) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
    if (client_diagnostics.pending_recovery_count != 0U)
    {
        goto cleanup;
    }
    failure_stage = "reconnect";
    for (iteration = 0U; iteration < 2000U; ++iteration)
    {
        if (poll_terrain_server_capture_peer(
                terrain_server, network_server, 2U, 8000U + iteration, &server_peer_id) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client, 2U, 8U, &event_count) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
        if (client_diagnostics.connected_event_count >= 2U)
        {
            break;
        }
    }
    if (iteration == 2000U ||
        henka_terrain_world_get_region_state(client_world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        state.revision != 8U ||
        !terrain_region_samples_equal(
            server_world, client_world, (henka_terrain_region_id){0, 0}))
    {
        goto cleanup;
    }

    henka_terrain_server_destroy(terrain_server);
    terrain_server = NULL;
    henka_network_server_destroy(network_server);
    network_server = NULL;
    if (henka_network_server_create(&server_desc, &network_server) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    server_config.network = network_server;
    if (henka_terrain_server_create(&server_config, &terrain_server) != HENKA_SUCCESS ||
        henka_terrain_client_reconnect(terrain_client) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    failure_stage = "server restart";
    for (iteration = 0U; iteration < 2000U; ++iteration)
    {
        if (poll_terrain_server_capture_peer(
                terrain_server, network_server, 2U, 10000U + iteration, &server_peer_id) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client, 2U, 8U, &event_count) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
        if (client_diagnostics.connected_event_count >= 3U)
        {
            break;
        }
    }
    if (iteration == 2000U ||
        henka_terrain_world_get_region_state(client_world, (henka_terrain_region_id){0, 0}, &state) != HENKA_SUCCESS ||
        state.revision != 8U ||
        !terrain_region_samples_equal(
            server_world, client_world, (henka_terrain_region_id){0, 0}))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    if (!result)
    {
        henka_terrain_client_get_diagnostics(terrain_client, &client_diagnostics);
        fprintf(stderr, "terrain client test failed during %s at iteration %u (connected=%llu snapshots=%llu deltas=%llu recovery=%llu disconnects=%llu)\n",
            failure_stage,
            (unsigned int)iteration,
            (unsigned long long)client_diagnostics.connected_event_count,
            (unsigned long long)client_diagnostics.completed_snapshot_count,
            (unsigned long long)client_diagnostics.applied_delta_count,
            (unsigned long long)client_diagnostics.recovery_snapshot_request_count,
            (unsigned long long)client_diagnostics.disconnected_event_count);
    }
    henka_terrain_client_destroy(terrain_client);
    henka_terrain_server_destroy(terrain_server);
    henka_network_client_destroy(network_client);
    henka_network_server_destroy(network_server);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(client_world);
    henka_terrain_world_destroy(server_world);
    henka_free(fragment_payload);
    henka_free(corrupted_record);
    henka_free(encoded_record);
    henka_free(samples);
    return result;
}

static int test_two_client_authoritative_convergence(void)
{
    henka_terrain_world_desc world_desc = henka_terrain_world_desc_default();
    henka_terrain_layout layout;
    henka_terrain_world* server_world = NULL;
    henka_terrain_world* client_world_a = NULL;
    henka_terrain_world* client_world_b = NULL;
    henka_terrain_storage* storage = NULL;
    henka_network_server* network_server = NULL;
    henka_network_client* network_client_a = NULL;
    henka_network_client* network_client_b = NULL;
    henka_terrain_server* terrain_server = NULL;
    henka_terrain_client* terrain_client_a = NULL;
    henka_terrain_client* terrain_client_b = NULL;
    henka_terrain_sample* samples = NULL;
    henka_network_server_desc server_desc = henka_network_server_desc_default();
    henka_network_client_desc client_desc = henka_network_client_desc_default();
    henka_terrain_server_desc server_config;
    henka_terrain_client_desc client_config;
    henka_terrain_client_diagnostics diagnostics_a;
    henka_terrain_client_diagnostics diagnostics_b;
    henka_terrain_edit_request request;
    henka_terrain_region_state state_a;
    henka_terrain_region_state state_b;
    uint32_t event_count;
    uint32_t index;
    uint32_t iteration = 0U;
    const char* failure_stage = "setup";
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
    server_desc.bind_endpoint.port = 7802U;
    client_desc.remote_endpoint = server_desc.bind_endpoint;
    if (henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &server_world) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &client_world_a) != HENKA_SUCCESS ||
        henka_terrain_world_create(&world_desc, &client_world_b) != HENKA_SUCCESS ||
        henka_terrain_storage_create(&world_desc, "build/test_tmp/terrain_two_client_v1", &storage) != HENKA_SUCCESS)
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
        henka_terrain_world_apply_region_snapshot(
            server_world,
            (henka_terrain_region_storage_info){{0, 0}, 7U, 3U},
            samples,
            layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_apply_region_snapshot(
            client_world_a,
            (henka_terrain_region_storage_info){{0, 0}, 0U, 0U},
            samples,
            layout.samples_per_region) != HENKA_SUCCESS ||
        henka_terrain_world_apply_region_snapshot(
            client_world_b,
            (henka_terrain_region_storage_info){{0, 0}, 0U, 0U},
            samples,
            layout.samples_per_region) != HENKA_SUCCESS ||
        henka_network_server_create(&server_desc, &network_server) != HENKA_SUCCESS ||
        henka_network_client_create(&client_desc, &network_client_a) != HENKA_SUCCESS ||
        henka_network_client_create(&client_desc, &network_client_b) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    server_config = henka_terrain_server_desc_default();
    server_config.network = network_server;
    server_config.world = server_world;
    server_config.storage = storage;
    client_config = henka_terrain_client_desc_default();
    client_config.world = client_world_a;
    client_config.network = network_client_a;
    if (henka_terrain_server_create(&server_config, &terrain_server) != HENKA_SUCCESS ||
        henka_terrain_client_create(&client_config, &terrain_client_a) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    client_config.world = client_world_b;
    client_config.network = network_client_b;
    if (henka_terrain_client_create(&client_config, &terrain_client_b) != HENKA_SUCCESS)
    {
        goto cleanup;
    }

    failure_stage = "two-client initial snapshots";
    for (iteration = 0U; iteration < 3000U; ++iteration)
    {
        if (henka_terrain_server_poll(terrain_server, 2U, 11000U + iteration) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client_a, 2U, 8U, &event_count) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client_b, 2U, 8U, &event_count) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client_a, &diagnostics_a);
        henka_terrain_client_get_diagnostics(terrain_client_b, &diagnostics_b);
        if (diagnostics_a.connected_event_count >= 1U &&
            diagnostics_b.connected_event_count >= 1U &&
            diagnostics_a.completed_snapshot_count >= 1U &&
            diagnostics_b.completed_snapshot_count >= 1U &&
            henka_terrain_world_get_region_state(
                client_world_a, (henka_terrain_region_id){0, 0}, &state_a) == HENKA_SUCCESS &&
            henka_terrain_world_get_region_state(
                client_world_b, (henka_terrain_region_id){0, 0}, &state_b) == HENKA_SUCCESS &&
            state_a.revision == 7U && state_b.revision == 7U)
        {
            break;
        }
    }
    if (iteration == 3000U)
    {
        goto cleanup;
    }

    request.world_identity = world_desc.world_identity;
    request.base_asset_identity = world_desc.base_asset_identity;
    request.client_nonce = 1201U;
    request.command = henka_terrain_edit_command_default();
    request.command.center_sample_x = 100;
    request.command.center_sample_z = 100;
    request.command.radius_samples = 4U;
    request.affected_region_count = 1U;
    request.affected_regions[0] =
        (henka_terrain_network_region_revision){{0, 0}, 7U};
    if (henka_terrain_client_send_edit_request(terrain_client_a, &request) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    failure_stage = "two-client first delta";
    for (iteration = 0U; iteration < 3000U; ++iteration)
    {
        if (henka_terrain_server_poll(terrain_server, 2U, 15000U + iteration) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client_a, 2U, 8U, &event_count) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client_b, 2U, 8U, &event_count) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client_a, &diagnostics_a);
        henka_terrain_client_get_diagnostics(terrain_client_b, &diagnostics_b);
        if (diagnostics_a.applied_delta_count >= 1U &&
            diagnostics_b.applied_delta_count >= 1U &&
            henka_terrain_world_get_region_state(
                client_world_a, (henka_terrain_region_id){0, 0}, &state_a) == HENKA_SUCCESS &&
            henka_terrain_world_get_region_state(
                client_world_b, (henka_terrain_region_id){0, 0}, &state_b) == HENKA_SUCCESS &&
            state_a.revision == 8U && state_b.revision == 8U)
        {
            break;
        }
    }
    if (iteration == 3000U ||
        !terrain_region_samples_equal(server_world, client_world_a, (henka_terrain_region_id){0, 0}) ||
        !terrain_region_samples_equal(server_world, client_world_b, (henka_terrain_region_id){0, 0}))
    {
        goto cleanup;
    }

    request.client_nonce = 1202U;
    request.command.center_sample_x = 112;
    request.command.center_sample_z = 112;
    request.affected_regions[0].revision = 8U;
    if (henka_terrain_client_send_edit_request(terrain_client_b, &request) != HENKA_SUCCESS)
    {
        goto cleanup;
    }
    failure_stage = "two-client second delta";
    for (iteration = 0U; iteration < 3000U; ++iteration)
    {
        if (henka_terrain_server_poll(terrain_server, 2U, 19000U + iteration) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client_a, 2U, 8U, &event_count) != HENKA_SUCCESS ||
            henka_terrain_client_poll(terrain_client_b, 2U, 8U, &event_count) != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        henka_terrain_client_get_diagnostics(terrain_client_a, &diagnostics_a);
        henka_terrain_client_get_diagnostics(terrain_client_b, &diagnostics_b);
        if (diagnostics_a.applied_delta_count >= 2U &&
            diagnostics_b.applied_delta_count >= 2U &&
            henka_terrain_world_get_region_state(
                client_world_a, (henka_terrain_region_id){0, 0}, &state_a) == HENKA_SUCCESS &&
            henka_terrain_world_get_region_state(
                client_world_b, (henka_terrain_region_id){0, 0}, &state_b) == HENKA_SUCCESS &&
            state_a.revision == 9U && state_b.revision == 9U)
        {
            break;
        }
    }
    if (iteration == 3000U ||
        !terrain_region_samples_equal(server_world, client_world_a, (henka_terrain_region_id){0, 0}) ||
        !terrain_region_samples_equal(server_world, client_world_b, (henka_terrain_region_id){0, 0}))
    {
        goto cleanup;
    }
    result = 1;

cleanup:
    if (!result)
    {
        henka_terrain_client_get_diagnostics(terrain_client_a, &diagnostics_a);
        henka_terrain_client_get_diagnostics(terrain_client_b, &diagnostics_b);
        fprintf(stderr, "two-client terrain test failed during %s at iteration %u (A connected=%llu snapshots=%llu deltas=%llu; B connected=%llu snapshots=%llu deltas=%llu)\n",
            failure_stage,
            (unsigned int)iteration,
            (unsigned long long)diagnostics_a.connected_event_count,
            (unsigned long long)diagnostics_a.completed_snapshot_count,
            (unsigned long long)diagnostics_a.applied_delta_count,
            (unsigned long long)diagnostics_b.connected_event_count,
            (unsigned long long)diagnostics_b.completed_snapshot_count,
            (unsigned long long)diagnostics_b.applied_delta_count);
    }
    henka_terrain_client_destroy(terrain_client_b);
    henka_terrain_client_destroy(terrain_client_a);
    henka_terrain_server_destroy(terrain_server);
    henka_network_client_destroy(network_client_b);
    henka_network_client_destroy(network_client_a);
    henka_network_server_destroy(network_server);
    henka_terrain_storage_destroy(storage);
    henka_terrain_world_destroy(client_world_b);
    henka_terrain_world_destroy(client_world_a);
    henka_terrain_world_destroy(server_world);
    henka_free(samples);
    return result;
}

int main(void)
{
    return test_client_snapshot_and_delta_path() &&
        test_two_client_authoritative_convergence() ? 0 : 1;
}
