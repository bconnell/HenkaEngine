#include <henka/terrain_server.h>

#include <stdbool.h>
#include <string.h>

#include <henka/memory.h>

#include "terrain_internal.h"

struct henka_terrain_server
{
    henka_network_server* network;
    henka_terrain_world* world;
    henka_terrain_storage* storage;
    henka_terrain_collision_runtime* collision_runtime;
    henka_terrain_authority* authority;
    henka_terrain_server_diagnostics diagnostics;
    uint64_t next_snapshot_transfer_id;
    henka_terrain_edit_delta delta_history[HENKA_TERRAIN_SERVER_MAX_DELTA_HISTORY];
    uint32_t delta_history_next;
    uint32_t delta_history_count;
};

henka_terrain_server_desc henka_terrain_server_desc_default(void)
{
    return (henka_terrain_server_desc){
        NULL,
        NULL,
        NULL,
        NULL,
        32U,
        HENKA_TERRAIN_DEFAULT_EDIT_RATE_PER_SECOND,
        NULL,
        NULL};
}

henka_result henka_terrain_server_create(
    const henka_terrain_server_desc* desc,
    henka_terrain_server** out_server)
{
    henka_terrain_server* server;
    henka_terrain_authority_desc authority_desc;
    if (out_server == NULL || desc == NULL || desc->network == NULL ||
        desc->world == NULL || desc->storage == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_server = NULL;
    server = henka_calloc(1U, sizeof(*server));
    if (server == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    authority_desc = henka_terrain_authority_desc_default();
    authority_desc.world = desc->world;
    authority_desc.storage = desc->storage;
    authority_desc.max_clients = desc->max_clients;
    authority_desc.edit_rate_per_second = desc->edit_rate_per_second;
    authority_desc.permission_callback = desc->permission_callback;
    authority_desc.permission_user_data = desc->permission_user_data;
    {
        henka_result result = henka_terrain_authority_create(&authority_desc, &server->authority);
        if (result != HENKA_SUCCESS)
        {
            henka_free(server);
            return result;
        }
    }
    server->network = desc->network;
    server->world = desc->world;
    server->storage = desc->storage;
    server->collision_runtime = desc->collision_runtime;
    server->next_snapshot_transfer_id = 1U;
    *out_server = server;
    return HENKA_SUCCESS;
}

void henka_terrain_server_destroy(henka_terrain_server* server)
{
    if (server == NULL)
    {
        return;
    }
    henka_terrain_authority_destroy(server->authority);
    henka_free(server);
}

static henka_result henka_terrain_server_send_rejection(
    henka_terrain_server* server,
    henka_network_peer_id peer_id,
    const henka_terrain_edit_rejection* rejection)
{
    uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_DELTA_BYTES];
    size_t payload_size;
    henka_result result = henka_terrain_edit_rejection_encode(
        rejection, payload, sizeof(payload), &payload_size);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    return henka_network_server_send(
        server->network, peer_id, HENKA_NETWORK_CHANNEL_TERRAIN,
        HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REJECTED, payload, payload_size);
}

static bool henka_terrain_server_region_precedes(
    henka_terrain_region_id left,
    henka_terrain_region_id right)
{
    return left.z < right.z || (left.z == right.z && left.x < right.x);
}

static uint64_t henka_terrain_server_region_distance_squared(
    henka_terrain_region_id center,
    henka_terrain_region_id region)
{
    const int64_t dx = (int64_t)region.x - (int64_t)center.x;
    const int64_t dz = (int64_t)region.z - (int64_t)center.z;
    return (uint64_t)(dx * dx + dz * dz);
}

static bool henka_terrain_server_relevant_region_precedes(
    henka_terrain_region_id center,
    const henka_terrain_session_region* left,
    const henka_terrain_session_region* right)
{
    const uint64_t left_distance = henka_terrain_server_region_distance_squared(center, left->region_id);
    const uint64_t right_distance = henka_terrain_server_region_distance_squared(center, right->region_id);
    return left_distance < right_distance ||
        (left_distance == right_distance &&
            henka_terrain_server_region_precedes(left->region_id, right->region_id));
}

static void henka_terrain_server_sort_session_regions(
    henka_terrain_session_info* info)
{
    uint32_t index;
    if (info == NULL)
    {
        return;
    }
    for (index = 1U; index < info->region_count; ++index)
    {
        henka_terrain_session_region candidate = info->regions[index];
        uint32_t insertion_index = index;
        while (insertion_index > 0U &&
               henka_terrain_server_region_precedes(
                   candidate.region_id,
                   info->regions[insertion_index - 1U].region_id))
        {
            info->regions[insertion_index] = info->regions[insertion_index - 1U];
            --insertion_index;
        }
        info->regions[insertion_index] = candidate;
    }
}

static henka_result henka_terrain_server_send_session_info(
    henka_terrain_server* server,
    henka_network_peer_id peer_id,
    const henka_terrain_session_request* request)
{
    henka_terrain_world_desc world_desc;
    henka_terrain_world_stats stats;
    henka_terrain_session_info info = {0};
    uint8_t payload[512U];
    size_t payload_size;
    uint32_t index;
    uint32_t max_regions;
    if (henka_terrain_world_get_desc(server->world, &world_desc) != HENKA_SUCCESS ||
        henka_terrain_world_get_stats(server->world, &stats) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    info.world_identity = world_desc.world_identity;
    info.base_asset_identity = world_desc.base_asset_identity;
    max_regions = request != NULL ? request->max_regions : HENKA_TERRAIN_NETWORK_MAX_SESSION_REGIONS;
    for (index = 0U; index < stats.resident_region_count; ++index)
    {
        henka_terrain_region_state state;
        if (henka_terrain_world_get_resident_region_at(server->world, index, &state) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_UNKNOWN;
        }
        if (request != NULL)
        {
            const int64_t dx = (int64_t)state.id.x - (int64_t)request->center_region.x;
            const int64_t dz = (int64_t)state.id.z - (int64_t)request->center_region.z;
            if ((uint64_t)(dx < 0 ? -dx : dx) > request->radius_regions ||
                (uint64_t)(dz < 0 ? -dz : dz) > request->radius_regions)
            {
                continue;
            }
        }
        {
            henka_terrain_session_region candidate = {
                state.id, state.revision, state.generation};
            uint32_t insertion_index = info.region_count;
            if (request != NULL)
            {
                while (insertion_index > 0U &&
                       henka_terrain_server_relevant_region_precedes(
                           request->center_region,
                           &candidate,
                           &info.regions[insertion_index - 1U]))
                {
                    --insertion_index;
                }
            }
            if (insertion_index < max_regions)
            {
                uint32_t move_index = info.region_count;
                if (move_index >= max_regions)
                {
                    move_index = max_regions - 1U;
                }
                while (move_index > insertion_index)
                {
                    info.regions[move_index] = info.regions[move_index - 1U];
                    --move_index;
                }
                info.regions[insertion_index] = candidate;
                if (info.region_count < max_regions)
                {
                    ++info.region_count;
                }
            }
        }
    }
    if (request != NULL)
    {
        info.flags = HENKA_TERRAIN_SESSION_INFO_FLAG_RELEVANCE_FILTERED;
        server->diagnostics.session_interest_region_count += info.region_count;
    }
    else
    {
        /* A capped legacy manifest must remain deterministic after residency slot reuse. */
        henka_terrain_server_sort_session_regions(&info);
    }
    if (henka_terrain_session_info_encode(
            &info, payload, sizeof(payload), &payload_size) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_LIMIT;
    }
    return henka_network_server_send(
        server->network, peer_id, HENKA_NETWORK_CHANNEL_CONTROL,
        HENKA_NETWORK_MESSAGE_CONNECT, payload, payload_size);
}

static void henka_terrain_server_send_snapshot_failure(
    henka_terrain_server* server,
    henka_network_peer_id peer_id,
    const henka_terrain_snapshot_request* request,
    henka_terrain_snapshot_failure_reason reason);

static henka_result henka_terrain_server_send_snapshot(
    henka_terrain_server* server,
    henka_network_peer_id peer_id,
    const henka_terrain_snapshot_request* request)
{
    henka_terrain_world_desc world_desc;
    henka_terrain_layout layout;
    henka_terrain_region_storage_info info;
    const henka_terrain_sample* source_samples = NULL;
    uint8_t* record = NULL;
    uint8_t payload[HENKA_NETWORK_MAX_SNAPSHOT_FRAGMENT_PAYLOAD];
    size_t record_size = 0U;
    size_t source_sample_count = 0U;
    size_t offset;
    uint32_t fragment_count;
    uint32_t fragment_index;
    uint64_t transfer_id;
    bool send_started = false;
    henka_result result = HENKA_SUCCESS;

    if (henka_terrain_world_get_desc(server->world, &world_desc) != HENKA_SUCCESS ||
        request->world_identity != world_desc.world_identity ||
        request->base_asset_identity != world_desc.base_asset_identity ||
        henka_terrain_world_desc_get_layout(&world_desc, &layout) != HENKA_SUCCESS)
    {
        ++server->diagnostics.snapshot_failure_count;
        henka_terrain_server_send_snapshot_failure(
            server, peer_id, request, HENKA_TERRAIN_SNAPSHOT_FAILURE_INVALID);
        (void)henka_network_server_disconnect(
            server->network, peer_id, HENKA_NETWORK_DISCONNECT_REASON_PROTOCOL);
        return HENKA_SUCCESS;
    }
    if (henka_terrain_world_get_region_state(
            server->world, request->region_id, &(henka_terrain_region_state){0}) != HENKA_SUCCESS)
    {
        henka_terrain_sample* samples = NULL;
        samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
        if (samples == NULL ||
            henka_terrain_storage_load_region(
                server->storage, request->region_id, &info,
                samples, layout.samples_per_region) != HENKA_SUCCESS ||
            henka_terrain_world_apply_region_snapshot(
                server->world, info, samples, layout.samples_per_region) != HENKA_SUCCESS)
        {
            henka_free(samples);
            ++server->diagnostics.snapshot_failure_count;
            henka_terrain_server_send_snapshot_failure(
                server, peer_id, request, HENKA_TERRAIN_SNAPSHOT_FAILURE_STORAGE);
            return HENKA_SUCCESS;
        }
        henka_free(samples);
    }
    record = henka_malloc(HENKA_TERRAIN_MAX_REGION_RECORD_BYTES);
    if (record == NULL)
    {
        result = HENKA_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    if (henka_terrain_world_get_region_state(
            server->world, request->region_id, &(henka_terrain_region_state){0}) != HENKA_SUCCESS ||
        henka_terrain_world_get_region_samples(
            server->world, request->region_id, &source_samples, &source_sample_count) != HENKA_SUCCESS)
    {
        result = HENKA_ERROR_INVALID_ARGUMENT;
        goto cleanup;
    }
    {
        henka_terrain_region_state state;
        if (henka_terrain_world_get_region_state(
                server->world, request->region_id, &state) != HENKA_SUCCESS)
        {
            result = HENKA_ERROR_INVALID_ARGUMENT;
            goto cleanup;
        }
        info = (henka_terrain_region_storage_info){state.id, state.revision, state.generation};
    }
    result = henka_terrain_region_encode(
        &world_desc, request->region_id, info.revision, info.generation,
        source_samples, source_sample_count, record,
        HENKA_TERRAIN_MAX_REGION_RECORD_BYTES, &record_size);
    if (result != HENKA_SUCCESS || record_size == 0U)
    {
        goto cleanup;
    }
    fragment_count = (uint32_t)((record_size + HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES - 1U) /
        HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES);
    if (fragment_count == 0U || fragment_count > HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENTS ||
        record_size > UINT32_MAX || server->next_snapshot_transfer_id == 0U)
    {
        result = HENKA_ERROR_LIMIT;
        goto cleanup;
    }
    transfer_id = server->next_snapshot_transfer_id++;
    for (fragment_index = 0U, offset = 0U; fragment_index < fragment_count; ++fragment_index)
    {
        henka_terrain_snapshot_fragment fragment = {0};
        size_t remaining = record_size - offset;
        size_t data_size = remaining > HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES
            ? HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FRAGMENT_DATA_BYTES : remaining;
        size_t payload_size;
        fragment.world_identity = world_desc.world_identity;
        fragment.base_asset_identity = world_desc.base_asset_identity;
        fragment.transfer_id = transfer_id;
        fragment.region_id = request->region_id;
        fragment.revision = info.revision;
        fragment.generation = info.generation;
        fragment.fragment_index = fragment_index;
        fragment.fragment_count = fragment_count;
        fragment.total_bytes = (uint32_t)record_size;
        fragment.data_size = (uint32_t)data_size;
        fragment.data = record + offset;
        result = henka_terrain_snapshot_fragment_encode(
            &fragment, payload, sizeof(payload), &payload_size);
        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        send_started = true;
        result = henka_network_server_send(
            server->network, peer_id, HENKA_NETWORK_CHANNEL_SNAPSHOT,
            HENKA_NETWORK_MESSAGE_SNAPSHOT_FRAGMENT, payload, payload_size);
        if (result != HENKA_SUCCESS)
        {
            goto cleanup;
        }
        ++server->diagnostics.snapshot_fragment_count;
        offset += data_size;
    }

cleanup:
    henka_free(record);
    if (result != HENKA_SUCCESS)
    {
        ++server->diagnostics.snapshot_failure_count;
        if (send_started)
        {
            /* A stream with a missing middle fragment cannot be completed or
             * safely resumed by the receiver. Retire the transport so the
             * client clears the partial replica state and reconnects for a
             * fresh authoritative session. */
            (void)henka_network_server_disconnect(
                server->network, peer_id, HENKA_NETWORK_DISCONNECT_REASON_PROTOCOL);
        }
        else
        {
            henka_terrain_snapshot_failure_reason reason =
                result == HENKA_ERROR_OUT_OF_MEMORY
                    ? HENKA_TERRAIN_SNAPSHOT_FAILURE_OUT_OF_MEMORY
                    : result == HENKA_ERROR_LIMIT
                    ? HENKA_TERRAIN_SNAPSHOT_FAILURE_LIMIT
                    : HENKA_TERRAIN_SNAPSHOT_FAILURE_INVALID;
            henka_terrain_server_send_snapshot_failure(server, peer_id, request, reason);
        }
    }
    /* A failed snapshot request is reported on the wire or by the protocol
     * disconnect above; it must not tear down the terrain server poll loop. */
    return HENKA_SUCCESS;
}

static void henka_terrain_server_send_snapshot_failure(
    henka_terrain_server* server,
    henka_network_peer_id peer_id,
    const henka_terrain_snapshot_request* request,
    henka_terrain_snapshot_failure_reason reason)
{
    uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_FAILURE_BYTES];
    size_t payload_size;
    henka_terrain_snapshot_failure failure = {
        request->world_identity, request->base_asset_identity,
        request->region_id, request->expected_revision, reason};
    if (henka_terrain_snapshot_failure_encode(
            &failure, payload, sizeof(payload), &payload_size) == HENKA_SUCCESS)
    {
        (void)henka_network_server_send(
            server->network, peer_id, HENKA_NETWORK_CHANNEL_SNAPSHOT,
            HENKA_NETWORK_MESSAGE_SNAPSHOT_FAILED, payload, payload_size);
    }
}

static bool henka_terrain_server_delta_region_revision(
    const henka_terrain_edit_delta* delta,
    henka_terrain_region_id region_id,
    henka_terrain_revision* out_revision)
{
    uint32_t index;
    for (index = 0U; index < delta->affected_region_count; ++index)
    {
        if (henka_terrain_region_id_equal(delta->affected_regions[index].region_id, region_id))
        {
            if (out_revision != NULL)
            {
                *out_revision = delta->affected_regions[index].revision;
            }
            return true;
        }
    }
    return false;
}

static void henka_terrain_server_store_delta(
    henka_terrain_server* server,
    const henka_terrain_edit_delta* delta)
{
    server->delta_history[server->delta_history_next] = *delta;
    server->delta_history_next =
        (server->delta_history_next + 1U) % HENKA_TERRAIN_SERVER_MAX_DELTA_HISTORY;
    if (server->delta_history_count < HENKA_TERRAIN_SERVER_MAX_DELTA_HISTORY)
    {
        ++server->delta_history_count;
    }
}

static henka_result henka_terrain_server_send_delta(
    henka_terrain_server* server,
    henka_network_peer_id peer_id,
    const henka_terrain_edit_delta* delta)
{
    uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_DELTA_BYTES];
    size_t payload_size;
    if (henka_terrain_edit_delta_encode(
            delta, payload, sizeof(payload), &payload_size) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_network_server_send(
        server->network, peer_id, HENKA_NETWORK_CHANNEL_TERRAIN,
        HENKA_NETWORK_MESSAGE_TERRAIN_DELTA, payload, payload_size);
}

static henka_result henka_terrain_server_recover_deltas(
    henka_terrain_server* server,
    henka_network_peer_id peer_id,
    const henka_terrain_delta_recovery_request* request)
{
    henka_terrain_revision expected = request->from_revision;
    uint32_t index;
    for (index = 0U; index < server->delta_history_count && expected <= request->target_revision; ++index)
    {
        uint32_t history_index =
            (server->delta_history_next + HENKA_TERRAIN_SERVER_MAX_DELTA_HISTORY -
                server->delta_history_count + index) % HENKA_TERRAIN_SERVER_MAX_DELTA_HISTORY;
        const henka_terrain_edit_delta* delta = &server->delta_history[history_index];
        henka_terrain_revision revision;
        if (!henka_terrain_server_delta_region_revision(delta, request->region_id, &revision))
        {
            continue;
        }
        if (revision < expected)
        {
            continue;
        }
        if (revision != expected || henka_terrain_server_send_delta(server, peer_id, delta) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_ASSET_SOURCE;
        }
        ++server->diagnostics.delta_recovery_sent_count;
        ++expected;
    }
    return expected > request->target_revision ? HENKA_SUCCESS : HENKA_ERROR_ASSET_SOURCE;
}

static henka_result henka_terrain_server_handle_recovery_request(
    henka_terrain_server* server,
    henka_network_peer_id peer_id,
    const henka_terrain_delta_recovery_request* request)
{
    henka_terrain_world_desc desc;
    henka_terrain_region_state state;
    henka_terrain_snapshot_request snapshot_request;
    if (henka_terrain_world_get_desc(server->world, &desc) != HENKA_SUCCESS ||
        request->world_identity != desc.world_identity ||
        request->base_asset_identity != desc.base_asset_identity ||
        henka_terrain_world_get_region_state(server->world, request->region_id, &state) != HENKA_SUCCESS)
    {
        ++server->diagnostics.snapshot_failure_count;
        (void)henka_network_server_disconnect(
            server->network, peer_id, HENKA_NETWORK_DISCONNECT_REASON_PROTOCOL);
        return HENKA_SUCCESS;
    }
    if (state.revision >= request->target_revision &&
        henka_terrain_server_recover_deltas(server, peer_id, request) == HENKA_SUCCESS)
    {
        return HENKA_SUCCESS;
    }
    snapshot_request = (henka_terrain_snapshot_request){
        request->world_identity,
        request->base_asset_identity,
        request->region_id,
        request->from_revision - 1U};
    ++server->diagnostics.delta_recovery_snapshot_fallback_count;
    return henka_terrain_server_send_snapshot(server, peer_id, &snapshot_request);
}

static bool henka_terrain_server_materialize_request_regions(
    henka_terrain_server* server,
    const henka_terrain_edit_request* request,
    henka_terrain_region_id materialized[HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS],
    uint32_t* out_materialized_count)
{
    henka_terrain_world_desc desc;
    henka_terrain_layout layout;
    henka_terrain_region_id regions[HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS];
    uint32_t region_count = HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS;
    uint32_t materialized_count = 0U;
    uint32_t index;
    if (server == NULL || request == NULL || materialized == NULL || out_materialized_count == NULL)
    {
        return false;
    }
    if (henka_terrain_world_get_desc(server->world, &desc) != HENKA_SUCCESS ||
        henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
        henka_terrain_edit_get_affected_regions(
            server->world, &request->command, regions, &region_count) != HENKA_SUCCESS)
    {
        *out_materialized_count = 0U;
        return false;
    }
    for (index = 0U; index < region_count; ++index)
    {
        henka_terrain_region_state state;
        henka_terrain_region_storage_info info;
        henka_terrain_sample* samples;
        if (henka_terrain_world_get_region_state(server->world, regions[index], &state) == HENKA_SUCCESS)
        {
            continue;
        }
        samples = henka_calloc(layout.samples_per_region, sizeof(*samples));
        if (samples == NULL)
        {
            ++server->diagnostics.materialization_failure_count;
            goto rollback;
        }
        if (henka_terrain_storage_load_region(
                server->storage, regions[index], &info, samples, layout.samples_per_region) == HENKA_SUCCESS)
        {
            if (henka_terrain_world_apply_region_snapshot(
                    server->world, info, samples, layout.samples_per_region) != HENKA_SUCCESS)
            {
                ++server->diagnostics.materialization_failure_count;
                henka_free(samples);
                goto rollback;
            }
            materialized[materialized_count++] = regions[index];
        }
        else
        {
            ++server->diagnostics.materialization_failure_count;
            henka_free(samples);
            goto rollback;
        }
        henka_free(samples);
    }
    *out_materialized_count = materialized_count;
    return true;

rollback:
    while (materialized_count > 0U)
    {
        --materialized_count;
        (void)henka_terrain_world_release_region(server->world, materialized[materialized_count]);
    }
    *out_materialized_count = 0U;
    return false;
}

static void henka_terrain_server_release_request_materialization(
    henka_terrain_server* server,
    const henka_terrain_region_id materialized[HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS],
    uint32_t materialized_count)
{
    while (materialized_count > 0U)
    {
        --materialized_count;
        (void)henka_terrain_world_release_region(server->world, materialized[materialized_count]);
    }
}

henka_result henka_terrain_server_handle_event(
    henka_terrain_server* server,
    const henka_network_event* event,
    uint64_t now_milliseconds)
{
    henka_terrain_edit_request request;
    henka_terrain_authority_response response;
    henka_terrain_region_id request_materialized[HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS];
    uint32_t request_materialized_count = 0U;
    uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_EDIT_RESPONSE_BYTES];
    size_t payload_size;
    henka_result result;

    if (server == NULL || event == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (event->type == HENKA_NETWORK_EVENT_CONNECTED)
    {
        return henka_terrain_server_send_session_info(server, event->peer_id, NULL);
    }
    if (event->type == HENKA_NETWORK_EVENT_DISCONNECTED)
    {
        henka_terrain_authority_retire_peer(server->authority, event->peer_id);
        return HENKA_SUCCESS;
    }
    if (event->type != HENKA_NETWORK_EVENT_MESSAGE)
    {
        return HENKA_SUCCESS;
    }
    if (event->message.channel == HENKA_NETWORK_CHANNEL_CONTROL &&
        event->message.type == HENKA_NETWORK_MESSAGE_PING)
    {
        return henka_network_server_send(
            server->network, event->peer_id, event->message.channel,
            HENKA_NETWORK_MESSAGE_PING, event->message.payload,
            event->message.payload_size);
    }
    if (event->message.channel == HENKA_NETWORK_CHANNEL_CONTROL &&
        event->message.type == HENKA_NETWORK_MESSAGE_TERRAIN_SESSION_REQUEST)
    {
        henka_terrain_session_request session_request;
        henka_terrain_world_desc world_desc;
        ++server->diagnostics.session_interest_request_count;
        if (henka_terrain_session_request_decode(
                event->message.payload, event->message.payload_size, &session_request) != HENKA_SUCCESS ||
            henka_terrain_world_get_desc(server->world, &world_desc) != HENKA_SUCCESS ||
            session_request.world_identity != world_desc.world_identity ||
            session_request.base_asset_identity != world_desc.base_asset_identity)
        {
            ++server->diagnostics.session_interest_failure_count;
            ++server->diagnostics.protocol_disconnect_count;
            (void)henka_network_server_disconnect(
                server->network, event->peer_id, HENKA_NETWORK_DISCONNECT_REASON_PROTOCOL);
            return HENKA_SUCCESS;
        }
        return henka_terrain_server_send_session_info(
            server, event->peer_id, &session_request);
    }
    if (event->message.channel == HENKA_NETWORK_CHANNEL_SNAPSHOT &&
        event->message.type == HENKA_NETWORK_MESSAGE_SNAPSHOT_REQUEST)
    {
        henka_terrain_snapshot_request snapshot_request;
        ++server->diagnostics.snapshot_request_count;
        if (henka_terrain_snapshot_request_decode(
                event->message.payload, event->message.payload_size, &snapshot_request) != HENKA_SUCCESS)
        {
            ++server->diagnostics.snapshot_failure_count;
            (void)henka_network_server_disconnect(
                server->network, event->peer_id, HENKA_NETWORK_DISCONNECT_REASON_PROTOCOL);
            return HENKA_SUCCESS;
        }
        return henka_terrain_server_send_snapshot(server, event->peer_id, &snapshot_request);
    }
    if (event->message.channel == HENKA_NETWORK_CHANNEL_TERRAIN &&
        event->message.type == HENKA_NETWORK_MESSAGE_TERRAIN_RECOVERY_REQUEST)
    {
        henka_terrain_delta_recovery_request recovery_request;
        ++server->diagnostics.delta_recovery_request_count;
        if (henka_terrain_delta_recovery_request_decode(
                event->message.payload, event->message.payload_size, &recovery_request) != HENKA_SUCCESS)
        {
            ++server->diagnostics.protocol_disconnect_count;
            (void)henka_network_server_disconnect(
                server->network, event->peer_id, HENKA_NETWORK_DISCONNECT_REASON_PROTOCOL);
            return HENKA_SUCCESS;
        }
        return henka_terrain_server_handle_recovery_request(
            server, event->peer_id, &recovery_request);
    }
    if (event->message.channel != HENKA_NETWORK_CHANNEL_TERRAIN ||
        event->message.type != HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REQUEST)
    {
        return HENKA_SUCCESS;
    }
    ++server->diagnostics.processed_edit_request_count;
    result = henka_terrain_edit_request_decode(
        event->message.payload, event->message.payload_size, &request);
    if (result != HENKA_SUCCESS)
    {
        ++server->diagnostics.malformed_edit_count;
        ++server->diagnostics.protocol_disconnect_count;
        (void)henka_network_server_disconnect(
            server->network, event->peer_id, HENKA_NETWORK_DISCONNECT_REASON_PROTOCOL);
        return HENKA_SUCCESS;
    }
    if (!henka_terrain_server_materialize_request_regions(
            server, &request, request_materialized, &request_materialized_count))
    {
        ++server->diagnostics.rejected_edit_count;
        return henka_terrain_server_send_rejection(
            server, event->peer_id,
            &(henka_terrain_edit_rejection){request.client_nonce, HENKA_TERRAIN_EDIT_REJECT_INVALID});
    }
    result = henka_terrain_authority_process_request(
        server->authority, event->peer_id, &request, now_milliseconds, &response);
    if (result != HENKA_SUCCESS)
    {
        henka_terrain_server_release_request_materialization(
            server, request_materialized, request_materialized_count);
        return result;
    }
    if (!response.accepted)
    {
        henka_terrain_server_release_request_materialization(
            server, request_materialized, request_materialized_count);
        ++server->diagnostics.rejected_edit_count;
        return henka_terrain_server_send_rejection(server, event->peer_id, &response.rejection);
    }
    ++server->diagnostics.accepted_edit_count;
    if (server->collision_runtime != NULL)
    {
        (void)henka_terrain_collision_runtime_request_edit(
            server->collision_runtime, &request.command);
    }
    result = henka_terrain_edit_acceptance_encode(
        &response.acceptance, payload, sizeof(payload), &payload_size);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    result = henka_network_server_send(
        server->network, event->peer_id, HENKA_NETWORK_CHANNEL_TERRAIN,
        HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_ACCEPTED, payload, payload_size);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    {
        henka_terrain_edit_delta delta = {0};
        delta.world_identity = request.world_identity;
        delta.base_asset_identity = request.base_asset_identity;
        delta.client_nonce = request.client_nonce;
        delta.server_command_id = response.acceptance.server_command_id;
        delta.command = request.command;
        delta.affected_region_count = response.acceptance.affected_region_count;
        memcpy(
            delta.affected_regions, response.acceptance.affected_regions,
            (size_t)delta.affected_region_count * sizeof(delta.affected_regions[0]));
        result = henka_terrain_edit_delta_encode(
            &delta, payload, sizeof(payload), &payload_size);
        if (result == HENKA_SUCCESS)
        {
            henka_terrain_server_store_delta(server, &delta);
            result = henka_network_server_broadcast(
                server->network, HENKA_NETWORK_CHANNEL_TERRAIN,
                HENKA_NETWORK_MESSAGE_TERRAIN_DELTA, payload, payload_size);
        }
    }
    return result;
}

henka_result henka_terrain_server_poll(
    henka_terrain_server* server,
    uint32_t timeout_milliseconds,
    uint64_t now_milliseconds)
{
    henka_network_event event;
    henka_result result;
    if (server == NULL || timeout_milliseconds > HENKA_NETWORK_MAX_POLL_TIMEOUT_MILLISECONDS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_network_server_poll(server->network, timeout_milliseconds, &event);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    return henka_terrain_server_handle_event(server, &event, now_milliseconds);
}

void henka_terrain_server_get_diagnostics(
    const henka_terrain_server* server,
    henka_terrain_server_diagnostics* out_diagnostics)
{
    if (out_diagnostics == NULL)
    {
        return;
    }
    *out_diagnostics = server == NULL
        ? (henka_terrain_server_diagnostics){0}
        : server->diagnostics;
}
