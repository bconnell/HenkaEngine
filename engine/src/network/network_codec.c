#include <henka/network.h>

#include <stdbool.h>
#include <string.h>

#include "network_internal.h"

static void henka_network_write_u16_le(uint8_t* destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & UINT16_C(0xFF));
    destination[1] = (uint8_t)((value >> 8U) & UINT16_C(0xFF));
}

static void henka_network_write_u32_le(uint8_t* destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & UINT32_C(0xFF));
    destination[1] = (uint8_t)((value >> 8U) & UINT32_C(0xFF));
    destination[2] = (uint8_t)((value >> 16U) & UINT32_C(0xFF));
    destination[3] = (uint8_t)((value >> 24U) & UINT32_C(0xFF));
}

static uint16_t henka_network_read_u16_le(const uint8_t* source)
{
    return (uint16_t)source[0] | (uint16_t)((uint16_t)source[1] << 8U);
}

static uint32_t henka_network_read_u32_le(const uint8_t* source)
{
    return (uint32_t)source[0] |
        ((uint32_t)source[1] << 8U) |
        ((uint32_t)source[2] << 16U) |
        ((uint32_t)source[3] << 24U);
}

bool henka_network_channel_is_valid(henka_network_channel channel)
{
    return channel >= HENKA_NETWORK_CHANNEL_CONTROL &&
        channel <= HENKA_NETWORK_CHANNEL_SNAPSHOT;
}

bool henka_network_message_type_is_valid(henka_network_message_type type)
{
    switch (type)
    {
        case HENKA_NETWORK_MESSAGE_CONNECT:
        case HENKA_NETWORK_MESSAGE_DISCONNECT:
        case HENKA_NETWORK_MESSAGE_PING:
        case HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REQUEST:
        case HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_ACCEPTED:
        case HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REJECTED:
        case HENKA_NETWORK_MESSAGE_TERRAIN_DELTA:
        case HENKA_NETWORK_MESSAGE_SNAPSHOT_FRAGMENT:
        case HENKA_NETWORK_MESSAGE_SNAPSHOT_REQUEST:
            return true;
        default:
            return false;
    }
}

bool henka_network_message_payload_is_valid(
    henka_network_message_type type,
    size_t payload_size)
{
    if (payload_size > HENKA_NETWORK_MAX_PAYLOAD_BYTES)
    {
        return false;
    }
    return type != HENKA_NETWORK_MESSAGE_SNAPSHOT_FRAGMENT ||
        payload_size <= HENKA_NETWORK_MAX_SNAPSHOT_FRAGMENT_PAYLOAD;
}

henka_result henka_network_message_encode(
    henka_network_channel channel,
    henka_network_message_type type,
    const void* payload,
    size_t payload_size,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size)
{
    size_t packet_size;

    if (out_size == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_size = 0U;
    if (!henka_network_channel_is_valid(channel) ||
        !henka_network_message_type_is_valid(type) ||
        (payload == NULL && payload_size > 0U) ||
        !henka_network_message_payload_is_valid(type, payload_size) ||
        payload_size > UINT32_MAX)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    packet_size = HENKA_NETWORK_PROTOCOL_HEADER_BYTES + payload_size;
    if (buffer == NULL || buffer_capacity < packet_size)
    {
        return HENKA_ERROR_LIMIT;
    }

    henka_network_write_u32_le(buffer, HENKA_NETWORK_PROTOCOL_MAGIC);
    henka_network_write_u16_le(buffer + 4U, HENKA_NETWORK_PROTOCOL_VERSION);
    buffer[6] = (uint8_t)channel;
    buffer[7] = (uint8_t)type;
    henka_network_write_u32_le(buffer + 8U, (uint32_t)payload_size);
    if (payload_size > 0U)
    {
        memcpy(buffer + HENKA_NETWORK_PROTOCOL_HEADER_BYTES, payload, payload_size);
    }
    *out_size = packet_size;
    return HENKA_SUCCESS;
}

henka_result henka_network_message_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_network_message_view* out_message)
{
    uint32_t payload_size;
    henka_network_channel channel;
    henka_network_message_type type;

    if (out_message == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_message = (henka_network_message_view){0};
    if (buffer == NULL || buffer_size < HENKA_NETWORK_PROTOCOL_HEADER_BYTES)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_network_read_u32_le(buffer) != HENKA_NETWORK_PROTOCOL_MAGIC)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_network_read_u16_le(buffer + 4U) != HENKA_NETWORK_PROTOCOL_VERSION)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    channel = (henka_network_channel)buffer[6];
    type = (henka_network_message_type)buffer[7];
    payload_size = henka_network_read_u32_le(buffer + 8U);
    if (!henka_network_channel_is_valid(channel) ||
        !henka_network_message_type_is_valid(type) ||
        !henka_network_message_payload_is_valid(type, payload_size) ||
        payload_size > buffer_size - HENKA_NETWORK_PROTOCOL_HEADER_BYTES ||
        HENKA_NETWORK_PROTOCOL_HEADER_BYTES + (size_t)payload_size != buffer_size)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    out_message->channel = channel;
    out_message->type = type;
    out_message->payload = buffer + HENKA_NETWORK_PROTOCOL_HEADER_BYTES;
    out_message->payload_size = payload_size;
    return HENKA_SUCCESS;
}
