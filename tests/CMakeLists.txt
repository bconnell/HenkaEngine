add_executable(henka_headless_runtime_tests
    test_headless_runtime.c
)

target_link_libraries(henka_headless_runtime_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_headless_runtime_tests)

add_test(NAME henka_headless_runtime_tests COMMAND henka_headless_runtime_tests)

if(WIN32)
    add_test(
        NAME henka_showcase_asset_generator_tests
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            ${CMAKE_SOURCE_DIR}/scripts/test_showcase_assets_windows.ps1
            -OutputDirectory ${CMAKE_BINARY_DIR}/showcase_asset_test)
endif()

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

add_executable(henka_terrain_streaming_tests
    test_terrain_streaming.c
)

target_link_libraries(henka_terrain_streaming_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_terrain_streaming_tests)

add_test(NAME henka_terrain_streaming_tests COMMAND henka_terrain_streaming_tests)

add_executable(henka_terrain_edit_tests
    test_terrain_edit.c
)

target_link_libraries(henka_terrain_edit_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_terrain_edit_tests)

add_test(NAME henka_terrain_edit_tests COMMAND henka_terrain_edit_tests)

add_executable(henka_terrain_network_tests
    test_terrain_network.c
)

target_link_libraries(henka_terrain_network_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_terrain_network_tests)

add_test(NAME henka_terrain_network_tests COMMAND henka_terrain_network_tests)

add_executable(henka_terrain_authority_tests
    test_terrain_authority.c
)

target_link_libraries(henka_terrain_authority_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_terrain_authority_tests)

add_test(NAME henka_terrain_authority_tests COMMAND henka_terrain_authority_tests)

add_executable(henka_terrain_server_tests
    test_terrain_server.c
)

target_link_libraries(henka_terrain_server_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_terrain_server_tests)

add_test(NAME henka_terrain_server_tests COMMAND henka_terrain_server_tests)

add_executable(henka_terrain_client_tests
    test_terrain_client.c
)
target_link_libraries(henka_terrain_client_tests
    PRIVATE
        henka_runtime
)
henka_enable_project_warnings(henka_terrain_client_tests)
add_test(NAME henka_terrain_client_tests COMMAND henka_terrain_client_tests)

add_executable(henka_terrain_process_client
    terrain_process_client.c
)
target_link_libraries(henka_terrain_process_client
    PRIVATE
        henka_runtime
)
henka_enable_project_warnings(henka_terrain_process_client)

add_executable(henka_terrain_replica_tests
    test_terrain_replica.c
)

target_link_libraries(henka_terrain_replica_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_terrain_replica_tests)

add_test(NAME henka_terrain_replica_tests COMMAND henka_terrain_replica_tests)

add_executable(henka_terrain_lod_tests
    test_terrain_lod.c
)

target_link_libraries(henka_terrain_lod_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_terrain_lod_tests)

add_test(NAME henka_terrain_lod_tests COMMAND henka_terrain_lod_tests)

add_executable(henka_terrain_collision_tests
    test_terrain_collision.c
)

target_link_libraries(henka_terrain_collision_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_terrain_collision_tests)

add_test(NAME henka_terrain_collision_tests COMMAND henka_terrain_collision_tests)

add_executable(henka_terrain_collision_runtime_tests
    test_terrain_collision_runtime.c
)

target_link_libraries(henka_terrain_collision_runtime_tests
    PRIVATE
        henka_runtime
)

target_include_directories(henka_terrain_collision_runtime_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/include
)

henka_enable_project_warnings(henka_terrain_collision_runtime_tests)

add_test(NAME henka_terrain_collision_runtime_tests COMMAND henka_terrain_collision_runtime_tests)

add_executable(henka_terrain_prediction_tests
    test_terrain_prediction.c
)

target_link_libraries(henka_terrain_prediction_tests
    PRIVATE
        henka_runtime
)

target_include_directories(henka_terrain_prediction_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/include
)

henka_enable_project_warnings(henka_terrain_prediction_tests)

add_test(NAME henka_terrain_prediction_tests COMMAND henka_terrain_prediction_tests)

add_executable(henka_terrain_physics_tests
    test_terrain_physics.c
)
target_link_libraries(henka_terrain_physics_tests
    PRIVATE
        henka_runtime
)
henka_enable_project_warnings(henka_terrain_physics_tests)
add_test(NAME henka_terrain_physics_tests COMMAND henka_terrain_physics_tests)

add_executable(henka_terrain_workflow_tests
    test_terrain_workflow.c
)
target_link_libraries(henka_terrain_workflow_tests
    PRIVATE
        henka_runtime
)
henka_enable_project_warnings(henka_terrain_workflow_tests)
add_test(NAME henka_terrain_workflow_tests COMMAND henka_terrain_workflow_tests)

add_executable(henka_heightfield_physics_tests
    test_heightfield_physics.c
)

target_link_libraries(henka_heightfield_physics_tests
    PRIVATE
        henka_runtime
)

target_include_directories(henka_heightfield_physics_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/engine/include
)

henka_enable_project_warnings(henka_heightfield_physics_tests)

add_test(NAME henka_heightfield_physics_tests COMMAND henka_heightfield_physics_tests)

add_executable(henka_terrain_mesh_tests
    test_terrain_mesh.c
)
target_link_libraries(henka_terrain_mesh_tests
    PRIVATE
        henka_runtime
)
henka_enable_project_warnings(henka_terrain_mesh_tests)
add_test(NAME henka_terrain_mesh_tests COMMAND henka_terrain_mesh_tests)

if(HENKA_BUILD_CLIENT)
add_executable(henka_terrain_render_tests
    test_terrain_render.c
)
target_link_libraries(henka_terrain_render_tests
    PRIVATE
        henka
)
henka_enable_project_warnings(henka_terrain_render_tests)
add_test(NAME henka_terrain_render_tests COMMAND henka_terrain_render_tests)
endif()

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
    ../examples/sandbox3d/studio_environment.c
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

# Tests use bounded fixtures under build/test_tmp relative to this test
# working directory. Keep
# CTest's working directory stable across single-test and full-suite runs, but
# isolate it from the repository root so repeated validation does not reuse a
# stale generated journal from an unrelated invocation.
get_property(HENKA_TEST_NAMES DIRECTORY PROPERTY TESTS)
if(HENKA_TEST_NAMES)
    set(HENKA_TEST_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/test-runtime")
    file(MAKE_DIRECTORY "${HENKA_TEST_WORKING_DIRECTORY}")
    set_tests_properties(${HENKA_TEST_NAMES} PROPERTIES WORKING_DIRECTORY "${HENKA_TEST_WORKING_DIRECTORY}")
endif()
