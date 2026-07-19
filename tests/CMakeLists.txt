add_executable(henka_tests
    ../examples/sandbox3d/editor_controls.c
    ../examples/sandbox3d/interaction_tools.c
    ../examples/sandbox3d/physics_tools.c
    ../examples/sandbox3d/workspace_tools.c
    test_action.c
    test_assets.c
    test_camera.c
    test_editor_controls.c
    test_gizmo.c
    test_input.c
    test_limits.c
    test_sandbox3d_interaction.c
    test_sandbox3d_physics.c
    test_sandbox3d_workspace.c
    test_main.c
    test_math.c
    test_material.c
    test_model.c
    test_persistence.c
    test_physics.c
    test_result.c
    test_memory.c
    test_scene.c
    test_ui.c
    test_workspace.c
)

target_link_libraries(henka_tests
    PRIVATE
        henka
)

henka_enable_project_warnings(henka_tests)

add_test(NAME henka_tests COMMAND henka_tests)
