#include <stdio.h>

#include <henka/engine.h>

int main(void)
{
    henka_engine_config config = {0};
    henka_engine* engine = NULL;
    henka_result result;

    config.application_name = "Henka OpenGL Capability Probe";
    config.window_width = 64;
    config.window_height = 64;
    config.enable_vsync = false;
    config.asset_base_path = ".";

    result = henka_engine_create(&config, &engine);
    if (result != HENKA_SUCCESS)
    {
        fprintf(
            stderr,
            "HENKA_OPENGL_PROBE_RESULT status=FAILED result=%s\n",
            henka_result_to_string(result));
        return 1;
    }

    henka_engine_destroy(engine);
    puts("HENKA_OPENGL_PROBE_RESULT status=PASS");
    return 0;
}
