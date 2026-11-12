cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED SANDBOX3D_SOURCE_DIR OR NOT IS_DIRECTORY "${SANDBOX3D_SOURCE_DIR}")
    message(FATAL_ERROR "SANDBOX3D_SOURCE_DIR must name the Sandbox3D source directory")
endif()

if(NOT DEFINED SANDBOX3D_MAIN_SOURCE)
    set(SANDBOX3D_MAIN_SOURCE "${SANDBOX3D_SOURCE_DIR}/main.c")
endif()
if(NOT EXISTS "${SANDBOX3D_MAIN_SOURCE}")
    message(FATAL_ERROR "Sandbox3D main source is missing: ${SANDBOX3D_MAIN_SOURCE}")
endif()

file(READ "${SANDBOX3D_MAIN_SOURCE}" main_source)
file(READ "${SANDBOX3D_SOURCE_DIR}/editor_typography_runtime.c" runtime_source)
file(READ "${SANDBOX3D_SOURCE_DIR}/editor_font_bootstrap.c" bootstrap_source)
file(READ "${SANDBOX3D_SOURCE_DIR}/editor_font_bootstrap.h" bootstrap_header)
file(READ "${SANDBOX3D_SOURCE_DIR}/../../scripts/test_editor_typography_windows.ps1" typography_package_test_source)
file(READ "${SANDBOX3D_SOURCE_DIR}/../../scripts/henka_ui_automation_helpers.ps1" ui_automation_helper_source)
set(violations "")

if(NOT main_source MATCHES "editor_typography_runtime.h")
    list(APPEND violations "Sandbox3D main must use the focused typography runtime owner")
endif()

string(FIND "${main_source}" "sandbox3d_editor_typography_runtime_initialize(" typography_startup_position)
string(FIND "${main_source}" "sandbox3d_apply_loaded_settings(engine, state)" workspace_restore_position)
if(typography_startup_position LESS 0 OR workspace_restore_position LESS 0 OR
   typography_startup_position GREATER workspace_restore_position)
    list(APPEND violations "the bundled font and global scale migration must publish before workspace restore")
endif()

foreach(contract IN ITEMS
        "sandbox3d_editor_typography_runtime_attach_context("
        "sandbox3d_editor_typography_runtime_detach_context("
        "sandbox3d_editor_typography_runtime_destroy(")
    string(FIND "${main_source}" "${contract}" contract_position)
    if(contract_position LESS 0)
        list(APPEND violations "Sandbox3D application lifecycle is missing ${contract}")
    endif()
endforeach()

if(NOT main_source MATCHES "henka_engine_get_window_content_size\\(")
    list(APPEND violations "workspace geometry must use the platform's content-space window dimensions")
endif()
if(NOT main_source MATCHES "henka_engine_get_window_display_scale\\(")
    list(APPEND violations "renderer viewport conversion must consume the platform's explicit display scale")
endif()
if(NOT main_source MATCHES "sandbox3d_editor_layout_content_viewport_to_framebuffer\\(")
    list(APPEND violations "renderer scene viewport must convert the content-space layout using the canonical display-scale converter")
endif()

if(main_source MATCHES "sandbox3d_get_workspace_layout[ \t\r\n]*\\([ \t\r\n]*state,[ \t\r\n]*framebuffer_width[ \t\r\n]*,[ \t\r\n]*framebuffer_height")
    list(APPEND violations "workspace geometry must be built from content dimensions, not framebuffer pixels")
endif()

string(REGEX MATCHALL
    "sandbox3d_editor_typography_runtime_get_layout_scale[ \t\r\n]*\\([ \t\r\n]*state->typography_runtime[ \t\r\n]*\\)"
    workspace_scale_reads "${main_source}")
list(LENGTH workspace_scale_reads workspace_scale_read_count)
string(REGEX MATCHALL
    "sandbox3d_workspace_set_ui_scale[ \t\r\n]*\\([ \t\r\n]*&state->workspace.model,[ \t\r\n]*\\(float\\)app_scale_percent"
    workspace_scale_projections "${main_source}")
list(LENGTH workspace_scale_projections workspace_scale_projection_count)
if(NOT workspace_scale_read_count EQUAL 1 OR
   NOT workspace_scale_projection_count EQUAL 2)
    list(APPEND violations "the runtime preference must be resolved by the canonical content-size adapter and projected into both normal and fallback workspace layouts")
endif()

string(FIND "${main_source}" "switch (state->workspace.active_utility)" utility_switch_start)
if(utility_switch_start LESS 0)
    list(APPEND violations "Sandbox3D Utility draw switch is missing")
else()
    string(LENGTH "${main_source}" main_source_length)
    math(EXPR utility_source_length "${main_source_length} - ${utility_switch_start}")
    string(SUBSTRING "${main_source}" ${utility_switch_start} ${utility_source_length} utility_source)
    string(FIND "${utility_source}" "case SANDBOX3D_UTILITY_SETTINGS:" settings_start)
    string(FIND "${utility_source}" "case SANDBOX3D_UTILITY_DIAGNOSTICS:" settings_end)
endif()
if(utility_switch_start LESS 0 OR settings_start LESS 0 OR settings_end LESS 0 OR settings_end LESS settings_start)
    list(APPEND violations "Sandbox3D Settings utility body could not be isolated for the typography reachability contract")
else()
    math(EXPR settings_length "${settings_end} - ${settings_start}")
    string(SUBSTRING "${utility_source}" ${settings_start} ${settings_length} settings_source)
    string(FIND "${settings_source}" "sandbox3d_editor_typography_runtime_draw_scale_control(" scale_control_position)
    string(FIND "${settings_source}" "y_start += 12.0f" settings_body_position)
    if(scale_control_position LESS 0 OR settings_body_position LESS 0 OR
       scale_control_position GREATER settings_body_position OR
       settings_source MATCHES "y_start[ \t\r\n]*\\+[ \t\r\n]*552\\.0f")
        list(APPEND violations "Settings scale control must remain reachable in the Settings header row before its content body")
    endif()
endif()

if(NOT main_source MATCHES "sandbox3d_editor_layout_text_control_grid_for_context\\(" OR
   NOT main_source MATCHES "utility_navigation_row_count")
    list(APPEND violations "Utility navigation must use the canonical measured wrapping layout and derive content placement from its actual row count")
endif()
if(NOT main_source MATCHES "state->automation_diagnostic_terrain_utility_tabs_visible = false;[ \t\r\n]*utility_navigation_row_count = 0U;[ \t\r\n]*navigation_result =")
    list(APPEND violations "Utility tab visibility telemetry must reset before each measured layout attempt so failed layout cannot reuse stale visibility")
endif()
if(NOT runtime_source MATCHES "sandbox3d_editor_scale_control_layout\\(" OR
   NOT runtime_source MATCHES "layout\\.choices" OR
   NOT runtime_source MATCHES "choices_x=" OR
   NOT typography_package_test_source MATCHES "choices_x=" OR
   NOT typography_package_test_source MATCHES "choices_y=")
    list(APPEND violations "the Editor UI Scale control must use shared inline-or-stacked geometry so narrow Utility docks retain reachable full-size choices")
endif()

if(NOT bootstrap_header MATCHES "sandbox3d_editor_font_candidate_resolve" OR
   NOT bootstrap_header MATCHES "sandbox3d_editor_font_candidate_get_asset" OR
   NOT bootstrap_source MATCHES "AtkinsonHyperlegible-Regular\\.ttf" OR
   NOT bootstrap_source MATCHES "NotoSans-Regular\\.ttf" OR
   NOT bootstrap_source MATCHES "7FB917C89019896D0B52EE84B7CBB3304C18CB90B19A62F5E32712BD23E97669" OR
   NOT bootstrap_source MATCHES "478C558EA716033CD60C03438F628DFA75694DCF6B5F6D505A2F05FD2B4F3823")
    list(APPEND violations "the bundled Atkinson and Noto candidates must have one bounded descriptor authority with pinned identities and hashes")
endif()
if(NOT runtime_source MATCHES "HENKA_AUTOMATION_EDITOR_FONT" OR
   NOT runtime_source MATCHES "HENKA_AUTOMATION_INPUT_OWNED" OR
   NOT runtime_source MATCHES "HENKA_AUTOMATION_DIAGNOSTICS" OR
   NOT runtime_source MATCHES "font_identity=" OR
   NOT runtime_source MATCHES "font_sha256=" OR
   NOT runtime_source MATCHES "ui_scale_percent=")
    list(APPEND violations "packaged typography comparison must select only a whitelisted automation-owned candidate and report the verified font and active scale")
endif()

if(NOT main_source MATCHES "utility_settings" OR
   NOT main_source MATCHES "utility_settings_rect" OR
   NOT main_source MATCHES "HENKA_AUTOMATION_DIAGNOSTIC utility_tab")
    list(APPEND violations "the packaged typography path must expose the real Settings tab geometry and active state only through bounded automation telemetry")
endif()
if(NOT main_source MATCHES "HENKA_AUTOMATION_DIAGNOSTIC controls_page_rect id=camera_status" OR
   NOT main_source MATCHES "HENKA_AUTOMATION_DIAGNOSTIC controls_page id=camera_status selected=1" OR
   NOT typography_package_test_source MATCHES "controls_page_rect id=camera_status" OR
   NOT typography_package_test_source MATCHES "controls_page id=camera_status selected=1")
    list(APPEND violations "the packaged typography workflow must navigate to the production Camera/Status page that owns Settings and confirm its real hit target and selected state")
endif()
if(NOT main_source MATCHES "sandbox3d_editor_layout_controls_status_page\\(" OR
   NOT main_source MATCHES "sandbox3d_controls_flow_next_fully_visible_row\\(" OR
   NOT main_source MATCHES "HENKA_AUTOMATION_DIAGNOSTIC controls_scroll_viewport page=camera_status" OR
   NOT main_source MATCHES "sandbox3d_draw_panel_scrollbar\\(" OR
   NOT main_source MATCHES "SANDBOX3D_PANEL_SCROLL_CONTROLS")
    list(APPEND violations "Camera/Status content must use the canonical bounded scroll viewport, fully visible row hit targets, and existing controls scrollbar authority")
endif()
string(FIND "${main_source}" "static void sandbox3d_handle_panel_scroll(" controls_scroll_handler_start)
if(controls_scroll_handler_start LESS 0)
    list(APPEND violations "the shared panel scroll input owner could not be isolated for its Camera/Status routing contract")
else()
    string(SUBSTRING "${main_source}" ${controls_scroll_handler_start} -1 controls_scroll_handler_tail)
    string(FIND "${controls_scroll_handler_tail}" "static void sandbox3d_handle_detached_panel_input(" controls_scroll_handler_end)
    if(controls_scroll_handler_end LESS 0)
        list(APPEND violations "the shared panel scroll input owner could not be bounded for its Camera/Status routing contract")
    else()
        string(SUBSTRING "${controls_scroll_handler_tail}" 0 ${controls_scroll_handler_end} controls_scroll_handler_source)
        if(NOT controls_scroll_handler_source MATCHES "SANDBOX3D_CONTROLS_PAGE_MAIN" OR
           NOT controls_scroll_handler_source MATCHES "SANDBOX3D_CONTROLS_PAGE_CAMERA_STATUS" OR
           NOT controls_scroll_handler_source MATCHES "sandbox3d_editor_ui_scroll_controls_by")
            list(APPEND violations "the shared controls scroll handler must route both Main and Camera/Status pages through the same existing scroll state")
        endif()
    endif()
endif()
if(NOT typography_package_test_source MATCHES "Get-ReportedScrollState" OR
   NOT typography_package_test_source MATCHES "Send-HenkaAutomationScroll" OR
   NOT typography_package_test_source MATCHES "utility_tab id=settings" OR
   NOT typography_package_test_source MATCHES "insideViewport")
    list(APPEND violations "the packaged typography workflow must reach Settings by bounded real wheel input using product-reported scroll and tab geometry")
endif()
string(FIND "${typography_package_test_source}"
    "$fontUserDataRoot = Join-Path $userDataRoot $font"
    font_userdata_root_position)
string(FIND "${typography_package_test_source}"
    "HENKA_AUTOMATION_USER_DATA_BASE_PATH = $UserDataRoot"
    font_userdata_environment_position)
string(REGEX MATCHALL
    "-UserDataRoot \\$fontUserDataRoot"
    font_userdata_arguments
    "${typography_package_test_source}")
list(LENGTH font_userdata_arguments font_userdata_argument_count)
string(FIND "${typography_package_test_source}"
    "captures = $captures.ToArray()"
    capture_array_materialization_position)
if(font_userdata_root_position LESS 0 OR
   font_userdata_environment_position LESS 0 OR
   NOT font_userdata_argument_count EQUAL 2 OR
   capture_array_materialization_position LESS 0)
    list(APPEND violations "Atkinson and Noto comparisons must use separate per-font user-data roots across both sessions and materialize capture records into a JSON-compatible array")
endif()
string(FIND "${main_source}" "sandbox3d_editor_typography_runtime_apply_pending_scale(" pending_scale_apply_position)
string(FIND "${main_source}" "sandbox3d_build_ui(engine, state)" editor_ui_build_position)
if(pending_scale_apply_position LESS 0 OR editor_ui_build_position LESS 0 OR
   pending_scale_apply_position GREATER editor_ui_build_position OR
   NOT runtime_source MATCHES "pending_scale_change = true")
    list(APPEND violations "a clicked scale change must be deferred until the pre-UI-frame update boundary so UI layout and atlas resources cannot mix generations")
endif()
if(NOT typography_package_test_source MATCHES "-BorderlessViewport" OR
   NOT typography_package_test_source MATCHES "window_presentation = \\$Session.WindowPresentation" OR
   NOT ui_automation_helper_source MATCHES "SetBorderlessDesktopViewportNoActivate" OR
   NOT ui_automation_helper_source MATCHES "SWP_NOACTIVATE")
    list(APPEND violations "1920x1080 packaged typography captures must use an exact-desktop borderless viewport and preserve non-activating behavior in their evidence metadata")
endif()

if(NOT runtime_source MATCHES "HENKA_AUTOMATION_FRAME_CAPTURE_DIRECTORY" OR
   NOT runtime_source MATCHES "HENKA_AUTOMATION_FRAME_CAPTURE_LABEL" OR
   NOT runtime_source MATCHES "henka_engine_request_frame_capture" OR
   NOT runtime_source MATCHES "sandbox3d_typography_request_automation_frame_capture" OR
   NOT typography_package_test_source MATCHES "Wait-TypographyFrameCapture" OR
   NOT typography_package_test_source MATCHES "framebuffer_capture_width" OR
   NOT typography_package_test_source MATCHES "window_capture_sha256" OR
   NOT typography_package_test_source MATCHES "screenshot_sha256 -eq")
    list(APPEND violations "typography evidence must be captured from the product-owned completed framebuffer at startup and after an applied scale, under bounded automation-only path authority")
endif()

string(FIND "${typography_package_test_source}"
    "function Convert-TypographyLogicalPointToFramebufferPoint"
    typography_coordinate_transform_position)
string(FIND "${typography_package_test_source}"
    "os_display_scale=" typography_os_scale_authority_position)
string(FIND "${typography_package_test_source}"
    "-X $framebufferPoint.X" typography_transformed_click_x_position)
string(FIND "${typography_package_test_source}"
    "-Y $framebufferPoint.Y" typography_transformed_click_y_position)
if(typography_coordinate_transform_position LESS 0 OR
   typography_os_scale_authority_position LESS 0 OR
   typography_transformed_click_x_position LESS 0 OR
   typography_transformed_click_y_position LESS 0)
    list(APPEND violations "scale-control automation must transform product-reported logical rectangles using the runtime UI and OS display scales before sending framebuffer pointer input")
endif()

string(FIND "${main_source}" "henka_ui_context_set_font_profile(" direct_profile_setter_position)
if(NOT direct_profile_setter_position LESS 0)
    list(APPEND violations "main.c must not become a competing font-profile authority")
endif()

if(violations)
    list(JOIN violations "\n  " formatted_violations)
    message(FATAL_ERROR "Sandbox3D editor typography integration is incomplete:\n  ${formatted_violations}")
endif()

message(STATUS "Sandbox3D installs bundled typography before workspace restore and routes settings, scale consumers, and context lifetimes through its runtime owner")
