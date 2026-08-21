#include <henka/script.h>

#include <math.h>
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
    henka_script_host_dispatch_callback dispatcher;
    void* dispatcher_user_data;
    bool dispatching;
    henka_script_state_store* state_store;
    henka_script_state_identity state_identity;
    bool state_context_active;
    size_t event_head;
    size_t event_count;
    henka_script_event events[HENKA_SCRIPT_HOST_MAX_EVENTS];
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
    },
    {
        HENKA_SCRIPT_API_STATE_GET_I32,
        "State.GetI32",
        HENKA_SCRIPT_API_DOMAIN_STATE,
        HENKA_SCRIPT_API_VALUE_I32,
        1U,
        {HENKA_SCRIPT_API_VALUE_STATE_KEY, HENKA_SCRIPT_API_VALUE_VOID,
         HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}
    },
    {
        HENKA_SCRIPT_API_STATE_SET_I32,
        "State.SetI32",
        HENKA_SCRIPT_API_DOMAIN_STATE,
        HENKA_SCRIPT_API_VALUE_RESULT,
        2U,
        {HENKA_SCRIPT_API_VALUE_STATE_KEY, HENKA_SCRIPT_API_VALUE_I32,
         HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}
    },
    {
        HENKA_SCRIPT_API_STATE_GET_BOOL,
        "State.GetBool",
        HENKA_SCRIPT_API_DOMAIN_STATE,
        HENKA_SCRIPT_API_VALUE_BOOL,
        1U,
        {HENKA_SCRIPT_API_VALUE_STATE_KEY, HENKA_SCRIPT_API_VALUE_VOID,
         HENKA_SCRIPT_API_VALUE_VOID, HENKA_SCRIPT_API_VALUE_VOID}
    },
    {
        HENKA_SCRIPT_API_STATE_SET_BOOL,
        "State.SetBool",
        HENKA_SCRIPT_API_DOMAIN_STATE,
        HENKA_SCRIPT_API_VALUE_RESULT,
        2U,
        {HENKA_SCRIPT_API_VALUE_STATE_KEY, HENKA_SCRIPT_API_VALUE_BOOL,
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

henka_result henka_script_host_set_state_store(
    henka_script_host* host,
    henka_script_state_store* store)
{
    if (host == NULL || host->dispatching)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    host->state_store = store;
    return HENKA_SUCCESS;
}

henka_result henka_script_host_set_execution_context(
    henka_script_host* host,
    henka_script_state_identity identity)
{
    if (host == NULL || host->dispatching ||
        (identity.entity_id == 0U && identity.behavior_id != 0U))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    host->state_identity = identity;
    host->state_context_active = identity.entity_id != 0U &&
        identity.behavior_id != 0U;
    return HENKA_SUCCESS;
}

static bool henka_script_host_has_binding(
    const henka_script_host* host,
    uint32_t api_id)
{
    size_t index;
    if (host == NULL)
    {
        return false;
    }
    for (index = 0U; index < host->binding_count; ++index)
    {
        if (host->bindings[index].api_id == api_id)
        {
            return true;
        }
    }
    return false;
}

static bool henka_script_api_value_is_valid(
    const henka_script_api_value* value)
{
    if (value == NULL)
    {
        return false;
    }
    switch (value->type)
    {
        case HENKA_SCRIPT_API_VALUE_VOID:
        case HENKA_SCRIPT_API_VALUE_BOOL:
        case HENKA_SCRIPT_API_VALUE_I32:
        case HENKA_SCRIPT_API_VALUE_ENTITY:
        case HENKA_SCRIPT_API_VALUE_ACTION_ID:
        case HENKA_SCRIPT_API_VALUE_EVENT_ID:
        case HENKA_SCRIPT_API_VALUE_RESULT:
            return true;
        case HENKA_SCRIPT_API_VALUE_STATE_KEY:
            return value->as.state_key != 0U;
        case HENKA_SCRIPT_API_VALUE_FLOAT32:
            return isfinite(value->as.f32) != 0;
        case HENKA_SCRIPT_API_VALUE_VEC3:
            return isfinite(value->as.vec3.x) != 0 &&
                isfinite(value->as.vec3.y) != 0 &&
                isfinite(value->as.vec3.z) != 0;
        default:
            return false;
    }
}

static bool henka_script_host_is_state_api(uint32_t api_id)
{
    return api_id == HENKA_SCRIPT_API_STATE_GET_I32 ||
        api_id == HENKA_SCRIPT_API_STATE_SET_I32 ||
        api_id == HENKA_SCRIPT_API_STATE_GET_BOOL ||
        api_id == HENKA_SCRIPT_API_STATE_SET_BOOL;
}

static henka_result henka_script_host_invoke_state(
    henka_script_host* host,
    uint32_t api_id,
    const henka_script_api_value* arguments,
    henka_script_api_value* out_value)
{
    henka_script_state_value state_value;
    bool present = false;
    henka_result result;
    if (host == NULL || arguments == NULL || out_value == NULL ||
        host->state_store == NULL || !host->state_context_active)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (api_id == HENKA_SCRIPT_API_STATE_GET_I32 ||
        api_id == HENKA_SCRIPT_API_STATE_GET_BOOL)
    {
        result = henka_script_state_store_get(
            host->state_store,
            host->state_identity,
            arguments[0].as.state_key,
            &state_value,
            &present);
        if (result != HENKA_SUCCESS)
        {
            return result;
        }
        if (api_id == HENKA_SCRIPT_API_STATE_GET_I32)
        {
            out_value->type = HENKA_SCRIPT_API_VALUE_I32;
            out_value->as.i32 = present &&
                state_value.type == HENKA_SCRIPT_STATE_VALUE_I32
                ? state_value.as.i32
                : 0;
        }
        else
        {
            out_value->type = HENKA_SCRIPT_API_VALUE_BOOL;
            out_value->as.boolean = present &&
                state_value.type == HENKA_SCRIPT_STATE_VALUE_BOOL &&
                state_value.as.boolean;
        }
        out_value->present = present;
        return HENKA_SUCCESS;
    }
    if (api_id == HENKA_SCRIPT_API_STATE_SET_I32)
    {
        state_value = (henka_script_state_value){
            HENKA_SCRIPT_STATE_VALUE_I32,
            {.i32 = arguments[1].as.i32}};
    }
    else if (api_id == HENKA_SCRIPT_API_STATE_SET_BOOL)
    {
        state_value = (henka_script_state_value){
            HENKA_SCRIPT_STATE_VALUE_BOOL,
            {.boolean = arguments[1].as.boolean}};
    }
    else
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    result = henka_script_state_store_set(
        host->state_store,
        host->state_identity,
        arguments[0].as.state_key,
        state_value);
    out_value->type = HENKA_SCRIPT_API_VALUE_RESULT;
    out_value->as.result = result;
    out_value->present = true;
    return HENKA_SUCCESS;
}

henka_result henka_script_host_set_dispatcher(
    henka_script_host* host,
    henka_script_host_dispatch_callback callback,
    void* user_data)
{
    if (host == NULL || host->dispatching)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    host->dispatcher = callback;
    host->dispatcher_user_data = user_data;
    return HENKA_SUCCESS;
}

henka_result henka_script_host_emit_event(
    henka_script_host* host,
    uint32_t event_id,
    uint64_t source_entity,
    uint64_t frame_index)
{
    const size_t tail = host == NULL
        ? 0U
        : (host->event_head + host->event_count) % HENKA_SCRIPT_HOST_MAX_EVENTS;
    if (host == NULL || event_id == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (host->event_count >= HENKA_SCRIPT_HOST_MAX_EVENTS)
    {
        return HENKA_ERROR_LIMIT;
    }
    host->events[tail] = (henka_script_event){event_id, source_entity, frame_index};
    ++host->event_count;
    return HENKA_SUCCESS;
}

henka_result henka_script_host_poll_event(
    henka_script_host* host,
    henka_script_event* out_event)
{
    if (out_event != NULL)
    {
        *out_event = (henka_script_event){0U, 0U, 0U};
    }
    if (host == NULL || out_event == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    if (host->event_count == 0U)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    *out_event = host->events[host->event_head];
    host->events[host->event_head] = (henka_script_event){0U, 0U, 0U};
    host->event_head = (host->event_head + 1U) % HENKA_SCRIPT_HOST_MAX_EVENTS;
    --host->event_count;
    return HENKA_SUCCESS;
}

size_t henka_script_host_get_pending_event_count(
    const henka_script_host* host)
{
    return host == NULL ? 0U : host->event_count;
}

henka_result henka_script_host_invoke(
    henka_script_host* host,
    uint32_t api_id,
    const henka_script_api_value* arguments,
    size_t argument_count,
    henka_script_api_value* out_value)
{
    const henka_script_api_function* function = NULL;
    size_t index;
    henka_result result;
    if (out_value != NULL)
    {
        memset(out_value, 0, sizeof(*out_value));
        out_value->type = HENKA_SCRIPT_API_VALUE_VOID;
    }
    if (host == NULL || host->dispatching ||
        henka_script_api_schema_find_by_id(api_id, &function) != HENKA_SUCCESS ||
        !henka_script_host_has_binding(host, api_id) ||
        argument_count != function->parameter_count ||
        (argument_count != 0U && arguments == NULL) ||
        (function->return_type != HENKA_SCRIPT_API_VALUE_VOID && out_value == NULL))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0U; index < argument_count; ++index)
    {
        if (arguments[index].type != function->parameters[index] ||
            !henka_script_api_value_is_valid(&arguments[index]))
        {
            return HENKA_ERROR_INVALID_ARGUMENT;
        }
    }
    if (out_value != NULL)
    {
        out_value->type = function->return_type;
    }
    if (henka_script_host_is_state_api(api_id))
    {
        return henka_script_host_invoke_state(
            host, api_id, arguments, out_value);
    }
    if (api_id == HENKA_SCRIPT_API_EVENTS_EMIT && host->dispatcher == NULL)
    {
        result = henka_script_host_emit_event(
            host,
            arguments[0].as.event_id,
            arguments[1].as.entity,
            0U);
        if (out_value != NULL)
        {
            out_value->as.result = result;
        }
        return result;
    }
    if (host->dispatcher == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    host->dispatching = true;
    result = host->dispatcher(
        host->dispatcher_user_data,
        api_id,
        arguments,
        argument_count,
        out_value);
    host->dispatching = false;
    if (result != HENKA_SUCCESS)
    {
        return result;
    }
    if (function->return_type != HENKA_SCRIPT_API_VALUE_VOID &&
        (out_value == NULL || out_value->type != function->return_type))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_SUCCESS;
}
