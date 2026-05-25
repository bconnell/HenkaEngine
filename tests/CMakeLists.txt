add_executable(henka_tests
    test_assets.c
    test_camera.c
    test_input.c
    test_main.c
    test_math.c
    test_material.c
    test_model.c
    test_persistence.c
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

add_test(NAME henka_tests COMMAND henka_tests)
