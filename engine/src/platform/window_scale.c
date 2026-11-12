#include "window_scale.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>

enum
{
    HENKA_PLATFORM_WINDOW_SCALE_MAX_COMPONENT = 16U
};

static uint32_t henka_platform_window_scale_gcd(uint32_t left, uint32_t right)
{
    while (right != 0U)
    {
        const uint32_t remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

bool henka_platform_window_scale_from_display_scale(
    float display_scale,
    uint32_t* out_numerator,
    uint32_t* out_denominator)
{
    double best_error = DBL_MAX;
    uint32_t best_numerator = 0U;
    uint32_t best_denominator = 0U;
    uint32_t denominator;

    if (out_numerator == NULL || out_denominator == NULL ||
        !isfinite((double)display_scale) || display_scale < 0.25f ||
        display_scale > 4.0f)
    {
        return false;
    }

    /* SDL may report common display scales as inexact floats (for example,
     * 1.3333333 for 4/3). Search the bounded font-profile fraction domain and
     * choose the closest representable reduced ratio instead of preserving an
     * arbitrary decimal approximation. */
    for (denominator = 1U;
         denominator <= HENKA_PLATFORM_WINDOW_SCALE_MAX_COMPONENT;
         ++denominator)
    {
        const double scaled_numerator =
            (double)display_scale * (double)denominator;
        const long rounded_numerator = lround(scaled_numerator);
        uint32_t numerator;
        uint32_t divisor;
        uint32_t reduced_numerator;
        uint32_t reduced_denominator;
        double error;

        if (rounded_numerator < 1L ||
            rounded_numerator >
                (long)HENKA_PLATFORM_WINDOW_SCALE_MAX_COMPONENT)
        {
            continue;
        }

        numerator = (uint32_t)rounded_numerator;
        divisor = henka_platform_window_scale_gcd(numerator, denominator);
        if (divisor == 0U)
        {
            continue;
        }
        reduced_numerator = numerator / divisor;
        reduced_denominator = denominator / divisor;
        error = fabs((double)display_scale -
            (double)reduced_numerator / (double)reduced_denominator);

        if (error < best_error ||
            (error == best_error &&
                (best_denominator == 0U ||
                    reduced_denominator < best_denominator)))
        {
            best_error = error;
            best_numerator = reduced_numerator;
            best_denominator = reduced_denominator;
        }
    }

    if (best_numerator == 0U || best_denominator == 0U ||
        best_error > 0.0005)
    {
        return false;
    }

    *out_numerator = best_numerator;
    *out_denominator = best_denominator;
    return true;
}

bool henka_platform_window_scale_from_native_geometry(
    float display_scale,
    float pixel_density,
    uint32_t* out_native_to_content_numerator,
    uint32_t* out_native_to_content_denominator)
{
    if (out_native_to_content_numerator == NULL ||
        out_native_to_content_denominator == NULL ||
        !isfinite((double)display_scale) || display_scale < 0.25f ||
        display_scale > 4.0f || !isfinite((double)pixel_density) ||
        pixel_density < 0.25f || pixel_density > 4.0f)
    {
        return false;
    }

    return henka_platform_window_scale_from_display_scale(
        pixel_density / display_scale,
        out_native_to_content_numerator,
        out_native_to_content_denominator);
}

bool henka_platform_window_scale_content_size_from_framebuffer(
    int framebuffer_width,
    int framebuffer_height,
    uint32_t display_scale_numerator,
    uint32_t display_scale_denominator,
    int* out_content_width,
    int* out_content_height)
{
    double width;
    double height;
    long rounded_width;
    long rounded_height;

    if (out_content_width != NULL)
    {
        *out_content_width = 0;
    }
    if (out_content_height != NULL)
    {
        *out_content_height = 0;
    }
    if (framebuffer_width <= 0 || framebuffer_height <= 0 ||
        display_scale_numerator == 0U || display_scale_denominator == 0U ||
        out_content_width == NULL || out_content_height == NULL)
    {
        return false;
    }

    width = (double)framebuffer_width * (double)display_scale_denominator /
        (double)display_scale_numerator;
    height = (double)framebuffer_height * (double)display_scale_denominator /
        (double)display_scale_numerator;
    if (!isfinite(width) || !isfinite(height) || width < 0.5 ||
        height < 0.5 || width > (double)INT_MAX || height > (double)INT_MAX)
    {
        return false;
    }
    rounded_width = lround(width);
    rounded_height = lround(height);
    if (rounded_width <= 0L || rounded_height <= 0L ||
        rounded_width > (long)INT_MAX || rounded_height > (long)INT_MAX)
    {
        return false;
    }

    *out_content_width = (int)rounded_width;
    *out_content_height = (int)rounded_height;
    return true;
}

bool henka_platform_window_scale_content_size_to_native(
    int content_width,
    int content_height,
    uint32_t native_to_content_numerator,
    uint32_t native_to_content_denominator,
    int* out_native_width,
    int* out_native_height)
{
    double width;
    double height;
    long rounded_width;
    long rounded_height;

    if (out_native_width != NULL)
    {
        *out_native_width = 0;
    }
    if (out_native_height != NULL)
    {
        *out_native_height = 0;
    }
    if (content_width <= 0 || content_height <= 0 ||
        native_to_content_numerator == 0U ||
        native_to_content_denominator == 0U ||
        out_native_width == NULL || out_native_height == NULL)
    {
        return false;
    }

    width = (double)content_width *
        (double)native_to_content_denominator /
        (double)native_to_content_numerator;
    height = (double)content_height *
        (double)native_to_content_denominator /
        (double)native_to_content_numerator;
    if (!isfinite(width) || !isfinite(height) || width < 0.5 ||
        height < 0.5 || width > (double)INT_MAX || height > (double)INT_MAX)
    {
        return false;
    }
    rounded_width = lround(width);
    rounded_height = lround(height);
    if (rounded_width <= 0L || rounded_height <= 0L ||
        rounded_width > (long)INT_MAX || rounded_height > (long)INT_MAX)
    {
        return false;
    }

    *out_native_width = (int)rounded_width;
    *out_native_height = (int)rounded_height;
    return true;
}

bool henka_platform_window_scale_point_to_framebuffer(
    float logical_x,
    float logical_y,
    uint32_t scale_numerator,
    uint32_t scale_denominator,
    float* out_framebuffer_x,
    float* out_framebuffer_y)
{
    double scale;
    double framebuffer_x;
    double framebuffer_y;

    if (out_framebuffer_x == NULL || out_framebuffer_y == NULL ||
        !isfinite((double)logical_x) || !isfinite((double)logical_y) ||
        scale_numerator == 0U || scale_denominator == 0U)
    {
        return false;
    }

    scale = (double)scale_numerator / (double)scale_denominator;
    if (!isfinite(scale) || scale < 0.25 || scale > 4.0)
    {
        return false;
    }
    framebuffer_x = (double)logical_x * scale;
    framebuffer_y = (double)logical_y * scale;
    if (!isfinite(framebuffer_x) || !isfinite(framebuffer_y) ||
        fabs(framebuffer_x) > (double)FLT_MAX ||
        fabs(framebuffer_y) > (double)FLT_MAX)
    {
        return false;
    }

    *out_framebuffer_x = (float)framebuffer_x;
    *out_framebuffer_y = (float)framebuffer_y;
    return true;
}
