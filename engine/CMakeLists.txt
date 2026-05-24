add_library(henka STATIC
    src/core/math.c
    src/core/engine.c
    src/core/log.c
    src/core/memory.c
    src/core/result.c
    src/core/time.c
    src/platform/platform_sdl.c
    src/renderer/mesh.c
    src/renderer/renderer.c
    src/renderer/renderer_opengl.c
    src/renderer/shader.c
    src/scene/camera.c
    src/scene/scene.c
)

target_include_directories(henka
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(TARGET SDL3::SDL3)
    set(HENKA_SDL_TARGET SDL3::SDL3)
elseif(TARGET SDL3::SDL3-static)
    set(HENKA_SDL_TARGET SDL3::SDL3-static)
else()
    message(FATAL_ERROR "SDL3 target was not created by FetchContent.")
endif()

target_link_libraries(henka
    PRIVATE
        ${HENKA_SDL_TARGET}
)

if(WIN32)
    target_link_libraries(henka PRIVATE opengl32)
elseif(APPLE)
    find_library(HENKA_OPENGL_FRAMEWORK OpenGL REQUIRED)
    target_link_libraries(henka PRIVATE ${HENKA_OPENGL_FRAMEWORK})
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(henka PRIVATE OpenGL::GL)
endif()

target_compile_definitions(henka
    PRIVATE
        $<$<CONFIG:Debug>:HENKA_DEBUG=1>
)

target_compile_options(henka
    PRIVATE
        $<$<C_COMPILER_ID:MSVC>:/W4 /permissive->
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)
