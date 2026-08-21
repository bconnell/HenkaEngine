#include <henka/script_runtime.h>

#include <math.h>
#include <string.h>

#include <henka/memory.h>

typedef struct henka_script_behavior_slot
{
    uint32_t generation;
    bool occupied;
    uint64_t entity_id;
    uint64_t behavior_id;
    henka_script_language language;
    bool enabled;
    uint32_t instruction_budget;
    henka_script_behavior_state state;
    uint32_t failure_count;
    henka_script_behavior_callback callback;
    void* user_data;
    henka_script_host* host;
} henka_script_behavior_slot;

struct henka_script_behavior_runtime
{
    size_t count;
    bool dispatching;
    henka_script_behavior_slot slots[HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS];
};

static const henka_script_lifecycle_descriptor g_lifecycle_schema[] =
{
    {HENKA_SCRIPT_LIFECYCLE_CREATE, "OnCreate", 0U, {HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}},
    {HENKA_SCRIPT_LIFECYCLE_START, "OnStart", 0U, {HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}},
    {HENKA_SCRIPT_LIFECYCLE_UPDATE, "OnUpdate", 0U, {HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}},
    {HENKA_SCRIPT_LIFECYCLE_STOP, "OnStop", 0U, {HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}},
    {HENKA_SCRIPT_LIFECYCLE_EVENT, "OnEvent", 2U, {HENKA_SCRIPT_API_VALUE_EVENT_ID, HENKA_SCRIPT_API_VALUE_ENTITY}},
    {HENKA_SCRIPT_LIFECYCLE_FIXED_UPDATE, "OnFixedUpdate", 0U, {HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}},
    {HENKA_SCRIPT_LIFECYCLE_INTERACT, "OnInteract", 2U, {HENKA_SCRIPT_API_VALUE_ENTITY, HENKA_SCRIPT_API_VALUE_I32}},
    {HENKA_SCRIPT_LIFECYCLE_COLLISION_ENTER, "OnCollisionEnter", 2U, {HENKA_SCRIPT_API_VALUE_ENTITY, HENKA_SCRIPT_API_VALUE_I32}},
    {HENKA_SCRIPT_LIFECYCLE_COLLISION_STAY, "OnCollisionStay", 2U, {HENKA_SCRIPT_API_VALUE_ENTITY, HENKA_SCRIPT_API_VALUE_I32}},
    {HENKA_SCRIPT_LIFECYCLE_COLLISION_EXIT, "OnCollisionExit", 2U, {HENKA_SCRIPT_API_VALUE_ENTITY, HENKA_SCRIPT_API_VALUE_I32}},
    {HENKA_SCRIPT_LIFECYCLE_TRIGGER_ENTER, "OnTriggerEnter", 2U, {HENKA_SCRIPT_API_VALUE_ENTITY, HENKA_SCRIPT_API_VALUE_I32}},
    {HENKA_SCRIPT_LIFECYCLE_TRIGGER_STAY, "OnTriggerStay", 2U, {HENKA_SCRIPT_API_VALUE_ENTITY, HENKA_SCRIPT_API_VALUE_I32}},
    {HENKA_SCRIPT_LIFECYCLE_TRIGGER_EXIT, "OnTriggerExit", 2U, {HENKA_SCRIPT_API_VALUE_ENTITY, HENKA_SCRIPT_API_VALUE_I32}},
    {HENKA_SCRIPT_LIFECYCLE_DESTROY, "OnDestroy", 0U, {HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}}
};

static const size_t g_lifecycle_schema_count =
    sizeof(g_lifecycle_schema) / sizeof(g_lifecycle_schema[0]);

static bool henka_script_lifecycle_schema_is_valid(void)
{
    size_t index;
    for (index = 0U; index < g_lifecycle_schema_count; ++index)
    {
        size_t compare_index;
        if (g_lifecycle_schema[index].name == NULL ||
            g_lifecycle_schema[index].parameter_count > HENKA_SCRIPT_LIFECYCLE_MAX_PARAMETERS)
        {
            return false;
        }
        for (compare_index = index + 1U; compare_index < g_lifecycle_schema_count; ++compare_index)
        {
            if (g_lifecycle_schema[index].event == g_lifecycle_schema[compare_index].event ||
                strcmp(g_lifecycle_schema[index].name, g_lifecycle_schema[compare_index].name) == 0)
            {
                return false;
            }
        }
    }
    return true;
}

henka_result henka_script_lifecycle_schema_get(
    const henka_script_lifecycle_descriptor** out_descriptors,
    size_t* out_count)
{
    if (out_descriptors == NULL || out_count == NULL ||
        !henka_script_lifecycle_schema_is_valid())
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_descriptors = g_lifecycle_schema;
    *out_count = g_lifecycle_schema_count;
    return HENKA_SUCCESS;
}

henka_result henka_script_lifecycle_schema_find(
    henka_script_lifecycle_event event,
    const henka_script_lifecycle_descriptor** out_descriptor)
{
    size_t index;
    if (out_descriptor == NULL || !henka_script_lifecycle_schema_is_valid())
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_descriptor = NULL;
    for (index = 0U; index < g_lifecycle_schema_count; ++index)
    {
        if (g_lifecycle_schema[index].event == event)
        {
            *out_descriptor = &g_lifecycle_schema[index];
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

static henka_script_behavior_handle henka_script_behavior_make_handle(
    size_t index,
    uint32_t generation)
{
    return ((henka_script_behavior_handle)generation << 32U) |
        (henka_script_behavior_handle)(index + 1U);
}

static bool henka_script_behavior_resolve(
    const henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    size_t* out_index)
{
    const uint32_t encoded_index = (uint32_t)(behavior & UINT32_MAX);
    const uint32_t generation = (uint32_t)(behavior >> 32U);
    size_t index;
    if (runtime == NULL || out_index == NULL || encoded_index == 0U || generation == 0U)
    {
        return false;
    }
    index = (size_t)encoded_index - 1U;
    if (index >= HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS ||
        !runtime->slots[index].occupied ||
        runtime->slots[index].generation != generation)
    {
        return false;
    }
    *out_index = index;
    return true;
}

static bool henka_script_behavior_event_is_valid(henka_script_lifecycle_event event)
{
    return event >= HENKA_SCRIPT_LIFECYCLE_CREATE &&
        event <= HENKA_SCRIPT_LIFECYCLE_DESTROY;
}

static bool henka_script_behavior_delta_is_valid(float delta_seconds)
{
    return isfinite(delta_seconds) && delta_seconds >= 0.0f;
}

static bool henka_script_behavior_transition_is_valid(
    henka_script_behavior_state state,
    henka_script_lifecycle_event event)
{
    switch (event)
    {
        case HENKA_SCRIPT_LIFECYCLE_CREATE:
            return state == HENKA_SCRIPT_BEHAVIOR_PENDING;
        case HENKA_SCRIPT_LIFECYCLE_START:
            return state == HENKA_SCRIPT_BEHAVIOR_CREATED;
        case HENKA_SCRIPT_LIFECYCLE_UPDATE:
        case HENKA_SCRIPT_LIFECYCLE_FIXED_UPDATE:
        case HENKA_SCRIPT_LIFECYCLE_INTERACT:
        case HENKA_SCRIPT_LIFECYCLE_COLLISION_ENTER:
        case HENKA_SCRIPT_LIFECYCLE_COLLISION_STAY:
        case HENKA_SCRIPT_LIFECYCLE_COLLISION_EXIT:
        case HENKA_SCRIPT_LIFECYCLE_TRIGGER_ENTER:
        case HENKA_SCRIPT_LIFECYCLE_TRIGGER_STAY:
        case HENKA_SCRIPT_LIFECYCLE_TRIGGER_EXIT:
            return state == HENKA_SCRIPT_BEHAVIOR_STARTED;
        case HENKA_SCRIPT_LIFECYCLE_DESTROY:
            return state == HENKA_SCRIPT_BEHAVIOR_CREATED ||
                state == HENKA_SCRIPT_BEHAVIOR_STARTED ||
                state == HENKA_SCRIPT_BEHAVIOR_FAULTED;
        case HENKA_SCRIPT_LIFECYCLE_STOP:
            return state == HENKA_SCRIPT_BEHAVIOR_CREATED ||
                state == HENKA_SCRIPT_BEHAVIOR_STARTED ||
                state == HENKA_SCRIPT_BEHAVIOR_FAULTED ||
                state == HENKA_SCRIPT_BEHAVIOR_DESTROYED;
        case HENKA_SCRIPT_LIFECYCLE_EVENT:
            return state == HENKA_SCRIPT_BEHAVIOR_STARTED;
        default:
            return false;
    }
}

static henka_script_behavior_state henka_script_behavior_state_after(
    henka_script_lifecycle_event event)
{
    switch (event)
    {
        case HENKA_SCRIPT_LIFECYCLE_CREATE: return HENKA_SCRIPT_BEHAVIOR_CREATED;
        case HENKA_SCRIPT_LIFECYCLE_START:
        case HENKA_SCRIPT_LIFECYCLE_UPDATE:
        case HENKA_SCRIPT_LIFECYCLE_FIXED_UPDATE:
        case HENKA_SCRIPT_LIFECYCLE_INTERACT:
        case HENKA_SCRIPT_LIFECYCLE_COLLISION_ENTER:
        case HENKA_SCRIPT_LIFECYCLE_COLLISION_STAY:
        case HENKA_SCRIPT_LIFECYCLE_COLLISION_EXIT:
        case HENKA_SCRIPT_LIFECYCLE_TRIGGER_ENTER:
        case HENKA_SCRIPT_LIFECYCLE_TRIGGER_STAY:
        case HENKA_SCRIPT_LIFECYCLE_TRIGGER_EXIT:
        case HENKA_SCRIPT_LIFECYCLE_EVENT: return HENKA_SCRIPT_BEHAVIOR_STARTED;
        case HENKA_SCRIPT_LIFECYCLE_DESTROY:
            return HENKA_SCRIPT_BEHAVIOR_DESTROYED;
        case HENKA_SCRIPT_LIFECYCLE_STOP: return HENKA_SCRIPT_BEHAVIOR_STOPPED;
        default: return HENKA_SCRIPT_BEHAVIOR_FAULTED;
    }
}

static void henka_script_behavior_report_clear(
    henka_script_behavior_report* report,
    const henka_script_behavior_slot* slot)
{
    if (report != NULL)
    {
        memset(report, 0, sizeof(*report));
        report->result = HENKA_SCRIPT_BEHAVIOR_INVALID_STATE;
        report->state = slot == NULL
            ? HENKA_SCRIPT_BEHAVIOR_FAULTED
            : slot->state;
        report->failure_count = slot == NULL ? 0U : slot->failure_count;
    }
}

static henka_result henka_script_behavior_dispatch_one(
    henka_script_behavior_runtime* runtime,
    size_t index,
    henka_script_lifecycle_event event,
    float delta_seconds,
    uint64_t frame_index,
    uint32_t event_id,
    uint64_t event_source_entity,
    uint64_t event_other_entity,
    uint32_t event_type,
    henka_script_behavior_report* out_report)
{
    henka_script_behavior_slot* slot = &runtime->slots[index];
    henka_script_behavior_context context;
    henka_script_behavior_callback_result callback_result;
    uint32_t instructions_used = 0U;
    henka_script_behavior_state previous_state;
    henka_result error = HENKA_SUCCESS;
    henka_result host_context_clear_result = HENKA_SUCCESS;

    henka_script_behavior_report_clear(out_report, slot);
    if (!slot->enabled)
    {
        if (out_report != NULL)
        {
            out_report->result = HENKA_SCRIPT_BEHAVIOR_SKIPPED_DISABLED;
        }
        return HENKA_SUCCESS;
    }
    if (event == HENKA_SCRIPT_LIFECYCLE_STOP &&
        slot->state == HENKA_SCRIPT_BEHAVIOR_STOPPED)
    {
        if (out_report != NULL)
        {
            out_report->result = HENKA_SCRIPT_BEHAVIOR_SKIPPED_STOPPED;
        }
        return HENKA_SUCCESS;
    }
    if (!henka_script_behavior_transition_is_valid(slot->state, event))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (slot->callback == NULL)
    {
        slot->state = HENKA_SCRIPT_BEHAVIOR_FAULTED;
        ++slot->failure_count;
        if (out_report != NULL)
        {
            out_report->result = HENKA_SCRIPT_BEHAVIOR_UNBOUND;
            out_report->state = slot->state;
            out_report->failure_count = slot->failure_count;
            out_report->state_changed = true;
        }
        return HENKA_ERROR_ASSET_SOURCE;
    }
    context = (henka_script_behavior_context){
        henka_script_behavior_make_handle(index, slot->generation),
        slot->entity_id,
        slot->language,
        event,
        delta_seconds,
        frame_index,
        slot->instruction_budget,
        slot->host,
        slot->behavior_id,
        event_id,
        event_source_entity,
        event_other_entity,
        event_type};
    previous_state = slot->state;
    if (slot->host != NULL &&
        henka_script_host_set_execution_context(
            slot->host,
            (henka_script_state_identity){slot->entity_id, slot->behavior_id}) != HENKA_SUCCESS)
    {
        slot->state = HENKA_SCRIPT_BEHAVIOR_FAULTED;
        ++slot->failure_count;
        if (out_report != NULL)
        {
            out_report->result = HENKA_SCRIPT_BEHAVIOR_CALLBACK_FAILED;
            out_report->state = slot->state;
            out_report->failure_count = slot->failure_count;
            out_report->state_changed = previous_state != slot->state;
        }
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime->dispatching = true;
    callback_result = slot->callback(&context, slot->user_data, &instructions_used);
    runtime->dispatching = false;
    if (slot->host != NULL)
    {
        host_context_clear_result = henka_script_host_set_execution_context(
            slot->host,
            (henka_script_state_identity){0U, 0U});
    }
    if (instructions_used > slot->instruction_budget ||
        callback_result == HENKA_SCRIPT_CALLBACK_BUDGET_EXHAUSTED)
    {
        slot->state = HENKA_SCRIPT_BEHAVIOR_FAULTED;
        ++slot->failure_count;
        error = HENKA_ERROR_LIMIT;
        if (out_report != NULL)
        {
            out_report->result = HENKA_SCRIPT_BEHAVIOR_BUDGET_EXHAUSTED;
        }
    }
    else if (callback_result != HENKA_SCRIPT_CALLBACK_COMPLETED)
    {
        slot->state = HENKA_SCRIPT_BEHAVIOR_FAULTED;
        ++slot->failure_count;
        error = HENKA_ERROR_UNKNOWN;
        if (out_report != NULL)
        {
            out_report->result = HENKA_SCRIPT_BEHAVIOR_CALLBACK_FAILED;
        }
    }
    else
    {
        slot->state = henka_script_behavior_state_after(event);
        if (out_report != NULL)
        {
            out_report->result = HENKA_SCRIPT_BEHAVIOR_EXECUTED;
        }
    }
    if (host_context_clear_result != HENKA_SUCCESS &&
        callback_result == HENKA_SCRIPT_CALLBACK_COMPLETED)
    {
        slot->state = HENKA_SCRIPT_BEHAVIOR_FAULTED;
        ++slot->failure_count;
        error = host_context_clear_result;
        if (out_report != NULL)
        {
            out_report->result = HENKA_SCRIPT_BEHAVIOR_CALLBACK_FAILED;
        }
    }
    if (out_report != NULL)
    {
        out_report->state = slot->state;
        out_report->instructions_used = instructions_used;
        out_report->failure_count = slot->failure_count;
        out_report->state_changed = previous_state != slot->state;
    }
    return error;
}

henka_result henka_script_behavior_runtime_create(
    henka_script_behavior_runtime** out_runtime)
{
    henka_script_behavior_runtime* runtime;
    if (out_runtime == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_runtime = NULL;
    runtime = (henka_script_behavior_runtime*)henka_calloc(1U, sizeof(*runtime));
    if (runtime == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    *out_runtime = runtime;
    return HENKA_SUCCESS;
}

void henka_script_behavior_runtime_destroy(henka_script_behavior_runtime* runtime)
{
    henka_free(runtime);
}

henka_result henka_script_behavior_runtime_add(
    henka_script_behavior_runtime* runtime,
    const henka_script_behavior_desc* desc,
    henka_script_behavior_handle* out_behavior)
{
    size_t index;
    henka_script_behavior_slot* slot;
    if (out_behavior != NULL)
    {
        *out_behavior = HENKA_INVALID_SCRIPT_BEHAVIOR_HANDLE;
    }
    if (runtime == NULL || desc == NULL || out_behavior == NULL ||
        runtime->dispatching || desc->language == HENKA_SCRIPT_LANGUAGE_NONE ||
        desc->instruction_budget > HENKA_SCRIPT_MAX_BEHAVIOR_INSTRUCTION_BUDGET)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS; ++index)
    {
        if (!runtime->slots[index].occupied)
        {
            break;
        }
    }
    if (index >= HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS)
    {
        return HENKA_ERROR_LIMIT;
    }
    slot = &runtime->slots[index];
    if (slot->generation == 0U)
    {
        slot->generation = 1U;
    }
    slot->occupied = true;
    slot->entity_id = desc->entity_id;
    slot->behavior_id = desc->behavior_id;
    slot->language = desc->language;
    slot->enabled = desc->enabled;
    slot->instruction_budget = desc->instruction_budget == 0U
        ? HENKA_SCRIPT_DEFAULT_BEHAVIOR_INSTRUCTION_BUDGET
        : desc->instruction_budget;
    slot->state = HENKA_SCRIPT_BEHAVIOR_PENDING;
    slot->failure_count = 0U;
    slot->callback = desc->callback;
    slot->user_data = desc->user_data;
    slot->host = desc->host;
    ++runtime->count;
    *out_behavior = henka_script_behavior_make_handle(index, slot->generation);
    return HENKA_SUCCESS;
}

henka_result henka_script_behavior_runtime_bind(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    henka_script_behavior_callback callback,
    void* user_data)
{
    size_t index;
    if (runtime == NULL || runtime->dispatching || callback == NULL ||
        !henka_script_behavior_resolve(runtime, behavior, &index))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime->slots[index].callback = callback;
    runtime->slots[index].user_data = user_data;
    return HENKA_SUCCESS;
}

henka_result henka_script_behavior_runtime_rebind(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    const henka_script_behavior_desc* desc)
{
    size_t index;
    henka_script_behavior_slot* slot;
    if (runtime == NULL || desc == NULL || runtime->dispatching ||
        desc->language == HENKA_SCRIPT_LANGUAGE_NONE ||
        desc->callback == NULL ||
        desc->instruction_budget > HENKA_SCRIPT_MAX_BEHAVIOR_INSTRUCTION_BUDGET ||
        !henka_script_behavior_resolve(runtime, behavior, &index))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot = &runtime->slots[index];
    if (slot->entity_id != desc->entity_id ||
        slot->behavior_id != desc->behavior_id)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot->language = desc->language;
    slot->enabled = desc->enabled;
    slot->instruction_budget = desc->instruction_budget == 0U
        ? HENKA_SCRIPT_DEFAULT_BEHAVIOR_INSTRUCTION_BUDGET
        : desc->instruction_budget;
    slot->callback = desc->callback;
    slot->user_data = desc->user_data;
    slot->host = desc->host;
    return HENKA_SUCCESS;
}

henka_result henka_script_behavior_runtime_remove(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior)
{
    size_t index;
    uint32_t generation;
    if (runtime == NULL || runtime->dispatching ||
        !henka_script_behavior_resolve(runtime, behavior, &index))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    generation = runtime->slots[index].generation == UINT32_MAX
        ? 1U
        : runtime->slots[index].generation + 1U;
    memset(&runtime->slots[index], 0, sizeof(runtime->slots[index]));
    runtime->slots[index].generation = generation;
    --runtime->count;
    return HENKA_SUCCESS;
}

henka_result henka_script_behavior_runtime_set_enabled(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    bool enabled)
{
    size_t index;
    if (runtime == NULL || runtime->dispatching ||
        !henka_script_behavior_resolve(runtime, behavior, &index))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime->slots[index].enabled = enabled;
    return HENKA_SUCCESS;
}

henka_result henka_script_behavior_runtime_reset(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior)
{
    size_t index;
    if (runtime == NULL || runtime->dispatching ||
        !henka_script_behavior_resolve(runtime, behavior, &index))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime->slots[index].state = HENKA_SCRIPT_BEHAVIOR_PENDING;
    runtime->slots[index].failure_count = 0U;
    return HENKA_SUCCESS;
}

henka_result henka_script_behavior_runtime_get(
    const henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    henka_script_behavior_snapshot* out_snapshot)
{
    size_t index;
    const henka_script_behavior_slot* slot;
    if (out_snapshot != NULL)
    {
        memset(out_snapshot, 0, sizeof(*out_snapshot));
        out_snapshot->behavior = HENKA_INVALID_SCRIPT_BEHAVIOR_HANDLE;
    }
    if (runtime == NULL || out_snapshot == NULL ||
        !henka_script_behavior_resolve(runtime, behavior, &index))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    slot = &runtime->slots[index];
    out_snapshot->behavior = behavior;
    out_snapshot->entity_id = slot->entity_id;
    out_snapshot->behavior_id = slot->behavior_id;
    out_snapshot->language = slot->language;
    out_snapshot->enabled = slot->enabled;
    out_snapshot->bound = slot->callback != NULL;
    out_snapshot->state = slot->state;
    out_snapshot->instruction_budget = slot->instruction_budget;
    out_snapshot->failure_count = slot->failure_count;
    return HENKA_SUCCESS;
}

henka_result henka_script_behavior_runtime_dispatch(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    henka_script_lifecycle_event event,
    float delta_seconds,
    uint64_t frame_index,
    henka_script_behavior_report* out_report)
{
    size_t index;
    if (out_report != NULL)
    {
        memset(out_report, 0, sizeof(*out_report));
        out_report->result = HENKA_SCRIPT_BEHAVIOR_INVALID_STATE;
    }
    if (runtime == NULL || runtime->dispatching ||
        event == HENKA_SCRIPT_LIFECYCLE_EVENT ||
        !henka_script_behavior_event_is_valid(event) ||
        !henka_script_behavior_delta_is_valid(delta_seconds) ||
        !henka_script_behavior_resolve(runtime, behavior, &index))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime->dispatching = true;
    {
        henka_result result = henka_script_behavior_dispatch_one(
            runtime,
            index,
            event,
            delta_seconds,
            frame_index,
            0U,
            0U,
            0U,
            0U,
            out_report);
        runtime->dispatching = false;
        return result;
    }
}

henka_result henka_script_behavior_runtime_dispatch_all(
    henka_script_behavior_runtime* runtime,
    henka_script_lifecycle_event event,
    float delta_seconds,
    uint64_t frame_index,
    henka_script_behavior_batch_report* out_report)
{
    size_t index;
    henka_result first_error = HENKA_SUCCESS;
    if (out_report != NULL)
    {
        memset(out_report, 0, sizeof(*out_report));
        out_report->event = event;
        out_report->first_error = HENKA_SUCCESS;
    }
    if (runtime == NULL || runtime->dispatching ||
        event == HENKA_SCRIPT_LIFECYCLE_EVENT ||
        !henka_script_behavior_event_is_valid(event) ||
        !henka_script_behavior_delta_is_valid(delta_seconds))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime->dispatching = true;
    for (index = 0U; index < HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS; ++index)
    {
        henka_script_behavior_report report;
        henka_result result;
        if (!runtime->slots[index].occupied)
        {
            continue;
        }
        if (out_report != NULL)
        {
            ++out_report->attempted;
        }
        result = henka_script_behavior_dispatch_one(
            runtime,
            index,
            event,
            delta_seconds,
            frame_index,
            0U,
            0U,
            0U,
            0U,
            &report);
        if (result != HENKA_SUCCESS && first_error == HENKA_SUCCESS)
        {
            first_error = result;
        }
        if (out_report != NULL)
        {
            if (report.result == HENKA_SCRIPT_BEHAVIOR_EXECUTED)
            {
                ++out_report->executed;
            }
            else if (report.result == HENKA_SCRIPT_BEHAVIOR_SKIPPED_DISABLED ||
                     report.result == HENKA_SCRIPT_BEHAVIOR_SKIPPED_STOPPED)
            {
                ++out_report->skipped;
            }
            else
            {
                ++out_report->failed;
            }
        }
    }
    runtime->dispatching = false;
    if (out_report != NULL)
    {
        out_report->first_error = first_error;
    }
    return first_error;
}

henka_result henka_script_behavior_runtime_dispatch_event(
    henka_script_behavior_runtime* runtime,
    henka_script_behavior_handle behavior,
    uint32_t event_id,
    uint64_t source_entity,
    uint64_t frame_index,
    henka_script_behavior_report* out_report)
{
    size_t index;
    if (out_report != NULL)
    {
        memset(out_report, 0, sizeof(*out_report));
        out_report->result = HENKA_SCRIPT_BEHAVIOR_INVALID_STATE;
    }
    if (runtime == NULL || runtime->dispatching || event_id == 0U ||
        !henka_script_behavior_resolve(runtime, behavior, &index))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime->dispatching = true;
    {
        henka_result result = henka_script_behavior_dispatch_one(
            runtime,
            index,
            HENKA_SCRIPT_LIFECYCLE_EVENT,
            0.0f,
            frame_index,
            event_id,
            source_entity,
            0U,
            0U,
            out_report);
        runtime->dispatching = false;
        return result;
    }
}

henka_result henka_script_behavior_runtime_dispatch_event_all(
    henka_script_behavior_runtime* runtime,
    uint32_t event_id,
    uint64_t source_entity,
    uint64_t frame_index,
    henka_script_behavior_batch_report* out_report)
{
    size_t index;
    henka_result first_error = HENKA_SUCCESS;
    if (out_report != NULL)
    {
        memset(out_report, 0, sizeof(*out_report));
        out_report->event = HENKA_SCRIPT_LIFECYCLE_EVENT;
        out_report->first_error = HENKA_SUCCESS;
    }
    if (runtime == NULL || runtime->dispatching || event_id == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime->dispatching = true;
    for (index = 0U; index < HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS; ++index)
    {
        henka_script_behavior_report report;
        henka_result result;
        if (!runtime->slots[index].occupied)
        {
            continue;
        }
        if (out_report != NULL)
        {
            ++out_report->attempted;
        }
        result = henka_script_behavior_dispatch_one(
            runtime,
            index,
            HENKA_SCRIPT_LIFECYCLE_EVENT,
            0.0f,
            frame_index,
            event_id,
            source_entity,
            0U,
            0U,
            &report);
        if (result != HENKA_SUCCESS && first_error == HENKA_SUCCESS)
        {
            first_error = result;
        }
        if (out_report != NULL)
        {
            if (report.result == HENKA_SCRIPT_BEHAVIOR_EXECUTED)
            {
                ++out_report->executed;
            }
            else if (report.result == HENKA_SCRIPT_BEHAVIOR_SKIPPED_DISABLED ||
                     report.result == HENKA_SCRIPT_BEHAVIOR_SKIPPED_STOPPED)
            {
                ++out_report->skipped;
            }
            else
            {
                ++out_report->failed;
            }
        }
    }
    runtime->dispatching = false;
    if (out_report != NULL)
    {
        out_report->first_error = first_error;
    }
    return first_error;
}

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
    henka_script_behavior_batch_report* out_report)
{
    size_t index;
    henka_result first_error = HENKA_SUCCESS;
    if (out_report != NULL)
    {
        memset(out_report, 0, sizeof(*out_report));
        out_report->event = event;
        out_report->first_error = HENKA_SUCCESS;
    }
    if (runtime == NULL || runtime->dispatching || entity_id == 0U ||
        event == HENKA_SCRIPT_LIFECYCLE_EVENT ||
        !henka_script_behavior_event_is_valid(event) ||
        !henka_script_behavior_delta_is_valid(delta_seconds))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    runtime->dispatching = true;
    for (index = 0U; index < HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS; ++index)
    {
        henka_script_behavior_report report;
        henka_result result;
        if (!runtime->slots[index].occupied ||
            runtime->slots[index].entity_id != entity_id)
        {
            continue;
        }
        if (out_report != NULL)
        {
            ++out_report->attempted;
        }
        result = henka_script_behavior_dispatch_one(
            runtime,
            index,
            event,
            delta_seconds,
            frame_index,
            event_id,
            source_entity,
            other_entity,
            event_type,
            &report);
        if (result != HENKA_SUCCESS && first_error == HENKA_SUCCESS)
        {
            first_error = result;
        }
        if (out_report != NULL)
        {
            if (report.result == HENKA_SCRIPT_BEHAVIOR_EXECUTED)
            {
                ++out_report->executed;
            }
            else if (report.result == HENKA_SCRIPT_BEHAVIOR_SKIPPED_DISABLED ||
                     report.result == HENKA_SCRIPT_BEHAVIOR_SKIPPED_STOPPED)
            {
                ++out_report->skipped;
            }
            else
            {
                ++out_report->failed;
            }
        }
    }
    runtime->dispatching = false;
    if (out_report != NULL)
    {
        out_report->first_error = first_error;
    }
    return first_error;
}

size_t henka_script_behavior_runtime_get_count(
    const henka_script_behavior_runtime* runtime)
{
    return runtime == NULL ? 0U : runtime->count;
}
