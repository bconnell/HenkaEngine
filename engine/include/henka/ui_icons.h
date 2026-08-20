#ifndef HENKA_UI_ICONS_H
#define HENKA_UI_ICONS_H

#include <stdbool.h>

#include <henka/ui.h>

typedef enum henka_ui_icon
{
    HENKA_UI_ICON_SELECT = 0,
    HENKA_UI_ICON_MOVE,
    HENKA_UI_ICON_ROTATE,
    HENKA_UI_ICON_SCALE,
    HENKA_UI_ICON_SNAP,
    HENKA_UI_ICON_SAVE,
    HENKA_UI_ICON_UNDO,
    HENKA_UI_ICON_REDO,
    HENKA_UI_ICON_BUILD,
    HENKA_UI_ICON_PLAY,
    HENKA_UI_ICON_WORLD,
    HENKA_UI_ICON_COUNT
} henka_ui_icon;

const char* henka_ui_icon_name(henka_ui_icon icon);

henka_result henka_ui_draw_icon(
    henka_ui_context* context,
    henka_ui_icon icon,
    henka_ui_rect bounds,
    henka_vec4 color);

typedef struct henka_ui_tool_button_desc
{
    const char* id;
    const char* label;
    const char* tooltip;
    henka_ui_icon icon;
    bool enabled;
    bool selected;
} henka_ui_tool_button_desc;

bool henka_ui_tool_button(
    henka_ui_context* context,
    henka_ui_rect bounds,
    const henka_ui_tool_button_desc* desc);

#endif
