#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/script.h>
#include <henka/script_backends.h>

static henka_result hks_event_dispatch(
    void* user_data,
    uint32_t api_id,
    const henka_script_api_value* arguments,
    size_t argument_count,
    henka_script_api_value* out_value)
{
    henka_script_host* host = (henka_script_host*)user_data;
    if (host == NULL || api_id != HENKA_SCRIPT_API_EVENTS_EMIT ||
        arguments == NULL || argument_count != 2U || out_value == NULL ||
        henka_script_host_emit_event(
            host,
            arguments[0].as.event_id,
            arguments[1].as.entity,
            12U) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    out_value->type = HENKA_SCRIPT_API_VALUE_RESULT;
    out_value->as.result = HENKA_SUCCESS;
    return HENKA_SUCCESS;
}

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

static void test_henkascript_shared_event_host(void)
{
    static const char source[] = "fn OnUpdate() { emit(7); }";
    henka_hks_behavior_backend* backend = NULL;
    henka_hks_diagnostic diagnostic;
    henka_script_host* host = NULL;
    henka_script_behavior_context context;
    henka_script_event event;
    uint32_t used = 0U;

    memset(&diagnostic, 0, sizeof(diagnostic));
    assert(henka_hks_behavior_backend_create(
               source, strlen(source), &backend, &diagnostic) == HENKA_SUCCESS);
    assert(henka_script_host_create(&host) == HENKA_SUCCESS);
    assert(henka_script_host_bind_api(
               host, HENKA_SCRIPT_API_EVENTS_EMIT, &(size_t){0U}) == HENKA_SUCCESS);
    assert(henka_script_host_set_dispatcher(host, hks_event_dispatch, host) == HENKA_SUCCESS);
    context = (henka_script_behavior_context){
        1U, 42U, HENKA_SCRIPT_LANGUAGE_HENKASCRIPT, HENKA_SCRIPT_LIFECYCLE_UPDATE,
        0.016f, 12U, 64U, host};
    assert(henka_hks_behavior_backend_callback(
               &context, backend, &used) == HENKA_SCRIPT_CALLBACK_COMPLETED);
    assert(used > 0U);
    assert(henka_script_host_poll_event(host, &event) == HENKA_SUCCESS);
    assert(event.event_id == 7U && event.source_entity == 42U && event.frame_index == 12U);
    henka_script_host_destroy(host);
    henka_hks_behavior_backend_destroy(backend);
}

static void test_mixed_language_event_routing(void)
{
    static const char hks_source[] =
        "fn OnUpdate() { emit(7); } "
        "fn OnEvent() { state_set_i32(80, event_id()); }";
    static const char lua_source[] =
        "function OnUpdate() Events.Emit(9, 2) end "
        "function OnEvent(event_id, source_entity) State.SetI32(90, event_id) end";
    henka_hks_behavior_backend* hks_backend = NULL;
    henka_lua_behavior_backend* lua_backend = NULL;
    henka_hks_diagnostic hks_diagnostic;
    henka_lua_diagnostic lua_diagnostic;
    henka_script_behavior_runtime* runtime = NULL;
    henka_script_host* host = NULL;
    henka_script_state_store* store = NULL;
    henka_script_behavior_handle ignored_behavior;
    henka_script_behavior_batch_report report;
    henka_script_event event;
    henka_script_state_value value;
    bool present;

    memset(&hks_diagnostic, 0, sizeof(hks_diagnostic));
    memset(&lua_diagnostic, 0, sizeof(lua_diagnostic));
    assert(henka_hks_behavior_backend_create(
               hks_source, strlen(hks_source), &hks_backend, &hks_diagnostic) == HENKA_SUCCESS);
    assert(henka_lua_behavior_backend_create(
               lua_source, strlen(lua_source), &lua_backend, &lua_diagnostic) == HENKA_SUCCESS);
    assert(henka_script_host_create(&host) == HENKA_SUCCESS);
    assert(henka_script_state_store_create(&store) == HENKA_SUCCESS);
    assert(henka_script_host_bind_api(
               host, HENKA_SCRIPT_API_EVENTS_EMIT, &(size_t){0U}) == HENKA_SUCCESS);
    assert(henka_script_host_bind_api(
               host, HENKA_SCRIPT_API_STATE_GET_I32, &(size_t){0U}) == HENKA_SUCCESS);
    assert(henka_script_host_bind_api(
               host, HENKA_SCRIPT_API_STATE_SET_I32, &(size_t){0U}) == HENKA_SUCCESS);
    assert(henka_script_host_set_state_store(host, store) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_create(&runtime) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_add(
               runtime,
               &(henka_script_behavior_desc){
                   1U, HENKA_SCRIPT_LANGUAGE_HENKASCRIPT, true, 128U,
                   henka_hks_behavior_backend_callback, hks_backend, host, 11U},
               &ignored_behavior) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_add(
               runtime,
               &(henka_script_behavior_desc){
                   2U, HENKA_SCRIPT_LANGUAGE_LUA, true, 128U,
                   henka_lua_behavior_backend_callback, lua_backend, host, 12U},
               &ignored_behavior) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_START, 0.0f, 1U, &report) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_UPDATE, 0.016f, 2U, &report) == HENKA_SUCCESS);
    assert(henka_script_host_get_pending_event_count(host) == 2U);
    assert(henka_script_host_poll_event(host, &event) == HENKA_SUCCESS);
    assert(event.event_id == 7U &&
           henka_script_behavior_runtime_dispatch_event_all(
               runtime, event.event_id, event.source_entity, event.frame_index,
               &report) == HENKA_SUCCESS);
    assert(henka_script_host_poll_event(host, &event) == HENKA_SUCCESS);
    assert(event.event_id == 9U &&
           henka_script_behavior_runtime_dispatch_event_all(
               runtime, event.event_id, event.source_entity, event.frame_index,
               &report) == HENKA_SUCCESS);
    assert(henka_script_state_store_get(
               store, (henka_script_state_identity){1U, 11U}, 80U,
               &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_I32 && value.as.i32 == 9);
    assert(henka_script_state_store_get(
               store, (henka_script_state_identity){2U, 12U}, 90U,
               &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_I32 && value.as.i32 == 9);
    assert(henka_script_host_get_pending_event_count(host) == 0U);
    henka_script_behavior_runtime_destroy(runtime);
    henka_script_state_store_destroy(store);
    henka_script_host_destroy(host);
    henka_lua_behavior_backend_destroy(lua_backend);
    henka_hks_behavior_backend_destroy(hks_backend);
}

int main(void)
{
    const size_t allocations_before = henka_memory_get_allocation_count();
    test_henkascript_lifecycle_adapter();
    test_henkascript_adapter_budget_and_noop();
    test_backend_rejection_and_memory();
    test_henkascript_shared_event_host();
    test_mixed_language_event_routing();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_script_backend_tests: PASS");
    return 0;
}
