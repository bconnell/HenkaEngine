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

# Give both single- and multi-config generators one explicit runtime directory
# expression so the asset lifecycle target can use the same destination
# without depending on the executable target itself.
set(EXTERNAL_GAME_RUNTIME_DIR "${CMAKE_BINARY_DIR}/$<CONFIG>")
set_target_properties(external_game_minimal PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${EXTERNAL_GAME_RUNTIME_DIR}"
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

# Asset delivery is a dependency of the executable target rather than a
# separate ALL-only side effect. The phony target intentionally runs whenever
# external_game_minimal is requested, including when the executable itself is
# already up to date. Engine-owned generic assets are copied first and the
# consumer's assets are copied second so project-owned files win on collisions.
add_custom_target(external_game_template_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${HENKA_ENGINE_DIR}/assets"
        "${EXTERNAL_GAME_RUNTIME_DIR}/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${EXTERNAL_GAME_RUNTIME_DIR}/assets"
    COMMENT "Synchronizing Henka and external game assets"
)
add_dependencies(external_game_minimal external_game_template_assets)
