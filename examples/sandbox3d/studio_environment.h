#ifndef SANDBOX3D_STUDIO_ENVIRONMENT_H
#define SANDBOX3D_STUDIO_ENVIRONMENT_H

#include <stdbool.h>
#include <stddef.h>

#define SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH 32U
#define SANDBOX3D_STUDIO_ENVIRONMENT_HEIGHT 16U
#define SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS 4U
#define SANDBOX3D_STUDIO_ENVIRONMENT_PIXEL_COUNT \
    (SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH * \
        SANDBOX3D_STUDIO_ENVIRONMENT_HEIGHT * \
        SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS)

void sandbox3d_generate_studio_environment(float* pixels, size_t pixel_count);
bool sandbox3d_studio_environment_is_valid(const float* pixels, size_t pixel_count);

#endif
