#ifndef HENKA_NETWORK_INTERNAL_H
#define HENKA_NETWORK_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/network.h>

bool henka_network_channel_is_valid(henka_network_channel channel);
bool henka_network_message_type_is_valid(henka_network_message_type type);
bool henka_network_message_payload_is_valid(
    henka_network_message_type type,
    size_t payload_size);

typedef struct henka_network_transport henka_network_transport;

henka_result henka_network_transport_create(
    bool server_mode,
    const henka_network_endpoint* endpoint,
    uint32_t max_peers,
    henka_network_transport** out_transport);
void henka_network_transport_destroy(henka_network_transport* transport);
henka_result henka_network_transport_poll(
    henka_network_transport* transport,
    uint32_t timeout_milliseconds,
    henka_network_event* out_event);
henka_result henka_network_transport_send(
    henka_network_transport* transport,
    henka_network_peer_id peer_id,
    henka_network_channel channel,
    henka_network_message_type type,
    const void* payload,
    size_t payload_size);
henka_result henka_network_transport_disconnect(
    henka_network_transport* transport,
    henka_network_peer_id peer_id,
    henka_network_disconnect_reason reason);
void henka_network_transport_get_diagnostics(
    const henka_network_transport* transport,
    henka_network_diagnostics* out_diagnostics);

#endif
