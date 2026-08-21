#include <henka/script.h>

#include <string.h>

#include <henka/memory.h>

typedef struct henka_script_binding
{
    uint32_t api_id;
    const henka_script_api_function* function;
} henka_script_binding;

struct henka_script_host
{
    size_t binding_count;
    henka_script_binding bindings[HENKA_SCRIPT_HOST_MAX_BINDINGS];
};

static const henka_script_api_function g_schema[] =
{
    {
        HENKA_SCRIPT_API_ENTITY_IS_VALID,
        "Entity.IsValid",
        HENKA_SCRIPT_API_DOMAIN_ENTITY,
        HENKA_SCRIPT_API_VALUE_BOOL,
        1U,
        {HENKA_SCRIPT_API_VALUE_ENTITY, HENKA_SCRIPT_API_VALUE_VOID,
         HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}
    },
    {
        HENKA_SCRIPT_API_TRANSFORM_GET_POSITION,
        "Transform.GetPosition",
        HENKA_SCRIPT_API_DOMAIN_TRANSFORM,
        HENKA_SCRIPT_API_VALUE_VEC3,
        1U,
        {HENKA_SCRIPT_API_VALUE_ENTITY, HENKA_SCRIPT_API_VALUE_VOID,
         HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}
    },
    {
        HENKA_SCRIPT_API_TRANSFORM_SET_POSITION,
        "Transform.SetPosition",
        HENKA_SCRIPT_API_DOMAIN_TRANSFORM,
        HENKA_SCRIPT_API_VALUE_RESULT,
        2U,
        {HENKA_SCRIPT_API_VALUE_ENTITY, HENKA_SCRIPT_API_VALUE_VEC3,
         HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}
    },
    {
        HENKA_SCRIPT_API_INPUT_IS_ACTION_DOWN,
        "Input.IsActionDown",
        HENKA_SCRIPT_API_DOMAIN_INPUT,
        HENKA_SCRIPT_API_VALUE_BOOL,
        1U,
        {HENKA_SCRIPT_API_VALUE_ACTION_ID, HENKA_SCRIPT_API_VALUE_VOID,
         HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}
    },
    {
        HENKA_SCRIPT_API_PHYSICS_APPLY_IMPULSE,
        "Physics.ApplyImpulse",
        HENKA_SCRIPT_API_DOMAIN_PHYSICS,
        HENKA_SCRIPT_API_VALUE_RESULT,
        2U,
        {HENKA_SCRIPT_API_VALUE_ENTITY, HENKA_SCRIPT_API_VALUE_VEC3,
         HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}
    },
    {
        HENKA_SCRIPT_API_INTERACTION_TRY,
        "Interaction.Try",
        HENKA_SCRIPT_API_DOMAIN_INTERACTION,
        HENKA_SCRIPT_API_VALUE_RESULT,
        1U,
        {HENKA_SCRIPT_API_VALUE_ENTITY, HENKA_SCRIPT_API_VALUE_VOID,
         HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}
    },
    {
        HENKA_SCRIPT_API_EVENTS_EMIT,
        "Events.Emit",
        HENKA_SCRIPT_API_DOMAIN_EVENTS,
        HENKA_SCRIPT_API_VALUE_RESULT,
        2U,
        {HENKA_SCRIPT_API_VALUE_EVENT_ID, HENKA_SCRIPT_API_VALUE_ENTITY,
         HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}
    }
};

static const size_t g_schema_count = sizeof(g_schema) / sizeof(g_schema[0]);

static bool henka_script_schema_is_valid(void)
{
    size_t index;
    for (index = 0U; index < g_schema_count; ++index)
    {
        size_t compare_index;
        if (g_schema[index].id == 0U ||
            g_schema[index].name == NULL ||
            g_schema[index].parameter_count > HENKA_SCRIPT_API_MAX_PARAMETERS)
        {
            return false;
        }
        for (compare_index = index + 1U; compare_index < g_schema_count; ++compare_index)
        {
            if (g_schema[index].id == g_schema[compare_index].id ||
                strcmp(g_schema[index].name, g_schema[compare_index].name) == 0)
            {
                return false;
            }
        }
    }
    return true;
}

henka_result henka_script_api_schema_get(
    const henka_script_api_function** out_functions,
    size_t* out_count)
{
    if (out_functions == NULL || out_count == NULL || !henka_script_schema_is_valid())
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_functions = g_schema;
    *out_count = g_schema_count;
    return HENKA_SUCCESS;
}

henka_result henka_script_api_schema_find_by_id(
    uint32_t id,
    const henka_script_api_function** out_function)
{
    size_t index;
    if (out_function == NULL || id == 0U || !henka_script_schema_is_valid())
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_function = NULL;
    for (index = 0U; index < g_schema_count; ++index)
    {
        if (g_schema[index].id == id)
        {
            *out_function = &g_schema[index];
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

henka_result henka_script_api_schema_find_by_name(
    const char* name,
    const henka_script_api_function** out_function)
{
    size_t index;
    if (name == NULL || name[0] == '\0' || out_function == NULL ||
        !henka_script_schema_is_valid())
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_function = NULL;
    for (index = 0U; index < g_schema_count; ++index)
    {
        if (strcmp(g_schema[index].name, name) == 0)
        {
            *out_function = &g_schema[index];
            return HENKA_SUCCESS;
        }
    }
    return HENKA_ERROR_INVALID_ARGUMENT;
}

henka_result henka_script_host_create(henka_script_host** out_host)
{
    henka_script_host* host;
    const henka_script_api_function* functions;
    size_t count;
    if (out_host == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_host = NULL;
    if (henka_script_api_schema_get(&functions, &count) != HENKA_SUCCESS ||
        count > HENKA_SCRIPT_HOST_MAX_BINDINGS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    host = (henka_script_host*)henka_calloc(1U, sizeof(*host));
    if (host == NULL)
    {
        return HENKA_ERROR_OUT_OF_MEMORY;
    }
    *out_host = host;
    return HENKA_SUCCESS;
}

void henka_script_host_destroy(henka_script_host* host)
{
    henka_free(host);
}

henka_result henka_script_host_bind_api(
    henka_script_host* host,
    uint32_t api_id,
    size_t* out_binding_index)
{
    const henka_script_api_function* function;
    size_t index;
    if (host == NULL || out_binding_index == NULL ||
        henka_script_api_schema_find_by_id(api_id, &function) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < host->binding_count; ++index)
    {
        if (host->bindings[index].api_id == api_id)
        {
            *out_binding_index = index;
            return HENKA_SUCCESS;
        }
    }
    if (host->binding_count >= HENKA_SCRIPT_HOST_MAX_BINDINGS)
    {
        return HENKA_ERROR_LIMIT;
    }
    index = host->binding_count++;
    host->bindings[index].api_id = api_id;
    host->bindings[index].function = function;
    *out_binding_index = index;
    return HENKA_SUCCESS;
}

henka_result henka_script_host_get_binding(
    const henka_script_host* host,
    size_t binding_index,
    const henka_script_api_function** out_function)
{
    if (host == NULL || out_function == NULL ||
        binding_index >= host->binding_count)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_function = host->bindings[binding_index].function;
    return HENKA_SUCCESS;
}

size_t henka_script_host_get_binding_count(const henka_script_host* host)
{
    return host == NULL ? 0U : host->binding_count;
}
