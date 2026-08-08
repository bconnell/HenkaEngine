add_executable(henka_headless_runtime_tests
    test_headless_runtime.c
)

target_link_libraries(henka_headless_runtime_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_headless_runtime_tests)

add_test(NAME henka_headless_runtime_tests COMMAND henka_headless_runtime_tests)

add_executable(henka_terrain_tests
    test_terrain.c
)

target_link_libraries(henka_terrain_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_terrain_tests)

add_test(NAME henka_terrain_tests COMMAND henka_terrain_tests)

add_executable(henka_terrain_storage_tests
    test_terrain_storage.c
)

target_link_libraries(henka_terrain_storage_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_terrain_storage_tests)

add_test(NAME henka_terrain_storage_tests COMMAND henka_terrain_storage_tests)

if(HENKA_ENABLE_NETWORK)
add_executable(henka_network_tests
    test_network.c
)

target_link_libraries(henka_network_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_network_tests)

add_test(NAME henka_network_tests COMMAND henka_network_tests)
endif()

if(HENKA_BUILD_CLIENT)
add_executable(henka_tests
    ../examples/sandbox3d/editor_controls.c
    ../examples/sandbox3d/editor_ui_state.c
    ../examples/sandbox3d/object_details_tools.c
    ../examples/sandbox3d/object_authoring_tools.c
    ../examples/sandbox3d/asset_browser_tools.c
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
    test_sandbox3d_editor_ui.c
    test_sandbox3d_object_details.c
    test_sandbox3d_object_authoring.c
    test_sandbox3d_asset_browser.c
    test_main.c
    test_math.c
    test_material.c
    test_model.c
    test_persistence.c
    test_physics.c
    test_result.c
    test_memory.c
    test_scene.c
    test_shader.c
    test_ui.c
    test_viewport_shading.c
    test_workspace.c
)

target_link_libraries(henka_tests
    PRIVATE
        henka
)

if(HENKA_ENABLE_KTX2_TRANSCODER)
    target_compile_definitions(henka_tests PRIVATE HENKA_WITH_KTX2_TRANSCODER=1)
    target_include_directories(henka_tests PRIVATE ${ktxsoftware_SOURCE_DIR}/include)
endif()

henka_enable_project_warnings(henka_tests)

add_test(NAME henka_tests COMMAND henka_tests)
endif()
