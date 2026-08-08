#ifndef HENKA_TERRAIN_NETWORK_H
#define HENKA_TERRAIN_NETWORK_H

#include <stddef.h>
#include <stdint.h>

#include <henka/network.h>
#include <henka/terrain_edit.h>

#define HENKA_TERRAIN_NETWORK_MAX_AFFECTED_REGIONS 16U
#define HENKA_TERRAIN_NETWORK_MAX_EDIT_REQUEST_BYTES 512U
#define HENKA_TERRAIN_NETWORK_MAX_EDIT_RESPONSE_BYTES 320U
#define HENKA_TERRAIN_NETWORK_MAX_DELTA_BYTES 512U
#define HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_REQUEST_BYTES 32U
#define HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_HEADER_BYTES 64U
#define HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES \
    (HENKA_NETWORK_MAX_SNAPSHOT_FRAGMENT_PAYLOAD - HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_HEADER_BYTES)
#define HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENTS 1024U

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

typedef struct henka_terrain_edit_delta
{
    henka_terrain_world_identity world_identity;
    henka_terrain_base_asset_identity base_asset_identity;
    uint64_t client_nonce;
    uint64_t server_command_id;
    henka_terrain_edit_command command;
    uint32_t affected_region_count;
    henka_terrain_network_region_revision affected_regions[HENKA_TERRAIN_NETWORK_MAX_AFFECTED_REGIONS];
} henka_terrain_edit_delta;

typedef struct henka_terrain_snapshot_request
{
    henka_terrain_world_identity world_identity;
    henka_terrain_base_asset_identity base_asset_identity;
    henka_terrain_region_id region_id;
    henka_terrain_revision expected_revision;
} henka_terrain_snapshot_request;

typedef struct henka_terrain_snapshot_fragment
{
    henka_terrain_world_identity world_identity;
    henka_terrain_base_asset_identity base_asset_identity;
    uint64_t transfer_id;
    henka_terrain_region_id region_id;
    henka_terrain_revision revision;
    henka_terrain_generation generation;
    uint32_t fragment_index;
    uint32_t fragment_count;
    uint32_t total_bytes;
    uint32_t data_size;
    const uint8_t* data;
} henka_terrain_snapshot_fragment;

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
henka_result henka_terrain_edit_delta_encode(
    const henka_terrain_edit_delta* delta,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size);
henka_result henka_terrain_edit_delta_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_edit_delta* out_delta);
henka_result henka_terrain_snapshot_request_encode(
    const henka_terrain_snapshot_request* request,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size);
henka_result henka_terrain_snapshot_request_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_snapshot_request* out_request);
henka_result henka_terrain_snapshot_fragment_encode(
    const henka_terrain_snapshot_fragment* fragment,
    uint8_t* buffer,
    size_t buffer_capacity,
    size_t* out_size);
henka_result henka_terrain_snapshot_fragment_decode(
    const uint8_t* buffer,
    size_t buffer_size,
    henka_terrain_snapshot_fragment* out_fragment);

#endif
