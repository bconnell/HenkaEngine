#ifndef SANDBOX3D_EDITOR_UTILITY_SETTINGS_LAYOUT_H
#define SANDBOX3D_EDITOR_UTILITY_SETTINGS_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/ui.h>

typedef enum sandbox3d_utility_settings_row
{
    SANDBOX3D_UTILITY_SETTINGS_ROW_LAYOUT = 0,
    SANDBOX3D_UTILITY_SETTINGS_ROW_SELECTED,
    SANDBOX3D_UTILITY_SETTINGS_ROW_GRID,
    SANDBOX3D_UTILITY_SETTINGS_ROW_SHADING,
    SANDBOX3D_UTILITY_SETTINGS_ROW_ENVIRONMENT,
    SANDBOX3D_UTILITY_SETTINGS_ROW_SKY_TIME,
    SANDBOX3D_UTILITY_SETTINGS_ROW_EXPOSURE,
    SANDBOX3D_UTILITY_SETTINGS_ROW_SENSITIVITY_SPEED,
    SANDBOX3D_UTILITY_SETTINGS_ROW_CONTROLS,
    SANDBOX3D_UTILITY_SETTINGS_ROW_COUNT
} sandbox3d_utility_settings_row;

typedef enum sandbox3d_utility_settings_control
{
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_EXPOSURE_LESS = 0,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_EXPOSURE_MORE,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_SENSITIVITY_LESS,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_SENSITIVITY_MORE,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_SPEED_LESS,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_SPEED_MORE,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_CYCLE,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_TIME,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_SKY_PRESET,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_VISIBLE,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_SIDE,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_SCALE,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_INFO_VISIBLE,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_INFO_MODE,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_COMPASS_SMOOTH,
    SANDBOX3D_UTILITY_SETTINGS_CONTROL_COUNT
} sandbox3d_utility_settings_control;

typedef struct sandbox3d_utility_settings_layout
{
    henka_ui_rect viewport;
    henka_ui_rect scale_bounds;
    henka_ui_rect rows[SANDBOX3D_UTILITY_SETTINGS_ROW_COUNT];
    henka_ui_rect controls[SANDBOX3D_UTILITY_SETTINGS_CONTROL_COUNT];
    henka_ui_rect compass_heading;
    float content_height;
    unsigned int adjustment_row_count;
    unsigned int environment_row_count;
    bool exposure_stacked;
    bool scale_stacked;
} sandbox3d_utility_settings_layout;

/* Builds the single geometry authority used by Utility Settings drawing,
 * scrolling, automation telemetry, and hit-target visibility checks. The
 * offset moves content only; the viewport remains fixed inside the dock. */
henka_result sandbox3d_utility_settings_layout_build(
    henka_ui_rect viewport,
    bool scale_stacked,
    float scroll_offset,
    sandbox3d_utility_settings_layout* out_layout);

bool sandbox3d_utility_settings_rect_is_fully_visible(
    henka_ui_rect viewport,
    henka_ui_rect rect);

#endif
