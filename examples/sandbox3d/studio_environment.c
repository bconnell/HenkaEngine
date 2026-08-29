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
        /* Keep the lower hemisphere continuous and luminous enough to read as
         * a studio enclosure. The visible sandbox floor is a separate scene
         * surface; the IBL source must not stamp a localized dark basin into
         * every reflected sphere. */
        float lower_gradient = sandbox3d_smoothstep((latitude - 0.38f) / 0.62f);
        for (x = 0U; x < SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH; ++x)
        {
            const float longitude = 2.0f * (float)HENKA_PI *
                ((float)x + 0.5f) /
                (float)SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH;
            /* The camera-facing +Z direction is the primary reflected view
             * in the close benchmark. Keep the broad warm key on that side
             * so the IBL roughness ladder resolves authored reflection energy
             * instead of measuring a uniformly dark back hemisphere. */
            float key_delta = fabsf(longitude - 4.35f);
            float fill_delta = fabsf(longitude - 1.05f);
            float key_field;
            float fill_field;
            float r = 0.070f + 0.065f * horizon + 0.045f * lower_gradient;
            float g = 0.090f + 0.090f * horizon + 0.060f * lower_gradient;
            float b = 0.140f + 0.120f * horizon + 0.080f * lower_gradient;
            size_t offset = (y * SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH + x) *
                SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS;

            if (key_delta > (float)HENKA_PI) key_delta = 2.0f * (float)HENKA_PI - key_delta;
            if (fill_delta > (float)HENKA_PI) fill_delta = 2.0f * (float)HENKA_PI - fill_delta;
            /* Cosine fields model broad studio softboxes. They retain
             * directional color and roughness contrast without a compact
             * Gaussian maximum that can project as a pinched sphere lobe. */
            key_field = 0.60f + 0.40f * cosf(key_delta);
            fill_field = 0.78f + 0.22f * cosf(fill_delta);
            r += key_field * 1.75f + fill_field * 0.24f;
            g += key_field * 1.08f + fill_field * 0.30f;
            b += key_field * 0.62f + fill_field * 0.42f;
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

henka_vec4 sandbox3d_debug_grid_color(void)
{
    return (henka_vec4){0.055f, 0.075f, 0.100f, 1.0f};
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
