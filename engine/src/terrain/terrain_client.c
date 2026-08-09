#include <henka/terrain_client.h>

#include <string.h>

#include <henka/memory.h>

#define HENKA_TERRAIN_CLIENT_MAX_SNAPSHOT_RETRIES 4U

struct henka_terrain_client
{
    henka_network_client* network;
    henka_terrain_world* world;
    henka_terrain_replica* replica;
    henka_terrain_prediction* prediction;
    henka_terrain_client_diagnostics diagnostics;
    uint64_t last_snapshot_retry_transfer_id;
    uint32_t snapshot_retry_count;
};

henka_terrain_client_desc henka_terrain_client_desc_default(void)
{
    return (henka_terrain_client_desc){
        NULL,
        NULL,
        HENKA_TERRAIN_MAX_REGION_RECORD_BYTES,
        16U};
}

henka_result henka_terrain_client_create(
    const henka_terrain_client_desc* desc,
    henka_terrain_client** out_client)
{
    henka_terrain_client* client;
    henka_terrain_replica_desc replica_desc;
    if (out_client == NULL || desc == NULL || desc->network == NULL || desc->world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_client = NULL;
    client = henka_calloc(1U, sizeof(*client));
    if (client == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    replica_desc = henka_terrain_replica_desc_default();
    replica_desc.world = desc->world;
    if (desc->max_snapshot_bytes != 0U)
    {
        replica_desc.max_snapshot_bytes = desc->max_snapshot_bytes;
    }
    if (henka_terrain_replica_create(&replica_desc, &client->replica) != HENKA_SUCCESS)
    {
        henka_free(client);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    {
        henka_terrain_prediction_desc prediction_desc = henka_terrain_prediction_desc_default();
        prediction_desc.authoritative_world = desc->world;
        if (desc->max_pending_prediction_commands != 0U)
        {
            prediction_desc.max_pending_commands = desc->max_pending_prediction_commands;
        }
        if (henka_terrain_prediction_create(&prediction_desc, &client->prediction) != HENKA_SUCCESS)
        {
            henka_terrain_replica_destroy(client->replica);
            henka_free(client);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }
    client->network = desc->network;
    client->world = desc->world;
    *out_client = client;
    return HENKA_SUCCESS;
}

void henka_terrain_client_destroy(henka_terrain_client* client)
{
    if (client == NULL)
    {
        return;
    }
    henka_terrain_replica_destroy(client->replica);
    henka_terrain_prediction_destroy(client->prediction);
    henka_free(client);
}

henka_result henka_terrain_client_send_edit_request(
    henka_terrain_client* client,
    const henka_terrain_edit_request* request)
{
    uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_EDIT_REQUEST_BYTES];
    size_t payload_size;
    henka_terrain_edit_command prediction_command;
    if (client == NULL || request == NULL ||
        henka_terrain_edit_request_encode(request, payload, sizeof(payload), &payload_size) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    prediction_command = request->command;
    prediction_command.client_nonce = request->client_nonce;
    if (henka_terrain_prediction_submit(client->prediction, &prediction_command) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_LIMIT;
    }
    {
        henka_result send_result = henka_network_client_send(
            client->network, HENKA_NETWORK_CHANNEL_TERRAIN,
            HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REQUEST, payload, payload_size);
        if (send_result != HENKA_SUCCESS)
        {
            (void)henka_terrain_prediction_reject(
                client->prediction, prediction_command.client_nonce);
        }
        return send_result;
    }
}

henka_result henka_terrain_client_reconnect(henka_terrain_client* client)
{
    return client == NULL
        ? HENKA_ERROR_INVALID_ARGUMENT
        : henka_network_client_reconnect(client->network);
}

henka_result henka_terrain_client_request_snapshot(
    henka_terrain_client* client,
    henka_terrain_snapshot_request request)
{
    uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_REQUEST_BYTES];
    size_t payload_size;
    if (client == NULL ||
        henka_terrain_snapshot_request_encode(
            &request, payload, sizeof(payload), &payload_size) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_network_client_send(
        client->network, HENKA_NETWORK_CHANNEL_SNAPSHOT,
        HENKA_NETWORK_MESSAGE_SNAPSHOT_REQUEST, payload, payload_size);
}

static henka_result henka_terrain_client_retry_snapshot(
    henka_terrain_client* client,
    const henka_terrain_snapshot_fragment* fragment,
    bool* out_requested)
{
    henka_terrain_world_desc desc;
    henka_terrain_snapshot_request request;

    if (out_requested == NULL || client == NULL || fragment == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_requested = false;
    if (client->snapshot_retry_count >= HENKA_TERRAIN_CLIENT_MAX_SNAPSHOT_RETRIES ||
        fragment->transfer_id == 0U ||
        fragment->transfer_id == client->last_snapshot_retry_transfer_id ||
        fragment->revision == 0U ||
        henka_terrain_world_get_desc(client->world, &desc) != HENKA_SUCCESS ||
        desc.world_identity != fragment->world_identity ||
        desc.base_asset_identity != fragment->base_asset_identity)
    {
        return HENKA_SUCCESS;
    }
    request = (henka_terrain_snapshot_request){
        desc.world_identity,
        desc.base_asset_identity,
        fragment->region_id,
        fragment->revision};
    if (henka_terrain_client_request_snapshot(client, request) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_PLATFORM;
    }
    client->last_snapshot_retry_transfer_id = fragment->transfer_id;
    ++client->snapshot_retry_count;
    ++client->diagnostics.recovery_snapshot_request_count;
    *out_requested = true;
    return HENKA_SUCCESS;
}

static henka_result henka_terrain_client_request_delta_recovery(
    henka_terrain_client* client,
    const henka_terrain_edit_delta* delta)
{
    uint32_t index;
    for (index = 0U; index < delta->affected_region_count; ++index)
    {
        henka_terrain_region_state state;
        henka_terrain_delta_recovery_request request = {
            delta->world_identity,
            delta->base_asset_identity,
            delta->affected_regions[index].region_id,
            1U,
            delta->affected_regions[index].revision};
        if (henka_terrain_world_get_region_state(
                client->world, request.region_id, &state) == HENKA_SUCCESS)
        {
            if (state.revision >= request.target_revision)
            {
                continue;
            }
            if (state.revision == UINT64_MAX)
            {
                return HENKA_ERROR_LIMIT;
            }
            request.from_revision = state.revision + 1U;
        }
        {
            uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_RECOVERY_REQUEST_BYTES];
            size_t payload_size;
            if (henka_terrain_delta_recovery_request_encode(
                    &request, payload, sizeof(payload), &payload_size) != HENKA_SUCCESS ||
                henka_network_client_send(
                    client->network, HENKA_NETWORK_CHANNEL_TERRAIN,
                    HENKA_NETWORK_MESSAGE_TERRAIN_RECOVERY_REQUEST,
                    payload, payload_size) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_PLATFORM;
            }
        }
        ++client->diagnostics.recovery_delta_request_count;
    }
    return HENKA_SUCCESS;
}

static henka_result henka_terrain_client_handle_session_info(
    henka_terrain_client* client,
    const henka_network_event* event)
{
    henka_terrain_world_desc desc;
    henka_terrain_session_info info;
    uint32_t index;

    if (henka_terrain_session_info_decode(
            event->message.payload, event->message.payload_size, &info) != HENKA_SUCCESS ||
        henka_terrain_world_get_desc(client->world, &desc) != HENKA_SUCCESS ||
        desc.world_identity != info.world_identity ||
        desc.base_asset_identity != info.base_asset_identity)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < info.region_count; ++index)
    {
        henka_terrain_region_state state;
        henka_terrain_snapshot_request request;
        if (henka_terrain_world_get_region_state(
                client->world, info.regions[index].region_id, &state) == HENKA_SUCCESS &&
            state.revision == info.regions[index].revision &&
            state.generation == info.regions[index].generation)
        {
            continue;
        }
        request = (henka_terrain_snapshot_request){
            desc.world_identity,
            desc.base_asset_identity,
            info.regions[index].region_id,
            info.regions[index].revision};
        if (henka_terrain_client_request_snapshot(client, request) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_PLATFORM;
        }
        ++client->diagnostics.session_snapshot_request_count;
    }
    return HENKA_SUCCESS;
}

static void henka_terrain_client_sync_replica_diagnostics(
    henka_terrain_client* client)
{
    henka_terrain_replica_diagnostics replica_diagnostics;
    henka_terrain_replica_get_diagnostics(client->replica, &replica_diagnostics);
    client->diagnostics.applied_delta_count = replica_diagnostics.applied_delta_count;
    client->diagnostics.duplicate_delta_count = replica_diagnostics.duplicate_delta_count;
    client->diagnostics.rejected_delta_count = replica_diagnostics.rejected_delta_count;
    client->diagnostics.completed_snapshot_count = replica_diagnostics.completed_snapshot_count;
    client->diagnostics.rejected_snapshot_count = replica_diagnostics.rejected_snapshot_count;
    {
        henka_terrain_prediction_stats prediction_stats;
        henka_terrain_prediction_get_stats(client->prediction, &prediction_stats);
        client->diagnostics.pending_prediction_count = prediction_stats.pending_command_count;
        client->diagnostics.prediction_replay_failure_count = prediction_stats.replay_failure_count;
        client->diagnostics.prediction_enabled = prediction_stats.prediction_enabled;
    }
}

henka_result henka_terrain_client_handle_event(
    henka_terrain_client* client,
    const henka_network_event* event)
{
    henka_result result;
    if (client == NULL || event == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (event->type == HENKA_NETWORK_EVENT_CONNECTED)
    {
        ++client->diagnostics.connected_event_count;
        client->last_snapshot_retry_transfer_id = 0U;
        client->snapshot_retry_count = 0U;
        return HENKA_SUCCESS;
    }
    if (event->type == HENKA_NETWORK_EVENT_DISCONNECTED)
    {
        ++client->diagnostics.disconnected_event_count;
        return HENKA_SUCCESS;
    }
    if (event->type != HENKA_NETWORK_EVENT_MESSAGE)
    {
        return HENKA_SUCCESS;
    }
    ++client->diagnostics.processed_message_count;
    if (event->message.channel == HENKA_NETWORK_CHANNEL_CONTROL &&
        event->message.type == HENKA_NETWORK_MESSAGE_CONNECT)
    {
        return henka_terrain_client_handle_session_info(client, event);
    }
    if (event->message.type == HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_ACCEPTED)
    {
        henka_terrain_edit_acceptance acceptance;
        result = henka_terrain_edit_acceptance_decode(
            event->message.payload, event->message.payload_size, &acceptance);
        if (result != HENKA_SUCCESS)
        {
            ++client->diagnostics.malformed_message_count;
            return result;
        }
        client->diagnostics.last_acceptance = acceptance;
        client->diagnostics.last_acceptance_valid = true;
        ++client->diagnostics.acceptance_count;
        return HENKA_SUCCESS;
    }
    if (event->message.type == HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REJECTED)
    {
        henka_terrain_edit_rejection rejection;
        result = henka_terrain_edit_rejection_decode(
            event->message.payload, event->message.payload_size, &rejection);
        if (result != HENKA_SUCCESS)
        {
            ++client->diagnostics.malformed_message_count;
            return result;
        }
        client->diagnostics.last_rejection = rejection;
        client->diagnostics.last_rejection_valid = true;
        ++client->diagnostics.rejection_count;
        {
            henka_result prediction_result = henka_terrain_prediction_reject(
                client->prediction, rejection.client_nonce);
            if (prediction_result != HENKA_SUCCESS &&
                prediction_result != HENKA_ERROR_INVALID_ARGUMENT)
            {
                return prediction_result;
            }
        }
        henka_terrain_client_sync_replica_diagnostics(client);
        return HENKA_SUCCESS;
    }
    if (event->message.type == HENKA_NETWORK_MESSAGE_TERRAIN_DELTA)
    {
        henka_terrain_edit_delta delta;
        bool applied = false;
        result = henka_terrain_edit_delta_decode(
            event->message.payload, event->message.payload_size, &delta);
        if (result != HENKA_SUCCESS)
        {
            ++client->diagnostics.malformed_message_count;
            return result;
        }
        result = henka_terrain_replica_apply_delta(client->replica, &delta, &applied);
        if (result != HENKA_SUCCESS)
        {
            if (henka_terrain_client_request_delta_recovery(client, &delta) != HENKA_SUCCESS)
            {
                return result;
            }
            return HENKA_SUCCESS;
        }
        if (delta.client_nonce != 0U)
        {
            henka_result prediction_result = henka_terrain_prediction_accept(
                client->prediction, delta.client_nonce);
            if (prediction_result != HENKA_SUCCESS &&
                prediction_result != HENKA_ERROR_INVALID_ARGUMENT)
            {
                return prediction_result;
            }
        }
        if (henka_terrain_prediction_refresh(client->prediction) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_ASSET_SOURCE;
        }
        (void)applied;
        henka_terrain_client_sync_replica_diagnostics(client);
        return HENKA_SUCCESS;
    }
    if (event->message.type == HENKA_NETWORK_MESSAGE_SNAPSHOT_FRAGMENT)
    {
        henka_terrain_snapshot_fragment fragment;
        bool complete = false;
        bool decoded = false;
        bool retry_requested = false;
        result = henka_terrain_snapshot_fragment_decode(
            event->message.payload, event->message.payload_size, &fragment);
        if (result == HENKA_SUCCESS)
        {
            decoded = true;
            result = henka_terrain_replica_apply_snapshot_fragment(
                client->replica, &fragment, &complete);
        }
        if (result != HENKA_SUCCESS)
        {
            if (decoded &&
                henka_terrain_client_retry_snapshot(
                    client, &fragment, &retry_requested) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_PLATFORM;
            }
            henka_terrain_client_sync_replica_diagnostics(client);
            if (retry_requested)
            {
                return HENKA_SUCCESS;
            }
            ++client->diagnostics.rejected_snapshot_count;
            return result;
        }
        if (complete && henka_terrain_prediction_refresh(client->prediction) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_ASSET_SOURCE;
        }
        if (complete)
        {
            client->last_snapshot_retry_transfer_id = 0U;
            client->snapshot_retry_count = 0U;
        }
        (void)complete;
        henka_terrain_client_sync_replica_diagnostics(client);
        return HENKA_SUCCESS;
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_client_poll(
    henka_terrain_client* client,
    uint32_t timeout_milliseconds,
    uint32_t max_events,
    uint32_t* out_event_count)
{
    uint32_t index;
    uint32_t processed = 0U;
    if (out_event_count == NULL || client == NULL || max_events == 0U ||
        timeout_milliseconds > HENKA_NETWORK_MAX_POLL_TIMEOUT_MILLISECONDS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_event_count = 0U;
    for (index = 0U; index < max_events; ++index)
    {
        henka_network_event event;
        henka_result result = henka_network_client_poll(
            client->network, index == 0U ? timeout_milliseconds : 0U, &event);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (event.type == HENKA_NETWORK_EVENT_NONE)
        {
            break;
        }
        result = henka_terrain_client_handle_event(client, &event);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        ++processed;
    }
    *out_event_count = processed;
    return HENKA_SUCCESS;
}

void henka_terrain_client_get_diagnostics(
    const henka_terrain_client* client,
    henka_terrain_client_diagnostics* out_diagnostics)
{
    if (out_diagnostics == NULL)
    {
        return;
    }
    *out_diagnostics = client == NULL
        ? (henka_terrain_client_diagnostics){0}
        : client->diagnostics;
}

henka_terrain_world* henka_terrain_client_get_predicted_world(
    henka_terrain_client* client)
{
    return client == NULL ? NULL : henka_terrain_prediction_get_world(client->prediction);
}
