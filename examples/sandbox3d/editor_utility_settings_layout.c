#include "editor_utility_settings_layout.h"
#include "editor_scale_control.h"

#include <math.h>
#include <float.h>
#include <string.h>

static henka_ui_rect sandbox3d_utility_settings_rect(
    float x,
    float y,
    float width,
    float height)
{
    return (henka_ui_rect){x, y, width, height};
}

static bool sandbox3d_utility_settings_rect_values_are_valid(
    henka_ui_rect rect)
{
    return isfinite(rect.x) && isfinite(rect.y) &&
        isfinite(rect.width) && isfinite(rect.height) &&
        rect.width > 0.0f && rect.height > 0.0f &&
        isfinite(rect.x + rect.width) &&
        isfinite(rect.y + rect.height);
}

static bool sandbox3d_utility_settings_layout_geometry_is_valid(
    const sandbox3d_utility_settings_layout* layout)
{
    size_t index;

    if (layout == NULL ||
        !sandbox3d_utility_settings_rect_values_are_valid(layout->viewport) ||
        !sandbox3d_utility_settings_rect_values_are_valid(layout->scale_bounds) ||
        !sandbox3d_utility_settings_rect_values_are_valid(layout->compass_heading) ||
        !isfinite(layout->content_height) || layout->content_height <= 0.0f)
    {
        return false;
    }
    for (index = 0U; index < SANDBOX3D_UTILITY_SETTINGS_ROW_COUNT; ++index)
    {
        if (!sandbox3d_utility_settings_rect_values_are_valid(layout->rows[index]))
        {
            return false;
        }
    }
    for (index = 0U; index < SANDBOX3D_UTILITY_SETTINGS_CONTROL_COUNT; ++index)
    {
        if (!sandbox3d_utility_settings_rect_values_are_valid(layout->controls[index]))
        {
            return false;
        }
    }
    return true;
}

static bool sandbox3d_utility_settings_layout_values_are_valid(
    henka_ui_rect viewport,
    float scroll_offset)
{
    return isfinite(viewport.x) && isfinite(viewport.y) &&
        isfinite(viewport.width) && isfinite(viewport.height) &&
        isfinite(scroll_offset) && viewport.width >= 112.0f &&
        viewport.height > 0.0f && scroll_offset >= 0.0f;
}

henka_result sandbox3d_utility_settings_layout_build(
    henka_ui_rect viewport,
    bool scale_stacked,
    float scroll_offset,
    sandbox3d_utility_settings_layout* out_layout)
{
    const float left = viewport.x;
    const float content_y = viewport.y - scroll_offset;
    const float scale_advance = scale_stacked ? 56.0f : 12.0f;
    const float exposure_stack_advance = viewport.width < 184.0f ? 30.0f : 0.0f;
    const float rows_y = content_y + scale_advance;
    float adjustment_y;
    float environment_y;
    float compass_y;
    float right;
    float bottom;
    float content_height;
    unsigned int adjustment_rows;
    unsigned int environment_rows;

    if (out_layout == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(out_layout, 0, sizeof(*out_layout));
    if (!sandbox3d_utility_settings_layout_values_are_valid(
            viewport,
            scroll_offset))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    right = viewport.x + viewport.width;
    bottom = content_y + viewport.height;
    if (!isfinite(right) || !isfinite(bottom) ||
        right > FLT_MAX || right < -FLT_MAX ||
        bottom > FLT_MAX || bottom < -FLT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    adjustment_rows = viewport.width >= 274.0f ?
        1U : viewport.width >= 128.0f ? 2U : 4U;
    environment_rows = viewport.width >= 328.0f ?
        1U : viewport.width >= 232.0f ? 2U : 3U;
    adjustment_y = rows_y + 290.0f + exposure_stack_advance;
    environment_y = rows_y + 320.0f + exposure_stack_advance +
        (float)(adjustment_rows - 1U) * 30.0f;
    compass_y = environment_y +
        (float)environment_rows * 30.0f + 10.0f;
    content_height = scale_advance +
        (compass_y - rows_y) + 186.0f + 12.0f;
    if (!isfinite(content_height) || content_height <= 0.0f ||
        content_height > FLT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    out_layout->viewport = viewport;
    out_layout->scale_bounds = sandbox3d_utility_settings_rect(
        left,
        content_y,
        viewport.width,
        scale_stacked ||
                viewport.width < SANDBOX3D_EDITOR_SCALE_COMPACT_INLINE_MIN_WIDTH
            ? 44.0f
            : 24.0f);
    out_layout->scale_stacked = scale_stacked;
    out_layout->adjustment_row_count = adjustment_rows;
    out_layout->environment_row_count = environment_rows;
    out_layout->exposure_stacked = exposure_stack_advance > 0.0f;
    out_layout->content_height = content_height;
    out_layout->compass_heading = sandbox3d_utility_settings_rect(
        left,
        compass_y,
        viewport.width,
        20.0f);

    out_layout->rows[SANDBOX3D_UTILITY_SETTINGS_ROW_LAYOUT] =
        sandbox3d_utility_settings_rect(left, rows_y + 18.0f, viewport.width, 22.0f);
    out_layout->rows[SANDBOX3D_UTILITY_SETTINGS_ROW_SELECTED] =
        sandbox3d_utility_settings_rect(left, rows_y + 44.0f, viewport.width, 22.0f);
    out_layout->rows[SANDBOX3D_UTILITY_SETTINGS_ROW_GRID] =
        sandbox3d_utility_settings_rect(left, rows_y + 70.0f, viewport.width, 22.0f);
    out_layout->rows[SANDBOX3D_UTILITY_SETTINGS_ROW_SHADING] =
        sandbox3d_utility_settings_rect(left, rows_y + 96.0f, viewport.width, 22.0f);
    out_layout->rows[SANDBOX3D_UTILITY_SETTINGS_ROW_ENVIRONMENT] =
        sandbox3d_utility_settings_rect(left, rows_y + 122.0f, viewport.width, 22.0f);
    out_layout->rows[SANDBOX3D_UTILITY_SETTINGS_ROW_SKY_TIME] =
        sandbox3d_utility_settings_rect(left, rows_y + 148.0f, viewport.width, 22.0f);
    out_layout->rows[SANDBOX3D_UTILITY_SETTINGS_ROW_EXPOSURE] =
        sandbox3d_utility_settings_rect(left, rows_y + 174.0f, viewport.width, 22.0f);
    out_layout->rows[SANDBOX3D_UTILITY_SETTINGS_ROW_SENSITIVITY_SPEED] =
        sandbox3d_utility_settings_rect(
            left,
            rows_y + 234.0f + exposure_stack_advance,
            viewport.width,
            22.0f);
    out_layout->rows[SANDBOX3D_UTILITY_SETTINGS_ROW_CONTROLS] =
        sandbox3d_utility_settings_rect(
            left,
            rows_y + 260.0f + exposure_stack_advance,
            viewport.width,
            22.0f);

    out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_EXPOSURE_LESS] =
        sandbox3d_utility_settings_rect(left, rows_y + 204.0f, 88.0f, 24.0f);
    out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_EXPOSURE_MORE] =
        sandbox3d_utility_settings_rect(
            left + (exposure_stack_advance > 0.0f ? 0.0f : 96.0f),
            rows_y + 204.0f + exposure_stack_advance,
            88.0f,
            24.0f);

    if (adjustment_rows == 1U)
    {
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SENSITIVITY_LESS] =
            sandbox3d_utility_settings_rect(left, adjustment_y, 60.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SENSITIVITY_MORE] =
            sandbox3d_utility_settings_rect(left + 68.0f, adjustment_y, 60.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SPEED_LESS] =
            sandbox3d_utility_settings_rect(left + 146.0f, adjustment_y, 60.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SPEED_MORE] =
            sandbox3d_utility_settings_rect(left + 214.0f, adjustment_y, 60.0f, 24.0f);
    }
    else if (adjustment_rows == 2U)
    {
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SENSITIVITY_LESS] =
            sandbox3d_utility_settings_rect(left, adjustment_y, 60.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SENSITIVITY_MORE] =
            sandbox3d_utility_settings_rect(left + 68.0f, adjustment_y, 60.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SPEED_LESS] =
            sandbox3d_utility_settings_rect(left, adjustment_y + 30.0f, 60.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SPEED_MORE] =
            sandbox3d_utility_settings_rect(left + 68.0f, adjustment_y + 30.0f, 60.0f, 24.0f);
    }
    else
    {
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SENSITIVITY_LESS] =
            sandbox3d_utility_settings_rect(left, adjustment_y, 60.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SENSITIVITY_MORE] =
            sandbox3d_utility_settings_rect(left, adjustment_y + 30.0f, 60.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SPEED_LESS] =
            sandbox3d_utility_settings_rect(left, adjustment_y + 60.0f, 60.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SPEED_MORE] =
            sandbox3d_utility_settings_rect(left, adjustment_y + 90.0f, 60.0f, 24.0f);
    }

    if (environment_rows == 1U)
    {
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_CYCLE] =
            sandbox3d_utility_settings_rect(left, environment_y, 112.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_TIME] =
            sandbox3d_utility_settings_rect(left + 120.0f, environment_y, 112.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_PRESET] =
            sandbox3d_utility_settings_rect(left + 240.0f, environment_y, 88.0f, 24.0f);
    }
    else if (environment_rows == 2U)
    {
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_CYCLE] =
            sandbox3d_utility_settings_rect(left, environment_y, 112.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_TIME] =
            sandbox3d_utility_settings_rect(left + 120.0f, environment_y, 112.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_PRESET] =
            sandbox3d_utility_settings_rect(left, environment_y + 30.0f, 88.0f, 24.0f);
    }
    else
    {
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_CYCLE] =
            sandbox3d_utility_settings_rect(left, environment_y, 112.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_TIME] =
            sandbox3d_utility_settings_rect(left, environment_y + 30.0f, 112.0f, 24.0f);
        out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_PRESET] =
            sandbox3d_utility_settings_rect(left, environment_y + 60.0f, 88.0f, 24.0f);
    }

    out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_VISIBLE] =
        sandbox3d_utility_settings_rect(left, compass_y + 22.0f, viewport.width, 24.0f);
    out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_SIDE] =
        sandbox3d_utility_settings_rect(left, compass_y + 50.0f, viewport.width, 24.0f);
    out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_SCALE] =
        sandbox3d_utility_settings_rect(left, compass_y + 78.0f, viewport.width, 24.0f);
    out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_INFO_VISIBLE] =
        sandbox3d_utility_settings_rect(left, compass_y + 106.0f, viewport.width, 24.0f);
    out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_INFO_MODE] =
        sandbox3d_utility_settings_rect(left, compass_y + 134.0f, viewport.width, 24.0f);
    out_layout->controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_SMOOTH] =
        sandbox3d_utility_settings_rect(left, compass_y + 162.0f, viewport.width, 24.0f);

    if (!sandbox3d_utility_settings_layout_geometry_is_valid(out_layout))
    {
        memset(out_layout, 0, sizeof(*out_layout));
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    return HENKA_SUCCESS;
}

bool sandbox3d_utility_settings_rect_is_fully_visible(
    henka_ui_rect viewport,
    henka_ui_rect rect)
{
    const double viewport_right = (double)viewport.x + (double)viewport.width;
    const double viewport_bottom = (double)viewport.y + (double)viewport.height;
    const double rect_right = (double)rect.x + (double)rect.width;
    const double rect_bottom = (double)rect.y + (double)rect.height;

    return isfinite(viewport.x) && isfinite(viewport.y) &&
        isfinite(viewport.width) && isfinite(viewport.height) &&
        isfinite(rect.x) && isfinite(rect.y) &&
        isfinite(rect.width) && isfinite(rect.height) &&
        viewport.width > 0.0f && viewport.height > 0.0f &&
        rect.width > 0.0f && rect.height > 0.0f &&
        rect.x >= viewport.x && rect.y >= viewport.y &&
        rect_right <= viewport_right && rect_bottom <= viewport_bottom;
}
