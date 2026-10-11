cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SANDBOX3D_SOURCE_DIR OR NOT IS_DIRECTORY "${SANDBOX3D_SOURCE_DIR}")
    message(FATAL_ERROR "SANDBOX3D_SOURCE_DIR must name the production Sandbox3D source directory")
endif()
if(NOT DEFINED UI_TEXT_LAYOUT_SOURCE OR NOT EXISTS "${UI_TEXT_LAYOUT_SOURCE}")
    message(FATAL_ERROR "UI_TEXT_LAYOUT_SOURCE must name the canonical UI text-layout implementation")
endif()
set(editor_layout_source_path "${SANDBOX3D_SOURCE_DIR}/editor_layout.c")
set(editor_layout_header_path "${SANDBOX3D_SOURCE_DIR}/editor_layout.h")
set(sandbox3d_cmake_path "${SANDBOX3D_SOURCE_DIR}/CMakeLists.txt")
if(DEFINED EDITOR_LAYOUT_SOURCE)
    set(editor_layout_source_path "${EDITOR_LAYOUT_SOURCE}")
endif()
if(DEFINED EDITOR_LAYOUT_HEADER)
    set(editor_layout_header_path "${EDITOR_LAYOUT_HEADER}")
endif()
if(DEFINED SANDBOX3D_CMAKE_SOURCE)
    set(sandbox3d_cmake_path "${SANDBOX3D_CMAKE_SOURCE}")
endif()
if(NOT EXISTS "${editor_layout_source_path}" OR NOT EXISTS "${editor_layout_header_path}")
    message(FATAL_ERROR "The canonical Sandbox3D editor-layout source/header must exist")
endif()
if(NOT EXISTS "${sandbox3d_cmake_path}")
    message(FATAL_ERROR "The canonical Sandbox3D CMake source must exist")
endif()

file(GLOB sandbox3d_sources "${SANDBOX3D_SOURCE_DIR}/*.c")
if(NOT sandbox3d_sources)
    message(FATAL_ERROR "No Sandbox3D production C sources found under ${SANDBOX3D_SOURCE_DIR}")
endif()

set(violations "")
foreach(source_path IN LISTS sandbox3d_sources)
    file(READ "${source_path}" source_text)
    # Comments do not represent production call sites. Keep string literals
    # intact so a prohibited call embedded in a macro/string is still visible.
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" " " source_text "${source_text}")
    string(REGEX REPLACE "//[^\r\n]*" " " source_text "${source_text}")

    if(source_text MATCHES "henka_ui_measure_text[ \t\r\n]*\\(")
        list(APPEND violations "${source_path}: context-free henka_ui_measure_text()")
    endif()

    if(source_path MATCHES "[/\\\\]main\\.c$")
        foreach(legacy_helper IN ITEMS
                sandbox3d_editor_layout_text_control_row
                sandbox3d_editor_layout_measure_wrapped_text
                sandbox3d_editor_layout_wrap_text
                sandbox3d_editor_layout_limit_wrapped_text
                sandbox3d_editor_layout_format_hidden_row_label
                sandbox3d_truncate_text)
            if(source_text MATCHES "${legacy_helper}[ \t\r\n]*\\(")
                list(APPEND violations "${source_path}: legacy character-column helper ${legacy_helper}()")
            endif()
        endforeach()
        if(source_text MATCHES "glyph_advance|character_advance|columns_for_width")
            list(APPEND violations "${source_path}: fixed-cell glyph/column width authority")
        endif()
    endif()

    if(source_path MATCHES "[/\\\\]script_editor\\.c$")
        if(source_text MATCHES "character_width|\\*[^;\r\n]*character_width|character_width[^;\r\n]*\\*")
            list(APPEND violations "${source_path}: fixed-cell character-width positioning")
        elseif(NOT source_text MATCHES "henka_ui_measure_text_for_context[ \t\r\n]*\\(")
            list(APPEND violations "${source_path}: code spans/caret do not use active UI font metrics")
        endif()
    endif()
endforeach()

file(READ "${editor_layout_source_path}" editor_layout_source)
file(READ "${editor_layout_header_path}" editor_layout_header)
file(READ "${sandbox3d_cmake_path}" sandbox3d_cmake)
string(FIND "${editor_layout_source}" "#ifdef HENKA_SANDBOX3D_TEST_SEAM" source_guard_start)
string(FIND "${editor_layout_source}" "static henka_result sandbox3d_editor_layout_process_wrapped_text(" source_helpers_start)
string(FIND "${editor_layout_source}" "#endif /* HENKA_SANDBOX3D_TEST_SEAM */" source_guard_end)
string(FIND "${editor_layout_source}" "henka_result sandbox3d_editor_layout_text_row_height(" source_product_helpers)
string(FIND "${editor_layout_header}" "#ifdef HENKA_SANDBOX3D_TEST_SEAM" header_guard_start)
string(FIND "${editor_layout_header}" "sandbox3d_editor_layout_measure_wrapped_text(" header_helpers_start)
string(FIND "${editor_layout_header}" "#endif /* HENKA_SANDBOX3D_TEST_SEAM */" header_guard_end)
string(FIND "${editor_layout_header}" "sandbox3d_editor_layout_text_row_height(" header_product_helpers)
if(source_guard_start LESS 0 OR source_helpers_start LESS source_guard_start OR
   source_guard_end LESS source_helpers_start OR source_product_helpers LESS source_guard_end OR
   header_guard_start LESS 0 OR header_helpers_start LESS header_guard_start OR
   header_guard_end LESS header_helpers_start OR header_product_helpers LESS header_guard_end OR
   sandbox3d_cmake MATCHES "HENKA_SANDBOX3D_TEST_SEAM")
    message(FATAL_ERROR
        "Retired fixed-column wrapping/hidden-row helpers must remain test-only; "
        "product layout must use active font metrics")
endif()

file(READ "${UI_TEXT_LAYOUT_SOURCE}" ui_text_layout_source)
string(REGEX MATCHALL "henka_ui_text_decode_utf8[ \t\r\n]*\\(" utf8_decoder_references "${ui_text_layout_source}")
list(LENGTH utf8_decoder_references utf8_decoder_reference_count)
if(NOT utf8_decoder_reference_count EQUAL 2)
    message(FATAL_ERROR
        "UI measurement and glyph placement must share one UTF-8/font-metrics traversal; "
        "expected one decoder declaration plus one traversal, found ${utf8_decoder_reference_count} references")
endif()

if(violations)
    list(JOIN violations "\n  " formatted_violations)
    message(FATAL_ERROR
        "Sandbox3D product layout must use the active UI font measurement authority.\n  ${formatted_violations}")
endif()

message(STATUS "Sandbox3D production text measurement uses the active UI authority")
