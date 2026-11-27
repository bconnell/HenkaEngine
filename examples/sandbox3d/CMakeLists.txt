add_executable(henka_sandbox3d
    editor_automation_frame_capture.c
    editor_controls.c
    editor_ui_state.c
    camera_tools.c
    editor_preferences.c
    editor_font_bootstrap.c
    editor_typography_runtime.c
    editor_text_layout.c
    view_compass.c
    asset_browser_tools.c
    authoring_asset_controller.c
    authoring_asset_document.c
    authoring_asset_commands.c
    authoring_asset_ui.c
    object_authoring_tools.c
    modeling_operator.c
    modeling_selection.c
    modeling_selection_commands.c
    modeling_selection_scene.c
    modeling_toolbar.c
    object_details_tools.c
    interaction_tools.c
    terrain_autosave.c
    terrain_scene_transition.c
    physics_tools.c
    studio_environment.c
    workspace_tools.c
    workspace_persistence.c
    editor_layout.c
    editor_scale_control.c
    editor_utility_settings_layout.c
    editor_scale_transaction.c
    scene_hierarchy_projection.c
    transform_inspector.c
    scene_document_bridge.c
    play_session.c
    game_authoring.c
    project_save_transaction.c
    scene_name_editor.c
    scene_path_editor.c
    project_scene_catalog.c
    mcp_server.c
    audio_runtime.c
    script_editor_model.c
    script_editor.c
    realism_detail.c
    main.c
)

target_link_libraries(henka_sandbox3d
    PRIVATE
        henka
)

if(TARGET SDL3::SDL3)
    target_link_libraries(henka_sandbox3d PRIVATE SDL3::SDL3)
elseif(TARGET SDL3::SDL3-static)
    target_link_libraries(henka_sandbox3d PRIVATE SDL3::SDL3-static)
endif()

if(HENKA_ENABLE_KTX2_TRANSCODER)
    target_compile_definitions(henka_sandbox3d PRIVATE HENKA_WITH_KTX2_TRANSCODER=1)
    target_include_directories(henka_sandbox3d PRIVATE ${ktxsoftware_SOURCE_DIR}/include)
    target_link_libraries(henka_sandbox3d PRIVATE ktx)
endif()

henka_enable_project_warnings(henka_sandbox3d)

# Showcase source assets do not vary by configuration. Keep one generated
# output set for the whole multi-config build tree, then copy it into the
# active executable configuration after that target is built.
set(HENKA_SHOWCASE_ASSET_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/showcase_assets/assets/models")
set(HENKA_SHOWCASE_ASSET_MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/showcase_assets/showcase-assets-manifest.json")

set(HENKA_SHOWCASE_ASSET_FILENAMES
    cheeky_giraffe.gltf
    cheeky_giraffe.bin
    giraffe_base_color.png
    giraffe_detail_normal.png
    giraffe_metallic_roughness.png
    original_realistic_rocket.gltf
    original_realistic_rocket.bin
    rocket_base_color.png
    rocket_detail_normal.png
    rocket_metallic_roughness.png
)
set(HENKA_SHOWCASE_ASSET_OUTPUTS)
foreach(showcase_filename IN LISTS HENKA_SHOWCASE_ASSET_FILENAMES)
    list(APPEND HENKA_SHOWCASE_ASSET_OUTPUTS
        "${HENKA_SHOWCASE_ASSET_OUTPUT_DIRECTORY}/${showcase_filename}")
endforeach()
list(APPEND HENKA_SHOWCASE_ASSET_OUTPUTS "${HENKA_SHOWCASE_ASSET_MANIFEST}")

add_custom_command(OUTPUT ${HENKA_SHOWCASE_ASSET_OUTPUTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${HENKA_SHOWCASE_ASSET_OUTPUT_DIRECTORY}"
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
        ${CMAKE_SOURCE_DIR}/scripts/generate_showcase_assets.ps1
        -OutputDirectory "${HENKA_SHOWCASE_ASSET_OUTPUT_DIRECTORY}"
        -ReuseVerifiedOutput
        -ManifestPath "${HENKA_SHOWCASE_ASSET_MANIFEST}"
    DEPENDS
        ${CMAKE_SOURCE_DIR}/scripts/generate_showcase_assets.ps1
        ${CMAKE_SOURCE_DIR}/scripts/henka_script_common.ps1
        ${CMAKE_SOURCE_DIR}/scripts/showcase_generation_cache.ps1
    VERBATIM
)
add_custom_target(henka_showcase_asset_generation DEPENDS ${HENKA_SHOWCASE_ASSET_OUTPUTS})
add_dependencies(henka_sandbox3d henka_showcase_asset_generation)

add_custom_command(TARGET henka_sandbox3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:henka_sandbox3d>/assets
)

add_custom_command(TARGET henka_sandbox3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${HENKA_SHOWCASE_ASSET_OUTPUT_DIRECTORY}
        $<TARGET_FILE_DIR:henka_sandbox3d>/assets/models
)

add_custom_command(TARGET henka_sandbox3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        $<TARGET_FILE_DIR:henka_sandbox3d>/assets/textures/residency
)

# The residency stress run is opt-in. Keep its bounded duplicate fixtures out
# of ordinary Sandbox3D builds and generate them only when the package/stress
# validation target is explicitly requested.
add_custom_target(henka_sandbox3d_residency_fixtures
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
        ${CMAKE_SOURCE_DIR}/scripts/generate_residency_fixtures_windows.ps1
        -OutputDirectory $<TARGET_FILE_DIR:henka_sandbox3d>/assets/textures/residency
    DEPENDS henka_sandbox3d
    VERBATIM
)
