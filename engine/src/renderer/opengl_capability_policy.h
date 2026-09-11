#ifndef HENKA_OPENGL_CAPABILITY_POLICY_H
#define HENKA_OPENGL_CAPABILITY_POLICY_H

#include <stdbool.h>
#include <stddef.h>

#define HENKA_OPENGL_REQUIRED_FRAGMENT_TEXTURE_IMAGE_UNITS 29
#define HENKA_OPENGL_REQUIRED_COMBINED_TEXTURE_IMAGE_UNITS 27
#define HENKA_OPENGL_REQUIRED_DRAW_BUFFERS 4
#define HENKA_OPENGL_REQUIRED_COLOR_ATTACHMENTS 4
#define HENKA_OPENGL_REQUIRED_TEXTURE_SIZE 256
#define HENKA_OPENGL_REQUIRED_CUBE_MAP_TEXTURE_SIZE 256

typedef struct henka_opengl_capability_limits
{
    int max_fragment_texture_image_units;
    int max_combined_texture_image_units;
    int max_draw_buffers;
    int max_color_attachments;
    int max_texture_size;
    int max_cube_map_texture_size;
} henka_opengl_capability_limits;

bool henka_opengl_capability_limits_are_sufficient(
    const henka_opengl_capability_limits* limits);

#endif
