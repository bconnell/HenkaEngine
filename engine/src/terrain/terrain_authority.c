#include <henka/terrain_authority.h>

#include <string.h>

#include <henka/memory.h>

typedef struct henka_terrain_rate_record
{
    henka_network_peer_id peer_id;
    uint64_t window_start_milliseconds;
    uint32_t request_count;
} henka_terrain_rate_record;

typedef struct henka_terrain_authority_backup
{
    henka_terrain_region_id id;
    henka_terrain_region_state state;
    henka_terrain_sample* samples;
} henka_terrain_authority_backup;

struct henka_terrain_authority
{
    henka_terrain_world* world;
    henka_terrain_storage* storage;
    uint32_t max_clients;
    uint32_t edit_rate_per_second;
    henka_terrain_edit_permission_callback permission_callback;
    void* permission_user_data;
    uint64_t next_command_id;
    henka_terrain_rate_record* rate_records;
};

henka_terrain_authority_desc henka_terrain_authority_desc_default(void)
{
    return (henka_terrain_authority_desc){
        NULL,
        NULL,
        32U,
        HENKA_TERRAIN_DEFAULT_EDIT_RATE_PER_SECOND,
        NULL,
        NULL};
}

henka_result henka_terrain_authority_create(
    const henka_terrain_authority_desc* desc,
    henka_terrain_authority** out_authority)
{
    henka_terrain_authority* authority;
    if (out_authority == NULL || desc == NULL || desc->world == NULL || desc->storage == NULL ||
        desc->max_clients == 0U || desc->max_clients > 256U || desc->edit_rate_per_second == 0U ||
        desc->edit_rate_per_second > HENKA_TERRAIN_MAX_EDIT_RATE_PER_SECOND)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_authority = NULL;
    authority = henka_calloc(1U, sizeof(*authority));
    if (authority == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    authority->rate_records = henka_calloc(desc->max_clients, sizeof(*authority->rate_records));
    if (authority->rate_records == NULL)
    {
        henka_free(authority);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    authority->world = desc->world;
    authority->storage = desc->storage;
    authority->max_clients = desc->max_clients;
    authority->edit_rate_per_second = desc->edit_rate_per_second;
    authority->permission_callback = desc->permission_callback;
    authority->permission_user_data = desc->permission_user_data;
    authority->next_command_id = 1U;
    *out_authority = authority;
    return HENKA_SUCCESS;
}

void henka_terrain_authority_destroy(henka_terrain_authority* authority)
{
    if (authority == NULL)
    {
        return;
    }
    henka_free(authority->rate_records);
    henka_free(authority);
}

void henka_terrain_authority_retire_peer(
    henka_terrain_authority* authority,
    henka_network_peer_id peer_id)
{
    uint32_t index;
    if (authority == NULL || peer_id == HENKA_NETWORK_INVALID_PEER_ID)
    {
        return;
    }
    for (index = 0U; index < authority->max_clients; ++index)
    {
        if (authority->rate_records[index].peer_id == peer_id)
        {
            authority->rate_records[index] = (henka_terrain_rate_record){0};
            return;
        }
    }
}

static henka_result henka_terrain_authority_reject(
    const henka_terrain_edit_request* request,
    henka_terrain_edit_rejection_reason reason,
    henka_terrain_authority_response* response)
{
    response->accepted = false;
    response->acceptance = (henka_terrain_edit_acceptance){0};
    response->rejection.client_nonce = request == NULL ? 0U : request->client_nonce;
    response->rejection.reason = reason;
    return HENKA_SUCCESS;
}

static void henka_terrain_authority_free_backups(
    henka_terrain_authority_backup backups[HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS])
{
    uint32_t index;
    for (index = 0U; index < HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS; ++index)
    {
        henka_free(backups[index].samples);
        backups[index].samples = NULL;
    }
}

static henka_terrain_rate_record* henka_terrain_authority_rate_record(
    henka_terrain_authority* authority,
    henka_network_peer_id peer_id)
{
    uint32_t index;
    henka_terrain_rate_record* free_record = NULL;
    for (index = 0U; index < authority->max_clients; ++index)
    {
        if (authority->rate_records[index].peer_id == peer_id)
        {
            return &authority->rate_records[index];
        }
        if (free_record == NULL && authority->rate_records[index].peer_id == HENKA_NETWORK_INVALID_PEER_ID)
        {
            free_record = &authority->rate_records[index];
        }
    }
    if (free_record != NULL)
    {
        free_record->peer_id = peer_id;
    }
    return free_record;
}

henka_result henka_terrain_authority_process_request(
    henka_terrain_authority* authority,
    henka_network_peer_id peer_id,
    const henka_terrain_edit_request* request,
    uint64_t now_milliseconds,
    henka_terrain_authority_response* out_response)
{
    henka_terrain_region_id affected[HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS];
    henka_terrain_authority_backup backups[HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS] = {0};
    henka_terrain_rate_record* rate_record;
    uint32_t affected_count = HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS;
    uint32_t index;
    uint64_t command_id = 0U;
    henka_result result;

    if (out_response == NULL || authority == NULL || request == NULL ||
        peer_id == HENKA_NETWORK_INVALID_PEER_ID)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_response = (henka_terrain_authority_response){0};
    rate_record = henka_terrain_authority_rate_record(authority, peer_id);
    if (rate_record == NULL)
    {
        return henka_terrain_authority_reject(request, HENKA_TERRAIN_EDIT_REJECT_LIMIT, out_response);
    }
    if (now_milliseconds < rate_record->window_start_milliseconds ||
        now_milliseconds - rate_record->window_start_milliseconds >= 1000U)
    {
        rate_record->window_start_milliseconds = now_milliseconds;
        rate_record->request_count = 0U;
    }
    if (rate_record->request_count >= authority->edit_rate_per_second)
    {
        return henka_terrain_authority_reject(request, HENKA_TERRAIN_EDIT_REJECT_RATE_LIMITED, out_response);
    }
    ++rate_record->request_count;
    {
        henka_terrain_world_desc desc;
        if (henka_terrain_world_get_desc(authority->world, &desc) != HENKA_SUCCESS)
        {
            return henka_terrain_authority_reject(request, HENKA_TERRAIN_EDIT_REJECT_INVALID, out_response);
        }
        if (request->world_identity != desc.world_identity)
        {
            return henka_terrain_authority_reject(request, HENKA_TERRAIN_EDIT_REJECT_WORLD_MISMATCH, out_response);
        }
        if (request->base_asset_identity != desc.base_asset_identity)
        {
            return henka_terrain_authority_reject(request, HENKA_TERRAIN_EDIT_REJECT_BASE_MISMATCH, out_response);
        }
    }
    if (authority->permission_callback != NULL &&
        !authority->permission_callback(authority->permission_user_data, peer_id, request))
    {
        return henka_terrain_authority_reject(request, HENKA_TERRAIN_EDIT_REJECT_UNAUTHORIZED, out_response);
    }
    result = henka_terrain_edit_get_affected_regions(
        authority->world, &request->command, affected, &affected_count);
    if (result != HENKA_SUCCESS || request->affected_region_count != affected_count)
    {
        return henka_terrain_authority_reject(request, HENKA_TERRAIN_EDIT_REJECT_INVALID, out_response);
    }
    for (index = 0U; index < affected_count; ++index)
    {
        if (!henka_terrain_region_id_equal(affected[index], request->affected_regions[index].region_id))
        {
            henka_terrain_authority_free_backups(backups);
            return henka_terrain_authority_reject(request, HENKA_TERRAIN_EDIT_REJECT_INVALID, out_response);
        }
        if (henka_terrain_world_get_region_state(
                authority->world, affected[index], &backups[index].state) != HENKA_SUCCESS ||
            backups[index].state.revision != request->affected_regions[index].revision)
        {
            henka_terrain_authority_free_backups(backups);
            return henka_terrain_authority_reject(request, HENKA_TERRAIN_EDIT_REJECT_STALE_REVISION, out_response);
        }
        backups[index].id = affected[index];
        {
            size_t sample_count;
            const henka_terrain_sample* source;
            if (henka_terrain_world_get_region_samples(
                    authority->world, affected[index], &source, &sample_count) != HENKA_SUCCESS)
            {
                henka_terrain_authority_free_backups(backups);
                return henka_terrain_authority_reject(request, HENKA_TERRAIN_EDIT_REJECT_INVALID, out_response);
            }
            backups[index].samples = henka_malloc(sample_count * sizeof(*source));
            if (backups[index].samples == NULL)
            {
                result = HENKA_ERROR_OUT_OF_MEMORY;
                goto rollback;
            }
            memcpy(backups[index].samples, source, sample_count * sizeof(*source));
        }
    }
    command_id = authority->next_command_id;
    if (command_id == UINT64_MAX || henka_terrain_world_apply_edit(
            authority->world, &request->command, command_id) != HENKA_SUCCESS)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto rollback;
    }
    result = henka_terrain_storage_begin(authority->storage, command_id);
    for (index = 0U; result == HENKA_SUCCESS && index < affected_count; ++index)
    {
        const henka_terrain_sample* samples;
        size_t sample_count;
        henka_terrain_region_state state = {0};
        result = henka_terrain_world_get_region_samples(
            authority->world, affected[index], &samples, &sample_count);
        if (result == HENKA_SUCCESS)
        {
            result = henka_terrain_world_get_region_state(authority->world, affected[index], &state);
        }
        if (result == HENKA_SUCCESS)
        {
            result = henka_terrain_storage_write_region(
                authority->storage, affected[index], state.revision, state.generation,
                samples, sample_count);
        }
    }
    if (result == HENKA_SUCCESS)
    {
        result = henka_terrain_storage_commit(authority->storage, command_id);
    }
    if (result != HENKA_SUCCESS)
    {
rollback:
        if (command_id != 0U)
        {
            (void)henka_terrain_storage_abort(authority->storage, command_id);
        }
        for (index = 0U; index < affected_count; ++index)
        {
            const henka_terrain_sample* current;
            size_t sample_count;
            if (backups[index].samples != NULL &&
                henka_terrain_world_get_region_samples(
                    authority->world, backups[index].id, &current, &sample_count) == HENKA_SUCCESS)
            {
                memcpy((void*)current, backups[index].samples, sample_count * sizeof(*current));
                (void)henka_terrain_world_set_region_revision(
                    authority->world, backups[index].id, backups[index].state.revision,
                    backups[index].state.generation, backups[index].state.dirty);
            }
            henka_free(backups[index].samples);
        }
        return henka_terrain_authority_reject(request, HENKA_TERRAIN_EDIT_REJECT_LIMIT, out_response);
    }
    /* The journal commit is the authority boundary. Keep the in-memory
     * regions consistent with the durable transaction so streaming does not
     * retain already-persisted edits as dirty work. */
    for (index = 0U; index < affected_count; ++index)
    {
        henka_terrain_region_state state;
        if (henka_terrain_world_get_region_state(
                authority->world, affected[index], &state) != HENKA_SUCCESS ||
            henka_terrain_world_set_region_revision(
                authority->world, affected[index], state.revision, state.generation, false) != HENKA_SUCCESS)
        {
            /* Storage is already committed; retaining the dirty bit is safer
             * than reporting a false rollback after the durable boundary. */
            return HENKA_ERROR_UNKNOWN;
        }
    }
    out_response->accepted = true;
    out_response->acceptance.client_nonce = request->client_nonce;
    out_response->acceptance.server_command_id = command_id;
    out_response->acceptance.affected_region_count = affected_count;
    for (index = 0U; index < affected_count; ++index)
    {
        henka_terrain_world_get_region_state(authority->world, affected[index], &backups[index].state);
        out_response->acceptance.affected_regions[index] =
            (henka_terrain_network_region_revision){affected[index], backups[index].state.revision};
        henka_free(backups[index].samples);
    }
    ++authority->next_command_id;
    return HENKA_SUCCESS;
}

uint64_t henka_terrain_authority_get_next_command_id(
    const henka_terrain_authority* authority)
{
    return authority == NULL ? 0U : authority->next_command_id;
}
