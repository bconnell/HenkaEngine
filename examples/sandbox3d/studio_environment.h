#ifndef SANDBOX3D_STUDIO_ENVIRONMENT_H
#define SANDBOX3D_STUDIO_ENVIRONMENT_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/math.h>

#define SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH 32U
#define SANDBOX3D_STUDIO_ENVIRONMENT_HEIGHT 16U
#define SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS 4U
#define SANDBOX3D_STUDIO_ENVIRONMENT_PIXEL_COUNT \
    (SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH * \
        SANDBOX3D_STUDIO_ENVIRONMENT_HEIGHT * \
        SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS)

void sandbox3d_generate_studio_environment(float* pixels, size_t pixel_count);
bool sandbox3d_studio_environment_is_valid(const float* pixels, size_t pixel_count);

/* The sandbox floor is a restrained editor surface, not a checkerboard test
 * texture. Keeping this policy in the fixture module gives the regression
 * test and the runtime one authoritative color contract. */
henka_vec4 sandbox3d_ground_surface_color(void);
bool sandbox3d_ground_surface_uses_texture(void);

#endif
