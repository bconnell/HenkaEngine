#ifndef SANDBOX3D_STUDIO_ENVIRONMENT_H
#define SANDBOX3D_STUDIO_ENVIRONMENT_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/math.h>

/* Keep the deterministic studio source dense enough that smooth reflections
 * do not inherit visible latitude/longitude bands from the fixture itself. */
#define SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH 128U
#define SANDBOX3D_STUDIO_ENVIRONMENT_HEIGHT 64U
#define SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS 4U
#define SANDBOX3D_STUDIO_ENVIRONMENT_PIXEL_COUNT \
    (SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH * \
        SANDBOX3D_STUDIO_ENVIRONMENT_HEIGHT * \
        SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS)

#define SANDBOX3D_GROUND_TEXTURE_WIDTH 16U
#define SANDBOX3D_GROUND_TEXTURE_HEIGHT 16U
#define SANDBOX3D_GROUND_TEXTURE_CHANNELS 4U
#define SANDBOX3D_GROUND_TEXTURE_PIXEL_COUNT \
    (SANDBOX3D_GROUND_TEXTURE_WIDTH * \
        SANDBOX3D_GROUND_TEXTURE_HEIGHT * \
        SANDBOX3D_GROUND_TEXTURE_CHANNELS)

void sandbox3d_generate_studio_environment(float* pixels, size_t pixel_count);
bool sandbox3d_studio_environment_is_valid(const float* pixels, size_t pixel_count);

/* The sandbox floor is a restrained graphite surface with bounded procedural
 * variation. Keeping this policy in the fixture module gives the regression
 * test and the runtime one authoritative material contract. */
henka_vec4 sandbox3d_ground_surface_color(void);
henka_vec4 sandbox3d_debug_grid_color(void);
bool sandbox3d_ground_surface_uses_texture(void);
void sandbox3d_generate_ground_surface_texture(
    unsigned char* pixels,
    size_t pixel_count);
bool sandbox3d_ground_surface_texture_is_valid(
    const unsigned char* pixels,
    size_t pixel_count);

#endif
