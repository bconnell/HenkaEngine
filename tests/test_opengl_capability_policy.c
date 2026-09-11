#include <stdio.h>

#include "../engine/src/renderer/opengl_capability_policy.h"

static henka_opengl_capability_limits supported_limits(void)
{
    return (henka_opengl_capability_limits){
        HENKA_OPENGL_REQUIRED_FRAGMENT_TEXTURE_IMAGE_UNITS,
        HENKA_OPENGL_REQUIRED_COMBINED_TEXTURE_IMAGE_UNITS,
        HENKA_OPENGL_REQUIRED_DRAW_BUFFERS,
        HENKA_OPENGL_REQUIRED_COLOR_ATTACHMENTS,
        HENKA_OPENGL_REQUIRED_TEXTURE_SIZE,
        HENKA_OPENGL_REQUIRED_CUBE_MAP_TEXTURE_SIZE};
}

static int expect_rejected(
    const henka_opengl_capability_limits* limits,
    const char* label)
{
    if (henka_opengl_capability_limits_are_sufficient(limits))
    {
        fprintf(stderr, "under-capable OpenGL limits were accepted: %s\n", label);
        return 1;
    }
    return 0;
}

int main(void)
{
    henka_opengl_capability_limits limits = supported_limits();

    if (!henka_opengl_capability_limits_are_sufficient(&limits))
    {
        fprintf(stderr, "declared supported OpenGL limits were rejected\n");
        return 1;
    }
    if (expect_rejected(NULL, "null limits") != 0)
        return 1;

    limits.max_fragment_texture_image_units -= 1;
    if (expect_rejected(&limits, "fragment texture image units") != 0)
        return 1;
    limits = supported_limits();
    limits.max_combined_texture_image_units -= 1;
    if (expect_rejected(&limits, "combined texture image units") != 0)
        return 1;
    limits = supported_limits();
    limits.max_draw_buffers -= 1;
    if (expect_rejected(&limits, "draw buffers") != 0)
        return 1;
    limits = supported_limits();
    limits.max_color_attachments -= 1;
    if (expect_rejected(&limits, "color attachments") != 0)
        return 1;
    limits = supported_limits();
    limits.max_texture_size -= 1;
    if (expect_rejected(&limits, "2D texture size") != 0)
        return 1;
    limits = supported_limits();
    limits.max_cube_map_texture_size -= 1;
    if (expect_rejected(&limits, "cube-map texture size") != 0)
        return 1;

    puts("OpenGL capability limit policy passed");
    return 0;
}
