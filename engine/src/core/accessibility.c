#include <henka/accessibility.h>

#include <math.h>

henka_accessibility_preferences henka_accessibility_preferences_default(void)
{
    return (henka_accessibility_preferences){
        1.0f,
        1.0f,
        1.0f,
        false,
        false};
}

henka_result henka_accessibility_preferences_validate(
    const henka_accessibility_preferences* preferences)
{
    if (preferences == NULL ||
        !isfinite(preferences->ui_scale) ||
        !isfinite(preferences->text_scale) ||
        !isfinite(preferences->motion_scale) ||
        preferences->ui_scale < 0.5f ||
        preferences->ui_scale > 3.0f ||
        preferences->text_scale < 0.5f ||
        preferences->text_scale > 3.0f ||
        preferences->motion_scale < 0.0f ||
        preferences->motion_scale > 1.0f)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }
    return HENKA_SUCCESS;
}

henka_result henka_accessibility_scale_motion(
    const henka_accessibility_preferences* preferences,
    float value,
    float* out_value)
{
    float candidate;

    if (out_value == NULL ||
        !isfinite(value) ||
        henka_accessibility_preferences_validate(preferences) != HENKA_SUCCESS)
    {
        return HENKA_ERROR_INVALID_ARGUMENT;
    }

    candidate = value * preferences->motion_scale;
    if (!isfinite(candidate))
    {
        return HENKA_ERROR_NUMERIC_RANGE;
    }
    *out_value = candidate;
    return HENKA_SUCCESS;
}
