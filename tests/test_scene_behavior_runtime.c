#include <assert.h>
#include <stdio.h>

#include <henka/memory.h>
#include <henka/scene_behavior_runtime.h>
#include <henka/script_state.h>

static henka_scene_document_object make_object_with_behaviors(void)
{
    henka_scene_document_object object = henka_scene_document_object_default();
    (void)snprintf(object.name, sizeof(object.name), "%s", "ScriptObject");
    object.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE;
    object.source.primitive = HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX;
    object.source.primitive_dimensions = (henka_vec3){1.0f, 1.0f, 1.0f};
    object.behaviors[0] = henka_scene_document_behavior_default();
    object.behaviors[0].id = 10U;
    object.behaviors[0].language = HENKA_SCRIPT_LANGUAGE_HENKASCRIPT;
    object.behaviors[0].enabled = true;
    (void)snprintf(
        object.behaviors[0].asset_path,
        sizeof(object.behaviors[0].asset_path),
        "%s",
        "scripts/mixed.hks");
    object.behaviors[1] = henka_scene_document_behavior_default();
    object.behaviors[1].id = 11U;
    object.behaviors[1].language = HENKA_SCRIPT_LANGUAGE_LUA;
    object.behaviors[1].enabled = true;
    (void)snprintf(
        object.behaviors[1].asset_path,
        sizeof(object.behaviors[1].asset_path),
        "%s",
        "scripts/mixed.lua");
    object.behavior_count = 2U;
    return object;
}

static void test_scene_behavior_runtime_dispatch(void)
{
    henka_scene_document* document = NULL;
    henka_scene_document_object object = make_object_with_behaviors();
    henka_scene_behavior_runtime* runtime = NULL;
    henka_script_behavior_batch_report report;
    henka_scene_document_id ignored_id;

    assert(henka_scene_document_create(&document) == HENKA_SUCCESS);
    assert(henka_scene_document_add_object(
               document, &object, &ignored_id) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_create(
               document, "tests/fixtures", 64U, &runtime) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_get_behavior_count(runtime) == 2U);
    assert(henka_scene_behavior_runtime_dispatch(
               runtime, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_SUCCESS);
    assert(report.attempted == 2U && report.executed == 2U && report.failed == 0U);
    assert(henka_scene_behavior_runtime_dispatch(
               runtime, HENKA_SCRIPT_LIFECYCLE_START, 0.0f, 2U, &report) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_dispatch(
               runtime, HENKA_SCRIPT_LIFECYCLE_UPDATE, 0.016f, 3U, &report) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_dispatch(
               runtime, HENKA_SCRIPT_LIFECYCLE_STOP, 0.0f, 4U, &report) == HENKA_SUCCESS);
    henka_scene_behavior_runtime_destroy(runtime);
    henka_scene_document_destroy(document);
}

static void test_scene_behavior_runtime_fails_closed(void)
{
    henka_scene_document* document = NULL;
    henka_scene_document_object object = henka_scene_document_object_default();
    henka_scene_behavior_runtime* runtime = NULL;
    henka_scene_document_id ignored_id;

    (void)snprintf(object.name, sizeof(object.name), "%s", "MissingScriptObject");
    object.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE;
    object.source.primitive = HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX;
    object.source.primitive_dimensions = (henka_vec3){1.0f, 1.0f, 1.0f};
    object.behavior_count = 1U;
    object.behaviors[0] = henka_scene_document_behavior_default();
    object.behaviors[0].id = 10U;
    object.behaviors[0].language = HENKA_SCRIPT_LANGUAGE_LUA;
    (void)snprintf(
        object.behaviors[0].asset_path,
        sizeof(object.behaviors[0].asset_path),
        "%s",
        "scripts/missing.lua");
    assert(henka_scene_document_create(&document) == HENKA_SUCCESS);
    assert(henka_scene_document_add_object(
               document, &object, &ignored_id) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_create(
               document, "tests/fixtures", 64U, &runtime) == HENKA_ERROR_ASSET_SOURCE);
    assert(runtime == NULL);
    henka_scene_document_destroy(document);
}

static void test_scene_behavior_runtime_event_drain(void)
{
    henka_scene_document* document = NULL;
    henka_scene_document_object object = make_object_with_behaviors();
    henka_scene_document_object bound_object;
    henka_scene_behavior_runtime* runtime = NULL;
    henka_script_host* host = NULL;
    henka_script_state_store* store = NULL;
    henka_script_behavior_batch_report report;
    henka_script_state_value value;
    bool present;
    henka_scene_document_id ignored_id;

    assert(henka_scene_document_create(&document) == HENKA_SUCCESS);
    assert(henka_scene_document_add_object(
               document, &object, &ignored_id) == HENKA_SUCCESS);
    assert(henka_scene_document_get_object_at(
               document, 0U, &bound_object) == HENKA_SUCCESS);
    assert(henka_script_host_create(&host) == HENKA_SUCCESS);
    assert(henka_script_state_store_create(&store) == HENKA_SUCCESS);
    assert(henka_script_host_bind_api(
               host, HENKA_SCRIPT_API_STATE_SET_I32, &(size_t){0U}) == HENKA_SUCCESS);
    assert(henka_script_host_set_state_store(host, store) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_create_with_host(
               document, "tests/fixtures", 64U, host, &runtime) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_dispatch(
               runtime, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_dispatch(
               runtime, HENKA_SCRIPT_LIFECYCLE_START, 0.0f, 2U, &report) == HENKA_SUCCESS);
    assert(henka_script_host_emit_event(host, 7U, bound_object.id, 3U) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_dispatch_events(runtime, &report) == HENKA_SUCCESS);
    assert(report.event == HENKA_SCRIPT_LIFECYCLE_EVENT);
    assert(report.attempted == 2U && report.executed == 2U && report.failed == 0U);
    assert(henka_script_state_store_get(
               store, (henka_script_state_identity){bound_object.id, 10U}, 80U,
               &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_I32 && value.as.i32 == 7);
    assert(henka_script_state_store_get(
               store, (henka_script_state_identity){bound_object.id, 11U}, 90U,
               &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_I32 && value.as.i32 == 7);
    assert(henka_script_host_get_pending_event_count(host) == 0U);
    henka_scene_behavior_runtime_destroy(runtime);
    henka_script_state_store_destroy(store);
    henka_script_host_destroy(host);
    henka_scene_document_destroy(document);
}

static void test_scene_behavior_runtime_mixed_signal(void)
{
    henka_scene_document* document = NULL;
    henka_scene_document_object object = make_object_with_behaviors();
    henka_scene_behavior_runtime* runtime = NULL;
    henka_script_host* host = NULL;
    henka_script_state_store* store = NULL;
    henka_script_behavior_batch_report report;
    henka_script_state_value value;
    bool present;
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;

    (void)snprintf(
        object.behaviors[0].asset_path,
        sizeof(object.behaviors[0].asset_path),
        "%s",
        "scripts/contact.hks");
    (void)snprintf(
        object.behaviors[1].asset_path,
        sizeof(object.behaviors[1].asset_path),
        "%s",
        "scripts/contact.lua");
    assert(henka_scene_document_create(&document) == HENKA_SUCCESS);
    assert(henka_scene_document_add_object(document, &object, &object_id) == HENKA_SUCCESS);
    assert(henka_script_host_create(&host) == HENKA_SUCCESS);
    assert(henka_script_state_store_create(&store) == HENKA_SUCCESS);
    assert(henka_script_host_bind_api(
               host, HENKA_SCRIPT_API_STATE_SET_I32, &(size_t){0U}) == HENKA_SUCCESS);
    assert(henka_script_host_set_state_store(host, store) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_create_with_host(
               document, "tests/fixtures", 64U, host, &runtime) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_dispatch(
               runtime, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_dispatch(
               runtime, HENKA_SCRIPT_LIFECYCLE_START, 0.0f, 2U, &report) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_dispatch_signal_for_entity(
               runtime,
               object_id,
               HENKA_SCRIPT_LIFECYCLE_COLLISION_ENTER,
               1.0f / 60.0f,
               3U,
               0U,
               99U,
               99U,
               7U,
               &report) == HENKA_SUCCESS);
    assert(report.attempted == 2U && report.executed == 2U && report.failed == 0U);
    assert(henka_script_state_store_get(
               store, (henka_script_state_identity){object_id, 10U}, 81U,
               &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_I32 && value.as.i32 == 7);
    assert(henka_script_state_store_get(
               store, (henka_script_state_identity){object_id, 11U}, 81U,
               &value, &present) == HENKA_SUCCESS);
    assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_I32 && value.as.i32 == 7);
    henka_scene_behavior_runtime_destroy(runtime);
    henka_script_state_store_destroy(store);
    henka_script_host_destroy(host);
    henka_scene_document_destroy(document);
}

static henka_scene_document_object make_event_pair_object(
    henka_script_language publisher_language,
    const char* publisher_path,
    henka_script_language subscriber_language,
    const char* subscriber_path)
{
    henka_scene_document_object object = henka_scene_document_object_default();
    (void)snprintf(object.name, sizeof(object.name), "%s", "EventPair");
    object.source.kind = HENKA_SCENE_DOCUMENT_SOURCE_PRIMITIVE;
    object.source.primitive = HENKA_SCENE_DOCUMENT_PRIMITIVE_BOX;
    object.source.primitive_dimensions = (henka_vec3){1.0f, 1.0f, 1.0f};
    object.behaviors[0] = henka_scene_document_behavior_default();
    object.behaviors[0].id = 20U;
    object.behaviors[0].language = publisher_language;
    (void)snprintf(
        object.behaviors[0].asset_path,
        sizeof(object.behaviors[0].asset_path),
        "%s",
        publisher_path);
    object.behaviors[1] = henka_scene_document_behavior_default();
    object.behaviors[1].id = 21U;
    object.behaviors[1].language = subscriber_language;
    (void)snprintf(
        object.behaviors[1].asset_path,
        sizeof(object.behaviors[1].asset_path),
        "%s",
        subscriber_path);
    object.behavior_count = 2U;
    return object;
}

static void test_scene_behavior_runtime_cross_language_events(void)
{
    const struct event_case
    {
        henka_script_language publisher_language;
        const char* publisher_path;
        henka_script_language subscriber_language;
        const char* subscriber_path;
        uint32_t expected_event;
        uint32_t subscriber_state_key;
    } cases[] = {
        {
            HENKA_SCRIPT_LANGUAGE_HENKASCRIPT,
            "scripts/publisher.hks",
            HENKA_SCRIPT_LANGUAGE_LUA,
            "scripts/subscriber.lua",
            7U,
            90U},
        {
            HENKA_SCRIPT_LANGUAGE_LUA,
            "scripts/publisher.lua",
            HENKA_SCRIPT_LANGUAGE_HENKASCRIPT,
            "scripts/subscriber.hks",
            8U,
            80U}};
    for (size_t case_index = 0U; case_index < sizeof(cases) / sizeof(cases[0]); ++case_index)
    {
        henka_scene_document* document = NULL;
        henka_scene_behavior_runtime* runtime = NULL;
        henka_script_host* host = NULL;
        henka_script_state_store* store = NULL;
        henka_script_behavior_batch_report report;
        henka_script_state_value value;
        bool present;
        henka_scene_document_id object_id;
        henka_result event_result;
        const henka_scene_document_object object = make_event_pair_object(
            cases[case_index].publisher_language,
            cases[case_index].publisher_path,
            cases[case_index].subscriber_language,
            cases[case_index].subscriber_path);

        assert(henka_scene_document_create(&document) == HENKA_SUCCESS);
        assert(henka_scene_document_add_object(document, &object, &object_id) == HENKA_SUCCESS);
        assert(henka_script_host_create(&host) == HENKA_SUCCESS);
        assert(henka_script_state_store_create(&store) == HENKA_SUCCESS);
        assert(henka_script_host_bind_api(
                   host, HENKA_SCRIPT_API_EVENTS_EMIT, &(size_t){0U}) == HENKA_SUCCESS);
        assert(henka_script_host_bind_api(
                   host, HENKA_SCRIPT_API_STATE_SET_I32, &(size_t){0U}) == HENKA_SUCCESS);
        assert(henka_script_host_set_state_store(host, store) == HENKA_SUCCESS);
        assert(henka_scene_behavior_runtime_create_with_host(
                   document, "tests/fixtures", 128U, host, &runtime) == HENKA_SUCCESS);
        assert(henka_scene_behavior_runtime_dispatch(
                   runtime, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_SUCCESS);
        assert(henka_script_host_get_pending_event_count(host) == 1U);
        assert(henka_scene_behavior_runtime_dispatch(
                   runtime, HENKA_SCRIPT_LIFECYCLE_START, 0.0f, 1U, &report) == HENKA_SUCCESS);
        event_result = henka_scene_behavior_runtime_dispatch_events(runtime, &report);
        assert(event_result == HENKA_SUCCESS);
        assert(report.attempted == 2U && report.executed == 2U && report.failed == 0U);
        assert(henka_script_state_store_get(
                   store,
                   (henka_script_state_identity){object_id, 21U},
                   cases[case_index].subscriber_state_key,
                   &value,
                   &present) == HENKA_SUCCESS);
        assert(present && value.type == HENKA_SCRIPT_STATE_VALUE_I32 &&
               value.as.i32 == (int32_t)cases[case_index].expected_event);
        henka_scene_behavior_runtime_destroy(runtime);
        henka_script_state_store_destroy(store);
        henka_script_host_destroy(host);
        henka_scene_document_destroy(document);
    }
}

static void test_scene_behavior_runtime_reload_fails_closed(void)
{
    henka_scene_document* document = NULL;
    henka_scene_document_object object = make_object_with_behaviors();
    henka_scene_document_behavior replacement;
    henka_scene_behavior_runtime* runtime = NULL;
    henka_script_behavior_batch_report report;
    henka_scene_document_id object_id = HENKA_INVALID_SCENE_DOCUMENT_ID;

    assert(henka_scene_document_create(&document) == HENKA_SUCCESS);
    assert(henka_scene_document_add_object(document, &object, &object_id) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_create(
               document, "tests/fixtures", 64U, &runtime) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_dispatch(
               runtime, HENKA_SCRIPT_LIFECYCLE_CREATE, 0.0f, 1U, &report) == HENKA_SUCCESS);
    assert(henka_scene_behavior_runtime_dispatch(
               runtime, HENKA_SCRIPT_LIFECYCLE_START, 0.0f, 2U, &report) == HENKA_SUCCESS);

    replacement = object.behaviors[0];
    (void)snprintf(
        replacement.asset_path,
        sizeof(replacement.asset_path),
        "%s",
        "scripts/reload-missing.hks");
    assert(henka_scene_behavior_runtime_reload_behavior(
               runtime, "tests/fixtures", &replacement, object_id) ==
           HENKA_ERROR_ASSET_SOURCE);
    assert(henka_scene_behavior_runtime_dispatch(
               runtime, HENKA_SCRIPT_LIFECYCLE_UPDATE, 0.016f, 3U, &report) == HENKA_SUCCESS);
    assert(report.attempted == 2U && report.executed == 2U && report.failed == 0U);

    henka_scene_behavior_runtime_destroy(runtime);
    henka_scene_document_destroy(document);
}

int main(void)
{
    const size_t allocations_before = henka_memory_get_allocation_count();
    test_scene_behavior_runtime_dispatch();
    test_scene_behavior_runtime_fails_closed();
    test_scene_behavior_runtime_event_drain();
    test_scene_behavior_runtime_mixed_signal();
    test_scene_behavior_runtime_cross_language_events();
    test_scene_behavior_runtime_reload_fails_closed();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_scene_behavior_runtime_tests: PASS");
    return 0;
}
