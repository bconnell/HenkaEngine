add_executable(henka_tests
    test_main.c
    test_result.c
    test_memory.c
)

target_link_libraries(henka_tests
    PRIVATE
        henka
)

add_test(NAME henka_tests COMMAND henka_tests)
