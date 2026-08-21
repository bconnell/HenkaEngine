#include <henka/script_backends.h>

#include <stdint.h>

#include <henka/memory.h>

struct henka_hks_behavior_backend
{
    henka_hks_program* program;
    size_t lifecycle_callable[13];
    size_t event_callable;
};

static size_t henka_hks_backend_event_index(henka_script_lifecycle_event event)
{
    switch (event)
    {
        case HENKA_SCRIPT_LIFECYCLE_CREATE: return 0U;
        case HENKA_SCRIPT_LIFECYCLE_START: return 1U;
        case HENKA_SCRIPT_LIFECYCLE_UPDATE: return 2U;
        case HENKA_SCRIPT_LIFECYCLE_STOP: return 3U;
        case HENKA_SCRIPT_LIFECYCLE_FIXED_UPDATE: return 4U;
        case HENKA_SCRIPT_LIFECYCLE_INTERACT: return 5U;
        case HENKA_SCRIPT_LIFECYCLE_COLLISION_ENTER: return 6U;
        case HENKA_SCRIPT_LIFECYCLE_COLLISION_STAY: return 7U;
        case HENKA_SCRIPT_LIFECYCLE_COLLISION_EXIT: return 8U;
        case HENKA_SCRIPT_LIFECYCLE_TRIGGER_ENTER: return 9U;
        case HENKA_SCRIPT_LIFECYCLE_TRIGGER_STAY: return 10U;
        case HENKA_SCRIPT_LIFECYCLE_TRIGGER_EXIT: return 11U;
        case HENKA_SCRIPT_LIFECYCLE_DESTROY: return 12U;
        default: return SIZE_MAX;
    }
}

henka_result henka_hks_behavior_backend_create(
    const char* source,
    size_t source_size,
    henka_hks_behavior_backend** out_backend,
    henka_hks_diagnostic* out_diagnostic)
{
    static const henka_script_lifecycle_event lifecycle_events[13] =
    {
        HENKA_SCRIPT_LIFECYCLE_CREATE,
        HENKA_SCRIPT_LIFECYCLE_START,
        HENKA_SCRIPT_LIFECYCLE_UPDATE,
        HENKA_SCRIPT_LIFECYCLE_STOP,
        HENKA_SCRIPT_LIFECYCLE_FIXED_UPDATE,
        HENKA_SCRIPT_LIFECYCLE_INTERACT,
        HENKA_SCRIPT_LIFECYCLE_COLLISION_ENTER,
        HENKA_SCRIPT_LIFECYCLE_COLLISION_STAY,
        HENKA_SCRIPT_LIFECYCLE_COLLISION_EXIT,
        HENKA_SCRIPT_LIFECYCLE_TRIGGER_ENTER,
        HENKA_SCRIPT_LIFECYCLE_TRIGGER_STAY,
        HENKA_SCRIPT_LIFECYCLE_TRIGGER_EXIT,
        HENKA_SCRIPT_LIFECYCLE_DESTROY
    };
    const henka_script_lifecycle_descriptor* event_descriptor = NULL;
    const size_t lifecycle_count = sizeof(lifecycle_events) / sizeof(lifecycle_events[0]);
    henka_hks_behavior_backend* backend;
    henka_result compile_result;
    size_t index;
    if (out_backend == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_backend = NULL;
    backend = (henka_hks_behavior_backend*)henka_calloc(1U, sizeof(*backend));
    if (backend == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    for (index = 0U; index < sizeof(backend->lifecycle_callable) / sizeof(backend->lifecycle_callable[0]); ++index)
    {
        backend->lifecycle_callable[index] = SIZE_MAX;
    }
    backend->event_callable = SIZE_MAX;
    compile_result = henka_hks_compile(source, source_size, &backend->program, out_diagnostic);
    if (compile_result != HENKA_SUCCESS)
    {
        henka_free(backend);
        return compile_result;
    }
    for (index = 0U; index < lifecycle_count; ++index)
    {
        const henka_script_lifecycle_descriptor* descriptor = NULL;
        if (henka_script_lifecycle_schema_find(lifecycle_events[index], &descriptor) != HENKA_SUCCESS ||
            descriptor == NULL)
        {
            henka_hks_behavior_backend_destroy(backend);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        (void)henka_hks_program_find_callable(
            backend->program,
            descriptor->name,
            &backend->lifecycle_callable[index]);
    }
    if (henka_script_lifecycle_schema_find(
            HENKA_SCRIPT_LIFECYCLE_EVENT, &event_descriptor) != HENKA_SUCCESS ||
        event_descriptor == NULL)
    {
        henka_hks_behavior_backend_destroy(backend);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    (void)henka_hks_program_find_callable(
        backend->program, event_descriptor->name, &backend->event_callable);
    *out_backend = backend;
    return HENKA_SUCCESS;
}

void henka_hks_behavior_backend_destroy(henka_hks_behavior_backend* backend)
{
    if (backend != NULL)
    {
        henka_hks_program_destroy(backend->program);
        henka_free(backend);
    }
}

henka_script_behavior_callback_result henka_hks_behavior_backend_callback(
    const henka_script_behavior_context* context,
    void* user_data,
    uint32_t* out_instructions_used)
{
    henka_hks_behavior_backend* backend = (henka_hks_behavior_backend*)user_data;
    henka_hks_value return_value;
    henka_hks_execution_report report;
    henka_hks_execution_context execution_context;
    size_t callable_index;
    henka_hks_execution_result result;
    if (out_instructions_used != NULL)
    {
        *out_instructions_used = 0U;
    }
    if (context == NULL || backend == NULL || backend->program == NULL ||
        out_instructions_used == NULL ||
        context->event > HENKA_SCRIPT_LIFECYCLE_DESTROY)
    {
        return HENKA_SCRIPT_CALLBACK_FAILED;
    }
    callable_index = context->event == HENKA_SCRIPT_LIFECYCLE_EVENT
        ? backend->event_callable
        : (henka_hks_backend_event_index(context->event) == SIZE_MAX
            ? SIZE_MAX
            : backend->lifecycle_callable[henka_hks_backend_event_index(context->event)]);
    if (callable_index == SIZE_MAX)
    {
        return HENKA_SCRIPT_CALLBACK_COMPLETED;
    }
    execution_context = (henka_hks_execution_context){
        context->host,
        context->entity_id,
        context->frame_index,
        context->behavior_id,
        context->event == HENKA_SCRIPT_LIFECYCLE_EVENT,
        context->event_id,
        context->event_source_entity,
        context->event_other_entity,
        context->event_type,
        context->event >= HENKA_SCRIPT_LIFECYCLE_INTERACT &&
            context->event <= HENKA_SCRIPT_LIFECYCLE_TRIGGER_EXIT};
    result = henka_hks_execute_with_context(
        backend->program,
        callable_index,
        context->instruction_budget,
        &execution_context,
        &return_value,
        &report);
    *out_instructions_used = report.instructions_executed;
    if (result == HENKA_HKS_EXECUTION_BUDGET_EXHAUSTED)
    {
        return HENKA_SCRIPT_CALLBACK_BUDGET_EXHAUSTED;
    }
    if (result == HENKA_HKS_EXECUTION_HOST_ERROR)
    {
        return HENKA_SCRIPT_CALLBACK_FAILED;
    }
    return result == HENKA_HKS_EXECUTION_COMPLETED
        ? HENKA_SCRIPT_CALLBACK_COMPLETED
        : HENKA_SCRIPT_CALLBACK_FAILED;
}
