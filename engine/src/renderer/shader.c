#include "henka_internal.h"

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
    henka_renderer_destroy_shader(shader);
}
