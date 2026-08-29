#ifndef SANDBOX3D_REALISM_DETAIL_H
#define SANDBOX3D_REALISM_DETAIL_H

#include <stdbool.h>
#include <stddef.h>

#define SANDBOX3D_REALISM_TEXTURE_EDGE 128U
#define SANDBOX3D_REALISM_TEXTURE_CHANNEL_COUNT 4U
#define SANDBOX3D_REALISM_TEXTURE_PIXEL_COUNT \
    ((size_t)SANDBOX3D_REALISM_TEXTURE_EDGE * \
        (size_t)SANDBOX3D_REALISM_TEXTURE_EDGE * \
        (size_t)SANDBOX3D_REALISM_TEXTURE_CHANNEL_COUNT)

/* Generates the deterministic RGBA detail maps used by the realism reference
 * scene. Every destination is caller-owned and capacity is validated before
 * any output is written. */
bool sandbox3d_generate_realism_detail_textures(
    unsigned char* normal_pixels,
    size_t normal_capacity,
    unsigned char* macro_variation_pixels,
    size_t macro_variation_capacity,
    unsigned char* wood_grain_pixels,
    size_t wood_grain_capacity,
    unsigned char* wet_dry_roughness_pixels,
    size_t wet_dry_roughness_capacity);

#endif
