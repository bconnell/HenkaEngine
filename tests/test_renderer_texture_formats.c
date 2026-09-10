#include <string.h>

#include <henka/engine.h>
#include <henka/texture.h>

#include "../engine/src/henka_internal.h"

typedef struct renderer_texture_format_test_context
{
    int passed;
} renderer_texture_format_test_context;

static henka_result renderer_texture_format_test_initialize(
    henka_engine* engine,
    void* user_data)
{
    renderer_texture_format_test_context* context = user_data;
    const float pixel[4] = {1.0f, 0.5f, 0.25f, 1.0f};
    henka_texture_descriptor descriptor = henka_texture_descriptor_default_color();
    henka_texture* texture = NULL;
    henka_texture_info info;
    henka_result result;

    memset(&info, 0, sizeof(info));
    result = engine == NULL || context == NULL ?
        HENKA_ERROR_INVALID_ARGUMENT :
        henka_renderer_create_texture_from_rgba32f_with_descriptor(
            engine->renderer,
            1,
            1,
            pixel,
            &descriptor,
            &texture);
    if (result == HENKA_SUCCESS)
    {
        result = henka_texture_get_info(texture, &info);
        context->passed = result == HENKA_SUCCESS &&
            info.source_class == HENKA_TEXTURE_SOURCE_CLASS_HDR &&
            info.gpu_format == HENKA_TEXTURE_GPU_FORMAT_RGBA16F &&
            info.resident_gpu_bytes == sizeof(pixel) / 2U;
    }
    if (texture != NULL)
        henka_texture_destroy(texture);
    henka_engine_request_exit(engine);
    return HENKA_SUCCESS;
}

static int test_hdr_upload_reports_rgba16f_gpu_format(void)
{
    renderer_texture_format_test_context context = {0};
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_result result;

    config.application_name = "Renderer Texture Format Test";
    config.window_width = 64;
    config.window_height = 64;
    config.enable_vsync = false;
    config.asset_base_path = ".";
    config.on_initialize = renderer_texture_format_test_initialize;
    config.user_data = &context;
    result = henka_engine_create(&config, &engine);
    if (result == HENKA_SUCCESS)
    {
        result = henka_engine_run(engine);
        henka_engine_destroy(engine);
    }
    return result == HENKA_SUCCESS && context.passed;
}

int main(void)
{
    return test_hdr_upload_reports_rgba16f_gpu_format() ? 0 : 1;
}
