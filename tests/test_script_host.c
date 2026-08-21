#include <string.h>

#include <henka/memory.h>
#include <henka/script.h>

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
        functions == NULL || count != 7U)
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
    result = 0;

cleanup:
    henka_script_host_destroy(host);
    if (henka_memory_get_allocation_count() != 0U)
    {
        return 1;
    }
    return result;
}
