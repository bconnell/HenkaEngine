cmake_minimum_required(VERSION 3.25)

project(HenkaEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(HENKA_BUILD_TESTS "Build Henka Engine tests." ON)
option(HENKA_BUILD_EXAMPLES "Build Henka Engine example applications." ON)
option(HENKA_ENABLE_KTX2_TRANSCODER "Use pinned KTX-Software for checked KTX2/Basis transcoding." ON)

if(NOT WIN32)
    message(WARNING "Henka Engine currently validates its full build, runtime, packaging, and external-project path on Windows. Other platforms are not yet supported targets.")
endif()

function(henka_enable_project_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

include(FetchContent)

if(TARGET SDL3::SDL3 OR TARGET SDL3::SDL3-static)
    message(STATUS "Using an SDL3 target supplied by the parent project.")
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG d9d5536704d585616d4db3c8ba3c4ff6fc2757e1
    )
    FetchContent_MakeAvailable(SDL3)
endif()

if(HENKA_ENABLE_KTX2_TRANSCODER)
    set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
    set(KTX_FEATURE_JNI OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_PY OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_TOOLS_CTS OFF CACHE BOOL "" FORCE)
    # libktx's public stream vtable retains a KTX1 constructor reference even
    # for read/transcode-only KTX2 use; retain that compatibility object while
    # rejecting KTX1 at Henka's own texture boundary.
    set(KTX_FEATURE_KTX1 ON CACHE BOOL "" FORCE)
    set(KTX_FEATURE_KTX2 ON CACHE BOOL "" FORCE)
    set(KTX_FEATURE_VK_UPLOAD OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_GL_UPLOAD OFF CACHE BOOL "" FORCE)
    set(KTX_FEATURE_LOADTEST_APPS OFF CACHE STRING "" FORCE)
    FetchContent_Declare(
        KTXSoftware
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
        GIT_TAG 91ace88675ac59a97e55d0378a6602a9ae6b98bd
        GIT_SUBMODULES ""
        GIT_SUBMODULES_RECURSE FALSE
    )
    FetchContent_MakeAvailable(KTXSoftware)
endif()

add_subdirectory(engine)

if(HENKA_BUILD_EXAMPLES)
    add_subdirectory(examples/sandbox3d)
endif()

if(HENKA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
