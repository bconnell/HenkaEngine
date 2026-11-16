#ifndef SANDBOX3D_EDITOR_AUTOMATION_FRAME_CAPTURE_H
#define SANDBOX3D_EDITOR_AUTOMATION_FRAME_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <henka/engine.h>
#include <henka/result.h>

typedef struct sandbox3d_automation_frame_capture
    sandbox3d_automation_frame_capture;

bool sandbox3d_automation_frame_capture_label_is_valid(const char* label);
bool sandbox3d_automation_frame_capture_format_path(
    char* out_path,
    size_t out_path_capacity,
    const char* output_directory,
    uint32_t sequence,
    const char* label);

henka_result sandbox3d_automation_frame_capture_create(
    sandbox3d_automation_frame_capture** out_capture);
void sandbox3d_automation_frame_capture_destroy(
    sandbox3d_automation_frame_capture* capture);
void sandbox3d_automation_frame_capture_update(
    sandbox3d_automation_frame_capture* capture,
    henka_engine* engine);

#endif
