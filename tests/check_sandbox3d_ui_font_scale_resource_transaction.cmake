cmake_minimum_required(VERSION 3.25)

foreach(required_var IN ITEMS ENGINE_SOURCE_DIR ENGINE_INCLUDE_DIR SANDBOX3D_SOURCE_DIR)
    if(NOT DEFINED ${required_var} OR NOT IS_DIRECTORY "${${required_var}}")
        message(FATAL_ERROR "${required_var} must name an existing Henka source directory")
    endif()
endforeach()

set(engine_api "${ENGINE_INCLUDE_DIR}/henka/engine.h")
set(engine_source "${ENGINE_SOURCE_DIR}/core/engine.c")
set(renderer_source "${ENGINE_SOURCE_DIR}/renderer/renderer.c")
set(opengl_source "${ENGINE_SOURCE_DIR}/renderer/renderer_opengl.c")
set(scale_source "${SANDBOX3D_SOURCE_DIR}/editor_scale_transaction.c")

foreach(required_file IN ITEMS engine_api engine_source renderer_source opengl_source scale_source)
    if(NOT EXISTS "${${required_file}}")
        message(FATAL_ERROR "Required UI-font resource-transaction owner is missing: ${${required_file}}")
    endif()
    file(READ "${${required_file}}" ${required_file}_text)
endforeach()

set(violations "")
foreach(contract IN ITEMS
        "henka_engine_ui_font_resource_update_prepare"
        "henka_engine_ui_font_resource_update_publish"
        "henka_engine_ui_font_resource_update_discard")
    if(NOT engine_api_text MATCHES "${contract}")
        list(APPEND violations "public engine transaction contract lacks ${contract}")
    endif()
endforeach()

if(NOT engine_source_text MATCHES "henka_renderer_ui_font_resource_update_prepare" OR
   NOT engine_source_text MATCHES "engine->active_ui" OR
   NOT engine_source_text MATCHES "engine->tool_windows")
    list(APPEND violations "engine must prepare the main and every live tool-window UI resource through the renderer")
endif()

foreach(contract IN ITEMS
        "henka_renderer_ui_font_resource_update_prepare"
        "henka_renderer_ui_font_resource_update_publish"
        "henka_renderer_ui_font_resource_update_discard")
    if(NOT renderer_source_text MATCHES "${contract}")
        list(APPEND violations "renderer facade lacks ${contract}")
    endif()
endforeach()

foreach(contract IN ITEMS
        "henka_opengl_renderer_ui_font_resource_update_prepare"
        "henka_opengl_renderer_ui_font_resource_update_publish"
        "henka_opengl_renderer_ui_font_resource_update_discard")
    if(NOT opengl_source_text MATCHES "${contract}")
        list(APPEND violations "OpenGL resource owner lacks ${contract}")
    endif()
endforeach()

if(NOT scale_source_text MATCHES "henka_engine_ui_font_resource_update_prepare" OR
   NOT scale_source_text MATCHES "henka_engine_ui_font_resource_update_publish" OR
   NOT scale_source_text MATCHES "henka_engine_ui_font_resource_update_discard")
    list(APPEND violations "editor scale candidate must prepare, publish, and discard renderer-owned font resources in the same scale transaction")
endif()

if(violations)
    list(JOIN violations "\n  " formatted_violations)
    message(FATAL_ERROR
        "UI scale cannot persist CPU metrics before GPU font atlas readiness:\n  ${formatted_violations}")
endif()

message(STATUS "Editor scale transaction includes renderer-owned resources for every live UI context")
