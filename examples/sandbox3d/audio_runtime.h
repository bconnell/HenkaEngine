#ifndef SANDBOX3D_AUDIO_RUNTIME_H
#define SANDBOX3D_AUDIO_RUNTIME_H

#include <stdbool.h>

#include <henka/audio.h>
#include <henka/audio_output.h>
#include <henka/camera.h>
#include <henka/scene.h>

typedef struct sandbox3d_audio_runtime sandbox3d_audio_runtime;

/* Owns the client-side Audio system and caller-pumped output boundary. The
 * scene is borrowed and must outlive the runtime; Play emitters borrow the
 * same Audio system through the Game Authoring coordinator. */
henka_result sandbox3d_audio_runtime_create(
    henka_scene* scene,
    sandbox3d_audio_runtime** out_runtime);
void sandbox3d_audio_runtime_destroy(
    sandbox3d_audio_runtime* runtime);

bool sandbox3d_audio_runtime_is_output_available(
    const sandbox3d_audio_runtime* runtime);
henka_audio_system* sandbox3d_audio_runtime_get_system_if_available(
    const sandbox3d_audio_runtime* runtime);
henka_result sandbox3d_audio_runtime_update_listener(
    sandbox3d_audio_runtime* runtime,
    const henka_camera* camera);
henka_result sandbox3d_audio_runtime_pump(
    sandbox3d_audio_runtime* runtime,
    double delta_seconds);
henka_result sandbox3d_audio_runtime_get_output_info(
    const sandbox3d_audio_runtime* runtime,
    henka_audio_output_info* out_info);

#endif
