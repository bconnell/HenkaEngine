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
if(DEFINED TYPOGRAPHY_RUNTIME_SOURCE)
    file(READ "${TYPOGRAPHY_RUNTIME_SOURCE}" runtime_source)
else()
    file(READ "${SANDBOX3D_SOURCE_DIR}/editor_typography_runtime.c" runtime_source)
endif()
file(READ "${SANDBOX3D_SOURCE_DIR}/editor_font_bootstrap.c" bootstrap_source)
file(READ "${SANDBOX3D_SOURCE_DIR}/editor_font_bootstrap.h" bootstrap_header)
file(READ "${SANDBOX3D_SOURCE_DIR}/../../engine/src/platform/platform_sdl.c" platform_source)
file(READ "${SANDBOX3D_SOURCE_DIR}/../../engine/src/core/engine.c" engine_source)
file(READ "${SANDBOX3D_SOURCE_DIR}/../../scripts/package_sandbox3d_windows.ps1" package_source)
if(DEFINED TYPOGRAPHY_PACKAGE_TEST_SOURCE)
    file(READ "${TYPOGRAPHY_PACKAGE_TEST_SOURCE}" typography_package_test_source)
else()
    file(READ "${SANDBOX3D_SOURCE_DIR}/../../scripts/test_editor_typography_windows.ps1" typography_package_test_source)
endif()
file(READ "${SANDBOX3D_SOURCE_DIR}/../../scripts/check_packaged_sandbox3d_windows.ps1" packaged_sandbox_checker_source)
file(READ "${SANDBOX3D_SOURCE_DIR}/../../scripts/henka_typography_package_policy.ps1" typography_package_policy_source)
file(READ "${SANDBOX3D_SOURCE_DIR}/../../scripts/henka_typography_capture_policy.ps1" typography_capture_policy_source)
if(DEFINED HENKA_UI_AUTOMATION_HELPERS_SOURCE)
    file(READ "${HENKA_UI_AUTOMATION_HELPERS_SOURCE}" ui_automation_helper_source)
else()
    file(READ "${SANDBOX3D_SOURCE_DIR}/../../scripts/henka_ui_automation_helpers.ps1" ui_automation_helper_source)
endif()
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

string(FIND "${main_source}" "static henka_result sandbox3d_initialize(" app_initialize_start)
string(FIND "${main_source}" "static void sandbox3d_update(" app_update_start)
string(FIND "${main_source}" "static void sandbox3d_shutdown(" app_shutdown_start)
string(FIND "${main_source}" "static bool sandbox3d_run_prefab_public_smoke(" app_shutdown_end)
if(app_initialize_start LESS 0 OR app_update_start LESS app_initialize_start OR
   app_shutdown_start LESS app_update_start OR app_shutdown_end LESS app_shutdown_start)
    list(APPEND violations "Sandbox3D frame-capture lifecycle boundaries could not be identified")
else()
    math(EXPR app_initialize_length "${app_update_start} - ${app_initialize_start}")
    math(EXPR app_update_length "${app_shutdown_start} - ${app_update_start}")
    math(EXPR app_shutdown_length "${app_shutdown_end} - ${app_shutdown_start}")
    string(SUBSTRING "${main_source}" ${app_initialize_start} ${app_initialize_length} app_initialize_source)
    string(SUBSTRING "${main_source}" ${app_update_start} ${app_update_length} app_update_source)
    string(SUBSTRING "${main_source}" ${app_shutdown_start} ${app_shutdown_length} app_shutdown_source)
    if(NOT app_initialize_source MATCHES "sandbox3d_automation_frame_capture_create\\(" OR
       NOT app_initialize_source MATCHES "sandbox3d_automation_frame_capture_destroy\\(" OR
       NOT app_update_source MATCHES "sandbox3d_automation_frame_capture_update\\(" OR
       NOT app_shutdown_source MATCHES "sandbox3d_automation_frame_capture_destroy\\(")
        list(APPEND violations "the application-owned framebuffer capture request must be created, serviced in the frame update, and destroyed on normal and failed initialization paths")
    endif()
    string(FIND "${app_update_source}"
        "sandbox3d_service_tool_window_close_requests(engine, state);" typography_close_service_position)
    string(FIND "${app_update_source}"
        "sandbox3d_editor_typography_runtime_sync_display_scales(" typography_scale_sync_position)
    if(typography_close_service_position LESS 0 OR typography_scale_sync_position LESS 0 OR
       typography_close_service_position GREATER typography_scale_sync_position)
        list(APPEND violations "owned tool-window close requests must be serviced before typography scale sync can reject a closing context")
    endif()
endif()

string(FIND "${engine_source}" "engine->config.on_update(" typography_engine_update_position)
string(FIND "${engine_source}" "henka_engine_close_requested_tool_windows(engine);"
    typography_engine_close_fallback_position)
if(typography_engine_update_position LESS 0 OR typography_engine_close_fallback_position LESS 0 OR
   typography_engine_close_fallback_position LESS typography_engine_update_position)
    list(APPEND violations "the application update must observe tool-window close requests before the engine applies its automatic close fallback")
endif()

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
    string(FIND "${settings_source}" "sandbox3d_utility_settings_layout_build(" settings_layout_position)
    string(FIND "${settings_source}" "sandbox3d_editor_scale_control_layout(" scale_layout_position)
    string(FIND "${settings_source}" "sandbox3d_utility_settings_rect_visible(" scale_visibility_position)
    string(FIND "${settings_source}" "settings_layout.scale_bounds" scale_bounds_position)
    string(FIND "${settings_source}" "sandbox3d_editor_typography_runtime_draw_scale_control(" scale_control_position)
    string(FIND "${settings_source}" "sandbox3d_utility_settings_draw_value_row(" settings_body_position)
    if(settings_layout_position LESS 0 OR scale_layout_position LESS 0 OR
       scale_visibility_position LESS 0 OR scale_bounds_position LESS 0 OR
       scale_control_position LESS 0 OR settings_body_position LESS 0 OR
       settings_layout_position GREATER scale_layout_position OR
       scale_layout_position GREATER scale_control_position OR
       scale_visibility_position GREATER scale_control_position OR
       scale_bounds_position GREATER scale_control_position OR
       scale_control_position GREATER settings_body_position)
        list(APPEND violations "Settings scale control must remain reachable in the Settings header row before its content body")
    endif()
endif()

if(NOT main_source MATCHES "sandbox3d_editor_layout_text_control_grid_for_context\\(" OR
   NOT main_source MATCHES "utility_navigation_row_count")
    list(APPEND violations "Utility navigation must use the canonical measured wrapping layout and derive content placement from its actual row count")
endif()
if(NOT main_source MATCHES "state->automation_diagnostic_terrain_utility_tabs_visible = false;[ \t\r\n]*utility_navigation_row_count = 0U;[ \t\r\n]*utility_navigation_row_height = 0\\.0f;[ \t\r\n]*navigation_result =")
    list(APPEND violations "Utility tab visibility telemetry and measured geometry outputs must reset before each layout attempt so failed layout cannot reuse stale state")
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

string(FIND "${package_source}" "IncludeTypographyComparisonFonts" comparison_package_switch_position)
string(FIND "${package_source}" "Remove-Item -LiteralPath $comparisonAssetPath" comparison_asset_removal_position)
string(FIND "${package_source}" "Typography comparison candidates: $comparisonFontDisposition" comparison_package_marker_position)
string(FIND "${package_source}" "Get-HenkaTypographySelectedFontMetadata" selected_font_metadata_position)
if(comparison_package_switch_position LESS 0 OR
   comparison_asset_removal_position LESS 0 OR
   comparison_package_marker_position LESS 0 OR
   NOT package_source MATCHES "NotoSans-Regular\\.ttf" OR
   selected_font_metadata_position LESS 0 OR
   NOT package_source MATCHES "Assert-HenkaTypographyPackageAssetSet" OR
   NOT typography_package_policy_source MATCHES "AtkinsonHyperlegible-Regular\\.ttf" OR
   NOT typography_package_policy_source MATCHES "7FB917C89019896D0B52EE84B7CBB3304C18CB90B19A62F5E32712BD23E97669" OR
   NOT typography_package_policy_source MATCHES "478C558EA716033CD60C03438F628DFA75694DCF6B5F6D505A2F05FD2B4F3823" OR
   NOT typography_package_test_source MATCHES "Typography comparison candidates" OR
   NOT typography_package_test_source MATCHES "Assert-HenkaTypographyPackageAssetSet")
    list(APPEND violations "normal Sandbox packages must ship the selected Atkinson face only; Noto must require the explicit local comparison-package path")
endif()
if(NOT runtime_source MATCHES "HENKA_AUTOMATION_EDITOR_FONT" OR
   NOT runtime_source MATCHES "HENKA_AUTOMATION_INPUT_OWNED" OR
   NOT runtime_source MATCHES "HENKA_AUTOMATION_DIAGNOSTICS" OR
   NOT runtime_source MATCHES "font_identity=" OR
   NOT runtime_source MATCHES "font_sha256=" OR
   NOT runtime_source MATCHES "ui_scale_percent=")
    list(APPEND violations "packaged typography comparison must select only a whitelisted automation-owned candidate and report the verified font and active scale")
endif()
if(NOT platform_source MATCHES "SDL_WINDOW_NOT_FOCUSABLE" OR
   NOT platform_source MATCHES "window_focus_policy focusable=%u input_focused=%u" OR
   NOT typography_package_test_source MATCHES "window_focus_policy focusable=0 input_focused=0" OR
   NOT typography_package_test_source MATCHES "-Path[ \t]+\\$stderrPath[^\r\n]*`[ \t\r\n]*-Pattern[ \t\r\n]*'HENKA_AUTOMATION_DIAGNOSTIC window_focus_policy")
    list(APPEND violations "automation-owned typography windows must use and report the SDL non-focusable policy, and the packaged test must require its observed state")
endif()

string(FIND "${platform_source}" "if (platform->window_start_hidden)" hidden_main_policy_position)
string(FIND "${platform_source}" "window_flags |= SDL_WINDOW_HIDDEN;" hidden_main_flag_position)
string(FIND "${platform_source}" "platform->window = SDL_CreateWindow(" main_window_create_position)
string(FIND "${platform_source}" "platform->tool_windows_start_hidden = platform->window_start_hidden;" hidden_tool_policy_position)
string(FIND "${platform_source}" "if (platform->tool_windows_start_hidden)" hidden_tool_flag_position)
string(FIND "${platform_source}" "slot->window = SDL_CreateWindow(" tool_window_create_position)
string(FIND "${platform_source}" "desc->window_start_hidden || platform->automation_input_owned" effective_hidden_policy_position)
if(hidden_main_policy_position LESS 0 OR hidden_main_flag_position LESS 0 OR
   main_window_create_position LESS 0 OR hidden_main_policy_position GREATER hidden_main_flag_position OR
   hidden_main_flag_position GREATER main_window_create_position OR
   hidden_tool_policy_position LESS 0 OR hidden_tool_flag_position LESS 0 OR
   tool_window_create_position LESS 0 OR hidden_tool_policy_position GREATER hidden_tool_flag_position OR
   hidden_tool_flag_position GREATER tool_window_create_position OR
   effective_hidden_policy_position LESS 0 OR effective_hidden_policy_position GREATER main_window_create_position)
    list(APPEND violations "automation-owned SDL main and tool windows must start hidden so late window creation cannot flash across the user's desktop")
endif()

string(FIND "${packaged_sandbox_checker_source}" [=[HENKA_AUTOMATION_TOOL_WINDOWS_START_HIDDEN = "1"]=] packaged_hidden_tool_env_position)
string(FIND "${packaged_sandbox_checker_source}" "Show-HenkaAutomationWindowOffscreen -Handle $nativeWindowHandle" native_window_safe_reveal_position)
string(FIND "${packaged_sandbox_checker_source}" [=[-Description "Packaged native panel screenshot"]=] native_window_capture_position)
if(packaged_hidden_tool_env_position LESS 0 OR native_window_safe_reveal_position LESS 0 OR
   native_window_capture_position LESS 0 OR native_window_safe_reveal_position GREATER native_window_capture_position)
    list(APPEND violations "packaged automation must keep late tool windows hidden and reveal only the capture target after moving it fully outside the virtual desktop")
endif()

string(FIND "${typography_package_test_source}" [=[HENKA_AUTOMATION_TOOL_WINDOWS_START_HIDDEN = "1"]=] typography_hidden_tool_env_position)
if(typography_hidden_tool_env_position LESS 0)
    list(APPEND violations "typography comparison automation must never opt into on-desktop native tool windows")
endif()

if(NOT main_source MATCHES "utility_settings" OR
   NOT main_source MATCHES "utility_settings_rect" OR
   NOT main_source MATCHES "HENKA_AUTOMATION_DIAGNOSTIC utility_tab")
    list(APPEND violations "the packaged typography path must expose the real Settings tab geometry and active state only through bounded automation telemetry")
endif()
if(NOT main_source MATCHES "sandbox3d_utility_settings_layout_build" OR
   NOT main_source MATCHES "SANDBOX3D_PANEL_SCROLL_UTILITY" OR
   NOT main_source MATCHES "sandbox3d_utility_settings_rect_visible" OR
   NOT main_source MATCHES "HENKA_AUTOMATION_DIAGNOSTIC utility_settings_layout" OR
   NOT typography_package_test_source MATCHES "Test-TypographySettingsScrollControl" OR
   NOT typography_package_test_source MATCHES "settings_scroll_control_tests" OR
   NOT typography_package_test_source MATCHES "real_click_changed_authoritative_setting" OR
   NOT typography_package_test_source MATCHES "wheel_events_to_reveal_control")
    list(APPEND violations "Utility Settings must use one product geometry authority, bounded visible hit targets, independent scrolling, and a packaged control interaction regression")
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
   NOT typography_package_test_source MATCHES "Wait-TypographyStableToolsControl" OR
   NOT typography_package_test_source MATCHES "phase=render-complete" OR
   NOT typography_package_test_source MATCHES "Wait-TypographySettingsNavigationTab" OR
   NOT typography_package_test_source MATCHES "utility_navigation_tab id=settings" OR
   NOT typography_package_test_source MATCHES "visible=1 active=1 ui_scale_percent=" OR
   NOT main_source MATCHES "utility_navigation_tab id=settings" OR
   NOT typography_package_test_source MATCHES "insideViewport")
    list(APPEND violations "the packaged typography workflow must reach Settings by bounded real wheel input and use the canonical visible Utility-tab rectangle for the active Editor UI Scale")
endif()
if(NOT typography_package_test_source MATCHES "Wait-TypographyStableUtilityContextHeader" OR
   NOT typography_package_test_source MATCHES "workspace_context_header id=utility" OR
   NOT typography_package_test_source MATCHES "headerInsideViewport" OR
   NOT typography_package_test_source MATCHES "Send-TypographyLogicalClick" OR
   NOT main_source MATCHES "workspace_context_header id=utility" OR
   NOT main_source MATCHES "sandbox3d_workspace_docked_title_drag_rect" OR
   NOT main_source MATCHES "ui_scale_percent=%d frame=%llu")
    list(APPEND violations "the detached-context workflow must right-click the product-reported Utility title-strip hit rectangle used by the production context-menu handler")
endif()
if(NOT ui_automation_helper_source MATCHES "function Convert-HenkaUiLogicalPointToWindowPoint" OR
   NOT ui_automation_helper_source MATCHES "function Send-HenkaAutomationUiClick" OR
   NOT ui_automation_helper_source MATCHES "function Send-HenkaAutomationUiScroll" OR
   NOT typography_package_test_source MATCHES "Send-TypographyLogicalClick" OR
   NOT typography_package_test_source MATCHES "Send-TypographyLogicalScroll")
    list(APPEND violations "typography package interactions must use the canonical logical-to-window input helper at the selected Editor UI Scale")
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

string(FIND "${typography_package_test_source}"
    [=[$validationHarnessIdentity = Get-HenkaSourceIdentity -RepoRoot $repoRoot]=]
    validation_harness_identity_position)
string(FIND "${typography_package_test_source}"
    [=[$validationHarnessIdentity.source_state -ne "clean"]=]
    validation_harness_clean_candidate_position)
string(FIND "${typography_package_test_source}"
    [=[$finalHarnessIdentity = Get-HenkaSourceIdentity -RepoRoot $repoRoot]=]
    validation_harness_final_identity_position)
string(FIND "${typography_package_test_source}"
    [=[$finalHarnessIdentity.source_identity -ne $validationHarnessIdentity.source_identity]=]
    validation_harness_unchanged_check_position)
string(FIND "${typography_package_test_source}"
    [=[commit_sha = [string]$validationHarnessIdentity.commit_sha]=]
    validation_harness_commit_field_position)
string(FIND "${typography_package_test_source}"
    [=[source_state = [string]$validationHarnessIdentity.source_state]=]
    validation_harness_state_field_position)
string(FIND "${typography_package_test_source}"
    [=[source_identity = [string]$validationHarnessIdentity.source_identity]=]
    validation_harness_source_identity_field_position)

if(NOT typography_package_test_source MATCHES "os_display_scale_numerator = \\$runtimeScales.DisplayScaleNumerator" OR
   NOT typography_package_test_source MATCHES "os_display_scale_denominator = \\$runtimeScales.DisplayScaleDenominator" OR
   NOT typography_package_test_source MATCHES "schema_version = 5" OR
   NOT typography_package_test_source MATCHES "Get-HenkaTypographyCaptureScalePairViolation" OR
   NOT typography_capture_policy_source MATCHES "different product-reported OS display scales")
    list(APPEND violations "each packaged capture must record product-reported OS display scale and reject unmatched Atkinson/Noto comparison pairs")
endif()

if(validation_harness_identity_position LESS 0 OR
   validation_harness_clean_candidate_position LESS 0 OR
   validation_harness_final_identity_position LESS 0 OR
   validation_harness_unchanged_check_position LESS 0 OR
   validation_harness_commit_field_position LESS 0 OR
   validation_harness_state_field_position LESS 0 OR
   validation_harness_source_identity_field_position LESS 0 OR
   validation_harness_final_identity_position LESS validation_harness_identity_position OR
   validation_harness_unchanged_check_position LESS validation_harness_final_identity_position OR
   validation_harness_commit_field_position LESS validation_harness_final_identity_position)
    list(APPEND violations "packaged typography evidence must bind the exact clean validation-harness commit and source identity separately from the executable build")
endif()

string(FIND "${typography_package_test_source}" [=[detached_context = $detachedContextProof]=]
    detached_context_assignment_position)
string(FIND "${typography_package_test_source}" [=[HENKA_AUTOMATION_TOOL_WINDOWS_START_HIDDEN = "1"]=]
    detached_context_hidden_launch_position)
string(FIND "${typography_package_test_source}" "IsWindowVisible($detachedWindow)"
    detached_context_hidden_state_check_position)
string(FIND "${typography_package_test_source}"
    [=[$contextLine = Wait-LogLine]=]
    detached_context_attach_position)
string(FIND "${typography_package_test_source}"
    [=[$renderSequenceAtContextReady = Get-TypographyLastRenderCompleteSequence]=]
    detached_context_frame_baseline_position)
string(FIND "${typography_package_test_source}"
    [=[$detachedRender = Wait-TypographyRenderCompleteAfterSequence]=]
    detached_context_frame_wait_position)
string(FIND "${typography_package_test_source}"
    [=[$foregroundAfterDetach = [HenkaUiAutomationNative]::GetForegroundWindow()]=]
    detached_context_post_render_state_position)
string(FIND "${typography_package_test_source}"
    [=[render_complete_after_context_attach = $true]=]
    detached_context_render_proof_position)
string(FIND "${typography_package_test_source}"
    [=[screenshot_status = 'not_captured']=]
    detached_context_screenshot_disposition_position)
string(FIND "${typography_package_test_source}"
    "Detached OpenGL pixels are not represented by a Win32 PrintWindow screenshot."
    detached_context_capture_scope_position)
if(NOT main_source MATCHES "HENKA_AUTOMATION_START_HIDDEN" OR
   NOT main_source MATCHES "window_start_hidden=%u" OR
    NOT runtime_source MATCHES "editor_typography_context stage=attached" OR
   NOT typography_package_test_source MATCHES "Test-TypographyDetachedUtilityContext" OR
   detached_context_hidden_launch_position LESS 0 OR
   detached_context_hidden_state_check_position LESS 0 OR
   detached_context_attach_position LESS 0 OR
   detached_context_frame_baseline_position LESS 0 OR
   detached_context_frame_wait_position LESS 0 OR
   detached_context_post_render_state_position LESS 0 OR
   detached_context_render_proof_position LESS 0 OR
   detached_context_screenshot_disposition_position LESS 0 OR
   detached_context_capture_scope_position LESS 0 OR
   detached_context_assignment_position LESS 0 OR
   detached_context_frame_baseline_position LESS detached_context_attach_position OR
   detached_context_frame_wait_position LESS detached_context_frame_baseline_position OR
   detached_context_post_render_state_position LESS detached_context_frame_wait_position OR
   detached_context_render_proof_position LESS detached_context_post_render_state_position OR
   detached_context_capture_scope_position LESS detached_context_render_proof_position)
    list(APPEND violations "packaged typography must prove a real detached-font context and completed render while hidden, and must not claim Win32 PrintWindow output as detached OpenGL pixels")
endif()

string(FIND "${typography_package_test_source}"
    [=[HENKA_AUTOMATION_START_HIDDEN = "1"]=]
    typography_main_window_hidden_mode_position)
string(FIND "${typography_package_test_source}"
    "window_start_hidden=1"
    typography_main_window_hidden_assertion_position)
string(FIND "${typography_package_test_source}"
    "-IncludeHidden"
    typography_main_window_discovery_position)
string(FIND "${typography_package_test_source}"
    "-AllowHidden"
    typography_main_window_capture_position)
string(FIND "${typography_package_test_source}"
    "-StartVisibleWithoutActivation"
    typography_main_window_activation_position)
string(FIND "${typography_package_test_source}"
    [=[if ($windowVisible -or $windowIconic -or $foregroundWindow -eq $window)]=]
    typography_main_window_state_assertion_position)
if(typography_main_window_hidden_mode_position LESS 0 OR
   typography_main_window_hidden_assertion_position LESS 0 OR
   typography_main_window_discovery_position LESS 0 OR
   typography_main_window_capture_position LESS 0 OR
   typography_main_window_state_assertion_position LESS 0 OR
   NOT typography_main_window_activation_position LESS 0)
    list(APPEND violations "packaged typography must launch and capture the main Sandbox window hidden, without an activation request")
endif()

string(REGEX MATCHALL "-StartOffscreenWithoutActivation" packaged_offscreen_launches
    "${packaged_sandbox_checker_source}")
list(LENGTH packaged_offscreen_launches packaged_offscreen_launch_count)
if(packaged_offscreen_launch_count LESS 2)
    list(APPEND violations "both packaged Sandbox launch paths must keep capture windows outside the owner's desktop")
endif()
string(FIND "${typography_package_test_source}"
    "function Ensure-TypographySettingsVisible"
    typography_settings_path_position)
string(FIND "${typography_package_test_source}"
    "workspace_topology cause=tools_toggle tools_visible=1"
    typography_tools_visibility_position)
string(FIND "${typography_package_test_source}"
    "controls_page_rect id=camera_status"
    typography_camera_status_geometry_position)
string(FIND "${typography_package_test_source}"
    "controls_page id=camera_status selected=1"
    typography_camera_status_selection_position)
string(FIND "${typography_package_test_source}"
    "controls_scroll_viewport page=camera_status"
    typography_camera_status_scroll_viewport_position)
string(FIND "${typography_package_test_source}"
    "utility_tab id=settings"
    typography_settings_tab_position)
if(typography_settings_path_position LESS 0 OR
   typography_tools_visibility_position LESS 0 OR
   typography_camera_status_geometry_position LESS 0 OR
   typography_camera_status_selection_position LESS 0 OR
   typography_camera_status_scroll_viewport_position LESS 0 OR
   typography_settings_tab_position LESS 0)
    list(APPEND violations "the typography package workflow must reach Editor UI Scale through the actual Tools, Camera/Status, scroll, and Settings controls")
endif()

string(FIND "${main_source}" "henka_ui_context_set_font_profile(" direct_profile_setter_position)
if(NOT direct_profile_setter_position LESS 0)
    list(APPEND violations "main.c must not become a competing font-profile authority")
endif()

string(FIND "${main_source}"
    "static void sandbox3d_update(henka_engine* engine, double delta_seconds, void* user_data)"
    typography_update_definition_position)
if(typography_update_definition_position LESS 0)
    list(APPEND violations "Sandbox3D update boundary must remain identifiable for deferred native-window resource work")
else()
    string(SUBSTRING "${main_source}" ${typography_update_definition_position} -1
        typography_update_source)
    string(FIND "${typography_update_source}"
        "sandbox3d_process_pending_tool_window_open("
        typography_deferred_window_process_position)
    string(FIND "${typography_update_source}"
        "sandbox3d_build_ui("
        typography_main_ui_build_position)
    if(typography_deferred_window_process_position LESS 0 OR
       typography_main_ui_build_position LESS 0 OR
       NOT typography_deferred_window_process_position LESS typography_main_ui_build_position)
        list(APPEND violations "queued native-window font/resource preparation must run from Sandbox3D update before the next main UI frame")
    endif()
endif()

string(FIND "${main_source}" "pending_tool_window_open_kind"
    typography_pending_window_state_position)
string(FIND "${main_source}" "sandbox3d_queue_tool_window_open("
    typography_pending_window_queue_position)
string(FIND "${main_source}"
    "static bool sandbox3d_open_native_panel_test(henka_engine* engine, sandbox3d_state* state)"
    typography_native_open_definition_position REVERSE)
string(FIND "${main_source}"
    "static bool sandbox3d_open_native_panel_test_now(henka_engine* engine, sandbox3d_state* state)"
    typography_native_open_executor_position REVERSE)
if(typography_pending_window_state_position LESS 0 OR
   typography_pending_window_queue_position LESS 0 OR
   typography_native_open_definition_position LESS 0 OR
   typography_native_open_executor_position LESS typography_native_open_definition_position)
    list(APPEND violations "native tool-window requests need explicit bounded pending state and a product-owned deferred processor")
else()
    math(EXPR typography_native_open_body_length
        "${typography_native_open_executor_position} - ${typography_native_open_definition_position}")
    string(SUBSTRING "${main_source}" ${typography_native_open_definition_position}
        ${typography_native_open_body_length} typography_native_open_body)
    if(NOT typography_native_open_body MATCHES "sandbox3d_queue_tool_window_open" OR
       typography_native_open_body MATCHES "henka_engine_open_tool_window|sandbox3d_editor_typography_runtime_attach_context")
        list(APPEND violations "the Native Panel Test UI action must queue its request without opening a window or preparing fonts during the active UI frame")
    endif()
endif()

string(FIND "${main_source}"
    "static void sandbox3d_build_native_panel_test_ui(henka_engine* engine, sandbox3d_state* state)"
    typography_native_panel_ui_definition_position REVERSE)
string(FIND "${main_source}"
    "static bool sandbox3d_open_detached_workspace_panel("
    typography_native_panel_ui_end_position REVERSE)
if(typography_native_panel_ui_definition_position LESS 0 OR
   typography_native_panel_ui_end_position LESS typography_native_panel_ui_definition_position)
    list(APPEND violations "native panel context teardown must remain in its product-owned update boundary")
else()
    math(EXPR typography_native_panel_ui_body_length
        "${typography_native_panel_ui_end_position} - ${typography_native_panel_ui_definition_position}")
    string(SUBSTRING "${main_source}" ${typography_native_panel_ui_definition_position}
        ${typography_native_panel_ui_body_length} typography_native_panel_ui_body)
    string(FIND "${typography_native_panel_ui_body}"
        "if (window_state.close_requested)" typography_native_panel_close_condition_position)
    string(FIND "${typography_native_panel_ui_body}"
        "sandbox3d_close_native_panel_test(engine, state);" typography_native_panel_close_owner_position)
    string(FIND "${typography_native_panel_ui_body}"
        "!window_state.open" typography_native_panel_closed_state_position)
    if(typography_native_panel_close_condition_position LESS 0 OR
       typography_native_panel_close_owner_position LESS typography_native_panel_close_condition_position OR
       (typography_native_panel_closed_state_position GREATER_EQUAL 0 AND
        typography_native_panel_close_condition_position GREATER typography_native_panel_closed_state_position))
        list(APPEND violations "the Native Panel Test must consume OS close requests before stale/closed-state handling, close through Henka, and detach its font context")
    endif()
endif()

string(FIND "${platform_source}" "bool henka_platform_get_tool_window_state("
    typography_platform_tool_state_start REVERSE)
string(FIND "${platform_source}" "bool henka_platform_set_tool_window_position("
    typography_platform_tool_state_end REVERSE)
if(typography_platform_tool_state_start LESS 0 OR
   typography_platform_tool_state_end LESS typography_platform_tool_state_start)
    list(APPEND violations "the platform tool-window state boundary could not be identified")
else()
    math(EXPR typography_platform_tool_state_length
        "${typography_platform_tool_state_end} - ${typography_platform_tool_state_start}")
    string(SUBSTRING "${platform_source}" ${typography_platform_tool_state_start}
        ${typography_platform_tool_state_length} typography_platform_tool_state_body)
    string(FIND "${typography_platform_tool_state_body}"
        "if (slot->close_requested)" typography_platform_close_request_guard_position)
    string(FIND "${typography_platform_tool_state_body}"
        "out_state->close_requested = true;" typography_platform_close_request_field_position)
    string(FIND "${typography_platform_tool_state_body}"
        "return true;" typography_platform_close_request_success_position)
    string(FIND "${typography_platform_tool_state_body}"
        "SDL_GetWindowSize(slot->window" typography_platform_live_metrics_position)
    if(typography_platform_close_request_guard_position LESS 0 OR
       typography_platform_close_request_field_position LESS typography_platform_close_request_guard_position OR
       typography_platform_close_request_success_position LESS typography_platform_close_request_field_position OR
       typography_platform_live_metrics_position LESS typography_platform_close_request_success_position)
        list(APPEND violations "tool-window close requests must remain observable even when optional live display metrics are unavailable")
    endif()
endif()

string(FIND "${packaged_sandbox_checker_source}"
    [=[-Pattern "Native Panel Test: closed"]=]
    typography_native_panel_close_ack_position)
string(FIND "${packaged_sandbox_checker_source}"
    [=[$nativeReopenOffset]=]
    typography_native_panel_reopen_position)
if(typography_native_panel_close_ack_position LESS 0 OR
   typography_native_panel_reopen_position LESS typography_native_panel_close_ack_position)
    list(APPEND violations "the packaged native-window reopen proof must wait for product acknowledgement of the OS close before clicking reopen")
endif()

string(FIND "${main_source}"
    "static void sandbox3d_build_detached_workspace_panel_ui(henka_engine* engine, sandbox3d_state* state)"
    typography_detached_panel_ui_definition_position REVERSE)
string(FIND "${main_source}"
    "static bool sandbox3d_apply_camera_preset("
    typography_detached_panel_ui_end_position REVERSE)
if(typography_detached_panel_ui_definition_position LESS 0 OR
   typography_detached_panel_ui_end_position LESS typography_detached_panel_ui_definition_position)
    list(APPEND violations "detached panel close-request recovery must remain in its product-owned update boundary")
else()
    math(EXPR typography_detached_panel_ui_body_length
        "${typography_detached_panel_ui_end_position} - ${typography_detached_panel_ui_definition_position}")
    string(SUBSTRING "${main_source}" ${typography_detached_panel_ui_definition_position}
        ${typography_detached_panel_ui_body_length} typography_detached_panel_ui_body)
    string(FIND "${typography_detached_panel_ui_body}"
        "window_state.close_requested" typography_detached_panel_close_condition_position)
    string(FIND "${typography_detached_panel_ui_body}"
        "panel->last_docked_zone" typography_detached_panel_restore_zone_position)
    if(typography_detached_panel_close_condition_position LESS 0 OR
       typography_detached_panel_restore_zone_position LESS 0)
        list(APPEND violations "an OS-closed detached workspace panel must release its typography context and return to its canonical prior dock")
    endif()
endif()

string(FIND "${main_source}" "static bool sandbox3d_open_detached_workspace_panel("
    typography_detached_open_definition_position REVERSE)
string(FIND "${main_source}" "static bool sandbox3d_open_detached_workspace_panel_now("
    typography_detached_open_executor_position REVERSE)
if(typography_detached_open_definition_position LESS 0 OR
   typography_detached_open_executor_position LESS typography_detached_open_definition_position)
    list(APPEND violations "detached workspace window requests must use the same deferred typography-safe boundary")
else()
    math(EXPR typography_detached_open_body_length
        "${typography_detached_open_executor_position} - ${typography_detached_open_definition_position}")
    string(SUBSTRING "${main_source}" ${typography_detached_open_definition_position}
        ${typography_detached_open_body_length} typography_detached_open_body)
    if(NOT typography_detached_open_body MATCHES "sandbox3d_queue_tool_window_open" OR
       typography_detached_open_body MATCHES "henka_engine_open_tool_window|sandbox3d_editor_typography_runtime_attach_context")
        list(APPEND violations "detaching a workspace panel must queue creation and leave workspace state unchanged until font attachment succeeds")
    endif()
endif()

if(violations)
    list(JOIN violations "\n  " formatted_violations)
    message(FATAL_ERROR "Sandbox3D editor typography integration is incomplete:\n  ${formatted_violations}")
endif()

message(STATUS "Sandbox3D installs bundled typography before workspace restore and routes settings, scale consumers, and context lifetimes through its runtime owner")
