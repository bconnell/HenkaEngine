#include <henka/terrain_client.h>

#include <string.h>

#include <henka/memory.h>

#define HENKA_TERRAIN_CLIENT_MAX_SNAPSHOT_RETRIES 4U
#define HENKA_TERRAIN_CLIENT_MAX_PENDING_RECOVERIES HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS

typedef struct henka_terrain_client_pending_recovery
{
    bool active;
    henka_terrain_region_id region_id;
    henka_terrain_revision target_revision;
} henka_terrain_client_pending_recovery;

typedef struct henka_terrain_client_pending_snapshot
{
    bool active;
    henka_terrain_region_id region_id;
    henka_terrain_revision target_revision;
    henka_terrain_generation target_generation;
} henka_terrain_client_pending_snapshot;

struct henka_terrain_client
{
    henka_network_client* network;
    henka_terrain_world* world;
    henka_terrain_replica* replica;
    henka_terrain_prediction* prediction;
    henka_terrain_client_diagnostics diagnostics;
    uint64_t last_snapshot_retry_transfer_id;
    uint32_t snapshot_retry_count;
    henka_terrain_client_pending_recovery pending_recoveries[
        HENKA_TERRAIN_CLIENT_MAX_PENDING_RECOVERIES];
    henka_terrain_client_pending_snapshot pending_session_snapshots[
        HENKA_TERRAIN_NETWORK_MAX_SESSION_REGIONS];
    bool session_interest_enabled;
    henka_terrain_region_id session_center_region;
    uint32_t session_radius_regions;
    uint32_t session_max_regions;
    bool session_interest_pending;
};

static uint32_t henka_terrain_client_find_pending_recovery(
    const henka_terrain_client* client,
    henka_terrain_region_id region_id)
{
    uint32_t index;
    for (index = 0U; index < HENKA_TERRAIN_CLIENT_MAX_PENDING_RECOVERIES; ++index)
    {
        if (client->pending_recoveries[index].active &&
            henka_terrain_region_id_equal(
                client->pending_recoveries[index].region_id, region_id))
        {
            return index;
        }
    }
    return HENKA_TERRAIN_CLIENT_MAX_PENDING_RECOVERIES;
}

static void henka_terrain_client_clear_pending_recovery(
    henka_terrain_client* client,
    henka_terrain_region_id region_id,
    henka_terrain_revision revision)
{
    uint32_t index = henka_terrain_client_find_pending_recovery(client, region_id);
    if (index < HENKA_TERRAIN_CLIENT_MAX_PENDING_RECOVERIES &&
        revision >= client->pending_recoveries[index].target_revision)
    {
        client->pending_recoveries[index].active = false;
        if (client->diagnostics.pending_recovery_count > 0U)
        {
            --client->diagnostics.pending_recovery_count;
        }
    }
}

static void henka_terrain_client_clear_all_pending_recoveries(
    henka_terrain_client* client)
{
    memset(client->pending_recoveries, 0, sizeof(client->pending_recoveries));
    client->diagnostics.pending_recovery_count = 0U;
}

static uint32_t henka_terrain_client_find_pending_session_snapshot(
    const henka_terrain_client* client,
    henka_terrain_region_id region_id)
{
    uint32_t index;
    for (index = 0U; index < HENKA_TERRAIN_NETWORK_MAX_SESSION_REGIONS; ++index)
    {
        if (client->pending_session_snapshots[index].active &&
            henka_terrain_region_id_equal(
                client->pending_session_snapshots[index].region_id, region_id))
        {
            return index;
        }
    }
    return HENKA_TERRAIN_NETWORK_MAX_SESSION_REGIONS;
}

static void henka_terrain_client_clear_session_snapshot(
    henka_terrain_client* client,
    henka_terrain_region_id region_id,
    henka_terrain_revision revision)
{
    uint32_t index = henka_terrain_client_find_pending_session_snapshot(client, region_id);
    if (index < HENKA_TERRAIN_NETWORK_MAX_SESSION_REGIONS &&
        revision >= client->pending_session_snapshots[index].target_revision)
    {
        client->pending_session_snapshots[index].active = false;
    }
}

static void henka_terrain_client_clear_all_pending_session_snapshots(
    henka_terrain_client* client)
{
    memset(client->pending_session_snapshots, 0, sizeof(client->pending_session_snapshots));
}

static void henka_terrain_client_clear_failed_snapshot_state(
    henka_terrain_client* client,
    henka_terrain_region_id region_id)
{
    uint32_t index = henka_terrain_client_find_pending_recovery(client, region_id);
    if (index < HENKA_TERRAIN_CLIENT_MAX_PENDING_RECOVERIES)
    {
        client->pending_recoveries[index].active = false;
        if (client->diagnostics.pending_recovery_count > 0U)
        {
            --client->diagnostics.pending_recovery_count;
        }
    }
    index = henka_terrain_client_find_pending_session_snapshot(client, region_id);
    if (index < HENKA_TERRAIN_NETWORK_MAX_SESSION_REGIONS)
    {
        client->pending_session_snapshots[index].active = false;
    }
}

henka_terrain_client_desc henka_terrain_client_desc_default(void)
{
    return (henka_terrain_client_desc){
        NULL,
        NULL,
        HENKA_TERRAIN_MAX_REGION_RECORD_BYTES,
        16U,
        false,
        {0, 0},
        0U,
        HENKA_TERRAIN_NETWORK_MAX_SESSION_REGIONS};
}

henka_result henka_terrain_client_create(
    const henka_terrain_client_desc* desc,
    henka_terrain_client** out_client)
{
    henka_terrain_client* client;
    henka_terrain_replica_desc replica_desc;
    if (out_client == NULL || desc == NULL || desc->network == NULL || desc->world == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_client = NULL;
    client = henka_calloc(1U, sizeof(*client));
    if (client == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    replica_desc = henka_terrain_replica_desc_default();
    replica_desc.world = desc->world;
    if (desc->max_snapshot_bytes != 0U)
    {
        replica_desc.max_snapshot_bytes = desc->max_snapshot_bytes;
    }
    if (henka_terrain_replica_create(&replica_desc, &client->replica) != HENKA_SUCCESS)
    {
        henka_free(client);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    {
        henka_terrain_prediction_desc prediction_desc = henka_terrain_prediction_desc_default();
        prediction_desc.authoritative_world = desc->world;
        if (desc->max_pending_prediction_commands != 0U)
        {
            prediction_desc.max_pending_commands = desc->max_pending_prediction_commands;
        }
        if (henka_terrain_prediction_create(&prediction_desc, &client->prediction) != HENKA_SUCCESS)
        {
            henka_terrain_replica_destroy(client->replica);
            henka_free(client);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
    }
    client->network = desc->network;
    client->world = desc->world;
    client->session_interest_enabled = desc->session_interest_enabled;
    client->session_center_region = desc->session_center_region;
    client->session_radius_regions = desc->session_radius_regions;
    client->session_max_regions = desc->session_max_regions != 0U
        ? desc->session_max_regions : HENKA_TERRAIN_NETWORK_MAX_SESSION_REGIONS;
    client->session_interest_pending = false;
    *out_client = client;
    return HENKA_SUCCESS;
}

void henka_terrain_client_destroy(henka_terrain_client* client)
{
    if (client == NULL)
    {
        return;
    }
    henka_terrain_replica_destroy(client->replica);
    henka_terrain_prediction_destroy(client->prediction);
    henka_free(client);
}

henka_result henka_terrain_client_send_edit_request(
    henka_terrain_client* client,
    const henka_terrain_edit_request* request)
{
    uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_EDIT_REQUEST_BYTES];
    size_t payload_size;
    henka_terrain_edit_command prediction_command;
    if (client == NULL || request == NULL ||
        henka_terrain_edit_request_encode(request, payload, sizeof(payload), &payload_size) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    prediction_command = request->command;
    prediction_command.client_nonce = request->client_nonce;
    if (henka_terrain_prediction_submit(client->prediction, &prediction_command) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_LIMIT;
    }
    {
        henka_result send_result = henka_network_client_send(
            client->network, HENKA_NETWORK_CHANNEL_TERRAIN,
            HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REQUEST, payload, payload_size);
        if (send_result != HENKA_SUCCESS)
        {
            (void)henka_terrain_prediction_reject(
                client->prediction, prediction_command.client_nonce);
        }
        return send_result;
    }
}

henka_result henka_terrain_client_reconnect(henka_terrain_client* client)
{
    henka_result result;
    if (client == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_network_client_reconnect(client->network);
    if (result == HENKA_SUCCESS)
    {
        /* Requests queued for the retired transport can no longer describe
         * the new connection's authoritative stream. The reconnect event will
         * bootstrap fresh session state and any required snapshots. */
        henka_terrain_client_clear_all_pending_recoveries(client);
        henka_terrain_client_clear_all_pending_session_snapshots(client);
    }
    return result;
}

henka_result henka_terrain_client_request_session_interest(
    henka_terrain_client* client)
{
    henka_terrain_world_desc desc;
    henka_terrain_session_request request;
    uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_SESSION_REQUEST_BYTES];
    size_t payload_size;
    if (client == NULL || !client->session_interest_enabled ||
        henka_terrain_world_get_desc(client->world, &desc) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    request = (henka_terrain_session_request){
        desc.world_identity,
        desc.base_asset_identity,
        client->session_center_region,
        client->session_radius_regions,
        client->session_max_regions};
    if (henka_terrain_session_request_encode(
            &request, payload, sizeof(payload), &payload_size) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (henka_network_client_send(
            client->network,
            HENKA_NETWORK_CHANNEL_CONTROL,
            HENKA_NETWORK_MESSAGE_TERRAIN_SESSION_REQUEST,
            payload,
            payload_size) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_PLATFORM;
    }
    client->session_interest_pending = true;
    ++client->diagnostics.session_interest_request_count;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_client_request_snapshot(
    henka_terrain_client* client,
    henka_terrain_snapshot_request request)
{
    uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_SNAPSHOT_REQUEST_BYTES];
    size_t payload_size;
    if (client == NULL ||
        henka_terrain_snapshot_request_encode(
            &request, payload, sizeof(payload), &payload_size) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return henka_network_client_send(
        client->network, HENKA_NETWORK_CHANNEL_SNAPSHOT,
        HENKA_NETWORK_MESSAGE_SNAPSHOT_REQUEST, payload, payload_size);
}

static henka_result henka_terrain_client_retry_snapshot(
    henka_terrain_client* client,
    const henka_terrain_snapshot_fragment* fragment,
    bool* out_requested)
{
    henka_terrain_world_desc desc;
    henka_terrain_snapshot_request request;

    if (out_requested == NULL || client == NULL || fragment == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_requested = false;
    if (client->snapshot_retry_count >= HENKA_TERRAIN_CLIENT_MAX_SNAPSHOT_RETRIES ||
        fragment->transfer_id == 0U ||
        fragment->transfer_id == client->last_snapshot_retry_transfer_id ||
        fragment->revision == 0U ||
        henka_terrain_world_get_desc(client->world, &desc) != HENKA_SUCCESS ||
        desc.world_identity != fragment->world_identity ||
        desc.base_asset_identity != fragment->base_asset_identity)
    {
        return HENKA_SUCCESS;
    }
    request = (henka_terrain_snapshot_request){
        desc.world_identity,
        desc.base_asset_identity,
        fragment->region_id,
        fragment->revision};
    if (henka_terrain_client_request_snapshot(client, request) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_PLATFORM;
    }
    client->last_snapshot_retry_transfer_id = fragment->transfer_id;
    ++client->snapshot_retry_count;
    ++client->diagnostics.recovery_snapshot_request_count;
    *out_requested = true;
    return HENKA_SUCCESS;
}

static henka_result henka_terrain_client_request_delta_recovery(
    henka_terrain_client* client,
    const henka_terrain_edit_delta* delta)
{
    henka_terrain_world_desc desc;
    henka_terrain_region_id affected[HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS];
    uint32_t affected_count = HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS;
    uint32_t index;
    henka_result result = henka_terrain_world_get_desc(client->world, &desc);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (desc.world_identity != delta->world_identity ||
        desc.base_asset_identity != delta->base_asset_identity)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_terrain_edit_get_affected_regions(
        client->world, &delta->command, affected, &affected_count);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (affected_count != delta->affected_region_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < delta->affected_region_count; ++index)
    {
        henka_terrain_region_state state;
        if (!henka_terrain_region_id_equal(
                affected[index], delta->affected_regions[index].region_id))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        uint32_t pending_index;
        bool replacing_pending = false;
        henka_terrain_delta_recovery_request request = {
            delta->world_identity,
            delta->base_asset_identity,
            delta->affected_regions[index].region_id,
            1U,
            delta->affected_regions[index].revision};
        if (henka_terrain_world_get_region_state(
                client->world, request.region_id, &state) == HENKA_SUCCESS)
        {
            if (state.revision >= request.target_revision)
            {
                henka_terrain_client_clear_pending_recovery(
                    client, request.region_id, state.revision);
                continue;
            }
            if (state.revision == UINT64_MAX)
            {
                return HENKA_ERROR_LIMIT;
            }
            request.from_revision = state.revision + 1U;
        }
        pending_index = henka_terrain_client_find_pending_recovery(
            client, request.region_id);
        if (pending_index < HENKA_TERRAIN_CLIENT_MAX_PENDING_RECOVERIES &&
            request.target_revision <= client->pending_recoveries[pending_index].target_revision)
        {
            ++client->diagnostics.recovery_delta_suppressed_count;
            continue;
        }
        if (pending_index < HENKA_TERRAIN_CLIENT_MAX_PENDING_RECOVERIES)
        {
            replacing_pending = true;
        }
        if (pending_index >= HENKA_TERRAIN_CLIENT_MAX_PENDING_RECOVERIES)
        {
            for (pending_index = 0U;
                 pending_index < HENKA_TERRAIN_CLIENT_MAX_PENDING_RECOVERIES;
                 ++pending_index)
            {
                if (!client->pending_recoveries[pending_index].active)
                {
                    break;
                }
            }
            if (pending_index >= HENKA_TERRAIN_CLIENT_MAX_PENDING_RECOVERIES)
            {
                return HENKA_ERROR_LIMIT;
            }
        }
        {
            uint8_t payload[HENKA_TERRAIN_NETWORK_MAX_RECOVERY_REQUEST_BYTES];
            size_t payload_size;
            if (henka_terrain_delta_recovery_request_encode(
                    &request, payload, sizeof(payload), &payload_size) != HENKA_SUCCESS ||
                henka_network_client_send(
                    client->network, HENKA_NETWORK_CHANNEL_TERRAIN,
                    HENKA_NETWORK_MESSAGE_TERRAIN_RECOVERY_REQUEST,
                    payload, payload_size) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_PLATFORM;
            }
        }
        client->pending_recoveries[pending_index].active = true;
        client->pending_recoveries[pending_index].region_id = request.region_id;
        client->pending_recoveries[pending_index].target_revision = request.target_revision;
        if (!replacing_pending)
        {
            ++client->diagnostics.pending_recovery_count;
        }
        ++client->diagnostics.recovery_delta_request_count;
    }
    return HENKA_SUCCESS;
}

static bool henka_terrain_client_delta_has_revision_gap(
    const henka_terrain_client* client,
    const henka_terrain_edit_delta* delta)
{
    henka_terrain_world_desc desc;
    uint32_t index;

    if (client == NULL || delta == NULL ||
        henka_terrain_world_get_desc(client->world, &desc) != HENKA_SUCCESS ||
        desc.world_identity != delta->world_identity ||
        desc.base_asset_identity != delta->base_asset_identity)
    {
        return false;
    }
    for (index = 0U; index < delta->affected_region_count; ++index)
    {
        henka_terrain_region_state state;
        const henka_terrain_revision target_revision =
            delta->affected_regions[index].revision;
        if (target_revision == 0U ||
            henka_terrain_world_get_region_state(
                client->world, delta->affected_regions[index].region_id, &state) != HENKA_SUCCESS ||
            state.revision == UINT64_MAX)
        {
            return false;
        }
        if (target_revision > state.revision + 1U)
        {
            return true;
        }
    }
    return false;
}

static henka_result henka_terrain_client_handle_session_info(
    henka_terrain_client* client,
    const henka_network_event* event)
{
    henka_terrain_world_desc desc;
    henka_terrain_session_info info;
    uint32_t index;

    if (henka_terrain_session_info_decode(
            event->message.payload, event->message.payload_size, &info) != HENKA_SUCCESS ||
        henka_terrain_world_get_desc(client->world, &desc) != HENKA_SUCCESS ||
        desc.world_identity != info.world_identity ||
        desc.base_asset_identity != info.base_asset_identity)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (client->session_interest_pending && info.flags == 0U)
    {
        /* The server sends its bounded legacy summary immediately on connect.
         * An opted-in client waits for the relevance-filtered response so it
         * does not bootstrap an irrelevant second set of regions. */
        return HENKA_SUCCESS;
    }
    if ((info.flags & HENKA_TERRAIN_SESSION_INFO_FLAG_RELEVANCE_FILTERED) != 0U)
    {
        client->session_interest_pending = false;
        client->diagnostics.session_interest_region_count += info.region_count;
    }
    for (index = 0U; index < info.region_count; ++index)
    {
        henka_terrain_region_state state;
        henka_terrain_snapshot_request request;
        if (henka_terrain_world_get_region_state(
                client->world, info.regions[index].region_id, &state) == HENKA_SUCCESS &&
            state.revision == info.regions[index].revision &&
            state.generation == info.regions[index].generation)
        {
            henka_terrain_client_clear_session_snapshot(
                client, info.regions[index].region_id, state.revision);
            continue;
        }
        request = (henka_terrain_snapshot_request){
            desc.world_identity,
            desc.base_asset_identity,
            info.regions[index].region_id,
            info.regions[index].revision};
        {
            uint32_t pending_snapshot_index = henka_terrain_client_find_pending_session_snapshot(
                client, info.regions[index].region_id);
            if (pending_snapshot_index < HENKA_TERRAIN_NETWORK_MAX_SESSION_REGIONS &&
                info.regions[index].revision <=
                    client->pending_session_snapshots[pending_snapshot_index].target_revision &&
                info.regions[index].generation <=
                    client->pending_session_snapshots[pending_snapshot_index].target_generation)
            {
                ++client->diagnostics.session_snapshot_suppressed_count;
                continue;
            }
            if (pending_snapshot_index >= HENKA_TERRAIN_NETWORK_MAX_SESSION_REGIONS)
            {
                for (pending_snapshot_index = 0U;
                     pending_snapshot_index < HENKA_TERRAIN_NETWORK_MAX_SESSION_REGIONS;
                     ++pending_snapshot_index)
                {
                    if (!client->pending_session_snapshots[pending_snapshot_index].active)
                    {
                        break;
                    }
                }
            }
            if (pending_snapshot_index >= HENKA_TERRAIN_NETWORK_MAX_SESSION_REGIONS)
            {
                return HENKA_ERROR_LIMIT;
            }
            if (henka_terrain_client_request_snapshot(client, request) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_PLATFORM;
            }
            client->pending_session_snapshots[pending_snapshot_index].active = true;
            client->pending_session_snapshots[pending_snapshot_index].region_id = request.region_id;
            client->pending_session_snapshots[pending_snapshot_index].target_revision = request.expected_revision;
            client->pending_session_snapshots[pending_snapshot_index].target_generation =
                info.regions[index].generation;
            ++client->diagnostics.session_snapshot_request_count;
            continue;
        }
    }
    return HENKA_SUCCESS;
}

static void henka_terrain_client_sync_replica_diagnostics(
    henka_terrain_client* client)
{
    henka_terrain_replica_diagnostics replica_diagnostics;
    henka_terrain_replica_get_diagnostics(client->replica, &replica_diagnostics);
    client->diagnostics.applied_delta_count = replica_diagnostics.applied_delta_count;
    client->diagnostics.duplicate_delta_count = replica_diagnostics.duplicate_delta_count;
    client->diagnostics.rejected_delta_count = replica_diagnostics.rejected_delta_count;
    client->diagnostics.completed_snapshot_count = replica_diagnostics.completed_snapshot_count;
    client->diagnostics.rejected_snapshot_count = replica_diagnostics.rejected_snapshot_count;
    client->diagnostics.stale_snapshot_count = replica_diagnostics.stale_snapshot_count;
    {
        henka_terrain_prediction_stats prediction_stats;
        henka_terrain_prediction_get_stats(client->prediction, &prediction_stats);
        client->diagnostics.pending_prediction_count = prediction_stats.pending_command_count;
        client->diagnostics.prediction_replay_failure_count = prediction_stats.replay_failure_count;
        client->diagnostics.prediction_enabled = prediction_stats.prediction_enabled;
    }
}

static bool henka_terrain_client_delta_is_out_of_interest(
    const henka_terrain_client* client,
    const henka_terrain_edit_delta* delta)
{
    henka_terrain_world_desc desc;
    henka_terrain_region_id affected[HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS];
    uint32_t affected_count = HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS;
    uint32_t index;
    bool missing_region = false;
    bool resident_region = false;
    if (client == NULL || delta == NULL ||
        henka_terrain_world_get_desc(client->world, &desc) != HENKA_SUCCESS ||
        desc.world_identity != delta->world_identity ||
        desc.base_asset_identity != delta->base_asset_identity ||
        henka_terrain_edit_get_affected_regions(
            client->world, &delta->command, affected, &affected_count) != HENKA_SUCCESS ||
        affected_count != delta->affected_region_count)
    {
        return false;
    }
    for (index = 0U; index < delta->affected_region_count; ++index)
    {
        henka_terrain_region_state state;
        if (!henka_terrain_region_id_equal(
                affected[index], delta->affected_regions[index].region_id))
        {
            return false;
        }
        if (henka_terrain_world_get_region_state(
                client->world, delta->affected_regions[index].region_id, &state) != HENKA_SUCCESS)
        {
            missing_region = true;
        }
        else
        {
            resident_region = true;
        }
    }
    return missing_region && !resident_region;
}

henka_result henka_terrain_client_handle_event(
    henka_terrain_client* client,
    const henka_network_event* event)
{
    henka_result result;
    if (client == NULL || event == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (event->type == HENKA_NETWORK_EVENT_CONNECTED)
    {
        ++client->diagnostics.connected_event_count;
        client->last_snapshot_retry_transfer_id = 0U;
        client->snapshot_retry_count = 0U;
        if (client->session_interest_enabled &&
            henka_terrain_client_request_session_interest(client) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_PLATFORM;
        }
        return HENKA_SUCCESS;
    }
    if (event->type == HENKA_NETWORK_EVENT_DISCONNECTED)
    {
        ++client->diagnostics.disconnected_event_count;
        /* The transport has retired this stream. Requests recorded against
         * it must not suppress recovery on the next connection, even when a
         * caller observes the disconnect before invoking reconnect. */
        henka_terrain_client_clear_all_pending_recoveries(client);
        henka_terrain_client_clear_all_pending_session_snapshots(client);
        client->session_interest_pending = false;
        return HENKA_SUCCESS;
    }
    if (event->type != HENKA_NETWORK_EVENT_MESSAGE)
    {
        return HENKA_SUCCESS;
    }
    ++client->diagnostics.processed_message_count;
    if (event->message.channel == HENKA_NETWORK_CHANNEL_CONTROL &&
        event->message.type == HENKA_NETWORK_MESSAGE_CONNECT)
    {
        return henka_terrain_client_handle_session_info(client, event);
    }
    if (event->message.type == HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_ACCEPTED)
    {
        henka_terrain_edit_acceptance acceptance;
        result = henka_terrain_edit_acceptance_decode(
            event->message.payload, event->message.payload_size, &acceptance);
        if (result != HENKA_SUCCESS)
        {
            ++client->diagnostics.malformed_message_count;
            return result;
        }
        client->diagnostics.last_acceptance = acceptance;
        client->diagnostics.last_acceptance_valid = true;
        ++client->diagnostics.acceptance_count;
        return HENKA_SUCCESS;
    }
    if (event->message.type == HENKA_NETWORK_MESSAGE_TERRAIN_EDIT_REJECTED)
    {
        henka_terrain_edit_rejection rejection;
        result = henka_terrain_edit_rejection_decode(
            event->message.payload, event->message.payload_size, &rejection);
        if (result != HENKA_SUCCESS)
        {
            ++client->diagnostics.malformed_message_count;
            return result;
        }
        client->diagnostics.last_rejection = rejection;
        client->diagnostics.last_rejection_valid = true;
        ++client->diagnostics.rejection_count;
        {
            henka_result prediction_result = henka_terrain_prediction_reject(
                client->prediction, rejection.client_nonce);
            if (prediction_result != HENKA_SUCCESS &&
                prediction_result != HENKA_ERROR_INVALID_ARGUMENT)
            {
                return prediction_result;
            }
        }
        henka_terrain_client_sync_replica_diagnostics(client);
        return HENKA_SUCCESS;
    }
    if (event->message.type == HENKA_NETWORK_MESSAGE_TERRAIN_DELTA)
    {
        henka_terrain_edit_delta delta;
        bool applied = false;
        result = henka_terrain_edit_delta_decode(
            event->message.payload, event->message.payload_size, &delta);
        if (result != HENKA_SUCCESS)
        {
            ++client->diagnostics.malformed_message_count;
            return result;
        }
        if (henka_terrain_client_delta_is_out_of_interest(client, &delta))
        {
            ++client->diagnostics.out_of_interest_delta_count;
            return HENKA_SUCCESS;
        }
        result = henka_terrain_replica_apply_delta(client->replica, &delta, &applied);
        if (result != HENKA_SUCCESS)
        {
            /* The replica also uses ASSET_SOURCE for some non-gap failures.
             * Verify the revision relationship independently before allowing
             * recovery; protocol, identity, sizing, and allocation failures
             * must remain hard errors and must never cause a request derived
             * from the rejected message. */
            if (result != HENKA_ERROR_ASSET_SOURCE ||
                !henka_terrain_client_delta_has_revision_gap(client, &delta))
            {
                henka_terrain_client_sync_replica_diagnostics(client);
                return result;
            }
            result = henka_terrain_client_request_delta_recovery(client, &delta);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            return HENKA_SUCCESS;
        }
        if (delta.client_nonce != 0U)
        {
            henka_result prediction_result = henka_terrain_prediction_accept(
                client->prediction, delta.client_nonce);
            if (prediction_result != HENKA_SUCCESS &&
                prediction_result != HENKA_ERROR_INVALID_ARGUMENT)
            {
                return prediction_result;
            }
        }
        for (uint32_t index = 0U; index < delta.affected_region_count; ++index)
        {
            henka_terrain_region_state state;
            if (henka_terrain_world_get_region_state(
                    client->world, delta.affected_regions[index].region_id, &state) == HENKA_SUCCESS)
            {
                henka_terrain_client_clear_pending_recovery(
                    client, state.id, state.revision);
            }
        }
        if (henka_terrain_prediction_refresh(client->prediction) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_ASSET_SOURCE;
        }
        (void)applied;
        henka_terrain_client_sync_replica_diagnostics(client);
        return HENKA_SUCCESS;
    }
    if (event->message.channel == HENKA_NETWORK_CHANNEL_SNAPSHOT &&
        event->message.type == HENKA_NETWORK_MESSAGE_SNAPSHOT_FAILED)
    {
        henka_terrain_snapshot_failure failure;
        henka_terrain_world_desc desc;
        if (henka_terrain_snapshot_failure_decode(
                event->message.payload, event->message.payload_size, &failure) != HENKA_SUCCESS ||
            henka_terrain_world_get_desc(client->world, &desc) != HENKA_SUCCESS ||
            desc.world_identity != failure.world_identity ||
            desc.base_asset_identity != failure.base_asset_identity)
        {
            ++client->diagnostics.malformed_message_count;
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        henka_terrain_client_clear_failed_snapshot_state(client, failure.region_id);
        ++client->diagnostics.snapshot_failure_count;
        return HENKA_SUCCESS;
    }
    if (event->message.type == HENKA_NETWORK_MESSAGE_SNAPSHOT_FRAGMENT)
    {
        henka_terrain_snapshot_fragment fragment;
        bool complete = false;
        bool decoded = false;
        bool retry_requested = false;
        result = henka_terrain_snapshot_fragment_decode(
            event->message.payload, event->message.payload_size, &fragment);
        if (result == HENKA_SUCCESS)
        {
            decoded = true;
            result = henka_terrain_replica_apply_snapshot_fragment(
                client->replica, &fragment, &complete);
        }
        if (result != HENKA_SUCCESS)
        {
            if (decoded &&
                henka_terrain_client_retry_snapshot(
                    client, &fragment, &retry_requested) != HENKA_SUCCESS)
            {
                return HENKA_ERROR_PLATFORM;
            }
            henka_terrain_client_sync_replica_diagnostics(client);
            if (retry_requested)
            {
                return HENKA_SUCCESS;
            }
            ++client->diagnostics.rejected_snapshot_count;
            return result;
        }
        if (complete && henka_terrain_prediction_refresh(client->prediction) != HENKA_SUCCESS)
        {
            return HENKA_ERROR_ASSET_SOURCE;
        }
        if (complete)
        {
            henka_terrain_client_clear_session_snapshot(
                client, fragment.region_id, fragment.revision);
            henka_terrain_client_clear_pending_recovery(
                client, fragment.region_id, fragment.revision);
            client->last_snapshot_retry_transfer_id = 0U;
            client->snapshot_retry_count = 0U;
        }
        (void)complete;
        henka_terrain_client_sync_replica_diagnostics(client);
        return HENKA_SUCCESS;
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_client_poll(
    henka_terrain_client* client,
    uint32_t timeout_milliseconds,
    uint32_t max_events,
    uint32_t* out_event_count)
{
    uint32_t index;
    uint32_t processed = 0U;
    if (out_event_count == NULL || client == NULL || max_events == 0U ||
        timeout_milliseconds > HENKA_NETWORK_MAX_POLL_TIMEOUT_MILLISECONDS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_event_count = 0U;
    for (index = 0U; index < max_events; ++index)
    {
        henka_network_event event;
        henka_result result = henka_network_client_poll(
            client->network, index == 0U ? timeout_milliseconds : 0U, &event);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (event.type == HENKA_NETWORK_EVENT_NONE)
        {
            break;
        }
        result = henka_terrain_client_handle_event(client, &event);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        ++processed;
    }
    *out_event_count = processed;
    return HENKA_SUCCESS;
}

void henka_terrain_client_get_diagnostics(
    const henka_terrain_client* client,
    henka_terrain_client_diagnostics* out_diagnostics)
{
    if (out_diagnostics == NULL)
    {
        return;
    }
    *out_diagnostics = client == NULL
        ? (henka_terrain_client_diagnostics){0}
        : client->diagnostics;
}

henka_terrain_world* henka_terrain_client_get_predicted_world(
    henka_terrain_client* client)
{
    return client == NULL ? NULL : henka_terrain_prediction_get_world(client->prediction);
}
