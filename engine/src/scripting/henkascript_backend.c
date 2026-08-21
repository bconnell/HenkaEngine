#include <henka/script_backends.h>

#include <stdint.h>

#include <henka/memory.h>

struct henka_hks_behavior_backend
{
    henka_hks_program* program;
    size_t callable[HENKA_SCRIPT_LIFECYCLE_SLOT_COUNT];
};

henka_result henka_hks_behavior_backend_create(
    const char* source,
    size_t source_size,
    henka_hks_behavior_backend** out_backend,
    henka_hks_diagnostic* out_diagnostic)
{
    const henka_script_lifecycle_descriptor* lifecycle_schema = NULL;
    size_t lifecycle_count = 0U;
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
    for (index = 0U; index < HENKA_SCRIPT_LIFECYCLE_SLOT_COUNT; ++index)
    {
        backend->callable[index] = SIZE_MAX;
    }
    compile_result = henka_hks_compile(source, source_size, &backend->program, out_diagnostic);
    if (compile_result != HENKA_SUCCESS)
    {
        henka_free(backend);
        return compile_result;
    }
    if (henka_script_lifecycle_schema_get(
            &lifecycle_schema,
            &lifecycle_count) != HENKA_SUCCESS ||
        lifecycle_schema == NULL)
    {
        henka_hks_behavior_backend_destroy(backend);
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < lifecycle_count; ++index)
    {
        const henka_script_lifecycle_descriptor* descriptor = &lifecycle_schema[index];
        if (descriptor->event >= HENKA_SCRIPT_LIFECYCLE_SLOT_COUNT)
        {
            henka_hks_behavior_backend_destroy(backend);
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
        (void)henka_hks_program_find_callable(
            backend->program,
            descriptor->name,
            &backend->callable[descriptor->event]);
    }
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
    callable_index = backend->callable[context->event];
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
