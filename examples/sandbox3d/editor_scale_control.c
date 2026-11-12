#include "editor_scale_control.h"

#include <math.h>
#include <float.h>
#include <stddef.h>
#include <string.h>

static const uint16_t g_editor_scale_percentages[] = {
    100U,
    125U,
    150U,
    200U};

static const char* const g_editor_scale_labels[] = {
    "100%",
    "125%",
    "150%",
    "200%"};

henka_result sandbox3d_editor_scale_control_layout(
    henka_ui_rect bounds,
    sandbox3d_editor_scale_control_layout_desc* out_layout)
{
    double right;
    double bottom;

    if (out_layout == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    memset(out_layout, 0, sizeof(*out_layout));
    if (!isfinite(bounds.x) || !isfinite(bounds.y) ||
        !isfinite(bounds.width) || !isfinite(bounds.height) ||
        bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    right = (double)bounds.x + (double)bounds.width;
    bottom = (double)bounds.y + (double)bounds.height;
    if (!isfinite(right) || !isfinite(bottom) ||
        right > (double)FLT_MAX || right < -(double)FLT_MAX ||
        bottom > (double)FLT_MAX || bottom < -(double)FLT_MAX)
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }

    if (bounds.width >= 272.0f && bounds.height >= 24.0f)
    {
        out_layout->label = (henka_ui_rect){
            bounds.x, bounds.y, 72.0f, 24.0f};
        out_layout->choices = (henka_ui_rect){
            bounds.x + 72.0f,
            bounds.y,
            bounds.width - 72.0f,
            24.0f};
        out_layout->stacked = false;
        return HENKA_SUCCESS;
    }

    if (bounds.width >= 176.0f && bounds.height >= 44.0f)
    {
        out_layout->label = (henka_ui_rect){
            bounds.x, bounds.y, bounds.width, 16.0f};
        out_layout->choices = (henka_ui_rect){
            bounds.x,
            bounds.y + 20.0f,
            bounds.width,
            24.0f};
        out_layout->stacked = true;
        return HENKA_SUCCESS;
    }

    return HENKA_ERROR_LIMIT;
}

static bool sandbox3d_editor_scale_find_index(
    uint16_t percent,
    size_t* out_index)
{
    size_t index;

    if (out_index != NULL)
    {
        *out_index = 0U;
    }

    if (out_index == NULL)
    {
        return false;
    }

    for (index = 0U;
         index < sizeof(g_editor_scale_percentages) /
             sizeof(g_editor_scale_percentages[0]);
         ++index)
    {
        if (g_editor_scale_percentages[index] == percent)
        {
            *out_index = index;
            return true;
        }
    }

    return false;
}

henka_result sandbox3d_editor_scale_control_draw(
    henka_ui_context* ui,
    henka_ui_rect bounds,
    uint16_t current_percent,
    uint16_t* out_selected_percent,
    bool* out_changed)
{
    size_t selected_index;
    bool changed;
    henka_result result;

    if (out_selected_percent != NULL)
    {
        *out_selected_percent = current_percent;
    }
    if (out_changed != NULL)
    {
        *out_changed = false;
    }

    if (ui == NULL ||
        out_selected_percent == NULL ||
        out_changed == NULL ||
        !sandbox3d_editor_scale_find_index(
            current_percent,
            &selected_index))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    changed = false;
    result = henka_ui_segmented_select(
        ui,
        "settings.editor.ui_scale",
        bounds,
        g_editor_scale_labels,
        sizeof(g_editor_scale_labels) / sizeof(g_editor_scale_labels[0]),
        &selected_index,
        &changed);
    if (result != HENKA_SUCCESS)
    {
        return result;
    }

    if (changed)
    {
        *out_selected_percent =
            g_editor_scale_percentages[selected_index];
        *out_changed = true;
    }

    return HENKA_SUCCESS;
}
