#include "henka_internal.h"

#include <henka/log.h>

henka_result henka_shader_create_from_files(henka_engine* engine, const char* vertex_path, const char* fragment_path, henka_shader** out_shader)
{
    if (engine == NULL || out_shader == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    return henka_renderer_create_shader_from_files(engine->renderer, vertex_path, fragment_path, out_shader);
}

void henka_shader_destroy(henka_shader* shader)
{
    if (shader == NULL)
    {
        return;
    }

    if (shader->asset_manager_owned)
    {
        HENKA_LOG_WARN(
            "ignored an attempt to destroy a manager-owned borrowed shader");
        return;
    }

    henka_renderer_destroy_shader(shader);
}

void henka_shader_destroy_owned(henka_shader* shader)
{
    if (shader == NULL)
    {
        return;
    }

    shader->asset_manager_owned = false;
    henka_renderer_destroy_shader(shader);
}
