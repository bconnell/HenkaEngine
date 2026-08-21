#ifndef HENKA_SCRIPT_H
#define HENKA_SCRIPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/result.h>

#define HENKA_SCRIPT_API_SCHEMA_VERSION UINT32_C(1)
#define HENKA_SCRIPT_API_MAX_PARAMETERS 4U
#define HENKA_SCRIPT_HOST_MAX_BINDINGS 128U

typedef struct henka_script_host henka_script_host;

typedef enum henka_script_language
{
    HENKA_SCRIPT_LANGUAGE_NONE = 0,
    HENKA_SCRIPT_LANGUAGE_LUA,
    HENKA_SCRIPT_LANGUAGE_HENKASCRIPT
} henka_script_language;

typedef enum henka_script_api_domain
{
    HENKA_SCRIPT_API_DOMAIN_ENTITY = 0,
    HENKA_SCRIPT_API_DOMAIN_TRANSFORM,
    HENKA_SCRIPT_API_DOMAIN_INPUT,
    HENKA_SCRIPT_API_DOMAIN_PHYSICS,
    HENKA_SCRIPT_API_DOMAIN_INTERACTION,
    HENKA_SCRIPT_API_DOMAIN_EVENTS
} henka_script_api_domain;

typedef enum henka_script_api_value_type
{
    HENKA_SCRIPT_API_VALUE_VOID = 0,
    HENKA_SCRIPT_API_VALUE_BOOL,
    HENKA_SCRIPT_API_VALUE_FLOAT32,
    HENKA_SCRIPT_API_VALUE_VEC3,
    HENKA_SCRIPT_API_VALUE_ENTITY,
    HENKA_SCRIPT_API_VALUE_ACTION_ID,
    HENKA_SCRIPT_API_VALUE_EVENT_ID,
    HENKA_SCRIPT_API_VALUE_RESULT
} henka_script_api_value_type;

typedef enum henka_script_api_id
{
    HENKA_SCRIPT_API_ENTITY_IS_VALID = UINT32_C(0x0101),
    HENKA_SCRIPT_API_TRANSFORM_GET_POSITION = UINT32_C(0x0201),
    HENKA_SCRIPT_API_TRANSFORM_SET_POSITION = UINT32_C(0x0202),
    HENKA_SCRIPT_API_INPUT_IS_ACTION_DOWN = UINT32_C(0x0301),
    HENKA_SCRIPT_API_PHYSICS_APPLY_IMPULSE = UINT32_C(0x0401),
    HENKA_SCRIPT_API_INTERACTION_TRY = UINT32_C(0x0501),
    HENKA_SCRIPT_API_EVENTS_EMIT = UINT32_C(0x0601)
} henka_script_api_id;

/* This is a bind-time schema, not a dynamic call payload. Backends resolve
 * these stable IDs once and can retain their own typed/native thunks for the
 * hot path. The names are for diagnostics, tooling, and serialization only. */
typedef struct henka_script_api_function
{
    uint32_t id;
    const char* name;
    henka_script_api_domain domain;
    henka_script_api_value_type return_type;
    uint8_t parameter_count;
    henka_script_api_value_type parameters[HENKA_SCRIPT_API_MAX_PARAMETERS];
} henka_script_api_function;

/* Returns the immutable engine-owned schema. The returned array remains valid
 * for the process lifetime and must not be freed or modified by the caller. */
henka_result henka_script_api_schema_get(
    const henka_script_api_function** out_functions,
    size_t* out_count);

henka_result henka_script_api_schema_find_by_id(
    uint32_t id,
    const henka_script_api_function** out_function);

henka_result henka_script_api_schema_find_by_name(
    const char* name,
    const henka_script_api_function** out_function);

henka_result henka_script_host_create(henka_script_host** out_host);
void henka_script_host_destroy(henka_script_host* host);

/* A host owns only bounded bind-time records. It does not own script source,
 * backend state, runtime entities, or filesystem paths. Those lifetimes are
 * supplied by the later behavior/runtime layers. */
henka_result henka_script_host_bind_api(
    henka_script_host* host,
    uint32_t api_id,
    size_t* out_binding_index);

henka_result henka_script_host_get_binding(
    const henka_script_host* host,
    size_t binding_index,
    const henka_script_api_function** out_function);

size_t henka_script_host_get_binding_count(const henka_script_host* host);

#endif
