cmake_minimum_required(VERSION 3.25)

project(HenkaEngine VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(HENKA_BUILD_TESTS "Build Henka Engine tests." ON)
option(HENKA_BUILD_EXAMPLES "Build Henka Engine example applications." ON)

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.8
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL3)

add_subdirectory(engine)

if(HENKA_BUILD_EXAMPLES)
    add_subdirectory(examples/sandbox3d)
endif()

if(HENKA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
