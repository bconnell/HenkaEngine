#ifndef HENKA_TERRAIN_SERVER_H
#define HENKA_TERRAIN_SERVER_H

#include <stdint.h>

#include <henka/network.h>
#include <henka/terrain_authority.h>

typedef struct henka_terrain_server henka_terrain_server;

typedef struct henka_terrain_server_desc
{
    henka_network_server* network;
    henka_terrain_world* world;
    henka_terrain_storage* storage;
    uint32_t max_clients;
    uint32_t edit_rate_per_second;
    henka_terrain_edit_permission_callback permission_callback;
    void* permission_user_data;
} henka_terrain_server_desc;

typedef struct henka_terrain_server_diagnostics
{
    uint64_t processed_edit_request_count;
    uint64_t accepted_edit_count;
    uint64_t rejected_edit_count;
    uint64_t malformed_edit_count;
    uint64_t protocol_disconnect_count;
} henka_terrain_server_diagnostics;

henka_terrain_server_desc henka_terrain_server_desc_default(void);
henka_result henka_terrain_server_create(
    const henka_terrain_server_desc* desc,
    henka_terrain_server** out_server);
void henka_terrain_server_destroy(henka_terrain_server* server);
henka_result henka_terrain_server_handle_event(
    henka_terrain_server* server,
    const henka_network_event* event,
    uint64_t now_milliseconds);
henka_result henka_terrain_server_poll(
    henka_terrain_server* server,
    uint32_t timeout_milliseconds,
    uint64_t now_milliseconds);
void henka_terrain_server_get_diagnostics(
    const henka_terrain_server* server,
    henka_terrain_server_diagnostics* out_diagnostics);

#endif
