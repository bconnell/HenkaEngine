#ifndef HENKA_TERRAIN_EDIT_HISTORY_H
#define HENKA_TERRAIN_EDIT_HISTORY_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/terrain_edit.h>

#define HENKA_TERRAIN_EDIT_HISTORY_DEFAULT_ENTRIES UINT32_C(16)
#define HENKA_TERRAIN_EDIT_HISTORY_DEFAULT_BYTES (UINT64_C(32) * UINT64_C(1024) * UINT64_C(1024))

typedef struct henka_terrain_edit_history henka_terrain_edit_history;

typedef struct henka_terrain_edit_history_desc
{
    uint32_t max_entries;
    uint64_t max_bytes;
} henka_terrain_edit_history_desc;

typedef struct henka_terrain_edit_history_stats
{
    uint32_t entry_count;
    uint32_t applied_entry_count;
    uint32_t max_entries;
    uint64_t bytes;
    uint64_t max_bytes;
    bool can_undo;
    bool can_redo;
} henka_terrain_edit_history_stats;

henka_terrain_edit_history_desc henka_terrain_edit_history_desc_default(void);
henka_result henka_terrain_edit_history_create(
    henka_terrain_world* world,
    const henka_terrain_edit_history_desc* desc,
    henka_terrain_edit_history** out_history);
void henka_terrain_edit_history_destroy(henka_terrain_edit_history* history);

/* Applies one validated edit and records resident-region snapshots before and
 * after the mutation. If capture or the edit fails, the world is unchanged. */
henka_result henka_terrain_edit_history_apply(
    henka_terrain_edit_history* history,
    const henka_terrain_edit_command* command,
    henka_terrain_revision transaction_id);
henka_result henka_terrain_edit_history_undo(
    henka_terrain_edit_history* history,
    henka_terrain_edit_command* out_command);
henka_result henka_terrain_edit_history_redo(
    henka_terrain_edit_history* history,
    henka_terrain_edit_command* out_command);
void henka_terrain_edit_history_get_stats(
    const henka_terrain_edit_history* history,
    henka_terrain_edit_history_stats* out_stats);

#endif
