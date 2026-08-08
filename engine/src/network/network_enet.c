#include <henka/network.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <enet/enet.h>

#include <henka/memory.h>

#include "network_internal.h"

typedef struct henka_network_peer_record
{
    ENetPeer* peer;
    henka_network_peer_id id;
    henka_network_disconnect_reason disconnect_reason;
} henka_network_peer_record;

struct henka_network_transport
{
    bool server_mode;
    ENetHost* host;
    henka_network_peer_record peers[256];
    uint32_t peer_capacity;
    henka_network_peer_id next_peer_id;
    uint8_t receive_buffer[HENKA_NETWORK_MAX_PACKET_BYTES];
    henka_network_diagnostics diagnostics;
};

static uint32_t g_henka_enet_users;

static henka_result henka_enet_acquire(void)
{
    if (g_henka_enet_users == 0U && enet_initialize() != 0)
    {
        return HENKA_ERROR_PLATFORM;
    }
    ++g_henka_enet_users;
    return HENKA_SUCCESS;
}

static void henka_enet_release(void)
{
    if (g_henka_enet_users == 0U)
    {
        return;
    }
    --g_henka_enet_users;
    if (g_henka_enet_users == 0U)
    {
        enet_deinitialize();
    }
}

static henka_network_peer_record* henka_network_find_peer(
    henka_network_transport* transport,
    ENetPeer* peer)
{
    uint32_t index;
    for (index = 0U; index < transport->peer_capacity; ++index)
    {
        if (transport->peers[index].peer == peer)
        {
            return &transport->peers[index];
        }
    }
    return NULL;
}

static henka_network_peer_record* henka_network_find_peer_id(
    henka_network_transport* transport,
    henka_network_peer_id id)
{
    uint32_t index;
    for (index = 0U; index < transport->peer_capacity; ++index)
    {
        if (transport->peers[index].peer != NULL && transport->peers[index].id == id)
        {
            return &transport->peers[index];
        }
    }
    return NULL;
}

static henka_network_peer_record* henka_network_find_free_peer(
    henka_network_transport* transport)
{
    uint32_t index;
    for (index = 0U; index < transport->peer_capacity; ++index)
    {
        if (transport->peers[index].peer == NULL)
        {
            return &transport->peers[index];
        }
    }
    return NULL;
}

static henka_network_peer_id henka_network_next_peer_id(henka_network_transport* transport)
{
    henka_network_peer_id id = transport->next_peer_id++;
    if (id == HENKA_NETWORK_INVALID_PEER_ID)
    {
        id = transport->next_peer_id++;
    }
    if (transport->next_peer_id == HENKA_NETWORK_INVALID_PEER_ID)
    {
        transport->next_peer_id = 1U;
    }
    return id;
}

static int henka_network_set_address_host(ENetAddress* address, const char* host)
{
    if (strcmp(host, "0.0.0.0") == 0)
    {
        address->host = ENET_HOST_ANY;
        return 0;
    }
    return enet_address_set_host(address, host);
}

henka_result henka_network_transport_create(
    bool server_mode,
    const henka_network_endpoint* endpoint,
    uint32_t max_peers,
    henka_network_transport** out_transport)
{
    ENetAddress address;
    henka_network_transport* transport;

    if (out_transport == NULL || endpoint == NULL || endpoint->host[0] == '\0' ||
        memchr(endpoint->host, '\0', sizeof(endpoint->host)) == NULL ||
        endpoint->port == 0U || max_peers == 0U || max_peers > 256U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_transport = NULL;
    if (henka_enet_acquire() != HENKA_SUCCESS)
    {
        return HENKA_ERROR_PLATFORM;
    }
    transport = henka_calloc(1U, sizeof(*transport));
    if (transport == NULL)
    {
        henka_enet_release();
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    transport->server_mode = server_mode;
    transport->peer_capacity = max_peers;
    transport->next_peer_id = 1U;
    address = (ENetAddress){0};
    address.port = endpoint->port;
    if (server_mode)
    {
        if (henka_network_set_address_host(&address, endpoint->host) != 0)
        {
            henka_free(transport);
            henka_enet_release();
            return HENKA_ERROR_PLATFORM;
        }
        transport->host = enet_host_create(&address, max_peers, 3U, 0U, 0U);
    }
    else
    {
        transport->host = enet_host_create(NULL, 1U, 3U, 0U, 0U);
        if (transport->host != NULL && enet_address_set_host(&address, endpoint->host) == 0)
        {
            if (enet_host_connect(transport->host, &address, 3U, 0U) == NULL)
            {
                enet_host_destroy(transport->host);
                transport->host = NULL;
            }
        }
        else if (transport->host != NULL)
        {
            enet_host_destroy(transport->host);
            transport->host = NULL;
        }
    }
    if (transport->host == NULL)
    {
        henka_free(transport);
        henka_enet_release();
        return HENKA_ERROR_PLATFORM;
    }
    *out_transport = transport;
    return HENKA_SUCCESS;
}

void henka_network_transport_destroy(henka_network_transport* transport)
{
    if (transport == NULL)
    {
        return;
    }
    enet_host_destroy(transport->host);
    henka_free(transport);
    henka_enet_release();
}

henka_result henka_network_transport_poll(
    henka_network_transport* transport,
    uint32_t timeout_milliseconds,
    henka_network_event* out_event)
{
    ENetEvent event;
    henka_network_peer_record* record;
    henka_network_message_view message;

    if (transport == NULL || out_event == NULL ||
        timeout_milliseconds > HENKA_NETWORK_MAX_POLL_TIMEOUT_MILLISECONDS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_event = (henka_network_event){0};
    {
        int service_result = enet_host_service(transport->host, &event, timeout_milliseconds);
        if (service_result < 0)
        {
            return HENKA_ERROR_PLATFORM;
        }
        if (service_result == 0)
        {
            return HENKA_SUCCESS;
        }
    }
    switch (event.type)
    {
        case ENET_EVENT_TYPE_CONNECT:
            record = henka_network_find_free_peer(transport);
            if (record == NULL)
            {
                ++transport->diagnostics.dropped_packet_count;
                enet_peer_disconnect(event.peer, 0U);
                return HENKA_SUCCESS;
            }
            record->peer = event.peer;
            record->id = henka_network_next_peer_id(transport);
            record->disconnect_reason = HENKA_NETWORK_DISCONNECT_REASON_NONE;
            event.peer->data = record;
            ++transport->diagnostics.connected_peer_count;
            out_event->type = HENKA_NETWORK_EVENT_CONNECTED;
            out_event->peer_id = record->id;
            return HENKA_SUCCESS;

        case ENET_EVENT_TYPE_DISCONNECT:
            if (event.peer == NULL)
            {
                ++transport->diagnostics.dropped_packet_count;
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            record = (henka_network_peer_record*)event.peer->data;
            if (record == NULL)
            {
                record = henka_network_find_peer(transport, event.peer);
            }
            if (record != NULL && record->peer != NULL)
            {
                henka_network_disconnect_reason received_reason =
                    event.data > 0U && event.data <= HENKA_NETWORK_DISCONNECT_REASON_APPLICATION
                    ? (henka_network_disconnect_reason)event.data
                    : HENKA_NETWORK_DISCONNECT_REASON_NONE;
                out_event->type = HENKA_NETWORK_EVENT_DISCONNECTED;
                out_event->peer_id = record->id;
                out_event->disconnect_reason = record->disconnect_reason == HENKA_NETWORK_DISCONNECT_REASON_NONE
                    ? (received_reason == HENKA_NETWORK_DISCONNECT_REASON_NONE
                        ? HENKA_NETWORK_DISCONNECT_REASON_APPLICATION
                        : received_reason)
                    : record->disconnect_reason;
                record->peer = NULL;
                record->id = HENKA_NETWORK_INVALID_PEER_ID;
                record->disconnect_reason = HENKA_NETWORK_DISCONNECT_REASON_NONE;
                if (transport->diagnostics.connected_peer_count > 0U)
                {
                    --transport->diagnostics.connected_peer_count;
                }
            }
            return HENKA_SUCCESS;

        case ENET_EVENT_TYPE_RECEIVE:
            if (event.packet == NULL || event.packet->dataLength > sizeof(transport->receive_buffer))
            {
                ++transport->diagnostics.dropped_packet_count;
                enet_packet_destroy(event.packet);
                return HENKA_ERROR_LIMIT;
            }
            if (event.peer == NULL)
            {
                ++transport->diagnostics.dropped_packet_count;
                enet_packet_destroy(event.packet);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            record = (henka_network_peer_record*)event.peer->data;
            if (record == NULL ||
                event.packet->dataLength > sizeof(transport->receive_buffer))
            {
                ++transport->diagnostics.dropped_packet_count;
                enet_packet_destroy(event.packet);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            memcpy(transport->receive_buffer, event.packet->data, event.packet->dataLength);
            if (henka_network_message_decode(
                    transport->receive_buffer,
                    event.packet->dataLength,
                    &message) != HENKA_SUCCESS)
            {
                ++transport->diagnostics.malformed_packet_count;
                enet_packet_destroy(event.packet);
                return HENKA_ERROR_INVALID_ARGUMENT;
            }
            enet_packet_destroy(event.packet);
            ++transport->diagnostics.received_message_count;
            out_event->type = HENKA_NETWORK_EVENT_MESSAGE;
            out_event->peer_id = record->id;
            out_event->message = message;
            return HENKA_SUCCESS;

        default:
            return HENKA_SUCCESS;
    }
}

henka_result henka_network_transport_send(
    henka_network_transport* transport,
    henka_network_peer_id peer_id,
    henka_network_channel channel,
    henka_network_message_type type,
    const void* payload,
    size_t payload_size)
{
    uint8_t buffer[HENKA_NETWORK_MAX_PACKET_BYTES];
    size_t packet_size;
    henka_network_peer_record* record;
    henka_result encode_result;
    ENetPacket* packet;

    if (transport == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (transport->server_mode)
    {
        record = henka_network_find_peer_id(transport, peer_id);
    }
    else
    {
        record = NULL;
        {
            uint32_t index;
            for (index = 0U; index < transport->peer_capacity; ++index)
            {
                if (transport->peers[index].peer != NULL)
                {
                    record = &transport->peers[index];
                    break;
                }
            }
        }
    }
    encode_result = henka_network_message_encode(
            channel,
            type,
            payload,
            payload_size,
            buffer,
            sizeof(buffer),
            &packet_size);
    if (record == NULL || encode_result != HENKA_SUCCESS)
    {
        ++transport->diagnostics.rejected_send_count;
        return encode_result != HENKA_SUCCESS ? encode_result : HENKA_ERROR_INVALID_ARGUMENT;
    }
    packet = enet_packet_create(buffer, packet_size, ENET_PACKET_FLAG_RELIABLE);
    if (packet == NULL)
    {
        ++transport->diagnostics.rejected_send_count;
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (enet_peer_send(record->peer, (enet_uint8)channel, packet) != 0)
    {
        enet_packet_destroy(packet);
        ++transport->diagnostics.rejected_send_count;
        return HENKA_ERROR_PLATFORM;
    }
    enet_host_flush(transport->host);
    ++transport->diagnostics.sent_message_count;
    return HENKA_SUCCESS;
}

henka_result henka_network_transport_disconnect(
    henka_network_transport* transport,
    henka_network_peer_id peer_id,
    henka_network_disconnect_reason reason)
{
    henka_network_peer_record* record;
    if (transport == NULL || peer_id == HENKA_NETWORK_INVALID_PEER_ID ||
        reason <= HENKA_NETWORK_DISCONNECT_REASON_NONE ||
        reason > HENKA_NETWORK_DISCONNECT_REASON_APPLICATION)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record = henka_network_find_peer_id(transport, peer_id);
    if (record == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    record->disconnect_reason = reason;
    enet_peer_disconnect(record->peer, (enet_uint32)reason);
    enet_host_flush(transport->host);
    return HENKA_SUCCESS;
}

void henka_network_transport_get_diagnostics(
    const henka_network_transport* transport,
    henka_network_diagnostics* out_diagnostics)
{
    if (out_diagnostics == NULL)
    {
        return;
    }
    *out_diagnostics = transport == NULL
        ? (henka_network_diagnostics){0}
        : transport->diagnostics;
}
