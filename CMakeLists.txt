cmake_minimum_required(VERSION 3.25)

project(HenkaEngine VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    # Keep clean Windows FetchContent builds independent of machine-wide
    # MSBuild file tracking and resilient to KTX-Software's nested paths.
    set(CMAKE_VS_GLOBALS "TrackFileAccess=false" CACHE STRING "" FORCE)
    set(CMAKE_OBJECT_PATH_MAX 128 CACHE STRING "" FORCE)
endif()

option(HENKA_BUILD_TESTS "Build Henka Engine tests." ON)
option(HENKA_BUILD_EXAMPLES "Build Henka Engine example applications." ON)
option(HENKA_BUILD_CLIENT "Build the graphical Henka client and compatibility target." ON)
option(HENKA_BUILD_DEDICATED_SERVER "Build the renderer-free dedicated server." ON)
option(HENKA_ENABLE_NETWORK "Build Henka networking support." ON)
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

if(HENKA_BUILD_CLIENT AND HENKA_ENABLE_KTX2_TRANSCODER)
    enable_language(CXX)
endif()

if(HENKA_BUILD_CLIENT OR HENKA_ENABLE_NETWORK)
    include(FetchContent)
endif()

if(HENKA_BUILD_CLIENT AND (TARGET SDL3::SDL3 OR TARGET SDL3::SDL3-static))
    message(STATUS "Using an SDL3 target supplied by the parent project.")
elseif(HENKA_BUILD_CLIENT)
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

if(HENKA_BUILD_CLIENT AND HENKA_ENABLE_KTX2_TRANSCODER)
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
        # KTX-Software contains legitimate long iOS resource names. Keep
        # the clean FetchContent clone reproducible on Windows without
        # requiring a pre-populated local source tree or machine-wide Git
        # configuration.
        GIT_CONFIG core.longpaths=true
        GIT_SUBMODULES ""
        GIT_SUBMODULES_RECURSE FALSE
    )
    FetchContent_MakeAvailable(KTXSoftware)
endif()

if(HENKA_ENABLE_NETWORK)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        ENet
        GIT_REPOSITORY https://github.com/lsalzman/enet.git
        GIT_TAG 5a9c537fd464b3c6d3c55e1d3bd47588faf71b42
        GIT_SUBMODULES ""
        GIT_SUBMODULES_RECURSE FALSE
    )
    FetchContent_GetProperties(ENet)
    if(NOT enet_POPULATED)
        if(POLICY CMP0169)
            cmake_policy(PUSH)
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Populate(ENet)
        if(POLICY CMP0169)
            cmake_policy(POP)
        endif()
    endif()
    add_library(henka_enet STATIC
        ${enet_SOURCE_DIR}/callbacks.c
        ${enet_SOURCE_DIR}/compress.c
        ${enet_SOURCE_DIR}/host.c
        ${enet_SOURCE_DIR}/list.c
        ${enet_SOURCE_DIR}/packet.c
        ${enet_SOURCE_DIR}/peer.c
        ${enet_SOURCE_DIR}/protocol.c
        ${enet_SOURCE_DIR}/unix.c
        ${enet_SOURCE_DIR}/win32.c
    )
    target_include_directories(henka_enet PRIVATE ${enet_SOURCE_DIR}/include)
    if(WIN32)
        target_link_libraries(henka_enet PRIVATE winmm ws2_32)
    endif()
endif()

add_subdirectory(engine)

if(HENKA_BUILD_CLIENT AND HENKA_BUILD_EXAMPLES)
    add_subdirectory(examples/sandbox3d)
endif()

if(HENKA_BUILD_DEDICATED_SERVER)
    add_subdirectory(examples/dedicated_server)
endif()

if(HENKA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
