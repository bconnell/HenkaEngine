#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/script_backends.h>

static void test_henkascript_lifecycle_adapter(void)
{
    static const char source[] =
        "fn OnCreate() { i32 value = 2; value = value + 3; }\n"
        "fn OnUpdate() { i32 value = 1; value = value * 4; }\n";
    henka_hks_behavior_backend* backend = NULL;
    henka_hks_diagnostic diagnostic;
    henka_script_behavior_runtime* runtime = NULL;
    henka_script_behavior_desc desc;
    henka_script_behavior_handle behavior = HENKA_INVALID_SCRIPT_BEHAVIOR_HANDLE;
    henka_script_behavior_report report;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_hks_behavior_backend_create(
               source, strlen(source), &backend, &diagnostic) == HENKA_SUCCESS);
    assert(backend != NULL);
    assert(henka_script_behavior_runtime_create(&runtime) == HENKA_SUCCESS);
    desc = (henka_script_behavior_desc){
        7U,
        HENKA_SCRIPT_LANGUAGE_HENKASCRIPT,
        true,
        64U,
        henka_hks_behavior_backend_callback,
        backend};
    assert(henka_script_behavior_runtime_add(runtime, &desc, &behavior) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_SUCCESS);
    assert(report.result == HENKA_SCRIPT_BEHAVIOR_EXECUTED);
    assert(report.instructions_used > 0U);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_START, 0.0f, 2U, &report) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_UPDATE, 0.016f, 3U, &report) == HENKA_SUCCESS);
    assert(report.instructions_used > 0U);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_STOP, 0.0f, 4U, &report) == HENKA_SUCCESS);
    henka_script_behavior_runtime_destroy(runtime);
    henka_hks_behavior_backend_destroy(backend);
}

static void test_henkascript_adapter_budget_and_noop(void)
{
    static const char source[] = "fn OnUpdate() { i32 value = 1; value = value + 1; }";
    henka_hks_behavior_backend* backend = NULL;
    henka_hks_diagnostic diagnostic;
    henka_script_behavior_context context;
    uint32_t used = 0U;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_hks_behavior_backend_create(
               source, strlen(source), &backend, &diagnostic) == HENKA_SUCCESS);
    context = (henka_script_behavior_context){
        1U, 2U, HENKA_SCRIPT_LANGUAGE_HENKASCRIPT,
        HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, 64U};
    assert(henka_hks_behavior_backend_callback(
               &context, backend, &used) == HENKA_SCRIPT_CALLBACK_COMPLETED);
    assert(used == 0U);
    context.event = HENKA_SCRIPT_LIFECYCLE_UPDATE;
    context.instruction_budget = 1U;
    assert(henka_hks_behavior_backend_callback(
               &context, backend, &used) == HENKA_SCRIPT_CALLBACK_BUDGET_EXHAUSTED);
    assert(used == 1U);
    henka_hks_behavior_backend_destroy(backend);
}

static void test_backend_rejection_and_memory(void)
{
    henka_hks_behavior_backend* backend = NULL;
    henka_hks_diagnostic diagnostic;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_hks_behavior_backend_create(
               "fn OnCreate() {", strlen("fn OnCreate() {"), &backend, &diagnostic) != HENKA_SUCCESS);
    assert(backend == NULL);
}

int main(void)
{
    const size_t allocations_before = henka_memory_get_allocation_count();
    test_henkascript_lifecycle_adapter();
    test_henkascript_adapter_budget_and_noop();
    test_backend_rejection_and_memory();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_script_backend_tests: PASS");
    return 0;
}
