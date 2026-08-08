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
    return test_codec_rejects_malformed_input() ? 0 : 1;
}
