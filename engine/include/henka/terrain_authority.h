#ifndef HENKA_TERRAIN_AUTHORITY_H
#define HENKA_TERRAIN_AUTHORITY_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/network.h>
#include <henka/terrain_network.h>
#include <henka/terrain_storage.h>

#define HENKA_TERRAIN_DEFAULT_EDIT_RATE_PER_SECOND 20U
#define HENKA_TERRAIN_MAX_EDIT_RATE_PER_SECOND 120U

typedef struct henka_terrain_authority henka_terrain_authority;

typedef bool (*henka_terrain_edit_permission_callback)(
    void* user_data,
    henka_network_peer_id peer_id,
    const henka_terrain_edit_request* request);

typedef struct henka_terrain_authority_desc
{
    henka_terrain_world* world;
    henka_terrain_storage* storage;
    uint32_t max_clients;
    uint32_t edit_rate_per_second;
    henka_terrain_edit_permission_callback permission_callback;
    void* permission_user_data;
} henka_terrain_authority_desc;

typedef struct henka_terrain_authority_response
{
    bool accepted;
    henka_terrain_edit_acceptance acceptance;
    henka_terrain_edit_rejection rejection;
} henka_terrain_authority_response;

henka_terrain_authority_desc henka_terrain_authority_desc_default(void);
henka_result henka_terrain_authority_create(
    const henka_terrain_authority_desc* desc,
    henka_terrain_authority** out_authority);
void henka_terrain_authority_destroy(henka_terrain_authority* authority);
void henka_terrain_authority_retire_peer(
    henka_terrain_authority* authority,
    henka_network_peer_id peer_id);
henka_result henka_terrain_authority_process_request(
    henka_terrain_authority* authority,
    henka_network_peer_id peer_id,
    const henka_terrain_edit_request* request,
    uint64_t now_milliseconds,
    henka_terrain_authority_response* out_response);
uint64_t henka_terrain_authority_get_next_command_id(
    const henka_terrain_authority* authority);

#endif
