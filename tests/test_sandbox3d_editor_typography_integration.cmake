cmake_minimum_required(VERSION 3.25)

foreach(required_var IN ITEMS SANDBOX3D_SOURCE_DIR TYPOGRAPHY_CHECKER TEST_ROOT)
    if(NOT DEFINED ${required_var})
        message(FATAL_ERROR "${required_var} must be supplied by the owning CTest")
    endif()
endforeach()
if(NOT IS_DIRECTORY "${SANDBOX3D_SOURCE_DIR}" OR
   NOT EXISTS "${TYPOGRAPHY_CHECKER}" OR
   NOT IS_DIRECTORY "${TEST_ROOT}")
    message(FATAL_ERROR "Typography integration test inputs must be existing repository-owned paths")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${SANDBOX3D_SOURCE_DIR}"
        "-P" "${TYPOGRAPHY_CHECKER}"
    RESULT_VARIABLE production_result
    OUTPUT_VARIABLE production_stdout
    ERROR_VARIABLE production_stderr)
if(NOT "${production_result}" STREQUAL "0")
    message(FATAL_ERROR
        "Production typography integration contract failed.\n"
        "stdout:\n${production_stdout}\n"
        "stderr:\n${production_stderr}")
endif()

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef probe_id)
set(probe_dir "${TEST_ROOT}/typography-integration-${probe_id}")
if(EXISTS "${probe_dir}")
    message(FATAL_ERROR "Refusing to reuse an existing typography integration probe: ${probe_dir}")
endif()
file(MAKE_DIRECTORY "${probe_dir}")
file(READ "${SANDBOX3D_SOURCE_DIR}/main.c" main_source)
set(candidate_main "${main_source}")
string(REPLACE
    "state->automation_diagnostic_terrain_utility_tabs_visible = false;\n        utility_navigation_row_count = 0U;"
    "state->automation_diagnostic_terrain_utility_tabs_visible = true;\n        utility_navigation_row_count = 0U;"
    candidate_main "${candidate_main}")
if(candidate_main STREQUAL main_source)
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Could not construct the Utility layout-reset negative control")
endif()
file(WRITE "${probe_dir}/main.c" "${candidate_main}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${SANDBOX3D_SOURCE_DIR}"
        "-DSANDBOX3D_MAIN_SOURCE=${probe_dir}/main.c"
        -P "${TYPOGRAPHY_CHECKER}"
    RESULT_VARIABLE utility_reset_negative_result
    OUTPUT_VARIABLE utility_reset_negative_stdout
    ERROR_VARIABLE utility_reset_negative_stderr)
set(utility_reset_negative_output "${utility_reset_negative_stdout}\n${utility_reset_negative_stderr}")
if("${utility_reset_negative_result}" STREQUAL "0")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Typography integration checker accepted stale Utility layout state")
endif()
if(NOT utility_reset_negative_output MATCHES "Utility tab visibility telemetry and measured geometry outputs must reset")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR
        "Utility layout-reset negative control failed for an unexpected reason.\n"
        "result=${utility_reset_negative_result}\noutput:\n${utility_reset_negative_output}")
endif()

set(candidate_main "${main_source}")
string(REPLACE
    "sandbox3d_editor_typography_runtime_initialize("
    "sandbox3d_editor_typography_runtime_startup_removed("
    candidate_main "${candidate_main}")
file(WRITE "${probe_dir}/main.c" "${candidate_main}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${SANDBOX3D_SOURCE_DIR}"
        "-DSANDBOX3D_MAIN_SOURCE=${probe_dir}/main.c"
        -P "${TYPOGRAPHY_CHECKER}"
    RESULT_VARIABLE negative_result
    OUTPUT_VARIABLE negative_stdout
    ERROR_VARIABLE negative_stderr)
set(negative_output "${negative_stdout}\n${negative_stderr}")

if("${negative_result}" STREQUAL "0")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Typography integration checker accepted removal of startup font installation")
endif()
if(NOT negative_output MATCHES "bundled font and global scale migration must publish before workspace restore")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR
        "Typography integration negative control failed for an unexpected reason.\n"
        "result=${negative_result}\noutput:\n${negative_output}")
endif()

string(REPLACE
    "sandbox3d_editor_typography_runtime_draw_scale_control("
    "sandbox3d_editor_typography_runtime_draw_scale_control_removed("
    candidate_main "${main_source}")
file(WRITE "${probe_dir}/main.c" "${candidate_main}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${SANDBOX3D_SOURCE_DIR}"
        "-DSANDBOX3D_MAIN_SOURCE=${probe_dir}/main.c"
        -P "${TYPOGRAPHY_CHECKER}"
    RESULT_VARIABLE scale_negative_result
    OUTPUT_VARIABLE scale_negative_stdout
    ERROR_VARIABLE scale_negative_stderr)
set(scale_negative_output "${scale_negative_stdout}\n${scale_negative_stderr}")
if("${scale_negative_result}" STREQUAL "0")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Typography integration checker accepted removal of the visible Settings scale control")
endif()
if(NOT scale_negative_output MATCHES "Settings scale control must remain reachable in the Settings header row")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR
        "Typography scale reachability negative control failed for an unexpected reason.\n"
        "result=${scale_negative_result}\noutput:\n${scale_negative_output}")
endif()

string(REGEX REPLACE
    "sandbox3d_editor_typography_runtime_get_layout_scale[ \t\r\n]*\\([ \t\r\n]*state->typography_runtime[ \t\r\n]*\\)"
    "1.0f"
    candidate_main "${main_source}")
file(WRITE "${probe_dir}/main.c" "${candidate_main}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${SANDBOX3D_SOURCE_DIR}"
        "-DSANDBOX3D_MAIN_SOURCE=${probe_dir}/main.c"
        -P "${TYPOGRAPHY_CHECKER}"
    RESULT_VARIABLE layout_scale_negative_result
    OUTPUT_VARIABLE layout_scale_negative_stdout
    ERROR_VARIABLE layout_scale_negative_stderr)
set(layout_scale_negative_output "${layout_scale_negative_stdout}\n${layout_scale_negative_stderr}")
if("${layout_scale_negative_result}" STREQUAL "0")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Typography integration checker accepted layout scale being replaced with 100%")
endif()
if(NOT layout_scale_negative_output MATCHES "the runtime preference must be resolved by the canonical content-size adapter")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR
        "Typography layout-scale negative control failed for an unexpected reason.\n"
        "result=${layout_scale_negative_result}\noutput:\n${layout_scale_negative_output}")
endif()

string(REPLACE
    "sandbox3d_editor_layout_content_viewport_to_framebuffer("
    "sandbox3d_editor_layout_content_viewport_to_framebuffer_removed("
    candidate_main "${main_source}")
file(WRITE "${probe_dir}/main.c" "${candidate_main}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${SANDBOX3D_SOURCE_DIR}"
        "-DSANDBOX3D_MAIN_SOURCE=${probe_dir}/main.c"
        -P "${TYPOGRAPHY_CHECKER}"
    RESULT_VARIABLE display_scale_negative_result
    OUTPUT_VARIABLE display_scale_negative_stdout
    ERROR_VARIABLE display_scale_negative_stderr)
set(display_scale_negative_output "${display_scale_negative_stdout}\n${display_scale_negative_stderr}")
if("${display_scale_negative_result}" STREQUAL "0")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Typography integration checker accepted removal of the content-to-framebuffer viewport conversion")
endif()
if(NOT display_scale_negative_output MATCHES "renderer scene viewport must convert the content-space layout using the canonical display-scale converter")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR
        "Typography display-scale negative control failed for an unexpected reason.\n"
        "result=${display_scale_negative_result}\noutput:\n${display_scale_negative_output}")
endif()

file(READ "${SANDBOX3D_SOURCE_DIR}/../../scripts/test_editor_typography_windows.ps1" typography_test_source)
string(REPLACE
    "os_display_scale_numerator = \$runtimeScales.DisplayScaleNumerator"
    "os_display_scale_numerator_removed = \$runtimeScales.DisplayScaleNumerator"
    candidate_typography_test_source "${typography_test_source}")
if(candidate_typography_test_source STREQUAL typography_test_source)
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Could not construct the OS display-scale capture negative control")
endif()
file(WRITE "${probe_dir}/test_editor_typography_windows.ps1" "${candidate_typography_test_source}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${SANDBOX3D_SOURCE_DIR}"
        "-DTYPOGRAPHY_PACKAGE_TEST_SOURCE=${probe_dir}/test_editor_typography_windows.ps1"
        -P "${TYPOGRAPHY_CHECKER}"
    RESULT_VARIABLE capture_scale_negative_result
    OUTPUT_VARIABLE capture_scale_negative_stdout
    ERROR_VARIABLE capture_scale_negative_stderr)
set(capture_scale_negative_output "${capture_scale_negative_stdout}\n${capture_scale_negative_stderr}")
if("${capture_scale_negative_result}" STREQUAL "0")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Typography integration checker accepted capture evidence with its OS display-scale field removed")
endif()
if(NOT capture_scale_negative_output MATCHES "each packaged capture must record product-reported OS display scale")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR
        "Typography capture display-scale negative control failed for an unexpected reason.\n"
        "result=${capture_scale_negative_result}\noutput:\n${capture_scale_negative_output}")
endif()

file(READ "${SANDBOX3D_SOURCE_DIR}/../../scripts/test_editor_typography_windows.ps1"
    typography_package_source)
string(REPLACE
    [=[commit_sha = [string]$validationHarnessIdentity.commit_sha]=]
    [=[reported_commit = 'missing']=]
    candidate_typography_package_source "${typography_package_source}")
if(candidate_typography_package_source STREQUAL typography_package_source)
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Could not construct the validation-harness provenance negative control")
endif()
file(WRITE "${probe_dir}/test_editor_typography_windows.ps1"
    "${candidate_typography_package_source}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${SANDBOX3D_SOURCE_DIR}"
        "-DTYPOGRAPHY_PACKAGE_TEST_SOURCE=${probe_dir}/test_editor_typography_windows.ps1"
        -P "${TYPOGRAPHY_CHECKER}"
    RESULT_VARIABLE harness_provenance_negative_result
    OUTPUT_VARIABLE harness_provenance_negative_stdout
    ERROR_VARIABLE harness_provenance_negative_stderr)
set(harness_provenance_negative_output "${harness_provenance_negative_stdout}\n${harness_provenance_negative_stderr}")
if("${harness_provenance_negative_result}" STREQUAL "0")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Typography integration checker accepted missing validation-harness commit provenance")
endif()
if(NOT harness_provenance_negative_output MATCHES
    "packaged typography evidence must bind the exact clean validation-harness commit and source identity")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR
        "Typography validation-harness provenance negative control failed for an unexpected reason.\n"
        "result=${harness_provenance_negative_result}\noutput:\n${harness_provenance_negative_output}")
endif()

file(READ "${SANDBOX3D_SOURCE_DIR}/../../scripts/henka_ui_automation_helpers.ps1" ui_automation_helper_source)
string(REPLACE
    "function Convert-HenkaUiLogicalPointToWindowPoint"
    "function Convert-HenkaUiLogicalPointToFramebufferPoint"
    candidate_ui_automation_helper_source "${ui_automation_helper_source}")
if(candidate_ui_automation_helper_source STREQUAL ui_automation_helper_source)
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Could not construct the non-100% UI automation coordinate-transform negative control")
endif()
file(WRITE "${probe_dir}/henka_ui_automation_helpers.ps1" "${candidate_ui_automation_helper_source}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${SANDBOX3D_SOURCE_DIR}"
        "-DHENKA_UI_AUTOMATION_HELPERS_SOURCE=${probe_dir}/henka_ui_automation_helpers.ps1"
        -P "${TYPOGRAPHY_CHECKER}"
    RESULT_VARIABLE ui_input_scale_negative_result
    OUTPUT_VARIABLE ui_input_scale_negative_stdout
    ERROR_VARIABLE ui_input_scale_negative_stderr)
set(ui_input_scale_negative_output "${ui_input_scale_negative_stdout}\n${ui_input_scale_negative_stderr}")
if("${ui_input_scale_negative_result}" STREQUAL "0")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Typography integration checker accepted removal of the non-100% UI input-coordinate transform")
endif()
if(NOT ui_input_scale_negative_output MATCHES "typography package interactions must use the canonical logical-to-window input helper")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR
        "Typography UI input-scale negative control failed for an unexpected reason.\n"
        "result=${ui_input_scale_negative_result}\noutput:\n${ui_input_scale_negative_output}")
endif()

file(READ "${SANDBOX3D_SOURCE_DIR}/editor_typography_runtime.c" runtime_source)
string(REPLACE
    "editor_typography_context stage=attached"
    "editor_typography_context stage=removed"
    candidate_runtime_source "${runtime_source}")
if(candidate_runtime_source STREQUAL runtime_source)
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Could not construct the detached-font-context telemetry negative control")
endif()
file(WRITE "${probe_dir}/editor_typography_runtime.c" "${candidate_runtime_source}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DSANDBOX3D_SOURCE_DIR=${SANDBOX3D_SOURCE_DIR}"
        "-DTYPOGRAPHY_RUNTIME_SOURCE=${probe_dir}/editor_typography_runtime.c"
        -P "${TYPOGRAPHY_CHECKER}"
    RESULT_VARIABLE detached_context_negative_result
    OUTPUT_VARIABLE detached_context_negative_stdout
    ERROR_VARIABLE detached_context_negative_stderr)
set(detached_context_negative_output "${detached_context_negative_stdout}\n${detached_context_negative_stderr}")
if("${detached_context_negative_result}" STREQUAL "0")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR "Typography integration checker accepted missing detached-context telemetry")
endif()
if(NOT detached_context_negative_output MATCHES "packaged typography must prove a real detached-font context and completed render")
    file(REMOVE_RECURSE "${probe_dir}")
    message(FATAL_ERROR
        "Typography detached-context negative control failed for an unexpected reason.\n"
        "result=${detached_context_negative_result}\noutput:\n${detached_context_negative_output}")
endif()

file(REMOVE_RECURSE "${probe_dir}")

message(STATUS "Sandbox3D typography integration passes and rejects missing startup, Settings scale control, runtime layout-scale use, display-scale viewport conversion, capture provenance, scale-aware typography input conversion, or detached-context render proof")
