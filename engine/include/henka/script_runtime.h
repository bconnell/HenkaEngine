#ifndef HENKA_SCRIPT_RUNTIME_H
#define HENKA_SCRIPT_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>
#include <henka/script.h>

#define HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS 128U
#define HENKA_SCRIPT_DEFAULT_BEHAVIOR_INSTRUCTION_BUDGET 1024U
#define HENKA_SCRIPT_MAX_BEHAVIOR_INSTRUCTION_BUDGET 1000000U

typedef struct henka_script_behavior_runtime henka_script_behavior_runtime;
typedef uint64_t henka_script_behavior_handle;

#define HENKA_INVALID_SCRIPT_BEHAVIOR_HANDLE ((henka_script_behavior_handle)0)

typedef enum henka_script_lifecycle_event
{
    HENKA_SCRIPT_LIFECYCLE_CREATE = 0,
    HENKA_SCRIPT_LIFECYCLE_START,
    HENKA_SCRIPT_LIFECYCLE_UPDATE,
    HENKA_SCRIPT_LIFECYCLE_STOP,
    /* Keep EVENT at its published value for serialized/runtime compatibility. */
    HENKA_SCRIPT_LIFECYCLE_EVENT,
    HENKA_SCRIPT_LIFECYCLE_FIXED_UPDATE,
    HENKA_SCRIPT_LIFECYCLE_INTERACT,
    HENKA_SCRIPT_LIFECYCLE_COLLISION_ENTER,
    HENKA_SCRIPT_LIFECYCLE_COLLISION_STAY,
    HENKA_SCRIPT_LIFECYCLE_COLLISION_EXIT,
    HENKA_SCRIPT_LIFECYCLE_TRIGGER_ENTER,
    HENKA_SCRIPT_LIFECYCLE_TRIGGER_STAY,
    HENKA_SCRIPT_LIFECYCLE_TRIGGER_EXIT,
    HENKA_SCRIPT_LIFECYCLE_DESTROY
} henka_script_lifecycle_event;

/* Includes the published EVENT slot so backend callback storage can index by
 * the enum directly without a second ordered lifecycle table. */
#define HENKA_SCRIPT_LIFECYCLE_SLOT_COUNT \
    (HENKA_SCRIPT_LIFECYCLE_DESTROY + 1U)

#define HENKA_SCRIPT_LIFECYCLE_SCHEMA_VERSION UINT32_C(1)
#define HENKA_SCRIPT_LIFECYCLE_MAX_PARAMETERS 2U

/* The immutable lifecycle registry is the language-neutral source of truth
 * for callback names and bounded signal argument shapes. Backends must use
 * this contract rather than maintaining independent callback-name tables. */
typedef struct henka_script_lifecycle_descriptor
{
    henka_script_lifecycle_event event;
    const char* name;
    uint8_t parameter_count;
    henka_script_api_value_type parameters[HENKA_SCRIPT_LIFECYCLE_MAX_PARAMETERS];
} henka_script_lifecycle_descriptor;

typedef enum henka_script_behavior_state
{
    HENKA_SCRIPT_BEHAVIOR_PENDING = 0,
    HENKA_SCRIPT_BEHAVIOR_CREATED,
    HENKA_SCRIPT_BEHAVIOR_STARTED,
    HENKA_SCRIPT_BEHAVIOR_STOPPED,
    HENKA_SCRIPT_BEHAVIOR_FAULTED,
    /* Destroy has run; Stop remains available for final runtime teardown. */
    HENKA_SCRIPT_BEHAVIOR_DESTROYED
} henka_script_behavior_state;

typedef enum henka_script_behavior_execution_result
{
    HENKA_SCRIPT_BEHAVIOR_EXECUTED = 0,
    HENKA_SCRIPT_BEHAVIOR_SKIPPED_DISABLED,
    HENKA_SCRIPT_BEHAVIOR_SKIPPED_STOPPED,
    HENKA_SCRIPT_BEHAVIOR_INVALID_STATE,
    HENKA_SCRIPT_BEHAVIOR_UNBOUND,
    HENKA_SCRIPT_BEHAVIOR_CALLBACK_FAILED,
    HENKA_SCRIPT_BEHAVIOR_BUDGET_EXHAUSTED
} henka_script_behavior_execution_result;

typedef enum henka_script_behavior_callback_result
{
    HENKA_SCRIPT_CALLBACK_COMPLETED = 0,
    HENKA_SCRIPT_CALLBACK_FAILED,
    HENKA_SCRIPT_CALLBACK_BUDGET_EXHAUSTED
} henka_script_behavior_callback_result;

typedef struct henka_script_behavior_context
{
    henka_script_behavior_handle behavior;
    uint64_t entity_id;
    henka_script_language language;
    henka_script_lifecycle_event event;
    float delta_seconds;
    uint64_t frame_index;
    uint32_t instruction_budget;
    henka_script_host* host;
    uint64_t behavior_id;
    uint32_t event_id;
    uint64_t event_source_entity;
    uint64_t event_other_entity;
    uint32_t event_type;
} henka_script_behavior_context;

/* The callback, user_data, and host are borrowed. The owner must keep them
 * valid until the behavior is removed or rebound. Callbacks run synchronously
 * on the dispatching thread and must not re-enter or destroy the runtime. */
typedef henka_script_behavior_callback_result (*henka_script_behavior_callback)(
    const henka_script_behavior_context* context,
    void* user_data,
    uint32_t* out_instructions_used);

typedef struct henka_script_behavior_desc
{
    uint64_t entity_id;
    henka_script_language language;
    bool enabled;
    uint32_t instruction_budget;
    henka_script_behavior_callback callback;
    void* user_data;
    henka_script_host* host;
    uint64_t behavior_id;
} henka_script_behavior_desc;

typedef struct henka_script_behavior_snapshot
{
    henka_script_behavior_handle behavior;
    uint64_t entity_id;
    henka_script_language language;
    bool enabled;
    bool bound;
    henka_script_behavior_state state;
    uint32_t instruction_budget;
    uint32_t failure_count;
    uint64_t behavior_id;
} henka_script_behavior_snapshot;

typedef struct henka_script_behavior_report
{
    henka_script_behavior_execution_result result;
    henka_script_behavior_state state;
    uint32_t instructions_used;
    uint32_t failure_count;
    bool state_changed;
} henka_script_behavior_report;

typedef struct henka_script_behavior_batch_report
{
    henka_script_lifecycle_event event;
    size_t attempted;
    size_t executed;
    size_t skipped;
    size_t failed;
    henka_result first_error;
} henka_script_behavior_batch_report;

henka_result henka_script_lifecycle_schema_get(
    const henka_script_lifecycle_descriptor** out_descriptors,
    size_t* out_count);
henka_result henka_script_lifecycle_schema_find(
    henka_script_lifecycle_event event,
    const henka_script_lifecycle_descriptor** out_descriptor);

henka_result henka_script_behavior_runtime_create(
    henka_script_behavior_runtime** out_runtime);
void henka_script_behavior_runtime_destroy(henka_script_behavior_runtime* runtime);

henka_result henka_script_behavior_runtime_add(
    henka_script_behavior_runtime* runtime,
    const henka_script_behavior_desc* desc,
    henka_script_behavior_handle* out_behavior);
henka_result henka_script_behavior_runtime_bind(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    henka_script_behavior_callback callback,
    void* user_data);
/* Replaces the callback/backend metadata for an existing generation-checked
 * behavior slot without resetting its lifecycle state or failure count. The
 * persistent entity and behavior IDs must match the existing slot. */
henka_result henka_script_behavior_runtime_rebind(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    const henka_script_behavior_desc* desc);
henka_result henka_script_behavior_runtime_remove(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior);
henka_result henka_script_behavior_runtime_set_enabled(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    bool enabled);
henka_result henka_script_behavior_runtime_reset(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior);
henka_result henka_script_behavior_runtime_get(
    const henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    henka_script_behavior_snapshot* out_snapshot);

henka_result henka_script_behavior_runtime_dispatch(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    henka_script_lifecycle_event event,
    float delta_seconds,
    uint64_t frame_index,
    henka_script_behavior_report* out_report);
henka_result henka_script_behavior_runtime_dispatch_all(
    henka_script_behavior_runtime* runtime,
    henka_script_lifecycle_event event,
    float delta_seconds,
    uint64_t frame_index,
    henka_script_behavior_batch_report* out_report);
henka_result henka_script_behavior_runtime_dispatch_event(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    uint32_t event_id,
    uint64_t source_entity,
    uint64_t frame_index,
    henka_script_behavior_report* out_report);
henka_result henka_script_behavior_runtime_dispatch_event_all(
    henka_script_behavior_runtime* runtime,
    uint32_t event_id,
    uint64_t source_entity,
    uint64_t frame_index,
    henka_script_behavior_batch_report* out_report);
/* Dispatches a typed signal only to behaviors attached to entity_id. The
 * entity ID is the persistent authored identity, never a runtime pointer or
 * body ID. event_id/source_entity are used for custom signals; other_entity
 * and event_type carry bounded contact/interaction metadata. */
henka_result henka_script_behavior_runtime_dispatch_signal_for_entity(
    henka_script_behavior_runtime* runtime,
    uint64_t entity_id,
    henka_script_lifecycle_event event,
    float delta_seconds,
    uint64_t frame_index,
    uint32_t event_id,
    uint64_t source_entity,
    uint64_t other_entity,
    uint32_t event_type,
    henka_script_behavior_batch_report* out_report);

size_t henka_script_behavior_runtime_get_count(
    const henka_script_behavior_runtime* runtime);

#endif
