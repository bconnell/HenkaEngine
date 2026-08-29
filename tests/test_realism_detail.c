#include "../examples/sandbox3d/realism_detail.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* This fixture preserves the coefficients and sampling algorithm used by the
 * pre-repair production generator. It is intentionally test-only: the
 * shipped generator lives in examples/sandbox3d/realism_detail.c. */
static uint32_t legacy_texture_hash(uint32_t x, uint32_t y, uint32_t seed)
{
    uint32_t value = x * 374761393U ^ y * 668265263U ^ seed * 2246822519U;

    value ^= value >> 13U;
    value *= 1274126177U;
    value ^= value >> 16U;
    return value;
}

static float legacy_value_noise(
    float u,
    float v,
    uint32_t seed,
    uint32_t x_period,
    uint32_t y_period)
{
    const float x = fmaxf(0.0f, fminf(1.0f, u)) * (float)x_period;
    const float y = fmaxf(0.0f, fminf(1.0f, v)) * (float)y_period;
    const float x_floor = floorf(x);
    const float y_floor = floorf(y);
    const float x_fraction = x - x_floor;
    const float y_fraction = y - y_floor;
    const float x_smooth = x_fraction * x_fraction * (3.0f - 2.0f * x_fraction);
    const float y_smooth = y_fraction * y_fraction * (3.0f - 2.0f * y_fraction);
    const uint32_t x0 = (uint32_t)x_floor % x_period;
    const uint32_t y0 = (uint32_t)y_floor % y_period;
    const uint32_t x1 = (x0 + 1U) % x_period;
    const uint32_t y1 = (y0 + 1U) % y_period;
    const float lower =
        (float)(legacy_texture_hash(x0, y0, seed) & 0xffffffU) /
            16777215.0f * (1.0f - x_smooth) +
        (float)(legacy_texture_hash(x1, y0, seed) & 0xffffffU) /
            16777215.0f * x_smooth;
    const float upper =
        (float)(legacy_texture_hash(x0, y1, seed) & 0xffffffU) /
            16777215.0f * (1.0f - x_smooth) +
        (float)(legacy_texture_hash(x1, y1, seed) & 0xffffffU) /
            16777215.0f * x_smooth;

    return lower * (1.0f - y_smooth) + upper * y_smooth;
}

static unsigned char legacy_detail_sample(
    size_t channel,
    float u,
    float v)
{
    float signal;
    float value;

    if (channel == 0U)
    {
        signal =
            legacy_value_noise(u, v, 101U, 5U, 5U) * 0.52f +
            legacy_value_noise(u, v, 103U, 13U, 11U) * 0.30f +
            legacy_value_noise(u, v, 107U, 29U, 23U) * 0.18f;
        value = 96.0f + signal * 82.0f;
    }
    else if (channel == 1U)
    {
        signal =
            legacy_value_noise(u, v, 131U, 9U, 5U) * 0.48f +
            legacy_value_noise(u, v, 137U, 23U, 9U) * 0.32f +
            legacy_value_noise(u, v, 139U, 43U, 17U) * 0.20f;
        value = 96.0f + signal * 48.0f;
    }
    else
    {
        signal =
            legacy_value_noise(u, v, 157U, 7U, 11U) * 0.55f +
            legacy_value_noise(u, v, 163U, 17U, 29U) * 0.30f +
            legacy_value_noise(u, v, 167U, 37U, 53U) * 0.15f;
        value = 34.0f + signal * 206.0f;
    }
    return (unsigned char)lroundf(fmaxf(0.0f, fminf(255.0f, value)));
}

static double legacy_average_delta(size_t channel, size_t stride)
{
    size_t x;
    size_t y;
    size_t samples = 0U;
    double total = 0.0;

    for (y = 0U; y < SANDBOX3D_REALISM_TEXTURE_EDGE; ++y)
    {
        for (x = 0U; x < SANDBOX3D_REALISM_TEXTURE_EDGE; ++x)
        {
            const float u = ((float)x + 0.5f) /
                (float)SANDBOX3D_REALISM_TEXTURE_EDGE;
            const float v = ((float)y + 0.5f) /
                (float)SANDBOX3D_REALISM_TEXTURE_EDGE;
            const float next_u = ((float)((x + stride) %
                SANDBOX3D_REALISM_TEXTURE_EDGE) + 0.5f) /
                (float)SANDBOX3D_REALISM_TEXTURE_EDGE;
            const float next_v = ((float)((y + stride) %
                SANDBOX3D_REALISM_TEXTURE_EDGE) + 0.5f) /
                (float)SANDBOX3D_REALISM_TEXTURE_EDGE;

            total += (double)abs(
                (int)legacy_detail_sample(channel, next_u, v) -
                (int)legacy_detail_sample(channel, u, v));
            total += (double)abs(
                (int)legacy_detail_sample(channel, u, next_v) -
                (int)legacy_detail_sample(channel, u, v));
            samples += 2U;
        }
    }
    return total / (double)samples;
}

static int require_condition(int condition, const char* expression)
{
    if (!condition)
    {
        fprintf(stderr, "realism detail assertion failed: %s\n", expression);
        return 0;
    }
    return 1;
}

#define REQUIRE(condition) \
    do \
    { \
        if (!require_condition((condition), #condition)) \
        { \
            result = 1; \
            goto cleanup; \
        } \
    } while (0)

static double average_delta(
    const unsigned char* pixels,
    size_t channel,
    size_t stride)
{
    size_t x;
    size_t y;
    size_t samples = 0U;
    double total = 0.0;

    for (y = 0U; y < SANDBOX3D_REALISM_TEXTURE_EDGE; ++y)
    {
        for (x = 0U; x < SANDBOX3D_REALISM_TEXTURE_EDGE; ++x)
        {
            const size_t current =
                (y * SANDBOX3D_REALISM_TEXTURE_EDGE + x) *
                SANDBOX3D_REALISM_TEXTURE_CHANNEL_COUNT + channel;
            const size_t horizontal =
                (y * SANDBOX3D_REALISM_TEXTURE_EDGE +
                    ((x + stride) % SANDBOX3D_REALISM_TEXTURE_EDGE)) *
                SANDBOX3D_REALISM_TEXTURE_CHANNEL_COUNT + channel;
            const size_t vertical =
                ((((y + stride) % SANDBOX3D_REALISM_TEXTURE_EDGE) *
                    SANDBOX3D_REALISM_TEXTURE_EDGE) + x) *
                SANDBOX3D_REALISM_TEXTURE_CHANNEL_COUNT + channel;

            total += (double)abs((int)pixels[horizontal] - (int)pixels[current]);
            total += (double)abs((int)pixels[vertical] - (int)pixels[current]);
            samples += 2U;
        }
    }
    return samples == 0U ? 0.0 : total / (double)samples;
}

static double boundary_delta(
    const unsigned char* pixels,
    size_t channel)
{
    size_t index;
    size_t samples = 0U;
    double total = 0.0;

    for (index = 0U; index < SANDBOX3D_REALISM_TEXTURE_EDGE; ++index)
    {
        const size_t left =
            (index * SANDBOX3D_REALISM_TEXTURE_EDGE) *
            SANDBOX3D_REALISM_TEXTURE_CHANNEL_COUNT + channel;
        const size_t right =
            (index * SANDBOX3D_REALISM_TEXTURE_EDGE +
                SANDBOX3D_REALISM_TEXTURE_EDGE - 1U) *
            SANDBOX3D_REALISM_TEXTURE_CHANNEL_COUNT + channel;
        const size_t top = index * SANDBOX3D_REALISM_TEXTURE_CHANNEL_COUNT + channel;
        const size_t bottom =
            (((SANDBOX3D_REALISM_TEXTURE_EDGE - 1U) *
                SANDBOX3D_REALISM_TEXTURE_EDGE) + index) *
            SANDBOX3D_REALISM_TEXTURE_CHANNEL_COUNT + channel;

        total += (double)abs((int)pixels[left] - (int)pixels[right]);
        total += (double)abs((int)pixels[top] - (int)pixels[bottom]);
        samples += 2U;
    }
    return samples == 0U ? 0.0 : total / (double)samples;
}

static int row_range(
    const unsigned char* pixels,
    size_t row,
    size_t channel)
{
    size_t x;
    int minimum = 255;
    int maximum = 0;

    for (x = 0U; x < SANDBOX3D_REALISM_TEXTURE_EDGE; ++x)
    {
        const size_t pixel =
            (row * SANDBOX3D_REALISM_TEXTURE_EDGE + x) *
            SANDBOX3D_REALISM_TEXTURE_CHANNEL_COUNT + channel;
        const int value = (int)pixels[pixel];

        if (value < minimum)
        {
            minimum = value;
        }
        if (value > maximum)
        {
            maximum = value;
        }
    }
    return maximum - minimum;
}

static int check_frequency_contract(
    const unsigned char* pixels,
    size_t channel,
    const char* label)
{
    const double adjacent = average_delta(pixels, channel, 1U);
    const double coarse = average_delta(pixels, channel, 8U);
    const double seam = boundary_delta(pixels, channel);

    if (!require_condition(adjacent > 0.5, label) ||
        !require_condition(coarse <= adjacent * 4.25, label) ||
        !require_condition(seam <= adjacent * 1.5, label))
    {
        fprintf(
            stderr,
            "%s frequency metrics: adjacent=%.3f coarse=%.3f seam=%.3f\n",
            label,
            adjacent,
            coarse,
            seam);
        return 0;
    }
    return 1;
}

int main(void)
{
    const size_t byte_count = SANDBOX3D_REALISM_TEXTURE_PIXEL_COUNT;
    int result = 1;
    unsigned char* normal_a = (unsigned char*)calloc(byte_count, 1U);
    unsigned char* macro_a = (unsigned char*)calloc(byte_count, 1U);
    unsigned char* wood_a = (unsigned char*)calloc(byte_count, 1U);
    unsigned char* wet_a = (unsigned char*)calloc(byte_count, 1U);
    unsigned char* normal_b = (unsigned char*)calloc(byte_count, 1U);
    unsigned char* macro_b = (unsigned char*)calloc(byte_count, 1U);
    unsigned char* wood_b = (unsigned char*)calloc(byte_count, 1U);
    unsigned char* wet_b = (unsigned char*)calloc(byte_count, 1U);

    REQUIRE(
        normal_a != NULL && macro_a != NULL && wood_a != NULL && wet_a != NULL &&
        normal_b != NULL && macro_b != NULL && wood_b != NULL && wet_b != NULL);
    REQUIRE(!sandbox3d_generate_realism_detail_textures(
        normal_a,
        byte_count - 1U,
        macro_a,
        byte_count,
        wood_a,
        byte_count,
        wet_a,
        byte_count));
    REQUIRE(sandbox3d_generate_realism_detail_textures(
        normal_a,
        byte_count,
        macro_a,
        byte_count,
        wood_a,
        byte_count,
        wet_a,
        byte_count));
    REQUIRE(sandbox3d_generate_realism_detail_textures(
        normal_b,
        byte_count,
        macro_b,
        byte_count,
        wood_b,
        byte_count,
        wet_b,
        byte_count));
    REQUIRE(memcmp(normal_a, normal_b, byte_count) == 0);
    REQUIRE(memcmp(macro_a, macro_b, byte_count) == 0);
    REQUIRE(memcmp(wood_a, wood_b, byte_count) == 0);
    REQUIRE(memcmp(wet_a, wet_b, byte_count) == 0);
    REQUIRE(row_range(macro_a, 0U, 0U) <= 8);
    REQUIRE(row_range(macro_a, SANDBOX3D_REALISM_TEXTURE_EDGE - 1U, 0U) <= 8);
    REQUIRE(row_range(wood_a, 0U, 0U) <= 8);
    REQUIRE(row_range(wood_a, SANDBOX3D_REALISM_TEXTURE_EDGE - 1U, 0U) <= 8);
    REQUIRE(row_range(wet_a, 0U, 1U) <= 8);
    REQUIRE(row_range(wet_a, SANDBOX3D_REALISM_TEXTURE_EDGE - 1U, 1U) <= 8);
    printf(
        "legacy frequency ratios: macro=%.2f wood=%.2f wet=%.2f\n",
        legacy_average_delta(0U, 8U) / legacy_average_delta(0U, 1U),
        legacy_average_delta(1U, 8U) / legacy_average_delta(1U, 1U),
        legacy_average_delta(2U, 8U) / legacy_average_delta(2U, 1U));
    REQUIRE(
        legacy_average_delta(0U, 8U) >
        legacy_average_delta(0U, 1U) * 4.25);
    REQUIRE(
        legacy_average_delta(1U, 8U) >
        legacy_average_delta(1U, 1U) * 4.25);
    REQUIRE(
        legacy_average_delta(2U, 8U) >
        legacy_average_delta(2U, 1U) * 4.25);
    REQUIRE(check_frequency_contract(macro_a, 0U, "macro variation"));
    REQUIRE(check_frequency_contract(wood_a, 0U, "wood grain"));
    REQUIRE(check_frequency_contract(wet_a, 1U, "wet/dry roughness"));

    result = 0;

cleanup:
    free(normal_a);
    free(macro_a);
    free(wood_a);
    free(wet_a);
    free(normal_b);
    free(macro_b);
    free(wood_b);
    free(wet_b);
    if (result == 0)
    {
        puts("realism detail frequency contract passed");
    }
    return result;
}
