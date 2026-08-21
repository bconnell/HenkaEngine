#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/script_backends.h>

static void test_lua_lifecycle_adapter(void)
{
    static const char source[] =
        "function OnCreate() local value = 2; value = value + 3 end\n"
        "function OnUpdate() local value = 1; value = value * 4 end\n";
    henka_lua_behavior_backend* backend = NULL;
    henka_lua_diagnostic diagnostic;
    henka_script_behavior_runtime* runtime = NULL;
    henka_script_behavior_desc desc;
    henka_script_behavior_handle behavior = HENKA_INVALID_SCRIPT_BEHAVIOR_HANDLE;
    henka_script_behavior_report report;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_lua_behavior_backend_create(
               source, strlen(source), &backend, &diagnostic) == HENKA_SUCCESS);
    assert(backend != NULL);
    assert(henka_script_behavior_runtime_create(&runtime) == HENKA_SUCCESS);
    desc = (henka_script_behavior_desc){
        9U,
        HENKA_SCRIPT_LANGUAGE_LUA,
        true,
        128U,
        henka_lua_behavior_backend_callback,
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
    henka_lua_behavior_backend_destroy(backend);
}

static void test_lua_budget_and_sandbox(void)
{
    static const char source[] =
        "function OnUpdate() "
        "local total = 0; "
        "for i = 1, 1000 do total = total + i end "
        "end";
    henka_lua_behavior_backend* backend = NULL;
    henka_lua_diagnostic diagnostic;
    henka_script_behavior_context context;
    uint32_t used = 0U;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_lua_behavior_backend_create(
               source, strlen(source), &backend, &diagnostic) == HENKA_SUCCESS);
    context = (henka_script_behavior_context){
        1U,
        2U,
        HENKA_SCRIPT_LANGUAGE_LUA,
        HENKA_SCRIPT_LIFECYCLE_CREATE,
        0.0f,
        1U,
        64U};
    assert(henka_lua_behavior_backend_callback(
               &context, backend, &used) == HENKA_SCRIPT_CALLBACK_COMPLETED);
    assert(used == 0U);
    context.event = HENKA_SCRIPT_LIFECYCLE_UPDATE;
    context.instruction_budget = 1U;
    assert(henka_lua_behavior_backend_callback(
               &context, backend, &used) == HENKA_SCRIPT_CALLBACK_BUDGET_EXHAUSTED);
    assert(used == 1U);
    context.instruction_budget = 64U;
    context.language = HENKA_SCRIPT_LANGUAGE_HENKASCRIPT;
    assert(henka_lua_behavior_backend_callback(
               &context, backend, &used) == HENKA_SCRIPT_CALLBACK_FAILED);
    henka_lua_behavior_backend_destroy(backend);
}

static void test_lua_rejection_and_memory(void)
{
    henka_lua_behavior_backend* backend = NULL;
    henka_lua_diagnostic diagnostic;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_lua_behavior_backend_create(
               "function OnCreate( ", strlen("function OnCreate( "),
               &backend, &diagnostic) != HENKA_SUCCESS);
    assert(backend == NULL);
    assert(diagnostic.code == HENKA_LUA_DIAGNOSTIC_COMPILE);
    assert(henka_lua_behavior_backend_create(
               NULL, HENKA_LUA_MAX_SOURCE_BYTES + 1U,
               &backend, &diagnostic) == HENKA_ERROR_INVALID_ARGUMENT);
}

int main(void)
{
    const size_t allocations_before = henka_memory_get_allocation_count();
    test_lua_lifecycle_adapter();
    test_lua_budget_and_sandbox();
    test_lua_rejection_and_memory();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_lua_backend_tests: PASS");
    return 0;
}
