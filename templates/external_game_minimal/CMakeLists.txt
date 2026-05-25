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
