#include "test_suite.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "../examples/sandbox3d/studio_environment.h"

void henka_test_sandbox3d_studio_environment(void)
{
    float* pixels;
    size_t row;
    size_t x;
    size_t peak_x = 0U;
    size_t broad_span = 0U;
    size_t lower_row = 50U;
    float peak_luma = 0.0f;
    float row_min_luma = FLT_MAX;
    float row_max_luma = 0.0f;
    float lower_min_luma = FLT_MAX;
    float lower_max_luma = 0.0f;
    int valid = 1;

    pixels = (float*)malloc(
        sizeof(*pixels) * SANDBOX3D_STUDIO_ENVIRONMENT_PIXEL_COUNT);
    HENKA_TEST_ASSERT(pixels != NULL);
    sandbox3d_generate_studio_environment(
        pixels,
        SANDBOX3D_STUDIO_ENVIRONMENT_PIXEL_COUNT);
    valid = sandbox3d_studio_environment_is_valid(
        pixels,
        SANDBOX3D_STUDIO_ENVIRONMENT_PIXEL_COUNT) ? 1 : 0;

    row = 28U;
    for (x = 0U; x < SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH; ++x)
    {
        const size_t offset =
            (row * SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH + x) *
            SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS;
        const float luma =
            pixels[offset + 0U] * 0.2126f +
            pixels[offset + 1U] * 0.7152f +
            pixels[offset + 2U] * 0.0722f;

        if (!isfinite(luma) || luma < 0.0f)
        {
            valid = 0;
        }
        if (luma > peak_luma)
        {
            peak_luma = luma;
            peak_x = x;
        }
        if (luma < row_min_luma)
        {
            row_min_luma = luma;
        }
        if (luma > row_max_luma)
        {
            row_max_luma = luma;
        }
        if (x < SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH)
        {
            const size_t lower_offset =
                (lower_row * SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH + x) *
                SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS;
            const float lower_luma =
                pixels[lower_offset + 0U] * 0.2126f +
                pixels[lower_offset + 1U] * 0.7152f +
                pixels[lower_offset + 2U] * 0.0722f;

            if (!isfinite(lower_luma) || lower_luma < 0.0f)
            {
                valid = 0;
            }
            if (lower_luma < lower_min_luma)
            {
                lower_min_luma = lower_luma;
            }
            if (lower_luma > lower_max_luma)
            {
                lower_max_luma = lower_luma;
            }
        }
    }
    if (!(peak_luma > 0.0f))
    {
        valid = 0;
    }
    for (x = 0U; x < SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH; ++x)
    {
        const size_t offset =
            (row * SANDBOX3D_STUDIO_ENVIRONMENT_WIDTH + x) *
            SANDBOX3D_STUDIO_ENVIRONMENT_CHANNELS;
        const float luma =
            pixels[offset + 0U] * 0.2126f +
            pixels[offset + 1U] * 0.7152f +
            pixels[offset + 2U] * 0.0722f;
        const size_t distance = x > peak_x ? x - peak_x : peak_x - x;

        if (luma >= peak_luma * 0.65f && distance < 64U)
        {
            ++broad_span;
        }
    }
    free(pixels);
    HENKA_TEST_ASSERT(
        valid &&
        broad_span >= 72U &&
        row_min_luma > 0.0f &&
        row_max_luma <= row_min_luma * 1.75f &&
        lower_max_luma > 0.0f &&
        lower_min_luma >= lower_max_luma * 0.45f);
}
