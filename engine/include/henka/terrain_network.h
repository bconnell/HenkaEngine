#ifndef HENKA_TERRAIN_NETWORK_H
#define HENKA_TERRAIN_NETWORK_H

#include <stddef.h>
#include <stdint.h>

#include <henka/terrain_edit.h>

#define HENKA_TERRAIN_NETWORK_MAX_AFFECTED_REGIONS 16U
#define HENKA_TERRAIN_NETWORK_MAX_EDIT_REQUEST_BYTES 512U
#define HENKA_TERRAIN_NETWORK_MAX_EDIT_RESPONSE_BYTES 320U

typedef struct henka_terrain_network_region_revision
{
    henka_terrain_region_id region_id;
    henka_terrain_revision revision;
} henka_terrain_network_region_revision;

typedef struct henka_terrain_edit_request
{
    henka_terrain_world_identity world_identity;
    henka_terrain_base_asset_identity base_asset_identity;
    uint64_t client_nonce;
    henka_terrain_edit_command command;
    uint32_t affected_region_count;
    henka_terrain_network_region_revision affected_regions[HENKA_TERRAIN_NETWORK_MAX_AFFECTED_REGIONS];
} henka_terrain_edit_request;

typedef struct henka_terrain_edit_acceptance
{
    uint64_t client_nonce;
    uint64_t server_command_id;
    uint32_t affected_region_count;
    henka_terrain_network_region_revision affected_regions[HENKA_TERRAIN_NETWORK_MAX_AFFECTED_REGIONS];
} henka_terrain_edit_acceptance;

typedef enum henka_terrain_edit_rejection_reason
{
    HENKA_TERRAIN_EDIT_REJECT_INVALID = 0,
    HENKA_TERRAIN_EDIT_REJECT_WORLD_MISMATCH,
    HENKA_TERRAIN_EDIT_REJECT_BASE_MISMATCH,
    HENKA_TERRAIN_EDIT_REJECT_UNAUTHORIZED,
    HENKA_TERRAIN_EDIT_REJECT_STALE_REVISION,
    HENKA_TERRAIN_EDIT_REJECT_RATE_LIMITED,
    HENKA_TERRAIN_EDIT_REJECT_LIMIT
} henka_terrain_edit_rejection_reason;

typedef struct henka_terrain_edit_rejection
{
    uint64_t client_nonce;
    henka_terrain_edit_rejection_reason reason;
} henka_terrain_edit_rejection;

henka_result henka_terrain_edit_request_encode(
    const henka_terrain_edit_request* request,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size);
henka_result henka_terrain_edit_request_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_edit_request* out_request);
henka_result henka_terrain_edit_acceptance_encode(
    const henka_terrain_edit_acceptance* acceptance,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size);
henka_result henka_terrain_edit_acceptance_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_edit_acceptance* out_acceptance);
henka_result henka_terrain_edit_rejection_encode(
    const henka_terrain_edit_rejection* rejection,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size);
henka_result henka_terrain_edit_rejection_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_edit_rejection* out_rejection);

#endif
