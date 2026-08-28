cmake_minimum_required(VERSION 3.25)

project(external_game_minimal C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT DEFINED HENKA_ENGINE_DIR OR HENKA_ENGINE_DIR STREQUAL "")
    message(FATAL_ERROR "HENKA_ENGINE_DIR is required. Point it at a local Henka Engine checkout.")
endif()

if(NOT EXISTS "${HENKA_ENGINE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "HENKA_ENGINE_DIR does not point to a valid Henka Engine checkout.")
endif()

# KTX-Software contains long but valid source names, and Visual Studio's
# per-target file-tracking logs add another long path layer in nested
# FetchContent builds. Keep the consumer project reproducible on Windows
# without requiring global Git or MSBuild settings.
if(MSVC)
    set(CMAKE_VS_GLOBALS "TrackFileAccess=false" CACHE STRING "" FORCE)
    set(CMAKE_OBJECT_PATH_MAX 128 CACHE STRING "" FORCE)
endif()

set(HENKA_BUILD_EXAMPLES OFF CACHE BOOL "Build Henka Engine example applications." FORCE)
set(HENKA_BUILD_TESTS OFF CACHE BOOL "Build Henka Engine tests." FORCE)

add_subdirectory("${HENKA_ENGINE_DIR}" "${CMAKE_BINARY_DIR}/henka-engine")

add_executable(external_game_minimal
    src/main.c
)

target_link_libraries(external_game_minimal
    PRIVATE
        henka
)

if(MSVC)
    target_compile_options(external_game_minimal PRIVATE /W4 /WX)
else()
    target_compile_options(external_game_minimal PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_custom_command(TARGET external_game_minimal POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${HENKA_ENGINE_DIR}/assets"
        "$<TARGET_FILE_DIR:external_game_minimal>/assets"
)

# Keep consumer-owned script fixtures synchronized even when the executable
# itself is already up to date in a reused validation build directory.
add_custom_target(external_game_template_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:external_game_minimal>/assets"
    DEPENDS external_game_minimal
)
