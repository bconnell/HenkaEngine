#ifndef HENKA_TERRAIN_RENDER_H
#define HENKA_TERRAIN_RENDER_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/engine.h>
#include <henka/math.h>
#include <henka/scene.h>
#include <henka/terrain.h>
#include <henka/terrain_edit.h>
#include <henka/terrain_mesh.h>

typedef struct henka_terrain_render_runtime henka_terrain_render_runtime;

typedef struct henka_terrain_render_desc
{
    /* The runtime owns these bounded queues and resident slots. */
    uint32_t max_resident_chunks;
    uint32_t max_pending_requests;
    /* Strictly increasing squared-distance bands are not required; distances are meters. */
    float lod_max_distances[HENKA_TERRAIN_MESH_MAX_LOD_LEVEL + 1U];
    /* Fraction of a LOD boundary retained when moving away or toward it. */
    float lod_hysteresis;
    /* Texture/shader handles are borrowed and must outlive the runtime and scene. */
    henka_material material;
} henka_terrain_render_desc;

typedef struct henka_terrain_render_stats
{
    uint32_t pending_requests;
    uint32_t resident_chunks;
    uint32_t visible_chunks;
    uint32_t max_pending_requests;
    uint32_t max_resident_chunks;
    uint32_t max_visible_chunks;
    uint32_t lod_counts[HENKA_TERRAIN_MESH_MAX_LOD_LEVEL + 1U];
    uint64_t queued_requests;
    uint64_t coalesced_requests;
    uint64_t rebuilt_chunks;
    uint64_t transition_rebuilds;
    uint64_t fallback_skirt_chunks;
    uint64_t failed_rebuilds;
    uint64_t dropped_requests;
    /* Exact owner allocations and uploaded Terrain resources. */
    uint64_t runtime_cpu_bytes;
    uint64_t gpu_vertex_bytes;
    uint64_t gpu_index_bytes;
    uint64_t material_gpu_bytes;
    /* Unique borrowed layer textures inspected for material stats. */
    uint32_t material_texture_count;
    /* Resident slots whose source revision/dependency identity was requeued. */
    uint64_t dirty_refresh_requests;
} henka_terrain_render_stats;

typedef struct henka_terrain_render_chunk_info
{
    henka_terrain_chunk_id chunk_id;
    henka_entity entity;
    henka_mesh* mesh;
    henka_terrain_revision revision;
    henka_terrain_generation generation;
    uint32_t lod_level;
    uint32_t edge_transition_mask;
    uint32_t fallback_skirt_mask;
    bool resident;
    bool visible;
    bool queued;
} henka_terrain_render_chunk_info;

henka_terrain_render_desc henka_terrain_render_desc_default(void);

/*
 * Creates a client-only owner. The engine and scene are borrowed and must
 * outlive the runtime. The terrain world is also borrowed; it is never
 * destroyed or mutated by this owner. All scene entities and meshes created
 * by the runtime are destroyed before the runtime returns from destroy.
 */
henka_result henka_terrain_render_runtime_create(
    henka_engine* engine,
    henka_scene* scene,
    const henka_terrain_world* world,
    const henka_terrain_render_desc* desc,
    henka_terrain_render_runtime** out_runtime);
void henka_terrain_render_runtime_destroy(henka_terrain_render_runtime* runtime);

/* Queues a chunk for a transactional visual build or revision refresh. */
henka_result henka_terrain_render_runtime_request_chunk(
    henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id,
    uint32_t lod_level);

/*
 * Requeues resident chunks covered by an accepted edit and the one-chunk
 * dependency border required for finite-difference normals and transitions.
 * Nonresident chunks are not admitted by this call; observer synchronization
 * remains responsible for the bounded working set.
 */
henka_result henka_terrain_render_runtime_request_edit(
    henka_terrain_render_runtime* runtime,
    const henka_terrain_edit_command* command);

/*
 * Requeues resident slots whose region revision, generation, or 3x3 source
 * dependency identity changed. Callers may use this after local/remote edits,
 * snapshot replacement, or transactional reload before pumping replacements.
 * Nonresident chunks remain owned by observer synchronization.
 */
henka_result henka_terrain_render_runtime_refresh_dirty(
    henka_terrain_render_runtime* runtime);

henka_result henka_terrain_render_runtime_remove_chunk(
    henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id);

/*
 * Synchronizes the bounded graphical working set from render-resident world
 * regions, selects LOD, and applies deterministic adjacent-chunk clamping.
 * The renderer performs frustum culling from the scene bounds.
 */
henka_result henka_terrain_render_runtime_update_observer(
    henka_terrain_render_runtime* runtime,
    henka_vec3 observer_position);

/* Performs at most max_rebuilds GPU replacements; zero performs no work. */
henka_result henka_terrain_render_runtime_pump(
    henka_terrain_render_runtime* runtime,
    uint32_t max_rebuilds);

henka_result henka_terrain_render_runtime_get_chunk(
    const henka_terrain_render_runtime* runtime,
    henka_terrain_chunk_id chunk_id,
    henka_terrain_render_chunk_info* out_info);
henka_result henka_terrain_render_runtime_get_stats(
    const henka_terrain_render_runtime* runtime,
    henka_terrain_render_stats* out_stats);

#endif
