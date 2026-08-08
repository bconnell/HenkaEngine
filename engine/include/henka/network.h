#ifndef HENKA_NETWORK_H
#define HENKA_NETWORK_H

#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>

#define HENKA_NETWORK_PROTOCOL_MAGIC UINT32_C(0x48454E4B)
#define HENKA_NETWORK_PROTOCOL_VERSION UINT16_C(1)
#define HENKA_NETWORK_MAX_PACKET_BYTES (64U * 1024U)
#define HENKA_NETWORK_PROTOCOL_HEADER_BYTES 12U
#define HENKA_NETWORK_MAX_PAYLOAD_BYTES \
    (HENKA_NETWORK_MAX_PACKET_BYTES - HENKA_NETWORK_PROTOCOL_HEADER_BYTES)
#define HENKA_NETWORK_MAX_SNAPSHOT_FRAGMENT_PAYLOAD (32U * 1024U)

typedef enum henka_network_channel
{
    HENKA_NETWORK_CHANNEL_CONTROL = 0,
    HENKA_NETWORK_CHANNEL_TERRAIN = 1,
    HENKA_NETWORK_CHANNEL_SNAPSHOT = 2
} henka_network_channel;

typedef enum henka_network_message_type
{
    HENKA_NETWORK_MESSAGE_CONNECT = 1,
    HENKA_NETWORK_MESSAGE_DISCONNECT = 2,
    HENKA_NETWORK_MESSAGE_PING = 3,
    HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REQUEST = 16,
    HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_ACCEPTED = 17,
    HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REJECTED = 18,
    HENKA_NETWORK_MESSAGE_TERRAIN_DELTA = 32,
    HENKA_NETWORK_MESSAGE_SNAPSHOT_FRAGMENT = 33,
    HENKA_NETWORK_MESSAGE_SNAPSHOT_REQUEST = 34
} henka_network_message_type;

typedef struct henka_network_message_view
{
    henka_network_channel channel;
    henka_network_message_type type;
    const uint8_t* payload;
    uint32_t payload_size;
} henka_network_message_view;

/* Encodes a Henka-owned message into a caller-owned bounded packet buffer. */
henka_result henka_network_message_encode(
    henka_network_channel channel,
    henka_network_message_type type,
    const void* payload,
    size_t payload_size,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size);

/* Decodes a complete packet without taking ownership of its input bytes. */
henka_result henka_network_message_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_network_message_view* out_message);

#endif
