#include <string.h>

#include <henka/memory.h>
#include <henka/script.h>

typedef struct script_host_dispatch_fixture
{
    henka_script_host* host;
    size_t calls;
    henka_result nested_result;
} script_host_dispatch_fixture;

static henka_result dispatch_transform_set_position(
    void* user_data,
    uint32_t api_id,
    const henka_script_api_value* arguments,
    size_t argument_count,
    henka_script_api_value* out_value)
{
    script_host_dispatch_fixture* fixture =
        (script_host_dispatch_fixture*)user_data;
    if (fixture == NULL || arguments == NULL || out_value == NULL ||
        api_id != HENKA_SCRIPT_API_TRANSFORM_SET_POSITION ||
        argument_count != 2U ||
        arguments[0].type != HENKA_SCRIPT_API_VALUE_ENTITY ||
        arguments[1].type != HENKA_SCRIPT_API_VALUE_VEC3)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    ++fixture->calls;
    fixture->nested_result = henka_script_host_invoke(
        fixture->host,
        api_id,
        arguments,
        argument_count,
        out_value);
    out_value->type = HENKA_SCRIPT_API_VALUE_RESULT;
    out_value->as.result = HENKA_SUCCESS;
    return HENKA_SUCCESS;
}

static int test_typed_dispatch_and_non_reentrancy(henka_script_host* host)
{
    script_host_dispatch_fixture fixture = {host, 0U, HENKA_SUCCESS};
    henka_script_api_value arguments[2] =
    {
        {HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = 9U}},
        {HENKA_SCRIPT_API_VALUE_VEC3, {.vec3 = {1.0f, 2.0f, 3.0f}}}
    };
    henka_script_api_value output;
    if (henka_script_host_bind_api(
            host, HENKA_SCRIPT_API_TRANSFORM_SET_POSITION, &(size_t){0U}) != HENKA_SUCCESS ||
        henka_script_host_set_dispatcher(
            host, dispatch_transform_set_position, &fixture) != HENKA_SUCCESS ||
        henka_script_host_invoke(
            host,
            HENKA_SCRIPT_API_TRANSFORM_SET_POSITION,
            arguments,
            1U,
            &output) == HENKA_SUCCESS ||
        henka_script_host_invoke(
            host,
            HENKA_SCRIPT_API_TRANSFORM_SET_POSITION,
            arguments,
            2U,
            &output) != HENKA_SUCCESS ||
        fixture.calls != 1U || fixture.nested_result != HENKA_ERROR_INVALID_ARGUMENT ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT ||
        output.as.result != HENKA_SUCCESS)
    {
        return 1;
    }
    arguments[0].type = HENKA_SCRIPT_API_VALUE_BOOL;
    if (henka_script_host_invoke(
            host,
            HENKA_SCRIPT_API_TRANSFORM_SET_POSITION,
            arguments,
            2U,
            &output) == HENKA_SUCCESS)
    {
        return 1;
    }
    return 0;
}

static int test_bounded_event_queue(void)
{
    henka_script_host* host = NULL;
    henka_script_api_value arguments[2] =
    {
        {HENKA_SCRIPT_API_VALUE_EVENT_ID, {.event_id = 77U}},
        {HENKA_SCRIPT_API_VALUE_ENTITY, {.entity = 42U}}
    };
    henka_script_api_value output;
    henka_script_event event;
    size_t index;
    if (henka_script_host_create(&host) != HENKA_SUCCESS ||
        henka_script_host_bind_api(host, HENKA_SCRIPT_API_EVENTS_EMIT, &(size_t){0U}) != HENKA_SUCCESS ||
        henka_script_host_invoke(
            host, HENKA_SCRIPT_API_EVENTS_EMIT, arguments, 2U, &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT ||
        output.as.result != HENKA_SUCCESS ||
        henka_script_host_get_pending_event_count(host) != 1U ||
        henka_script_host_poll_event(host, &event) != HENKA_SUCCESS ||
        event.event_id != 77U || event.source_entity != 42U ||
        henka_script_host_poll_event(host, &event) == HENKA_SUCCESS)
    {
        henka_script_host_destroy(host);
        return 1;
    }
    for (index = 0U; index < HENKA_SCRIPT_HOST_MAX_EVENTS; ++index)
    {
        if (henka_script_host_emit_event(host, (uint32_t)(index + 1U), 0U, index) != HENKA_SUCCESS)
        {
            henka_script_host_destroy(host);
            return 1;
        }
    }
    if (henka_script_host_emit_event(host, 999U, 0U, 0U) != HENKA_ERROR_LIMIT)
    {
        henka_script_host_destroy(host);
        return 1;
    }
    for (index = 0U; index < HENKA_SCRIPT_HOST_MAX_EVENTS; ++index)
    {
        if (henka_script_host_poll_event(host, &event) != HENKA_SUCCESS ||
            event.event_id != (uint32_t)(index + 1U) || event.frame_index != index)
        {
            henka_script_host_destroy(host);
            return 1;
        }
    }
    henka_script_host_destroy(host);
    return 0;
}

static int test_state_api_context_isolation(void)
{
    henka_script_host* host = NULL;
    henka_script_state_store* store = NULL;
    henka_script_api_value arguments[2];
    henka_script_api_value output;
    bool present;
    henka_script_state_value state_value;
    if (henka_script_host_create(&host) != HENKA_SUCCESS ||
        henka_script_state_store_create(&store) != HENKA_SUCCESS ||
        henka_script_host_bind_api(host, HENKA_SCRIPT_API_STATE_GET_I32, &(size_t){0U}) != HENKA_SUCCESS ||
        henka_script_host_bind_api(host, HENKA_SCRIPT_API_STATE_SET_I32, &(size_t){0U}) != HENKA_SUCCESS ||
        henka_script_host_bind_api(host, HENKA_SCRIPT_API_STATE_GET_BOOL, &(size_t){0U}) != HENKA_SUCCESS ||
        henka_script_host_bind_api(host, HENKA_SCRIPT_API_STATE_SET_BOOL, &(size_t){0U}) != HENKA_SUCCESS ||
        henka_script_host_set_state_store(host, store) != HENKA_SUCCESS ||
        henka_script_host_set_execution_context(
            host, (henka_script_state_identity){9U, 90U}) != HENKA_SUCCESS)
    {
        henka_script_state_store_destroy(store);
        henka_script_host_destroy(host);
        return 1;
    }

    arguments[0] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_STATE_KEY, {.state_key = 7U}};
    arguments[1] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_I32, {.i32 = -42}};
    if (henka_script_host_invoke(
            host, HENKA_SCRIPT_API_STATE_SET_I32, arguments, 2U, &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_RESULT ||
        output.as.result != HENKA_SUCCESS ||
        henka_script_host_invoke(
            host, HENKA_SCRIPT_API_STATE_GET_I32, arguments, 1U, &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_I32 ||
        output.as.i32 != -42 || !output.present ||
        henka_script_state_store_get(
            store, (henka_script_state_identity){9U, 90U}, 7U,
            &state_value, &present) != HENKA_SUCCESS ||
        !present || state_value.type != HENKA_SCRIPT_STATE_VALUE_I32 ||
        state_value.as.i32 != -42)
    {
        henka_script_state_store_destroy(store);
        henka_script_host_destroy(host);
        return 1;
    }

    arguments[1] = (henka_script_api_value){
        HENKA_SCRIPT_API_VALUE_BOOL, {.boolean = true}};
    if (henka_script_host_set_execution_context(
            host, (henka_script_state_identity){9U, 91U}) != HENKA_SUCCESS ||
        henka_script_host_invoke(
            host, HENKA_SCRIPT_API_STATE_GET_BOOL, arguments, 1U, &output) != HENKA_SUCCESS ||
        output.type != HENKA_SCRIPT_API_VALUE_BOOL || output.as.boolean || output.present ||
        henka_script_host_set_execution_context(
            host, (henka_script_state_identity){0U, 1U}) != HENKA_ERROR_INVALID_ARGUMENT ||
        henka_script_host_set_execution_context(
            host, (henka_script_state_identity){0U, 0U}) != HENKA_SUCCESS ||
        henka_script_host_invoke(
            host, HENKA_SCRIPT_API_STATE_GET_I32, arguments, 1U, &output) != HENKA_ERROR_INVALID_ARGUMENT)
    {
        henka_script_state_store_destroy(store);
        henka_script_host_destroy(host);
        return 1;
    }
    henka_script_state_store_destroy(store);
    henka_script_host_destroy(host);
    return 0;
}

int main(void)
{
    const henka_script_api_function* functions = NULL;
    const henka_script_api_function* function = NULL;
    henka_script_host* host = NULL;
    size_t count = 0U;
    size_t index;
    size_t binding_index = 0U;
    int result = 1;

    if (henka_script_api_schema_get(&functions, &count) != HENKA_SUCCESS ||
        functions == NULL || count != 23U)
    {
        goto cleanup;
    }
    for (index = 0U; index < count; ++index)
    {
        size_t compare_index;
        if (functions[index].id == 0U || functions[index].name == NULL ||
            functions[index].parameter_count > HENKA_SCRIPT_API_MAX_PARAMETERS)
        {
            goto cleanup;
        }
        if (index > 0U && functions[index - 1U].id >= functions[index].id)
        {
            goto cleanup;
        }
        for (compare_index = index + 1U; compare_index < count; ++compare_index)
        {
            if (functions[index].id == functions[compare_index].id ||
                strcmp(functions[index].name, functions[compare_index].name) == 0)
            {
                goto cleanup;
            }
        }
    }
    if (henka_script_api_schema_find_by_id(
            HENKA_SCRIPT_API_TRANSFORM_SET_POSITION,
            &function) != HENKA_SUCCESS ||
        function == NULL || strcmp(function->name, "Transform.SetPosition") != 0 ||
        function->parameter_count != 2U ||
        function->return_type != HENKA_SCRIPT_API_VALUE_RESULT)
    {
        goto cleanup;
    }
    function = NULL;
    if (henka_script_api_schema_find_by_name(
            "Physics.ApplyImpulse",
            &function) != HENKA_SUCCESS ||
        function == NULL ||
        function->id != HENKA_SCRIPT_API_PHYSICS_APPLY_IMPULSE)
    {
        goto cleanup;
    }
    if (henka_script_api_schema_find_by_id(UINT32_C(0xFFFF), &function) == HENKA_SUCCESS ||
        function != NULL ||
        henka_script_api_schema_find_by_name("", &function) == HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (henka_script_host_create(&host) != HENKA_SUCCESS || host == NULL ||
        henka_script_host_get_binding_count(host) != 0U)
    {
        goto cleanup;
    }
    if (henka_script_host_bind_api(
            host,
            HENKA_SCRIPT_API_TRANSFORM_SET_POSITION,
            &binding_index) != HENKA_SUCCESS ||
        binding_index != 0U ||
        henka_script_host_bind_api(
            host,
            HENKA_SCRIPT_API_TRANSFORM_SET_POSITION,
            &binding_index) != HENKA_SUCCESS ||
        binding_index != 0U ||
        henka_script_host_get_binding(host, binding_index, &function) != HENKA_SUCCESS ||
        function == NULL || function->id != HENKA_SCRIPT_API_TRANSFORM_SET_POSITION ||
        henka_script_host_get_binding_count(host) != 1U ||
        henka_script_host_bind_api(host, UINT32_C(0xFFFF), &binding_index) == HENKA_SUCCESS)
    {
        goto cleanup;
    }
    if (test_typed_dispatch_and_non_reentrancy(host) != 0)
    {
        goto cleanup;
    }
    if (test_bounded_event_queue() != 0)
    {
        goto cleanup;
    }
    if (test_state_api_context_isolation() != 0)
    {
        goto cleanup;
    }
    result = 0;

cleanup:
    henka_script_host_destroy(host);
    if (henka_memory_get_allocation_count() != 0U)
    {
        return 1;
    }
    return result;
}
