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

int main(void)
{
    const size_t allocations_before = henka_memory_get_allocation_count();
    test_scene_behavior_runtime_dispatch();
    test_scene_behavior_runtime_fails_closed();
    test_scene_behavior_runtime_event_drain();
    assert(henka_memory_get_allocation_count() == allocations_before);
    puts("henka_scene_behavior_runtime_tests: PASS");
    return 0;
}
