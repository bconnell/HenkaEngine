#include "opengl_capability_policy.h"

bool henka_opengl_capability_limits_are_sufficient(
    const henka_opengl_capability_limits* limits)
{
    if (limits == NULL)
    {
        return false;
    }

    return limits->max_fragment_texture_image_units >=
               HENKA_OPENGL_REQUIRED_FRAGMENT_TEXTURE_IMAGE_UNITS &&
           limits->max_combined_texture_image_units >=
               HENKA_OPENGL_REQUIRED_COMBINED_TEXTURE_IMAGE_UNITS &&
           limits->max_draw_buffers >= HENKA_OPENGL_REQUIRED_DRAW_BUFFERS &&
           limits->max_color_attachments >=
               HENKA_OPENGL_REQUIRED_COLOR_ATTACHMENTS &&
           limits->max_texture_size >= HENKA_OPENGL_REQUIRED_TEXTURE_SIZE &&
           limits->max_cube_map_texture_size >=
               HENKA_OPENGL_REQUIRED_CUBE_MAP_TEXTURE_SIZE;
}
