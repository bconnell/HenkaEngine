#include <henka/script_backends.h>

#include <stdint.h>

#include <henka/memory.h>

struct henka_hks_behavior_backend
{
    henka_hks_program* program;
    size_t lifecycle_callable[4];
};

static size_t henka_hks_backend_event_index(henka_script_lifecycle_event event)
{
    return (size_t)event;
}

henka_result henka_hks_behavior_backend_create(
    const char* source,
    size_t source_size,
    henka_hks_behavior_backend** out_backend,
    henka_hks_diagnostic* out_diagnostic)
{
    static const char* const lifecycle_names[] =
    {
        "OnCreate",
        "OnStart",
        "OnUpdate",
        "OnStop"
    };
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
    compile_result = henka_hks_compile(source, source_size, &backend->program, out_diagnostic);
    if (compile_result != HENKA_SUCCESS)
    {
        henka_free(backend);
        return compile_result;
    }
    for (index = 0U; index < sizeof(lifecycle_names) / sizeof(lifecycle_names[0]); ++index)
    {
        (void)henka_hks_program_find_callable(
            backend->program,
            lifecycle_names[index],
            &backend->lifecycle_callable[index]);
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
    size_t callable_index;
    henka_hks_execution_result result;
    if (out_instructions_used != NULL)
    {
        *out_instructions_used = 0U;
    }
    if (context == NULL || backend == NULL || backend->program == NULL ||
        out_instructions_used == NULL ||
        context->event > HENKA_SCRIPT_LIFECYCLE_STOP)
    {
        return HENKA_SCRIPT_CALLBACK_FAILED;
    }
    callable_index = backend->lifecycle_callable[henka_hks_backend_event_index(context->event)];
    if (callable_index == SIZE_MAX)
    {
        return HENKA_SCRIPT_CALLBACK_COMPLETED;
    }
    result = henka_hks_execute(
        backend->program,
        callable_index,
        context->instruction_budget,
        &return_value,
        &report);
    *out_instructions_used = report.instructions_executed;
    if (result == HENKA_HKS_EXECUTION_BUDGET_EXHAUSTED)
    {
        return HENKA_SCRIPT_CALLBACK_BUDGET_EXHAUSTED;
    }
    return result == HENKA_HKS_EXECUTION_COMPLETED
        ? HENKA_SCRIPT_CALLBACK_COMPLETED
        : HENKA_SCRIPT_CALLBACK_FAILED;
}
