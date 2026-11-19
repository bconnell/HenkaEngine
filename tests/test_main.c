#include "test_suite.h"

#include <string.h>

typedef void (*henka_test_group_fn)(void);

typedef struct henka_test_group
{
    const char* name;
    henka_test_group_fn run;
} henka_test_group;

static const henka_test_group g_henka_test_groups[] = {
    {"action", henka_test_action},
    {"assets", henka_test_assets},
    {"camera", henka_test_camera},
    {"editor_controls", henka_test_editor_controls},
    {"gizmo", henka_test_gizmo},
    {"input", henka_test_input},
    {"limits", henka_test_limits},
    {"math", henka_test_math},
    {"material", henka_test_material},
    {"model", henka_test_model},
    {"persistence", henka_test_persistence},
    {"physics", henka_test_physics},
    {"character_controller", henka_test_character_controller},
    {"result", henka_test_result},
    {"memory", henka_test_memory},
    {"sandbox3d_interaction", henka_test_sandbox3d_interaction},
    {"sandbox3d_physics", henka_test_sandbox3d_physics},
    {"sandbox3d_workspace", henka_test_sandbox3d_workspace},
    {"sandbox3d_editor_ui", henka_test_sandbox3d_editor_ui},
    {"sandbox3d_editor_scale_control", henka_test_sandbox3d_editor_scale_control},
    {"sandbox3d_automation_frame_capture", henka_test_sandbox3d_automation_frame_capture},
    {"sandbox3d_editor_font_bootstrap", henka_test_sandbox3d_editor_font_bootstrap},
    {"sandbox3d_modeling_selection", henka_test_sandbox3d_modeling_selection},
    {"sandbox3d_editor_layout", henka_test_sandbox3d_editor_layout},
    {"sandbox3d_camera_tools", henka_test_sandbox3d_camera_tools},
    {"camera_hardening", henka_test_camera_hardening},
    {"sandbox3d_object_details", henka_test_sandbox3d_object_details},
    {"sandbox3d_object_authoring", henka_test_sandbox3d_object_authoring},
    {"sandbox3d_authoring_asset_document", henka_test_sandbox3d_authoring_asset_document},
    {"sandbox3d_authoring_asset_ui", henka_test_sandbox3d_authoring_asset_ui},
    {"sandbox3d_asset_browser", henka_test_sandbox3d_asset_browser},
    {"sandbox3d_studio_environment", henka_test_sandbox3d_studio_environment},
    {"scene", henka_test_scene},
    {"shader", henka_test_shader},
    {"ui", henka_test_ui},
    {"ui_font", henka_test_ui_font},
    {"platform_display_scale", henka_test_platform_display_scale},
    {"viewport_shading", henka_test_viewport_shading},
    {"workspace", henka_test_workspace},
    {"view_compass", henka_test_view_compass}
};

static const size_t g_henka_test_group_count =
    sizeof(g_henka_test_groups) / sizeof(g_henka_test_groups[0]);

int g_henka_test_failures = 0;

static int henka_test_run_group(const char* requested_name)
{
    size_t index;

    for (index = 0U; index < g_henka_test_group_count; ++index)
    {
        if (strcmp(requested_name, g_henka_test_groups[index].name) == 0)
        {
            g_henka_test_groups[index].run();
            return 0;
        }
    }
    fprintf(stderr, "unknown test group: %s\n", requested_name);
    return 2;
}

int main(int argc, char** argv)
{
    size_t index;

    if (argc > 1)
    {
        int argument_index;
        if (((argc - 1) % 2) != 0)
        {
            fprintf(stderr, "usage: henka_tests [--group <name>]...\n");
            return 2;
        }
        for (argument_index = 1; argument_index < argc; argument_index += 2)
        {
            int result;
            if (strcmp(argv[argument_index], "--group") != 0)
            {
                fprintf(stderr, "usage: henka_tests [--group <name>]...\n");
                return 2;
            }
            result = henka_test_run_group(argv[argument_index + 1]);
            if (result != 0)
            {
                return result;
            }
        }
    }
    else
    {
        for (index = 0U; index < g_henka_test_group_count; ++index)
        {
            g_henka_test_groups[index].run();
        }
    }
    if (g_henka_test_failures > 0)
    {
        fprintf(stderr, "%d test(s) failed\n", g_henka_test_failures);
        return 1;
    }

    printf(argc == 1 ? "all tests passed\n" : "selected test groups passed\n");
    return 0;
}
