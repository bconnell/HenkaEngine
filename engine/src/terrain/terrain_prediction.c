#include <henka/terrain_prediction.h>

#include <string.h>

#include <henka/memory.h>

typedef struct henka_terrain_prediction_pending
{
    bool active;
    henka_terrain_edit_command command;
} henka_terrain_prediction_pending;

struct henka_terrain_prediction
{
    henka_terrain_world* authoritative_world;
    henka_terrain_world* predicted_world;
    henka_terrain_prediction_desc desc;
    henka_terrain_prediction_pending* pending;
    henka_terrain_prediction_stats stats;
};

static uint32_t henka_terrain_prediction_find_pending(
    const henka_terrain_prediction* prediction,
    uint64_t client_nonce)
{
    uint32_t index;
    for (index = 0U; index < prediction->desc.max_pending_commands; ++index)
    {
        if (prediction->pending[index].active &&
            prediction->pending[index].command.client_nonce == client_nonce)
        {
            return index;
        }
    }
    return prediction->desc.max_pending_commands;
}

static henka_result henka_terrain_prediction_rebuild(
    henka_terrain_prediction* prediction)
{
    henka_terrain_world_desc world_desc;
    uint32_t region_z;
    uint32_t region_x;
    uint32_t index;

    if (henka_terrain_world_get_desc(
            prediction->authoritative_world, &world_desc) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (region_z = 0U; region_z < world_desc.regions_down; ++region_z)
    {
        for (region_x = 0U; region_x < world_desc.regions_across; ++region_x)
        {
            henka_terrain_region_id region_id = {(int32_t)region_x, (int32_t)region_z};
            henka_terrain_region_state state;
            if (henka_terrain_world_get_region_state(
                    prediction->authoritative_world, region_id, &state) == HENKA_SUCCESS &&
                state.cpu_resident)
            {
                if (henka_terrain_world_copy_region_samples(
                        prediction->authoritative_world,
                        prediction->predicted_world,
                        region_id) != HENKA_SUCCESS)
                {
                    return HENKA_ERROR_LIMIT;
                }
            }
            else
            {
                (void)henka_terrain_world_release_region(
                    prediction->predicted_world, region_id);
            }
        }
    }
    for (index = 0U; index < prediction->desc.max_pending_commands; ++index)
    {
        if (prediction->pending[index].active &&
            henka_terrain_world_apply_edit(
                prediction->predicted_world,
                &prediction->pending[index].command,
                prediction->pending[index].command.client_nonce) != HENKA_SUCCESS)
        {
            ++prediction->stats.replay_failure_count;
            prediction->stats.prediction_enabled = false;
            return HENKA_ERROR_ASSET_SOURCE;
        }
    }
    ++prediction->stats.replay_count;
    prediction->stats.prediction_enabled = true;
    return HENKA_SUCCESS;
}

henka_terrain_prediction_desc henka_terrain_prediction_desc_default(void)
{
    return (henka_terrain_prediction_desc){NULL, 16U};
}

henka_result henka_terrain_prediction_create(
    const henka_terrain_prediction_desc* desc,
    henka_terrain_prediction** out_prediction)
{
    henka_terrain_prediction_desc defaults;
    henka_terrain_prediction* prediction;
    henka_terrain_world_desc world_desc;

    if (out_prediction == NULL || desc == NULL || desc->authoritative_world == NULL ||
        desc->max_pending_commands == 0U ||
        desc->max_pending_commands > HENKA_TERRAIN_PREDICTION_MAX_PENDING ||
        henka_terrain_world_get_desc(desc->authoritative_world, &world_desc) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_prediction = NULL;
    defaults = henka_terrain_prediction_desc_default();
    (void)defaults;
    prediction = henka_calloc(1U, sizeof(*prediction));
    if (prediction == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    prediction->pending = henka_calloc(
        desc->max_pending_commands, sizeof(*prediction->pending));
    if (prediction->pending == NULL)
    {
        henka_free(prediction);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    if (henka_terrain_world_create(&world_desc, &prediction->predicted_world) != HENKA_SUCCESS)
    {
        henka_free(prediction->pending);
        henka_free(prediction);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    prediction->authoritative_world = desc->authoritative_world;
    prediction->desc = *desc;
    prediction->stats.max_pending_commands = desc->max_pending_commands;
    prediction->stats.prediction_enabled = true;
    if (henka_terrain_prediction_rebuild(prediction) != HENKA_SUCCESS)
    {
        henka_terrain_prediction_destroy(prediction);
        return HENKA_ERROR_ASSET_SOURCE;
    }
    *out_prediction = prediction;
    return HENKA_SUCCESS;
}

void henka_terrain_prediction_destroy(henka_terrain_prediction* prediction)
{
    if (prediction == NULL)
    {
        return;
    }
    henka_terrain_world_destroy(prediction->predicted_world);
    henka_free(prediction->pending);
    henka_free(prediction);
}

henka_result henka_terrain_prediction_submit(
    henka_terrain_prediction* prediction,
    const henka_terrain_edit_command* command)
{
    uint32_t index;
    if (prediction == NULL || command == NULL || command->client_nonce == 0U ||
        henka_terrain_prediction_find_pending(prediction, command->client_nonce) <
            prediction->desc.max_pending_commands ||
        henka_terrain_edit_command_validate(prediction->authoritative_world, command) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < prediction->desc.max_pending_commands; ++index)
    {
        if (!prediction->pending[index].active)
        {
            prediction->pending[index].active = true;
            prediction->pending[index].command = *command;
            ++prediction->stats.pending_command_count;
            ++prediction->stats.submitted_count;
            if (henka_terrain_prediction_rebuild(prediction) != HENKA_SUCCESS)
            {
                prediction->pending[index].active = false;
                --prediction->stats.pending_command_count;
                (void)henka_terrain_prediction_rebuild(prediction);
                return HENKA_ERROR_ASSET_SOURCE;
            }
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_LIMIT;
}

static henka_result henka_terrain_prediction_remove(
    henka_terrain_prediction* prediction,
    uint64_t client_nonce,
    bool accepted)
{
    uint32_t index;
    if (prediction == NULL || client_nonce == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    index = henka_terrain_prediction_find_pending(prediction, client_nonce);
    if (index >= prediction->desc.max_pending_commands)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    prediction->pending[index].active = false;
    if (prediction->stats.pending_command_count > 0U)
    {
        --prediction->stats.pending_command_count;
    }
    if (accepted) ++prediction->stats.accepted_count;
    else ++prediction->stats.rejected_count;
    return henka_terrain_prediction_rebuild(prediction);
}

henka_result henka_terrain_prediction_accept(
    henka_terrain_prediction* prediction,
    uint64_t client_nonce)
{
    return henka_terrain_prediction_remove(prediction, client_nonce, true);
}

henka_result henka_terrain_prediction_reject(
    henka_terrain_prediction* prediction,
    uint64_t client_nonce)
{
    return henka_terrain_prediction_remove(prediction, client_nonce, false);
}

henka_result henka_terrain_prediction_refresh(
    henka_terrain_prediction* prediction)
{
    if (prediction == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_terrain_prediction_rebuild(prediction);
}

henka_terrain_world* henka_terrain_prediction_get_world(
    henka_terrain_prediction* prediction)
{
    return prediction == NULL ? NULL : prediction->predicted_world;
}

void henka_terrain_prediction_get_stats(
    const henka_terrain_prediction* prediction,
    henka_terrain_prediction_stats* out_stats)
{
    if (out_stats == NULL)
    {
        return;
    }
    *out_stats = prediction == NULL ?
        (henka_terrain_prediction_stats){0} : prediction->stats;
}
