#include <math.h>

#include <henka/accessibility.h>

int main(void)
{
    henka_accessibility_preferences preferences =
        henka_accessibility_preferences_default();
    float value = 99.0f;

    if (henka_accessibility_preferences_validate(&preferences) != HENKA_SUCCESS ||
        preferences.ui_scale != 1.0f ||
        preferences.text_scale != 1.0f ||
        preferences.motion_scale != 1.0f ||
        preferences.captions_enabled ||
        preferences.high_contrast_enabled)
    {
        return 1;
    }

    preferences.motion_scale = 0.25f;
    preferences.ui_scale = 1.5f;
    preferences.text_scale = 2.0f;
    preferences.captions_enabled = true;
    if (henka_accessibility_preferences_validate(&preferences) != HENKA_SUCCESS ||
        henka_accessibility_scale_motion(
            &preferences, -8.0f, &value) != HENKA_SUCCESS ||
        fabsf(value + 2.0f) > 0.0001f)
    {
        return 1;
    }

    value = 77.0f;
    preferences.motion_scale = -0.1f;
    if (henka_accessibility_preferences_validate(&preferences) !=
            HENKA_ERROR_INVALID_ARGUMENT ||
        henka_accessibility_scale_motion(
            &preferences, 1.0f, &value) != HENKA_ERROR_INVALID_ARGUMENT ||
        value != 77.0f)
    {
        return 1;
    }

    preferences = henka_accessibility_preferences_default();
    preferences.ui_scale = NAN;
    if (henka_accessibility_preferences_validate(&preferences) !=
        HENKA_ERROR_INVALID_ARGUMENT)
    {
        return 1;
    }

    return 0;
}
