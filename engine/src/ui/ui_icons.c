#include <henka/ui_icons.h>

#include <math.h>

static bool henka_ui_icon_bounds_are_valid(henka_ui_rect bounds)
{
    return isfinite((double)bounds.x) != 0 &&
        isfinite((double)bounds.y) != 0 &&
        isfinite((double)bounds.width) != 0 &&
        isfinite((double)bounds.height) != 0 &&
        bounds.width >= 8.0f && bounds.height >= 8.0f;
}

static henka_result henka_ui_draw_icon_polyline(
    henka_ui_context* context,
    const henka_vec2* points,
    size_t point_count,
    henka_vec4 color)
{
    return henka_ui_overlay_polyline(context, points, point_count, 1.5f, color);
}

const char* henka_ui_icon_name(henka_ui_icon icon)
{
    static const char* const names[HENKA_UI_ICON_COUNT] = {
        "select",
        "move",
        "rotate",
        "scale",
        "snap",
        "save",
        "undo",
        "redo",
        "build",
        "play",
        "world"};

    if (icon < 0 || icon >= HENKA_UI_ICON_COUNT)
    {
        return NULL;
    }
    return names[icon];
}

henka_result henka_ui_draw_icon(
    henka_ui_context* context,
    henka_ui_icon icon,
    henka_ui_rect bounds,
    henka_vec4 color)
{
    const float x = bounds.x + bounds.width * 0.5f;
    const float y = bounds.y + bounds.height * 0.5f;
    const float radius = fminf(bounds.width, bounds.height) * 0.32f;
    henka_vec2 points[8];

    if (context == NULL || !henka_ui_icon_bounds_are_valid(bounds) ||
        !isfinite((double)color.x) || !isfinite((double)color.y) ||
        !isfinite((double)color.z) || !isfinite((double)color.w) ||
        henka_ui_icon_name(icon) == NULL)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    switch (icon)
    {
        case HENKA_UI_ICON_SELECT:
            points[0] = (henka_vec2){x - radius * 0.35f, y - radius};
            points[1] = (henka_vec2){x - radius * 0.35f, y + radius * 0.8f};
            points[2] = (henka_vec2){x + radius * 0.05f, y + radius * 0.45f};
            points[3] = (henka_vec2){x + radius * 0.35f, y + radius};
            points[4] = (henka_vec2){x + radius * 0.55f, y + radius * 0.8f};
            points[5] = (henka_vec2){x + radius * 0.05f, y - radius * 0.1f};
            points[6] = (henka_vec2){x + radius * 0.45f, y - radius * 0.1f};
            points[7] = points[0];
            return henka_ui_draw_icon_polyline(context, points, 8U, color);
        case HENKA_UI_ICON_MOVE:
        {
            henka_result result;
            points[0] = (henka_vec2){x, y - radius};
            points[1] = (henka_vec2){x - radius * 0.25f, y - radius * 0.65f};
            points[2] = (henka_vec2){x + radius * 0.25f, y - radius * 0.65f};
            points[3] = points[0];
            points[4] = (henka_vec2){x, y + radius};
            points[5] = (henka_vec2){x - radius * 0.25f, y + radius * 0.65f};
            points[6] = (henka_vec2){x + radius * 0.25f, y + radius * 0.65f};
            points[7] = points[4];
            result = henka_ui_draw_icon_polyline(context, points, 4U, color);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            result = henka_ui_draw_icon_polyline(context, &points[4], 4U, color);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            return henka_ui_overlay_line(
                context,
                (henka_vec2){x - radius, y},
                (henka_vec2){x + radius, y},
                1.5f,
                color);
        }
        case HENKA_UI_ICON_ROTATE:
            points[0] = (henka_vec2){x + radius * 0.1f, y - radius};
            points[1] = (henka_vec2){x + radius * 0.65f, y - radius * 0.55f};
            points[2] = (henka_vec2){x + radius * 0.8f, y + radius * 0.1f};
            points[3] = (henka_vec2){x + radius * 0.45f, y + radius * 0.7f};
            points[4] = (henka_vec2){x - radius * 0.2f, y + radius * 0.8f};
            points[5] = (henka_vec2){x - radius * 0.7f, y + radius * 0.35f};
            points[6] = (henka_vec2){x - radius * 0.8f, y - radius * 0.25f};
            points[7] = (henka_vec2){x - radius * 0.35f, y - radius * 0.75f};
            return henka_ui_draw_icon_polyline(context, points, 8U, color);
        case HENKA_UI_ICON_SCALE:
            points[0] = (henka_vec2){x - radius, y - radius * 0.55f};
            points[1] = (henka_vec2){x - radius * 0.55f, y - radius};
            points[2] = (henka_vec2){x - radius * 0.55f, y - radius * 0.7f};
            points[3] = (henka_vec2){x + radius * 0.7f, y + radius * 0.55f};
            points[4] = (henka_vec2){x + radius, y + radius * 0.55f};
            points[5] = (henka_vec2){x + radius * 0.55f, y + radius};
            points[6] = (henka_vec2){x + radius * 0.55f, y + radius * 0.7f};
            points[7] = (henka_vec2){x - radius * 0.7f, y - radius * 0.55f};
            return henka_ui_draw_icon_polyline(context, points, 8U, color);
        case HENKA_UI_ICON_SNAP:
            points[0] = (henka_vec2){x - radius * 0.7f, y - radius * 0.7f};
            points[1] = (henka_vec2){x + radius * 0.7f, y - radius * 0.7f};
            points[2] = (henka_vec2){x + radius * 0.7f, y + radius * 0.7f};
            points[3] = (henka_vec2){x - radius * 0.7f, y + radius * 0.7f};
            points[4] = points[0];
            return henka_ui_draw_icon_polyline(context, points, 5U, color);
        case HENKA_UI_ICON_SAVE:
            points[0] = (henka_vec2){x - radius * 0.8f, y - radius * 0.8f};
            points[1] = (henka_vec2){x + radius * 0.8f, y - radius * 0.8f};
            points[2] = (henka_vec2){x + radius * 0.8f, y + radius * 0.8f};
            points[3] = (henka_vec2){x - radius * 0.8f, y + radius * 0.8f};
            points[4] = points[0];
            return henka_ui_draw_icon_polyline(context, points, 5U, color);
        case HENKA_UI_ICON_UNDO:
        case HENKA_UI_ICON_REDO:
            points[0] = (henka_vec2){x - radius * 0.8f, y};
            points[1] = (henka_vec2){x - radius * 0.35f, y - radius * 0.6f};
            points[2] = (henka_vec2){x + radius * 0.55f, y - radius * 0.5f};
            points[3] = (henka_vec2){x + radius * 0.8f, y + radius * 0.1f};
            points[4] = (henka_vec2){x + radius * 0.35f, y + radius * 0.7f};
            points[5] = (henka_vec2){x - radius * 0.35f, y + radius * 0.55f};
            points[6] = points[0];
            if (icon == HENKA_UI_ICON_REDO)
            {
                size_t index;
                for (index = 0U; index < 7U; ++index)
                {
                    points[index].x = 2.0f * x - points[index].x;
                }
            }
            return henka_ui_draw_icon_polyline(context, points, 7U, color);
        case HENKA_UI_ICON_BUILD:
        {
            henka_result result;
            points[0] = (henka_vec2){x - radius * 0.7f, y - radius * 0.55f};
            points[1] = (henka_vec2){x + radius * 0.7f, y - radius * 0.55f};
            points[2] = (henka_vec2){x + radius * 0.7f, y - radius * 0.1f};
            points[3] = (henka_vec2){x - radius * 0.7f, y - radius * 0.1f};
            points[4] = points[0];
            result = henka_ui_draw_icon_polyline(context, points, 5U, color);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            return henka_ui_overlay_line(
                context,
                (henka_vec2){x - radius * 0.35f, y - radius * 0.1f},
                (henka_vec2){x + radius * 0.45f, y + radius * 0.8f},
                1.5f,
                color);
        }
        case HENKA_UI_ICON_PLAY:
            points[0] = (henka_vec2){x - radius * 0.55f, y - radius * 0.8f};
            points[1] = (henka_vec2){x + radius * 0.7f, y};
            points[2] = (henka_vec2){x - radius * 0.55f, y + radius * 0.8f};
            points[3] = points[0];
            return henka_ui_draw_icon_polyline(context, points, 4U, color);
        case HENKA_UI_ICON_WORLD:
        {
            henka_result result;
            points[0] = (henka_vec2){x, y - radius};
            points[1] = (henka_vec2){x + radius * 0.7f, y - radius * 0.7f};
            points[2] = (henka_vec2){x + radius, y};
            points[3] = (henka_vec2){x + radius * 0.7f, y + radius * 0.7f};
            points[4] = (henka_vec2){x, y + radius};
            points[5] = (henka_vec2){x - radius * 0.7f, y + radius * 0.7f};
            points[6] = (henka_vec2){x - radius, y};
            points[7] = (henka_vec2){x - radius * 0.7f, y - radius * 0.7f};
            result = henka_ui_draw_icon_polyline(context, points, 8U, color);
            if (result != HENKA_SUCCESS)
            {
                return result;
            }
            return henka_ui_overlay_line(
                context,
                (henka_vec2){x - radius, y},
                (henka_vec2){x + radius, y},
                1.5f,
                color);
        }
        case HENKA_UI_ICON_COUNT:
        default:
            points[0] = (henka_vec2){x - radius * 0.7f, y - radius * 0.7f};
            points[1] = (henka_vec2){x + radius * 0.7f, y};
            points[2] = (henka_vec2){x - radius * 0.7f, y + radius * 0.7f};
            points[3] = points[0];
            return henka_ui_draw_icon_polyline(context, points, 4U, color);
    }
}

bool henka_ui_tool_button(
    henka_ui_context* context,
    henka_ui_rect bounds,
    const henka_ui_tool_button_desc* desc)
{
    const henka_vec2 mouse_position = henka_ui_get_mouse_position(context);
    const henka_ui_rect icon_bounds = {
        bounds.x + 6.0f,
        bounds.y + 6.0f,
        fminf(20.0f, bounds.width - 12.0f),
        fminf(20.0f, bounds.height - 12.0f)};
    bool clicked;

    if (context == NULL || desc == NULL || desc->id == NULL || desc->label == NULL ||
        desc->tooltip == NULL || !henka_ui_icon_bounds_are_valid(bounds) ||
        bounds.width < 44.0f || bounds.height < 28.0f ||
        henka_ui_icon_name(desc->icon) == NULL)
    {
        return false;
    }

    if (desc->enabled)
    {
        clicked = desc->selected
            ? henka_ui_selectable(context, desc->id, bounds, "", true)
            : henka_ui_button(context, desc->id, bounds, "");
    }
    else
    {
        clicked = false;
        (void)henka_ui_overlay_rect(
            context,
            bounds,
            (henka_vec4){0.12f, 0.14f, 0.17f, 0.72f});
    }

    (void)henka_ui_draw_icon(
        context,
        desc->icon,
        icon_bounds,
        desc->enabled
            ? (henka_vec4){0.72f, 0.82f, 0.90f, 1.0f}
            : (henka_vec4){0.40f, 0.45f, 0.51f, 1.0f});
    (void)henka_ui_label_colored(
        context,
        bounds.x + 32.0f,
        bounds.y + 9.0f,
        1.0f,
        desc->label,
        desc->enabled ? HENKA_UI_COLOR_NORMAL : HENKA_UI_COLOR_DISABLED);

    if (henka_ui_rect_contains(bounds, mouse_position) &&
        desc->tooltip[0] != '\0')
    {
        (void)henka_ui_overlay_hint(
            context,
            (henka_ui_rect){bounds.x, bounds.y + bounds.height + 4.0f, bounds.width, 28.0f},
            desc->tooltip,
            "");
    }
    return clicked;
}
