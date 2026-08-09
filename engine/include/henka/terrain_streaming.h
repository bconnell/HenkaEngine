#ifndef HENKA_TERRAIN_STREAMING_H
#define HENKA_TERRAIN_STREAMING_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/terrain_storage.h>

#define HENKA_TERRAIN_STREAM_MAX_REQUESTS 256U
#define HENKA_TERRAIN_STREAM_MAX_COMPLETIONS 256U

typedef struct henka_terrain_streamer henka_terrain_streamer;
typedef uint32_t henka_terrain_stream_observer_id;

typedef struct henka_terrain_stream_desc
{
    uint32_t max_requests;
    uint32_t max_completions;
} henka_terrain_stream_desc;

typedef struct henka_terrain_stream_observer
{
    henka_terrain_stream_observer_id id;
    henka_terrain_region_id center_region;
    uint32_t cpu_radius_regions;
    uint32_t physics_radius_regions;
    uint32_t render_radius_regions;
    /* Zero preserves the non-hysteretic policy and uses cpu_radius_regions. */
    uint32_t cpu_unload_radius_regions;
} henka_terrain_stream_observer;

typedef struct henka_terrain_stream_stats
{
    uint32_t queued_request_count;
    uint32_t active_request_count;
    uint32_t completion_count;
    uint32_t observer_count;
    uint32_t max_queued_request_count;
    uint32_t max_active_request_count;
    uint32_t max_completion_count;
    uint32_t max_observer_count;
    uint64_t coalesced_request_count;
    uint64_t completed_request_count;
    uint64_t failed_request_count;
    uint64_t cancelled_request_count;
    uint64_t dropped_completion_count;
    uint64_t evicted_region_count;
} henka_terrain_stream_stats;

henka_terrain_stream_desc henka_terrain_stream_desc_default(void);
henka_result henka_terrain_streamer_create(
    henka_terrain_world* world,
    henka_terrain_storage* storage,
    const henka_terrain_stream_desc* desc,
    henka_terrain_streamer** out_streamer);
void henka_terrain_streamer_destroy(henka_terrain_streamer* streamer);
henka_result henka_terrain_streamer_add_observer(
    henka_terrain_streamer* streamer,
    const henka_terrain_stream_observer* observer);
henka_result henka_terrain_streamer_update_observer(
    henka_terrain_streamer* streamer,
    const henka_terrain_stream_observer* observer);
henka_result henka_terrain_streamer_remove_observer(
    henka_terrain_streamer* streamer,
    henka_terrain_stream_observer_id observer_id);
henka_result henka_terrain_streamer_request_region(
    henka_terrain_streamer* streamer,
    henka_terrain_region_id region_id);
henka_result henka_terrain_streamer_cancel_region(
    henka_terrain_streamer* streamer,
    henka_terrain_region_id region_id);
henka_result henka_terrain_streamer_pump(
    henka_terrain_streamer* streamer,
    uint32_t max_completions);
void henka_terrain_streamer_get_stats(
    const henka_terrain_streamer* streamer,
    henka_terrain_stream_stats* out_stats);

#endif
