#ifndef HENKA_ACCESSIBILITY_H
#define HENKA_ACCESSIBILITY_H

#include <stdbool.h>

#include <henka/result.h>

typedef struct henka_accessibility_preferences
{
    float ui_scale;
    float text_scale;
    float motion_scale;
    bool captions_enabled;
    bool high_contrast_enabled;
} henka_accessibility_preferences;

henka_accessibility_preferences henka_accessibility_preferences_default(void);
henka_result henka_accessibility_preferences_validate(
    const henka_accessibility_preferences* preferences);

/* Applies the user-owned reduced-motion scale to one finite signed motion
 * amount. A failed request leaves out_value unchanged. */
henka_result henka_accessibility_scale_motion(
    const henka_accessibility_preferences* preferences,
    float value,
    float* out_value);

#endif
