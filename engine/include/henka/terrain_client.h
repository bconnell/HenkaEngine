#ifndef HENKA_TERRAIN_CLIENT_H
#define HENKA_TERRAIN_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/terrain_prediction.h>
#include <henka/terrain_replica.h>

typedef struct henka_terrain_client henka_terrain_client;

typedef struct henka_terrain_client_desc
{
    henka_network_client* network;
    henka_terrain_world* world;
    uint32_t max_snapshot_bytes;
    uint32_t max_pending_prediction_commands;
} henka_terrain_client_desc;

typedef struct henka_terrain_client_diagnostics
{
    uint64_t connected_event_count;
    uint64_t disconnected_event_count;
    uint64_t processed_message_count;
    uint64_t malformed_message_count;
    uint64_t acceptance_count;
    uint64_t rejection_count;
    uint64_t applied_delta_count;
    uint64_t duplicate_delta_count;
    uint64_t rejected_delta_count;
    uint64_t completed_snapshot_count;
    uint64_t rejected_snapshot_count;
    uint64_t recovery_snapshot_request_count;
    uint64_t session_snapshot_request_count;
    uint32_t pending_prediction_count;
    uint64_t prediction_replay_failure_count;
    bool prediction_enabled;
    bool last_acceptance_valid;
    henka_terrain_edit_acceptance last_acceptance;
    bool last_rejection_valid;
    henka_terrain_edit_rejection last_rejection;
} henka_terrain_client_diagnostics;

henka_terrain_client_desc henka_terrain_client_desc_default(void);
henka_result henka_terrain_client_create(
    const henka_terrain_client_desc* desc,
    henka_terrain_client** out_client);
void henka_terrain_client_destroy(henka_terrain_client* client);
henka_result henka_terrain_client_send_edit_request(
    henka_terrain_client* client,
    const henka_terrain_edit_request* request);
henka_result henka_terrain_client_request_snapshot(
    henka_terrain_client* client,
    henka_terrain_snapshot_request request);
henka_result henka_terrain_client_handle_event(
    henka_terrain_client* client,
    const henka_network_event* event);
henka_result henka_terrain_client_poll(
    henka_terrain_client* client,
    uint32_t timeout_milliseconds,
    uint32_t max_events,
    uint32_t* out_event_count);
void henka_terrain_client_get_diagnostics(
    const henka_terrain_client* client,
    henka_terrain_client_diagnostics* out_diagnostics);
henka_terrain_world* henka_terrain_client_get_predicted_world(
    henka_terrain_client* client);

#endif
