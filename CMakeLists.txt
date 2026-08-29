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
    # KTX-Software returns application-owned buffers from its C API. Keep the
    # static dependency and every Henka target on the same CRT heap in every
    # multi-config build so those buffers can be released safely on Windows.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL" CACHE STRING "" FORCE)
endif()

option(HENKA_BUILD_TESTS "Build Henka Engine tests." ON)
option(HENKA_BUILD_EXAMPLES "Build Henka Engine example applications." ON)
option(HENKA_BUILD_CLIENT "Build the graphical Henka client and compatibility target." ON)
option(HENKA_BUILD_DEDICATED_SERVER "Build the renderer-free dedicated server." ON)
option(HENKA_ENABLE_NETWORK "Build Henka networking support." ON)
option(HENKA_ENABLE_KTX2_TRANSCODER "Use pinned KTX-Software for checked KTX2/Basis transcoding." ON)
option(HENKA_ENABLE_LUA "Build the bounded Lua behavior backend." ON)
option(HENKA_ENABLE_SANITIZERS "Enable AddressSanitizer/UndefinedBehaviorSanitizer for first-party targets." OFF)

if(NOT WIN32)
    message(WARNING "Henka Engine currently validates its full build, runtime, packaging, and external-project path on Windows. Other platforms are not yet supported targets.")
endif()

function(henka_enable_project_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    if(HENKA_ENABLE_SANITIZERS)
        get_target_property(target_type ${target_name} TYPE)
        if(MSVC)
            # MSVC currently provides AddressSanitizer, but not a supported
            # UndefinedBehaviorSanitizer equivalent. Keep the switch local to
            # first-party targets. The compiler emits the required runtime
            # directive; /fsanitize=address is not a link.exe option.
            target_compile_options(${target_name} PRIVATE /fsanitize=address /Zi)
            if(NOT target_type STREQUAL "STATIC_LIBRARY")
                target_link_options(${target_name} PRIVATE /INCREMENTAL:NO)
            endif()
        elseif(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(
                ${target_name}
                PRIVATE
                    -fsanitize=address,undefined
                    -fno-omit-frame-pointer)
            if(NOT target_type STREQUAL "STATIC_LIBRARY")
                target_link_options(
                    ${target_name}
                    PRIVATE
                        -fsanitize=address,undefined)
            endif()
        else()
            message(FATAL_ERROR
                "HENKA_ENABLE_SANITIZERS requires MSVC, Clang, or GCC.")
        endif()
    endif()
endfunction()

if(HENKA_BUILD_CLIENT AND HENKA_ENABLE_KTX2_TRANSCODER)
    enable_language(CXX)
endif()

include(FetchContent)

# Audio decoding is a runtime dependency even for headless builds. Keep the
# decoder source pinned and static so supported compressed formats do not add
# a platform DLL or a machine-wide installation requirement.
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 9634bedb5b5a2ca38c1ee7108a9358a4e233f14d
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE FALSE
)
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(miniaudio)
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()
endif()

# miniaudio exposes a small stb_vorbis adapter rather than bundling the
# Vorbis decoder. Pin the public-domain stb source separately and keep both
# dependencies behind Henka's private decoder wrapper.
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE FALSE
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(stb)
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()
endif()

add_library(henka_audio_decoder STATIC
    engine/src/core/audio_decoder.c
)
target_include_directories(henka_audio_decoder
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
        ${miniaudio_SOURCE_DIR}
        ${stb_SOURCE_DIR}
)
# The translation unit contains upstream single-file decoder implementations.
# Their warning policy is not part of Henka's first-party warning surface; the
# wrapper and all callers remain covered by Henka's normal warning gates.
if(MSVC)
    target_compile_options(henka_audio_decoder PRIVATE /W0)
else()
    target_compile_options(henka_audio_decoder PRIVATE -w)
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

if(HENKA_ENABLE_LUA)
    FetchContent_Declare(
        Lua
        GIT_REPOSITORY https://github.com/lua/lua.git
        GIT_TAG 6e22fedb74cf0c9b6656e9fce8b7331db847c605
        GIT_SHALLOW TRUE
        GIT_SUBMODULES ""
        GIT_SUBMODULES_RECURSE FALSE
    )
    FetchContent_GetProperties(Lua)
    if(NOT lua_POPULATED)
        if(POLICY CMP0169)
            cmake_policy(PUSH)
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Populate(Lua)
        if(POLICY CMP0169)
            cmake_policy(POP)
        endif()
    endif()
    add_library(henka_lua STATIC
        ${lua_SOURCE_DIR}/lapi.c
        ${lua_SOURCE_DIR}/lauxlib.c
        ${lua_SOURCE_DIR}/lbaselib.c
        ${lua_SOURCE_DIR}/lcode.c
        ${lua_SOURCE_DIR}/lcorolib.c
        ${lua_SOURCE_DIR}/lctype.c
        ${lua_SOURCE_DIR}/ldblib.c
        ${lua_SOURCE_DIR}/ldebug.c
        ${lua_SOURCE_DIR}/ldo.c
        ${lua_SOURCE_DIR}/ldump.c
        ${lua_SOURCE_DIR}/lfunc.c
        ${lua_SOURCE_DIR}/lgc.c
        ${lua_SOURCE_DIR}/linit.c
        ${lua_SOURCE_DIR}/liolib.c
        ${lua_SOURCE_DIR}/llex.c
        ${lua_SOURCE_DIR}/lmathlib.c
        ${lua_SOURCE_DIR}/lmem.c
        ${lua_SOURCE_DIR}/loadlib.c
        ${lua_SOURCE_DIR}/lobject.c
        ${lua_SOURCE_DIR}/lopcodes.c
        ${lua_SOURCE_DIR}/loslib.c
        ${lua_SOURCE_DIR}/lparser.c
        ${lua_SOURCE_DIR}/lstate.c
        ${lua_SOURCE_DIR}/lstring.c
        ${lua_SOURCE_DIR}/lstrlib.c
        ${lua_SOURCE_DIR}/ltable.c
        ${lua_SOURCE_DIR}/ltablib.c
        ${lua_SOURCE_DIR}/ltm.c
        ${lua_SOURCE_DIR}/lundump.c
        ${lua_SOURCE_DIR}/lutf8lib.c
        ${lua_SOURCE_DIR}/lvm.c
        ${lua_SOURCE_DIR}/lzio.c
    )
    target_include_directories(henka_lua PUBLIC ${lua_SOURCE_DIR})
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
