add_executable(henka_sandbox3d
    editor_controls.c
    editor_ui_state.c
    camera_tools.c
    editor_preferences.c
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
    physics_tools.c
    studio_environment.c
    workspace_tools.c
    workspace_persistence.c
    editor_layout.c
    scene_document_bridge.c
    play_session.c
    game_authoring.c
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

if(HENKA_ENABLE_KTX2_TRANSCODER)
    target_compile_definitions(henka_sandbox3d PRIVATE HENKA_WITH_KTX2_TRANSCODER=1)
    target_include_directories(henka_sandbox3d PRIVATE ${ktxsoftware_SOURCE_DIR}/include)
    target_link_libraries(henka_sandbox3d PRIVATE ktx)
endif()

henka_enable_project_warnings(henka_sandbox3d)

add_custom_command(TARGET henka_sandbox3d POST_BUILD
    COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File
        ${CMAKE_SOURCE_DIR}/scripts/generate_showcase_assets.ps1
        -OutputDirectory $<TARGET_FILE_DIR:henka_sandbox3d>/assets/models
)

add_custom_command(TARGET henka_sandbox3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:henka_sandbox3d>/assets
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
