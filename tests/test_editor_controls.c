#include "test_suite.h"

#include <string.h>

#include "../examples/sandbox3d/editor_controls.h"

void henka_test_editor_controls(void)
{
    sandbox3d_editor_controls controls;
    sandbox3d_editor_controls loaded;
    sandbox3d_transform_session session;
    henka_settings* settings;
    henka_transform original;
    henka_transform output;
    char binding[64];
    char id_before[SANDBOX3D_EDITOR_PROFILE_ID_CAPACITY];
    int preset;

    sandbox3d_editor_controls_initialize(&controls);
    HENKA_TEST_ASSERT(strcmp(sandbox3d_editor_controls_get_active_profile_name(&controls), "Henka Default") == 0);
    sandbox3d_editor_controls_format_binding(&controls, HENKA_INPUT_ACTION_MOVE_TOOL, binding, sizeof(binding));
    HENKA_TEST_ASSERT(strcmp(binding, "M, G") == 0);

    for (preset = 0; preset < SANDBOX3D_EDITOR_PRESET_COUNT; ++preset)
    {
        char preset_id[24];
        snprintf(preset_id, sizeof(preset_id), "preset-%d", preset);
        HENKA_TEST_ASSERT(sandbox3d_editor_controls_set_active(&controls, preset_id) == HENKA_SUCCESS);
        HENKA_TEST_ASSERT(strcmp(sandbox3d_editor_controls_get_active_profile_name(&controls), sandbox3d_editor_preset_name((sandbox3d_editor_preset)preset)) == 0);
    }
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_set_binding_text(&controls, HENKA_INPUT_ACTION_MOVE_TOOL, "M") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_set_active(&controls, "preset-0-extra") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_create_custom(&controls, SANDBOX3D_EDITOR_PRESET_HENKA_DEFAULT, "Reserved ID", "preset-0") == HENKA_ERROR_INVALID_ARGUMENT);

    HENKA_TEST_ASSERT(sandbox3d_editor_controls_create_custom(&controls, SANDBOX3D_EDITOR_PRESET_HENKA_DEFAULT, "  My Controls  ", "profile-my-controls") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(sandbox3d_editor_controls_get_active_profile_name(&controls), "My Controls") == 0);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_create_custom(&controls, SANDBOX3D_EDITOR_PRESET_HENKA_DEFAULT, " ", "blank") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_create_custom(&controls, SANDBOX3D_EDITOR_PRESET_HENKA_DEFAULT, "My Controls", "duplicate") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_set_binding_text(&controls, HENKA_INPUT_ACTION_MOVE_TOOL, "G, G") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_set_binding_text(&controls, HENKA_INPUT_ACTION_MOVE_TOOL, "Unsupported") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_set_binding_text(&controls, HENKA_INPUT_ACTION_MOVE_TOOL, "X") == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_set_binding_text(&controls, HENKA_INPUT_ACTION_MOVE_TOOL, "G, M") == HENKA_SUCCESS);
    snprintf(id_before, sizeof(id_before), "%s", controls.custom_profiles[controls.active_custom_index].id);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_rename_custom(&controls, id_before, "  Renamed Controls ") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(controls.custom_profiles[controls.active_custom_index].id, id_before) == 0);
    HENKA_TEST_ASSERT(strcmp(sandbox3d_editor_controls_get_active_profile_name(&controls), "Renamed Controls") == 0);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_create_custom(&controls, SANDBOX3D_EDITOR_PRESET_HENKA_DEFAULT, "Secondary Controls", "profile-secondary") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_set_active(&controls, id_before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_delete_custom(&controls, "profile-secondary") == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(sandbox3d_editor_controls_get_active_profile_name(&controls), "Renamed Controls") == 0);

    HENKA_TEST_ASSERT(henka_settings_create(&settings) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_save(&controls, settings) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_load(&loaded, settings) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(sandbox3d_editor_controls_get_active_profile_name(&loaded), "Renamed Controls") == 0);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_reset_current(&loaded) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_delete_custom(&loaded, id_before) == HENKA_SUCCESS);
    HENKA_TEST_ASSERT(strcmp(sandbox3d_editor_controls_get_active_profile_name(&loaded), "Henka Default") == 0);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_create_custom(&loaded, SANDBOX3D_EDITOR_PRESET_DIRECT_TRANSFORM, "Reset Me", "profile-reset-me") == HENKA_SUCCESS);
    sandbox3d_editor_controls_reset_all(&loaded);
    HENKA_TEST_ASSERT(strcmp(sandbox3d_editor_controls_get_active_profile_name(&loaded), "Henka Default") == 0);
    HENKA_TEST_ASSERT(loaded.custom_profile_count == 0U);
    henka_settings_set_int(settings, "controls.custom_count", 99);
    HENKA_TEST_ASSERT(sandbox3d_editor_controls_load(&loaded, settings) == HENKA_ERROR_INVALID_ARGUMENT);
    HENKA_TEST_ASSERT(strcmp(sandbox3d_editor_controls_get_active_profile_name(&loaded), "Henka Default") == 0);
    henka_settings_destroy(settings);

    memset(&original, 0, sizeof(original));
    original.rotation = henka_quat_identity();
    original.scale = (henka_vec3){1.0f, 1.0f, 1.0f};
    HENKA_TEST_ASSERT(sandbox3d_transform_session_begin(&session, SANDBOX3D_TRANSFORM_TOOL_MOVE, 4U, original));
    HENKA_TEST_ASSERT(sandbox3d_transform_session_set_axis(&session, SANDBOX3D_TRANSFORM_AXIS_Y));
    HENKA_TEST_ASSERT(sandbox3d_transform_session_preview(&session, 1.0f, false, false));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(session.preview.position.y, 1.0f, 0.0001f);
    HENKA_TEST_ASSERT(sandbox3d_transform_session_cancel(&session, &output));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(output.position.y, 0.0f, 0.0001f);

    HENKA_TEST_ASSERT(sandbox3d_transform_session_begin(&session, SANDBOX3D_TRANSFORM_TOOL_SCALE, 4U, original));
    HENKA_TEST_ASSERT(sandbox3d_transform_session_preview(&session, 0.16f, true, false));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(session.preview.scale.x, 1.2f, 0.0001f);
    HENKA_TEST_ASSERT(sandbox3d_transform_session_confirm(&session, &output));
    HENKA_TEST_ASSERT_FLOAT_CLOSE(output.scale.x, 1.2f, 0.0001f);
}
