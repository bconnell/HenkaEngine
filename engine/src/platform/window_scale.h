#ifndef HENKA_PLATFORM_WINDOW_SCALE_H
#define HENKA_PLATFORM_WINDOW_SCALE_H

#include <stdbool.h>
#include <stdint.h>

bool henka_platform_window_scale_from_display_scale(
    float display_scale,
    uint32_t* out_numerator,
    uint32_t* out_denominator);

/* SDL event coordinates are in platform-native window units. Convert that
 * coordinate space to editor content units without confusing pixel density
 * with the combined content-to-framebuffer display scale. */
bool henka_platform_window_scale_from_native_geometry(
    float display_scale,
    float pixel_density,
    uint32_t* out_native_to_content_numerator,
    uint32_t* out_native_to_content_denominator);

bool henka_platform_window_scale_content_size_from_framebuffer(
    int framebuffer_width,
    int framebuffer_height,
    uint32_t display_scale_numerator,
    uint32_t display_scale_denominator,
    int* out_content_width,
    int* out_content_height);

bool henka_platform_window_scale_content_size_to_native(
    int content_width,
    int content_height,
    uint32_t native_to_content_numerator,
    uint32_t native_to_content_denominator,
    int* out_native_width,
    int* out_native_height);

bool henka_platform_window_scale_point_to_framebuffer(
    float logical_x,
    float logical_y,
    uint32_t scale_numerator,
    uint32_t scale_denominator,
    float* out_framebuffer_x,
    float* out_framebuffer_y);

#endif
