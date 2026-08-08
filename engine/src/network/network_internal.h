#ifndef HENKA_NETWORK_INTERNAL_H
#define HENKA_NETWORK_INTERNAL_H

#include <stdbool.h>

#include <henka/network.h>

bool henka_network_channel_is_valid(henka_network_channel channel);
bool henka_network_message_type_is_valid(henka_network_message_type type);
bool henka_network_message_payload_is_valid(
    henka_network_message_type type,
    size_t payload_size);

#endif
