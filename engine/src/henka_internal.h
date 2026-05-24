#ifndef HENKA_INTERNAL_H
#define HENKA_INTERNAL_H

#include <stdbool.h>

#include <henka/engine.h>
#include <henka/input.h>
#include <henka/platform.h>
#include <henka/renderer.h>
#include <henka/result.h>

typedef struct henka_input_state
{
    bool keys_down[HENKA_KEY_COUNT];
    bool keys_pressed[HENKA_KEY_COUNT];
    bool close_requested;
} henka_input_state;

typedef struct henka_platform_desc
{
    const char* application_name;
    int window_width;
    int window_height;
    bool enable_vsync;
} henka_platform_desc;

typedef struct henka_platform_frame_state
{
    bool close_requested;
    bool resized;
    int framebuffer_width;
    int framebuffer_height;
} henka_platform_frame_state;

struct henka_platform;
struct henka_renderer;

struct henka_engine
{
    henka_engine_config config;
    struct henka_platform* platform;
    struct henka_renderer* renderer;
    henka_input_state input;
    bool exit_requested;
};

henka_result henka_platform_create(const henka_platform_desc* desc, struct henka_platform** out_platform);
void henka_platform_destroy(struct henka_platform* platform);
henka_result henka_platform_poll_events(struct henka_platform* platform, henka_input_state* input, henka_platform_frame_state* out_state);
henka_result henka_platform_set_vsync(struct henka_platform* platform, bool enabled);

henka_result henka_renderer_create(struct henka_platform* platform, bool enable_vsync, struct henka_renderer** out_renderer);
void henka_renderer_destroy(struct henka_renderer* renderer);
henka_result henka_renderer_begin_frame(struct henka_renderer* renderer);
void henka_renderer_clear_frame(struct henka_renderer* renderer);
henka_result henka_renderer_end_frame(struct henka_renderer* renderer);
void henka_renderer_resize_viewport(struct henka_renderer* renderer, int width, int height);
henka_result henka_renderer_set_vsync(struct henka_renderer* renderer, bool enabled);

#endif
