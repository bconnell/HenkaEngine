add_executable(henka_sandbox3d
    editor_controls.c
    editor_ui_state.c
    camera_tools.c
    editor_preferences.c
    view_compass.c
    asset_browser_tools.c
    object_authoring_tools.c
    object_details_tools.c
    interaction_tools.c
    terrain_autosave.c
    physics_tools.c
    studio_environment.c
    workspace_tools.c
    editor_layout.c
    scene_document_bridge.c
    play_session.c
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

# The opt-in residency stress run uses distinct package paths backed by the
# checked-in texture fixtures. They exercise manager ownership and cache
# identity without making the normal sandbox scene larger or noisier.
foreach(residency_index RANGE 0 64)
    math(EXPR residency_parity "${residency_index} % 2")
    if(residency_parity EQUAL 0)
        set(residency_source cube_albedo.png)
    else()
        set(residency_source ground_checker.png)
    endif()
    add_custom_command(TARGET henka_sandbox3d POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:henka_sandbox3d>/assets/textures/residency
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/assets/textures/${residency_source}
            $<TARGET_FILE_DIR:henka_sandbox3d>/assets/textures/residency/residency_${residency_index}.png
    )
endforeach()
