cmake_minimum_required(VERSION 3.25)

project(external_server_minimal C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT DEFINED HENKA_ENGINE_DIR OR HENKA_ENGINE_DIR STREQUAL "")
    message(FATAL_ERROR "HENKA_ENGINE_DIR is required. Point it at a local Henka Engine checkout.")
endif()

if(NOT EXISTS "${HENKA_ENGINE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "HENKA_ENGINE_DIR does not point to a valid Henka Engine checkout.")
endif()

set(HENKA_BUILD_CLIENT OFF CACHE BOOL "Build the graphical Henka client." FORCE)
set(HENKA_BUILD_DEDICATED_SERVER OFF CACHE BOOL "Build the bundled dedicated server." FORCE)
set(HENKA_BUILD_EXAMPLES OFF CACHE BOOL "Build Henka Engine example applications." FORCE)
set(HENKA_BUILD_TESTS OFF CACHE BOOL "Build Henka Engine tests." FORCE)
set(HENKA_ENABLE_KTX2_TRANSCODER OFF CACHE BOOL "Enable the graphical KTX transcoder." FORCE)
set(HENKA_ENABLE_NETWORK ON CACHE BOOL "Enable Henka network transport." FORCE)

add_subdirectory("${HENKA_ENGINE_DIR}" "${CMAKE_BINARY_DIR}/henka-engine")

add_executable(external_server_minimal
    src/main.c
)

target_link_libraries(external_server_minimal
    PRIVATE
        henka_runtime
)
