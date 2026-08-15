#include "studio_environment.h"

#include <math.h>
#include <stdint.h>

#include <henka/math.h>
#include <henka/core.h>

static float sandbox3d_smoothstep(float value)
{
    value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    return value * value * (3.0f - 2.0f * value);
}

void sandbox3d_generate_studio_environment(float* pixels, size_t pixel_count)
{
    size_t y;

    if (pixels == NULL || pixel_count < SANDBOX3D_STUDIO_ENVIRONMENT_PIXEL_COUNT)
    {
        return;
    }
    for (y = 0U; y < SANDBOX3D_STUDIO_ENVIRONMENT_HEIGHT; ++y)
    {
        size_t x;
        float latitude = ((float)y + 0.5f) /
            (float)SANDBOX3D_STUDIO_ENVIRONMENT_HEIGHT;
        float horizon = sandbox3d_smoothstep((latitude - 0.12f) / 0.50f);
        float ground = sandbox3d_smoothstep((latitude - 0.54f) / 0.46f);
        for (x = 0U; x < SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH; ++x)
        {
            const float longitude = 2.0f * (float)HENKA_PI *
                ((float)x + 0.5f) /
                (float)SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH;
            float key_delta = fabsf(longitude - 1.05f);
            float fill_delta = fabsf(longitude - 4.35f);
            float key_lobe;
            float fill_lobe;
            float r = 0.045f + 0.075f * horizon + ground * 0.015f;
            float g = 0.065f + 0.105f * horizon + ground * 0.020f;
            float b = 0.115f + 0.155f * horizon + ground * 0.030f;
            size_t offset = (y * SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH + x) *
                SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS;

            if (key_delta > (float)HENKA_PI) key_delta = 2.0f * (float)HENKA_PI - key_delta;
            if (fill_delta > (float)HENKA_PI) fill_delta = 2.0f * (float)HENKA_PI - fill_delta;
            key_lobe = expf(
                -0.5f * (key_delta / 0.42f) * (key_delta / 0.42f) -
                0.5f * ((latitude - 0.21f) / 0.15f) * ((latitude - 0.21f) / 0.15f));
            fill_lobe = expf(
                -0.5f * (fill_delta / 0.70f) * (fill_delta / 0.70f) -
                0.5f * ((latitude - 0.32f) / 0.24f) * ((latitude - 0.32f) / 0.24f));
            /* The lobes are broad area-light structure, not a baked direct
             * light. Their asymmetric warm/cool energy gives clearcoat and
             * brushed metal a stable highlight to resolve through the same
             * derived IBL path as imported consumer materials. */
            r += key_lobe * 0.72f + fill_lobe * 0.10f;
            g += key_lobe * 0.44f + fill_lobe * 0.13f;
            b += key_lobe * 0.23f + fill_lobe * 0.20f;

            if (ground > 0.0f)
            {
                r = r * (1.0f - ground) + 0.035f * ground;
                g = g * (1.0f - ground) + 0.045f * ground;
                b = b * (1.0f - ground) + 0.065f * ground;
            }
            pixels[offset + 0U] = r;
            pixels[offset + 1U] = g;
            pixels[offset + 2U] = b;
            pixels[offset + 3U] = 1.0f;
        }
    }
}

bool sandbox3d_studio_environment_is_valid(const float* pixels, size_t pixel_count)
{
    size_t y;

    if (pixels == NULL || pixel_count < SANDBOX3D_STUDIO_ENVIRONMENT_PIXEL_COUNT)
    {
        return false;
    }
    for (y = 0U; y < SANDBOX3D_STUDIO_ENVIRONMENT_HEIGHT; ++y)
    {
        size_t x;
        size_t first = y * SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH *
            SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS;
        size_t last = (y * SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH +
            SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH - 1U) *
            SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS;
        for (x = 0U; x < SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH; ++x)
        {
            size_t offset = (y * SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH + x) *
                SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS;
            size_t channel;
            for (channel = 0U; channel < SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS; ++channel)
            {
                if (!isfinite(pixels[offset + channel]) ||
                    pixels[offset + channel] < 0.0f ||
                    (channel == 3U && fabsf(pixels[offset + channel] - 1.0f) > 0.0001f))
                {
                    return false;
                }
            }
        }
        for (x = 0U; x < 3U; ++x)
        {
            if (fabsf(pixels[first + x] - pixels[last + x]) > 0.35f)
            {
                return false;
            }
        }
    }
    return true;
}

henka_vec4 sandbox3d_ground_surface_color(void)
{
    return (henka_vec4){0.035f, 0.050f, 0.075f, 1.0f};
}

bool sandbox3d_ground_surface_uses_texture(void)
{
    return true;
}

void sandbox3d_generate_ground_surface_texture(
    unsigned char* pixels,
    size_t pixel_count)
{
    size_t y;

    if (pixels == NULL || pixel_count < SANDBOX3D_GROUND_TEXTURE_PIXEL_COUNT)
    {
        return;
    }
    for (y = 0U; y < SANDBOX3D_GROUND_TEXTURE_HEIGHT; ++y)
    {
        size_t x;
        for (x = 0U; x < SANDBOX3D_GROUND_TEXTURE_WIDTH; ++x)
        {
            const uint32_t hash =
                ((uint32_t)x * 73856093U) ^
                ((uint32_t)y * 19349663U) ^
                (((uint32_t)x + (uint32_t)y) * 83492791U);
            const unsigned int grain = 34U + ((hash >> 7U) % 28U);
            const unsigned int cool_bias = ((x * 5U + y * 3U) % 9U);
            const size_t offset =
                (y * SANDBOX3D_GROUND_TEXTURE_WIDTH + x) *
                SANDBOX3D_GROUND_TEXTURE_CHANNELS;

            /* Dark graphite with small-scale mineral/wetness variation. The
             * range is intentionally restrained so the floor supports the
             * showcase instead of becoming a checkerboard or noise test. */
            pixels[offset + 0U] = (unsigned char)grain;
            pixels[offset + 1U] = (unsigned char)(grain + 4U + cool_bias / 3U);
            pixels[offset + 2U] = (unsigned char)(grain + 10U + cool_bias);
            pixels[offset + 3U] = 255U;
        }
    }
}

bool sandbox3d_ground_surface_texture_is_valid(
    const unsigned char* pixels,
    size_t pixel_count)
{
    size_t index;
    unsigned int minimum = 255U;
    unsigned int maximum = 0U;

    if (pixels == NULL || pixel_count < SANDBOX3D_GROUND_TEXTURE_PIXEL_COUNT)
    {
        return false;
    }
    for (index = 0U; index < SANDBOX3D_GROUND_TEXTURE_PIXEL_COUNT;
         index += SANDBOX3D_GROUND_TEXTURE_CHANNELS)
    {
        unsigned int channel;
        if (pixels[index + 3U] != 255U)
        {
            return false;
        }
        for (channel = 0U; channel < 3U; ++channel)
        {
            if (pixels[index + channel] < 20U || pixels[index + channel] > 120U)
            {
                return false;
            }
            if ((unsigned int)pixels[index + channel] < minimum)
            {
                minimum = pixels[index + channel];
            }
            if ((unsigned int)pixels[index + channel] > maximum)
            {
                maximum = pixels[index + channel];
            }
        }
    }
    return maximum >= minimum + 12U;
}
