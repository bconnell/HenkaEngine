#include <stdio.h>
#include <stdlib.h>

#include <henka/henka.h>
#include <henka/scene.h>

#include "../examples/sandbox3d/audio_runtime.h"

int main(void)
{
    henka_scene* scene = NULL;
    sandbox3d_audio_runtime* runtime = NULL;
    henka_audio_output_info output_info;
    henka_camera camera;
    int exit_code = 1;

#if defined(_WIN32)
    (void)_putenv_s("SDL_AUDIODRIVER", "dummy");
#endif

    camera = henka_camera_create_perspective(
        60.0f * HENKA_DEG_TO_RAD,
        16.0f / 9.0f,
        0.1f,
        100.0f);
    camera.position = (henka_vec3){2.0f, 3.0f, 4.0f};
    camera.yaw_radians = 0.35f;
    camera.pitch_radians = -0.2f;
    if (henka_scene_create(&scene) != HENKA_SUCCESS ||
        sandbox3d_audio_runtime_create(scene, &runtime) != HENKA_SUCCESS ||
        !sandbox3d_audio_runtime_is_output_available(runtime) ||
        sandbox3d_audio_runtime_get_system_if_available(runtime) == NULL ||
        sandbox3d_audio_runtime_update_listener(runtime, &camera) != HENKA_SUCCESS ||
        sandbox3d_audio_runtime_pump(runtime, 1.0 / 60.0) != HENKA_SUCCESS ||
        sandbox3d_audio_runtime_get_output_info(runtime, &output_info) != HENKA_SUCCESS ||
        !output_info.device_open || output_info.sample_rate == 0U ||
        output_info.pumped_frames == 0U)
    {
        fprintf(stderr, "sandbox audio runtime test failed\n");
        goto cleanup;
    }
    exit_code = 0;

cleanup:
    sandbox3d_audio_runtime_destroy(runtime);
    henka_scene_destroy(scene);
    return exit_code;
}
