#ifndef HENKA_TERRAIN_PREDICTION_H
#define HENKA_TERRAIN_PREDICTION_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/terrain_edit.h>

#define HENKA_TERRAIN_PREDICTION_MAX_PENDING 64U

typedef struct henka_terrain_prediction henka_terrain_prediction;

typedef struct henka_terrain_prediction_desc
{
    henka_terrain_world* authoritative_world;
    uint32_t max_pending_commands;
} henka_terrain_prediction_desc;

typedef struct henka_terrain_prediction_stats
{
    uint32_t pending_command_count;
    uint32_t max_pending_commands;
    uint64_t submitted_count;
    uint64_t replay_count;
    uint64_t accepted_count;
    uint64_t rejected_count;
    uint64_t replay_failure_count;
    bool prediction_enabled;
} henka_terrain_prediction_stats;

henka_terrain_prediction_desc henka_terrain_prediction_desc_default(void);
henka_result henka_terrain_prediction_create(
    const henka_terrain_prediction_desc* desc,
    henka_terrain_prediction** out_prediction);
void henka_terrain_prediction_destroy(henka_terrain_prediction* prediction);
henka_result henka_terrain_prediction_submit(
    henka_terrain_prediction* prediction,
    const henka_terrain_edit_command* command);
henka_result henka_terrain_prediction_accept(
    henka_terrain_prediction* prediction,
    uint64_t client_nonce);
henka_result henka_terrain_prediction_reject(
    henka_terrain_prediction* prediction,
    uint64_t client_nonce);
henka_result henka_terrain_prediction_refresh(
    henka_terrain_prediction* prediction);
henka_terrain_world* henka_terrain_prediction_get_world(
    henka_terrain_prediction* prediction);
void henka_terrain_prediction_get_stats(
    const henka_terrain_prediction* prediction,
    henka_terrain_prediction_stats* out_stats);

#endif
