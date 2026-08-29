#include "realism_detail.h"

#include <math.h>
#include <stdint.h>

static unsigned char sandbox3d_encode_realism_texture_channel(float value)
{
    return (unsigned char)lroundf(fmaxf(0.0f, fminf(255.0f, value)));
}

static uint32_t sandbox3d_realism_texture_hash(
    uint32_t x,
    uint32_t y,
    uint32_t seed)
{
    uint32_t value = x * 374761393U ^ y * 668265263U ^ seed * 2246822519U;

    value ^= value >> 13U;
    value *= 1274126177U;
    value ^= value >> 16U;
    return value;
}

static float sandbox3d_realism_value_noise(
    float u,
    float v,
    uint32_t seed,
    uint32_t x_period,
    uint32_t y_period)
{
    float x;
    float y;
    float x_floor;
    float y_floor;
    float x_fraction;
    float y_fraction;
    float x_smooth;
    float y_smooth;
    uint32_t x0;
    uint32_t x1;
    uint32_t y0;
    uint32_t y1;
    float lower;
    float upper;

    if (x_period == 0U || y_period == 0U)
    {
        return 0.5f;
    }
    x = fmaxf(0.0f, fminf(1.0f, u)) * (float)x_period;
    y = fmaxf(0.0f, fminf(1.0f, v)) * (float)y_period;
    x_floor = floorf(x);
    y_floor = floorf(y);
    x_fraction = x - x_floor;
    y_fraction = y - y_floor;
    x_smooth = x_fraction * x_fraction * (3.0f - 2.0f * x_fraction);
    y_smooth = y_fraction * y_fraction * (3.0f - 2.0f * y_fraction);
    x0 = (uint32_t)x_floor % x_period;
    y0 = (uint32_t)y_floor % y_period;
    x1 = (x0 + 1U) % x_period;
    y1 = (y0 + 1U) % y_period;
    lower = (float)(sandbox3d_realism_texture_hash(x0, y0, seed) & 0xffffffU) /
        16777215.0f * (1.0f - x_smooth) +
        (float)(sandbox3d_realism_texture_hash(x1, y0, seed) & 0xffffffU) /
        16777215.0f * x_smooth;
    upper = (float)(sandbox3d_realism_texture_hash(x0, y1, seed) & 0xffffffU) /
        16777215.0f * (1.0f - x_smooth) +
        (float)(sandbox3d_realism_texture_hash(x1, y1, seed) & 0xffffffU) /
        16777215.0f * x_smooth;
    return lower * (1.0f - y_smooth) + upper * y_smooth;
}

static float sandbox3d_realism_pole_safe_u(float u, float v)
{
    const float pole_distance = fminf(v, 1.0f - v);
    const float normalized_distance = fmaxf(
        0.0f,
        fminf(1.0f, pole_distance / 0.18f));
    const float smooth_distance = normalized_distance * normalized_distance *
        (3.0f - 2.0f * normalized_distance);
    const float pole_fade = 1.0f - smooth_distance;

    /* A UV sphere stores one spatial pole once per longitude. Keep generated
     * detail continuous across that collapsed parameter region while leaving
     * the body of the map unchanged. */
    return u * (1.0f - pole_fade) + 0.5f * pole_fade;
}

bool sandbox3d_generate_realism_detail_textures(
    unsigned char* normal_pixels,
    size_t normal_capacity,
    unsigned char* macro_variation_pixels,
    size_t macro_variation_capacity,
    unsigned char* wood_grain_pixels,
    size_t wood_grain_capacity,
    unsigned char* wet_dry_roughness_pixels,
    size_t wet_dry_roughness_capacity)
{
    uint32_t x;
    uint32_t y;

    if (normal_pixels == NULL || macro_variation_pixels == NULL ||
        wood_grain_pixels == NULL || wet_dry_roughness_pixels == NULL ||
        normal_capacity < SANDBOX3D_REALISM_TEXTURE_PIXEL_COUNT ||
        macro_variation_capacity < SANDBOX3D_REALISM_TEXTURE_PIXEL_COUNT ||
        wood_grain_capacity < SANDBOX3D_REALISM_TEXTURE_PIXEL_COUNT ||
        wet_dry_roughness_capacity < SANDBOX3D_REALISM_TEXTURE_PIXEL_COUNT)
    {
        return false;
    }
    for (y = 0U; y < SANDBOX3D_REALISM_TEXTURE_EDGE; ++y)
    {
        for (x = 0U; x < SANDBOX3D_REALISM_TEXTURE_EDGE; ++x)
        {
            const float u = ((float)x + 0.5f) /
                (float)SANDBOX3D_REALISM_TEXTURE_EDGE;
            const float v = ((float)y + 0.5f) /
                (float)SANDBOX3D_REALISM_TEXTURE_EDGE;
            const float detail_u = sandbox3d_realism_pole_safe_u(u, v);
            const float normal_x =
                (sandbox3d_realism_value_noise(detail_u, v, 11U, 17U, 13U) - 0.5f) * 0.30f +
                (sandbox3d_realism_value_noise(detail_u, v, 29U, 31U, 23U) - 0.5f) * 0.14f;
            const float normal_y =
                (sandbox3d_realism_value_noise(detail_u, v, 47U, 19U, 29U) - 0.5f) * 0.28f +
                (sandbox3d_realism_value_noise(detail_u, v, 71U, 37U, 17U) - 0.5f) * 0.12f;
            /* Distribute the energy over several mid-scale bands. The former
             * dominant 5x5, 9x5, and 7x11 bands produced broad stepped patches
             * when the reference maps were viewed on curved meshes. */
            const float macro_signal =
                sandbox3d_realism_value_noise(detail_u, v, 101U, 8U, 9U) * 0.28f +
                sandbox3d_realism_value_noise(detail_u, v, 103U, 19U, 17U) * 0.36f +
                sandbox3d_realism_value_noise(detail_u, v, 107U, 37U, 31U) * 0.36f;
            const float grain_signal =
                sandbox3d_realism_value_noise(detail_u, v, 131U, 13U, 9U) * 0.30f +
                sandbox3d_realism_value_noise(detail_u, v, 137U, 29U, 19U) * 0.36f +
                sandbox3d_realism_value_noise(detail_u, v, 139U, 53U, 37U) * 0.34f;
            const float wet_signal =
                sandbox3d_realism_value_noise(detail_u, v, 157U, 11U, 13U) * 0.30f +
                sandbox3d_realism_value_noise(detail_u, v, 163U, 23U, 29U) * 0.35f +
                sandbox3d_realism_value_noise(detail_u, v, 167U, 47U, 59U) * 0.35f;
            const size_t pixel = ((size_t)y * SANDBOX3D_REALISM_TEXTURE_EDGE + x) *
                SANDBOX3D_REALISM_TEXTURE_CHANNEL_COUNT;
            const float normal_z = sqrtf(fmaxf(
                0.0f, 1.0f - normal_x * normal_x - normal_y * normal_y));

            normal_pixels[pixel + 0U] = sandbox3d_encode_realism_texture_channel(
                128.0f + normal_x * 127.0f);
            normal_pixels[pixel + 1U] = sandbox3d_encode_realism_texture_channel(
                128.0f + normal_y * 127.0f);
            normal_pixels[pixel + 2U] = sandbox3d_encode_realism_texture_channel(
                normal_z * 127.0f + 128.0f);
            normal_pixels[pixel + 3U] = 255U;

            macro_variation_pixels[pixel + 0U] = sandbox3d_encode_realism_texture_channel(
                96.0f + macro_signal * 82.0f);
            macro_variation_pixels[pixel + 1U] = sandbox3d_encode_realism_texture_channel(
                58.0f + macro_signal * 54.0f);
            macro_variation_pixels[pixel + 2U] = sandbox3d_encode_realism_texture_channel(
                32.0f + macro_signal * 32.0f);
            macro_variation_pixels[pixel + 3U] = 255U;

            wood_grain_pixels[pixel + 0U] = sandbox3d_encode_realism_texture_channel(
                96.0f + grain_signal * 48.0f);
            wood_grain_pixels[pixel + 1U] = sandbox3d_encode_realism_texture_channel(
                46.0f + grain_signal * 28.0f);
            wood_grain_pixels[pixel + 2U] = sandbox3d_encode_realism_texture_channel(
                20.0f + grain_signal * 16.0f);
            wood_grain_pixels[pixel + 3U] = 255U;

            wet_dry_roughness_pixels[pixel + 0U] = 255U;
            wet_dry_roughness_pixels[pixel + 1U] = sandbox3d_encode_realism_texture_channel(
                34.0f + wet_signal * 206.0f);
            wet_dry_roughness_pixels[pixel + 2U] = 255U;
            wet_dry_roughness_pixels[pixel + 3U] = 255U;
        }
    }
    return true;
}
