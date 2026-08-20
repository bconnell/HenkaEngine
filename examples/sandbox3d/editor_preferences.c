#include "editor_preferences.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char* g_compass_keys[] =
{
    "viewport.compass.version",
    "viewport.compass.visible",
    "viewport.compass.side",
    "viewport.compass.scale",
    "viewport.compass.show_info",
    "viewport.compass.smooth_navigation",
    "viewport.compass.info_mode"
};

static const float g_compass_scales[] = {0.85f, 1.0f, 1.15f};

static bool sandbox3d_compass_scale_is_supported(float scale)
{
    size_t index;

    if (!isfinite(scale))
    {
        return false;
    }
    for (index = 0U; index < sizeof(g_compass_scales) / sizeof(g_compass_scales[0]); ++index)
    {
        if (fabsf(scale - g_compass_scales[index]) <= 0.0001f)
        {
            return true;
        }
    }
    return false;
}

static sandbox3d_view_compass_side sandbox3d_parse_side(
    const char* value)
{
    return value != NULL && strcmp(value, "left") == 0
        ? SANDBOX3D_VIEW_COMPASS_SIDE_LEFT
        : SANDBOX3D_VIEW_COMPASS_SIDE_RIGHT;
}

static sandbox3d_view_compass_info_mode sandbox3d_parse_info_mode(
    const char* value)
{
    if (value != NULL && strcmp(value, "position") == 0)
    {
        return SANDBOX3D_VIEW_COMPASS_INFO_POSITION;
    }
    if (value != NULL && strcmp(value, "target") == 0)
    {
        return SANDBOX3D_VIEW_COMPASS_INFO_TARGET;
    }
    return SANDBOX3D_VIEW_COMPASS_INFO_ORIENTATION;
}

static const char* sandbox3d_info_mode_setting_value(
    sandbox3d_view_compass_info_mode mode)
{
    switch (mode)
    {
        case SANDBOX3D_VIEW_COMPASS_INFO_POSITION:
            return "position";
        case SANDBOX3D_VIEW_COMPASS_INFO_TARGET:
            return "target";
        case SANDBOX3D_VIEW_COMPASS_INFO_ORIENTATION:
        default:
            return "orientation";
    }
}

void sandbox3d_view_compass_preferences_defaults(
    sandbox3d_view_compass_preferences* preferences)
{
    if (preferences == NULL)
    {
        return;
    }

    *preferences = (sandbox3d_view_compass_preferences){
        true,
        SANDBOX3D_VIEW_COMPASS_SIDE_RIGHT,
        1.0f,
        true,
        true,
        SANDBOX3D_VIEW_COMPASS_INFO_ORIENTATION};
}

bool sandbox3d_view_compass_preferences_validate(
    const sandbox3d_view_compass_preferences* preferences)
{
    size_t index;

    if (preferences == NULL ||
        (preferences->side != SANDBOX3D_VIEW_COMPASS_SIDE_RIGHT &&
         preferences->side != SANDBOX3D_VIEW_COMPASS_SIDE_LEFT) ||
        !isfinite(preferences->scale) ||
        preferences->scale < g_compass_scales[0] ||
        preferences->scale > g_compass_scales[2] ||
        (preferences->info_mode < SANDBOX3D_VIEW_COMPASS_INFO_ORIENTATION ||
         preferences->info_mode >= SANDBOX3D_VIEW_COMPASS_INFO_COUNT))
    {
        return false;
    }

    for (index = 0U; index < sizeof(g_compass_scales) / sizeof(g_compass_scales[0]); ++index)
    {
        if (fabsf(preferences->scale - g_compass_scales[index]) <= 0.0001f)
        {
            return true;
        }
    }
    return false;
}

void sandbox3d_view_compass_preferences_load(
    const henka_settings* settings,
    sandbox3d_view_compass_preferences* preferences)
{
    const char* value;
    int info_mode;
    float scale;

    sandbox3d_view_compass_preferences_defaults(preferences);
    if (settings == NULL || preferences == NULL)
    {
        return;
    }

    preferences->visible = henka_settings_get_bool(settings, g_compass_keys[1], preferences->visible);
    preferences->side = sandbox3d_parse_side(henka_settings_get_string(settings, g_compass_keys[2], "right"));
    scale = henka_settings_get_float(settings, g_compass_keys[3], preferences->scale);
    preferences->scale = sandbox3d_compass_scale_is_supported(scale)
        ? scale
        : sandbox3d_view_compass_scale_for_index(1U);
    preferences->show_info = henka_settings_get_bool(settings, g_compass_keys[4], preferences->show_info);
    preferences->smooth_navigation = henka_settings_get_bool(settings, g_compass_keys[5], preferences->smooth_navigation);
    value = henka_settings_get_string(settings, g_compass_keys[6], "orientation");
    info_mode = (int)sandbox3d_parse_info_mode(value);
    if (info_mode < (int)SANDBOX3D_VIEW_COMPASS_INFO_ORIENTATION ||
        info_mode >= (int)SANDBOX3D_VIEW_COMPASS_INFO_COUNT)
    {
        info_mode = (int)SANDBOX3D_VIEW_COMPASS_INFO_ORIENTATION;
    }
    preferences->info_mode = (sandbox3d_view_compass_info_mode)info_mode;
}

henka_result sandbox3d_view_compass_preferences_store(
    henka_settings* settings,
    const sandbox3d_view_compass_preferences* preferences)
{
    henka_result result;

    if (settings == NULL || !sandbox3d_view_compass_preferences_validate(preferences))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    result = henka_settings_set_int(settings, g_compass_keys[0], 1);
    if (result == HENKA_SUCCESS) result = henka_settings_set_bool(settings, g_compass_keys[1], preferences->visible);
    if (result == HENKA_SUCCESS) result = henka_settings_set_string(settings, g_compass_keys[2], sandbox3d_view_compass_side_label(preferences->side));
    if (result == HENKA_SUCCESS) result = henka_settings_set_float(settings, g_compass_keys[3], preferences->scale);
    if (result == HENKA_SUCCESS) result = henka_settings_set_bool(settings, g_compass_keys[4], preferences->show_info);
    if (result == HENKA_SUCCESS) result = henka_settings_set_bool(settings, g_compass_keys[5], preferences->smooth_navigation);
    if (result == HENKA_SUCCESS) result = henka_settings_set_string(settings, g_compass_keys[6], sandbox3d_info_mode_setting_value(preferences->info_mode));
    return result;
}

const char* sandbox3d_view_compass_side_label(
    sandbox3d_view_compass_side side)
{
    return side == SANDBOX3D_VIEW_COMPASS_SIDE_LEFT ? "Left" : "Right";
}

const char* sandbox3d_view_compass_info_mode_label(
    sandbox3d_view_compass_info_mode mode)
{
    switch (mode)
    {
        case SANDBOX3D_VIEW_COMPASS_INFO_POSITION:
            return "Position";
        case SANDBOX3D_VIEW_COMPASS_INFO_TARGET:
            return "Target";
        case SANDBOX3D_VIEW_COMPASS_INFO_ORIENTATION:
        default:
            return "Orientation";
    }
}

float sandbox3d_view_compass_scale_for_index(size_t index)
{
    return g_compass_scales[index < 3U ? index : 1U];
}

size_t sandbox3d_view_compass_scale_index(float scale)
{
    size_t index;
    size_t nearest = 1U;
    float distance = INFINITY;

    if (!isfinite(scale))
    {
        return 1U;
    }
    for (index = 0U; index < 3U; ++index)
    {
        const float candidate_distance = fabsf(scale - g_compass_scales[index]);
        if (candidate_distance < distance)
        {
            distance = candidate_distance;
            nearest = index;
        }
    }
    return nearest;
}

henka_result sandbox3d_view_compass_preferences_commit(
    henka_settings* settings,
    const char* settings_path,
    sandbox3d_view_compass_preferences* in_out_preferences,
    const sandbox3d_view_compass_preferences* candidate)
{
    sandbox3d_view_compass_preferences previous_preferences;
    bool had_keys[sizeof(g_compass_keys) / sizeof(g_compass_keys[0])];
    char previous_values[sizeof(g_compass_keys) / sizeof(g_compass_keys[0])][32];
    size_t index;
    henka_result result;

    if (settings == NULL || settings_path == NULL || settings_path[0] == '\0' ||
        in_out_preferences == NULL ||
        !sandbox3d_view_compass_preferences_validate(candidate))
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    previous_preferences = *in_out_preferences;
    for (index = 0U; index < sizeof(g_compass_keys) / sizeof(g_compass_keys[0]); ++index)
    {
        const char* value = henka_settings_get_string(settings, g_compass_keys[index], "");
        had_keys[index] = henka_settings_has_key(settings, g_compass_keys[index]);
        (void)snprintf(previous_values[index], sizeof(previous_values[index]), "%s", value != NULL ? value : "");
    }

    *in_out_preferences = *candidate;
    result = sandbox3d_view_compass_preferences_store(settings, candidate);
    if (result == HENKA_SUCCESS)
    {
        result = henka_settings_save_file(settings, settings_path);
    }
    if (result == HENKA_SUCCESS)
    {
        return HENKA_SUCCESS;
    }

    *in_out_preferences = previous_preferences;
    for (index = 0U; index < sizeof(g_compass_keys) / sizeof(g_compass_keys[0]); ++index)
    {
        if (had_keys[index])
        {
            (void)henka_settings_set_string(settings, g_compass_keys[index], previous_values[index]);
        }
        else
        {
            (void)henka_settings_remove(settings, g_compass_keys[index]);
        }
    }
    return result;
}
