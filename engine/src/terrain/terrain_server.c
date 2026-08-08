#include <henka/terrain_server.h>

#include <string.h>

#include <henka/memory.h>

struct henka_terrain_server
{
    henka_network_server* network;
    henka_terrain_authority* authority;
    henka_terrain_server_diagnostics diagnostics;
};

henka_terrain_server_desc henka_terrain_server_desc_default(void)
{
    return (henka_terrain_server_desc){
        NULL,
        NULL,
        NULL,
        32U,
        HENKA_TERRAIN_DEFAULT_EDIT_RATE_PER_SECOND,
        NULL,
        NULL};
}

henka_result henka_terrain_server_create(
    const henka_terrain_server_desc* desc,
    henka_terrain_server** out_server)
{
    henka_terrain_server* server;
    henka_terrain_authority_desc authority_desc;
    if (out_server == NULL || desc == NULL || desc->network == NULL ||
        desc->world == NULL || desc->storage == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_server = NULL;
    server = henka_calloc(1U, sizeof(*server));
    if (server == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    authority_desc = henka_terrain_authority_desc_default();
    authority_desc.world = desc->world;
    authority_desc.storage = desc->storage;
    authority_desc.max_clients = desc->max_clients;
    authority_desc.edit_rate_per_second = desc->edit_rate_per_second;
    authority_desc.permission_callback = desc->permission_callback;
    authority_desc.permission_user_data = desc->permission_user_data;
    {
        henka_result result = henka_terrain_authority_create(&authority_desc, &server->authority);
        if (result != HENKA_SUCCESS)
        {
            henka_free(server);
            return result;
        }
    }
    server->network = desc->network;
    *out_server = server;
    return HENKA_SUCCESS;
}

void henka_terrain_server_destroy(henka_terrain_server* server)
{
    if (server == NULL)
    {
        return;
    }
    henka_terrain_authority_destroy(server->authority);
    henka_free(server);
}

static henka_result henka_terrain_server_send_rejection(
    henka_terrain_server* server,
    henka_network_peer_id peer_id,
    const henka_terrain_edit_rejection* rejection)
{
    uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_DELTA_BYTES];
    size_t payload_size;
    henka_result result = henka_terrain_edit_rejection_encode(
        rejection, payload, sizeof(payload), &payload_size);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    return henka_network_server_send(
        server->network, peer_id, HENKA_NETWORK_CHANNEL_TERRAIN,
        HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REJECTED, payload, payload_size);
}

henka_result henka_terrain_server_handle_event(
    henka_terrain_server* server,
    const henka_network_event* event,
    uint64_t now_milliseconds)
{
    henka_terrain_edit_request request;
    henka_terrain_authority_response response;
    uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_EDIT_RESPONSE_BYTES];
    size_t payload_size;
    henka_result result;

    if (server == NULL || event == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (event->type != HENKA_NETWORK_EVENT_MESSAGE)
    {
        return HENKA_SUCCESS;
    }
    if (event->message.channel == HENKA_NETWORK_CHANNEL_CONTROL &&
        event->message.type == HENKA_NETWORK_MESSAGE_PING)
    {
        return henka_network_server_send(
            server->network, event->peer_id, event->message.channel,
            HENKA_NETWORK_MESSAGE_PING, event->message.payload,
            event->message.payload_size);
    }
    if (event->message.channel != HENKA_NETWORK_CHANNEL_TERRAIN ||
        event->message.type != HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REQUEST)
    {
        return HENKA_SUCCESS;
    }
    ++server->diagnostics.processed_edit_request_count;
    result = henka_terrain_edit_request_decode(
        event->message.payload, event->message.payload_size, &request);
    if (result != HENKA_SUCCESS)
    {
        ++server->diagnostics.malformed_edit_count;
        ++server->diagnostics.protocol_disconnect_count;
        (void)henka_network_server_disconnect(
            server->network, event->peer_id, HENKA_NETWORK_DISCONNECT_REASON_PROTOCOL);
        return HENKA_SUCCESS;
    }
    result = henka_terrain_authority_process_request(
        server->authority, event->peer_id, &request, now_milliseconds, &response);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (!response.accepted)
    {
        ++server->diagnostics.rejected_edit_count;
        return henka_terrain_server_send_rejection(server, event->peer_id, &response.rejection);
    }
    ++server->diagnostics.accepted_edit_count;
    result = henka_terrain_edit_acceptance_encode(
        &response.acceptance, payload, sizeof(payload), &payload_size);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_network_server_send(
        server->network, event->peer_id, HENKA_NETWORK_CHANNEL_TERRAIN,
        HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_ACCEPTED, payload, payload_size);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    {
        henka_terrain_edit_delta delta = {0};
        delta.world_identity = request.world_identity;
        delta.base_asset_identity = request.base_asset_identity;
        delta.client_nonce = request.client_nonce;
        delta.server_command_id = response.acceptance.server_command_id;
        delta.command = request.command;
        delta.affected_region_count = response.acceptance.affected_region_count;
        memcpy(
            delta.affected_regions, response.acceptance.affected_regions,
            (size_t)delta.affected_region_count * sizeof(delta.affected_regions[0]));
        result = henka_terrain_edit_delta_encode(
            &delta, payload, sizeof(payload), &payload_size);
        if (result == HENKA_SUCCESS)
        {
            result = henka_network_server_broadcast(
                server->network, HENKA_NETWORK_CHANNEL_TERRAIN,
                HENKA_NETWORK_MESSAGE_TERRAIN_DELTA, payload, payload_size);
        }
    }
    return result;
}

henka_result henka_terrain_server_poll(
    henka_terrain_server* server,
    uint32_t timeout_milliseconds,
    uint64_t now_milliseconds)
{
    henka_network_event event;
    henka_result result;
    if (server == NULL || timeout_milliseconds > HENKA_NETWORK_MAX_POLL_TIMEOUT_MILLISECONDS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_network_server_poll(server->network, timeout_milliseconds, &event);
    if (result != HENKA_SUCCESS || event.type != HENKA_NETWORK_EVENT_MESSAGE)
    {
        return result;
    }
    return henka_terrain_server_handle_event(server, &event, now_milliseconds);
}

void henka_terrain_server_get_diagnostics(
    const henka_terrain_server* server,
    henka_terrain_server_diagnostics* out_diagnostics)
{
    if (out_diagnostics == NULL)
    {
        return;
    }
    *out_diagnostics = server == NULL
        ? (henka_terrain_server_diagnostics){0}
        : server->diagnostics;
}
