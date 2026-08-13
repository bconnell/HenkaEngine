#include <string.h>

#include <henka/memory.h>
#include <henka/terrain_edit_history.h>

#include "../core/memory_internal.h"

typedef struct henka_terrain_history_snapshot
{
    henka_terrain_region_storage_info info;
    henka_terrain_region_state state;
    henka_terrain_sample* samples;
} henka_terrain_history_snapshot;

typedef struct henka_terrain_history_entry
{
    henka_terrain_edit_command command;
    henka_terrain_history_snapshot* before;
    henka_terrain_history_snapshot* after;
    uint32_t region_count;
    uint64_t bytes;
} henka_terrain_history_entry;

struct henka_terrain_edit_history
{
    henka_terrain_world* world;
    henka_terrain_history_entry* entries;
    uint32_t max_entries;
    uint32_t entry_count;
    uint32_t applied_entry_count;
    uint64_t bytes;
    uint64_t max_bytes;
};

henka_terrain_edit_history_desc henka_terrain_edit_history_desc_default(void)
{
    return (henka_terrain_edit_history_desc){
        HENKA_TERRAIN_EDIT_HISTORY_DEFAULT_ENTRIES,
        HENKA_TERRAIN_EDIT_HISTORY_DEFAULT_BYTES};
}

static void henka_terrain_history_free_snapshots(
    henka_terrain_history_snapshot* snapshots,
    uint32_t count)
{
    uint32_t index;
    if (snapshots == NULL)
    {
        return;
    }
    for (index = 0U; index < count; ++index)
    {
        henka_free(snapshots[index].samples);
    }
    henka_free(snapshots);
}

static void henka_terrain_history_free_entry(henka_terrain_history_entry* entry)
{
    if (entry == NULL)
    {
        return;
    }
    henka_terrain_history_free_snapshots(entry->before, entry->region_count);
    henka_terrain_history_free_snapshots(entry->after, entry->region_count);
    *entry = (henka_terrain_history_entry){0};
}

static henka_result henka_terrain_history_capture(
    henka_terrain_world* world,
    const henka_terrain_edit_command* command,
    henka_terrain_history_snapshot** out_snapshots,
    uint32_t* out_count,
    uint64_t* out_bytes)
{
    henka_terrain_region_id regions[HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS];
    henka_terrain_layout layout;
    uint32_t region_count = HENKA_TERRAIN_EDIT_MAX_AFFECTED_REGIONS;
    henka_terrain_history_snapshot* snapshots = NULL;
    uint32_t index;
    uint64_t sample_bytes;
    uint64_t total_bytes;
    if (out_snapshots != NULL) *out_snapshots = NULL;
    if (out_count != NULL) *out_count = 0U;
    if (out_bytes != NULL) *out_bytes = 0U;
    {
        henka_terrain_world_desc desc;
        if (world == NULL || command == NULL || out_snapshots == NULL ||
            out_count == NULL || out_bytes == NULL ||
            henka_terrain_world_get_desc(world, &desc) != HENKA_SUCCESS ||
            henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS ||
            henka_terrain_edit_get_affected_regions(world, command, regions, &region_count) != HENKA_SUCCESS ||
            region_count == 0U)
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    sample_bytes = (uint64_t)layout.samples_per_region * sizeof(henka_terrain_sample);
    if (layout.samples_per_region == 0U || sample_bytes > UINT64_MAX / region_count ||
        (uint64_t)region_count * sizeof(*snapshots) > UINT64_MAX - sample_bytes * region_count)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    total_bytes = (uint64_t)region_count * sizeof(*snapshots) + sample_bytes * region_count;
    snapshots = henka_calloc(region_count, sizeof(*snapshots));
    if (snapshots == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < region_count; ++index)
    {
        const henka_terrain_sample* samples = NULL;
        size_t sample_count = 0U;
        henka_terrain_region_state state;
        if (henka_terrain_world_get_region_state(world, regions[index], &state) != HENKA_SUCCESS ||
            !state.cpu_resident ||
            henka_terrain_world_get_region_samples(world, regions[index], &samples, &sample_count) != HENKA_SUCCESS ||
            sample_count != layout.samples_per_region)
        {
            henka_terrain_history_free_snapshots(snapshots, region_count);
            return HENKA_ERROR_LIMIT;
        }
        snapshots[index].samples = henka_malloc(sample_count * sizeof(*samples));
        if (snapshots[index].samples == NULL)
        {
            henka_terrain_history_free_snapshots(snapshots, region_count);
            return HENKA_ERROR_OUT_OF_MEMORY;
        }
        snapshots[index].info = (henka_terrain_region_storage_info){
            regions[index], state.revision, state.generation};
        snapshots[index].state = state;
        memcpy(snapshots[index].samples, samples, sample_count * sizeof(*samples));
    }
    *out_snapshots = snapshots;
    *out_count = region_count;
    *out_bytes = total_bytes;
    return HENKA_SUCCESS;
}

static henka_result henka_terrain_history_restore(
    henka_terrain_world* world,
    const henka_terrain_history_snapshot* snapshots,
    uint32_t count)
{
    henka_terrain_world_desc desc;
    henka_terrain_layout layout;
    uint32_t index;
    if (world == NULL || snapshots == NULL || count == 0U ||
        henka_terrain_world_get_desc(world, &desc) != HENKA_SUCCESS ||
        henka_terrain_world_desc_get_layout(&desc, &layout) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < count; ++index)
    {
        henka_result result = henka_terrain_world_apply_region_snapshot(
            world,
            snapshots[index].info,
            snapshots[index].samples,
            layout.samples_per_region);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    for (index = 0U; index < count; ++index)
    {
        henka_result result = henka_terrain_world_set_region_residency(
            world,
            snapshots[index].state.id,
            snapshots[index].state.physics_resident,
            snapshots[index].state.render_resident,
            false);
        if (result == HENKA_SUCCESS)
        {
            result = henka_terrain_world_set_region_revision(
                world,
                snapshots[index].state.id,
                snapshots[index].state.revision,
                snapshots[index].state.generation,
                snapshots[index].state.dirty);
        }
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
    }
    return HENKA_SUCCESS;
}

henka_result henka_terrain_edit_history_create(
    henka_terrain_world* world,
    const henka_terrain_edit_history_desc* desc,
    henka_terrain_edit_history** out_history)
{
    henka_terrain_edit_history_desc defaults = henka_terrain_edit_history_desc_default();
    henka_terrain_edit_history_desc effective;
    henka_terrain_edit_history* history;
    if (out_history != NULL) *out_history = NULL;
    if (world == NULL || out_history == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    effective = desc == NULL ? defaults : *desc;
    if (effective.max_entries == 0U || effective.max_entries > 64U || effective.max_bytes == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    history = henka_calloc(1U, sizeof(*history));
    if (history == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    history->entries = henka_calloc(effective.max_entries, sizeof(*history->entries));
    if (history->entries == NULL)
    {
        henka_free(history);
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    history->world = world;
    history->max_entries = effective.max_entries;
    history->max_bytes = effective.max_bytes;
    *out_history = history;
    return HENKA_SUCCESS;
}

void henka_terrain_edit_history_destroy(henka_terrain_edit_history* history)
{
    uint32_t index;
    if (history == NULL) return;
    for (index = 0U; index < history->entry_count; ++index)
    {
        henka_terrain_history_free_entry(&history->entries[index]);
    }
    henka_free(history->entries);
    henka_free(history);
}

static void henka_terrain_history_clear_redo(henka_terrain_edit_history* history)
{
    uint32_t index;
    if (history == NULL) return;
    for (index = history->applied_entry_count; index < history->entry_count; ++index)
    {
        history->bytes -= history->entries[index].bytes;
        henka_terrain_history_free_entry(&history->entries[index]);
    }
    history->entry_count = history->applied_entry_count;
}

henka_result henka_terrain_edit_history_apply(
    henka_terrain_edit_history* history,
    const henka_terrain_edit_command* command,
    henka_terrain_revision transaction_id)
{
    henka_terrain_history_entry candidate = {0};
    henka_result result;
    if (history == NULL || command == NULL ||
        henka_terrain_edit_command_validate(history->world, command) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_terrain_history_capture(
        history->world, command, &candidate.before, &candidate.region_count, &candidate.bytes);
    if (result != HENKA_SUCCESS) return result;
    result = henka_terrain_world_apply_edit(history->world, command, transaction_id);
    if (result == HENKA_SUCCESS)
    {
        uint64_t after_bytes = 0U;
        uint32_t after_count = 0U;
        result = henka_terrain_history_capture(
            history->world, command, &candidate.after, &after_count, &after_bytes);
        if (result == HENKA_SUCCESS && after_count != candidate.region_count)
        {
            result = HENKA_ERROR_UNKNOWN;
        }
        candidate.bytes += after_bytes;
    }
    if (result != HENKA_SUCCESS || candidate.bytes > history->max_bytes)
    {
        if (candidate.before != NULL)
        {
            (void)henka_terrain_history_restore(
                history->world, candidate.before, candidate.region_count);
        }
        henka_terrain_history_free_entry(&candidate);
        return result == HENKA_SUCCESS ? HENKA_ERROR_LIMIT : result;
    }
    candidate.command = *command;
    henka_terrain_history_clear_redo(history);
    while (history->entry_count > 0U &&
        history->bytes > history->max_bytes - candidate.bytes)
    {
        history->bytes -= history->entries[0].bytes;
        henka_terrain_history_free_entry(&history->entries[0]);
        memmove(history->entries, history->entries + 1U,
            (history->entry_count - 1U) * sizeof(*history->entries));
        --history->entry_count;
        if (history->applied_entry_count > 0U)
        {
            --history->applied_entry_count;
        }
    }
    if (history->entry_count == history->max_entries)
    {
        history->bytes -= history->entries[0].bytes;
        henka_terrain_history_free_entry(&history->entries[0]);
        memmove(history->entries, history->entries + 1U,
            (history->entry_count - 1U) * sizeof(*history->entries));
        --history->entry_count;
        --history->applied_entry_count;
    }
    history->entries[history->entry_count++] = candidate;
    ++history->applied_entry_count;
    history->bytes += candidate.bytes;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_edit_history_undo(
    henka_terrain_edit_history* history,
    henka_terrain_edit_command* out_command)
{
    henka_terrain_history_entry* entry;
    henka_result result;
    if (history == NULL || history->applied_entry_count == 0U)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    entry = &history->entries[history->applied_entry_count - 1U];
    result = henka_terrain_history_restore(history->world, entry->before, entry->region_count);
    if (result != HENKA_SUCCESS) return result;
    --history->applied_entry_count;
    if (out_command != NULL) *out_command = entry->command;
    return HENKA_SUCCESS;
}

henka_result henka_terrain_edit_history_redo(
    henka_terrain_edit_history* history,
    henka_terrain_edit_command* out_command)
{
    henka_terrain_history_entry* entry;
    henka_result result;
    if (history == NULL || history->applied_entry_count >= history->entry_count)
    {
        return HENKA_ERROR_UNKNOWN;
    }
    entry = &history->entries[history->applied_entry_count];
    result = henka_terrain_history_restore(history->world, entry->after, entry->region_count);
    if (result != HENKA_SUCCESS) return result;
    ++history->applied_entry_count;
    if (out_command != NULL) *out_command = entry->command;
    return HENKA_SUCCESS;
}

void henka_terrain_edit_history_get_stats(
    const henka_terrain_edit_history* history,
    henka_terrain_edit_history_stats* out_stats)
{
    if (out_stats == NULL) return;
    *out_stats = (henka_terrain_edit_history_stats){0};
    if (history == NULL) return;
    *out_stats = (henka_terrain_edit_history_stats){
        history->entry_count,
        history->applied_entry_count,
        history->max_entries,
        history->bytes,
        history->max_bytes,
        history->applied_entry_count > 0U,
        history->applied_entry_count < history->entry_count};
}
