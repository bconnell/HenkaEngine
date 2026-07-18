add_executable(henka_sandbox3d
    editor_controls.c
    interaction_tools.c
    workspace_tools.c
    main.c
)

target_link_libraries(henka_sandbox3d
    PRIVATE
        henka
)

henka_enable_project_warnings(henka_sandbox3d)

add_custom_command(TARGET henka_sandbox3d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:henka_sandbox3d>/assets
)
