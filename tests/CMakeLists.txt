# Release test binaries must keep their assertions active. The engine and
# application targets retain the normal NDEBUG optimization contract, but a
# test binary that compiles out its checks can hide both regressions and
# warning-as-error failures in assertion-only code paths. Force the undefine
# through a header so MSVC does not emit D9025 for conflicting command-line
# definitions.
if(MSVC)
    add_compile_options("/FI${CMAKE_CURRENT_SOURCE_DIR}/test_assertions.h")
else()
    add_compile_options("-include${CMAKE_CURRENT_SOURCE_DIR}/test_assertions.h")
endif()

add_executable(henka_headless_runtime_tests
    test_headless_runtime.c
)

target_link_libraries(henka_headless_runtime_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_headless_runtime_tests)

add_test(NAME henka_headless_runtime_tests COMMAND henka_headless_runtime_tests)

add_executable(henka_audio_tests
    test_audio.c
)

target_link_libraries(henka_audio_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_audio_tests)

add_test(NAME henka_audio_tests COMMAND henka_audio_tests)

add_executable(henka_scene_document_tests
    test_scene_document.c
)

target_link_libraries(henka_scene_document_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_scene_document_tests)

add_test(NAME henka_scene_document_tests COMMAND henka_scene_document_tests)

add_executable(henka_script_host_tests
    test_script_host.c
)

target_link_libraries(henka_script_host_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_script_host_tests)

add_test(NAME henka_script_host_tests COMMAND henka_script_host_tests)

add_executable(henka_script_state_tests
    test_script_state.c
)

target_link_libraries(henka_script_state_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_script_state_tests)

add_test(NAME henka_script_state_tests COMMAND henka_script_state_tests)

add_executable(henka_script_runtime_tests
    test_script_runtime.c
)

target_link_libraries(henka_script_runtime_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_script_runtime_tests)

add_test(NAME henka_script_runtime_tests COMMAND henka_script_runtime_tests)

add_executable(henka_script_backend_tests
    test_script_backends.c
)

target_link_libraries(henka_script_backend_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_script_backend_tests)

add_test(NAME henka_script_backend_tests COMMAND henka_script_backend_tests)

add_executable(henka_lua_backend_tests
    test_lua_backend.c
)

target_link_libraries(henka_lua_backend_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_lua_backend_tests)

add_test(NAME henka_lua_backend_tests COMMAND henka_lua_backend_tests)

add_executable(henka_script_asset_tests
    test_script_asset.c
)

target_link_libraries(henka_script_asset_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_script_asset_tests)

add_test(NAME henka_script_asset_tests COMMAND henka_script_asset_tests)

add_executable(henka_script_source_tests
    test_script_source.c
)

target_link_libraries(henka_script_source_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_script_source_tests)

add_test(NAME henka_script_source_tests COMMAND henka_script_source_tests)

add_executable(henka_sandbox3d_script_editor_tests
    ../examples/sandbox3d/script_editor_model.c
    test_script_editor.c
)

target_include_directories(henka_sandbox3d_script_editor_tests
    PRIVATE
        ../examples/sandbox3d
)

target_link_libraries(henka_sandbox3d_script_editor_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_sandbox3d_script_editor_tests)

add_test(
    NAME henka_sandbox3d_script_editor_tests
    COMMAND henka_sandbox3d_script_editor_tests
)

add_executable(henka_scene_behavior_runtime_tests
    test_scene_behavior_runtime.c
)

target_link_libraries(henka_scene_behavior_runtime_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_scene_behavior_runtime_tests)

add_test(NAME henka_scene_behavior_runtime_tests COMMAND henka_scene_behavior_runtime_tests)

add_executable(henka_henkascript_tests
    test_henkascript.c
)

target_link_libraries(henka_henkascript_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_henkascript_tests)

add_test(NAME henka_henkascript_tests COMMAND henka_henkascript_tests)

add_executable(henka_sandbox3d_scene_document_bridge_tests
    ../examples/sandbox3d/scene_document_bridge.c
    test_sandbox3d_scene_document_bridge.c
)

target_link_libraries(henka_sandbox3d_scene_document_bridge_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_sandbox3d_scene_document_bridge_tests)

add_test(
    NAME henka_sandbox3d_scene_document_bridge_tests
    COMMAND henka_sandbox3d_scene_document_bridge_tests
)

add_executable(henka_sandbox3d_play_session_tests
    ../examples/sandbox3d/scene_document_bridge.c
    ../examples/sandbox3d/play_session.c
    test_sandbox3d_play_session.c
)

target_link_libraries(henka_sandbox3d_play_session_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_sandbox3d_play_session_tests)

add_test(
    NAME henka_sandbox3d_play_session_tests
    COMMAND henka_sandbox3d_play_session_tests
)

add_executable(henka_sandbox3d_game_authoring_tests
    ../examples/sandbox3d/scene_document_bridge.c
    ../examples/sandbox3d/play_session.c
    ../examples/sandbox3d/game_authoring.c
    test_sandbox3d_game_authoring.c
)

target_link_libraries(henka_sandbox3d_game_authoring_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_sandbox3d_game_authoring_tests)

add_test(
    NAME henka_sandbox3d_game_authoring_tests
    COMMAND henka_sandbox3d_game_authoring_tests
)

if(WIN32)
    add_test(
        NAME henka_fullscreen_presentation_mapping_tests
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            ${CMAKE_SOURCE_DIR}/scripts/test_fullscreen_presentation_windows.ps1
            -RepositoryRoot ${CMAKE_SOURCE_DIR})

    add_test(
        NAME henka_showcase_asset_generator_tests
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            ${CMAKE_SOURCE_DIR}/scripts/test_showcase_assets_windows.ps1
            -OutputDirectory ${CMAKE_BINARY_DIR}/showcase_asset_test)

    add_test(
        NAME henka_branding_asset_tests
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            ${CMAKE_SOURCE_DIR}/scripts/test_branding_windows.ps1
            -RepositoryRoot ${CMAKE_SOURCE_DIR})

    add_test(
        NAME henka_ssr_roughness_buffer_tests
        COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
            ${CMAKE_SOURCE_DIR}/scripts/test_ssr_roughness_buffer_windows.ps1)
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

add_executable(henka_authoring_mesh_tests
    test_authoring_mesh.c
)

target_link_libraries(henka_authoring_mesh_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_authoring_mesh_tests)

add_test(NAME henka_authoring_mesh_tests COMMAND henka_authoring_mesh_tests)

add_executable(henka_authoring_topology_tests
    test_authoring_topology.c
)

target_link_libraries(henka_authoring_topology_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_authoring_topology_tests)

add_test(
    NAME henka_authoring_topology_tests
    COMMAND henka_authoring_topology_tests
)

add_executable(henka_authoring_topology_report
    authoring_topology_report.c
)

target_link_libraries(henka_authoring_topology_report
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_authoring_topology_report)

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
add_executable(henka_opengl_capability_probe
    test_opengl_capability.c
)
target_link_libraries(henka_opengl_capability_probe
    PRIVATE
        henka
)
henka_enable_project_warnings(henka_opengl_capability_probe)

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
    ../examples/sandbox3d/camera_tools.c
    ../examples/sandbox3d/editor_preferences.c
    ../examples/sandbox3d/view_compass.c
    ../examples/sandbox3d/object_details_tools.c
    ../examples/sandbox3d/authoring_asset_controller.c
    ../examples/sandbox3d/authoring_asset_document.c
    ../examples/sandbox3d/authoring_asset_commands.c
    ../examples/sandbox3d/authoring_asset_ui.c
    ../examples/sandbox3d/object_authoring_tools.c
    ../examples/sandbox3d/modeling_operator.c
    ../examples/sandbox3d/modeling_selection.c
    ../examples/sandbox3d/modeling_selection_commands.c
    ../examples/sandbox3d/modeling_selection_scene.c
    ../examples/sandbox3d/modeling_toolbar.c
    ../examples/sandbox3d/asset_browser_tools.c
    ../examples/sandbox3d/interaction_tools.c
    ../examples/sandbox3d/terrain_autosave.c
    ../examples/sandbox3d/physics_tools.c
    ../examples/sandbox3d/studio_environment.c
    ../examples/sandbox3d/workspace_tools.c
    ../examples/sandbox3d/editor_layout.c
    test_action.c
    test_assets.c
    test_camera.c
    test_editor_controls.c
    test_gizmo.c
    test_input.c
    test_limits.c
    test_sandbox3d_interaction.c
    test_sandbox3d_modeling_selection.c
    test_sandbox3d_physics.c
    test_sandbox3d_workspace.c
    test_sandbox3d_editor_ui.c
    test_sandbox3d_editor_layout.c
    test_sandbox3d_camera_tools.c
    test_camera_hardening.c
    test_sandbox3d_object_details.c
    test_sandbox3d_object_authoring.c
    test_sandbox3d_authoring_asset_document.c
    test_sandbox3d_authoring_asset_ui.c
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
    test_view_compass.c
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

# Tests use bounded fixtures under build/test_tmp relative to the repository
# root. Keep CTest's working directory stable across single-test and full-suite
# runs so every test resolves the same documented fixture root.
get_property(HENKA_TEST_NAMES DIRECTORY PROPERTY TESTS)
if(HENKA_TEST_NAMES)
    set(HENKA_TEST_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    set_tests_properties(${HENKA_TEST_NAMES} PROPERTIES WORKING_DIRECTORY "${HENKA_TEST_WORKING_DIRECTORY}")
endif()
add_executable(henka_authoring_quad_recovery_tests
    test_authoring_quad_recovery.c
)

target_link_libraries(henka_authoring_quad_recovery_tests
    PRIVATE
        henka_runtime
)

henka_enable_project_warnings(henka_authoring_quad_recovery_tests)

add_test(
    NAME henka_authoring_quad_recovery_tests
    COMMAND henka_authoring_quad_recovery_tests
)
