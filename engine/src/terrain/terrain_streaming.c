#include <henka/terrain_streaming.h>

#include <string.h>

#include <henka/memory.h>

#include "terrain_internal.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct henka_terrain_stream_request
{
    bool active;
    henka_terrain_region_id region_id;
    bool observer_demand;
    uint32_t priority;
    uint64_t sequence;
} henka_terrain_stream_request;

typedef struct henka_terrain_stream_completion
{
    bool active;
    henka_result result;
    bool generated;
    henka_terrain_region_storage_info info;
    henka_terrain_sample* samples;
} henka_terrain_stream_completion;

struct henka_terrain_streamer
{
    henka_terrain_world* world;
    henka_terrain_storage* storage;
    henka_terrain_stream_desc desc;
    henka_terrain_stream_request* requests;
    henka_terrain_stream_completion* completions;
    henka_terrain_stream_observer* observers;
    uint32_t observer_capacity;
    uint32_t queued_request_count;
    uint32_t active_request_count;
    uint32_t completion_count;
    uint32_t observer_count;
    uint32_t max_queued_request_count;
    uint32_t max_active_request_count;
    uint32_t max_completion_count;
    uint32_t max_observer_count;
    uint64_t next_sequence;
    uint64_t coalesced_request_count;
    uint64_t completed_request_count;
    uint64_t failed_request_count;
    uint64_t cancelled_request_count;
    uint64_t dropped_completion_count;
    uint64_t stale_completion_count;
    uint64_t evicted_region_count;
    uint64_t generated_region_count;
    uint64_t generator_failure_count;
    henka_terrain_region_id active_region;
    bool active_region_valid;
    bool active_observer_demand;
    bool cancel_active;
    bool stop;
    CRITICAL_SECTION lock;
    CONDITION_VARIABLE condition;
    HANDLE worker;
};

static bool henka_terrain_stream_generated_samples_valid(
    const henka_terrain_sample* samples,
    size_t sample_count)
{
    size_t index;
    if (samples == NULL || sample_count == 0U)
    {
        return false;
    }
    for (index = 0U; index < sample_count; ++index)
    {
        uint32_t total = 0U;
        uint32_t material_index;
        for (material_index = 0U;
             material_index < HENKA_TERRAIN_ACTIVE_MATERIAL_COUNT;
             ++material_index)
        {
            total += samples[index].material_weights[material_index];
        }
        if (total != 255U)
        {
            return false;
        }
    }
    return true;
}

static bool henka_terrain_stream_region_equal(
    henka_terrain_region_id left,
    henka_terrain_region_id right)
{
    return left.x == right.x && left.z == right.z;
}

static uint32_t henka_terrain_stream_region_priority(
    const henka_terrain_streamer* streamer,
    henka_terrain_region_id region_id)
{
    uint32_t best_priority = UINT32_MAX;
    uint32_t index;
    for (index = 0U; index < streamer->observer_capacity; ++index)
    {
        const henka_terrain_stream_observer* observer = &streamer->observers[index];
        uint64_t dx;
        uint64_t dz;
        uint64_t distance;
        uint32_t band;
        uint32_t priority;
        if (observer->id == 0U)
        {
            continue;
        }
        dx = region_id.x >= observer->center_region.x
            ? (uint64_t)((int64_t)region_id.x - (int64_t)observer->center_region.x)
            : (uint64_t)((int64_t)observer->center_region.x - (int64_t)region_id.x);
        dz = region_id.z >= observer->center_region.z
            ? (uint64_t)((int64_t)region_id.z - (int64_t)observer->center_region.z)
            : (uint64_t)((int64_t)observer->center_region.z - (int64_t)region_id.z);
        distance = dx > dz ? dx : dz;
        band = distance <= observer->render_radius_regions
            ? 0U
            : distance <= observer->physics_radius_regions
                ? 1U
                : 2U;
        if (distance > observer->cpu_radius_regions)
        {
            band = 3U;
        }
        priority = band > UINT32_MAX / (streamer->world->desc.regions_across + 1U)
            ? UINT32_MAX
            : band * (streamer->world->desc.regions_across + 1U) +
                (distance > UINT32_MAX ? UINT32_MAX : (uint32_t)distance);
        if (priority < best_priority)
        {
            best_priority = priority;
        }
    }
    return best_priority;
}

static uint32_t henka_terrain_stream_find_free_request(const henka_terrain_streamer* streamer)
{
    uint32_t index;
    for (index = 0U; index < streamer->desc.max_requests; ++index)
    {
        if (!streamer->requests[index].active)
        {
            return index;
        }
    }
    return streamer->desc.max_requests;
}

static uint32_t henka_terrain_stream_find_free_completion(const henka_terrain_streamer* streamer)
{
    uint32_t index;
    for (index = 0U; index < streamer->desc.max_completions; ++index)
    {
        if (!streamer->completions[index].active)
        {
            return index;
        }
    }
    return streamer->desc.max_completions;
}

static bool henka_terrain_stream_request_exists(
    const henka_terrain_streamer* streamer,
    henka_terrain_region_id region_id)
{
    uint32_t index;
    if (streamer->active_region_valid && henka_terrain_stream_region_equal(streamer->active_region, region_id))
    {
        return true;
    }
    for (index = 0U; index < streamer->desc.max_requests; ++index)
    {
        if (streamer->requests[index].active &&
            henka_terrain_stream_region_equal(streamer->requests[index].region_id, region_id))
        {
            return true;
        }
    }
    return false;
}

static bool henka_terrain_stream_region_within_observer(
    henka_terrain_region_id region_id,
    const henka_terrain_stream_observer* observer,
    uint32_t radius)
{
    int64_t dx = (int64_t)region_id.x - (int64_t)observer->center_region.x;
    int64_t dz = (int64_t)region_id.z - (int64_t)observer->center_region.z;
    return dx >= -(int64_t)radius && dx <= (int64_t)radius &&
        dz >= -(int64_t)radius && dz <= (int64_t)radius;
}

static bool henka_terrain_stream_region_within_any_cpu_radius(
    const henka_terrain_streamer* streamer,
    henka_terrain_region_id region_id)
{
    uint32_t index;
    for (index = 0U; index < streamer->observer_capacity; ++index)
    {
        const henka_terrain_stream_observer* observer = &streamer->observers[index];
        if (observer->id != 0U &&
            henka_terrain_stream_region_within_observer(
                region_id, observer, observer->cpu_radius_regions))
        {
            return true;
        }
    }
    return false;
}

static void henka_terrain_stream_cancel_stale_observer_requests_locked(
    henka_terrain_streamer* streamer)
{
    uint32_t index;
    if (streamer->active_region_valid && streamer->active_observer_demand &&
        !henka_terrain_stream_region_within_any_cpu_radius(
            streamer, streamer->active_region))
    {
        streamer->cancel_active = true;
    }
    for (index = 0U; index < streamer->desc.max_requests; ++index)
    {
        if (streamer->requests[index].active && streamer->requests[index].observer_demand &&
            !henka_terrain_stream_region_within_any_cpu_radius(
                streamer, streamer->requests[index].region_id))
        {
            streamer->requests[index].active = false;
            --streamer->queued_request_count;
            ++streamer->cancelled_request_count;
        }
    }
}

static bool henka_terrain_stream_completion_is_stale(
    const henka_terrain_streamer* streamer,
    const henka_terrain_stream_completion* completion)
{
    henka_terrain_region_state current;
    if (streamer == NULL || completion == NULL || completion->result != HENKA_SUCCESS ||
        henka_terrain_world_get_region_state(
            streamer->world, completion->info.id, &current) != HENKA_SUCCESS)
    {
        return false;
    }
    if (current.generation != completion->info.generation)
    {
        return current.generation > completion->info.generation;
    }
    return current.revision > completion->info.revision;
}

static bool henka_terrain_stream_region_retained(
    const henka_terrain_streamer* streamer,
    henka_terrain_region_id region_id)
{
    uint32_t index;
    for (index = 0U; index < streamer->observer_capacity; ++index)
    {
        const henka_terrain_stream_observer* observer = &streamer->observers[index];
        uint32_t unload_radius;
        if (observer->id == 0U)
        {
            continue;
        }
        unload_radius = observer->cpu_unload_radius_regions == 0U
            ? observer->cpu_radius_regions
            : observer->cpu_unload_radius_regions;
        if (henka_terrain_stream_region_within_observer(region_id, observer, unload_radius))
        {
            return true;
        }
    }
    return false;
}

static bool henka_terrain_stream_region_within_any_observer_radius(
    const henka_terrain_streamer* streamer,
    henka_terrain_region_id region_id,
    bool physics)
{
    uint32_t index;
    for (index = 0U; index < streamer->observer_capacity; ++index)
    {
        const henka_terrain_stream_observer* observer = &streamer->observers[index];
        uint32_t radius;
        if (observer->id == 0U)
        {
            continue;
        }
        radius = physics ? observer->physics_radius_regions : observer->render_radius_regions;
        if (henka_terrain_stream_region_within_observer(region_id, observer, radius))
        {
            return true;
        }
    }
    return false;
}

static void henka_terrain_stream_sync_presentation_residency(
    henka_terrain_streamer* streamer)
{
    uint32_t index;
    for (index = 0U; index < streamer->world->desc.max_resident_regions; ++index)
    {
        henka_terrain_region_state state;
        bool physics_resident;
        bool render_resident;
        if (!streamer->world->regions[index].active ||
            henka_terrain_world_get_region_state(
                streamer->world,
                streamer->world->regions[index].state.id,
                &state) != HENKA_SUCCESS)
        {
            continue;
        }
        physics_resident = henka_terrain_stream_region_within_any_observer_radius(
            streamer, state.id, true);
        render_resident = henka_terrain_stream_region_within_any_observer_radius(
            streamer, state.id, false);
        if (state.physics_resident != physics_resident ||
            state.render_resident != render_resident)
        {
            (void)henka_terrain_world_set_region_residency(
                streamer->world, state.id, physics_resident, render_resident,
                state.pending_io);
        }
    }
}

static void henka_terrain_stream_reconcile_residency(henka_terrain_streamer* streamer)
{
    henka_terrain_world_desc world_desc;
    uint32_t z;
    uint32_t x;
    if (henka_terrain_world_get_desc(streamer->world, &world_desc) != HENKA_SUCCESS)
    {
        return;
    }
    henka_terrain_stream_sync_presentation_residency(streamer);
    for (z = 0U; z < world_desc.regions_down; ++z)
    {
        for (x = 0U; x < world_desc.regions_across; ++x)
        {
            henka_terrain_region_id region_id = {(int32_t)x, (int32_t)z};
            henka_terrain_region_state state;
            if (henka_terrain_world_get_region_state(streamer->world, region_id, &state) != HENKA_SUCCESS ||
                !state.cpu_resident || state.physics_resident || state.render_resident ||
                state.pending_io || state.dirty ||
                henka_terrain_stream_region_retained(streamer, region_id))
            {
                continue;
            }
            if (henka_terrain_world_release_region(streamer->world, region_id) == HENKA_SUCCESS)
            {
                ++streamer->evicted_region_count;
            }
        }
    }
}

static DWORD WINAPI henka_terrain_stream_worker(void* argument)
{
    henka_terrain_streamer* streamer = (henka_terrain_streamer*)argument;
    for (;;)
    {
        henka_terrain_stream_request request = {0};
        henka_terrain_sample* samples = NULL;
        henka_terrain_region_storage_info info = {0};
        henka_result result;
        uint32_t request_index;
        uint32_t completion_index;
        bool cancelled;
        bool generated = false;
        bool generator_attempted = false;

        EnterCriticalSection(&streamer->lock);
        while (!streamer->stop && streamer->queued_request_count == 0U)
        {
            SleepConditionVariableCS(&streamer->condition, &streamer->lock, INFINITE);
        }
        if (streamer->stop)
        {
            LeaveCriticalSection(&streamer->lock);
            return 0U;
        }
        request_index = streamer->desc.max_requests;
        {
            uint32_t candidate_index;
            uint32_t best_priority = UINT32_MAX;
            uint64_t best_sequence = UINT64_MAX;
            for (candidate_index = 0U;
                 candidate_index < streamer->desc.max_requests;
                 ++candidate_index)
            {
                const henka_terrain_stream_request* candidate = &streamer->requests[candidate_index];
                if (candidate->active &&
                    (candidate->priority < best_priority ||
                        (candidate->priority == best_priority && candidate->sequence < best_sequence)))
                {
                    request_index = candidate_index;
                    best_priority = candidate->priority;
                    best_sequence = candidate->sequence;
                }
            }
        }
        if (request_index >= streamer->desc.max_requests)
        {
            LeaveCriticalSection(&streamer->lock);
            continue;
        }
        request = streamer->requests[request_index];
        streamer->requests[request_index].active = false;
        --streamer->queued_request_count;
        streamer->active_request_count = 1U;
        if (streamer->active_request_count > streamer->max_active_request_count)
        {
            streamer->max_active_request_count = streamer->active_request_count;
        }
        streamer->active_region = request.region_id;
        streamer->active_region_valid = true;
        streamer->active_observer_demand = request.observer_demand;
        streamer->cancel_active = false;
        LeaveCriticalSection(&streamer->lock);

        samples = henka_calloc(streamer->world->layout.samples_per_region, sizeof(*samples));
        result = samples == NULL
            ? HENKA_ERROR_OUT_OF_MEMORY
            : henka_terrain_storage_load_region(
                streamer->storage,
                request.region_id,
                &info,
                samples,
                streamer->world->layout.samples_per_region);
        if (result == HENKA_ERROR_ASSET_SOURCE && streamer->desc.generate_region != NULL)
        {
            generator_attempted = true;
            result = streamer->desc.generate_region(
                streamer->desc.generate_region_user_data,
                request.region_id,
                &streamer->world->desc,
                &streamer->world->layout,
                samples,
                streamer->world->layout.samples_per_region);
            if (result == HENKA_SUCCESS)
            {
                if (!henka_terrain_stream_generated_samples_valid(
                        samples, streamer->world->layout.samples_per_region))
                {
                    result = HENKA_ERROR_INVALID_ARGUMENT;
                }
                else
                {
                    info = (henka_terrain_region_storage_info){request.region_id, 1U, 1U};
                    generated = true;
                }
            }
        }

        EnterCriticalSection(&streamer->lock);
        if (generator_attempted && !generated)
        {
            ++streamer->generator_failure_count;
        }
        cancelled = streamer->cancel_active;
        streamer->active_request_count = 0U;
        streamer->active_region_valid = false;
        streamer->active_observer_demand = false;
        streamer->cancel_active = false;
        if (cancelled)
        {
            ++streamer->cancelled_request_count;
            henka_free(samples);
        }
        else
        {
            completion_index = henka_terrain_stream_find_free_completion(streamer);
            if (completion_index >= streamer->desc.max_completions)
            {
                ++streamer->dropped_completion_count;
                henka_free(samples);
            }
            else
            {
                if (result != HENKA_SUCCESS)
                {
                    henka_free(samples);
                    samples = NULL;
                }
                streamer->completions[completion_index].active = true;
                streamer->completions[completion_index].result = result;
                streamer->completions[completion_index].generated = generated;
                streamer->completions[completion_index].info = info;
                streamer->completions[completion_index].samples = result == HENKA_SUCCESS ? samples : NULL;
                ++streamer->completion_count;
                if (streamer->completion_count > streamer->max_completion_count)
                {
                    streamer->max_completion_count = streamer->completion_count;
                }
            }
        }
        WakeAllConditionVariable(&streamer->condition);
        LeaveCriticalSection(&streamer->lock);
    }
}

henka_terrain_stream_desc henka_terrain_stream_desc_default(void)
{
    return (henka_terrain_stream_desc){16U, 16U, NULL, NULL};
}

henka_result henka_terrain_streamer_create(
    henka_terrain_world* world,
    henka_terrain_storage* storage,
    const henka_terrain_stream_desc* desc,
    henka_terrain_streamer** out_streamer)
{
    henka_terrain_streamer* streamer;
    henka_terrain_stream_desc defaults;

    if (out_streamer == NULL || world == NULL || storage == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_streamer = NULL;
    defaults = henka_terrain_stream_desc_default();
    if (desc == NULL)
    {
        desc = &defaults;
    }
    if (desc->max_requests == 0U || desc->max_requests > HENKA_TERRAIN_STREAM_MAX_REQUESTS ||
        desc->max_completions == 0U || desc->max_completions > HENKA_TERRAIN_STREAM_MAX_COMPLETIONS ||
        desc->max_requests > world->desc.max_pending_io ||
        desc->max_completions > world->desc.max_pending_io)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    streamer = henka_calloc(1U, sizeof(*streamer));
    if (streamer == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    streamer->desc = *desc;
    streamer->world = world;
    streamer->storage = storage;
    streamer->observer_capacity = world->desc.max_stream_observers;
    streamer->requests = henka_calloc(desc->max_requests, sizeof(*streamer->requests));
    streamer->completions = henka_calloc(desc->max_completions, sizeof(*streamer->completions));
    streamer->observers = henka_calloc(streamer->observer_capacity, sizeof(*streamer->observers));
    if (streamer->requests == NULL || streamer->completions == NULL || streamer->observers == NULL)
    {
        henka_free(streamer->observers);
        henka_free(streamer->completions);
        henka_free(streamer->requests);
        henka_free(streamer);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    InitializeCriticalSection(&streamer->lock);
    InitializeConditionVariable(&streamer->condition);
    streamer->worker = CreateThread(NULL, 0U, henka_terrain_stream_worker, streamer, 0U, NULL);
    if (streamer->worker == NULL)
    {
        DeleteCriticalSection(&streamer->lock);
        henka_free(streamer->observers);
        henka_free(streamer->completions);
        henka_free(streamer->requests);
        henka_free(streamer);
        return HENKA_ERROR_PLATFORM;
    }
    *out_streamer = streamer;
    return HENKA_SUCCESS;
}

void henka_terrain_streamer_destroy(henka_terrain_streamer* streamer)
{
    uint32_t index;
    if (streamer == NULL)
    {
        return;
    }
    EnterCriticalSection(&streamer->lock);
    streamer->stop = true;
    WakeAllConditionVariable(&streamer->condition);
    LeaveCriticalSection(&streamer->lock);
    WaitForSingleObject(streamer->worker, INFINITE);
    CloseHandle(streamer->worker);
    for (index = 0U; index < streamer->desc.max_completions; ++index)
    {
        henka_free(streamer->completions[index].samples);
    }
    DeleteCriticalSection(&streamer->lock);
    henka_free(streamer->observers);
    henka_free(streamer->completions);
    henka_free(streamer->requests);
    henka_free(streamer);
}

henka_result henka_terrain_streamer_request_region(
    henka_terrain_streamer* streamer,
    henka_terrain_region_id region_id)
{
    uint32_t index;
    if (streamer == NULL || !henka_terrain_region_id_is_valid(&streamer->world->desc, region_id))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    EnterCriticalSection(&streamer->lock);
    if (streamer->active_region_valid &&
        henka_terrain_stream_region_equal(streamer->active_region, region_id))
    {
        streamer->active_observer_demand = false;
        streamer->cancel_active = false;
        ++streamer->coalesced_request_count;
        LeaveCriticalSection(&streamer->lock);
        return HENKA_SUCCESS;
    }
    for (index = 0U; index < streamer->desc.max_requests; ++index)
    {
        if (streamer->requests[index].active &&
            henka_terrain_stream_region_equal(streamer->requests[index].region_id, region_id))
        {
            streamer->requests[index].observer_demand = false;
            ++streamer->coalesced_request_count;
            LeaveCriticalSection(&streamer->lock);
            return HENKA_SUCCESS;
        }
    }
    index = henka_terrain_stream_find_free_request(streamer);
    if (index >= streamer->desc.max_requests)
    {
        LeaveCriticalSection(&streamer->lock);
        return HENKA_ERROR_LIMIT;
    }
    streamer->requests[index] = (henka_terrain_stream_request){
        true,
        region_id,
        false,
        henka_terrain_stream_region_priority(streamer, region_id),
        streamer->next_sequence++};
    ++streamer->queued_request_count;
    if (streamer->queued_request_count > streamer->max_queued_request_count)
    {
        streamer->max_queued_request_count = streamer->queued_request_count;
    }
    WakeConditionVariable(&streamer->condition);
    LeaveCriticalSection(&streamer->lock);
    return HENKA_SUCCESS;
}

henka_result henka_terrain_streamer_cancel_region(
    henka_terrain_streamer* streamer,
    henka_terrain_region_id region_id)
{
    uint32_t index;
    if (streamer == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    EnterCriticalSection(&streamer->lock);
    if (streamer->active_region_valid && henka_terrain_stream_region_equal(streamer->active_region, region_id))
    {
        streamer->cancel_active = true;
        LeaveCriticalSection(&streamer->lock);
        return HENKA_SUCCESS;
    }
    for (index = 0U; index < streamer->desc.max_requests; ++index)
    {
        if (streamer->requests[index].active &&
            henka_terrain_stream_region_equal(streamer->requests[index].region_id, region_id))
        {
            streamer->requests[index].active = false;
            --streamer->queued_request_count;
            ++streamer->cancelled_request_count;
            LeaveCriticalSection(&streamer->lock);
            return HENKA_SUCCESS;
        }
    }
    LeaveCriticalSection(&streamer->lock);
    return HENKA_ERROR_INVALID_ARGUMENT;
}

static henka_result henka_terrain_streamer_store_observer(
    henka_terrain_streamer* streamer,
    const henka_terrain_stream_observer* observer,
    bool require_existing)
{
    uint32_t index;
    if (streamer == NULL || observer == NULL || observer->id == 0U ||
        !henka_terrain_region_id_is_valid(&streamer->world->desc, observer->center_region) ||
        observer->cpu_radius_regions > streamer->world->desc.regions_across ||
        observer->physics_radius_regions > streamer->world->desc.regions_across ||
        observer->render_radius_regions > streamer->world->desc.regions_across ||
        observer->cpu_unload_radius_regions > streamer->world->desc.regions_across ||
        (observer->cpu_unload_radius_regions != 0U &&
            observer->cpu_unload_radius_regions < observer->cpu_radius_regions))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    EnterCriticalSection(&streamer->lock);
    for (index = 0U; index < streamer->observer_capacity; ++index)
    {
        if (streamer->observers[index].id == observer->id)
        {
            streamer->observers[index] = *observer;
            for (uint32_t request_index = 0U;
                 request_index < streamer->desc.max_requests;
                 ++request_index)
            {
                if (streamer->requests[request_index].active)
                {
                    streamer->requests[request_index].priority =
                        henka_terrain_stream_region_priority(
                            streamer, streamer->requests[request_index].region_id);
                }
            }
            LeaveCriticalSection(&streamer->lock);
            return HENKA_SUCCESS;
        }
    }
    if (require_existing || streamer->observer_count >= streamer->observer_capacity)
    {
        LeaveCriticalSection(&streamer->lock);
        return HENKA_ERROR_LIMIT;
    }
    for (index = 0U; index < streamer->observer_capacity; ++index)
    {
        if (streamer->observers[index].id == 0U)
        {
            streamer->observers[index] = *observer;
            ++streamer->observer_count;
            if (streamer->observer_count > streamer->max_observer_count)
            {
                streamer->max_observer_count = streamer->observer_count;
            }
            LeaveCriticalSection(&streamer->lock);
            return HENKA_SUCCESS;
        }
    }
    LeaveCriticalSection(&streamer->lock);
    return HENKA_ERROR_LIMIT;
}

henka_result henka_terrain_streamer_add_observer(
    henka_terrain_streamer* streamer,
    const henka_terrain_stream_observer* observer)
{
    henka_result result = henka_terrain_streamer_store_observer(streamer, observer, false);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    return henka_terrain_streamer_update_observer(streamer, observer);
}

henka_result henka_terrain_streamer_update_observer(
    henka_terrain_streamer* streamer,
    const henka_terrain_stream_observer* observer)
{
    henka_result result = henka_terrain_streamer_store_observer(streamer, observer, true);
    int32_t radius;
    int32_t z;
    int32_t x;
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    henka_terrain_stream_reconcile_residency(streamer);
    henka_terrain_stream_sync_presentation_residency(streamer);
    EnterCriticalSection(&streamer->lock);
    henka_terrain_stream_cancel_stale_observer_requests_locked(streamer);
    radius = (int32_t)observer->cpu_radius_regions;
    for (z = observer->center_region.z - radius; z <= observer->center_region.z + radius; ++z)
    {
        for (x = observer->center_region.x - radius; x <= observer->center_region.x + radius; ++x)
        {
            henka_terrain_region_state state;
            henka_terrain_region_id region_id = {x, z};
            if (!henka_terrain_region_id_is_valid(&streamer->world->desc, region_id))
            {
                continue;
            }
            if (henka_terrain_world_get_region_state(streamer->world, region_id, &state) == HENKA_SUCCESS &&
                state.cpu_resident)
            {
                continue;
            }
            if (henka_terrain_stream_request_exists(streamer, region_id))
            {
                ++streamer->coalesced_request_count;
                continue;
            }
            uint32_t request_index = henka_terrain_stream_find_free_request(streamer);
            henka_result request_result;
            if (request_index >= streamer->desc.max_requests)
            {
                request_result = HENKA_ERROR_LIMIT;
            }
            else
            {
                streamer->requests[request_index] = (henka_terrain_stream_request){
                    true,
                    region_id,
                    true,
                    henka_terrain_stream_region_priority(streamer, region_id),
                    streamer->next_sequence++};
                ++streamer->queued_request_count;
                if (streamer->queued_request_count > streamer->max_queued_request_count)
                {
                    streamer->max_queued_request_count = streamer->queued_request_count;
                }
                request_result = HENKA_SUCCESS;
            }
            if (request_result != HENKA_SUCCESS && request_result != HENKA_ERROR_INVALID_ARGUMENT)
            {
                LeaveCriticalSection(&streamer->lock);
                return request_result;
            }
        }
    }
    WakeConditionVariable(&streamer->condition);
    LeaveCriticalSection(&streamer->lock);
    return HENKA_SUCCESS;
}

henka_result henka_terrain_streamer_remove_observer(
    henka_terrain_streamer* streamer,
    henka_terrain_stream_observer_id observer_id)
{
    uint32_t index;
    if (streamer == NULL || observer_id == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    EnterCriticalSection(&streamer->lock);
    for (index = 0U; index < streamer->observer_capacity; ++index)
    {
        if (streamer->observers[index].id == observer_id)
        {
            streamer->observers[index].id = 0U;
            --streamer->observer_count;
            for (uint32_t request_index = 0U;
                 request_index < streamer->desc.max_requests;
                 ++request_index)
            {
                if (streamer->requests[request_index].active)
                {
                    streamer->requests[request_index].priority =
                        henka_terrain_stream_region_priority(
                            streamer, streamer->requests[request_index].region_id);
                }
            }
            henka_terrain_stream_cancel_stale_observer_requests_locked(streamer);
            henka_terrain_stream_reconcile_residency(streamer);
            LeaveCriticalSection(&streamer->lock);
            return HENKA_SUCCESS;
        }
    }
    LeaveCriticalSection(&streamer->lock);
    return HENKA_ERROR_INVALID_ARGUMENT;
}

henka_result henka_terrain_streamer_pump(
    henka_terrain_streamer* streamer,
    uint32_t max_completions)
{
    uint32_t processed = 0U;
    if (streamer == NULL || max_completions > streamer->desc.max_completions)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    while (processed < max_completions)
    {
        henka_terrain_stream_completion completion = {0};
        uint32_t index;
        EnterCriticalSection(&streamer->lock);
        for (index = 0U; index < streamer->desc.max_completions; ++index)
        {
            if (streamer->completions[index].active)
            {
                completion = streamer->completions[index];
                streamer->completions[index] = (henka_terrain_stream_completion){0};
                --streamer->completion_count;
                break;
            }
        }
        LeaveCriticalSection(&streamer->lock);
        if (index >= streamer->desc.max_completions)
        {
            break;
        }
        if (henka_terrain_stream_completion_is_stale(streamer, &completion))
        {
            EnterCriticalSection(&streamer->lock);
            ++streamer->stale_completion_count;
            LeaveCriticalSection(&streamer->lock);
        }
        else if (completion.result == HENKA_SUCCESS &&
            henka_terrain_world_apply_region_snapshot(
                streamer->world, completion.info, completion.samples,
                streamer->world->layout.samples_per_region) == HENKA_SUCCESS)
        {
            henka_terrain_stream_sync_presentation_residency(streamer);
            EnterCriticalSection(&streamer->lock);
            ++streamer->completed_request_count;
            if (completion.generated)
            {
                ++streamer->generated_region_count;
            }
            LeaveCriticalSection(&streamer->lock);
        }
        else
        {
            EnterCriticalSection(&streamer->lock);
            ++streamer->failed_request_count;
            LeaveCriticalSection(&streamer->lock);
        }
        henka_free(completion.samples);
        ++processed;
    }
    return HENKA_SUCCESS;
}

void henka_terrain_streamer_get_stats(
    const henka_terrain_streamer* streamer,
    henka_terrain_stream_stats* out_stats)
{
    if (out_stats == NULL)
    {
        return;
    }
    *out_stats = (henka_terrain_stream_stats){0};
    if (streamer != NULL)
    {
        EnterCriticalSection((CRITICAL_SECTION*)&streamer->lock);
        *out_stats = (henka_terrain_stream_stats){0};
        out_stats->queued_request_count = streamer->queued_request_count;
        out_stats->active_request_count = streamer->active_request_count;
        out_stats->completion_count = streamer->completion_count;
        out_stats->observer_count = streamer->observer_count;
        out_stats->max_queued_request_count = streamer->max_queued_request_count;
        out_stats->max_active_request_count = streamer->max_active_request_count;
        out_stats->max_completion_count = streamer->max_completion_count;
        out_stats->max_observer_count = streamer->max_observer_count;
        out_stats->coalesced_request_count = streamer->coalesced_request_count;
        out_stats->completed_request_count = streamer->completed_request_count;
        out_stats->failed_request_count = streamer->failed_request_count;
        out_stats->cancelled_request_count = streamer->cancelled_request_count;
        out_stats->dropped_completion_count = streamer->dropped_completion_count;
        out_stats->stale_completion_count = streamer->stale_completion_count;
        out_stats->evicted_region_count = streamer->evicted_region_count;
        out_stats->generated_region_count = streamer->generated_region_count;
        out_stats->generator_failure_count = streamer->generator_failure_count;
        LeaveCriticalSection((CRITICAL_SECTION*)&streamer->lock);
    }
}

#else

henka_terrain_stream_desc henka_terrain_stream_desc_default(void)
{
    return (henka_terrain_stream_desc){16U, 16U, NULL, NULL};
}

henka_result henka_terrain_streamer_create(
    henka_terrain_world* world,
    henka_terrain_storage* storage,
    const henka_terrain_stream_desc* desc,
    henka_terrain_streamer** out_streamer)
{
    (void)world;
    (void)storage;
    (void)desc;
    if (out_streamer != NULL)
    {
        *out_streamer = NULL;
    }
    return HENKA_ERROR_PLATFORM;
}

void henka_terrain_streamer_destroy(henka_terrain_streamer* streamer) { (void)streamer; }
henka_result henka_terrain_streamer_add_observer(henka_terrain_streamer* s, const henka_terrain_stream_observer* o) { (void)s; (void)o; return HENKA_ERROR_PLATFORM; }
henka_result henka_terrain_streamer_update_observer(henka_terrain_streamer* s, const henka_terrain_stream_observer* o) { (void)s; (void)o; return HENKA_ERROR_PLATFORM; }
henka_result henka_terrain_streamer_remove_observer(henka_terrain_streamer* s, henka_terrain_stream_observer_id id) { (void)s; (void)id; return HENKA_ERROR_PLATFORM; }
henka_result henka_terrain_streamer_request_region(henka_terrain_streamer* s, henka_terrain_region_id id) { (void)s; (void)id; return HENKA_ERROR_PLATFORM; }
henka_result henka_terrain_streamer_cancel_region(henka_terrain_streamer* s, henka_terrain_region_id id) { (void)s; (void)id; return HENKA_ERROR_PLATFORM; }
henka_result henka_terrain_streamer_pump(henka_terrain_streamer* s, uint32_t max) { (void)s; (void)max; return HENKA_ERROR_PLATFORM; }
void henka_terrain_streamer_get_stats(const henka_terrain_streamer* s, henka_terrain_stream_stats* out) { (void)s; if (out != NULL) { *out = (henka_terrain_stream_stats){0}; } }

#endif
