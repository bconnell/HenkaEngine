#ifndef HENKA_ENGINE_H
#define HENKA_ENGINE_H

#include <stdbool.h>

#include <henka/result.h>

typedef struct henka_engine henka_engine;

typedef struct henka_engine_config
{
    const char* application_name;
    int window_width;
    int window_height;
    bool enable_vsync;
} henka_engine_config;

/*
 * Creates and owns a new engine instance. The caller becomes responsible for
 * releasing it with henka_engine_destroy.
 */
henka_result henka_engine_create(const henka_engine_config* config, henka_engine** out_engine);
void henka_engine_destroy(henka_engine* engine);
henka_result henka_engine_run(henka_engine* engine);
void henka_engine_request_exit(henka_engine* engine);

#endif
