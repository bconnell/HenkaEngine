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
#define HENKA_NETWORK_MAX_POLL_TIMEOUT_MILLISECONDS 1000U

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

typedef uint32_t henka_network_peer_id;
#define HENKA_NETWORK_INVALID_PEER_ID UINT32_C(0)

typedef struct henka_network_endpoint
{
    char host[64];
    uint16_t port;
} henka_network_endpoint;

typedef enum henka_network_disconnect_reason
{
    HENKA_NETWORK_DISCONNECT_REASON_NONE = 0,
    HENKA_NETWORK_DISCONNECT_REASON_SERVER_SHUTDOWN,
    HENKA_NETWORK_DISCONNECT_REASON_PROTOCOL,
    HENKA_NETWORK_DISCONNECT_REASON_TIMEOUT,
    HENKA_NETWORK_DISCONNECT_REASON_APPLICATION
} henka_network_disconnect_reason;

typedef enum henka_network_event_type
{
    HENKA_NETWORK_EVENT_NONE = 0,
    HENKA_NETWORK_EVENT_CONNECTED,
    HENKA_NETWORK_EVENT_DISCONNECTED,
    HENKA_NETWORK_EVENT_MESSAGE
} henka_network_event_type;

typedef struct henka_network_event
{
    henka_network_event_type type;
    henka_network_peer_id peer_id;
    henka_network_disconnect_reason disconnect_reason;
    henka_network_message_view message;
} henka_network_event;

typedef struct henka_network_diagnostics
{
    uint32_t connected_peer_count;
    uint64_t sent_message_count;
    uint64_t received_message_count;
    uint64_t malformed_packet_count;
    uint64_t rejected_send_count;
    uint64_t dropped_packet_count;
} henka_network_diagnostics;

typedef struct henka_network_server_desc
{
    henka_network_endpoint bind_endpoint;
    uint32_t max_clients;
} henka_network_server_desc;

typedef struct henka_network_client_desc
{
    henka_network_endpoint remote_endpoint;
} henka_network_client_desc;

typedef struct henka_network_server henka_network_server;
typedef struct henka_network_client henka_network_client;

henka_network_server_desc henka_network_server_desc_default(void);
henka_network_client_desc henka_network_client_desc_default(void);
henka_result henka_network_server_create(
    const henka_network_server_desc* desc,
    henka_network_server** out_server);
void henka_network_server_destroy(henka_network_server* server);
henka_result henka_network_client_create(
    const henka_network_client_desc* desc,
    henka_network_client** out_client);
void henka_network_client_destroy(henka_network_client* client);

/* Polling produces one event. Message payload memory remains valid until the next poll. */
henka_result henka_network_server_poll(
    henka_network_server* server,
    uint32_t timeout_milliseconds,
    henka_network_event* out_event);
henka_result henka_network_client_poll(
    henka_network_client* client,
    uint32_t timeout_milliseconds,
    henka_network_event* out_event);
henka_result henka_network_server_send(
    henka_network_server* server,
    henka_network_peer_id peer_id,
    henka_network_channel channel,
    henka_network_message_type type,
    const void* payload,
    size_t payload_size);
henka_result henka_network_client_send(
    henka_network_client* client,
    henka_network_channel channel,
    henka_network_message_type type,
    const void* payload,
    size_t payload_size);
henka_result henka_network_server_disconnect(
    henka_network_server* server,
    henka_network_peer_id peer_id,
    henka_network_disconnect_reason reason);
void henka_network_server_get_diagnostics(
    const henka_network_server* server,
    henka_network_diagnostics* out_diagnostics);
void henka_network_client_get_diagnostics(
    const henka_network_client* client,
    henka_network_diagnostics* out_diagnostics);

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
