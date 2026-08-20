#ifndef SANDBOX3D_EDITOR_PREFERENCES_H
#define SANDBOX3D_EDITOR_PREFERENCES_H

#include <stdbool.h>
#include <stddef.h>

#include <henka/persistence.h>

typedef enum sandbox3d_view_compass_side
{
    SANDBOX3D_VIEW_COMPASS_SIDE_RIGHT = 0,
    SANDBOX3D_VIEW_COMPASS_SIDE_LEFT
} sandbox3d_view_compass_side;

typedef enum sandbox3d_view_compass_info_mode
{
    SANDBOX3D_VIEW_COMPASS_INFO_ORIENTATION = 0,
    SANDBOX3D_VIEW_COMPASS_INFO_POSITION,
    SANDBOX3D_VIEW_COMPASS_INFO_TARGET,
    SANDBOX3D_VIEW_COMPASS_INFO_COUNT
} sandbox3d_view_compass_info_mode;

typedef struct sandbox3d_view_compass_preferences
{
    bool visible;
    sandbox3d_view_compass_side side;
    float scale;
    bool show_info;
    bool smooth_navigation;
    sandbox3d_view_compass_info_mode info_mode;
} sandbox3d_view_compass_preferences;

void sandbox3d_view_compass_preferences_defaults(
    sandbox3d_view_compass_preferences* preferences);

void sandbox3d_view_compass_preferences_load(
    const henka_settings* settings,
    sandbox3d_view_compass_preferences* preferences);

henka_result sandbox3d_view_compass_preferences_store(
    henka_settings* settings,
    const sandbox3d_view_compass_preferences* preferences);

bool sandbox3d_view_compass_preferences_validate(
    const sandbox3d_view_compass_preferences* preferences);

const char* sandbox3d_view_compass_side_label(
    sandbox3d_view_compass_side side);

const char* sandbox3d_view_compass_info_mode_label(
    sandbox3d_view_compass_info_mode mode);

float sandbox3d_view_compass_scale_for_index(size_t index);

size_t sandbox3d_view_compass_scale_index(float scale);

/*
 * Apply a committed preference mutation to the shared settings object and
 * persist it transactionally. On failure both the preference model and all
 * affected settings keys are restored to their previous values.
 */
henka_result sandbox3d_view_compass_preferences_commit(
    henka_settings* settings,
    const char* settings_path,
    sandbox3d_view_compass_preferences* in_out_preferences,
    const sandbox3d_view_compass_preferences* candidate);

#endif
