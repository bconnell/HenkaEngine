#ifndef HENKA_SCENE_BEHAVIOR_RUNTIME_H
#define HENKA_SCENE_BEHAVIOR_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>
#include <henka/scene_document.h>
#include <henka/script_runtime.h>

typedef struct henka_scene_behavior_runtime henka_scene_behavior_runtime;

/* Builds an owned, bounded behavior runtime from persisted Scene Document
 * attachments. The document and project root are borrowed only during
 * creation; no renderer or authoring-state pointer is retained. The optional
 * host is borrowed until the returned runtime is destroyed. */
henka_result henka_scene_behavior_runtime_create(
    const henka_scene_document* document,
    const char* project_root,
    uint32_t instruction_budget,
    henka_scene_behavior_runtime** out_runtime);
henka_result henka_scene_behavior_runtime_create_with_host(
    const henka_scene_document* document,
    const char* project_root,
    uint32_t instruction_budget,
    henka_script_host* host,
    henka_scene_behavior_runtime** out_runtime);
void henka_scene_behavior_runtime_destroy(
    henka_scene_behavior_runtime* runtime);

henka_result henka_scene_behavior_runtime_dispatch(
    henka_scene_behavior_runtime* runtime,
    henka_script_lifecycle_event event,
    float delta_seconds,
    uint64_t frame_index,
    henka_script_behavior_batch_report* out_report);
/* Dispatches the bounded event queue entries that existed when this call
 * began. Events emitted by handlers remain queued for the next drain, which
 * prevents same-batch event recursion from becoming unbounded. */
henka_result henka_scene_behavior_runtime_dispatch_events(
    henka_scene_behavior_runtime* runtime,
    henka_script_behavior_batch_report* out_report);
henka_result henka_scene_behavior_runtime_dispatch_signal_for_entity(
    henka_scene_behavior_runtime* runtime,
    uint64_t entity_id,
    henka_script_lifecycle_event event,
    float delta_seconds,
    uint64_t frame_index,
    uint32_t event_id,
    uint64_t source_entity,
    uint64_t other_entity,
    uint32_t event_type,
    henka_script_behavior_batch_report* out_report);

size_t henka_scene_behavior_runtime_get_behavior_count(
    const henka_scene_behavior_runtime* runtime);

#endif
