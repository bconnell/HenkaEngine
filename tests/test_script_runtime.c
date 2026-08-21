#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <henka/memory.h>
#include <henka/script_runtime.h>

typedef struct callback_fixture
{
    henka_script_lifecycle_event events[8];
    size_t event_count;
    henka_script_behavior_callback_result result;
    uint32_t instructions_used;
    uint64_t last_other_entity;
    uint32_t last_event_type;
} callback_fixture;

static henka_script_behavior_callback_result callback_record(
    const henka_script_behavior_context* context,
    void* user_data,
    uint32_t* out_instructions_used)
{
    callback_fixture* fixture = (callback_fixture*)user_data;
    assert(context != NULL);
    assert(fixture != NULL);
    assert(out_instructions_used != NULL);
    assert(fixture->event_count < sizeof(fixture->events) / sizeof(fixture->events[0]));
    fixture->events[fixture->event_count++] = context->event;
    fixture->last_other_entity = context->event_other_entity;
    fixture->last_event_type = context->event_type;
    *out_instructions_used = fixture->instructions_used;
    return fixture->result;
}

static henka_script_behavior_desc default_desc(callback_fixture* fixture)
{
    return (henka_script_behavior_desc){
        42U,
        HENKA_SCRIPT_LANGUAGE_HENKASCRIPT,
        true,
        0U,
        callback_record,
        fixture};
}

static void test_lifecycle_transitions_and_reports(void)
{
    callback_fixture fixture = {{0}, 0U, HENKA_SCRIPT_CALLBACK_COMPLETED, 4U, 0U, 0U};
    henka_script_behavior_runtime* runtime = NULL;
    henka_script_behavior_desc desc = default_desc(&fixture);
    henka_script_behavior_handle behavior = HENKA_INVALID_SCRIPT_BEHAVIOR_HANDLE;
    henka_script_behavior_snapshot snapshot;
    henka_script_behavior_report report;

    assert(henka_script_behavior_runtime_create(&runtime) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_add(runtime, &desc, &behavior) == HENKA_SUCCESS);
    assert(behavior != HENKA_INVALID_SCRIPT_BEHAVIOR_HANDLE);
    assert(henka_script_behavior_runtime_get_count(runtime) == 1U);
    assert(henka_script_behavior_runtime_get(runtime, behavior, &snapshot) == HENKA_SUCCESS);
    assert(snapshot.state == HENKA_SCRIPT_BEHAVIOR_PENDING);
    assert(snapshot.bound && snapshot.instruction_budget == HENKA_SCRIPT_DEFAULT_BEHAVIOR_INSTRUCTION_BUDGET);

    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_SUCCESS);
    assert(report.result == HENKA_SCRIPT_BEHAVIOR_EXECUTED);
    assert(report.state == HENKA_SCRIPT_BEHAVIOR_CREATED);
    assert(report.instructions_used == 4U);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_START, 0.0f, 2U, &report) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_UPDATE, 0.016f, 3U, &report) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_STOP, 0.0f, 4U, &report) == HENKA_SUCCESS);
    assert(fixture.event_count == 4U);
    assert(fixture.events[0] == HENKA_SCRIPT_LIFECYCLE_CREATE);
    assert(fixture.events[1] == HENKA_SCRIPT_LIFECYCLE_START);
    assert(fixture.events[2] == HENKA_SCRIPT_LIFECYCLE_UPDATE);
    assert(fixture.events[3] == HENKA_SCRIPT_LIFECYCLE_STOP);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_STOP, 0.0f, 5U, &report) == HENKA_SUCCESS);
    assert(report.result == HENKA_SCRIPT_BEHAVIOR_SKIPPED_STOPPED);
    assert(fixture.event_count == 4U);
    henka_script_behavior_runtime_destroy(runtime);
}

static void test_fail_closed_and_recovery(void)
{
    callback_fixture fixture = {{0}, 0U, HENKA_SCRIPT_CALLBACK_FAILED, 1U, 0U, 0U};
    henka_script_behavior_runtime* runtime = NULL;
    henka_script_behavior_desc desc = default_desc(&fixture);
    henka_script_behavior_handle behavior = HENKA_INVALID_SCRIPT_BEHAVIOR_HANDLE;
    henka_script_behavior_snapshot snapshot;
    henka_script_behavior_report report;

    assert(henka_script_behavior_runtime_create(&runtime) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_add(runtime, &desc, &behavior) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_ERROR_UNKNOWN);
    assert(report.result == HENKA_SCRIPT_BEHAVIOR_CALLBACK_FAILED);
    assert(henka_script_behavior_runtime_get(runtime, behavior, &snapshot) == HENKA_SUCCESS);
    assert(snapshot.state == HENKA_SCRIPT_BEHAVIOR_FAULTED && snapshot.failure_count == 1U);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 2U, &report) == HENKA_ERROR_INVALID_ARGUMENT);
    fixture.result = HENKA_SCRIPT_CALLBACK_COMPLETED;
    assert(henka_script_behavior_runtime_reset(runtime, behavior) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 3U, &report) == HENKA_SUCCESS);
    assert(report.state == HENKA_SCRIPT_BEHAVIOR_CREATED);
    henka_script_behavior_runtime_destroy(runtime);

    fixture = (callback_fixture){{0}, 0U, HENKA_SCRIPT_CALLBACK_BUDGET_EXHAUSTED, 100U, 0U, 0U};
    desc = default_desc(&fixture);
    desc.instruction_budget = 10U;
    assert(henka_script_behavior_runtime_create(&runtime) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_add(runtime, &desc, &behavior) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, behavior, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_ERROR_LIMIT);
    assert(report.result == HENKA_SCRIPT_BEHAVIOR_BUDGET_EXHAUSTED);
    henka_script_behavior_runtime_destroy(runtime);
}

static void test_bounds_disabled_unbound_and_generation(void)
{
    callback_fixture fixture = {{0}, 0U, HENKA_SCRIPT_CALLBACK_COMPLETED, 0U, 0U, 0U};
    henka_script_behavior_runtime* runtime = NULL;
    henka_script_behavior_desc desc = default_desc(&fixture);
    henka_script_behavior_handle first = HENKA_INVALID_SCRIPT_BEHAVIOR_HANDLE;
    henka_script_behavior_handle second = HENKA_INVALID_SCRIPT_BEHAVIOR_HANDLE;
    henka_script_behavior_report report;
    size_t index;

    assert(henka_script_behavior_runtime_create(&runtime) == HENKA_SUCCESS);
    desc.callback = NULL;
    assert(henka_script_behavior_runtime_add(runtime, &desc, &first) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, first, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_ERROR_ASSET_SOURCE);
    assert(report.result == HENKA_SCRIPT_BEHAVIOR_UNBOUND);
    assert(henka_script_behavior_runtime_reset(runtime, first) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_bind(runtime, first, callback_record, &fixture) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_set_enabled(runtime, first, false) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, first, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 2U, &report) == HENKA_SUCCESS);
    assert(report.result == HENKA_SCRIPT_BEHAVIOR_SKIPPED_DISABLED);
    assert(fixture.event_count == 0U);
    assert(henka_script_behavior_runtime_set_enabled(runtime, first, true) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch(
               runtime, first, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 3U, &report) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_remove(runtime, first) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_get(runtime, first, &(henka_script_behavior_snapshot){0}) == HENKA_ERROR_INVALID_ARGUMENT);
    desc.callback = callback_record;
    assert(henka_script_behavior_runtime_add(runtime, &desc, &second) == HENKA_SUCCESS);
    assert(second != first);
    assert(henka_script_behavior_runtime_get_count(runtime) == 1U);
    for (index = 0U; index < HENKA_SCRIPT_RUNTIME_MAX_BEHAVIORS - 1U; ++index)
    {
        henka_script_behavior_handle extra;
        assert(henka_script_behavior_runtime_add(runtime, &desc, &extra) == HENKA_SUCCESS);
    }
    assert(henka_script_behavior_runtime_add(runtime, &desc, &first) == HENKA_ERROR_LIMIT);
    henka_script_behavior_runtime_destroy(runtime);
}

static void test_batch_order_and_validation(void)
{
    callback_fixture first_fixture = {{0}, 0U, HENKA_SCRIPT_CALLBACK_COMPLETED, 1U, 0U, 0U};
    callback_fixture second_fixture = {{0}, 0U, HENKA_SCRIPT_CALLBACK_COMPLETED, 2U, 0U, 0U};
    henka_script_behavior_runtime* runtime = NULL;
    henka_script_behavior_desc first_desc = default_desc(&first_fixture);
    henka_script_behavior_desc second_desc = default_desc(&second_fixture);
    henka_script_behavior_handle first;
    henka_script_behavior_handle second;
    henka_script_behavior_batch_report report;

    second_desc.entity_id = 43U;
    assert(henka_script_behavior_runtime_create(&runtime) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_add(runtime, &first_desc, &first) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_add(runtime, &second_desc, &second) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 10U, &report) == HENKA_SUCCESS);
    assert(report.attempted == 2U && report.executed == 2U && report.failed == 0U);
    assert(first_fixture.event_count == 1U && second_fixture.event_count == 1U);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_UPDATE, -1.0f, 11U, &report) == HENKA_ERROR_INVALID_ARGUMENT);
    henka_script_behavior_runtime_destroy(runtime);
}

static void test_fixed_signal_targeting_and_destroy_order(void)
{
    callback_fixture first_fixture = {{0}, 0U, HENKA_SCRIPT_CALLBACK_COMPLETED, 1U, 0U, 0U};
    callback_fixture second_fixture = {{0}, 0U, HENKA_SCRIPT_CALLBACK_COMPLETED, 1U, 0U, 0U};
    callback_fixture unrelated_fixture = {{0}, 0U, HENKA_SCRIPT_CALLBACK_COMPLETED, 1U, 0U, 0U};
    henka_script_behavior_runtime* runtime = NULL;
    henka_script_behavior_handle first_behavior;
    henka_script_behavior_handle ignored;
    henka_script_behavior_batch_report report;
    henka_script_behavior_snapshot snapshot;
    henka_script_behavior_desc first_desc = default_desc(&first_fixture);

    assert(henka_script_behavior_runtime_create(&runtime) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_add(
               runtime, &first_desc, &first_behavior) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_add(
               runtime, &(henka_script_behavior_desc){
                   42U, HENKA_SCRIPT_LANGUAGE_HENKASCRIPT, true, 0U,
                   callback_record, &second_fixture}, &ignored) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_add(
               runtime, &(henka_script_behavior_desc){
                   99U, HENKA_SCRIPT_LANGUAGE_HENKASCRIPT, true, 0U,
                   callback_record, &unrelated_fixture}, &ignored) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_START, 0.0f, 1U, &report) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_dispatch_signal_for_entity(
               runtime,
               42U,
               HENKA_SCRIPT_LIFECYCLE_COLLISION_ENTER,
               1.0f / 60.0f,
               2U,
               0U,
               99U,
               99U,
               7U,
               &report) == HENKA_SUCCESS);
    assert(report.attempted == 2U && report.executed == 2U);
    assert(first_fixture.last_other_entity == 99U && first_fixture.last_event_type == 7U);
    assert(second_fixture.last_other_entity == 99U && second_fixture.last_event_type == 7U);
    assert(unrelated_fixture.event_count == 2U);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_FIXED_UPDATE, 1.0f / 60.0f, 3U, &report) == HENKA_SUCCESS);
    assert(first_fixture.events[first_fixture.event_count - 1U] == HENKA_SCRIPT_LIFECYCLE_FIXED_UPDATE);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_DESTROY, 0.0f, 4U, &report) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_get(
               runtime,
               first_behavior,
               &snapshot) == HENKA_SUCCESS);
    assert(snapshot.state == HENKA_SCRIPT_BEHAVIOR_DESTROYED);
    assert(henka_script_behavior_runtime_dispatch_all(
               runtime, HENKA_SCRIPT_LIFECYCLE_STOP, 0.0f, 5U, &report) == HENKA_SUCCESS);
    assert(henka_script_behavior_runtime_get(
               runtime, first_behavior, &snapshot) == HENKA_SUCCESS);
    assert(snapshot.state == HENKA_SCRIPT_BEHAVIOR_STOPPED);
    assert(first_fixture.events[first_fixture.event_count - 2U] == HENKA_SCRIPT_LIFECYCLE_DESTROY);
    assert(first_fixture.events[first_fixture.event_count - 1U] == HENKA_SCRIPT_LIFECYCLE_STOP);
    henka_script_behavior_runtime_destroy(runtime);
}

int main(void)
{
    const size_t allocations_before = henka_memory_get_allocation_count();
    test_lifecycle_transitions_and_reports();
    test_fail_closed_and_recovery();
    test_bounds_disabled_unbound_and_generation();
    test_batch_order_and_validation();
    test_fixed_signal_targeting_and_destroy_order();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_script_runtime_tests: PASS");
    return 0;
}
