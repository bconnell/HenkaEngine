#ifndef HENKA_SCRIPT_BACKENDS_H
#define HENKA_SCRIPT_BACKENDS_H

#include <stddef.h>

#include <henka/henkascript.h>
#include <henka/script_runtime.h>

typedef struct henka_hks_behavior_backend henka_hks_behavior_backend;

/* Compiles a bounded HenkaScript behavior asset. Lifecycle functions are
 * discovered by the exact names OnCreate, OnStart, OnUpdate, and OnStop.
 * Missing lifecycle functions are deterministic no-ops. */
henka_result henka_hks_behavior_backend_create(
    const char* source,
    size_t source_size,
    henka_hks_behavior_backend** out_backend,
    henka_hks_diagnostic* out_diagnostic);
void henka_hks_behavior_backend_destroy(henka_hks_behavior_backend* backend);

henka_script_behavior_callback_result henka_hks_behavior_backend_callback(
    const henka_script_behavior_context* context,
    void* user_data,
    uint32_t* out_instructions_used);

#endif
