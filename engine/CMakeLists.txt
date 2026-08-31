add_library(henka_runtime STATIC
    src/core/math.c
    src/core/log.c
    src/core/memory.c
    src/core/persistence.c
    src/core/physics.c
    src/core/character_controller.c
    src/core/prefab.c
    src/core/result.c
    src/core/scene_document.c
    src/core/time.c
    src/core/audio.c
    src/scripting/script_host.c
    src/scripting/script_state.c
    src/scripting/script_runtime.c
    src/scripting/script_source.c
    src/scripting/henkascript.c
    src/scripting/henkascript_backend.c
    src/scripting/script_asset.c
    src/scripting/scene_behavior_runtime.c
    src/scripting/lua_source.c
    $<$<BOOL:${HENKA_ENABLE_LUA}>:src/scripting/lua_backend.c>
    src/scene/camera.c
    src/scene/scene.c
    src/mesh/authoring_mesh.c
    src/mesh/authoring_topology.c
    src/mesh/authoring_modeling.c
    src/mesh/authoring_uv.c
    src/terrain/terrain.c
    src/terrain/terrain_edit.c
    src/terrain/terrain_edit_history.c
    src/terrain/terrain_authority.c
    src/terrain/terrain_network.c
    src/terrain/terrain_server.c
    src/terrain/terrain_client.c
    src/terrain/terrain_replica.c
    src/terrain/terrain_lod.c
    src/terrain/terrain_collision.c
    src/terrain/terrain_collision_runtime.c
    src/terrain/terrain_physics.c
    src/terrain/terrain_prediction.c
    src/terrain/terrain_mesh.c
    src/terrain/terrain_storage.c
    src/terrain/terrain_streaming.c
)

if(HENKA_ENABLE_NETWORK)
    target_include_directories(henka_runtime PRIVATE ${enet_SOURCE_DIR}/include)
    target_sources(henka_runtime PRIVATE
        src/network/network.c
        src/network/network_codec.c
        src/network/network_enet.c
    )
    target_link_libraries(henka_runtime PRIVATE henka_enet)
endif()

if(HENKA_ENABLE_LUA)
    target_link_libraries(henka_runtime PRIVATE henka_lua)
endif()

target_include_directories(henka_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(henka_runtime PRIVATE HENKA_RUNTIME_HEADLESS=1)
target_compile_definitions(henka_runtime PRIVATE HENKA_ENABLE_LUA=$<BOOL:${HENKA_ENABLE_LUA}>)
target_link_libraries(henka_runtime PRIVATE henka_audio_decoder)
henka_enable_project_warnings(henka_runtime)

if(HENKA_BUILD_CLIENT)
    add_library(henka STATIC
        src/assets/assets.c
        src/core/action.c
        src/assets/model_obj.c
        src/assets/model_gltf.c
        src/core/engine.c
        src/core/gizmo.c
        src/core/workspace.c
        src/platform/platform_sdl.c
        src/platform/audio_output_sdl.c
        src/renderer/mesh.c
        src/renderer/mesh_validation.c
        src/renderer/terrain_render.c
        src/renderer/renderer.c
        src/renderer/temporal_camera_policy.c
        src/renderer/renderer_opengl.c
        src/renderer/shader.c
        src/renderer/texture.c
        $<$<BOOL:${HENKA_ENABLE_KTX2_TRANSCODER}>:src/renderer/ktx_boundary.c>
        src/scene/camera.c
        src/scene/scene.c
        src/ui/ui.c
        src/ui/ui_icons.c
    )

    target_include_directories(henka
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_LIST_DIR}/../third_party/stb
    )

    target_link_libraries(henka
        PUBLIC
            henka_runtime
    )

    if(TARGET SDL3::SDL3)
        set(HENKA_SDL_TARGET SDL3::SDL3)
    elseif(TARGET SDL3::SDL3-static)
        set(HENKA_SDL_TARGET SDL3::SDL3-static)
    else()
        message(FATAL_ERROR "SDL3 target was not created or supplied.")
    endif()

    target_link_libraries(henka
        PRIVATE
            ${HENKA_SDL_TARGET}
    )

    if(HENKA_ENABLE_KTX2_TRANSCODER)
        target_compile_definitions(henka PRIVATE HENKA_WITH_KTX2_TRANSCODER=1)
        target_link_libraries(henka PRIVATE ktx)
    endif()

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

    henka_enable_project_warnings(henka)
endif()
