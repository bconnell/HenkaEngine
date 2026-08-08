#include <stdint.h>
#include <string.h>

#include <henka/network.h>

static int test_codec_round_trip(void)
{
    const uint8_t payload[] = {1U, 2U, 3U, 4U};
    uint8_t packet[HENKA_NETWORK_MAX_PACKET_BYTES];
    henka_network_message_view view;
    size_t packet_size = 0U;

    if (henka_network_message_encode(
            HENKA_NETWORK_CHANNEL_CONTROL,
            HENKA_NETWORK_MESSAGE_PING,
            payload,
            sizeof(payload),
            packet,
            sizeof(packet),
            &packet_size) != HENKA_SUCCESS ||
        packet_size != HENKA_NETWORK_PROTOCOL_HEADER_BYTES + sizeof(payload) ||
        henka_network_message_decode(packet, packet_size, &view) != HENKA_SUCCESS ||
        view.channel != HENKA_NETWORK_CHANNEL_CONTROL ||
        view.type != HENKA_NETWORK_MESSAGE_PING ||
        view.payload_size != sizeof(payload) ||
        memcmp(view.payload, payload, sizeof(payload)) != 0)
    {
        return 0;
    }
    return 1;
}

static int test_codec_rejects_malformed_input(void)
{
    uint8_t packet[HENKA_NETWORK_MAX_PACKET_BYTES];
    uint8_t oversized[HENKA_NETWORK_MAX_PACKET_BYTES];
    henka_network_message_view view;
    size_t packet_size = 0U;

    if (!test_codec_round_trip())
    {
        return 0;
    }
    if (henka_network_message_encode(
            HENKA_NETWORK_CHANNEL_CONTROL,
            HENKA_NETWORK_MESSAGE_PING,
            NULL,
            0U,
            packet,
            sizeof(packet),
            &packet_size) != HENKA_SUCCESS)
    {
        return 0;
    }
    packet[0] ^= 0x01U;
    if (henka_network_message_decode(packet, packet_size, &view) == HENKA_SUCCESS)
    {
        return 0;
    }
    if (henka_network_message_decode(packet, HENKA_NETWORK_PROTOCOL_HEADER_BYTES - 1U, &view) == HENKA_SUCCESS)
    {
        return 0;
    }
    memset(oversized, 0, sizeof(oversized));
    if (henka_network_message_encode(
            HENKA_NETWORK_CHANNEL_CONTROL,
            HENKA_NETWORK_MESSAGE_PING,
            oversized,
            HENKA_NETWORK_MAX_PACKET_BYTES,
            packet,
            sizeof(packet),
            &packet_size) == HENKA_SUCCESS)
    {
        return 0;
    }
    if (henka_network_message_encode(
            HENKA_NETWORK_CHANNEL_SNAPSHOT,
            HENKA_NETWORK_MESSAGE_SNAPSHOT_FRAGMENT,
            oversized,
            HENKA_NETWORK_MAX_SNAPSHOT_FRAGMENT_PAYLOAD + 1U,
            packet,
            sizeof(packet),
            &packet_size) == HENKA_SUCCESS)
    {
        return 0;
    }
    return 1;
}

int main(void)
{
    henka_network_server_desc server_desc;
    henka_network_client_desc client_desc;
    henka_network_server* server = NULL;
    henka_network_client* client = NULL;
    henka_network_event server_event;
    henka_network_event client_event;
    const uint8_t first_payload[] = {5U};
    const uint8_t second_payload[] = {6U};
    int client_connected = 0;
    int server_connected = 0;
    henka_network_peer_id server_peer_id = HENKA_NETWORK_INVALID_PEER_ID;
    int first_received = 0;
    int second_received = 0;
    int acknowledgement_received = 0;
    int disconnected = 0;
    int index;

    if (!test_codec_rejects_malformed_input())
    {
        return 1;
    }
    server_desc = henka_network_server_desc_default();
    server_desc.bind_endpoint.host[0] = '1';
    server_desc.bind_endpoint.host[1] = '2';
    server_desc.bind_endpoint.host[2] = '7';
    server_desc.bind_endpoint.host[3] = '.';
    server_desc.bind_endpoint.host[4] = '0';
    server_desc.bind_endpoint.host[5] = '.';
    server_desc.bind_endpoint.host[6] = '0';
    server_desc.bind_endpoint.host[7] = '.';
    server_desc.bind_endpoint.host[8] = '1';
    server_desc.bind_endpoint.host[9] = '\0';
    server_desc.bind_endpoint.port = 37991U;
    server_desc.max_clients = 2U;
    client_desc = henka_network_client_desc_default();
    memcpy(client_desc.remote_endpoint.host, server_desc.bind_endpoint.host, 10U);
    client_desc.remote_endpoint.port = server_desc.bind_endpoint.port;
    if (henka_network_server_create(&server_desc, &server) != HENKA_SUCCESS ||
        henka_network_client_create(&client_desc, &client) != HENKA_SUCCESS)
    {
        henka_network_client_destroy(client);
        henka_network_server_destroy(server);
        return 2;
    }

    for (index = 0; index < 200 && (!server_connected || !client_connected); ++index)
    {
        if (henka_network_server_poll(server, 5U, &server_event) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            henka_network_server_destroy(server);
            return 3;
        }
        if (server_event.type == HENKA_NETWORK_EVENT_CONNECTED)
        {
            server_connected = 1;
            server_peer_id = server_event.peer_id;
        }
        if (henka_network_client_poll(client, 5U, &client_event) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            henka_network_server_destroy(server);
            return 4;
        }
        if (client_event.type == HENKA_NETWORK_EVENT_CONNECTED)
        {
            client_connected = 1;
        }
    }
    if (!server_connected || !client_connected ||
        henka_network_client_send(
            client,
            HENKA_NETWORK_CHANNEL_CONTROL,
            HENKA_NETWORK_MESSAGE_PING,
            first_payload,
            sizeof(first_payload)) != HENKA_SUCCESS ||
        henka_network_client_send(
            client,
            HENKA_NETWORK_CHANNEL_CONTROL,
            HENKA_NETWORK_MESSAGE_PING,
            second_payload,
            sizeof(second_payload)) != HENKA_SUCCESS)
    {
        henka_network_client_destroy(client);
        henka_network_server_destroy(server);
        return 5;
    }
    {
        const uint8_t broadcast_payload[] = {7U};
        if (henka_network_server_broadcast(
                server, HENKA_NETWORK_CHANNEL_CONTROL, HENKA_NETWORK_MESSAGE_PING,
                broadcast_payload, sizeof(broadcast_payload)) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            henka_network_server_destroy(server);
            return 11;
        }
    }
    for (index = 0; index < 200; ++index)
    {
        if (henka_network_client_poll(client, 5U, &client_event) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            henka_network_server_destroy(server);
            return 12;
        }
        if (client_event.type == HENKA_NETWORK_EVENT_MESSAGE &&
            client_event.message.payload_size == 1U && client_event.message.payload[0] == 7U)
        {
            break;
        }
    }
    if (index == 200)
    {
        henka_network_client_destroy(client);
        henka_network_server_destroy(server);
        return 13;
    }
    for (index = 0; index < 200 && second_received == 0; ++index)
    {
        if (henka_network_server_poll(server, 5U, &server_event) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            henka_network_server_destroy(server);
            return 6;
        }
        if (server_event.type == HENKA_NETWORK_EVENT_MESSAGE)
        {
            if (server_event.message.payload_size != 1U ||
                server_event.message.payload[0] != (uint8_t)(first_received ? 6U : 5U))
            {
                henka_network_client_destroy(client);
                henka_network_server_destroy(server);
                return 7;
            }
            if (first_received)
            {
                second_received = 1;
            }
            else
            {
                first_received = 1;
            }
        }
        (void)henka_network_client_poll(client, 0U, &client_event);
    }
    if (!second_received)
    {
        henka_network_client_destroy(client);
        henka_network_server_destroy(server);
        return 8;
    }
    {
        const uint8_t acknowledgement_payload[] = {9U};
        if (henka_network_server_send(
                server,
                server_peer_id,
                HENKA_NETWORK_CHANNEL_CONTROL,
                HENKA_NETWORK_MESSAGE_PING,
                acknowledgement_payload,
                sizeof(acknowledgement_payload)) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            henka_network_server_destroy(server);
            return 9;
        }
    }
    for (index = 0; index < 200 && !acknowledgement_received; ++index)
    {
        if (henka_network_client_poll(client, 5U, &client_event) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            henka_network_server_destroy(server);
            return 10;
        }
        if (client_event.type == HENKA_NETWORK_EVENT_MESSAGE &&
            client_event.message.payload_size == 1U &&
            client_event.message.payload[0] == 9U)
        {
            acknowledgement_received = 1;
        }
        (void)henka_network_server_poll(server, 0U, &server_event);
    }
    if (!acknowledgement_received ||
        henka_network_server_disconnect(
            server,
            server_peer_id,
            HENKA_NETWORK_DISCONNECT_REASON_SERVER_SHUTDOWN) != HENKA_SUCCESS)
    {
        henka_network_client_destroy(client);
        henka_network_server_destroy(server);
        return 11;
    }
    for (index = 0; index < 200 && !disconnected; ++index)
    {
        if (henka_network_client_poll(client, 5U, &client_event) != HENKA_SUCCESS)
        {
            henka_network_client_destroy(client);
            henka_network_server_destroy(server);
            return 12;
        }
        if (client_event.type == HENKA_NETWORK_EVENT_DISCONNECTED)
        {
            disconnected = client_event.disconnect_reason ==
                HENKA_NETWORK_DISCONNECT_REASON_SERVER_SHUTDOWN;
        }
        (void)henka_network_server_poll(server, 0U, &server_event);
    }
    if (!disconnected)
    {
        henka_network_client_destroy(client);
        henka_network_server_destroy(server);
        return 13;
    }
    henka_network_client_destroy(client);
    henka_network_server_destroy(server);
    return 0;
}
