cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SANDBOX3D_SOURCE_DIR OR NOT IS_DIRECTORY "${SANDBOX3D_SOURCE_DIR}")
    message(FATAL_ERROR "SANDBOX3D_SOURCE_DIR must name the production Sandbox3D source directory")
endif()
if(NOT DEFINED TEXT_AUTHORITY_CHECKER OR NOT EXISTS "${TEXT_AUTHORITY_CHECKER}")
    message(FATAL_ERROR "TEXT_AUTHORITY_CHECKER must name the production-caller checker")
endif()
if(NOT DEFINED UI_TEXT_LAYOUT_SOURCE OR NOT EXISTS "${UI_TEXT_LAYOUT_SOURCE}")
    message(FATAL_ERROR "UI_TEXT_LAYOUT_SOURCE must name the canonical UI text-layout implementation")
endif()
if(NOT DEFINED NEGATIVE_CONTROL_ROOT OR NOT IS_DIRECTORY "${NEGATIVE_CONTROL_ROOT}")
    message(FATAL_ERROR "NEGATIVE_CONTROL_ROOT must name an existing generated test directory")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${SANDBOX3D_SOURCE_DIR}"
        "-DUI_TEXT_LAYOUT_SOURCE=${UI_TEXT_LAYOUT_SOURCE}"
        -P "${TEXT_AUTHORITY_CHECKER}"
    RESULT_VARIABLE production_result
    OUTPUT_VARIABLE production_stdout
    ERROR_VARIABLE production_stderr)
if(NOT "${production_result}" STREQUAL "0")
    message(FATAL_ERROR
        "Production Sandbox3D text authority check failed.\n"
        "stdout:\n${production_stdout}\n"
        "stderr:\n${production_stderr}")
endif()

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef negative_id)
set(negative_dir "${NEGATIVE_CONTROL_ROOT}/textauth-${negative_id}")
if(EXISTS "${negative_dir}")
    message(FATAL_ERROR "Refusing to reuse an existing text-authority test directory: ${negative_dir}")
endif()
file(MAKE_DIRECTORY "${negative_dir}")
file(WRITE "${negative_dir}/negative_control.c"
    "void reintroduced_legacy_metric(void) { (void)henka_ui_measure_text(\"label\", 1.0f, 0, 0); }\n")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${negative_dir}"
        "-DUI_TEXT_LAYOUT_SOURCE=${UI_TEXT_LAYOUT_SOURCE}"
        "-DEDITOR_LAYOUT_SOURCE=${SANDBOX3D_SOURCE_DIR}/editor_layout.c"
        "-DEDITOR_LAYOUT_HEADER=${SANDBOX3D_SOURCE_DIR}/editor_layout.h"
        "-DSANDBOX3D_CMAKE_SOURCE=${SANDBOX3D_SOURCE_DIR}/CMakeLists.txt"
        -P "${TEXT_AUTHORITY_CHECKER}"
    RESULT_VARIABLE negative_result
    OUTPUT_VARIABLE negative_stdout
    ERROR_VARIABLE negative_stderr)
set(negative_output "${negative_stdout}\n${negative_stderr}")
file(REMOVE_RECURSE "${negative_dir}")

if("${negative_result}" STREQUAL "0")
    message(FATAL_ERROR "Text-authority checker accepted an injected context-free measurement call")
endif()
if(NOT negative_output MATCHES "context-free henka_ui_measure_text")
    message(FATAL_ERROR
        "Text-authority negative control failed for an unexpected reason.\n"
        "result=${negative_result}\noutput:\n${negative_output}")
endif()

set(private_negative_dir "${NEGATIVE_CONTROL_ROOT}/textauth-${negative_id}-private")
if(EXISTS "${private_negative_dir}")
    message(FATAL_ERROR "Refusing to reuse an existing private-include test directory: ${private_negative_dir}")
endif()
file(MAKE_DIRECTORY "${private_negative_dir}")
file(WRITE "${private_negative_dir}/negative_control.c"
    "void private_include_negative_control(void) {}\n")
file(WRITE "${private_negative_dir}/negative_control.h"
    "#include \"../../engine/src/ui/ui_font_internal.h\"\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${private_negative_dir}"
        "-DUI_TEXT_LAYOUT_SOURCE=${UI_TEXT_LAYOUT_SOURCE}"
        "-DEDITOR_LAYOUT_SOURCE=${SANDBOX3D_SOURCE_DIR}/editor_layout.c"
        "-DEDITOR_LAYOUT_HEADER=${SANDBOX3D_SOURCE_DIR}/editor_layout.h"
        "-DSANDBOX3D_CMAKE_SOURCE=${SANDBOX3D_SOURCE_DIR}/CMakeLists.txt"
        -P "${TEXT_AUTHORITY_CHECKER}"
    RESULT_VARIABLE private_negative_result
    OUTPUT_VARIABLE private_negative_stdout
    ERROR_VARIABLE private_negative_stderr)
set(private_negative_output "${private_negative_stdout}\n${private_negative_stderr}")
file(REMOVE_RECURSE "${private_negative_dir}")
if("${private_negative_result}" STREQUAL "0")
    message(FATAL_ERROR "Text-authority checker accepted a Sandbox include into private engine/src")
endif()
if(NOT private_negative_output MATCHES "private engine source include")
    message(FATAL_ERROR
        "Private-engine-include negative control failed for an unexpected reason.\n"
        "result=${private_negative_result}\noutput:\n${private_negative_output}")
endif()

set(negative_metrics_dir "${negative_dir}-metrics")
if(EXISTS "${negative_metrics_dir}")
    message(FATAL_ERROR "Refusing to reuse an existing UI-metrics test directory: ${negative_metrics_dir}")
endif()
file(MAKE_DIRECTORY "${negative_metrics_dir}")
file(READ "${UI_TEXT_LAYOUT_SOURCE}" ui_text_layout_source)
file(WRITE "${negative_metrics_dir}/ui_text_layout.c"
    "${ui_text_layout_source}\n/* negative control: reintroduced second decoder traversal */\n"
    "static void injected_duplicate_metric_walk(void) { (void)henka_ui_text_decode_utf8(0, 0U); }\n")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${SANDBOX3D_SOURCE_DIR}"
        "-DUI_TEXT_LAYOUT_SOURCE=${negative_metrics_dir}/ui_text_layout.c"
        "-DEDITOR_LAYOUT_SOURCE=${SANDBOX3D_SOURCE_DIR}/editor_layout.c"
        "-DEDITOR_LAYOUT_HEADER=${SANDBOX3D_SOURCE_DIR}/editor_layout.h"
        "-DSANDBOX3D_CMAKE_SOURCE=${SANDBOX3D_SOURCE_DIR}/CMakeLists.txt"
        -P "${TEXT_AUTHORITY_CHECKER}"
    RESULT_VARIABLE metrics_negative_result
    OUTPUT_VARIABLE metrics_negative_stdout
    ERROR_VARIABLE metrics_negative_stderr)
set(metrics_negative_output "${metrics_negative_stdout}\n${metrics_negative_stderr}")
file(REMOVE_RECURSE "${negative_metrics_dir}")

if("${metrics_negative_result}" STREQUAL "0")
    file(REMOVE_RECURSE "${negative_dir}")
    message(FATAL_ERROR "Text-authority checker accepted a duplicated UI font-metrics traversal")
endif()
if(NOT metrics_negative_output MATCHES "UI measurement and glyph placement must share")
    file(REMOVE_RECURSE "${negative_dir}")
    message(FATAL_ERROR
        "UI-metrics negative control failed for an unexpected reason.\n"
        "result=${metrics_negative_result}\noutput:\n${metrics_negative_output}")
endif()
file(REMOVE_RECURSE "${negative_dir}")

set(grid_negative_dir "${negative_dir}-grid")
if(EXISTS "${grid_negative_dir}")
    message(FATAL_ERROR "Refusing to reuse an existing script-editor layout test directory: ${grid_negative_dir}")
endif()
file(MAKE_DIRECTORY "${grid_negative_dir}")
file(READ "${SANDBOX3D_SOURCE_DIR}/script_editor.c" script_editor_source)
file(WRITE "${grid_negative_dir}/script_editor.c"
    "${script_editor_source}\nstatic float injected_fixed_cell(size_t column, float character_width) { return (float)column * character_width; }\n")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${grid_negative_dir}"
        "-DUI_TEXT_LAYOUT_SOURCE=${UI_TEXT_LAYOUT_SOURCE}"
        "-DEDITOR_LAYOUT_SOURCE=${SANDBOX3D_SOURCE_DIR}/editor_layout.c"
        "-DEDITOR_LAYOUT_HEADER=${SANDBOX3D_SOURCE_DIR}/editor_layout.h"
        "-DSANDBOX3D_CMAKE_SOURCE=${SANDBOX3D_SOURCE_DIR}/CMakeLists.txt"
        -P "${TEXT_AUTHORITY_CHECKER}"
    RESULT_VARIABLE grid_negative_result
    OUTPUT_VARIABLE grid_negative_stdout
    ERROR_VARIABLE grid_negative_stderr)
set(grid_negative_output "${grid_negative_stdout}\n${grid_negative_stderr}")
file(REMOVE_RECURSE "${grid_negative_dir}")

if("${grid_negative_result}" STREQUAL "0")
    message(FATAL_ERROR "Text-authority checker accepted fixed-cell Script Editor positioning")
endif()
if(NOT grid_negative_output MATCHES "fixed-cell character-width positioning")
    message(FATAL_ERROR
        "Script Editor layout negative control failed for an unexpected reason.\n"
        "result=${grid_negative_result}\noutput:\n${grid_negative_output}")
endif()
file(REMOVE_RECURSE "${negative_dir}")

set(legacy_negative_dir "${negative_dir}-legacy")
if(EXISTS "${legacy_negative_dir}")
    message(FATAL_ERROR "Refusing to reuse an existing legacy-layout test directory: ${legacy_negative_dir}")
endif()
file(MAKE_DIRECTORY "${legacy_negative_dir}")
file(READ "${SANDBOX3D_SOURCE_DIR}/editor_layout.c" editor_layout_source)
string(REPLACE "#ifdef HENKA_SANDBOX3D_TEST_SEAM" "" unguarded_editor_layout "${editor_layout_source}")
string(REPLACE "#endif /* HENKA_SANDBOX3D_TEST_SEAM */" "" unguarded_editor_layout "${unguarded_editor_layout}")
file(WRITE "${legacy_negative_dir}/editor_layout.c" "${unguarded_editor_layout}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${legacy_negative_dir}"
        "-DEDITOR_LAYOUT_SOURCE=${legacy_negative_dir}/editor_layout.c"
        "-DUI_TEXT_LAYOUT_SOURCE=${UI_TEXT_LAYOUT_SOURCE}"
        "-DEDITOR_LAYOUT_HEADER=${SANDBOX3D_SOURCE_DIR}/editor_layout.h"
        "-DSANDBOX3D_CMAKE_SOURCE=${SANDBOX3D_SOURCE_DIR}/CMakeLists.txt"
        -P "${TEXT_AUTHORITY_CHECKER}"
    RESULT_VARIABLE legacy_negative_result
    OUTPUT_VARIABLE legacy_negative_stdout
    ERROR_VARIABLE legacy_negative_stderr)
set(legacy_negative_output "${legacy_negative_stdout}\n${legacy_negative_stderr}")
file(REMOVE_RECURSE "${legacy_negative_dir}")

if("${legacy_negative_result}" STREQUAL "0")
    file(REMOVE_RECURSE "${negative_dir}")
    message(FATAL_ERROR "Text-authority checker accepted unguarded fixed-column regression helpers")
endif()
if(NOT legacy_negative_output MATCHES "Retired fixed-column wrapping/hidden-row helpers must remain test-only")
    file(REMOVE_RECURSE "${negative_dir}")
    message(FATAL_ERROR
        "Test-only legacy-layout negative control failed for an unexpected reason.\n"
        "result=${legacy_negative_result}\noutput:\n${legacy_negative_output}")
endif()
file(REMOVE_RECURSE "${negative_dir}")

message(STATUS "Sandbox3D text authority passes and rejects legacy callers, duplicate metrics, fixed-cell code layout, and product exposure of legacy helpers")
