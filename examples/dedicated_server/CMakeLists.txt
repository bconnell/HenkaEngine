add_executable(henka_dedicated_server
    main.c
)

target_link_libraries(henka_dedicated_server
    PRIVATE
        henka_runtime
)

target_include_directories(henka_dedicated_server
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../engine/include
)

henka_enable_project_warnings(henka_dedicated_server)
