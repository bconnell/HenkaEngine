#ifndef SANDBOX3D_EDITOR_SCALE_CONTROL_H
#define SANDBOX3D_EDITOR_SCALE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include <henka/ui.h>

#define SANDBOX3D_EDITOR_SCALE_COMPACT_INLINE_MIN_WIDTH 192.0f
#define SANDBOX3D_EDITOR_SCALE_COMPACT_LABEL_WIDTH 64.0f

typedef struct sandbox3d_editor_scale_control_layout_desc
{
    henka_ui_rect label;
    henka_ui_rect choices;
    bool stacked;
    bool compact;
} sandbox3d_editor_scale_control_layout_desc;

/* Shared geometry authority for scale-control measurement, automation
 * telemetry, and rendering. Narrow controls stack the label above the same
 * full-size 24-unit segmented hit targets. */
henka_result sandbox3d_editor_scale_control_layout(
    henka_ui_rect bounds,
    sandbox3d_editor_scale_control_layout_desc* out_layout);

/* Draws the Settings control and returns a candidate only after interaction.
 * It never mutates or persists the authoritative editor preference. */
henka_result sandbox3d_editor_scale_control_draw(
    henka_ui_context* ui,
    henka_ui_rect bounds,
    uint16_t current_percent,
    uint16_t* out_selected_percent,
    bool* out_changed);

#endif
